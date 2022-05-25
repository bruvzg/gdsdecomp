#include "utility/gdre_cli_main.h"

Error GDRECLIMain::open_log(const String &path) {
	return GDRESettings::get_singleton()->open_log_file(path);
}

Error GDRECLIMain::close_log() {
	return GDRESettings::get_singleton()->close_log_file();
}

String GDRECLIMain::get_cli_abs_path(const String &path) {
	if (path.is_absolute_path()) {
		return path;
	}
	String exec_path = GDRESettings::get_singleton()->get_exec_dir();
	return exec_path.plus_file(path).replace("\\", "/");
}

void GDRECLIMain::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open_log"), &GDRECLIMain::open_log);
	ClassDB::bind_method(D_METHOD("close_log"), &GDRECLIMain::close_log);
	ClassDB::bind_method(D_METHOD("get_cli_abs_path"), &GDRECLIMain::get_cli_abs_path);
}
