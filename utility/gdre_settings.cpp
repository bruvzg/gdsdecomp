#include "gdre_settings.h"
#include "util_functions.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/file_access_encrypted.h"
#include "core/io/file_access_zip.h"
#include "core/object/script_language.h"
#include "core/version.h"
#include "file_access_apk.h"
#include "modules/regex/regex.h"

#if defined(WINDOWS_ENABLED)
#include <windows.h>
#elif defined(UNIX_ENABLED)
#include <limits.h>
#include <unistd.h>
#endif
#include <stdlib.h>

String GDRESettings::_get_cwd() {
#if defined(WINDOWS_ENABLED)
	const DWORD expected_size = ::GetCurrentDirectoryW(0, nullptr);

	Char16String buffer;
	buffer.resize((int)expected_size);
	if (::GetCurrentDirectoryW(expected_size, (wchar_t *)buffer.ptrw()) == 0)
		return ".";

	String result;
	if (result.parse_utf16(buffer.ptr())) {
		return ".";
	}
	return result.simplify_path();
#elif defined(UNIX_ENABLED)
	char buffer[PATH_MAX];
	if (::getcwd(buffer, sizeof(buffer)) == nullptr) {
		return ".";
	}

	String result;
	if (result.parse_utf8(buffer)) {
		return ".";
	}

	return result.simplify_path();
#else
	return ".";
#endif
}

GDRESettings *GDRESettings::singleton = nullptr;

// We have to set this in the singleton here, since after Godot is done initializing,
// it will change the CWD to the executable dir
String GDRESettings::exec_dir = GDRESettings::_get_cwd();
GDRESettings *GDRESettings::get_singleton() {
	// TODO: get rid of this hack (again), the in-editor menu requires this.
	// if (!singleton) {
	// 	memnew(GDRESettings);
	// }
	return singleton;
}

GDRESettings::GDRESettings() {
	singleton = this;
	gdre_resource_path = ProjectSettings::get_singleton()->get_resource_path();
	logger = memnew(GDRELogger);
	add_logger();
}

GDRESettings::~GDRESettings() {
	remove_current_pack();
	if (new_singleton != nullptr) {
		memdelete(new_singleton);
	}
	singleton = nullptr;
	// logger doesn't get memdeleted because the OS singleton will do so
}
String GDRESettings::get_cwd() {
	return GDRESettings::_get_cwd();
};
String GDRESettings::get_exec_dir() {
	return GDRESettings::exec_dir;
};

String GDRESettings::get_gdre_resource_path() const {
	return gdre_resource_path;
}

Vector<uint8_t> GDRESettings::get_encryption_key() {
	return enc_key;
}
String GDRESettings::get_encryption_key_string() {
	return enc_key_str;
}
bool GDRESettings::is_pack_loaded() const {
	return current_pack.is_valid();
}

void GDRESettings::add_pack_file(const Ref<PackedFileInfo> &f_info) {
	files.push_back(f_info);
}

GDRESettings::PackInfo::PackType GDRESettings::get_pack_type() {
	return is_pack_loaded() ? current_pack->type : PackInfo::UNKNOWN;
}
String GDRESettings::get_pack_path() {
	return is_pack_loaded() ? current_pack->pack_file : "";
}
uint32_t GDRESettings::get_pack_version() {
	return is_pack_loaded() ? current_pack->fmt_version : 0;
}
String GDRESettings::get_version_string() {
	return is_pack_loaded() ? current_pack->version_string : String();
}
uint32_t GDRESettings::get_ver_major() {
	return is_pack_loaded() ? current_pack->ver_major : 0;
}
uint32_t GDRESettings::get_ver_minor() {
	return is_pack_loaded() ? current_pack->ver_minor : 0;
}
uint32_t GDRESettings::get_ver_rev() {
	return is_pack_loaded() ? current_pack->ver_rev : 0;
}
uint32_t GDRESettings::get_file_count() {
	return is_pack_loaded() ? current_pack->file_count : 0;
}
void GDRESettings::set_project_path(const String &p_path) {
	project_path = p_path;
}
String GDRESettings::get_project_path() {
	return project_path;
}
bool GDRESettings::is_project_config_loaded() const {
	if (!is_pack_loaded()) {
		return false;
	}
	bool is_loaded = current_pack->pcfg->is_loaded();
	return is_loaded;
}

