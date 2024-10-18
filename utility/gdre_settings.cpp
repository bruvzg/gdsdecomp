#include "gdre_settings.h"
#include "bytecode/bytecode_tester.h"
#include "core/error/error_list.h"
#include "core/error/error_macros.h"
#include "core/io/file_access.h"
#include "core/object/class_db.h"
#include "core/string/print_string.h"
#include "editor/gdre_editor.h"
#include "editor/gdre_version.gen.h"
#include "file_access_apk.h"
#include "file_access_gdre.h"
#include "gdre_logger.h"
#include "gdre_packed_source.h"
#include "util_functions.h"

#include "core/config/engine.h"
#include "core/config/project_settings.h"
#include "core/io/file_access_encrypted.h"
#include "core/io/file_access_zip.h"
#include "core/object/script_language.h"
#include "core/version.h"
#include "modules/regex/regex.h"
#include "servers/rendering_server.h"

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
String get_java_path() {
	if (!OS::get_singleton()->has_environment("JAVA_HOME")) {
		return "";
	}
	String exe_ext = "";
	if (OS::get_singleton()->get_name() == "Windows") {
		exe_ext = ".exe";
	}
	return OS::get_singleton()->get_environment("JAVA_HOME").simplify_path().path_join("bin").path_join("java") + exe_ext;
}

int get_java_version() {
	List<String> args;
	// when using "-version", java will ALWAYS output on stderr in the format:
	// <java/openjdk/etc> version "x.x.x" <optional_builddate>
	args.push_back("-version");
	String output;
	int retval = 0;
	String java_path = get_java_path();
	if (java_path.is_empty()) {
		return -1;
	}
	Error err = OS::get_singleton()->execute(java_path, args, &output, &retval, true);
	if (err || retval) {
		return -1;
	}
	Vector<String> components = output.split("\n")[0].split(" ");
	if (components.size() < 3) {
		return 0;
	}
	String version_string = components[2].replace("\"", "");
	components = version_string.split(".", false);
	if (components.size() < 3) {
		return 0;
	}
	int version_major = components[0].to_int();
	int version_minor = components[1].to_int();
	// "1.8", and the like
	if (version_major == 1) {
		return version_minor;
	}
	return version_major;
}
bool GDRESettings::check_if_dir_is_v4() {
	// these are files that will only show up in version 4
	static const Vector<String> wildcards = { "*.ctex" };
	if (get_file_list(wildcards).size() > 0) {
		return true;
	} else {
		return false;
	}
}

bool GDRESettings::check_if_dir_is_v3() {
	// these are files that will only show up in version 3
	static const Vector<String> wildcards = { "*.stex" };
	if (get_file_list(wildcards).size() > 0) {
		return true;
	} else {
		return false;
	}
}

bool GDRESettings::check_if_dir_is_v2() {
	// these are files that will only show up in version 2
	static const Vector<String> wildcards = { "*.tex" };
	if (get_file_list(wildcards).size() > 0) {
		return true;
	} else {
		return false;
	}
}

int GDRESettings::get_ver_major_from_dir() {
	if (check_if_dir_is_v2())
		return 2;
	if (check_if_dir_is_v3())
		return 3;
	if (check_if_dir_is_v4())
		return 4;
	return 0;
}

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
// This adds compatibility classes for old objects that we know can be loaded on v4 just by changing the name
void addCompatibilityClasses() {
	ClassDB::add_compatibility_class("PHashTranslation", "OptimizedTranslation");
}

GDRESettings::GDRESettings() {
#ifdef TOOLS_ENABLED
	if (RenderingServer::get_singleton()) {
		RenderingServer::get_singleton()->set_warn_on_surface_upgrade(false);
	}
#endif
	singleton = this;
	gdre_packeddata_singleton = memnew(GDREPackedData);
	addCompatibilityClasses();
	gdre_resource_path = ProjectSettings::get_singleton()->get_resource_path();
	logger = memnew(GDRELogger);
	headless = !RenderingServer::get_singleton() || RenderingServer::get_singleton()->get_video_adapter_name().is_empty();
	add_logger();
}

GDRESettings::~GDRESettings() {
	remove_current_pack();
	memdelete(gdre_packeddata_singleton);
	singleton = nullptr;
	logger->_disable();
	// logger doesn't get memdeleted because the OS singleton will do so
}
String GDRESettings::get_cwd() {
	return GDRESettings::_get_cwd();
};

String GDRESettings::get_exec_dir() {
	return GDRESettings::exec_dir;
}

bool GDRESettings::are_imports_loaded() const {
	return import_files.size() > 0;
}

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
	file_map.insert(f_info->path, f_info);
}
bool GDRESettings::has_valid_version() const {
	return is_pack_loaded() && current_pack->version.is_valid() && current_pack->version->is_valid_semver();
}
GDRESettings::PackInfo::PackType GDRESettings::get_pack_type() const {
	return is_pack_loaded() ? current_pack->type : PackInfo::UNKNOWN;
}
String GDRESettings::get_pack_path() const {
	return is_pack_loaded() ? current_pack->pack_file : "";
}
uint32_t GDRESettings::get_pack_format() const {
	return is_pack_loaded() ? current_pack->fmt_version : 0;
}
String GDRESettings::get_version_string() const {
	return has_valid_version() ? current_pack->version->as_text() : String();
}
uint32_t GDRESettings::get_ver_major() const {
	return has_valid_version() ? current_pack->version->get_major() : 0;
}
uint32_t GDRESettings::get_ver_minor() const {
	return has_valid_version() ? current_pack->version->get_minor() : 0;
}
uint32_t GDRESettings::get_ver_rev() const {
	return has_valid_version() ? current_pack->version->get_patch() : 0;
}
uint32_t GDRESettings::get_file_count() const {
	return is_pack_loaded() ? current_pack->file_count : 0;
}

