#pragma once
#include "core/io/resource.h"
#include "core/object/ref_counted.h"
#include "exporters/export_report.h"
#include "utility/import_info.h"

class ResourceExporter : public RefCounted {
	GDCLASS(ResourceExporter, RefCounted);

protected:
	static void _bind_methods();
	static Error ensure_dir(const String &dir);
	static int get_ver_major(const String &res_path);
	static Error write_to_file(const String &path, const Vector<uint8_t> &data);

public:
	virtual Error export_file(const String &out_path, const String &res_path) = 0;
	virtual Ref<ExportReport> export_resource(const String &output_dir, Ref<ImportInfo> import_infos) = 0;
	virtual bool handles_import(const String &importer, const String &resource_type = String()) const = 0;
	virtual bool supports_multithread() const;
};