void GDRESettings::remove_current_pack() {
	current_pack = Ref<PackInfo>();
	packs.clear();
	files.clear();
	reset_encryption_key();
}

void GDRESettings::reset_encryption_key() {
	if (set_key) {
		memcpy(script_encryption_key, old_key, 32);
		set_key = false;
		enc_key_str = "";
		enc_key.clear();
	}
}

String get_standalone_pck_path() {
	String exec_path = OS::get_singleton()->get_executable_path();
	String exec_dir = exec_path.get_base_dir();
	String exec_filename = exec_path.get_file();
	String exec_basename = exec_filename.get_basename();

	return exec_dir.path_join(exec_basename + ".pck");
}

// This loads project directories by setting the global resource path to the project directory
// We have to be very careful about this, this means that any GDRE resources we have loaded
// could fail to reload if they somehow became unloaded while we were messing with the project.
Error GDRESettings::load_dir(const String &p_path) {
	if (is_pack_loaded()) {
		return ERR_ALREADY_IN_USE;
	}
	Ref<DirAccess> da = DirAccess::open(p_path.get_base_dir());
	ERR_FAIL_COND_V_MSG(da.is_null(), ERR_FILE_CANT_OPEN, "Can't find folder!");
	ERR_FAIL_COND_V_MSG(!da->dir_exists(p_path), ERR_FILE_CANT_OPEN, "Can't find folder!");

	// This is a hack to get the resource path set to the project folder
	ProjectSettings *settings_singleton = ProjectSettings::get_singleton();
	GDREPackSettings *new_singleton = static_cast<GDREPackSettings *>(settings_singleton);
	new_singleton->set_resource_path(p_path);

	da = da->open("res://");
	project_path = p_path;
	PackedStringArray pa = da->get_files_at("res://");
	for (auto s : pa) {
		print_line(s);
	}
	// TODO: Need to get minor version number from binary resource for 2.x,
	// there were breaking changes between 2.0 and 2.1
	int ver_major = gdreutil::get_ver_major_from_dir("res://");
	ERR_FAIL_COND_V_MSG(ver_major == 0, ERR_CANT_ACQUIRE_RESOURCE, "Can't find version from directory!");
	int ver_minor;
	WARN_PRINT("Guessing minor version number, 2.0 scripts may be mangled...");
	//for the sake of decompiling scripts
	if (ver_major == 2) {
		ver_minor = 1;
	} else if (ver_major == 3) {
		ver_minor = 3;
	} else {
		ver_minor = 0;
	}
	int ver_rev = 0;
	int version = 1;

	Ref<PackInfo> pckinfo;
	pckinfo.instantiate();
	pckinfo->init(
			p_path, ver_major, ver_minor, ver_rev, version, 0, 0, pa.size(),
			itos(ver_major) + "." + itos(ver_minor) + "." + itos(ver_rev), PackInfo::DIR);
	add_pack_info(pckinfo);
	load_project_config();
	return OK;
}
Error GDRESettings::unload_dir() {
	remove_current_pack();
	ProjectSettings *settings_singleton = ProjectSettings::get_singleton();
	GDREPackSettings *new_singleton = static_cast<GDREPackSettings *>(settings_singleton);
	new_singleton->set_resource_path(gdre_resource_path);
	project_path = "";
	return OK;
}

