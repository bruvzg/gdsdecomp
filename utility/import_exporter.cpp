
#include "import_exporter.h"
#include "bytecode/bytecode_versions.h"
#include "compat/oggstr_loader_compat.h"
#include "compat/resource_loader_compat.h"
#include "compat/texture_loader_compat.h"
#include "gdre_settings.h"
#include "pcfg_loader.h"
#include "util_functions.h"

#include "core/crypto/crypto_core.h"
#include "core/io/config_file.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/os/os.h"
#include "core/variant/variant_parser.h"
#include "core/version_generated.gen.h"
#include "modules/minimp3/audio_stream_mp3.h"
#include "modules/regex/regex.h"
#include "modules/vorbis/audio_stream_ogg_vorbis.h"
#include "scene/resources/audio_stream_wav.h"
#include "thirdparty/minimp3/minimp3_ex.h"

GDRESettings *get_settings() {
	return GDRESettings::get_singleton();
}

Array ImportExporter::get_import_files() {
	return files;
}
Vector<String> ImportExporter::get_v2_wildcards() {
	Vector<String> wildcards;
	// We look for file names with ".converted." in the name
	// Like "filename.tscn.converted.scn"
	// These are resources converted to binary format upon project export by the editor
	wildcards.push_back("*.converted.*");
	// The rest of these are imported resources
	wildcards.push_back("*.tex");
	wildcards.push_back("*.fnt");
	wildcards.push_back("*.msh");
	wildcards.push_back("*.scn");
	wildcards.push_back("*.res");
	wildcards.push_back("*.smp");
	wildcards.push_back("*.xl");
	wildcards.push_back("*.cbm");
	wildcards.push_back("*.pbm");
	wildcards.push_back("*.gdc");

	return wildcards;
}

int get_ver_major() {
	return get_settings()->get_ver_major();
}
int get_ver_minor() {
	return get_settings()->get_ver_minor();
}
Error ImportExporter::load_import_files() {
	Vector<String> file_names;
	reset();
	ERR_FAIL_COND_V_MSG(!get_settings()->is_pack_loaded(), ERR_DOES_NOT_EXIST, "pack/dir not loaded!");

	String bin_proj_file;
	bool ver_major_not_input = false;

	if (get_ver_major() <= 2) {
		if (get_settings()->is_pack_loaded()) {
			file_names = get_settings()->get_file_list(get_v2_wildcards());
		}
	} else {
		Vector<String> wildcards;
		wildcards.push_back("*.import");
		wildcards.push_back("*.gdc");
		wildcards.push_back("*.gde");
		file_names = get_settings()->get_file_list(wildcards);
	}

	for (int i = 0; i < file_names.size(); i++) {
		if (file_names[i].get_extension() == "gdc" || file_names[i].get_extension() == "gde") {
			code_files.push_back(file_names[i]);
		} else if (load_import_file(file_names[i]) != OK) {
			WARN_PRINT("Can't load import file: " + file_names[i]);
		}
	}
	return OK;
}

Error ImportExporter::load_import_file(const String &p_path) {
	Ref<ImportInfo> i_info;
	i_info.instantiate();
	i_info->load_from_file(p_path, get_ver_major(), get_ver_minor());
	files.push_back(i_info);
	return OK;
}

// export all the imported resources
Error ImportExporter::export_imports(const String &p_out_dir) {
	String t;
	return _export_imports(p_out_dir, Vector<String>(), nullptr, t);
}

