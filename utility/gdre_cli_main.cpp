#include "utility/gdre_cli_main.h"

Error GDRECLIMain::open_log(const String &path) {
	return GDRESettings::get_singleton()->open_log_file(path);
}
Error GDRECLIMain::close_log() {
	return GDRESettings::get_singleton()->close_log_file();
}

void GDRECLIMain::_bind_methods() {
	ClassDB::bind_method(D_METHOD("open_log"), &GDRECLIMain::open_log);
	ClassDB::bind_method(D_METHOD("close_log"), &GDRECLIMain::close_log);
}