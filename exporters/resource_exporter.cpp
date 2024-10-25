#include "resource_exporter.h"
#include "compat/resource_loader_compat.h"
#include "core/io/dir_access.h"
Error ResourceExporter::ensure_dir(const String &dst_dir) {
	Error err;
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	ERR_FAIL_COND_V(da.is_null(), ERR_FILE_CANT_OPEN);
	err = da->make_dir_recursive(dst_dir);
	return err;
}

int ResourceExporter::get_ver_major(const String &res_path) {
	auto info = ResourceCompatLoader::get_resource_info(res_path);
	return info.ver_major; // Placeholder return value
}

bool ResourceExporter::supports_multithread() const {
	return true;
}

void ResourceExporter::_bind_methods() {
	// ClassDB::bind_method(D_METHOD("export_to_path", "out_path", "import_info"), &ResourceExporter::export_to_path);
	// ClassDB::bind_method(D_METHOD("export_resource", "output_dir", "import_infos"), &ResourceExporter::export_resource);
	// ClassDB::bind_method(D_METHOD("handles_import", "importer", "resource_type"), &ResourceExporter::handles_import);
}