Error ImportExporter::_export_imports(const String &p_out_dir, const Vector<String> &files_to_export, EditorProgressGDDC *pr, String &error_string) {
	reset_log();
	ERR_FAIL_COND_V_MSG(!get_settings()->is_pack_loaded(), ERR_DOES_NOT_EXIST, "pack/dir not loaded!");
	uint64_t last_progress_upd = OS::get_singleton()->get_ticks_usec();
	String output_dir = !p_out_dir.is_empty() ? p_out_dir : get_settings()->get_project_path();
	Error err = OK;
	if (opt_lossy) {
		WARN_PRINT_ONCE("Converting lossy imports, you may lose fidelity for indicated assets when re-importing upon loading the project");
	}
	recreate_plugin_configs(output_dir);
	Ref<DirAccess> dir = DirAccess::open(output_dir);

	if (opt_decompile) {
		decompile_scripts(output_dir);
	}
	for (int i = 0; i < files.size(); i++) {
		Ref<ImportInfo> iinfo = files[i];
		String path = iinfo->get_path();
		String source = iinfo->get_source_file();
		String type = iinfo->get_type();
		String importer = iinfo->importer;
		auto loss_type = iinfo->get_import_loss_type();
		if (files_to_export.size()) {
			auto dest_files = iinfo->get_dest_files();
			bool has_path = false;
			for (auto dest : dest_files) {
				if (files_to_export.has(path)) {
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
				lossy_imports.push_back(iinfo);
				print_line("Not converting lossy import " + path);
				continue;
			}
		}
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
					print_line("Did not convert " + type + " resource " + path);
					not_converted.push_back(iinfo);
					break;
				default:
					WARN_PRINT_ONCE("Conversion for " + type + " not yet implemented");
					print_line("Did not convert " + type + " resource " + path);
					not_converted.push_back(iinfo);
					continue;
			}
		} else if (opt_export_samples && (importer == "sample" || importer == "wav")) {
			if (iinfo->get_import_loss_type() == ImportInfo::LOSSLESS) {
				err = export_sample(output_dir, iinfo);
			} else {
				// Godot doesn't support saving ADPCM samples as waves, nor converting them to PCM16
				WARN_PRINT_ONCE("Conversion for samples stored in IMA ADPCM format not yet implemented");
				print_line("Did not convert Sample " + path);
				not_converted.push_back(iinfo);
				continue;
			}
		} else if (opt_export_ogg && importer == "ogg_vorbis") {
			iinfo->preferred_dest = iinfo->source_file;
			err = convert_oggstr_to_ogg(output_dir, iinfo->import_path, iinfo->source_file);
		} else if (opt_export_mp3 && importer == "mp3") {
			iinfo->preferred_dest = iinfo->source_file;
			err = convert_mp3str_to_mp3(output_dir, iinfo->import_path, iinfo->source_file);
		} else if ((opt_bin2text && iinfo->import_path.get_extension() == "res") ||
				(importer == "scene" && (iinfo->source_file.get_extension() == "tscn" || iinfo->source_file.get_extension() == "escn"))) {
			iinfo->preferred_dest = iinfo->source_file;
			err = convert_res_bin_2_txt(output_dir, iinfo->import_path, iinfo->source_file);
			if (get_ver_major() == 2 && !err && iinfo->is_auto_converted()) {
				dir->remove(iinfo->import_path.replace("res://", ""));
			}
		} else {
			WARN_PRINT_ONCE("Conversion for Resource of type " + type + "and format " + iinfo->source_file.get_extension() + " not implemented");
			print_line("Did not convert " + type + " resource " + path);
			not_converted.push_back(iinfo);
			continue;
		}
		// we had to rewrite the import metadata
		if (err == ERR_PRINTER_ON_FIRE) {
			rewrote_metadata.push_back(iinfo);
			success.push_back(iinfo);
			// necessary to rewrite import metadata but failed
		} else if (err == ERR_DATABASE_CANT_WRITE) {
			success.push_back(iinfo);
			failed_rewrite_md.push_back(iinfo);
		} else if (err == ERR_UNAVAILABLE) {
			not_converted.push_back(iinfo);
			print_line("Did not convert " + type + " resource " + path);
		} else if (err != OK) {
			failed.push_back(iinfo);
			print_line("Failed to convert " + type + " resource " + path);
		} else {
			if (loss_type != ImportInfo::LOSSLESS) {
				lossy_imports.push_back(iinfo);
			}
			success.push_back(iinfo);
		}
		// remove v2 remaps
		if (get_ver_major() == 2 && get_settings()->has_any_remaps()) {
			if (get_settings()->has_remap(iinfo->source_file, iinfo->import_path)) {
				get_settings()->remove_remap(iinfo->source_file, iinfo->import_path);
			}
		}
	}

	if (get_settings()->save_project_config(output_dir) != OK) {
		print_line("ERROR: Failed to save project config!");
	} else {
		print_line("Saved " + String(get_ver_major() > 2 ? "project.godot" : "engine.cfg") + " to " + output_dir);
	}
	print_report();
	return OK;
}