// This loads the pack into PackedData so that the paths are globally accessible with FileAccess.
// This is VERY hacky. We have to make a new PackedData singleton when loading a pack, and then
// delete it and make another new one while unloading.
// A side-effect of this approach is that the standalone files will be in the path when running
// standalone, which is why they were renamed with the prefix "gdre_" to avoid collision.
// This also means that directory listings on the pack will include the standalone files, but we
// specifically avoid doing that.
// TODO: Consider submitting a PR to refactor PackedData to add and remove packs and sources
Error GDRESettings::load_pack(const String &p_path) {
	if (is_pack_loaded()) {
		return ERR_ALREADY_IN_USE;
	}
	Ref<DirAccess> da = DirAccess::open(p_path.get_base_dir());
	if (da->dir_exists(p_path)) {
		return load_dir(p_path);
	}
	// So that we don't use PackedSourcePCK when we load this
	String pack_path = p_path + "_GDRE_a_really_dumb_hack";

	old_pack_data_singleton = PackedData::get_singleton();
	// if packeddata is disabled, we're in the editor
	in_editor = PackedData::get_singleton()->is_disabled();
	// the PackedData constructor will set the singleton to the newly instanced one
	new_singleton = memnew(PackedData);
	GDREPackedSource *src = memnew(GDREPackedSource);
	Error err;

	new_singleton->add_pack_source(src);
	new_singleton->set_disabled(false);

#ifdef MINIZIP_ENABLED
	// For loading APKs
	new_singleton->add_pack_source(memnew(APKArchive));
#endif

	// If we're not in the editor, we have to add project pack back
	if (!in_editor) {
		// using the base PackedSourcePCK here
		// TODO: We should fail hard if we fail to add the pack back in,
		// but the way of detecting of whether or not we have the pck
		// is insufficient, find another way.
		err = new_singleton->add_pack(get_standalone_pck_path(), false, 0);
		//ERR_FAIL_COND_V_MSG(err, err, "FATAL ERROR: Failed to load GDRE pack!");
	}
	// set replace to true so that project.binary gets overwritten in case its loaded
	// Project settings have already been loaded by this point and this won't affect them,
	// so it's fine
	err = new_singleton->add_pack(pack_path, true, 0);
	if (err) {
		if (error_encryption) {
			error_encryption = false;
			ERR_FAIL_V_MSG(ERR_PRINTER_ON_FIRE, "FATAL ERROR: Failed to decrypt encrypted pack!");
		}
		ERR_FAIL_V_MSG(err, "FATAL ERROR: Failed to load project pack!");
	}

	// If we're in a first load, the old PackedData singleton is still held by main.cpp
	// If we delete it, we'll cause a double free when the program closes because main.cpp deletes it
	if (!first_load) {
		memdelete(old_pack_data_singleton);
	} else {
		first_load = false;
	}
	ERR_FAIL_COND_V_MSG(!is_pack_loaded(), ERR_FILE_CANT_READ, "FATAL ERROR: loaded project pack, but didn't load files from it!");
	if (get_version_string() == "unknown") {
		// TODO: we have to get it from the binary files
		// right now file_access_apk just sets it to 2.1.6 if it can't read it from the manifest
	}
	load_project_config();
	return OK;
}

Error GDRESettings::load_project_config() {
	Error err;
	ERR_FAIL_COND_V_MSG(!is_pack_loaded(), ERR_FILE_CANT_OPEN, "Pack not loaded!");
	ERR_FAIL_COND_V_MSG(is_project_config_loaded(), ERR_ALREADY_IN_USE, "Project config is already loaded!");

	if (get_ver_major() == 2) {
		ERR_FAIL_COND_V_MSG(!has_res_path("res://engine.cfb"), ERR_FILE_NOT_FOUND,
				"Cannot find project config in pack/project folder!");
		err = current_pack->pcfg->load_cfb("res://engine.cfb", get_ver_major(), get_ver_minor());
		ERR_FAIL_COND_V_MSG(err, err, "Failed to load project config!");
	}
	if (get_ver_minor() == 3 || get_ver_minor() == 4) {
		ERR_FAIL_COND_V_MSG(!has_res_path("res://project.binary"), ERR_FILE_NOT_FOUND,
				"Cannot find project config in pack/project folder!");
		err = current_pack->pcfg->load_cfb("res://project.binary", get_ver_major(), get_ver_minor());
		ERR_FAIL_COND_V_MSG(err, err, "Failed to load project config!");
	}
	return OK;
}

Error GDRESettings::save_project_config(const String &p_out_dir = "") {
	String output_dir = p_out_dir;
	if (output_dir.is_empty()) {
		output_dir = project_path;
	}
	return current_pack->pcfg->save_cfb(output_dir, get_ver_major(), get_ver_minor());
}

