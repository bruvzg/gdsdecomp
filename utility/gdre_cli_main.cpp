#include "gdre_cli_main.h"
#include "editor/gdre_version.gen.h"

GDRECLIMain *GDRECLIMain::singleton = nullptr;

Error GDRECLIMain::open_log(const String &path) {
	return GDRESettings::get_singleton()->open_log_file(path);
}
Error GDRECLIMain::load_pack(const String &path) {
	return GDRESettings::get_singleton()->load_pack(path);
}
Error GDRECLIMain::close_log() {
	if (GDRESettings::get_singleton() == nullptr) {
		return OK;
	}
	String path = GDRESettings::get_singleton()->get_log_file_path();
	if (path == "") {
		return OK;
	}
	Error err = GDRESettings::get_singleton()->close_log_file();
	print_line("Log file written to " + path);
	print_line("Please include this file when reporting an issue!");
	return err;
}

String GDRECLIMain::get_cli_abs_path(const String &path) {
	if (path.is_absolute_path()) {
		return path;
	}
	String exec_path = GDRESettings::get_singleton()->get_exec_dir();
	return exec_path.path_join(path).simplify_path();
}

Error GDRECLIMain::copy_dir(const String &src_path, const String dst_path) {
	Ref<DirAccess> f = DirAccess::open(src_path);
	ERR_FAIL_COND_V_MSG(f.is_null(), ERR_CANT_OPEN, "Can't open " + src_path);
	f->make_dir_recursive(dst_path);
	Error err = f->copy_dir(src_path, dst_path);
	return err;
}
Error GDRECLIMain::set_key(const String &key) {
	GDRESettings::get_singleton()->set_encryption_key(key);
	return OK;
}

void GDRECLIMain::_bind_methods() {
	ClassDB::bind_method(D_METHOD("is_loaded"), &GDRECLIMain::is_loaded);
	ClassDB::bind_method(D_METHOD("get_file_count"), &GDRECLIMain::get_file_count);
	ClassDB::bind_method(D_METHOD("get_loaded_files"), &GDRECLIMain::get_loaded_files);
	ClassDB::bind_method(D_METHOD("get_engine_version"), &GDRECLIMain::get_engine_version);
	ClassDB::bind_method(D_METHOD("get_gdre_version"), &GDRECLIMain::get_gdre_version);

	ClassDB::bind_method(D_METHOD("clear_data"), &GDRECLIMain::clear_data);
	ClassDB::bind_method(D_METHOD("set_key"), &GDRECLIMain::set_key);

	ClassDB::bind_method(D_METHOD("copy_dir"), &GDRECLIMain::copy_dir);
	ClassDB::bind_method(D_METHOD("open_log"), &GDRECLIMain::open_log);
	ClassDB::bind_method(D_METHOD("close_log"), &GDRECLIMain::close_log);
	ClassDB::bind_method(D_METHOD("load_pack"), &GDRECLIMain::load_pack);

	ClassDB::bind_method(D_METHOD("get_cli_abs_path"), &GDRECLIMain::get_cli_abs_path);
}

Vector<uint8_t> GDRECLIMain::get_key() const {
	return GDRESettings::get_singleton()->get_encryption_key();
}

String GDRECLIMain::get_key_str() const {
	return GDRESettings::get_singleton()->get_encryption_key_string();
}

int GDRECLIMain::get_file_count() {
	return GDRESettings::get_singleton()->get_file_count();
}

void GDRECLIMain::clear_data() {
	GDRESettings::get_singleton()->unload_pack();
}

bool GDRECLIMain::is_loaded() {
	return GDRESettings::get_singleton()->is_pack_loaded();
}

Vector<String> GDRECLIMain::get_loaded_files() {
	return GDRESettings::get_singleton()->get_file_list();
}

String GDRECLIMain::get_engine_version() {
	return GDRESettings::get_singleton()->get_version_string();
}
String GDRECLIMain::get_gdre_version() {
	return GDRE_VERSION;
}
GDRECLIMain::GDRECLIMain() {
}
GDRECLIMain::~GDRECLIMain() {
}