Error ImportExporter::decompile_scripts(const String &p_out_dir) {
	GDScriptDecomp *decomp;
	// we have to remove remaps if they exist
	bool has_remaps = get_settings()->has_any_remaps();
	// TODO: instead of doing this, run the detect bytecode script
	switch (get_ver_major()) {
		case 1:
			switch (get_ver_minor()) {
				case 0:
					decomp = memnew(GDScriptDecomp_e82dc40);
					break;
				case 1:
					decomp = memnew(GDScriptDecomp_e82dc40);
					break;
			}
			break;
		case 2:
			switch (get_ver_minor()) {
				case 0:
					decomp = memnew(GDScriptDecomp_23441ec);
					break;
				case 1:
					decomp = memnew(GDScriptDecomp_ed80f45);
					break;
			}
			break;
		case 3:
			switch (get_ver_minor()) {
				case 0:
					decomp = memnew(GDScriptDecomp_054a2ac);
					break;
				case 1:
					decomp = memnew(GDScriptDecomp_514a3fb);
					break;
				case 2:
					decomp = memnew(GDScriptDecomp_5565f55);
					break;
				case 3:
				case 4:
				case 5:
					decomp = memnew(GDScriptDecomp_5565f55);
					break;
			}
			break;
		case 4:
			decomp = memnew(GDScriptDecomp_5565f55);
			break;
		default:
			ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED, "Unknown version, failed to decompile");
	}
	print_line("Script version " + itos(get_ver_major()) + "." + itos(get_ver_minor()) + ".x detected");
	Error err;

	for (String f : code_files) {
		String dest_file = f.replace(".gdc", ".gd").replace(".gde", ".gd");
		Ref<DirAccess> da = DirAccess::open(p_out_dir);
		print_line("decompiling " + f);
		bool encrypted = false;
		if (f.get_extension() == "gde") {
			encrypted = true;
			err = decomp->decompile_byte_code_encrypted(f, get_settings()->get_encryption_key(), get_ver_major() == 3);
		} else {
			err = decomp->decompile_byte_code(f);
		}
		if (err) {
			memdelete(decomp);
			failed_scripts.push_back(f);
			// TODO: make it not fail hard on the first script that fails to decompile
			if (encrypted) {
				had_encryption_error = true;
				ERR_FAIL_V_MSG(err, "error decompiling encrypted script " + f);
			} else {
				ERR_FAIL_V_MSG(err, "error decompiling " + f);
			}
		} else {
			String text = decomp->get_script_text();
			String out_path = p_out_dir.path_join(dest_file.replace("res://", ""));
			Ref<FileAccess> fa = FileAccess::open(out_path, FileAccess::WRITE);
			if (fa.is_null()) {
				failed_scripts.push_back(f);
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
			print_line("successfully decompiled " + f);
			decompiled_scripts.push_back(f);
		}
	}
	memdelete(decomp);
	// save changed config file
	err = get_settings()->save_project_config(p_out_dir);
	if (err) {
		WARN_PRINT("Failed to save changed project config!");
	}
	return OK;
}

Error ImportExporter::recreate_plugin_config(const String &output_dir, const String &plugin_dir) {
	Error err;
	Vector<String> wildcards;
	wildcards.push_back("*.gd");
	String abs_plugin_path = output_dir.path_join("addons").path_join(plugin_dir);
	auto gd_scripts = gdreutil::get_recursive_dir_list(abs_plugin_path, wildcards, false);
	String main_script;
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
	return OK;
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
		err = recreate_plugin_config(output_dir, dirs[i]);
		if (err) {
			WARN_PRINT("Failed to recreate plugin.cfg for " + dirs[i]);
		}
	}
	return OK;
}

