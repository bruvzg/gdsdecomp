#include "gdre_cli_main.h"

Error GDRECLIMain::open_log(const String &path) {
	return GDRESettings::get_singleton()->open_log_file(path);
}

Error GDRECLIMain::close_log() {
	String path = GDRESettings::get_singleton()->get_log_file_path();
	if (path == "") {
		return OK;
	}
	GDRESettings::get_singleton()->close_log_file();
	print_line("Log file written to " + path);
	print_line("Please include this file when reporting an issue!");
	return GDRESettings::get_singleton()->close_log_file();
}

String GDRECLIMain::get_cli_abs_path(const String &path) {
	if (path.is_absolute_path()) {
		return path;
	}
	String exec_path = GDRESettings::get_singleton()->get_exec_dir();
	return exec_path.plus_file(path).replace("\\", "/");
}
Error GDRECLIMain::copy_dir(const String &src_path, const String dst_path) {
	Ref<DirAccess> f = DirAccess::open(src_path);
	ERR_FAIL_COND_V_MSG(f.is_null(), ERR_CANT_OPEN, "Can't open " + src_path);
	Error err = f->copy_dir(src_path, dst_path);
	return err;
}
Error GDRECLIMain::set_key(const String &key) {
	GDRESettings::get_singleton()->set_encryption_key(key);
	return OK;
}

void GDRECLIMain::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_key"), &GDRECLIMain::set_key);
	ClassDB::bind_method(D_METHOD("copy_dir"), &GDRECLIMain::copy_dir);
	ClassDB::bind_method(D_METHOD("open_log"), &GDRECLIMain::open_log);
	ClassDB::bind_method(D_METHOD("close_log"), &GDRECLIMain::close_log);
	ClassDB::bind_method(D_METHOD("get_cli_abs_path"), &GDRECLIMain::get_cli_abs_path);
}
