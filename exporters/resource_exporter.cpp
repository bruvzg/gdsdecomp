#include "resource_exporter.h"
#include "compat/resource_loader_compat.h"
#include "core/error/error_list.h"
#include "core/error/error_macros.h"
#include "core/io/dir_access.h"

Error ResourceExporter::ensure_dir(const String &dst_dir) {
	Error err;
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	ERR_FAIL_COND_V(da.is_null(), ERR_FILE_CANT_OPEN);
	err = da->make_dir_recursive(dst_dir);
	return err;
}

Error ResourceExporter::write_to_file(const String &path, const Vector<uint8_t> &data) {
	Error err = ensure_dir(path.get_base_dir());
	ERR_FAIL_COND_V_MSG(err, err, "Failed to create directory for " + path);
	Ref<FileAccess> file = FileAccess::open(path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(file.is_null(), !err ? ERR_FILE_CANT_WRITE : err, "Cannot open file '" + path + "' for writing.");
	file->store_buffer(data.ptr(), data.size());
	file->flush();
	return OK;
}

int ResourceExporter::get_ver_major(const String &res_path) {
	Error err;
	auto info = ResourceCompatLoader::get_resource_info(res_path, "", &err);
	ERR_FAIL_COND_V_MSG(err != OK, 0, "Failed to get resource info for " + res_path);
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