Error ImportExporter::export_texture(const String &output_dir, Ref<ImportInfo> &iinfo) {
	String path = iinfo->get_path();
	String source = iinfo->get_source_file();
	String dest = source;
	bool rewrite_metadata = false;
	bool lossy = false;

	// Rewrite the metadata for v2
	// This is essentially mandatory for v2 resources because they can be imported outside the
	// project directory tree and the import metadata often points to locations that don't exist.
	if (iinfo->ver_major == 2 && opt_rewrite_imd_v2 && iinfo->is_import()) {
		rewrite_metadata = true;
	}

	// for Godot 2.x resources, we can easily rewrite the metadata to point to a renamed file with a different extension,
	// but this isn't the case for 3.x and greater, so we have to save in the original (lossy) format.
	String source_ext = source.get_extension().to_lower();
	if (source_ext != "png") {
		if (iinfo->ver_major > 2) {
			if ((source_ext == "jpg" || source_ext == "jpeg") && opt_export_jpg) {
				lossy = true;
			} else if (source_ext == "webp" && opt_export_webp) {
				// if the engine is <3.4, it can't handle lossless encoded WEBPs
				if (get_ver_major() < 4 && !(get_ver_major() == 3 && get_ver_minor() >= 4)) {
					lossy = true;
				}
			} else {
				dest = source.get_basename() + ".png";
				// If this is version 3-4, we need to rewrite the import metadata to point to the new resource name
				if (opt_rewrite_imd_v3 && iinfo->is_import()) {
					rewrite_metadata = true;
				} else {
					// save it under .assets, which won't be picked up for import by the godot editor
					if (!dest.replace("res://", "").begins_with(".assets")) {
						String prefix = ".assets";
						if (dest.begins_with("res://")) {
							prefix = "res://.assets";
						}
						dest = prefix.path_join(dest.replace("res://", ""));
					}
				}
			}
		} else { //version 2
			dest = source.get_basename() + ".png";
		}
	}

	iinfo->preferred_dest = dest;
	String r_name;
	Error err;
	err = _convert_tex(output_dir, path, dest, &r_name, lossy);
	if (err == ERR_UNAVAILABLE) {
		return ERR_UNAVAILABLE;
	}
	ERR_FAIL_COND_V(err, err);
	// If lossy, also convert it as a png
	if (lossy) {
		dest = source.get_basename() + ".png";
		if (!dest.replace("res://", "").begins_with(".assets")) {
			String prefix = ".assets";
			if (dest.begins_with("res://")) {
				prefix = "res://.assets";
			}
			dest = prefix.path_join(dest.replace("res://", ""));
		}
		err = _convert_tex(output_dir, path, dest, &r_name, false);
		if (err == ERR_UNAVAILABLE) {
			return ERR_UNAVAILABLE;
		}
		ERR_FAIL_COND_V(err != OK, err);
	}
	if (iinfo->ver_major == 2 && iinfo->is_import()) {
		if (rewrite_metadata) {
			err = rewrite_v2_import_metadata(path, iinfo->preferred_dest, r_name, output_dir);
			ERR_FAIL_COND_V_MSG(err != OK, ERR_DATABASE_CANT_WRITE, "Failed to rewrite import metadata for " + iinfo->source_file);
			return ERR_PRINTER_ON_FIRE;
		}
		return ERR_DATABASE_CANT_WRITE;
	} else if (iinfo->ver_major >= 3 && iinfo->is_import()) {
		if (rewrite_metadata) {
			err = ERR_DATABASE_CANT_WRITE;
			ERR_FAIL_COND_V_MSG(err != OK, ERR_DATABASE_CANT_WRITE, "Failed to remap resource " + iinfo->source_file);
			return ERR_PRINTER_ON_FIRE;
			// If we saved the file to something other than png
		} else if (iinfo->source_file != iinfo->preferred_dest) {
			return ERR_DATABASE_CANT_WRITE;
		}
	}
	return err;
}

Error ImportExporter::export_sample(const String &output_dir, Ref<ImportInfo> &iinfo) {
	String path = iinfo->get_path();
	String source = iinfo->get_source_file();
	String dest = source;

	iinfo->preferred_dest = dest;
	if (iinfo->ver_major == 2) {
		WARN_PRINT_ONCE("Godot 2.x sample to wav conversion not yet implemented");
		print_line("Not converting sample " + path);
		return ERR_UNAVAILABLE;
	}
	convert_sample_to_wav(output_dir, path, dest);
	return OK;
}

