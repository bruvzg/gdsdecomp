#pragma once
#include "core/object/ref_counted.h"
#include "utility/import_info.h"
#include <sys/types.h>

class ExportReport : public RefCounted {
	GDCLASS(ExportReport, RefCounted);

public:
	enum MetadataStatus {
		NOT_DIRTY,
		NOT_IMPORTABLE,
		DEPENDENCY_CHANGED,
		REWRITTEN,
		FAILED,
		MD5_FAILED
	};

private:
	Ref<ImportInfo> import_info;
	String message;
	String source_path;
	String new_source_path;
	String saved_path;
	String unsupported_format_type;
	Error error = OK;
	ImportInfo::LossType loss_type = ImportInfo::LossType::LOSSLESS;
	MetadataStatus rewrote_metadata = NOT_DIRTY;

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

	void set_loss_type(ImportInfo::LossType p_loss_type) { loss_type = p_loss_type; }
	ImportInfo::LossType get_loss_type() const { return loss_type; }

	void set_unsupported_format_type(const String &p_type) { unsupported_format_type = p_type; }
	String get_unsupported_format_type() const { return unsupported_format_type; }

	void set_rewrote_metadata(MetadataStatus p_status) { rewrote_metadata = p_status; }
	MetadataStatus get_rewrote_metadata() const { return rewrote_metadata; }

	ExportReport() {}
	ExportReport(Ref<ImportInfo> p_import_info) :
			import_info(p_import_info), source_path(p_import_info->get_source_file()), new_source_path(import_info->get_export_dest()) {}
};