Error GDRESettings::unload_pack() {
	if (!is_pack_loaded()) {
		return ERR_DOES_NOT_EXIST;
	}
	if (get_pack_type() == PackInfo::DIR) {
		return unload_dir();
	}

	remove_current_pack();
	// we have to re-init PackedData to clear the paths
	PackedData *new_old_singleton = memnew(PackedData);
	new_old_singleton->set_disabled(in_editor);
	if (!in_editor) {
		new_old_singleton->add_pack(get_standalone_pck_path(), false, 0);
// main.cpp normally adds this
#ifdef MINIZIP_ENABLED
		// instead of the singleton
		new_old_singleton->add_pack_source(memnew(ZipArchive));
#endif
	}
	memdelete(new_singleton);
	new_singleton = nullptr;
	return OK;
}

void GDRESettings::add_pack_info(Ref<PackInfo> packinfo) {
	packs.push_back(packinfo);
	current_pack = packinfo;
}
// PackedSource doesn't pass back useful error information when loading packs,
// this is a hack so that we can tell if it was an encryption error.
void GDRESettings::_set_error_encryption(bool is_encryption_error) {
	error_encryption = is_encryption_error;
}
void GDRESettings::set_encryption_key(const String &key_str) {
	String skey = key_str.replace_first("0x", "");
	ERR_FAIL_COND_MSG(!skey.is_valid_hex_number(false) || skey.size() < 64, "not a valid key");

	Vector<uint8_t> key;
	key.resize(32);
	for (int i = 0; i < 32; i++) {
		int v = 0;
		if (i * 2 < skey.length()) {
			char32_t ct = skey.to_lower()[i * 2];
			if (ct >= '0' && ct <= '9')
				ct = ct - '0';
			else if (ct >= 'a' && ct <= 'f')
				ct = 10 + ct - 'a';
			v |= ct << 4;
		}

		if (i * 2 + 1 < skey.length()) {
			char32_t ct = skey.to_lower()[i * 2 + 1];
			if (ct >= '0' && ct <= '9')
				ct = ct - '0';
			else if (ct >= 'a' && ct <= 'f')
				ct = 10 + ct - 'a';
			v |= ct;
		}
		key.write[i] = v;
	}
	set_encryption_key(key);
}

void GDRESettings::set_encryption_key(Vector<uint8_t> key) {
	ERR_FAIL_COND_MSG(key.size() < 32, "key size incorrect!");
	if (!set_key) {
		memcpy(old_key, script_encryption_key, 32);
	}
	memcpy(script_encryption_key, key.ptr(), 32);
	set_key = true;
	enc_key = key;
	enc_key_str = String::hex_encode_buffer(key.ptr(), 32);
}

Vector<String> GDRESettings::get_file_list(const Vector<String> &filters) {
	Vector<String> ret;
	if (get_pack_type() == PackInfo::DIR) {
		return gdreutil::get_recursive_dir_list("res://", filters, true);
	}
	Vector<Ref<PackedFileInfo>> flist = get_file_info_list(filters);
	for (int i = 0; i < flist.size(); i++) {
		ret.push_back(flist[i]->path);
	}
	return ret;
}

Vector<Ref<PackedFileInfo>> GDRESettings::get_file_info_list(const Vector<String> &filters) {
	if (filters.size() == 0) {
		return files;
	}
	Vector<Ref<PackedFileInfo>> ret;
	for (int i = 0; i < files.size(); i++) {
		for (int j = 0; j < filters.size(); j++) {
			if (files.get(i)->get_path().get_file().match(filters[j])) {
				ret.push_back(files.get(i));
				break;
			}
		}
	}
	return ret;
}