Ref<ImportInfo> ImportExporter::get_import_info(const String &p_path) {
	Ref<ImportInfo> iinfo;
	for (int i = 0; i < files.size(); i++) {
		iinfo = files[i];
		if (iinfo->get_path() == p_path) {
			return iinfo;
		}
	}
	// not found
	return Ref<ImportInfo>();
}

// Makes a copy of the import metadata and changes the source to the new path
Ref<ResourceImportMetadatav2> ImportExporter::change_v2import_data(const String &p_path,
		const String &rel_dest_path,
		const String &p_res_name,
		const String &output_dir,
		const bool change_extension) {
	Ref<ResourceImportMetadatav2> imd;
	auto iinfo = get_import_info(p_path);
	if (iinfo.is_null()) {
		return imd;
	}
	// copy
	imd = Ref<ResourceImportMetadatav2>(iinfo->v2metadata);
	if (imd->get_source_count() > 1) {
		WARN_PRINT("multiple import sources detected!?");
	}
	int i;
	for (i = 0; i < imd->get_source_count(); i++) {
		// case insensitive windows paths...
		String src_file_name = imd->get_source_path(i).get_file().to_lower();
		String res_name = p_res_name.to_lower();
		if (change_extension) {
			src_file_name = src_file_name.get_basename();
			res_name = res_name.get_basename();
		}
		if (src_file_name == res_name) {
			String md5 = imd->get_source_md5(i);
			String dst_path = rel_dest_path;
			if (output_dir != "") {
				dst_path = output_dir.path_join(rel_dest_path.replace("res://", ""));
			}
			String new_hash = FileAccess::get_md5(dst_path);
			if (new_hash == "") {
				WARN_PRINT("Can't open exported file to calculate hash");
			} else {
				md5 = new_hash;
			}
			imd->remove_source(i);
			imd->add_source(rel_dest_path, md5);
			break;
		}
	}
	if (i == imd->get_source_count()) {
		WARN_PRINT("Can't find resource name!");
	}
	return imd;
}

Error ImportExporter::rewrite_v2_import_metadata(const String &p_path, const String &p_dst, const String &p_res_name, const String &output_dir) {
	Error err;
	String orig_file = output_dir.path_join(p_path.replace("res://", ""));
	auto imd = change_v2import_data(p_path, p_dst, p_res_name, output_dir, false);
	ERR_FAIL_COND_V_MSG(imd.is_null(), ERR_FILE_NOT_FOUND, "Failed to get metadata for " + p_path);

	ResourceFormatLoaderCompat rlc;
	err = rlc.rewrite_v2_import_metadata(p_path, orig_file + ".tmp", imd);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to rewrite metadata for " + orig_file);

	Ref<DirAccess> dr = DirAccess::open(orig_file.get_base_dir(), &err);
	ERR_FAIL_COND_V_MSG(dr.is_null(), err, "Failed to rename file " + orig_file + ".tmp");

	// this may fail, we don't care
	dr->remove(orig_file);
	err = dr->rename(orig_file + ".tmp", orig_file);
	ERR_FAIL_COND_V_MSG(dr.is_null(), err, "Failed to rename file " + orig_file + ".tmp");

	print_line("Rewrote import metadata for " + p_path);
	return OK;
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
	print_line("Converted " + p_path + " to " + p_dst);
	return err;
}

Error ImportExporter::convert_res_bin_2_txt(const String &output_dir, const String &p_path, const String &p_dst) {
	ResourceFormatLoaderCompat rlc;
	Error err = rlc.convert_bin_to_txt(p_path, p_dst, output_dir);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to convert " + p_path + " to " + p_dst);
	print_line("Converted " + p_path + " to " + p_dst);
	return err;
}

