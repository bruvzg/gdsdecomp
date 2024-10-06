
#include "import_exporter.h"
#include "bytecode/bytecode_tester.h"
#include "bytecode/bytecode_versions.h"
#include "compat/oggstr_loader_compat.h"
#include "compat/optimized_translation_extractor.h"
#include "compat/resource_loader_compat.h"
#include "compat/sample_loader_compat.h"
#include "compat/texture_loader_compat.h"
#include "core/error/error_list.h"
#include "core/string/print_string.h"
#include "gdre_settings.h"
#include "pcfg_loader.h"
#include "util_functions.h"

#include "core/crypto/crypto_core.h"
#include "core/io/config_file.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/math/audio_frame.h"
#include "core/os/os.h"
#include "core/string/optimized_translation.h"
#include "core/variant/variant_parser.h"
#include "core/version_generated.gen.h"
#include "modules/gltf/gltf_document.h"
#include "modules/minimp3/audio_stream_mp3.h"
#include "modules/regex/regex.h"
#include "modules/vorbis/audio_stream_ogg_vorbis.h"
#include "scene/resources/audio_stream_wav.h"
#include "scene/resources/font.h"
#include "thirdparty/minimp3/minimp3_ex.h"

GDRESettings *get_settings() {
	return GDRESettings::get_singleton();
}

int get_ver_major() {
	return get_settings()->get_ver_major();
}
int get_ver_minor() {
	return get_settings()->get_ver_minor();
}
int get_ver_rev() {
	return get_settings()->get_ver_rev();
}

// export all the imported resources
Error ImportExporter::export_imports(const String &p_out_dir, const Vector<String> &files_to_export) {
	String t;
	return _export_imports(p_out_dir, files_to_export, nullptr, t);
}

Ref<ImportExporterReport> ImportExporter::get_report() {
	return report;
}

/**
Sort the scenes so that they are exported last
 */
struct IInfoComparator {
	int is_glb_scene(const Ref<ImportInfo> &a) const {
		return a->get_importer() == "scene" && !a->is_auto_converted() ? 1 : 0;
	}
	bool operator()(const Ref<ImportInfo> &a, const Ref<ImportInfo> &b) const {
		return is_glb_scene(a) < is_glb_scene(b);
	}
};

Error ImportExporter::_export_imports(const String &p_out_dir, const Vector<String> &files_to_export, EditorProgressGDDC *pr, String &error_string) {
	reset_log();

	report = Ref<ImportExporterReport>(memnew(ImportExporterReport(get_settings()->get_version_string())));
	report->log_file_location = get_settings()->get_log_file_path();
	ERR_FAIL_COND_V_MSG(!get_settings()->is_pack_loaded(), ERR_DOES_NOT_EXIST, "pack/dir not loaded!");
	uint64_t last_progress_upd = OS::get_singleton()->get_ticks_usec();
	String output_dir = !p_out_dir.is_empty() ? p_out_dir : get_settings()->get_project_path();
	Error err = OK;
	if (opt_lossy) {
		WARN_PRINT_ONCE("Converting lossy imports, you may lose fidelity for indicated assets when re-importing upon loading the project");
	}
	// TODO: make this use "copy"
	Array _files = get_settings()->get_import_files();
	Vector<Ref<ImportInfo>> files;

	// Put scene files at the end, otherwise it slows down the export tremendously
	for (int i = 0; i < _files.size(); i++) {
		Ref<ImportInfo> thing = _files[i];
		if (thing->get_importer() != "scene") {
			files.push_back(_files[i]);
		}
	}
	for (int i = 0; i < _files.size(); i++) {
		Ref<ImportInfo> thing = _files[i];
		if (thing->get_importer() == "scene") {
			files.push_back(_files[i]);
		}
	}

	Ref<DirAccess> dir = DirAccess::open(output_dir);
	bool partial_export = files_to_export.size() > 0;
	report->session_files_total = partial_export ? files_to_export.size() : files.size();
	if (opt_decompile) {
		decompile_scripts(output_dir);
		// This only works if we decompile the scripts first
		recreate_plugin_configs(output_dir);
	}
	if (get_ver_major() >= 4) {
		// we need to re-save all the iinfo files to the output directory if they are dirty
		for (int i = 0; i < files.size(); i++) {
			Ref<ImportInfo> iinfo = files[i];
			if (iinfo->is_dirty()) {
				String dest = output_dir.path_join(iinfo->get_import_md_path().get_basename());
				err = iinfo->save_to(output_dir.path_join(iinfo->get_import_md_path().replace("res://", "")));
				if (err && err != ERR_UNAVAILABLE) {
					print_line("Failed to save import info " + iinfo->get_path());
				}
			}
		}
	}
	// files.sort_custom<IInfoComparator>();

	for (int i = 0; i < files.size(); i++) {
		Ref<ImportInfo> iinfo = files[i];
		String path = iinfo->get_path();
		String source = iinfo->get_source_file();
		String type = iinfo->get_type();
		String importer = iinfo->get_importer();
		auto loss_type = iinfo->get_import_loss_type();
		// If files_to_export is empty, then we export everything
		if (partial_export) {
			auto dest_files = iinfo->get_dest_files();
			bool has_path = false;
			for (auto dest : dest_files) {
				if (files_to_export.has(dest)) {
					has_path = true;
					break;
				}
			}
			if (!has_path) {
				continue;
			}
		}
		if (pr) {
			if (OS::get_singleton()->get_ticks_usec() - last_progress_upd > 10000) {
				last_progress_upd = OS::get_singleton()->get_ticks_usec();
				bool cancel = pr->step(path, i, true);
				if (cancel) {
					return ERR_PRINTER_ON_FIRE;
				}
			}
		}
		if (loss_type != ImportInfo::LOSSLESS) {
			if (!opt_lossy) {
				report->lossy_imports.push_back(iinfo);
				print_line("Not converting lossy import " + path);
				continue;
			}
		}
		// ***** Set export destination *****
		iinfo->set_export_dest(iinfo->get_source_file());
		bool should_rewrite_metadata = false;
		// This is a Godot asset that was imported outside of project directory
		if (!iinfo->get_source_file().begins_with("res://")) {
			if (get_ver_major() <= 2) {
				// import_md_path is the resource path in v2
				iinfo->set_export_dest(String("res://.assets").path_join(iinfo->get_import_md_path().get_base_dir().path_join(iinfo->get_source_file().get_file()).replace("res://", "")));
			} else {
				// import_md_path is the .import/.remap path in v3-v4
				iinfo->set_export_dest(iinfo->get_import_md_path().get_basename());
				// If the source_file path was not actually in the project structure, save it elsewhere
				if (iinfo->get_source_file().find(iinfo->get_export_dest().replace("res://", "")) == -1) {
					iinfo->set_export_dest(iinfo->get_export_dest().replace("res://", "res://.assets"));
				}
			}
			should_rewrite_metadata = true;
		}
		String src_ext = iinfo->get_source_file().get_extension().to_lower();
		bool not_exported = false;
		// ***** Export resource *****
		if (opt_export_textures && importer == "texture") {
			// Right now we only convert 2d image textures
			auto tex_type = TextureLoaderCompat::recognize(path, &err);
			switch (tex_type) {
				case TextureLoaderCompat::FORMAT_V2_IMAGE_TEXTURE:
				case TextureLoaderCompat::FORMAT_V3_STREAM_TEXTURE2D:
				case TextureLoaderCompat::FORMAT_V4_COMPRESSED_TEXTURE2D: {
					// Export texture
					err = export_texture(output_dir, iinfo);
				} break;
				case TextureLoaderCompat::FORMAT_NOT_TEXTURE:
					if (err == ERR_FILE_UNRECOGNIZED) {
						WARN_PRINT("Import of type " + type + " is not a texture(?): " + path);
					} else {
						WARN_PRINT("Failed to load texture " + type + " " + path);
					}
					report->not_converted.push_back(iinfo);
					break;
				default:
					report_unsupported_resource(type, src_ext, path);
					report->not_converted.push_back(iinfo);
					not_exported = true;
			}
		} else if (opt_export_samples && (importer == "sample" || importer == "wav")) {
			err = export_sample(output_dir, iinfo);
		} else if (opt_export_ogg && (importer == "ogg_vorbis" || importer == "oggvorbisstr")) {
			err = convert_oggstr_to_ogg(output_dir, iinfo->get_path(), iinfo->get_export_dest());
		} else if (opt_export_mp3 && importer == "mp3") {
			err = convert_mp3str_to_mp3(output_dir, iinfo->get_path(), iinfo->get_export_dest());
		} else if (importer == "bitmap") {
			err = export_texture(output_dir, iinfo);
		} else if ((opt_bin2text && iinfo->is_auto_converted())
				//|| (opt_bin2text && iinfo->get_source_file().get_extension() == "tres" && iinfo->get_source_file().get_extension() == "res") ||
				// (opt_bin2text && iinfo->get_importer() == "scene" && iinfo->get_source_file().get_extension() == "tscn")
		) {
			// We don't currently support converting old 2.x xml resources
			if (src_ext == "xml") {
				WARN_PRINT_ONCE("Conversion of Godot 2.x xml resource files currently unimplemented");
				report_unsupported_resource(type, src_ext, path);
				report->not_converted.push_back(iinfo);
				not_exported = true;
			}
			err = convert_res_bin_2_txt(output_dir, iinfo->get_path(), iinfo->get_export_dest());
			// v2-v3 export left the autoconverted resource in the main path, remove it
			if (get_ver_major() <= 3 && !err) {
				dir->remove(iinfo->get_path().replace("res://", ""));
			}
		} else if (importer == "scene" && !iinfo->is_auto_converted()) {
			// escn files are scenes exported from a blender plugin in a godot compatible format
			// These are the only ones we support at the moment
			if (src_ext == "escn") {
				err = convert_res_bin_2_txt(output_dir, iinfo->get_path(), iinfo->get_export_dest());
			} else {
				if (iinfo->get_ver_major() <= 3) {
					WARN_PRINT_ONCE("Export of Godot 2/3.x models/imported scenes currently unimplemented");
					report_unsupported_resource(itos(iinfo->get_ver_major()) + ".x " + type, src_ext, path);
					report->not_converted.push_back(iinfo);
					not_exported = true;
				} else {
					err = export_scene(output_dir, iinfo);
				}
			}
		} else if (importer == "font_data_dynamic") {
			err = export_fontfile(output_dir, iinfo);
		} else if (importer == "csv_translation") {
			err = export_translation(output_dir, iinfo);
		} else if (importer == "wavefront_obj") {
			WARN_PRINT_ONCE("Export of obj meshes currently unimplemented");
			report_unsupported_resource(type, src_ext, path);
			report->not_converted.push_back(iinfo);
			not_exported = true;
		} else {
			report_unsupported_resource(type, src_ext, path);
			report->not_converted.push_back(iinfo);
			not_exported = true;
		}

		// ****REWRITE METADATA****
		if (err == ERR_PRINTER_ON_FIRE || ((err == OK && should_rewrite_metadata) && iinfo->is_import())) {
			if (iinfo->get_ver_major() <= 2 && opt_rewrite_imd_v2) {
				// TODO: handle v2 imports with more than one source, like atlas textures
				err = rewrite_import_source(iinfo->get_export_dest(), output_dir, iinfo);
			} else if (iinfo->get_ver_major() >= 3 && opt_rewrite_imd_v3 && (iinfo->get_source_file().find(iinfo->get_export_dest().replace("res://", "")) != -1)) {
				// Currently, we only rewrite the import data for v3 if the source file was somehow recorded as an absolute file path,
				// But is still in the project structure
				err = rewrite_import_source(iinfo->get_export_dest(), output_dir, iinfo);
			} else if (iinfo->is_dirty()) {
				err = iinfo->save_to(output_dir.path_join(iinfo->get_import_md_path().replace("res://", "")));
				if (iinfo->get_export_dest() != iinfo->get_source_file()) {
					// we still report this as unwritten so that we can report to the user that it was saved to a non-original path
					err = ERR_PRINTER_ON_FIRE;
				}
			}
			// if we didn't rewrite the metadata, the err will still be ERR_PRINTER_ON_FIRE
			// if we failed, it won't be OK
			if (err != OK) {
				if (err == ERR_PRINTER_ON_FIRE) {
					print_line("Did not rewrite import metadata for " + iinfo->get_source_file());
				} else {
					print_line("Failed to rewrite import metadata for " + iinfo->get_source_file());
				}
				err = ERR_DATABASE_CANT_WRITE;
			} else {
				// we successfully rewrote the import data
				err = ERR_PRINTER_ON_FIRE;
			}
			// saved to non-original path, but we exporter knows we can't rewrite the metadata
		} else if (err == ERR_DATABASE_CANT_WRITE) {
			print_line("Did not rewrite import metadata for " + iinfo->get_source_file());
		} else if (err == OK && iinfo->is_dirty()) {
			err = iinfo->save_to(output_dir.path_join(iinfo->get_import_md_path().replace("res://", "")));
			if (err == OK) {
				err = ERR_PRINTER_ON_FIRE;
			} else {
				print_line("Failed to rewrite import metadata for " + iinfo->get_source_file());
				err = ERR_DATABASE_CANT_WRITE;
			}
		}

		// We continue down here so that modified import metadata is saved
		if (not_exported) {
			continue;
		}

		// write md5 files
		if (opt_write_md5_files && iinfo->is_import() && (err == OK || err == ERR_PRINTER_ON_FIRE) && get_ver_major() > 2) {
			err = ((Ref<ImportInfoModern>)iinfo)->save_md5_file(output_dir);
			if (err && err != ERR_PRINTER_ON_FIRE) {
				err = ERR_LINK_FAILED;
			} else {
				err = OK;
			}
		}
		// ***** Record export result *****

		// the following are successful exports, but we failed to rewrite metadata or write md5 files
		// we had to rewrite the import metadata
		if (err == ERR_PRINTER_ON_FIRE) {
			report->rewrote_metadata.push_back(iinfo);
			err = OK;
			// necessary to rewrite import metadata but failed
		} else if (err == ERR_DATABASE_CANT_WRITE) {
			report->failed_rewrite_md.push_back(iinfo);
			err = OK;
		} else if (err == ERR_LINK_FAILED) {
			report->failed_rewrite_md5.push_back(iinfo);
			err = OK;
		}

		if (err == ERR_UNAVAILABLE) {
			// already reported in exporters below
			report->not_converted.push_back(iinfo);
		} else if (err != OK) {
			report->failed.push_back(iinfo);
			print_line("Failed to convert " + type + " resource " + path);
		} else {
			if (loss_type != ImportInfo::LOSSLESS) {
				report->lossy_imports.push_back(iinfo);
			}
			report->success.push_back(iinfo);
		}
		// remove remaps
		if (!err && get_settings()->has_any_remaps()) {
			if (get_settings()->has_remap(iinfo->get_export_dest(), iinfo->get_path())) {
				get_settings()->remove_remap(iinfo->get_export_dest(), iinfo->get_path(), output_dir);
			}
		}
	}

	if (get_settings()->is_project_config_loaded()) { // some pcks do not have project configs
		if (get_settings()->save_project_config(output_dir) != OK) {
			print_line("ERROR: Failed to save project config!");
		} else {
			// Remove binary project config, as editors will load from it instead of the text one
			dir->remove(get_settings()->get_project_config_path().get_file());
		}
	}
	report->print_report();
	return OK;
}