String GDRESettings::localize_path(const String &p_path, const String &resource_dir) const {
	String res_path = resource_dir != "" ? resource_dir : project_path;

	if (res_path == "") {
		//not initialized yet
		if (!p_path.is_absolute_path()) {
			//just tack on a "res://" here
			return "res://" + p_path;
		}
		return p_path;
	}

	if (p_path.begins_with("res://") || p_path.begins_with("user://") ||
			(p_path.is_absolute_path() && !p_path.begins_with(res_path))) {
		return p_path.simplify_path();
	}

	Ref<DirAccess> dir = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);

	String path = p_path.replace("\\", "/").simplify_path();

	if (dir->change_dir(path) == OK) {
		String cwd = dir->get_current_dir();
		cwd = cwd.replace("\\", "/");

		res_path = res_path.path_join("");

		// DirAccess::get_current_dir() is not guaranteed to return a path that with a trailing '/',
		// so we must make sure we have it as well in order to compare with 'res_path'.
		cwd = cwd.path_join("");

		if (!cwd.begins_with(res_path)) {
			return p_path;
		}

		return cwd.replace_first(res_path, "res://");
	} else {
		int sep = path.rfind("/");
		if (sep == -1) {
			return "res://" + path;
		}

		String parent = path.substr(0, sep);

		String plocal = localize_path(parent, res_path);
		if (plocal == "") {
			return "";
		}
		// Only strip the starting '/' from 'path' if its parent ('plocal') ends with '/'
		if (plocal[plocal.length() - 1] == '/') {
			sep += 1;
		}
		return plocal + path.substr(sep, path.size() - sep);
	}
}

String GDRESettings::globalize_path(const String &p_path, const String &resource_dir) const {
	String res_path = resource_dir != "" ? resource_dir : project_path;

	if (p_path.begins_with("res://")) {
		if (res_path != "") {
			return p_path.replace("res:/", res_path);
		}
		return p_path.replace("res://", "");
	} else if (p_path.begins_with("user://")) {
		String data_dir = OS::get_singleton()->get_user_data_dir();
		if (data_dir != "") {
			return p_path.replace("user:/", data_dir);
		}
		return p_path.replace("user://", "");
	} else if (!p_path.is_absolute_path()) {
		return res_path.path_join(p_path);
	}

	return p_path;
}

bool GDRESettings::is_fs_path(const String &p_path) const {
	if (!p_path.is_absolute_path()) {
		return true;
	}
	if (p_path.find("://") == -1 || p_path.begins_with("file://")) {
		return true;
	}
	//windows
	if (OS::get_singleton()->get_name().begins_with("Win")) {
		auto reg = RegEx("^[A-Za-z]:\\/");
		if (reg.search(p_path).is_valid()) {
			return true;
		}
		return false;
	}
	// unix
	if (p_path.begins_with("/")) {
		return true;
	}
	return false;
}

// This gets the path necessary to open the file by checking for its existence
// If a pack is loaded, it will try to find it in the pack and fail if it can't
// If not, it will look for it in the file system and fail if it can't
// If it fails to find it, it returns an empty string
String GDRESettings::_get_res_path(const String &p_path, const String &resource_dir, const bool suppress_errors) {
	String res_dir = resource_dir != "" ? resource_dir : project_path;
	String res_path;
	// Try and find it in the packed data
	if (is_pack_loaded() && get_pack_type() != PackInfo::DIR) {
		if (PackedData::get_singleton()->has_path(p_path)) {
			return p_path;
		}
		res_path = localize_path(p_path, res_dir);
		if (res_path != p_path && PackedData::get_singleton()->has_path(res_path)) {
			return res_path;
		}
		// localize_path did nothing
		if (!res_path.is_absolute_path()) {
			res_path = "res://" + res_path;
			if (PackedData::get_singleton()->has_path(res_path)) {
				return res_path;
			}
		}
		// Can't find it
		ERR_FAIL_COND_V_MSG(!suppress_errors, "", "Can't find " + res_path + " in PackedData");
		return "";
	}
	//try and find it on the file system
	res_path = p_path;
	if (res_path.is_absolute_path() && is_fs_path(res_path)) {
		if (!FileAccess::exists(res_path)) {
			ERR_FAIL_COND_V_MSG(!suppress_errors, "", "Resource " + res_path + " does not exist");
			return "";
		}
		return res_path;
	}

	if (res_dir == "") {
		ERR_FAIL_COND_V_MSG(!suppress_errors, "", "Can't find resource without project dir set");
		return "";
	}

	res_path = globalize_path(res_path, res_dir);
	if (!FileAccess::exists(res_path)) {
		ERR_FAIL_COND_V_MSG(!suppress_errors, "", "Resource " + res_path + " does not exist");
		return "";
	}
	return res_path;
}