Error ImportExporter::_convert_tex(const String &output_dir, const String &p_path, const String &p_dst, String *r_name, bool lossy = true) {
	String dst_dir = output_dir.path_join(p_dst.get_base_dir().replace("res://", ""));
	String dest_path = output_dir.path_join(p_dst.replace("res://", ""));
	Error err;
	TextureLoaderCompat tl;

	Ref<Image> img = tl.load_image_from_tex(p_path, &err);
	// deprecated format
	if (err == ERR_UNAVAILABLE) {
		return ERR_UNAVAILABLE;
	}
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to load texture " + p_path);
	if (r_name) {
		*r_name = String(img->get_name());
	}
	err = ensure_dir(dst_dir);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to create dirs for " + dest_path);
	if (img->is_compressed()) {
		err = img->decompress();
		String fmt_name = Image::get_format_name(img->get_format());
		ERR_FAIL_COND_V_MSG(err == ERR_UNAVAILABLE, err, "Decompression not implemented yet for texture format " + fmt_name);
		ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to decompress " + fmt_name + " texture " + p_path);
	}
	String dest_ext = dest_path.get_extension().to_lower();
	if (dest_ext == "jpg" || dest_ext == "jpeg") {
		err = gdreutil::save_image_as_jpeg(dest_path, img);
	} else if (dest_ext == "webp") {
		err = gdreutil::save_image_as_webp(dest_path, img, lossy);
	} else if (dest_ext == "png") {
		err = img->save_png(dest_path);
	} else {
		ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "Invalid file name: " + dest_path);
	}

	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to save image " + dest_path + " from texture " + p_path);

	print_line("Converted " + p_path + " to " + p_dst);
	return OK;
}

