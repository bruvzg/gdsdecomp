#pragma once
#include "core/error/error_list.h"
#include "core/io/resource.h"
#include "core/object/ref_counted.h"
#include "exporters/export_report.h"
#include "utility/common.h"
#include "utility/import_exporter.h"
#include "utility/import_info.h"

class ResourceExporter : public RefCounted {
	GDCLASS(ResourceExporter, RefCounted);

protected:
	static void _bind_methods();
	static int get_ver_major(const String &res_path);
	static Error write_to_file(const String &path, const Vector<uint8_t> &data);

public:
	virtual Error export_file(const String &out_path, const String &res_path);
	virtual Ref<ExportReport> export_resource(const String &output_dir, Ref<ImportInfo> import_infos);
	virtual bool handles_import(const String &importer, const String &resource_type = String()) const;
	virtual void get_handled_types(List<String> *out) const;
	virtual void get_handled_importers(List<String> *out) const;
	virtual bool supports_multithread() const;
};

class Exporter : public Object {
	GDCLASS(Exporter, Object);

	friend class ImportExporter;
	enum {
		MAX_EXPORTERS = 64,
	};
	static Ref<ResourceExporter> exporters[MAX_EXPORTERS];
	static int exporter_count;

protected:
	static void _bind_methods();

public:
	static void add_exporter(Ref<ResourceExporter> exporter, bool at_front = false);
	static void remove_exporter(Ref<ResourceExporter> exporter);
	static Ref<ExportReport> export_resource(const String &output_dir, Ref<ImportInfo> import_infos);
	static Error export_file(const String &out_path, const String &res_path);
	static Ref<ResourceExporter> get_exporter(const String &importer, const String &type);
};