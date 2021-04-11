#ifndef IMPORT_EXPORTER_H
#define IMPORT_EXPORTER_H

#include "core/templates/map.h"
#include "core/io/resource.h"
#include "core/object/object.h"
#include "core/os/file_access.h"
#include "core/object/reference.h"
#include "core/io/resource_importer.h"
#include "resource_import_metadatav2.h"
#include "import_info.h"

class ImportExporter: public Reference {
	GDCLASS(ImportExporter, Reference)
	Array files;
	String project_dir;
	bool opt_bin2text = true;
	bool opt_export_textures = true;
	bool opt_export_samples = true;
	bool opt_export_ogg = true;
	bool opt_export_mp3 = true;
	bool opt_lossy = true;
	bool opt_rewrite_imd = true;
	Vector<String> files_lossy_exported;
	Vector<String> files_rewrote_metadata;

	Error load_import_file(const String &p_path);
	Error load_import(FileAccess * f, const String &p_path);
	Error load_import_file_v2(const String &p_path);
	Error rewrite_v2_import_metadata(const String &p_path, const String &p_dst, const String &p_res_name, const String &output_dir);
	Error export_texture(const String &output_dir, const Ref<ImportInfo> &iinfo);
	Error export_sample(const String &output_dir, const Ref<ImportInfo> &iinfo);
	Error _convert_tex_to_png(const String &output_dir, const String &p_path, const String &p_dst, String * r_name);
	Error rewrite_import_data(const String &rel_dest_path, const String &output_dir, const Ref<ImportInfo> &iinfo);
	Ref<ResourceImportMetadatav2> change_v2import_data(const String &p_path, const String &rel_dest_path, const String &p_res_name, const String &output_dir, const bool change_extension);

protected:
	static void _bind_methods();

public:
	Error convert_res_bin_2_txt(const String &output_dir, const String &p_path, const String &p_dst);
	Error convert_tex_to_png(const String &output_dir, const String &p_path, const String &p_dst);
	Error convert_sample_to_wav(const String &output_dir, const String &p_path, const String &p_dst);
	Error convert_oggstr_to_ogg(const String &output_dir, const String &p_path, const String &p_dst);
	Error convert_mp3str_to_mp3(const String &output_dir, const String &p_path, const String &p_dst);
	Error load_import_files(const String &dir, const uint32_t ver_major);
	Array get_import_files();
	Ref<ImportInfo> get_import_info(const String &p_path);
	Error export_imports(const String &output_dir = "");
	
	void reset();
	ImportExporter();
	~ImportExporter();
};

#endif