#include "export_report.h"

void ExportReport::_bind_methods() {
	ClassDB::bind_method(D_METHOD("set_message", "message"), &ExportReport::set_message);
	ClassDB::bind_method(D_METHOD("get_message"), &ExportReport::get_message);
	ClassDB::bind_method(D_METHOD("set_import_info", "import_info"), &ExportReport::set_import_info);
	ClassDB::bind_method(D_METHOD("get_import_info"), &ExportReport::get_import_info);
	ClassDB::bind_method(D_METHOD("set_source_path", "source_path"), &ExportReport::set_source_path);
	ClassDB::bind_method(D_METHOD("get_source_path"), &ExportReport::get_source_path);
	ClassDB::bind_method(D_METHOD("set_new_source_path", "new_source_parth"), &ExportReport::set_new_source_path);
	ClassDB::bind_method(D_METHOD("get_new_source_path"), &ExportReport::get_new_source_path);
	ClassDB::bind_method(D_METHOD("set_saved_path", "saved_path"), &ExportReport::set_saved_path);
	ClassDB::bind_method(D_METHOD("get_saved_path"), &ExportReport::get_saved_path);
	ClassDB::bind_method(D_METHOD("set_success", "error"), &ExportReport::set_error);
	ClassDB::bind_method(D_METHOD("get_success"), &ExportReport::get_error);
	ClassDB::bind_method(D_METHOD("set_is_lossless", "is_lossless"), &ExportReport::set_is_lossless);
	ClassDB::bind_method(D_METHOD("get_is_lossless"), &ExportReport::get_is_lossless);

	// properties
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "message"), "set_message", "get_message");
	ADD_PROPERTY(PropertyInfo(Variant::OBJECT, "import_info", PROPERTY_HINT_RESOURCE_TYPE, "ImportInfo"), "set_import_info", "get_import_info");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "source_path"), "set_source_path", "get_source_path");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "new_source_path"), "set_new_source_path", "get_new_source_path");
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "saved_path"), "set_saved_path", "get_saved_path");
	ADD_PROPERTY(PropertyInfo(Variant::INT, "success"), "set_success", "get_success");
	ADD_PROPERTY(PropertyInfo(Variant::BOOL, "is_lossless"), "set_is_lossless", "get_is_lossless");
}