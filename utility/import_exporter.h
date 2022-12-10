#ifndef IMPORT_EXPORTER_H
#define IMPORT_EXPORTER_H

#include "compat/resource_import_metadatav2.h"
#include "import_info.h"
#include "pcfg_loader.h"

#include "editor/gdre_progress.h"

#include "core/io/file_access.h"
#include "core/io/resource.h"
#include "core/io/resource_importer.h"
#include "core/object/object.h"
#include "core/object/ref_counted.h"
#include "core/templates/rb_map.h"

class ImportExporter : public RefCounted {
	GDCLASS(ImportExporter, RefCounted)
	int session_files_total = 0;
	bool opt_bin2text = true;
	bool opt_export_textures = true;
	bool opt_export_samples = true;
	bool opt_export_ogg = true;
	bool opt_export_mp3 = true;
	bool opt_lossy = true;
	bool opt_export_jpg = true;
	bool opt_export_webp = true;
	bool opt_rewrite_imd_v2 = true;
	bool opt_rewrite_imd_v3 = true;
	bool opt_decompile = true;
	bool opt_only_decompile = false;
	bool opt_write_md5_files = true;

	bool had_encryption_error = false;
	Vector<String> decompiled_scripts;
	Vector<String> failed_scripts;
	String translation_export_message;
	Vector<Ref<ImportInfo>> lossy_imports;
	Vector<Ref<ImportInfo>> rewrote_metadata;
	Vector<Ref<ImportInfo>> failed_rewrite_md;
	Vector<Ref<ImportInfo>> failed_rewrite_md5;
	Vector<String> unsupported_types;

	Vector<Ref<ImportInfo>> failed;
	Vector<Ref<ImportInfo>> success;
	Vector<Ref<ImportInfo>> not_converted;

	Error export_texture(const String &output_dir, Ref<ImportInfo> &iinfo);
	Error export_sample(const String &output_dir, Ref<ImportInfo> &iinfo);
	Error rewrite_import_data(const String &rel_dest_path, const String &output_dir, const Ref<ImportInfo> &iinfo);
	Error _convert_bitmap(const String &output_dir, const String &p_path, const String &p_dst, bool lossy);
	Error export_translation(const String &output_dir, Ref<ImportInfo> &iinfo);

	Error _convert_tex(const String &output_dir, const String &p_path, const String &p_dst, bool lossy);
	Error _convert_tex_to_jpg(const String &output_dir, const String &p_path, const String &p_dst);

	static Error ensure_dir(const String &dst_dir);
	static Vector<String> get_v2_wildcards();
	String _get_path(const String &output_dir, const String &p_path);
	void report_unsupported_resource(const String &type, const String &format_name, const String &import_path, bool suppress_warn = false, bool suppress_print = false);

protected:
	static void _bind_methods();

public:
	String get_session_notes();
	String get_detected_unsupported_resource_string();
	Error recreate_plugin_config(const String &output_dir, const String &plugin_dir);
	Error recreate_plugin_configs(const String &output_dir);
	Error remap_resource(const String &output_dir, Ref<ImportInfo> &iinfo);
	Error convert_res_txt_2_bin(const String &output_dir, const String &p_path, const String &p_dst);
	Error convert_res_bin_2_txt(const String &output_dir, const String &p_path, const String &p_dst);
	Error convert_tex_to_png(const String &output_dir, const String &p_path, const String &p_dst);
	Error convert_sample_to_wav(const String &output_dir, const String &p_path, const String &p_dst);
	Error convert_oggstr_to_ogg(const String &output_dir, const String &p_path, const String &p_dst);
	Error convert_mp3str_to_mp3(const String &output_dir, const String &p_path, const String &p_dst);
	Error decompile_scripts(const String &output_dir);

	Error _export_imports(const String &output_dir, const Vector<String> &files_to_export, EditorProgressGDDC *pr, String &error_string);
	Error export_imports(const String &output_dir = "", const Vector<String> &files_to_export = {});
	String get_totals();
	void print_report();
	String get_editor_message();
	String get_report();
	void reset_log();
	void reset();
	ImportExporter();
	~ImportExporter();
};

#endif