bool GDRESettings::has_res_path(const String &p_path, const String &resource_dir) {
	return _get_res_path(p_path, resource_dir, true) != "";
}

String GDRESettings::get_res_path(const String &p_path, const String &resource_dir) {
	return _get_res_path(p_path, resource_dir, false);
}
bool GDRESettings::has_any_remaps() const {
	if (is_pack_loaded()) {
		if (current_pack->pcfg->is_loaded() && current_pack->pcfg->has_setting("remap/all")) {
			return true;
		}
	}
	return false;
}

//only works on 2.x right now
bool GDRESettings::has_remap(const String &src, const String &dst) const {
	if (is_pack_loaded()) {
		if (is_project_config_loaded() && current_pack->pcfg->has_setting("remap/all")) {
			PackedStringArray v2remaps = current_pack->pcfg->get_setting("remap/all", PackedStringArray());
			if ((v2remaps.has(localize_path(src)) && v2remaps.has(localize_path(dst)))) {
				return true;
			}
		}
	}
	return false;
}

//only works on 2.x right now
Error GDRESettings::add_remap(const String &src, const String &dst) {
	ERR_FAIL_COND_V_MSG(!is_pack_loaded(), ERR_DATABASE_CANT_READ, "Pack not loaded!");
	ERR_FAIL_COND_V_MSG(!is_project_config_loaded(), ERR_DATABASE_CANT_READ, "project config not loaded!");
	PackedStringArray v2remaps = current_pack->pcfg->get_setting("remap/all", PackedStringArray());
	v2remaps.push_back(localize_path(src));
	v2remaps.push_back(localize_path(dst));
	current_pack->pcfg->set_setting("remap/all", v2remaps);
	return OK;
}

//only works on 2.x right now
Error GDRESettings::remove_remap(const String &src, const String &dst) {
	ERR_FAIL_COND_V_MSG(!is_pack_loaded(), ERR_DATABASE_CANT_READ, "Pack not loaded!");
	ERR_FAIL_COND_V_MSG(!is_project_config_loaded(), ERR_DATABASE_CANT_READ, "project config not loaded!");
	PackedStringArray v2remaps = current_pack->pcfg->get_setting("remap/all", PackedStringArray());
	Error err;
	if ((v2remaps.has(localize_path(src)) && v2remaps.has(localize_path(dst)))) {
		v2remaps.erase(localize_path(src));
		v2remaps.erase(localize_path(dst));
		if (v2remaps.size()) {
			err = current_pack->pcfg->set_setting("remap/all", v2remaps);
		} else {
			err = current_pack->pcfg->remove_setting("remap/all");
		}
		return err;
	}
	ERR_FAIL_V_MSG(ERR_DOES_NOT_EXIST, "Remap between" + src + " and " + dst + " does not exist!");
}

void GDRELogger::logv(const char *p_format, va_list p_list, bool p_err) {
	if (!should_log(p_err)) {
		return;
	}

	if (file.is_valid()) {
		const int static_buf_size = 512;
		char static_buf[static_buf_size];
		char *buf = static_buf;
		va_list list_copy;
		va_copy(list_copy, p_list);
		int len = vsnprintf(buf, static_buf_size, p_format, p_list);
		if (len >= static_buf_size) {
			buf = (char *)Memory::alloc_static(len + 1);
			vsnprintf(buf, len + 1, p_format, list_copy);
		}
		va_end(list_copy);
		file->store_buffer((uint8_t *)buf, len);

		if (len >= static_buf_size) {
			Memory::free_static(buf);
		}

		if (p_err || _flush_stdout_on_print) {
			// Don't always flush when printing stdout to avoid performance
			// issues when `print()` is spammed in release builds.
			file->flush();
		}
	}
}

String GDRESettings::get_log_file_path() {
	if (!logger) {
		return "";
	}
	return logger->get_path();
}