Error ImportExporter::decompile_scripts(const String &p_out_dir) {
	GDScriptDecomp *decomp;
	// we have to remove remaps if they exist
	bool has_remaps = get_settings()->has_any_remaps();
	Vector<String> code_files = get_settings()->get_code_files();
	if (code_files.is_empty()) {
		return OK;
	}
	uint64_t revision = 0;

	// we don't consider the dev versions here, only the release/beta versions
	switch (get_ver_major()) {
		case 1:
			switch (get_ver_minor()) {
				case 0:
					revision = 0xe82dc40;
					break;
				case 1:
					revision = 0x65d48d6;
					break;
			}
			break;
		case 2:
			switch (get_ver_minor()) {
				case 0:
					revision = 0x23441ec;
					break;
				case 1: {
					switch (get_settings()->get_ver_rev()) {
						case 0:
						case 1:
							revision = 0x7124599;
							break;
						case 2:
							revision = 0x85585c7;
							break;
						case 3:
						case 4:
						case 5:
						case 6:
							revision = 0xed80f45;
							break;
					}
				} break;
			}
			break;
		case 3:
			switch (get_ver_minor()) {
				case 0:
					revision = 0x54a2ac;
					break;
				case 1: {
					// They broke compatibility for 3.1.x in successive patch releases.
					if (get_settings()->get_version_string().contains("beta")) {
						revision = 0x1ca61a3;
					} else {
						switch (get_settings()->get_ver_rev()) {
							case 0:
								revision = 0x1a36141;
								break;
							case 1:
								revision = 0x514a3fb;
								break;
						}
					}
				} break;
				case 2:
				case 3:
				case 4:
					revision = 0x5565f55;
					break;
				case 5:
					revision = 0xa7aad78;
					break;
				default:
					// We do not anticipate further GSScript changes in 3.x because GDScript 2.0 in 4.x can't be backported
					// but just in case...
					WARN_PRINT("Unsupported version 3." + itos(get_ver_minor()) + "." + itos(get_ver_rev()) + " of Godot detected");
					WARN_PRINT("If your scripts are mangled, please report this on the Github issue tracker");
					revision = 0xa7aad78;
					break;
			}
			break;
		case 4:
			switch (get_ver_minor()) {
				// Compiled mode was removed in 4.0; if this is part of a 4.0 project, might be in one of the pre GDScript 2.0 4.0-dev bytecodes
				// but we can't really detect which mutually incompatible revision it is (there are several), so we just fail
				case 0:
					ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED, "Support for Godot 4.0 pre-release dev GDScript not implemented, failed to decompile");
					break;
				case 1: // might be added back in 4.1; if so, this will need to be updated
					ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED, "Support for Godot 4.1 GDScript not yet implemented, failed to decompile");
					break;
				case 2:
				case 3:
					revision = 0x77af6ca;
					break;
				default:
					ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED, "Unsupported version 4." + itos(get_ver_minor()) + "." + itos(get_ver_rev()) + " of Godot detected, failed to decompile");
					break;
			}
			break;
		default:
			ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED, "Unknown version, failed to decompile");
	}
	if (revision > 0) {
		decomp = create_decomp_for_commit(revision);
	} else {
		ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED, "Unknown version, failed to decompile");
	}

	print_line("Script version " + get_settings()->get_version_string() + " (rev 0x" + String::num_int64(revision, 16) + ") detected");
	Error err;
	for (String f : code_files) {
		String dest_file = f.replace(".gdc", ".gd").replace(".gde", ".gd");
		Ref<DirAccess> da = DirAccess::open(p_out_dir);
		print_verbose("decompiling " + f);
		bool encrypted = false;
		if (f.get_extension().to_lower() == "gde") {
			encrypted = true;
			err = decomp->decompile_byte_code_encrypted(f, get_settings()->get_encryption_key());
		} else {
			err = decomp->decompile_byte_code(f);
		}
		if (err) {
			String err_string = decomp->get_error_message();
			memdelete(decomp);
			report->failed_scripts.push_back(f);
			// TODO: make it not fail hard on the first script that fails to decompile
			if (encrypted) {
				report->had_encryption_error = true;
				ERR_FAIL_V_MSG(err, "error decompiling encrypted script " + f + ": " + err_string);
			} else {
				ERR_FAIL_V_MSG(err, "error decompiling " + f + ": " + err_string);
			}
		} else {
			String text = decomp->get_script_text();
			String out_path = p_out_dir.path_join(dest_file.replace("res://", ""));
			Ref<FileAccess> fa = FileAccess::open(out_path, FileAccess::WRITE);
			if (fa.is_null()) {
				report->failed_scripts.push_back(f);
				memdelete(decomp);
				ERR_FAIL_V_MSG(ERR_FILE_CANT_WRITE, "error failed to save " + f);
			}
			fa->store_string(text);
			da->remove(f.replace("res://", ""));
			if (has_remaps && get_settings()->has_remap(f, dest_file)) {
				get_settings()->remove_remap(f, dest_file);
			}
			// TODO: make "remove_remap" do this instead
			if (da->file_exists(f.replace(".gdc", ".gd.remap").replace("res://", ""))) {
				da->remove(f.replace(".gdc", ".gd.remap").replace("res://", ""));
			}
			print_verbose("successfully decompiled " + f);
			report->decompiled_scripts.push_back(f);
		}
	}
	memdelete(decomp);
	// save changed config file
	if (get_settings()->is_project_config_loaded()) { // some game pcks do not have project configs
		err = get_settings()->save_project_config(p_out_dir);
	}

	if (err) {
		WARN_PRINT("Failed to save changed project config!");
	}
	return OK;
}