Error ImportExporter::convert_tex_to_png(const String &output_dir, const String &p_path, const String &p_dst) {
	return _convert_tex(output_dir, p_path, p_dst, nullptr);
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

Error ImportExporter::convert_sample_to_wav(const String &output_dir, const String &p_path, const String &p_dst) {
	String src_path = _get_path(output_dir, p_path);
	String dst_path = output_dir.path_join(p_dst.replace("res://", ""));
	Error err;

	Ref<AudioStreamWAV> sample = ResourceLoader::load(src_path, "", ResourceFormatLoader::CACHE_MODE_IGNORE, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not load sample file " + p_path);

	err = ensure_dir(dst_path.get_base_dir());
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to create dirs for " + dst_path);

	err = sample->save_to_wav(dst_path);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not save " + p_dst);

	print_line("Converted " + src_path + " to " + dst_path);
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

	print_line("Converted " + src_path + " to " + dst_path);
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

	print_line("Converted " + src_path + " to " + dst_path);
	return OK;
}
String ImportExporter::get_editor_message() {
	String report = "";
	report += "Use Godot editor version " + itos(get_ver_major()) + "." + itos(get_ver_minor()) + " to edit the project." + String("\n");
	report += "Note: the project may be using a custom version of Godot. Detection for this has not been implemented yet." + String("\n");
	report += "If you find that you have many non-import errors upon opening the project " + String("\n");
	report += "(i.e. scripts or shaders have many errors), use the original game's binary as the export template." + String("\n");
	return report;
}
String ImportExporter::get_report() {
	String report;
	if (had_encryption_error) {
		report += "Failed to decompile encrypted scripts!\n";
		report += "Set the correct key and try again!\n\n";
	}
	report += "Totals: " + String("\n");
	report += "Decompiled scripts: " + itos(decompiled_scripts.size()) + String("\n");
	report += "Failed scripts: " + itos(failed_scripts.size()) + String("\n");
	report += "Imported resources for export session: 	" + itos(files.size()) + String("\n");
	report += "Successfully converted: 		" + itos(success.size()) + String("\n");
	if (opt_lossy) {
		report += "Lossy: 		" + itos(lossy_imports.size()) + String("\n");
	} else {
		report += "Lossy not converted: 	" + itos(lossy_imports.size()) + String("\n");
	}
	report += "Rewrote metadata: " + itos(rewrote_metadata.size()) + String("\n");
	report += "Failed to rewrite metadata: " + itos(failed_rewrite_md.size()) + String("\n");
	report += "Not converted: " + itos(not_converted.size()) + String("\n");
	report += "Failed conversions: " + itos(failed.size()) + String("\n");
	report += "-------------\n" + String("\n");
	if (lossy_imports.size() > 0) {
		if (opt_lossy) {
			report += "\nThe following files were converted from an import that was stored lossy." + String("\n");
			report += "You may lose fidelity when re-importing these files upon loading the project." + String("\n");
			for (int i = 0; i < lossy_imports.size(); i++) {
				report += lossy_imports[i]->import_path + " to " + lossy_imports[i]->preferred_dest + String("\n");
			}
		} else {
			report += "\nThe following files were not converted from a lossy import." + String("\n");
			for (int i = 0; i < lossy_imports.size(); i++) {
				report += lossy_imports[i]->import_path + String("\n");
			}
		}
	}
	// we skip this for version 2 because we have to rewrite the metadata for nearly all the converted resources
	if (rewrote_metadata.size() > 0 && get_ver_major() != 2) {
		report += "\nThe following files had their import data rewritten:" + String("\n");
		for (int i = 0; i < rewrote_metadata.size(); i++) {
			report += rewrote_metadata[i]->import_path + " to " + rewrote_metadata[i]->preferred_dest + String("\n");
		}
	}
	if (failed_rewrite_md.size() > 0) {
		report += "\nThe following files were converted and saved to a non-original path, but did not have their import data rewritten." + String("\n");
		report += "These files will not be re-imported when loading the project." + String("\n");
		for (int i = 0; i < failed_rewrite_md.size(); i++) {
			report += failed_rewrite_md[i]->import_path + " to " + failed_rewrite_md[i]->preferred_dest + String("\n");
		}
	}
	if (not_converted.size() > 0) {
		report += "\nThe following files were not converted because support has not been implemented yet:" + String("\n");
		for (int i = 0; i < not_converted.size(); i++) {
			report += not_converted[i]->import_path + String("\n");
		}
	}
	if (failed.size() > 0) {
		report += "\nFailed conversions:" + String("\n");
		for (int i = 0; i < failed.size(); i++) {
			report += failed[i]->import_path + String("\n");
		}
	}

	return report;
}

void ImportExporter::print_report() {
	print_line("\n\n********************************EXPORT REPORT********************************" + String("\n"));
	print_line(get_report());
	print_line("\n---------------------------------------------------------------------------------" + String("\n"));
	print_line(get_editor_message());
	print_line("*******************************************************************************\n\n" + String("\n"));
}

void ImportExporter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("decompile_scripts"), &ImportExporter::decompile_scripts);
	ClassDB::bind_method(D_METHOD("load_import_files"), &ImportExporter::load_import_files);
	ClassDB::bind_method(D_METHOD("get_import_files"), &ImportExporter::get_import_files);
	ClassDB::bind_method(D_METHOD("export_imports"), &ImportExporter::export_imports);
	ClassDB::bind_method(D_METHOD("convert_res_txt_2_bin"), &ImportExporter::convert_res_txt_2_bin);
	ClassDB::bind_method(D_METHOD("convert_res_bin_2_txt"), &ImportExporter::convert_res_bin_2_txt);
	ClassDB::bind_method(D_METHOD("convert_tex_to_png"), &ImportExporter::convert_tex_to_png);
	ClassDB::bind_method(D_METHOD("convert_sample_to_wav"), &ImportExporter::convert_sample_to_wav);
	ClassDB::bind_method(D_METHOD("convert_oggstr_to_ogg"), &ImportExporter::convert_oggstr_to_ogg);
	ClassDB::bind_method(D_METHOD("convert_mp3str_to_mp3"), &ImportExporter::convert_mp3str_to_mp3);
	ClassDB::bind_method(D_METHOD("reset"), &ImportExporter::reset);
}

void ImportExporter::reset_log() {
	had_encryption_error = false;
	lossy_imports.clear();
	rewrote_metadata.clear();
	failed_rewrite_md.clear();
	failed.clear();
	success.clear();
	not_converted.clear();
	decompiled_scripts.clear();
	failed_scripts.clear();
}

void ImportExporter::reset() {
	files.clear();
	opt_bin2text = true;
	opt_export_textures = true;
	opt_export_samples = true;
	opt_export_ogg = true;
	opt_export_mp3 = true;
	opt_lossy = true;
	opt_rewrite_imd_v2 = true;
	reset_log();
}

ImportExporter::ImportExporter() {}
ImportExporter::~ImportExporter() {
	reset();
}