Error GDRESettings::open_log_file(const String &output_dir) {
	String logfile = output_dir.path_join("gdre_export.log");
	Error err = logger->open_file(logfile);
	ERR_FAIL_COND_V_MSG(err == ERR_ALREADY_IN_USE, err, "Already logging to another file");
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not open log file " + logfile);
	return OK;
}

Error GDRESettings::close_log_file() {
	logger->close_file();
	return OK;
}

Error GDRELogger::open_file(const String &p_base_path) {
	if (file.is_valid()) {
		return ERR_ALREADY_IN_USE;
	}
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_USERDATA);
	if (da.is_valid()) {
		da->make_dir_recursive(p_base_path.get_base_dir());
	}
	Error err;
	file = FileAccess::open(p_base_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(file.is_null(), err, "Failed to open log file " + p_base_path + " for writing.");
	base_path = p_base_path;
	return OK;
}

void GDRELogger::close_file() {
	if (file.is_valid()) {
		file->flush();
		file = Ref<FileAccess>();
		base_path = "";
	}
}

GDRELogger::GDRELogger() {}
GDRELogger::~GDRELogger() {
	close_file();
}

// This adds another logger to the global composite logger so that we can write
// export logs to project directories.
// main.cpp is apparently the only class that can add these, but "add_logger" is
// only protected, so we can cast the singleton to a child class pointer and then call it.
void GDRESettings::add_logger() {
	OS *os_singleton = OS::get_singleton();
	String os_name = os_singleton->get_name();

	if (os_name == "Windows") {
#ifdef WINDOWS_ENABLED
		GDREOS<OS_Windows> *_gdre_os = static_cast<GDREOS<OS_Windows> *>(os_singleton);
		_gdre_os->_add_logger(logger);
#endif
	}
#ifdef X11_ENABLED
	else if (os_name == "Linux" || os_name.find("BSD") == -1) {
		GDREOS<OS_LinuxBSD> *_gdre_os = static_cast<GDREOS<OS_LinuxBSD> *>(os_singleton);
		_gdre_os->_add_logger(logger);
	}
#endif
#ifdef OSX_ENABLED
	else if (os_name == "macOS") {
		GDREOS<OS_OSX> *_gdre_os = static_cast<GDREOS<OS_OSX> *>(os_singleton);
		_gdre_os->_add_logger(logger);
	}
#endif
#ifdef UWP_ENABLED
	else if (os_name == "UWP") {
		GDREOS<OS_UWP> *_gdre_os = static_cast<GDREOS<OS_UWP> *>(os_singleton);
		_gdre_os->_add_logger(logger);
	}
#endif
// the rest of these are probably unnecessary
#ifdef JAVASCRIPT_ENABLED
	else if (os_name == "Javascript") {
		GDREOS<OS_JavaScript> *_gdre_os = static_cast<GDREOS<OS_JavaScript> *>(os_singleton);
		_gdre_os->_add_logger(logger);
	}
#endif
#if defined(__ANDROID__)
	else if (os_name == "Android") {
		GDREOS<OS_Android> *_gdre_os = static_cast<GDREOS<OS_Android> *>(os_singleton);
		_gdre_os->_add_logger(logger);
	}
#endif
#ifdef IPHONE_ENABLED
	else if (os_name == "iOS") {
		GDREOS<OSIPhone> *_gdre_os = static_cast<GDREOS<OSIPhone> *>(os_singleton);
		_gdre_os->_add_logger(logger);
	}
#endif
	else {
		WARN_PRINT("No logger being set, there will be no logs!");
	}
}