Error ImportExporter::recreate_plugin_config(const String &output_dir, const String &plugin_dir) {
	Error err;
	static const Vector<String> wildcards = { "*.gd" };
	String rel_plugin_path = String("addons").path_join(plugin_dir);
	String abs_plugin_path = output_dir.path_join(rel_plugin_path);
	auto gd_scripts = gdreutil::get_recursive_dir_list(abs_plugin_path, wildcards, false);
	String main_script;

	auto copy_gdext_dll_func = [&](String gdextension_path) -> Error {
		bool is_gdnative = gdextension_path.ends_with(".gdnlib");
		String lib_section_name = is_gdnative ? "entry" : "libraries";
		// check if directory; we can't do anything here if we're working from an unpacked dir
		if (get_settings()->get_pack_type() == GDRESettings::PackInfo::PackType::DIR) {
			return OK;
		}
		String parent_dir = get_settings()->get_pack_path().get_base_dir();
		// open gdextension_path like a config file
		Vector<String> libs_to_find;
		List<String> platforms;
		Dictionary lib_to_parentdir;
		Dictionary lib_to_platform;
		// what the decomp is running on
		String our_platform = OS::get_singleton()->get_name().to_lower();
		Ref<ConfigFile> cf;
		cf.instantiate();
		cf->load(gdextension_path);
		if (!cf->has_section(lib_section_name)) {
			if (cf->has_section("entry")) {
				lib_section_name = "entry";
			} else if (cf->has_section("libraries")) {
				lib_section_name = "libraries";
			} else {
				WARN_PRINT("Failed to find library section in " + gdextension_path);
				return ERR_BUG;
			}
		}
		cf->get_section_keys(lib_section_name, &platforms);
		auto add_path_func = [&](String path, String platform) mutable {
			String plugin_pardir = path.get_base_dir();
			if (!plugin_pardir.is_absolute_path()) {
				plugin_pardir = rel_plugin_path.path_join(plugin_pardir);
			} else {
				plugin_pardir = plugin_pardir.replace("res://", "");
			}
			libs_to_find.push_back(path.get_file());
			lib_to_parentdir[path.get_file()] = plugin_pardir;
			auto splits = platform.split(".");
			if (!splits.is_empty()) {
				platform = splits[0];
			}
			platform = platform.to_lower();
			// normalize old platform names
			if (platform == "x11") {
				platform = "linux";
			} else if (platform == "osx") {
				platform = "macos";
			}
			lib_to_platform[path.get_file()] = platform;
		};
		for (String &platform : platforms) {
			add_path_func(cf->get_value(lib_section_name, platform), platform);
		}
		platforms.clear();
		if (cf->has_section("dependencies")) {
			cf->get_section_keys("dependencies", &platforms);
		}
		for (String &platform : platforms) {
			Array keys;
			if (!is_gdnative) {
				Dictionary val = cf->get_value("dependencies", platform);
				// the keys in the dict are the library names
				keys = val.keys();
			} else {
				// they're just arrays in gdnlib
				keys = cf->get_value("dependencies", platform);
			}
			for (int i = 0; i < keys.size(); i++) {
				add_path_func(keys[i], platform);
			}
		}
		// search for the libraries
		Vector<String> lib_paths;
		// auto paths = gdreutil::get_recursive_dir_list(parent_dir, libs_to_find, true);

		// get a non-recursive dir listing
		{
			Ref<DirAccess> da = DirAccess::open(parent_dir, &err);
			ERR_FAIL_COND_V_MSG(err, ERR_FILE_CANT_OPEN, "Failed to open directory " + parent_dir);
			da->list_dir_begin();
			String f = da->get_next();
			while (!f.is_empty()) {
				if (f != "." && f != "..") {
					if (libs_to_find.has(f)) {
						lib_paths.push_back(parent_dir.path_join(f));
					}
				}
				f = da->get_next();
			}
		}
		if (lib_paths.size() == 0) {
			// if we're on MacOS, try one path up
			if (parent_dir.get_file() == "Resources") {
				parent_dir = parent_dir.get_base_dir();
				lib_paths = gdreutil::get_recursive_dir_list(parent_dir, libs_to_find, true);
			}
			if (lib_paths.size() == 0) {
				WARN_PRINT("Failed to find gdextension libraries for plugin " + plugin_dir);
				return ERR_BUG;
			}
		}
		// copy all of the libraries to the plugin directory
		Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		bool found_our_platform = false;
		for (String path : lib_paths) {
			String lib_name = path.get_file();
			String lib_dir_path = output_dir.path_join(lib_to_parentdir[lib_name]);
			if (!da->dir_exists(lib_dir_path)) {
				ERR_FAIL_COND_V_MSG(da->make_dir_recursive(lib_dir_path), ERR_FILE_CANT_WRITE, "Failed to make plugin directory " + lib_dir_path);
			}
			String dest_path = lib_dir_path.path_join(lib_name);
			ERR_FAIL_COND_V_MSG(da->copy(path, dest_path), ERR_FILE_CANT_WRITE, "Failed to copy library " + path + " to " + dest_path);
			if (lib_to_platform[lib_name] == our_platform) {
				found_our_platform = true;
			}
		}
		if (!found_our_platform) {
			WARN_PRINT("Failed to find library for our platform for plugin " + plugin_dir);
			return ERR_PRINTER_ON_FIRE;
		}
		return OK;
	};
	bool found_our_platform = true;
	if (get_ver_major() >= 3) {
		// check if this is a gdextension/gdnative plugin
		static const Vector<String> gdext_wildcards = { "*.gdextension", "*.gdnlib" };
		auto gdexts = gdreutil::get_recursive_dir_list(abs_plugin_path, gdext_wildcards, true);
		for (String gdext : gdexts) {
			err = copy_gdext_dll_func(gdext);
			if (err) {
				found_our_platform = false;
			}
		}
		// gdextension/gdnative plugins may not have a main script, and no plugin.cfg
		if (gd_scripts.is_empty()) {
			return found_our_platform ? OK : ERR_PRINTER_ON_FIRE;
		}
	}

	for (int j = 0; j < gd_scripts.size(); j++) {
		String gd_script_abs_path = abs_plugin_path.path_join(gd_scripts[j]);
		String gd_text = FileAccess::get_file_as_string(gd_script_abs_path, &err);
		ERR_FAIL_COND_V_MSG(err, err, "failed to open gd_script " + gd_script_abs_path + "!");
		if (gd_text.find("extends EditorPlugin") != -1) {
			main_script = gd_scripts[j];
			break;
		}
	}
	ERR_FAIL_COND_V_MSG(main_script == "", ERR_FILE_NOT_FOUND, "Failed to find main script for plugin " + plugin_dir + "!");
	String plugin_cfg_text = String("[plugin]\n\n") +
			"name=\"" + plugin_dir.replace("_", " ").replace(".", " ") + "\"\n" +
			"description=\"" + plugin_dir.replace("_", " ").replace(".", " ") + " plugin\"\n" +
			"author=\"Unknown\"\n" +
			"version=\"1.0\"\n" +
			"script=\"" + main_script + "\"";
	Ref<FileAccess> f = FileAccess::open(abs_plugin_path.path_join("plugin.cfg"), FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err, err, "can't open plugin.cfg for writing");
	f->store_string(plugin_cfg_text);
	print_line("Recreated plugin config for " + plugin_dir);
	return found_our_platform ? OK : ERR_PRINTER_ON_FIRE;
}