void GDRESettings::set_ver_rev(uint32_t p_rev) {
	if (is_pack_loaded()) {
		if (!has_valid_version()) {
			current_pack->version = Ref<GodotVer>(memnew(GodotVer(0, 0, p_rev)));
		} else {
			current_pack->version->set_patch(p_rev);
			if (current_pack->version->get_build_metadata() == "x") {
				current_pack->version->set_build_metadata("");
			}
		}
	}
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
	file_map.clear();
	import_files.clear();
	code_files.clear();
	remap_iinfo.clear();
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

Error GDRESettings::fix_patch_number() {
	if (!is_pack_loaded()) {
		return OK;
	}
	if (get_ver_major() == 2 && get_ver_minor() == 0) {
		// just set it to the latest patch
		set_ver_rev(4);
		return OK;
	}
	// This is only implemented for 3.1 and 2.1; they started writing the correct patch number to the pck in 3.2
	if (!((get_ver_major() == 3 || get_ver_major() == 2) && get_ver_minor() == 1)) {
		return OK;
	}
	auto revision = BytecodeTester::test_files(code_files, get_ver_major(), get_ver_minor());
	switch (revision) {
		case 0xed80f45: // 2.1.{3-6}
			set_ver_rev(6);
			break;
		case 0x85585c7: // 2.1.2
			set_ver_rev(2);
			break;
		case 0x7124599: // 2.1.{0-1}
			set_ver_rev(1);
			break;
		case 0x1a36141: // 3.1.0
			set_ver_rev(0);
			break;
		case 0x514a3fb: // 3.1.1
			set_ver_rev(1);
			break;
		case 0x1ca61a3: // 3.1.beta
			current_pack->version = GodotVer::parse("3.1.0-beta5");
			break;
		default:
			ERR_FAIL_COND_V_MSG(true, ERR_CANT_RESOLVE, "Could not determine patch number!");
			break;
	}
	return OK;
}

// This loads project directories by setting the global resource path to the project directory
// We have to be very careful about this, this means that any GDRE resources we have loaded
// could fail to reload if they somehow became unloaded while we were messing with the project.
Error GDRESettings::load_dir(const String &p_path) {
	if (is_pack_loaded()) {
		return ERR_ALREADY_IN_USE;
	}
	print_line("Opening file: " + p_path);
	Ref<DirAccess> da = DirAccess::open(p_path.get_base_dir());
	ERR_FAIL_COND_V_MSG(da.is_null(), ERR_FILE_CANT_OPEN, "FATAL ERROR: Can't find folder!");
	ERR_FAIL_COND_V_MSG(!da->dir_exists(p_path), ERR_FILE_CANT_OPEN, "FATAL ERROR: Can't find folder!");

	// This is a hack to get the resource path set to the project folder
	ProjectSettings *settings_singleton = ProjectSettings::get_singleton();
	GDREPackSettings *new_singleton = static_cast<GDREPackSettings *>(settings_singleton);
	new_singleton->set_resource_path(p_path);

	da = da->open("res://");
	project_path = p_path;
	PackedStringArray pa = da->get_files_at("res://");
	if (is_print_verbose_enabled()) {
		for (auto s : pa) {
			print_verbose(s);
		}
	}

	Ref<PackInfo> pckinfo;
	pckinfo.instantiate();
	pckinfo->init(
			p_path, Ref<GodotVer>(memnew(GodotVer)), 1, 0, 0, pa.size(), PackInfo::DIR);
	// Need to get version number from binary resources

	add_pack_info(pckinfo);
	Error err = load_import_files();
	ERR_FAIL_COND_V_MSG(err, err, "FATAL ERROR: Could not load imported binary files!");
	err = get_version_from_bin_resources();
	// this is a catastrophic failure, unload the pack
	if (err) {
		unload_pack();
		ERR_FAIL_V_MSG(err, "FATAL ERROR: Can't determine engine version of project directory!");
	}
	if (!pack_has_project_config()) {
		WARN_PRINT("Could not find project configuration in directory, may be a seperate resource directory...");
	} else {
		err = load_project_config();
		ERR_FAIL_COND_V_MSG(err, err, "FATAL ERROR: Can't open project config!");
	}
	err = fix_patch_number();
	if (err) { // this only fails on 2.1 and 3.1, where it's crucial to determine the patch number; if this fails, we can't continue
		unload_pack();
		ERR_FAIL_V_MSG(err, "FATAL ERROR: Could not determine patch number to decompile scripts!");
	}
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

bool is_macho(const String &p_path) {
	Ref<FileAccess> fa = FileAccess::open(p_path, FileAccess::READ);
	if (fa.is_null()) {
		return false;
	}
	uint8_t header[4];
	fa->get_buffer(header, 4);
	fa->close();
	if ((header[0] == 0xcf || header[0] == 0xce) && header[1] == 0xfa && header[2] == 0xed && header[3] == 0xfe) {
		return true;
	}

	// handle fat binaries
	// always stored in big-endian format
	if (header[0] == 0xca && header[1] == 0xfe && header[2] == 0xba && header[3] == 0xbe) {
		return true;
	}
	// handle big-endian mach-o binaries
	if (header[0] == 0xfe && header[1] == 0xed && header[2] == 0xfa && (header[3] == 0xce || header[3] == 0xcf)) {
		return true;
	}

	return false;
}

Error check_embedded(String &p_path) {
	String extension = p_path.get_extension().to_lower();
	if (extension != "pck" && extension != "apk" && extension != "zip") {
		// check if it's a mach-o executable

		if (GDREPackedSource::is_embeddable_executable(p_path)) {
			if (!GDREPackedSource::has_embedded_pck(p_path)) {
				return ERR_FILE_UNRECOGNIZED;
			}
		} else if (is_macho(p_path)) {
			return ERR_FILE_UNRECOGNIZED;
		}
	}
	return OK;
}

Error GDRESettings::load_pack(const String &p_path, bool _cmd_line_extract) {
	if (is_pack_loaded()) {
		return ERR_ALREADY_IN_USE;
	}
	String path = p_path;
	if (DirAccess::exists(p_path)) {
		// This may be a ".app" bundle, so we need to check if it's a valid Godot app
		// and if so, load the pck from inside the bundle
		if (p_path.get_extension().to_lower() == "app") {
			String resources_path = p_path.path_join("Contents").path_join("Resources");
			if (DirAccess::exists(resources_path)) {
				auto list = gdreutil::get_recursive_dir_list(resources_path, { "*.pck" }, true);
				if (!list.is_empty()) {
					if (list.size() > 1) {
						WARN_PRINT("Found multiple pck files in bundle, using first one!!!");
					}
					return load_pack(list[0]);
				} else {
					ERR_FAIL_V_MSG(ERR_FILE_NOT_FOUND, "Can't find pck file in .app bundle!");
				}
			}
		}
		return load_dir(p_path);
	}
	print_line("Opening file: " + path);
	Error err = check_embedded(path);
	if (err != OK) {
		String parent_path = path.get_base_dir();
		if (parent_path.is_empty()) {
			parent_path = GDRESettings::get_exec_dir();
		}
		if (parent_path.get_file().to_lower() == "macos") {
			// we want to get ../Resources
			parent_path = parent_path.get_base_dir().path_join("Resources");
			String pck_path = parent_path.path_join(path.get_file().get_basename() + ".pck");
			if (FileAccess::exists(pck_path)) {
				path = pck_path;
				err = OK;
			}
		}
		if (err != OK) {
			String pck_path = path.get_basename() + ".pck";
			ERR_FAIL_COND_V_MSG(!FileAccess::exists(pck_path), err, "Can't find embedded pck file in executable and cannot find pck file in same directory!");
			path = pck_path;
		}
		WARN_PRINT("Could not find embedded pck in EXE, found pck file, loading from: " + path);
	}
	err = GDREPackedData::get_singleton()->add_pack(path, false, 0);
	ERR_FAIL_COND_V_MSG(err, err, "FATAL ERROR: Can't open pack!");
	ERR_FAIL_COND_V_MSG(!is_pack_loaded(), ERR_FILE_CANT_READ, "FATAL ERROR: loaded project pack, but didn't load files from it!");
	if (_cmd_line_extract) {
		// we don't want to load the imports and project config if we're just extracting.
		return OK;
	}
	if (!pack_has_project_config()) {
		WARN_PRINT("Could not find project configuration in directory, may be a seperate resource pack...");
	} else if (has_valid_version()) {
		err = load_project_config();
		ERR_FAIL_COND_V_MSG(err, err, "FATAL ERROR: Can't open project config!");
	}
	err = load_import_files();
	ERR_FAIL_COND_V_MSG(err, ERR_FILE_CANT_READ, "FATAL ERROR: Could not load imported binary files!");
	if (!has_valid_version()) {
		err = get_version_from_bin_resources();
		// this is a catastrophic failure, unload the pack
		if (err) {
			unload_pack();
			ERR_FAIL_V_MSG(err, "FATAL ERROR: Can't determine engine version of project pack!");
		}
		if (pack_has_project_config()) {
			ERR_FAIL_COND_V_MSG(load_project_config(), err, "FATAL ERROR: Can't open project config!");
		}
	}
	err = fix_patch_number();
	if (err) { // this only fails on 2.1 and 3.1, where it's crucial to determine the patch number; if this fails, we can't continue
		unload_pack();
		ERR_FAIL_V_MSG(err, "FATAL ERROR: Could not determine patch number to decompile scripts, please report this on the GitHub page!");
	}
	return OK;
}

Error GDRESettings::get_version_from_bin_resources() {
	int consistent_versions = 0;
	int inconsistent_versions = 0;
	int ver_major = 0;
	int ver_minor = 0;

	int i;
	for (i = 0; i < import_files.size(); i++) {
		Ref<ImportInfo> iinfo = import_files[i];
		int _res_ver_major = iinfo->get_ver_major();
		int _res_ver_minor = iinfo->get_ver_minor();
		if (consistent_versions == 0) {
			ver_major = _res_ver_major;
			ver_minor = _res_ver_minor;
		}
		if (ver_major == _res_ver_major && _res_ver_minor == ver_minor) {
			consistent_versions++;
		} else {
			if (ver_major != _res_ver_major) {
				WARN_PRINT_ONCE("WARNING!!!!! Inconsistent major versions in binary resources!");
				if (ver_major < _res_ver_major) {
					ver_major = _res_ver_major;
					ver_minor = _res_ver_minor;
				}
			} else if (ver_minor < _res_ver_minor) {
				ver_minor = _res_ver_minor;
			}
			inconsistent_versions++;
		}
	}
	if (inconsistent_versions > 0) {
		WARN_PRINT(itos(inconsistent_versions) + " binary resources had inconsistent versions!");
	}
	// we somehow didn't get a version major??
	if (ver_major == 0) {
		WARN_PRINT("Couldn't determine ver major from binary resources?!");
		ver_major = get_ver_major_from_dir();
		ERR_FAIL_COND_V_MSG(ver_major == 0, ERR_CANT_ACQUIRE_RESOURCE, "Can't find version from directory!");
	}

	current_pack->version = GodotVer::create(ver_major, ver_minor, 0);
	return OK;
}

Error GDRESettings::load_project_config() {
	Error err;
	ERR_FAIL_COND_V_MSG(!is_pack_loaded(), ERR_FILE_CANT_OPEN, "Pack not loaded!");
	ERR_FAIL_COND_V_MSG(is_project_config_loaded(), ERR_ALREADY_IN_USE, "Project config is already loaded!");
	ERR_FAIL_COND_V_MSG(!pack_has_project_config(), ERR_FILE_NOT_FOUND, "Could not find project config!");
	if (get_ver_major() == 2) {
		err = current_pack->pcfg->load_cfb("res://engine.cfb", get_ver_major(), get_ver_minor());
		ERR_FAIL_COND_V_MSG(err, err, "Failed to load project config!");
	} else if (get_ver_major() == 3 || get_ver_major() == 4) {
		err = current_pack->pcfg->load_cfb("res://project.binary", get_ver_major(), get_ver_minor());
		ERR_FAIL_COND_V_MSG(err, err, "Failed to load project config!");
	} else {
		ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED,
				"Godot version not set or project uses unsupported Godot version");
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
	GDREPackedData::get_singleton()->clear();
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
Error GDRESettings::set_encryption_key_string(const String &key_str) {
	String skey = key_str.replace_first("0x", "");
	ERR_FAIL_COND_V_MSG(!skey.is_valid_hex_number(false) || skey.size() < 64, ERR_INVALID_PARAMETER, "not a valid key");

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
	return OK;
}

Error GDRESettings::set_encryption_key(Vector<uint8_t> key) {
	ERR_FAIL_COND_V_MSG(key.size() < 32, ERR_INVALID_PARAMETER, "Key must be 32 bytes!");
	if (!set_key) {
		memcpy(old_key, script_encryption_key, 32);
	}
	memcpy(script_encryption_key, key.ptr(), 32);
	set_key = true;
	enc_key = key;
	enc_key_str = String::hex_encode_buffer(key.ptr(), 32);
	return OK;
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

Array GDRESettings::get_file_info_array(const Vector<String> &filters) {
	Array ret;
	bool no_filters = !filters.size();
	for (auto E : file_map) {
		if (no_filters) {
			ret.push_back(E.value);
			continue;
		}
		for (int j = 0; j < filters.size(); j++) {
			if (E.key.get_file().match(filters[j])) {
				ret.push_back(E.value);
				break;
			}
		}
	}
	return ret;
}

Vector<Ref<PackedFileInfo>> GDRESettings::get_file_info_list(const Vector<String> &filters) {
	Vector<Ref<PackedFileInfo>> ret;
	bool no_filters = !filters.size();
	for (auto E : file_map) {
		if (no_filters) {
			ret.push_back(E.value);
			continue;
		}
		for (int j = 0; j < filters.size(); j++) {
			if (E.key.get_file().match(filters[j])) {
				ret.push_back(E.value);
				break;
			}
		}
	}
	return ret;
}
// TODO: Overhaul all these pathing functions
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
		if (GDREPackedData::get_singleton()->has_path(p_path)) {
			return p_path;
		}
		res_path = localize_path(p_path, res_dir);
		if (res_path != p_path && GDREPackedData::get_singleton()->has_path(res_path)) {
			return res_path;
		}
		// localize_path did nothing
		if (!res_path.is_absolute_path()) {
			res_path = "res://" + res_path;
			if (GDREPackedData::get_singleton()->has_path(res_path)) {
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
		// version 3-4
		if (get_ver_major() >= 3) {
			if (remap_iinfo.size() > 0) {
				return true;
			}
		} else { // version 1-2
			if (current_pack->pcfg->is_loaded() && current_pack->pcfg->has_setting("remap/all")) {
				return true;
			}
		}
	}
	return false;
}

Dictionary GDRESettings::get_remaps(bool include_imports) const {
	Dictionary ret;
	if (is_pack_loaded()) {
		if (get_ver_major() >= 3) {
			for (auto E : remap_iinfo) {
				ret[E.key] = E.value->get_path();
			}
		} else {
			if (current_pack->pcfg->is_loaded() && current_pack->pcfg->has_setting("remap/all")) {
				PackedStringArray v2remaps = current_pack->pcfg->get_setting("remap/all", PackedStringArray());
				for (int i = 0; i < v2remaps.size(); i += 2) {
					ret[v2remaps[i]] = v2remaps[i + 1];
				}
			}
		}
		if (include_imports) {
			for (int i = 0; i < import_files.size(); i++) {
				Ref<ImportInfo> iinfo = import_files[i];
				ret[iinfo->get_source_file()] = iinfo->get_path();
			}
		}
	}
	return ret;
}

//only works on 2.x right now
bool GDRESettings::has_remap(const String &src, const String &dst) const {
	if (is_pack_loaded()) {
		if (get_ver_major() >= 3) {
			String remap_file = localize_path(src) + ".remap";
			if (remap_iinfo.has(remap_file)) {
				if (dst.is_empty()) {
					return true;
				}
				String dest_file = remap_iinfo[remap_file]->get_path();
				return dest_file == localize_path(dst);
			}
		} else {
			if (is_project_config_loaded() && current_pack->pcfg->has_setting("remap/all")) {
				PackedStringArray v2remaps = current_pack->pcfg->get_setting("remap/all", PackedStringArray());
				if ((v2remaps.has(localize_path(src)) && v2remaps.has(localize_path(dst)))) {
					return true;
				}
			}
		}
	}
	return false;
}

//only works on 2.x right now
Error GDRESettings::add_remap(const String &src, const String &dst) {
	ERR_FAIL_COND_V_MSG(!is_pack_loaded(), ERR_DATABASE_CANT_READ, "Pack not loaded!");
	if (get_ver_major() >= 3) {
		ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "Adding Remaps is not supported in 3.x-4.x packs yet!");
	} else {
		ERR_FAIL_COND_V_MSG(!is_project_config_loaded(), ERR_DATABASE_CANT_READ, "project config not loaded!");
		PackedStringArray v2remaps = current_pack->pcfg->get_setting("remap/all", PackedStringArray());
		v2remaps.push_back(localize_path(src));
		v2remaps.push_back(localize_path(dst));
		current_pack->pcfg->set_setting("remap/all", v2remaps);
	}
	return OK;
}

Error GDRESettings::remove_remap(const String &src, const String &dst, const String &output_dir) {
	ERR_FAIL_COND_V_MSG(!is_pack_loaded(), ERR_DATABASE_CANT_READ, "Pack not loaded!");
	ERR_FAIL_COND_V_MSG(!is_project_config_loaded(), ERR_DATABASE_CANT_READ, "project config not loaded!");
	PackedStringArray v2remaps = current_pack->pcfg->get_setting("remap/all", PackedStringArray());
	Error err;
	if (get_ver_major() >= 3) {
		ERR_FAIL_COND_V_MSG(output_dir.is_empty(), ERR_INVALID_PARAMETER, "Output directory must be specified for 3.x-4.x packs!");
		String remap_file = localize_path(src) + ".remap";
		if (remap_iinfo.has(remap_file)) {
			Ref<DirAccess> da = DirAccess::open(output_dir, &err);
			ERR_FAIL_COND_V_MSG(err, err, "Can't open directory " + output_dir);
			if (!dst.is_empty()) {
				String dest_file = remap_iinfo[remap_file]->get_path();
				if (dest_file != localize_path(dst)) {
					ERR_FAIL_V_MSG(ERR_DOES_NOT_EXIST, "Remap between" + src + " and " + dst + " does not exist!");
				}
			}
			remap_iinfo.erase(remap_file);
			return da->remove(remap_file.replace("res://", ""));
		}
		ERR_FAIL_V_MSG(ERR_DOES_NOT_EXIST, "Remap for " + src + " does not exist!");
	}
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

bool GDRESettings::has_project_setting(const String &p_setting) {
	ERR_FAIL_COND_V_MSG(!is_pack_loaded(), false, "Pack not loaded!");
	if (!is_project_config_loaded()) {
		WARN_PRINT("Attempted to check project setting " + p_setting + ", but no project config loaded");
		return false;
	}
	return current_pack->pcfg->has_setting(p_setting);
}

Variant GDRESettings::get_project_setting(const String &p_setting) {
	ERR_FAIL_COND_V_MSG(!is_pack_loaded(), Variant(), "Pack not loaded!");
	ERR_FAIL_COND_V_MSG(!is_project_config_loaded(), Variant(), "project config not loaded!");
	return current_pack->pcfg->get_setting(p_setting, Variant());
}

String GDRESettings::get_project_config_path() {
	ERR_FAIL_COND_V_MSG(!is_project_config_loaded(), String(), "project config not loaded!");
	return current_pack->pcfg->get_cfg_path();
}

String GDRESettings::get_log_file_path() {
	if (!logger) {
		return "";
	}
	return logger->get_path();
}

bool GDRESettings::is_headless() const {
	return headless;
}

String GDRESettings::get_sys_info_string() const {
	String OS_Name = OS::get_singleton()->get_distribution_name();
	String OS_Version = OS::get_singleton()->get_version();
	String adapter_name = RenderingServer::get_singleton()->get_video_adapter_name();
	String render_driver = OS::get_singleton()->get_current_rendering_driver_name();
	if (adapter_name.is_empty()) {
		adapter_name = "headless";
	} else {
		adapter_name += ", " + render_driver;
	}

	return OS_Name + " " + OS_Version + ", " + adapter_name;
}

Error GDRESettings::open_log_file(const String &output_dir) {
	String logfile = output_dir.path_join("gdre_export.log");
	Error err = logger->open_file(logfile);
	print_line("GDRE Tools " + String(GDRE_VERSION));
	print_line(get_sys_info_string());
	ERR_FAIL_COND_V_MSG(err == ERR_ALREADY_IN_USE, err, "Already logging to another file");
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not open log file " + logfile);
	return OK;
}

Error GDRESettings::close_log_file() {
	logger->close_file();
	return OK;
}

Array GDRESettings::get_import_files(bool copy) {
	if (!copy) {
		return import_files;
	}
	Array ifiles;
	for (int i = 0; i < import_files.size(); i++) {
		ifiles.push_back(ImportInfo::copy(import_files[i]));
	}
	return ifiles;
}

bool GDRESettings::has_file(const String &p_path) {
	return file_map.has(p_path);
}

Error GDRESettings::load_import_files() {
	Vector<String> file_names;
	ERR_FAIL_COND_V_MSG(!is_pack_loaded(), ERR_DOES_NOT_EXIST, "pack/dir not loaded!");
	static const Vector<String> v2wildcards = {
		"*.converted.*",
		"*.tex",
		"*.fnt",
		"*.msh",
		"*.scn",
		"*.res",
		"*.smp",
		"*.xl",
		"*.cbm",
		"*.pbm",
		"*.gdc"
	};
	static const Vector<String> v3wildcards = {
		"*.import",
		"*.gdc",
		"*.gde",
		"*.remap"
	};
	int _ver_major = get_ver_major();
	// version isn't set, we have to guess from contents of dir.
	// While the load_import_file() below will still have 0.0 set as the version,
	// load_from_file() will automatically load the binary resources to determine the version.
	if (_ver_major == 0) {
		_ver_major = get_ver_major_from_dir();
	}
	if (_ver_major == 2) {
		file_names = get_file_list(v2wildcards);
	} else if (_ver_major == 3 || _ver_major == 4) {
		file_names = get_file_list(v3wildcards);
	} else {
		ERR_FAIL_V_MSG(ERR_BUG, "Can't determine major version!");
	}
	bool should_load_md5 = _ver_major > 2 && get_file_info_list({ "*.md5" }).size() > 0;
	for (int i = 0; i < file_names.size(); i++) {
		String ext = file_names[i].get_extension();
		if (ext == "gdc" || ext == "gde") {
			code_files.push_back(file_names[i]);
		} else if (ext == "remap") {
			String ext2 = file_names[i].get_file().get_basename().get_extension();
			// ignore, we will be handling these when decompiling
			if (ext2 == "gd") {
				continue;
			}
			Error err = _load_import_file(file_names[i], should_load_md5);
			if (err && err != ERR_PRINTER_ON_FIRE) {
				WARN_PRINT("Can't load import file: " + file_names[i]);
				continue;
			}
			Ref<ImportInfoRemap> r_info = (Ref<ImportInfoRemap>)import_files.back();
			remap_iinfo.insert(file_names[i], r_info);
		} else {
			Error err = _load_import_file(file_names[i], should_load_md5);
			if (err && err != ERR_PRINTER_ON_FIRE) {
				WARN_PRINT("Can't load import file: " + file_names[i]);
			}
		}
	}
	return OK;
}

Error GDRESettings::_load_import_file(const String &p_path, bool should_load_md5) {
	Ref<ImportInfo> i_info = ImportInfo::load_from_file(p_path, get_ver_major(), get_ver_minor());
	ERR_FAIL_COND_V_MSG(i_info.is_null(), ERR_FILE_CANT_OPEN, "Failed to load import file " + p_path);

	import_files.push_back(i_info);
	// get source md5 from md5 file
	if (should_load_md5) {
		String src = i_info->get_dest_files()[0];
		// only files under the ".import" or ".godot" paths will have md5 files
		if (src.begins_with("res://.godot") || src.begins_with("res://.import")) {
			// sound.wav-<pathmd5>.smp -> sound.wav-<pathmd5>.md5
			String md5 = src.get_basename() + ".md5";
			while (true) {
				if (file_map.has(md5)) {
					break;
				}

				// image.png-<pathmd5>.s3tc.stex -> image.png-<pathmd5>.md5
				if (md5 != md5.get_basename()) {
					md5 = md5.get_basename();
					continue;
				}
				// we didn't find it
				md5 = "";
				break;
			}
			if (!md5.is_empty()) {
				Ref<FileAccess> file = FileAccess::open(md5, FileAccess::READ);
				ERR_FAIL_COND_V_MSG(file.is_null(), ERR_PRINTER_ON_FIRE, "Failed to load md5 file associated with import");
				String text = file->get_line();
				while (!text.begins_with("source") && !file->eof_reached()) {
					text = file->get_line();
				}
				if (!text.begins_with("source") || text.split("=").size() < 2) {
					WARN_PRINT("md5 file does not have source md5 info!");
					return ERR_PRINTER_ON_FIRE;
				}
				text = text.split("=")[1].strip_edges().replace("\"", "");
				if (!text.is_valid_hex_number(false)) {
					WARN_PRINT("source md5 hash is not valid!");
					return ERR_PRINTER_ON_FIRE;
				}
				i_info->set_source_md5(text);
			}
		}
	}
	return OK;
}
Error GDRESettings::load_import_file(const String &p_path) {
	return _load_import_file(p_path, false);
}

Ref<ImportInfo> GDRESettings::get_import_info_by_source(const String &p_path) {
	Ref<ImportInfo> iinfo;
	for (int i = 0; i < import_files.size(); i++) {
		iinfo = import_files[i];
		if (iinfo->get_source_file() == p_path) {
			return iinfo;
		}
	}
	// not found
	return Ref<ImportInfo>();
}

Ref<ImportInfo> GDRESettings::get_import_info_by_dest(const String &p_path) {
	Ref<ImportInfo> iinfo;
	for (int i = 0; i < import_files.size(); i++) {
		iinfo = import_files[i];
		if (iinfo->get_path() == p_path) {
			return iinfo;
		}
	}
	// not found
	return Ref<ImportInfo>();
}

Vector<String> GDRESettings::get_code_files() {
	return code_files;
}

bool GDRESettings::pack_has_project_config() {
	if (get_ver_major() == 2) {
		if (has_res_path("res://engine.cfb")) {
			return true;
		}
	} else if (get_ver_major() == 3 || get_ver_major() == 4) {
		if (has_res_path("res://project.binary")) {
			return true;
		}
	} else {
		WARN_PRINT("Unsupported godot version " + itos(get_ver_major()) + "...");
	}
	return false;
}

String GDRESettings::get_gdre_version() const {
	return GDRE_VERSION;
}

String GDRESettings::get_disclaimer_text() const {
	return String("Godot RE Tools, ") + String(GDRE_VERSION) + String(" \n\n") +
			get_disclaimer_body();
}

String GDRESettings::get_disclaimer_body() {
	return RTR(String("Resources, binary code and source code might be protected by copyright and trademark ") +
			"laws. Before using this software make sure that decompilation is not prohibited by the " +
			"applicable license agreement, permitted under applicable law or you obtained explicit " +
			"permission from the copyright owner.\n\n" +
			"The authors and copyright holders of this software do neither encourage nor condone " +
			"the use of this software, and disclaim any liability for use of the software in violation of " +
			"applicable laws.\n\n" +
			"This software in an alpha stage. Please report any bugs to the GitHub repository\n");
}

void GDRESettings::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load_pack", "p_path", "cmd_line_extract"), &GDRESettings::load_pack, DEFVAL(false));
	ClassDB::bind_method(D_METHOD("unload_pack"), &GDRESettings::unload_pack);
	ClassDB::bind_method(D_METHOD("get_gdre_resource_path"), &GDRESettings::get_gdre_resource_path);
	ClassDB::bind_method(D_METHOD("get_encryption_key"), &GDRESettings::get_encryption_key);
	ClassDB::bind_method(D_METHOD("get_encryption_key_string"), &GDRESettings::get_encryption_key_string);
	ClassDB::bind_method(D_METHOD("is_pack_loaded"), &GDRESettings::is_pack_loaded);
	ClassDB::bind_method(D_METHOD("_set_error_encryption", "is_encryption_error"), &GDRESettings::_set_error_encryption);
	ClassDB::bind_method(D_METHOD("set_encryption_key_string", "key"), &GDRESettings::set_encryption_key_string);
	ClassDB::bind_method(D_METHOD("set_encryption_key", "key"), &GDRESettings::set_encryption_key);
	ClassDB::bind_method(D_METHOD("reset_encryption_key"), &GDRESettings::reset_encryption_key);
	ClassDB::bind_method(D_METHOD("add_pack_info", "packinfo"), &GDRESettings::add_pack_info);
	ClassDB::bind_method(D_METHOD("add_pack_file", "f_info"), &GDRESettings::add_pack_file);
	ClassDB::bind_method(D_METHOD("get_file_list", "filters"), &GDRESettings::get_file_list, DEFVAL(Vector<String>()));
	ClassDB::bind_method(D_METHOD("get_file_info_array", "filters"), &GDRESettings::get_file_info_array, DEFVAL(Vector<String>()));
	ClassDB::bind_method(D_METHOD("get_pack_type"), &GDRESettings::get_pack_type);
	ClassDB::bind_method(D_METHOD("get_pack_path"), &GDRESettings::get_pack_path);
	ClassDB::bind_method(D_METHOD("get_pack_format"), &GDRESettings::get_pack_format);
	ClassDB::bind_method(D_METHOD("get_version_string"), &GDRESettings::get_version_string);
	ClassDB::bind_method(D_METHOD("get_ver_major"), &GDRESettings::get_ver_major);
	ClassDB::bind_method(D_METHOD("get_ver_minor"), &GDRESettings::get_ver_minor);
	ClassDB::bind_method(D_METHOD("get_ver_rev"), &GDRESettings::get_ver_rev);
	ClassDB::bind_method(D_METHOD("get_file_count"), &GDRESettings::get_file_count);
	ClassDB::bind_method(D_METHOD("globalize_path", "p_path", "resource_path"), &GDRESettings::globalize_path);
	ClassDB::bind_method(D_METHOD("localize_path", "p_path", "resource_path"), &GDRESettings::localize_path);
	ClassDB::bind_method(D_METHOD("set_project_path", "p_path"), &GDRESettings::set_project_path);
	ClassDB::bind_method(D_METHOD("get_project_path"), &GDRESettings::get_project_path);
	ClassDB::bind_method(D_METHOD("get_res_path", "p_path", "resource_dir"), &GDRESettings::get_res_path);
	ClassDB::bind_method(D_METHOD("has_res_path", "p_path", "resource_dir"), &GDRESettings::has_res_path);
	ClassDB::bind_method(D_METHOD("open_log_file", "output_dir"), &GDRESettings::open_log_file);
	ClassDB::bind_method(D_METHOD("get_log_file_path"), &GDRESettings::get_log_file_path);
	ClassDB::bind_method(D_METHOD("is_fs_path", "p_path"), &GDRESettings::is_fs_path);
	ClassDB::bind_method(D_METHOD("close_log_file"), &GDRESettings::close_log_file);
	ClassDB::bind_method(D_METHOD("get_remaps", "include_imports"), &GDRESettings::get_remaps, DEFVAL(true));
	ClassDB::bind_method(D_METHOD("has_any_remaps"), &GDRESettings::has_any_remaps);
	ClassDB::bind_method(D_METHOD("has_remap", "src", "dst"), &GDRESettings::has_remap);
	ClassDB::bind_method(D_METHOD("add_remap", "src", "dst"), &GDRESettings::add_remap);
	ClassDB::bind_method(D_METHOD("remove_remap", "src", "dst", "output_dir"), &GDRESettings::remove_remap);
	ClassDB::bind_method(D_METHOD("get_project_setting", "p_setting"), &GDRESettings::get_project_setting);
	ClassDB::bind_method(D_METHOD("has_project_setting", "p_setting"), &GDRESettings::has_project_setting);
	ClassDB::bind_method(D_METHOD("get_project_config_path"), &GDRESettings::get_project_config_path);
	ClassDB::bind_method(D_METHOD("get_cwd"), &GDRESettings::get_cwd);
	ClassDB::bind_method(D_METHOD("get_import_files", "copy"), &GDRESettings::get_import_files);
	ClassDB::bind_method(D_METHOD("has_file", "p_path"), &GDRESettings::has_file);
	ClassDB::bind_method(D_METHOD("load_import_files"), &GDRESettings::load_import_files);
	ClassDB::bind_method(D_METHOD("load_import_file", "p_path"), &GDRESettings::load_import_file);
	ClassDB::bind_method(D_METHOD("get_import_info_by_source", "p_path"), &GDRESettings::get_import_info_by_source);
	ClassDB::bind_method(D_METHOD("get_import_info_by_dest", "p_path"), &GDRESettings::get_import_info_by_dest);
	ClassDB::bind_method(D_METHOD("get_code_files"), &GDRESettings::get_code_files);
	ClassDB::bind_method(D_METHOD("get_exec_dir"), &GDRESettings::get_exec_dir);
	ClassDB::bind_method(D_METHOD("are_imports_loaded"), &GDRESettings::are_imports_loaded);
	ClassDB::bind_method(D_METHOD("is_project_config_loaded"), &GDRESettings::is_project_config_loaded);
	ClassDB::bind_method(D_METHOD("is_headless"), &GDRESettings::is_headless);
	ClassDB::bind_method(D_METHOD("get_sys_info_string"), &GDRESettings::get_sys_info_string);
	ClassDB::bind_method(D_METHOD("load_project_config"), &GDRESettings::load_project_config);
	ClassDB::bind_method(D_METHOD("save_project_config", "p_out_dir"), &GDRESettings::save_project_config);
	ClassDB::bind_method(D_METHOD("pack_has_project_config"), &GDRESettings::pack_has_project_config);
	ClassDB::bind_method(D_METHOD("get_gdre_version"), &GDRESettings::get_gdre_version);
	ClassDB::bind_method(D_METHOD("get_disclaimer_text"), &GDRESettings::get_disclaimer_text);
	// ClassDB::bind_method(D_METHOD("get_auto_display_scale"), &GDRESettings::get_auto_display_scale);
	// TODO: route this through GDRE Settings rather than GDRE Editor
	//ADD_SIGNAL(MethodInfo("write_log_message", PropertyInfo(Variant::STRING, "message")));
}

// This is at the bottom to account for the platform header files pulling in their respective OS headers and creating all sorts of issues

#ifdef WINDOWS_ENABLED
#include "platform/windows/os_windows.h"
#endif
#ifdef LINUXBSD_ENABLED
#include "platform/linuxbsd/os_linuxbsd.h"
#endif
#ifdef MACOS_ENABLED
#include "drivers/unix/os_unix.h"
#endif
#ifdef UWP_ENABLED
#include "platform/uwp/os_uwp.h"
#endif
#ifdef WEB_ENABLED
#include "platform/web/os_web.h"
#endif
#if defined(__ANDROID__)
#include "platform/android/os_android.h"
#endif
#ifdef IPHONE_ENABLED
#include "platform/iphone/os_iphone.h"
#endif
// A hack to add another logger to the OS singleton
template <class T>
class GDREOS : public T {
	static_assert(std::is_base_of<OS, T>::value, "T must derive from OS");

public:
	void _add_logger(Logger *p_logger) { T::add_logger(p_logger); }
};

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
#ifdef LINUXBSD_ENABLED
	else if (os_name == "Linux" || os_name.find("BSD") == -1) {
		GDREOS<OS_LinuxBSD> *_gdre_os = static_cast<GDREOS<OS_LinuxBSD> *>(os_singleton);
		_gdre_os->_add_logger(logger);
	}
#endif
#ifdef MACOS_ENABLED
	else if (os_name == "macOS") {
		GDREOS<OS_Unix> *_gdre_os = static_cast<GDREOS<OS_Unix> *>(os_singleton);
		_gdre_os->_add_logger(logger);
	}
#endif
#ifdef UWP_ENABLED
	else if (os_name == "UWP") {
		GDREOS<OS_UWP> *_gdre_os = static_cast<GDREOS<OS_UWP> *>(os_singleton);
		_gdre_os->_add_logger(logger);
	}
#endif
#ifdef WEB_ENABLED
	else if (os_name == "Web") {
		GDREOS<OS_Web> *_gdre_os = static_cast<GDREOS<OS_Web> *>(os_singleton);
		_gdre_os->_add_logger(logger);
	}
#endif
#if defined(__ANDROID__) // the rest of these are probably unnecessary
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