bool GDREPackedSource::try_open_pack(const String &p_path, bool p_replace_files, uint64_t p_offset) {
	if (p_path.get_extension().to_lower() == "apk") {
		return false;
	}
	String pck_path = p_path.replace("_GDRE_a_really_dumb_hack", "");
	Ref<FileAccess> f = FileAccess::open(pck_path, FileAccess::READ);
	if (f.is_null()) {
		return false;
	}

	f->seek(p_offset);

	uint32_t magic = f->get_32();

	if (magic != PACK_HEADER_MAGIC) {
		// loading with offset feature not supported for self contained exe files
		if (p_offset != 0) {
			ERR_FAIL_V_MSG(false, "Loading self-contained executable with offset not supported.");
		}

		//maybe at the end.... self contained exe
		f->seek_end();
		f->seek(f->get_position() - 4);
		magic = f->get_32();
		if (magic != PACK_HEADER_MAGIC) {
			return false;
		}
		f->seek(f->get_position() - 12);

		uint64_t ds = f->get_64();
		f->seek(f->get_position() - ds - 8);

		magic = f->get_32();
		if (magic != PACK_HEADER_MAGIC) {
			return false;
		}
	}

	uint32_t version = f->get_32();
	uint32_t ver_major = f->get_32();
	uint32_t ver_minor = f->get_32();
	uint32_t ver_rev = f->get_32(); // patch number, not used for validation.

	if (version > PACK_FORMAT_VERSION) {
		ERR_FAIL_V_MSG(false, "Pack version unsupported: " + itos(version) + ".");
	}

	uint32_t pack_flags = 0;
	uint64_t file_base = 0;

	if (version == 2) {
		pack_flags = f->get_32();
		file_base = f->get_64();
	}

	bool enc_directory = (pack_flags & PACK_DIR_ENCRYPTED);

	for (int i = 0; i < 16; i++) {
		//reserved
		f->get_32();
	}

	uint32_t file_count = f->get_32();

	if (enc_directory) {
		Ref<FileAccessEncrypted> fae = memnew(FileAccessEncrypted);
		if (fae.is_null()) {
			GDRESettings::get_singleton()->_set_error_encryption(true);
			ERR_FAIL_V_MSG(false, "Can't open encrypted pack directory.");
		}

		Vector<uint8_t> key;
		key.resize(32);
		for (int i = 0; i < key.size(); i++) {
			key.write[i] = script_encryption_key[i];
		}

		Error err = fae->open_and_parse(f, key, FileAccessEncrypted::MODE_READ, false);
		if (err) {
			GDRESettings::get_singleton()->_set_error_encryption(true);
			ERR_FAIL_V_MSG(false, "Can't open encrypted pack directory.");
		}
		f = fae;
	}

	// Everything worked, now set the data
	Ref<GDRESettings::PackInfo> pckinfo;
	pckinfo.instantiate();
	pckinfo->init(
			pck_path, ver_major, ver_minor, ver_rev, version, pack_flags, file_base, file_count,
			itos(ver_major) + "." + itos(ver_minor) + "." + itos(ver_rev), GDRESettings::PackInfo::PCK);
	GDRESettings::get_singleton()->add_pack_info(pckinfo);

	for (uint32_t i = 0; i < file_count; i++) {
		uint32_t sl = f->get_32();
		CharString cs;
		cs.resize(sl + 1);
		f->get_buffer((uint8_t *)cs.ptr(), sl);
		cs[sl] = 0;

		String path;
		path.parse_utf8(cs.ptr());

		ERR_FAIL_COND_V_MSG(path.get_file().find("gdre_") != -1, false, "Tried to load a gdre file?!?!");

		uint64_t ofs = file_base + f->get_64();
		uint64_t size = f->get_64();
		uint8_t md5[16];
		uint32_t flags = 0;
		f->get_buffer(md5, 16);
		if (version == 2) {
			flags = f->get_32();
		}
		// add the file info to settings
		PackedData::PackedFile pf;
		pf.offset = ofs + p_offset;
		memcpy(pf.md5, md5, 16);
		pf.size = size;
		pf.encrypted = flags & PACK_FILE_ENCRYPTED;
		pf.pack = p_path;
		pf.src = this;
		Ref<PackedFileInfo> pf_info;
		pf_info.instantiate();
		pf_info->init(path, &pf);
		GDRESettings::get_singleton()->add_pack_file(pf_info);
		// use the corrected path, not the raw path
		path = pf_info->get_path();
		PackedData::get_singleton()->add_path(pck_path, path, ofs + p_offset, size, md5, this, p_replace_files, (flags & PACK_FILE_ENCRYPTED));
	}

	return true;
}
Ref<FileAccess> GDREPackedSource::get_file(const String &p_path, PackedData::PackedFile *p_file) {
	return memnew(FileAccessPack(p_path, *p_file));
}