// Recreates the "plugin.cfg" files for each plugin to avoid loading errors.
Error ImportExporter::recreate_plugin_configs(const String &output_dir) {
	Error err;
	if (!DirAccess::exists(output_dir.path_join("addons"))) {
		return OK;
	}
	Vector<String> dirs;
	Ref<DirAccess> da = DirAccess::open(output_dir.path_join("addons"), &err);
	da->list_dir_begin();
	String f = da->get_next();
	while (!f.is_empty()) {
		if (f != "." && f != ".." && da->current_is_dir()) {
			dirs.append(f);
		}
		f = da->get_next();
	}
	da->list_dir_end();
	for (int i = 0; i < dirs.size(); i++) {
		if (dirs[i].contains("godotsteam")) {
			report->godotsteam_detected = true;
		}
		err = recreate_plugin_config(output_dir, dirs[i]);
		if (err == ERR_PRINTER_ON_FIRE) {
			// we successfully copied the dlls, but failed to find one for our platform
			WARN_PRINT("Failed to find library for this platform for plugin " + dirs[i]);
			report->failed_gdnative_copy.push_back(dirs[i]);
		} else if (err) {
			WARN_PRINT("Failed to recreate plugin.cfg for " + dirs[i]);
			report->failed_plugin_cfg_create.push_back(dirs[i]);
		}
	}
	return OK;
}

