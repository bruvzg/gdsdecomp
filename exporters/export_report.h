#pragma once
#include "core/object/ref_counted.h"
#include "utility/import_info.h"

class ExportReport : public RefCounted {
	GDCLASS(ExportReport, RefCounted);
	String message;
	Ref<ImportInfo> import_info;
	String source_path;
	String new_source_path;
	String saved_path;
	Error error = OK;
	bool is_lossless = true;

protected:
	static void _bind_methods();

public:
	// setters and getters
	void set_message(const String &p_message) { message = p_message; }
	String get_message() const { return message; }

	void set_import_info(const Ref<ImportInfo> &p_import_info) { import_info = p_import_info; }
	Ref<ImportInfo> get_import_info() const { return import_info; }

	void set_source_path(const String &p_source_path) { source_path = p_source_path; }
	String get_source_path() const { return source_path; }

	void set_new_source_path(const String &p_saved_path) { new_source_path = p_saved_path; }
	String get_new_source_path() const { return (new_source_path.is_empty() ? source_path : new_source_path); }

	void set_saved_path(const String &p_saved_path) { saved_path = p_saved_path; }
	String get_saved_path() const { return saved_path; }

	void set_error(Error p_error) { error = p_error; }
	Error get_error() const { return error; }

	void set_is_lossless(bool p_is_lossless) { is_lossless = p_is_lossless; }
	bool get_is_lossless() const { return is_lossless; }

	ExportReport(Ref<ImportInfo> p_import_info) :
			import_info(p_import_info), source_path(p_import_info->get_source_file()), new_source_path(import_info->get_export_dest()) {}
};