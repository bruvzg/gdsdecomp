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
	Error load_import_file(const String &p_path);
	Error load_import(FileAccess * f, const String &p_path);
	Error load_import_file_v2(const String &p_path);
	Error load_import_v2(FileAccess * f, const String& p_path);
protected:
	static void _bind_methods();

public:
	Error convert_res_bin_2_txt(const String &output_dir, const String &p_path, const String &p_dst);
	Error convert_v2tex_to_png(const String &output_dir, const String &p_path, const String &p_dst);
	Error convert_v3stex_to_png(const String &output_dir, const String &p_path, const String &p_dst);
	Error convert_sample_to_wav(const String &output_dir, const String &p_path, const String &p_dst);
	Error convert_oggstr_to_ogg(const String &output_dir, const String &p_path, const String &p_dst);
	Error convert_mp3str_to_mp3(const String &output_dir, const String &p_path, const String &p_dst);
	Error load_import_files(const String &dir, const uint32_t ver_major);
	Array get_import_files();
	int convert_imports(const String &output_dir = "");
};

#endif