Error ImportExporter::export_fontfile(const String &output_dir, Ref<ImportInfo> &iinfo) {
	Error err;
	Ref<FontFile> fontfile = ResourceLoader::load(iinfo->get_path(), "", ResourceFormatLoader::CACHE_MODE_IGNORE, &err);
	ERR_FAIL_COND_V_MSG(err, err, "Failed to load font file " + iinfo->get_path());
	String out_path = output_dir.path_join(iinfo->get_export_dest().replace("res://", ""));
	err = ensure_dir(out_path.get_base_dir());
	ERR_FAIL_COND_V(err, err);
	Ref<FileAccess> f = FileAccess::open(out_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V(err, err);
	ERR_FAIL_COND_V(f.is_null(), ERR_FILE_CANT_WRITE);
	// just get the raw data and write it; TTF files are stored as raw data in the fontdata file
	PackedByteArray data = fontfile->get_data();
	f->store_buffer(data.ptr(), data.size());
	f->flush();
	return OK;
}

#define TEST_TR_KEY(key)                          \
	test = default_translation->get_message(key); \
	if (test == s) {                              \
		return key;                               \
	}                                             \
	key = key.to_upper();                         \
	test = default_translation->get_message(key); \
	if (test == s) {                              \
		return key;                               \
	}                                             \
	key = key.to_lower();                         \
	test = default_translation->get_message(key); \
	if (test == s) {                              \
		return key;                               \
	}

String guess_key_from_tr(String s, Ref<Translation> default_translation) {
	static const Vector<String> prefixes = { "$$", "##", "TR_", "KEY_TEXT_" };
	String key = s;
	String test;
	TEST_TR_KEY(key);
	String str = s;
	//remove punctuation
	str = str.replace("\n", "").replace(".", "").replace("…", "").replace("!", "").replace("?", "");
	str = str.strip_escapes().strip_edges().replace(" ", "_");
	key = str;
	TEST_TR_KEY(key);
	// Try adding prefixes
	for (String prefix : prefixes) {
		key = prefix + str;
		TEST_TR_KEY(key);
	}
	// failed
	return "";
}

Error ImportExporter::export_translation(const String &output_dir, Ref<ImportInfo> &iinfo) {
	Error err;
	ResourceFormatLoaderCompat rlc;
	// translation files are usually imported from one CSV and converted to multiple "<LOCALE>.translation" files
	String default_locale = get_settings()->pack_has_project_config() && get_settings()->has_project_setting("locale/fallback")
			? get_settings()->get_project_setting("locale/fallback")
			: "en";
	Vector<Ref<Translation>> translations;
	Vector<Vector<StringName>> translation_messages;
	Ref<Translation> default_translation;
	Vector<StringName> default_messages;
	String header = "key";
	Vector<StringName> keys;

	for (String path : iinfo->get_dest_files()) {
		Ref<Translation> tr = rlc.load(path, "", &err);
		ERR_FAIL_COND_V_MSG(err != OK, err, "Could not load translation file " + iinfo->get_path());
		ERR_FAIL_COND_V_MSG(!tr.is_valid(), err, "Translation file " + iinfo->get_path() + " was not valid");
		String locale = tr->get_locale();
		header += "," + locale;
		List<StringName> message_list;
		Vector<StringName> messages;
		if (tr->get_class_name() == "OptimizedTranslation") {
			Ref<OptimizedTranslation> otr = tr;
			Ref<OptimizedTranslationExtractor> ote;
			ote.instantiate();
			ote->set("locale", locale);
			ote->set("hash_table", otr->get("hash_table"));
			ote->set("bucket_table", otr->get("bucket_table"));
			ote->set("strings", otr->get("strings"));
			ote->get_message_value_list(&message_list);
			for (auto message : message_list) {
				messages.push_back(message);
			}
		} else {
			// We have a real translation class, get the keys
			if (locale == default_locale) {
				List<StringName> key_list;
				tr->get_message_list(&key_list);
				for (auto key : key_list) {
					keys.push_back(key);
				}
			}
			Dictionary msgdict = tr->get("messages");
			Array values = msgdict.values();
			for (int i = 0; i < values.size(); i++) {
				messages.push_back(values[i]);
			}
		}
		if (locale == default_locale) {
			default_messages = messages;
			default_translation = tr;
		}
		translation_messages.push_back(messages);
		translations.push_back(tr);
	}
	// We can't recover the keys from Optimized translations, we have to guess
	int missing_keys = 0;

	if (default_translation.is_null()) {
		if (translations.size() == 1) {
			// this is one of the rare dynamic translation schemes, which we don't support currently
			report_unsupported_resource("Translation", "Dynamic multi-csv", iinfo->get_path());
			return ERR_UNAVAILABLE;
		}
		ERR_FAIL_V_MSG(ERR_FILE_MISSING_DEPENDENCIES, "No default translation found for " + iinfo->get_path());
	}
	if (keys.size() == 0) {
		for (const StringName &s : default_messages) {
			String key = guess_key_from_tr(s, default_translation);
			if (key.is_empty()) {
				missing_keys++;
				keys.push_back("<MISSING KEY " + s + ">");
			} else {
				keys.push_back(key);
			}
		}
	}
	header += "\n";
	if (missing_keys) {
		iinfo->set_export_dest("res://.assets/" + iinfo->get_source_file().replace("res://", ""));
	}
	String output_path = output_dir.simplify_path().path_join(iinfo->get_export_dest().replace("res://", ""));
	err = ensure_dir(output_path.get_base_dir());
	ERR_FAIL_COND_V(err, err);
	Ref<FileAccess> f = FileAccess::open(output_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V(err, err);
	ERR_FAIL_COND_V(f.is_null(), ERR_FILE_CANT_WRITE);
	// Set UTF-8 BOM (required for opening with Excel in UTF-8 format, works with all Godot versions)
	f->store_8(0xef);
	f->store_8(0xbb);
	f->store_8(0xbf);
	f->store_string(header);
	for (int i = 0; i < keys.size(); i++) {
		Vector<String> line_values;
		line_values.push_back(keys[i]);
		for (auto messages : translation_messages) {
			line_values.push_back(messages[i]);
		}
		f->store_csv_line(line_values, ",");
	}
	f->flush();
	f = Ref<FileAccess>();
	if (missing_keys) {
		report->translation_export_message += "WARNING: Could not recover " + itos(missing_keys) + " keys for translation.csv" + "\n";
		report->translation_export_message += "Saved " + iinfo->get_source_file().get_file() + " to " + iinfo->get_export_dest() + "\n";
		WARN_PRINT("Could not guess all keys in translation.csv");
	}
	print_line("Recreated translation.csv");
	return missing_keys ? ERR_DATABASE_CANT_WRITE : OK;
}

Error _export_scene(const String &output_dir, Ref<ImportInfo> &iinfo) {
	Error err;
	// All 3.x scenes that were imported from scenes/models SHOULD be compatible with 4.x
	// The "escn" format basically force Godot to have compatibility with 3.x scenes
	// This will also pull in any dependencies that were created by the importer (like textures and materials, which should also be similarly compatible)
	Ref<PackedScene> scene = ResourceLoader::load(iinfo->get_path(), "", ResourceFormatLoader::CACHE_MODE_IGNORE, &err);
	ERR_FAIL_COND_V_MSG(err, err, "Failed to load scene " + iinfo->get_path());
	// GLTF export can result in inaccurate models
	// save it under .assets, which won't be picked up for import by the godot editor
	String new_path = iinfo->get_export_dest().replace("res://", "res://.assets/");
	// we only export glbs
	if (new_path.get_extension().to_lower() != "glb") {
		new_path = new_path.get_basename() + ".glb";
	}
	iinfo->set_export_dest(new_path);
	String out_path = output_dir.path_join(iinfo->get_export_dest().replace("res://", ""));
	err = ImportExporter::ensure_dir(out_path.get_base_dir());
	Node *root;
	{
		List<String> deps;
		Ref<GLTFDocument> doc;
		doc.instantiate();
		Ref<GLTFState> state;
		state.instantiate();
		int32_t flags = 0;
		flags |= 16; // EditorSceneFormatImporter::IMPORT_USE_NAMED_SKIN_BINDS;
		root = scene->instantiate();
		err = doc->append_from_scene(root, state, flags);
		if (err) {
			memdelete(root);
			ERR_FAIL_COND_V_MSG(err, err, "Failed to append scene " + iinfo->get_path() + " to glTF document");
		}
		err = doc->write_to_filesystem(state, out_path);
	}
	memdelete(root);
	ERR_FAIL_COND_V_MSG(err, err, "Failed to write glTF document to " + out_path);
	return OK;
}

Error ImportExporter::export_scene(const String &output_dir, Ref<ImportInfo> &iinfo) {
	Error err;
	report->exported_scenes = true;
	Vector<uint64_t> texture_uids;
	{
		// real load here
		List<String> get_deps;
		// We need to preload any Texture resources that are used by the scene with our own loader
		ResourceLoader::get_dependencies(iinfo->get_path(), &get_deps, true);
		HashMap<String, Vector<String>> get_deps_map;
		for (auto &i : get_deps) {
			auto splits = i.split("::");
			if (splits.size() < 3) {
				continue;
			}
			get_deps_map.insert(splits[2], splits);
		}
		// Check if the deps have any bad resources that we can't load
		for (auto &failed_info : report->failed) {
			if (get_deps_map.has(failed_info->get_source_file())) {
				print_line(vformat("Scene %s has known bad dependencies (%s), refusing to export.", iinfo->get_path(), failed_info->get_path()));
				return ERR_FILE_MISSING_DEPENDENCIES;
			}
		}

		TextureLoaderCompat tlc;
		tlc.glft_export = true; // overides `get_image()` for glTF export
		Vector<Ref<Texture>> textures;
		for (auto &E : get_deps_map) {
			String dep = E.key;
			auto &splits = E.value;
			if (splits[1].contains("Texture")) {
				Ref<ImportInfo> tex_iinfo = get_settings()->get_import_info_by_source(dep);
				if (tex_iinfo.is_null()) {
					WARN_PRINT("Failed to find import info for texture " + dep + " for scene " + iinfo->get_path());
					continue;
				}
				Ref<Texture> texture = tlc.load_texture(tex_iinfo->get_path(), &err);
				if (err || texture.is_null()) {
					WARN_PRINT("Failed to load texture " + tex_iinfo->get_path() + " for scene " + iinfo->get_path());
					continue;
				}
#ifdef TOOLS_ENABLED
				texture->set_import_path(tex_iinfo->get_path());
#endif
				texture->set_path(dep);
				if (!splits[0].is_empty()) {
					auto id = ResourceUID::get_singleton()->text_to_id(splits[0]);
					if (id != ResourceUID::INVALID_ID) {
						if (!ResourceUID::get_singleton()->has_id(id)) {
							ResourceUID::get_singleton()->add_id(id, splits[2]);
						} else {
							ResourceUID::get_singleton()->set_id(id, splits[2]);
						}
						texture_uids.push_back(id);
					}
				}
				textures.push_back(texture);
			}
		}
		err = _export_scene(output_dir, iinfo);
	}
	// remove the UIDs
	for (uint64_t id : texture_uids) {
		ResourceUID::get_singleton()->remove_id(id);
	}

	return err ? err : ERR_PRINTER_ON_FIRE; // We always save to an unoriginal path
}

Error ImportExporter::export_texture(const String &output_dir, Ref<ImportInfo> &iinfo) {
	String path = iinfo->get_path();
	String source = iinfo->get_source_file();
	bool should_rewrite_metadata = false;
	bool lossy = false;

	// for Godot 2.x resources, we can easily rewrite the metadata to point to a renamed file with a different extension,
	// but this isn't the case for 3.x and greater, so we have to save in the original (lossy) format.
	String source_ext = source.get_extension().to_lower();
	if (source_ext != "png" || iinfo->get_ver_major() == 2) {
		if (iinfo->get_ver_major() > 2) {
			if ((source_ext == "jpg" || source_ext == "jpeg") && opt_export_jpg) {
				lossy = true;
			} else if (source_ext == "webp" && opt_export_webp) {
				// if the engine is <3.4, it can't handle lossless encoded WEBPs
				if (get_ver_major() < 4 && !(get_ver_major() == 3 && get_ver_minor() >= 4)) {
					lossy = true;
				}
			} else {
				iinfo->set_export_dest(iinfo->get_export_dest().get_basename() + ".png");
				// If this is version 3-4, we need to rewrite the import metadata to point to the new resource name
				// disable this for now
				if (false && opt_rewrite_imd_v3 && iinfo->is_import()) {
					should_rewrite_metadata = true;
				} else {
					// save it under .assets, which won't be picked up for import by the godot editor
					if (!iinfo->get_export_dest().replace("res://", "").begins_with(".assets")) {
						String prefix = ".assets";
						if (iinfo->get_export_dest().begins_with("res://")) {
							prefix = "res://.assets";
						}
						iinfo->set_export_dest(prefix.path_join(iinfo->get_export_dest().replace("res://", "")));
					}
				}
			}
		} else { //version 2
			iinfo->set_export_dest(iinfo->get_export_dest().get_basename() + ".png");
			if (opt_rewrite_imd_v2 && iinfo->is_import()) {
				should_rewrite_metadata = true;
			}
		}
	}

	Error err;
	if (iinfo->get_importer() == "bitmap") {
		err = _convert_bitmap(output_dir, path, iinfo->get_export_dest(), lossy);
	} else {
		err = _convert_tex(output_dir, path, iinfo->get_export_dest(), lossy);
	}
	if (err == ERR_UNAVAILABLE) {
		// Already reported in export functions above
		return ERR_UNAVAILABLE;
	}
	ERR_FAIL_COND_V(err, err);
	// If lossy, also convert it as a png
	if (lossy) {
		String dest = iinfo->get_export_dest().get_basename() + ".png";
		if (!dest.replace("res://", "").begins_with(".assets")) {
			String prefix = ".assets";
			if (dest.begins_with("res://")) {
				prefix = "res://.assets";
			}
			dest = prefix.path_join(dest.replace("res://", ""));
		}
		iinfo->set_export_lossless_copy(dest);
		err = _convert_tex(output_dir, path, dest, false);
		if (err == ERR_UNAVAILABLE) {
			return ERR_UNAVAILABLE;
		}
		ERR_FAIL_COND_V(err != OK, err);
	}
	if (should_rewrite_metadata) {
		return ERR_PRINTER_ON_FIRE;
	} else if (iinfo->get_source_file() != iinfo->get_export_dest()) {
		return ERR_DATABASE_CANT_WRITE;
	}

	return err;
}

Error ImportExporter::export_sample(const String &output_dir, Ref<ImportInfo> &iinfo) {
	Error err = convert_sample_to_wav(output_dir, iinfo->get_path(), iinfo->get_export_dest());
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to convert sample " + iinfo->get_path() + " to WAV");
	if (iinfo->get_import_loss_type() != ImportInfo::LOSSLESS) {
		// We convert from ADPCM to 16-bit, so we need to make sure we reimport as lossless to avoid quality loss
		iinfo->set_param("compress/mode", 0);
	}
	return OK;
}

// Godot v3-v4 import data rewriting
// TODO: We have to rewrite the resources to remap to the new destination
// However, we currently only rewrite the import data if the source file was recorded as an absolute file path,
// but is still in the project directory structure, which means no resource rewriting is necessary
Error ImportExporter::rewrite_import_source(const String &rel_dest_path, const String &output_dir, const Ref<ImportInfo> &iinfo) {
	String new_source = rel_dest_path;
	String new_import_file = output_dir.path_join(iinfo->get_import_md_path().replace("res://", ""));
	String abs_file_path = GDRESettings::get_singleton()->globalize_path(new_source, output_dir);
	Array new_dest_files;
	Ref<ImportInfo> new_import = Ref<ImportInfo>(iinfo);
	new_import->set_source_and_md5(new_source, FileAccess::get_md5(abs_file_path));
	return new_import->save_to(new_import_file);
}

Error ImportExporter::ensure_dir(const String &dst_dir) {
	Error err;
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	ERR_FAIL_COND_V(da.is_null(), ERR_FILE_CANT_OPEN);
	err = da->make_dir_recursive(dst_dir);
	return err;
}

Error ImportExporter::convert_res_txt_2_bin(const String &output_dir, const String &p_path, const String &p_dst) {
	ResourceFormatLoaderCompat rlc;
	Error err = rlc.convert_txt_to_bin(p_path, p_dst, output_dir);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to convert " + p_path + " to " + p_dst);
	print_verbose("Converted " + p_path + " to " + p_dst);
	return err;
}

Error ImportExporter::convert_res_bin_2_txt(const String &output_dir, const String &p_path, const String &p_dst) {
	ResourceFormatLoaderCompat rlc;
	Error err = rlc.convert_bin_to_txt(p_path, p_dst, output_dir);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to convert " + p_path + " to " + p_dst);
	print_verbose("Converted " + p_path + " to " + p_dst);
	return err;
}
Error ImportExporter::_convert_bitmap(const String &output_dir, const String &p_path, const String &p_dst, bool lossy = true) {
	String dst_dir = output_dir.path_join(p_dst.get_base_dir().replace("res://", ""));
	String dest_path = output_dir.path_join(p_dst.replace("res://", ""));
	Error err;
	TextureLoaderCompat tl;
	Ref<Image> img = tl.load_image_from_bitmap(p_path, &err);
	// deprecated format
	if (err == ERR_UNAVAILABLE) {
		// TODO: Not reporting here because we can't get the deprecated format type yet,
		// implement functionality to pass it back
		print_line("Did not convert deprecated Bitmap resource " + p_path);
		return err;
	}
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to load bitmap " + p_path);
	err = ensure_dir(dst_dir);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to create dirs for " + dest_path);
	String dest_ext = dest_path.get_extension().to_lower();
	if (dest_ext == "jpg" || dest_ext == "jpeg") {
		err = gdreutil::save_image_as_jpeg(dest_path, img);
	} else if (dest_ext == "webp") {
		err = gdreutil::save_image_as_webp(dest_path, img, lossy);
	} else if (dest_ext == "png") {
		err = img->save_png(dest_path);
	} else {
		ERR_FAIL_V_MSG(ERR_FILE_BAD_PATH, "Invalid file name: " + dest_path);
	}

	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to save image " + dest_path + " from texture " + p_path);

	print_verbose("Converted " + p_path + " to " + p_dst);
	return OK;
}

Error ImportExporter::_convert_tex(const String &output_dir, const String &p_path, const String &p_dst, bool lossy = true) {
	String dst_dir = output_dir.path_join(p_dst.get_base_dir().replace("res://", ""));
	String dest_path = output_dir.path_join(p_dst.replace("res://", ""));
	Error err;
	TextureLoaderCompat tl;

	Ref<Image> img = tl.load_image_from_tex(p_path, &err);
	// deprecated format
	if (err == ERR_UNAVAILABLE) {
		// TODO: Not reporting here because we can't get the deprecated format type yet,
		// implement functionality to pass it back
		print_line("Did not convert deprecated Texture resource " + p_path);
		return err;
	}
	ERR_FAIL_COND_V_MSG(err != OK || img.is_null(), err, "Failed to load texture " + p_path);
	ERR_FAIL_COND_V_MSG(img->is_empty(), ERR_FILE_EOF, "Image data is empty for texture " + p_path + ", not saving");
	err = ensure_dir(dst_dir);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to create dirs for " + dest_path);
	if (img->is_compressed()) {
		int req_w, req_h;
		int w = img->get_width();
		int h = img->get_height();
		String fmt_name = Image::get_format_name(img->get_format());
		Image::get_format_min_pixel_size(img->get_format(), req_w, req_h);
		if (w < req_w || h < req_h) {
			print_line("Image %s (format: %s) is too small to decompress safely (%dx%d), not saving.", p_path.get_file(), fmt_name, img->get_width(), img->get_height());
			return ERR_FILE_CORRUPT;
		} else {
			err = img->decompress();
			if (err == ERR_UNAVAILABLE) {
				WARN_PRINT("Decompression not implemented yet for texture format " + fmt_name);
				report_unsupported_resource("Texture", fmt_name, p_path);
				return err;
			}
			ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to decompress " + fmt_name + " texture " + p_path);
		}
	}
	String dest_ext = dest_path.get_extension().to_lower();
	if (dest_ext == "jpg" || dest_ext == "jpeg") {
		err = gdreutil::save_image_as_jpeg(dest_path, img);
	} else if (dest_ext == "webp") {
		err = gdreutil::save_image_as_webp(dest_path, img, lossy);
	} else if (dest_ext == "png") {
		err = img->save_png(dest_path);
	} else {
		ERR_FAIL_V_MSG(ERR_FILE_BAD_PATH, "Invalid file name: " + dest_path);
	}

	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to save image " + dest_path + " from texture " + p_path);

	print_verbose("Converted " + p_path + " to " + p_dst);
	return OK;
}

Error ImportExporter::convert_tex_to_png(const String &output_dir, const String &p_path, const String &p_dst) {
	return _convert_tex(output_dir, p_path, p_dst);
}

String ImportExporter::_get_path(const String &output_dir, const String &p_path) {
	if (get_settings()->get_project_path() == "" && !get_settings()->is_pack_loaded()) {
		if (p_path.is_absolute_path()) {
			return p_path;
		} else {
			return output_dir.path_join(p_path.replace("res://", ""));
		}
	}
	if (get_settings()->has_res_path(p_path)) {
		return get_settings()->get_res_path(p_path);
	} else if (get_settings()->has_res_path(p_path, output_dir)) {
		return get_settings()->get_res_path(p_path, output_dir);
	} else {
		return output_dir.path_join(p_path.replace("res://", ""));
	}
}

void ImportExporter::report_unsupported_resource(const String &type, const String &format_name, const String &import_path, bool suppress_warn, bool suppress_print) {
	String type_format_str = type + "%" + format_name.to_lower();
	if (report->unsupported_types.find(type_format_str) == -1) {
		WARN_PRINT("Conversion for Resource of type " + type + " and format " + format_name + " not implemented");
		report->unsupported_types.push_back(type_format_str);
	}
	if (!suppress_print)
		print_line("Did not convert " + type + " resource " + import_path);
}

Error ImportExporter::convert_sample_to_wav(const String &output_dir, const String &p_path, const String &p_dst) {
	String src_path = _get_path(output_dir, p_path);
	String dst_path = output_dir.path_join(p_dst.replace("res://", ""));
	Error err;

	Ref<AudioStreamWAV> sample = SampleLoaderCompat::load_wav(src_path, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not load sample file " + p_path);

	err = ensure_dir(dst_path.get_base_dir());
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to create dirs for " + dst_path);

	if (sample->get_format() == AudioStreamWAV::FORMAT_IMA_ADPCM) {
		// convert to 16-bit
		sample = SampleLoaderCompat::convert_adpcm_to_16bit(sample);
	}
	err = sample->save_to_wav(dst_path);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not save " + p_dst);

	print_verbose("Converted " + src_path + " to " + dst_path);
	return OK;
}

Error ImportExporter::convert_oggstr_to_ogg(const String &output_dir, const String &p_path, const String &p_dst) {
	String src_path = _get_path(output_dir, p_path);
	String dst_path = output_dir.path_join(p_dst.replace("res://", ""));
	Error err;
	OggStreamLoaderCompat oslc;
	PackedByteArray data = oslc.get_ogg_stream_data(src_path, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not load oggstr file " + p_path);

	err = ensure_dir(dst_path.get_base_dir());
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to create dirs for " + dst_path);

	Ref<FileAccess> f = FileAccess::open(dst_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not open " + p_dst + " for saving");

	f->store_buffer(data.ptr(), data.size());

	print_verbose("Converted " + src_path + " to " + dst_path);
	return OK;
}

Error ImportExporter::convert_mp3str_to_mp3(const String &output_dir, const String &p_path, const String &p_dst) {
	String src_path = _get_path(output_dir, p_path);
	String dst_path = output_dir.path_join(p_dst.replace("res://", ""));
	Error err;

	Ref<AudioStreamMP3> sample = ResourceLoader::load(src_path, "", ResourceFormatLoader::CACHE_MODE_IGNORE, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not load mp3str file " + p_path);

	err = ensure_dir(dst_path.get_base_dir());
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to create dirs for " + dst_path);

	Ref<FileAccess> f = FileAccess::open(dst_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not open " + p_dst + " for saving");

	PackedByteArray data = sample->get_data();
	f->store_buffer(data.ptr(), data.size());

	print_verbose("Converted " + src_path + " to " + dst_path);
	return OK;
}

void ImportExporter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("decompile_scripts"), &ImportExporter::decompile_scripts);
	ClassDB::bind_method(D_METHOD("export_imports"), &ImportExporter::export_imports, DEFVAL(""), DEFVAL(PackedStringArray()));
	ClassDB::bind_method(D_METHOD("convert_res_txt_2_bin"), &ImportExporter::convert_res_txt_2_bin);
	ClassDB::bind_method(D_METHOD("convert_res_bin_2_txt"), &ImportExporter::convert_res_bin_2_txt);
	ClassDB::bind_method(D_METHOD("convert_tex_to_png"), &ImportExporter::convert_tex_to_png);
	ClassDB::bind_method(D_METHOD("convert_sample_to_wav"), &ImportExporter::convert_sample_to_wav);
	ClassDB::bind_method(D_METHOD("convert_oggstr_to_ogg"), &ImportExporter::convert_oggstr_to_ogg);
	ClassDB::bind_method(D_METHOD("convert_mp3str_to_mp3"), &ImportExporter::convert_mp3str_to_mp3);
	ClassDB::bind_method(D_METHOD("get_report"), &ImportExporter::get_report);

	ClassDB::bind_method(D_METHOD("reset"), &ImportExporter::reset);
}

void ImportExporter::reset_log() {
	report = Ref<ImportExporterReport>(memnew(ImportExporterReport));
}

void ImportExporter::reset() {
	opt_bin2text = true;
	opt_export_textures = true;
	opt_export_samples = true;
	opt_export_ogg = true;
	opt_export_mp3 = true;
	opt_lossy = true;
	opt_export_jpg = true;
	opt_export_webp = true;
	opt_rewrite_imd_v2 = true;
	opt_rewrite_imd_v3 = true;
	opt_decompile = true;
	opt_only_decompile = false;
	reset_log();
}

ImportExporter::ImportExporter() {
	reset_log();
}
ImportExporter::~ImportExporter() {
	reset();
}

void ImportExporterReport::set_ver(String ver) {
	this->ver = GodotVer::parse(ver);
}

String ImportExporterReport::get_ver() {
	return ver->as_text();
}

Dictionary ImportExporterReport::get_totals() {
	Dictionary totals;
	totals["total"] = decompiled_scripts.size() + failed_scripts.size() + lossy_imports.size() + rewrote_metadata.size() + failed_rewrite_md.size() + failed_rewrite_md5.size() + failed.size() + success.size() + not_converted.size() + failed_plugin_cfg_create.size() + failed_gdnative_copy.size() + unsupported_types.size();
	totals["decompiled_scripts"] = decompiled_scripts.size();
	totals["success"] = success.size();
	totals["failed"] = failed.size();
	totals["not_converted"] = not_converted.size();
	totals["failed_scripts"] = failed_scripts.size();
	totals["lossy_imports"] = lossy_imports.size();
	totals["rewrote_metadata"] = rewrote_metadata.size();
	totals["failed_rewrite_md"] = failed_rewrite_md.size();
	totals["failed_rewrite_md5"] = failed_rewrite_md5.size();

	totals["failed_plugin_cfg_create"] = failed_plugin_cfg_create.size();
	totals["failed_gdnative_copy"] = failed_gdnative_copy.size();
	totals["unsupported_types"] = unsupported_types.size();
	return totals;
}

Dictionary ImportExporterReport::get_unsupported_types() {
	Dictionary unsupported;
	for (int i = 0; i < unsupported_types.size(); i++) {
		auto split = unsupported_types[i].split("%");
		unsupported[i] = unsupported_types[i];
	}
	return unsupported;
}

Dictionary ImportExporterReport::get_session_notes() {
	Dictionary notes;
	String unsup = get_detected_unsupported_resource_string();
	if (!unsup.is_empty()) {
		Dictionary unsupported;
		unsupported["title"] = "Unsupported Resources Detected";
		String message = "The following resource types were detected in the project that conversion is not implemented for yet.\n";
		message += "See Export Report to see which resources were not exported.\n";
		message += "You will still be able to edit the project in the editor regardless.";
		unsupported["message"] = message;
		PackedStringArray list;
		for (int i = 0; i < unsupported_types.size(); i++) {
			auto split = unsupported_types[i].split("%");
			list.push_back("Resource Type: " + split[0] + ", Format: " + split[1]);
		}
		unsupported["details"] = list;
		notes["unsupported_types"] = unsupported;
	}
	if (exported_scenes) {
		Dictionary export_scenes_note;
		export_scenes_note["title"] = "Experimental Scene Export";
		export_scenes_note["message"] = "Scene export is EXPERIMENTAL and exported scenes may be inaccurate.\n"
										"Thus, all exported scenes have been saved to the .assets directory, which will not be picked up by the editor for import.\n"
										"Please report any issues you encounter with exported scenes to the Github page Github.";
		notes["export_scenes"] = export_scenes_note;
	}

	if (had_encryption_error) {
		// notes["encryption_error"] = "Failed to decompile encrypted scripts!\nSet the correct key and try again!";
		Dictionary encryption_error;
		encryption_error["title"] = "Encryption Error";
		encryption_error["message"] = "Failed to decompile encrypted scripts!\nSet the correct key and try again!";
		encryption_error["details"] = PackedStringArray();
		notes["encryption_error"] = encryption_error;
	}

	if (!translation_export_message.is_empty()) {
		// notes["translation_export_message"] = translation_export_message;
		Dictionary translation_export;
		translation_export["title"] = "Translation Export Incomplete";
		translation_export["message"] = translation_export_message;
		translation_export["details"] = PackedStringArray();
		notes["translation_export_message"] = translation_export;
	}

	if (!failed_gdnative_copy.is_empty() || !failed_plugin_cfg_create.is_empty()) {
		Dictionary failed_plugins;
		failed_plugins["title"] = "Incomplete Plugin Export";
		String message = "The following addons failed to have their libraries copied or plugin.cfg regenerated\n";
		message += "You may encounter editor errors due to this.\n";
		message += "Tip: Try finding the plugin in the Godot Asset Library or Github.\n";
		failed_plugins["message"] = message;
		PackedStringArray list;
		for (int i = 0; i < failed_gdnative_copy.size(); i++) {
			list.push_back(failed_gdnative_copy[i]);
		}
		for (int i = 0; i < failed_plugin_cfg_create.size(); i++) {
			list.push_back(failed_plugin_cfg_create[i]);
		}
		failed_plugins["details"] = list;
		notes["failed_plugins"] = failed_plugins;
	}
	if (ver->get_major() == 2) {
		// Godot 2.x's assets are all exported to .assets
		Dictionary godot_2_assets;
		godot_2_assets["title"] = "Godot 2.x Assets";
		godot_2_assets["message"] = "All exported assets can be found in the '.assets' directory in the project folder.";
		godot_2_assets["details"] = PackedStringArray();
		notes["godot_2_assets"] = godot_2_assets;
	}
	return notes;
}

String ImportExporterReport::get_totals_string() {
	String report = "";
	report += vformat("%-40s", "Totals: ") + String("\n");
	report += vformat("%-40s", "Decompiled scripts: ") + itos(decompiled_scripts.size()) + String("\n");
	report += vformat("%-40s", "Failed scripts: ") + itos(failed_scripts.size()) + String("\n");
	report += vformat("%-40s", "Imported resources for export session: ") + itos(session_files_total) + String("\n");
	report += vformat("%-40s", "Successfully converted: ") + itos(success.size()) + String("\n");
	if (opt_lossy) {
		report += vformat("%-40s", "Lossy: ") + itos(lossy_imports.size()) + String("\n");
	} else {
		report += vformat("%-40s", "Lossy not converted: ") + itos(lossy_imports.size()) + String("\n");
	}
	report += vformat("%-40s", "Rewrote metadata: ") + itos(rewrote_metadata.size()) + String("\n");
	report += vformat("%-40s", "Non-importable conversions: ") + itos(failed_rewrite_md.size()) + String("\n");
	report += vformat("%-40s", "Not converted: ") + itos(not_converted.size()) + String("\n");
	report += vformat("%-40s", "Failed conversions: ") + itos(failed.size()) + String("\n");
	return report;
}

Dictionary ImportExporterReport::get_report_sections() {
	Dictionary sections;
	// sections["totals"] = get_totals();
	// sections["unsupported_types"] = get_unsupported_types();
	// sections["session_notes"] = get_session_notes();
	sections["success"] = Dictionary();
	Dictionary success_dict = sections["success"];
	for (int i = 0; i < success.size(); i++) {
		success_dict[success[i]->get_path()] = success[i]->get_export_dest();
	}
	sections["decompiled_scripts"] = Dictionary();
	Dictionary decompiled_scripts_dict = sections["decompiled_scripts"];
	for (int i = 0; i < decompiled_scripts.size(); i++) {
		decompiled_scripts_dict[decompiled_scripts[i]] = decompiled_scripts[i];
	}

	if (!not_converted.is_empty()) {
		sections["not_converted"] = Dictionary();
		Dictionary not_converted_dict = sections["not_converted"];
		for (int i = 0; i < not_converted.size(); i++) {
			not_converted_dict[not_converted[i]->get_path()] = not_converted[i]->get_export_dest();
		}
	}
	if (!failed_scripts.is_empty()) {
		sections["failed_scripts"] = Dictionary();
		Dictionary failed_scripts_dict = sections["failed_scripts"];
		for (int i = 0; i < failed_scripts.size(); i++) {
			failed_scripts_dict[failed_scripts[i]] = failed_scripts[i];
		}
	}
	if (!failed.is_empty()) {
		sections["failed"] = Dictionary();
		Dictionary failed_dict = sections["failed"];
		for (int i = 0; i < failed.size(); i++) {
			failed_dict[failed[i]->get_path()] = failed[i]->get_export_dest();
		}
	}
	if (!lossy_imports.is_empty()) {
		sections["lossy_imports"] = Dictionary();
		Dictionary lossy_dict = sections["lossy_imports"];
		for (int i = 0; i < lossy_imports.size(); i++) {
			lossy_dict[lossy_imports[i]->get_path()] = lossy_imports[i]->get_export_dest();
		}
	}
	if (!rewrote_metadata.is_empty()) {
		sections["rewrote_metadata"] = Dictionary();
		Dictionary rewrote_metadata_dict = sections["rewrote_metadata"];
		for (int i = 0; i < rewrote_metadata.size(); i++) {
			rewrote_metadata_dict[rewrote_metadata[i]->get_path()] = rewrote_metadata[i]->get_export_dest();
		}
	}
	if (!failed_rewrite_md.is_empty()) {
		sections["failed_rewrite_md"] = Dictionary();
		Dictionary failed_rewrite_md_dict = sections["failed_rewrite_md"];
		for (int i = 0; i < failed_rewrite_md.size(); i++) {
			failed_rewrite_md_dict[failed_rewrite_md[i]->get_path()] = failed_rewrite_md[i]->get_export_dest();
		}
	}
	if (!failed_rewrite_md5.is_empty()) {
		sections["failed_rewrite_md5"] = Dictionary();
		Dictionary failed_rewrite_md5_dict = sections["failed_rewrite_md5"];
		for (int i = 0; i < failed_rewrite_md5.size(); i++) {
			failed_rewrite_md5_dict[failed_rewrite_md5[i]->get_path()] = failed_rewrite_md5[i]->get_export_dest();
		}
	}
	// plugins
	if (!failed_plugin_cfg_create.is_empty()) {
		sections["failed_plugin_cfg_create"] = Dictionary();
		Dictionary failed_plugin_cfg_create_dict = sections["failed_plugin_cfg_create"];
		for (int i = 0; i < failed_plugin_cfg_create.size(); i++) {
			failed_plugin_cfg_create_dict[failed_plugin_cfg_create[i]] = failed_plugin_cfg_create[i];
		}
	}
	if (!failed_gdnative_copy.is_empty()) {
		sections["failed_gdnative_copy"] = Dictionary();
		Dictionary failed_gdnative_copy_dict = sections["failed_gdnative_copy"];
		for (int i = 0; i < failed_gdnative_copy.size(); i++) {
			failed_gdnative_copy_dict[failed_gdnative_copy[i]] = failed_gdnative_copy[i];
		}
	}
	return sections;
}

String ImportExporterReport::get_report_string() {
	String report;
	report += get_totals_string();
	report += "-------------\n" + String("\n");
	if (lossy_imports.size() > 0) {
		if (opt_lossy) {
			report += "\nThe following files were converted from an import that was stored lossy." + String("\n");
			report += "You may lose fidelity when re-importing these files upon loading the project." + String("\n");
			for (int i = 0; i < lossy_imports.size(); i++) {
				report += lossy_imports[i]->get_path() + " to " + lossy_imports[i]->get_export_dest() + String("\n");
			}
		} else {
			report += "\nThe following files were not converted from a lossy import." + String("\n");
			for (int i = 0; i < lossy_imports.size(); i++) {
				report += lossy_imports[i]->get_path() + String("\n");
			}
		}
	}
	if (failed_plugin_cfg_create.size() > 0) {
		report += "------\n";
		report += "\nThe following plugins failed to have their plugin.cfg regenerated:" + String("\n");
		for (int i = 0; i < failed_plugin_cfg_create.size(); i++) {
			report += failed_plugin_cfg_create[i] + String("\n");
		}
	}

	if (failed_gdnative_copy.size() > 0) {
		report += "------\n";
		report += "\nThe following native plugins failed to have their libraries copied:" + String("\n");
		for (int i = 0; i < failed_gdnative_copy.size(); i++) {
			report += failed_gdnative_copy[i] + String("\n");
		}
	}
	// we skip this for version 2 because we have to rewrite the metadata for nearly all the converted resources
	if (rewrote_metadata.size() > 0 && ver->get_major() != 2) {
		report += "------\n";
		report += "\nThe following files had their import data rewritten:" + String("\n");
		for (int i = 0; i < rewrote_metadata.size(); i++) {
			report += rewrote_metadata[i]->get_path() + " to " + rewrote_metadata[i]->get_export_dest() + String("\n");
		}
	}
	if (failed_rewrite_md.size() > 0) {
		report += "------\n";
		report += "\nThe following files were converted and saved to a non-original path, but did not have their import data rewritten." + String("\n");
		report += "These files will not be re-imported when loading the project." + String("\n");
		for (int i = 0; i < failed_rewrite_md.size(); i++) {
			report += failed_rewrite_md[i]->get_path() + " to " + failed_rewrite_md[i]->get_export_dest() + String("\n");
		}
	}
	if (not_converted.size() > 0) {
		report += "------\n";
		report += "\nThe following files were not converted because support has not been implemented yet:" + String("\n");
		for (int i = 0; i < not_converted.size(); i++) {
			report += not_converted[i]->get_path() + String("\n");
		}
	}
	if (failed.size() > 0) {
		report += "------\n";
		report += "\nFailed conversions:" + String("\n");
		for (int i = 0; i < failed.size(); i++) {
			report += failed[i]->get_path() + String("\n");
		}
	}
	return report;
}
String ImportExporterReport::get_editor_message_string() {
	String report = "";
	report += "Use Godot editor version " + ver->as_text() + String(godotsteam_detected ? " (steam version)" : "") + " to edit the project." + String("\n");
	if (godotsteam_detected) {
		report += "GodotSteam can be found here: https://github.com/CoaguCo-Industries/GodotSteam/releases \n";
	}
	report += "Note: the project may be using a custom version of Godot. Detection for this has not been implemented yet." + String("\n");
	report += "If you find that you have many non-import errors upon opening the project " + String("\n");
	report += "(i.e. scripts or shaders have many errors), use the original game's binary as the export template." + String("\n");

	return report;
}
String ImportExporterReport::get_detected_unsupported_resource_string() {
	String str = "";
	for (auto type : unsupported_types) {
		Vector<String> spl = type.split("%");
		str += vformat("Type: %-20s", spl[0]) + "\tFormat: " + spl[1] + "\n";
	}
	return str;
}

String ImportExporterReport::get_session_notes_string() {
	String report = "";
	Dictionary notes = get_session_notes();
	auto keys = notes.keys();
	if (keys.size() == 0) {
		return report;
	}
	report += String("\n");
	for (int i = 0; i < keys.size(); i++) {
		Dictionary note = notes[keys[i]];
		if (i > 0) {
			report += String("------\n");
		}
		String title = note["title"];
		String message = note["message"];
		report += title + ":" + String("\n");
		report += message + String("\n");
		PackedStringArray details = note["details"];
		for (int j = 0; j < details.size(); j++) {
			report += " - " + details[j] + String("\n");
		}
		report += String("\n");
	}
	return report;
}
String ImportExporterReport::get_log_file_location() {
	return log_file_location;
}

Vector<String> ImportExporterReport::get_decompiled_scripts() {
	return decompiled_scripts;
}
Vector<String> ImportExporterReport::get_failed_scripts() {
	return failed_scripts;
}
TypedArray<ImportInfo> iinfo_vector_to_typedarray(const Vector<Ref<ImportInfo>> &vec) {
	TypedArray<ImportInfo> arr;
	arr.resize(vec.size());
	for (int i = 0; i < vec.size(); i++) {
		arr.set(i, vec[i]);
	}
	return arr;
}

TypedArray<ImportInfo> ImportExporterReport::get_successes() {
	return iinfo_vector_to_typedarray(success);
}
TypedArray<ImportInfo> ImportExporterReport::get_failed() {
	return iinfo_vector_to_typedarray(failed);
}
TypedArray<ImportInfo> ImportExporterReport::get_not_converted() {
	return iinfo_vector_to_typedarray(not_converted);
}
TypedArray<ImportInfo> ImportExporterReport::get_lossy_imports() {
	return iinfo_vector_to_typedarray(lossy_imports);
}
TypedArray<ImportInfo> ImportExporterReport::get_rewrote_metadata() {
	return iinfo_vector_to_typedarray(rewrote_metadata);
}
TypedArray<ImportInfo> ImportExporterReport::get_failed_rewrite_md() {
	return iinfo_vector_to_typedarray(failed_rewrite_md);
}
TypedArray<ImportInfo> ImportExporterReport::get_failed_rewrite_md5() {
	return iinfo_vector_to_typedarray(failed_rewrite_md5);
}
Vector<String> ImportExporterReport::get_failed_plugin_cfg_create() {
	return failed_plugin_cfg_create;
}
Vector<String> ImportExporterReport::get_failed_gdnative_copy() {
	return failed_gdnative_copy;
}

void ImportExporterReport::print_report() {
	print_line("\n\n********************************EXPORT REPORT********************************" + String("\n"));
	print_line(get_report_string());
	String notes = get_session_notes_string();
	if (!notes.is_empty()) {
		print_line("\n\n---------------------------------IMPORTANT NOTES----------------------------------" + String("\n"));
		print_line(notes);
	}
	print_line("\n------------------------------------------------------------------------------------" + String("\n"));
	print_line(get_editor_message_string());
	print_line("*******************************************************************************\n");
}

void ImportExporterReport::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_totals"), &ImportExporterReport::get_totals);
	ClassDB::bind_method(D_METHOD("get_unsupported_types"), &ImportExporterReport::get_unsupported_types);
	ClassDB::bind_method(D_METHOD("get_session_notes"), &ImportExporterReport::get_session_notes);
	ClassDB::bind_method(D_METHOD("get_totals_string"), &ImportExporterReport::get_totals_string);
	ClassDB::bind_method(D_METHOD("get_report_string"), &ImportExporterReport::get_report_string);
	ClassDB::bind_method(D_METHOD("get_detected_unsupported_resource_string"), &ImportExporterReport::get_detected_unsupported_resource_string);
	ClassDB::bind_method(D_METHOD("get_session_notes_string"), &ImportExporterReport::get_session_notes_string);
	ClassDB::bind_method(D_METHOD("get_editor_message_string"), &ImportExporterReport::get_editor_message_string);
	ClassDB::bind_method(D_METHOD("get_log_file_location"), &ImportExporterReport::get_log_file_location);
	ClassDB::bind_method(D_METHOD("get_decompiled_scripts"), &ImportExporterReport::get_decompiled_scripts);
	ClassDB::bind_method(D_METHOD("get_failed_scripts"), &ImportExporterReport::get_failed_scripts);
	ClassDB::bind_method(D_METHOD("get_successes"), &ImportExporterReport::get_successes);
	ClassDB::bind_method(D_METHOD("get_failed"), &ImportExporterReport::get_failed);
	ClassDB::bind_method(D_METHOD("get_not_converted"), &ImportExporterReport::get_not_converted);
	ClassDB::bind_method(D_METHOD("get_lossy_imports"), &ImportExporterReport::get_lossy_imports);
	ClassDB::bind_method(D_METHOD("get_rewrote_metadata"), &ImportExporterReport::get_rewrote_metadata);
	ClassDB::bind_method(D_METHOD("get_failed_rewrite_md"), &ImportExporterReport::get_failed_rewrite_md);
	ClassDB::bind_method(D_METHOD("get_failed_rewrite_md5"), &ImportExporterReport::get_failed_rewrite_md5);
	ClassDB::bind_method(D_METHOD("get_failed_plugin_cfg_create"), &ImportExporterReport::get_failed_plugin_cfg_create);
	ClassDB::bind_method(D_METHOD("get_failed_gdnative_copy"), &ImportExporterReport::get_failed_gdnative_copy);
	ClassDB::bind_method(D_METHOD("get_report_sections"), &ImportExporterReport::get_report_sections);
	ClassDB::bind_method(D_METHOD("print_report"), &ImportExporterReport::print_report);
	ClassDB::bind_method(D_METHOD("set_ver"), &ImportExporterReport::set_ver);
	ClassDB::bind_method(D_METHOD("get_ver"), &ImportExporterReport::get_ver);
}
