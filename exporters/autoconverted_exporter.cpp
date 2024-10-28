#include "autoconverted_exporter.h"
#include "compat/resource_loader_compat.h"
#include "exporters/export_report.h"

Error AutoConvertedExporter::export_file(const String &p_dest_path, const String &p_src_path) {
	return ResourceCompatLoader::to_text(p_src_path, p_dest_path);
}

Ref<ExportReport> AutoConvertedExporter::export_resource(const String &output_dir, Ref<ImportInfo> import_infos) {
	// Check if the exporter can handle the given importer and resource type

	String src_path = import_infos->get_path();
	String dst_path = output_dir.path_join(import_infos->get_export_dest().replace("res://", ""));
	Ref<ExportReport> report = memnew(ExportReport(import_infos));
	Error err = export_file(dst_path, src_path);
	report->set_error(err);
	report->set_saved_path(dst_path);
	return report;
}

bool AutoConvertedExporter::handles_import(const String &importer, const String &resource_type) const {
	return importer == "autoconverted";
}

void AutoConvertedExporter::get_handled_types(List<String> *out) const {
}

void AutoConvertedExporter::get_handled_importers(List<String> *out) const {
	out->push_back("autoconverted");
}