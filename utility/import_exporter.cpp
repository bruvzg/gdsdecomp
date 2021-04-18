
#include "import_exporter.h"
#include "bytecode/bytecode_versions.h"
#include "core/crypto/crypto_core.h"
#include "core/io/config_file.h"
#include "core/variant/variant_parser.h"
#include "gdre_packed_data.h"
#include "gdre_settings.h"
#include "modules/minimp3/audio_stream_mp3.h"
#include "modules/regex/regex.h"
#include "modules/stb_vorbis/audio_stream_ogg_vorbis.h"
#include "pcfg_loader.h"
#include "resource_loader_compat.h"
#include "scene/resources/audio_stream_sample.h"
#include "texture_loader_compat.h"
#include "thirdparty/minimp3/minimp3_ex.h"
#include <core/os/dir_access.h>
#include <core/os/file_access.h>
#include <core/os/os.h>
#include <core/version_generated.gen.h>

Vector<String> ImportExporter::get_recursive_dir_list(const String dir, const Vector<String> &wildcards = Vector<String>(), const bool absolute = true, const String rel = "") {
	Vector<String> ret;
	Error err;
	DirAccess *da = DirAccess::open(dir.plus_file(rel), &err);
	ERR_FAIL_COND_V_MSG(!da, ret, "Failed to open directory " + dir);

	if (!da) {
		return ret;
	}
	String base = absolute ? dir : "";
	da->list_dir_begin();
	String f;
	while ((f = da->get_next()) != "") {
		if (f == "." || f == "..") {
			continue;
		} else if (da->current_is_dir()) {
			ret.append_array(get_recursive_dir_list(dir, wildcards, absolute, rel.plus_file(f)));
		} else {
			if (wildcards.size() > 0) {
				for (int i = 0; i < wildcards.size(); i++) {
					if (f.get_file().match(wildcards[i])) {
						ret.append(base.plus_file(rel).plus_file(f));
						break;
					}
				}
			} else {
				ret.append(base.plus_file(rel).plus_file(f));
			}
		}
	}
	memdelete(da);
	return ret;
}

Array ImportExporter::get_import_files() {
	return files;
}

bool ImportExporter::check_if_dir_is_v2(const String &dir) {

	Vector<String> wildcards;
	// these are files that will only show up in version 2
	wildcards.push_back("*.converted.*");
	wildcards.push_back("*.tex");
	wildcards.push_back("*.smp");
	if (get_recursive_dir_list(dir, wildcards).size() > 0) {
		return true;
	} else {
		return false;
	}
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

	return wildcards;
}

Error ImportExporter::load_import_files(const String &dir, const uint32_t p_ver_major) {
	project_dir = dir;
	Vector<String> file_names;
	ver_major = p_ver_major;
	if (dir != "" && !dir.begins_with("res://")) {
		GDRESettings::get_singleton()->set_project_path(dir);
	}

	if (ver_major == 0) {
		ver_major = check_if_dir_is_v2(dir) ? 2 : 3; // we just assume 3 for now
	}

	if (ver_major <= 2) {
		if ((dir == "" || dir.begins_with("res://")) && GDRESettings::get_singleton()->is_pack_loaded()) {
			file_names = GDRESettings::get_singleton()->get_file_list(get_v2_wildcards());
		} else {
			file_names = get_recursive_dir_list(dir, get_v2_wildcards(), false);
		}
		for (int i = 0; i < file_names.size(); i++) {
			if (load_import_file_v2(file_names[i]) != OK) {
				WARN_PRINT("Can't load V2 converted file: " + file_names[i]);
			}
		}
	} else {
		Vector<String> wildcards;
		wildcards.push_back("*.import");
		if ((dir == "" || dir.begins_with("res://")) && GDRESettings::get_singleton()->is_pack_loaded()) {
			file_names = GDRESettings::get_singleton()->get_file_list(wildcards);
		} else {
			file_names = get_recursive_dir_list(dir, wildcards, false);
		}
		for (int i = 0; i < file_names.size(); i++) {
			if (load_import_file(file_names[i]) != OK) {
				WARN_PRINT("Can't load import file: " + file_names[i]);
			}
		}
	}
	return OK;
}

Error ImportExporter::load_import_file_v2(const String &p_path) {
	Error err;
	String dest;
	String type;
	Vector<String> spl = p_path.get_file().split(".");
	Ref<ImportInfo> iinfo;
	iinfo.instance();

	// This is an import file, possibly has import metadata
	ResourceFormatLoaderCompat rlc;
	err = rlc.get_import_info(p_path, project_dir, iinfo);

	if (err == OK) {
		// If this is a "converted" file, then it won't have import metadata...
		if (iinfo->has_import_data()) {
			// If this is a path outside of the project directory, we change it to the ".assets" directory in the project dir
			if (iinfo->get_source_file().begins_with("../") ||
					(iinfo->get_source_file().is_abs_path() && GDRESettings::get_singleton()->is_fs_path(iinfo->get_source_file()))) {

				dest = String(".assets").plus_file(p_path.replace("res://", "").get_base_dir().plus_file(iinfo->get_source_file().get_file()));
				iinfo->source_file = dest;
			}
			files.push_back(iinfo);
			return OK;
		}
		// The file loaded, but there was no metadata and it was not a ".converted." file
	} else if (err == ERR_PRINTER_ON_FIRE) {
		WARN_PRINT("Could not load metadata from " + p_path);
		String new_ext;
		if (p_path.get_extension() == "tex") {
			new_ext = "png";
		} else if (p_path.get_extension() == "smp") {
			new_ext = "wav";
		} else if (p_path.get_extension() == "cbm") {
			new_ext = "cube";
		} else {
			new_ext = "fixme";
		}
		//others??
		dest = String(".assets").plus_file(p_path.replace("res://", "").get_base_dir().plus_file(spl[0] + "." + new_ext));
		// File either didn't load or metadata was corrupt
	} else {
		ERR_FAIL_COND_V_MSG(err != OK, err, "Can't open imported file " + p_path);
	}

	//This is a converted file
	if (p_path.get_file().find(".converted.") != -1) {
		// if this doesn't match "filename.ext.converted.newext"
		ERR_FAIL_COND_V_MSG(spl.size() != 4, ERR_CANT_RESOLVE, "Can't open imported file " + p_path);
		dest = p_path.get_base_dir().plus_file(spl[0] + "." + spl[1]);
	}

	// either it's an import file without metadata or a converted file
	iinfo->source_file = dest;
	// If it's a converted file without metadata, it won't have this, and we need it for checking if the file is lossy or not
	if (iinfo->importer == "") {
		if (p_path.get_extension() == "scn") {
			iinfo->importer = "scene";
		} else if (p_path.get_extension() == "res") {
			iinfo->importer = "resource";
		} else if (p_path.get_extension() == "tex") {
			iinfo->importer = "texture";
		} else if (p_path.get_extension() == "smp") {
			iinfo->importer = "sample";
		} else if (p_path.get_extension() == "fnt") {
			iinfo->importer = "font";
		} else if (p_path.get_extension() == "msh") {
			iinfo->importer = "mesh";
		} else if (p_path.get_extension() == "xl") {
			iinfo->importer = "translation";
		} else if (p_path.get_extension() == "pbm") {
			iinfo->importer = "bitmask";
		} else if (p_path.get_extension() == "cbm") {
			iinfo->importer = "cubemap";
		} else {
			iinfo->importer = "none";
		}
	}

	files.push_back(iinfo);

	return OK;
}

Error ImportExporter::load_import_file(const String &p_path) {
	Ref<ConfigFile> cf;
	cf.instance();
	String path = GDRESettings::get_singleton()->get_res_path(p_path);
	Error err = cf->load(path);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not load " + path);
	Ref<ImportInfo> i_info;
	i_info.instance();
	i_info->import_md_path = path;
	i_info->import_path = cf->get_value("remap", "path", "");
	i_info->type = ClassDB::get_compatibility_remapped_class(cf->get_value("remap", "type", ""));
	i_info->importer = cf->get_value("remap", "importer", "");
	i_info->source_file = cf->get_value("deps", "source_file", "");
	i_info->dest_files = cf->get_value("deps", "dest_files", Array());
	i_info->v3metadata_prop = cf->get_value("remap", "metadata", Dictionary());
	i_info->version = ver_major;
	bool path_found = i_info->import_path == "";
	// special handler for imports with more than one path
	if (!path_found) {
		bool lossy_texture = false;
		List<String> remap_keys;
		cf->get_section_keys("remap", &remap_keys);
		ERR_FAIL_COND_V_MSG(remap_keys.size() == 0, ERR_BUG, "Failed to load import data from " + path);

		// check metadata first
		if (i_info->v3metadata_prop.size() != 0 && i_info->v3metadata_prop.has("imported_formats")) {
			Array fmts = i_info->v3metadata_prop["imported_formats"];
			for (int i = 0; i < fmts.size(); i++) {
				String f_fmt = fmts[i];
				if (remap_keys.find("path." + f_fmt)) {
					i_info->import_path = cf->get_value("remap", "path." + f_fmt, "");
					path_found = true;
					// if it's a texture that's vram compressed, there's a chance that it may have a ghost .stex file
					if (i_info->v3metadata_prop.get("vram_texture", false)) {
						lossy_texture = true;
					}
					break;
				}
				WARN_PRINT("Did not find path for imported format " + f_fmt);
			}
		}
		//otherwise, check destination files
		if (!path_found && i_info->dest_files.size() != 0) {
			i_info->import_path = i_info->dest_files[0];
			path_found = true;
		}
		// special case for textures: if we have multiple formats, and it's a lossy import,
		// we look to see if there's a ghost .stex file with the same prefix.
		// Godot 3.x and 4.x will often import it as a lossless stex first, then imports it
		// as lossy textures, and this sometimes ends up in the exported project.
		// It's just not listed in the .import file.
		if (path_found && lossy_texture) {
			String basedir = i_info->import_path.get_base_dir();
			Vector<String> split = i_info->import_path.get_file().split(".");
			if (split.size() == 4) {
				// for example, "res://.import/Texture.png-cefbe538e1226e204b4081ac39cf177b.s3tc.stex"
				// will become "res://.import/Texture.png-cefbe538e1226e204b4081ac39cf177b.stex"
				String new_path = basedir.plus_file(split[0] + "." + split[1] + "." + split[3]);
				// if we have the ghost .stex, set it to be the import path
				if (GDRESettings::get_singleton()->has_res_path(new_path)) {
					i_info->import_path = new_path;
				}
			}
		}
	}

	ERR_FAIL_COND_V_MSG(!path_found || i_info->type == String(), ERR_FILE_CORRUPT, p_path + ": file is corrupt");
	if (cf->has_section("params")) {
		List<String> param_keys;
		cf->get_section_keys("params", &param_keys);
		i_info->params = Dictionary();
		for (auto E = param_keys.front(); E; E = E->next()) {
			i_info->params[E->get()] = cf->get_value("params", E->get(), "");
		}
	}
	i_info->cf = cf;

	files.push_back(i_info);
	return OK;
}

// export all the imported resources
Error ImportExporter::export_imports(const String &output_dir) {
	String out_dir = output_dir == "" ? output_dir : GDRESettings::get_singleton()->get_project_path();
	Error err = OK;
	if (opt_lossy) {
		WARN_PRINT_ONCE("Converting lossy imports, you may lose fidelity for indicated assets when re-importing upon loading the project");
	}

	for (int i = 0; i < files.size(); i++) {
		Ref<ImportInfo> iinfo = files[i];
		String path = iinfo->get_path();
		String source = iinfo->get_source_file();
		String type = iinfo->get_type();
		String importer = iinfo->importer;
		auto loss_type = iinfo->get_import_loss_type();
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
				case TextureLoaderCompat::FORMAT_V4_STREAM_TEXTURE2D: {
					// Export texture
					err = export_texture(out_dir, iinfo);
				} break;
				case TextureLoaderCompat::FORMAT_NOT_TEXTURE:
					if (err == ERR_FILE_UNRECOGNIZED) {
						WARN_PRINT("Import of type " + type + " is not a texture(?): " + path);
					} else {
						WARN_PRINT("Failed to load texture " + type + " " + path);
					}
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
		} else if (opt_bin2text && (iinfo->import_path.get_extension() == "res") ||
				   (importer == "scene" && (iinfo->source_file.get_extension() == "tscn" ||
												   iinfo->source_file.get_extension() == "escn"))) {
			iinfo->preferred_dest = iinfo->source_file;
			err = convert_res_bin_2_txt(output_dir, iinfo->import_path, iinfo->source_file);
		} else {
			WARN_PRINT_ONCE("Conversion for " + type + " not implemented");
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
		} else if (err != OK) {
			failed.push_back(iinfo);
		} else {
			if (loss_type != ImportInfo::LOSSLESS) {
				lossy_imports.push_back(iinfo);
			}
			success.push_back(iinfo);
		}
	}
	print_report();
	return OK;
}

Error ImportExporter::export_texture(const String &output_dir, Ref<ImportInfo> &iinfo) {
	String path = iinfo->get_path();
	String source = iinfo->get_source_file();
	String dest = source;
	bool rewrite_metadata = false;

	// We currently only rewrite the metadata for v2
	// This is essentially mandatory for v2 resources because they can be imported outside the
	// project directory tree and the import metadata often points to locations that don't exist.
	if (iinfo->version == 2) {
		rewrite_metadata = true;
	}

	// We only convert textures to png, so we need to rename the destination
	if (source.get_extension() != "png") {
		dest = source.get_basename() + ".png";
		// If this is version 3-4, we need to rewrite the import metadata to point to the new resource name
		rewrite_metadata = true;
		// version 3-4 import rewrite not yet implemented, we catch this down below
		// Instead...
		if (iinfo->version > 2) {
			// save it under .assets, which won't be picked up for import by the godot editor
			if (!dest.replace("res://", "").begins_with(".assets")) {
				String prefix = ".assets";
				if (dest.begins_with("res://")) {
					prefix = "res://.assets";
				}
				dest = prefix.plus_file(dest.replace("res://", ""));
			}
		}
	}

	iinfo->preferred_dest = dest;
	String r_name;
	Error err = _convert_tex_to_png(output_dir, path, dest, &r_name);
	if (err != OK || !rewrite_metadata) {
		return err;
	} else if (rewrite_metadata && iinfo->version == 2) {
		err = rewrite_v2_import_metadata(path, dest, r_name, output_dir);
		if (err == OK) {
			return ERR_PRINTER_ON_FIRE;
		}
		return ERR_DATABASE_CANT_WRITE;
	} else {
		if (iinfo->version != 2) {
			WARN_PRINT_ONCE("Godot 3.x/4.x import data rewrite not yet implemented");
			WARN_PRINT_ONCE("These assets will not be re-imported when loading the project");
		}
		print_line("Exported " + path + " as " + dest + " but not rewriting import metadata");
		// ! version 3-4 import rewrite not yet implemented
		return ERR_DATABASE_CANT_WRITE;
		//Error err = rewrite_import_data(dest, output_dir, iinfo);
		//ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to rewrite import metadata " + iinfo->import_md_path);
		//print_line("rewrote metadata file " + iinfo->import_md_path);
	}
	return err;
}

Error ImportExporter::export_sample(const String &output_dir, Ref<ImportInfo> &iinfo) {
	String path = iinfo->get_path();
	String source = iinfo->get_source_file();
	String dest = source;

	iinfo->preferred_dest = dest;
	if (iinfo->version == 2) {
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

Error ImportExporter::get_md5_hash(const String &path, String &hash_str) {
	FileAccess *file = FileAccess::open(path, FileAccess::READ);
	if (!file) {
		return ERR_FILE_CANT_OPEN;
	}
	CryptoCore::MD5Context ctx;
	ctx.start();

	int64_t rq_size = file->get_len();
	uint8_t buf[32768];

	while (rq_size > 0) {
		int got = file->get_buffer(buf, MIN(32768, rq_size));
		if (got > 0) {
			ctx.update(buf, got);
		}
		if (got < 4096)
			break;
		rq_size -= 32768;
	}
	unsigned char hash[16];
	ctx.finish(hash);
	hash_str = String::md5(hash);
	file->close();
	memdelete(file);
	return OK;
}

// Godot v3-v4 import data rewriting
// TODO: rethink this, this isn't going to work by itself
// currently only needed for v3-v4 textures that were imported from something other than PNGs
// i.e. ground.jpg -> ground.png
// Need to either:
// 1) Implement renaming dependencies in the RFLC and load every other resource type after textures
// and rewrite the resource if necessary,
// 2) do remapping trickery,
// 3) just save them as lossy files?
Error ImportExporter::rewrite_import_data(const String &rel_dest_path, const String &output_dir, const Ref<ImportInfo> &iinfo) {
	String new_source = rel_dest_path;
	String new_import_file = iinfo->import_md_path.get_base_dir().plus_file(rel_dest_path.get_file() + ".import");
	Array new_dest_files;
	ERR_FAIL_COND_V_MSG(iinfo->dest_files.size() == 0, ERR_BUG, "Failed to change import data for " + rel_dest_path);
	for (int i = 0; i < iinfo->dest_files.size(); i++) {
		String old_dest = iinfo->dest_files[i];
		String new_dest = old_dest.replace(iinfo->source_file.get_file(), new_source.get_file());
		ERR_FAIL_COND_V_MSG(old_dest == new_dest, ERR_BUG, "Failed to change import data for " + rel_dest_path);
		new_dest_files.append(new_dest);
	}
	Ref<ConfigFile> import_file;
	import_file.instance();
	Error err = import_file->load(iinfo->import_md_path);
	ERR_FAIL_COND_V_MSG(err, ERR_BUG, "Failed to load import file " + iinfo->import_md_path);

	import_file->set_value("deps", "source_file", new_source);
	import_file->set_value("deps", "dest_files", new_dest_files);
	if (new_dest_files.size() > 1) {
		List<String> remap_keys;
		import_file->get_section_keys("remap", &remap_keys);
		ERR_FAIL_COND_V_MSG(remap_keys.size() == 0, ERR_BUG, "Failed to change import data for " + rel_dest_path);
		// we likely have multiple paths
		if (iinfo->v3metadata_prop.has("imported_formats")) {
			Vector<String> fmts = iinfo->v3metadata_prop["imported_formats"];
			for (int i = 0; i < fmts.size(); i++) {
				auto E = remap_keys.find("path." + fmts[i]);
				if (E) {
					String path_prop = E->get();
					String old_path = import_file->get_value("remap", path_prop, String());
					String new_path = old_path.replace(iinfo->source_file.get_file(), new_source.get_file());
					ERR_FAIL_COND_V_MSG(old_path == new_path, ERR_BUG, "Failed to change import data for " + rel_dest_path);
					import_file->set_value("remap", path_prop, new_path);
				} else {
					ERR_FAIL_V_MSG(ERR_BUG, "Expected to see path for import format" + fmts[i]);
				}
			}
		}
	} else {
		String old_path = iinfo->import_path;
		String new_path = old_path.replace(iinfo->source_file.get_file(), new_source.get_file());
		ERR_FAIL_COND_V_MSG(old_path == new_path, ERR_BUG, "Failed to change import data for " + rel_dest_path);
		import_file->set_value("remap", "path", new_path);
	}
	return import_file->save(iinfo->import_md_path);
}

// Makes a copy of the import metadata and changes the source to the new path
Ref<ResourceImportMetadatav2> ImportExporter::change_v2import_data(const String &p_path, const String &rel_dest_path, const String &p_res_name, const String &output_dir, const bool change_extension) {
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
				dst_path = output_dir.plus_file(rel_dest_path.replace("res://", ""));
			}
			String new_hash;
			if (get_md5_hash(dst_path, new_hash) != OK) {
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
	String orig_file = output_dir.plus_file(p_path.replace("res://", ""));
	auto imd = change_v2import_data(p_path, p_dst, p_res_name, output_dir, false);
	ERR_FAIL_COND_V_MSG(imd.is_null(), ERR_FILE_NOT_FOUND, "Failed to get metadata for " + p_path);

	ResourceFormatLoaderCompat rlc;
	err = rlc.rewrite_v2_import_metadata(p_path, orig_file + ".tmp", imd);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to rewrite metadata for " + orig_file);

	DirAccess *dr = DirAccess::open(orig_file.get_base_dir(), &err);
	ERR_FAIL_COND_V_MSG(!dr, err, "Failed to rename file " + orig_file + ".tmp");

	// this may fail, we don't care
	dr->remove(orig_file);
	err = dr->rename(orig_file + ".tmp", orig_file);
	ERR_FAIL_COND_V_MSG(!dr, err, "Failed to rename file " + orig_file + ".tmp");

	print_line("Rewrote import metadata for " + p_path);
	memdelete(dr);
	return OK;
}

Error ImportExporter::ensure_dir(const String &dst_dir) {
	Error err;
	DirAccess *da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	ERR_FAIL_COND_V(!da, ERR_FILE_CANT_OPEN);
	err = da->make_dir_recursive(dst_dir);
	memdelete(da);
	return err;
}

Error ImportExporter::convert_res_bin_2_txt(const String &output_dir, const String &p_path, const String &p_dst) {
	ResourceFormatLoaderCompat rlc;
	Error err = rlc.convert_bin_to_txt(p_path, p_dst, output_dir);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to convert " + p_path + " to " + p_dst);
	print_line("Converted " + p_path + " to " + p_dst);
	return err;
}

Error ImportExporter::_convert_tex_to_png(const String &output_dir, const String &p_path, const String &p_dst, String *r_name) {
	String dst_dir = output_dir.plus_file(p_dst.get_base_dir().replace("res://", ""));
	String dest_path = output_dir.plus_file(p_dst.replace("res://", ""));
	Error err;
	TextureLoaderCompat tl;

	Ref<Image> img = tl.load_image_from_tex(p_path, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to load texture " + p_path);
	if (r_name) {
		*r_name = String(img->get_name());
	}

	err = ensure_dir(dst_dir);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to create dirs for " + dest_path);

	err = img->save_png(dest_path);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to save image " + dest_path);

	print_line("Converted " + p_path + " to " + p_dst);
	return OK;
}

Error ImportExporter::convert_tex_to_png(const String &output_dir, const String &p_path, const String &p_dst) {
	return _convert_tex_to_png(output_dir, p_path, p_dst, nullptr);
}

String ImportExporter::_get_path(const String &output_dir, const String &p_path) {
	if (GDRESettings::get_singleton()->get_project_path() == "" && !GDRESettings::get_singleton()->is_pack_loaded()) {
		if (p_path.is_abs_path()) {
			return p_path;
		} else {
			return output_dir.plus_file(p_path.replace("res://", ""));
		}
	}
	if (GDRESettings::get_singleton()->has_res_path(p_path)) {
		return GDRESettings::get_singleton()->get_res_path(p_path);
	} else if (GDRESettings::get_singleton()->has_res_path(p_path, output_dir)) {
		return GDRESettings::get_singleton()->get_res_path(p_path, output_dir);
	} else {
		return output_dir.plus_file(p_path.replace("res://", ""));
	}
}

Error ImportExporter::convert_sample_to_wav(const String &output_dir, const String &p_path, const String &p_dst) {
	String src_path = _get_path(output_dir, p_path);
	String dst_path = output_dir.plus_file(p_dst.replace("res://", ""));
	Error err;

	Ref<AudioStreamSample> sample = ResourceLoader::load(src_path, "", ResourceFormatLoader::CACHE_MODE_IGNORE, &err);
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
	String dst_path = output_dir.plus_file(p_dst.replace("res://", ""));
	Error err;

	Ref<AudioStreamOGGVorbis> sample = ResourceLoader::load(src_path, "", ResourceFormatLoader::CACHE_MODE_IGNORE, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not load oggstr file " + p_path);

	err = ensure_dir(dst_path.get_base_dir());
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to create dirs for " + dst_path);

	FileAccess *f = FileAccess::open(dst_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not open " + p_dst + " for saving");

	PackedByteArray data = sample->get_data();
	f->store_buffer(data.ptr(), data.size());

	print_line("Converted " + src_path + " to " + dst_path);
	return OK;
}

Error ImportExporter::convert_mp3str_to_mp3(const String &output_dir, const String &p_path, const String &p_dst) {
	String src_path = _get_path(output_dir, p_path);
	String dst_path = output_dir.plus_file(p_dst.replace("res://", ""));
	Error err;

	Ref<AudioStreamMP3> sample = ResourceLoader::load(src_path, "", ResourceFormatLoader::CACHE_MODE_IGNORE, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not load mp3str file " + p_path);

	err = ensure_dir(dst_path.get_base_dir());
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to create dirs for " + dst_path);

	FileAccess *f = FileAccess::open(dst_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not open " + p_dst + " for saving");

	PackedByteArray data = sample->get_data();
	f->store_buffer(data.ptr(), data.size());

	print_line("Converted " + src_path + " to " + dst_path);
	return OK;
}

void ImportExporter::print_report() {
	print_line("\n\n*********EXPORT REPORT***********");
	print_line("Totals: ");
	print_line("Imports for export session: 	" + itos(files.size()));
	print_line("Successfully converted: 		" + itos(success.size()));
	if (opt_lossy) {
		print_line("Lossy: 		" + itos(lossy_imports.size()));
	} else {
		print_line("Lossy not converted: 	" + itos(lossy_imports.size()));
	}
	print_line("Rewrote metadata: " + itos(rewrote_metadata.size()));
	print_line("Require rewritten metadata but weren't: " + itos(failed_rewrite_md.size()));
	print_line("Not converted: " + itos(not_converted.size()));
	print_line("Failed conversions: " + itos(failed.size()));
	print_line("-------------\n");
	if (lossy_imports.size() > 0) {
		if (opt_lossy) {
			print_line("\nThe following files were converted from an import that was stored lossy.");
			print_line("You may lose fidelity when re-importing these files upon loading the project.");
			for (int i = 0; i < lossy_imports.size(); i++) {
				print_line(lossy_imports[i]->import_path + " to " + lossy_imports[i]->preferred_dest);
			}
		} else {
			print_line("\nThe following files were not converted from a lossy import.");
			for (int i = 0; i < lossy_imports.size(); i++) {
				print_line(lossy_imports[i]->import_path);
			}
		}
	}
	// we skip this for version 2 because we have to rewrite the metadata for nearly all the converted resources
	if (rewrote_metadata.size() > 0 && ver_major != 2) {
		print_line("\nThe following files had their import data rewritten:");
		for (int i = 0; i < rewrote_metadata.size(); i++) {
			print_line(rewrote_metadata[i]->import_path + " to " + rewrote_metadata[i]->preferred_dest);
		}
	}
	if (failed_rewrite_md.size() > 0) {
		print_line("\nThe following files were converted and saved to a non-original path, but did not have their import data rewritten.");
		print_line("These files will not be re-imported when loading the project.");
		for (int i = 0; i < failed_rewrite_md.size(); i++) {
			print_line(failed_rewrite_md[i]->import_path + " to " + failed_rewrite_md[i]->preferred_dest);
		}
	}
	if (not_converted.size() > 0) {
		print_line("\nThe following files were not converted because support has not been implemented yet:");
		for (int i = 0; i < not_converted.size(); i++) {
			print_line(not_converted[i]->import_path);
		}
	}
	if (failed.size() > 0) {
		print_line("\nFailed conversions:");
		for (int i = 0; i < failed.size(); i++) {
			print_line(failed[i]->import_path);
		}
	}
	print_line("*********************************\n\n");
}

void ImportExporter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load_import_files"), &ImportExporter::load_import_files);
	ClassDB::bind_method(D_METHOD("get_import_files"), &ImportExporter::get_import_files);
	ClassDB::bind_method(D_METHOD("export_imports"), &ImportExporter::export_imports);
	ClassDB::bind_method(D_METHOD("convert_res_bin_2_txt"), &ImportExporter::convert_res_bin_2_txt);
	ClassDB::bind_method(D_METHOD("convert_tex_to_png"), &ImportExporter::convert_tex_to_png);
	ClassDB::bind_method(D_METHOD("convert_sample_to_wav"), &ImportExporter::convert_sample_to_wav);
	ClassDB::bind_method(D_METHOD("convert_oggstr_to_ogg"), &ImportExporter::convert_oggstr_to_ogg);
	ClassDB::bind_method(D_METHOD("convert_mp3str_to_mp3"), &ImportExporter::convert_mp3str_to_mp3);
	ClassDB::bind_method(D_METHOD("reset"), &ImportExporter::reset);
}

void ImportExporter::reset() {
	files.clear();
	opt_bin2text = true;
	opt_export_textures = true;
	opt_export_samples = true;
	opt_export_ogg = true;
	opt_export_mp3 = true;
	opt_lossy = true;
	opt_rewrite_imd = true;
	GDRESettings::get_singleton()->set_project_path("");
	files_lossy_exported.clear();
	files_rewrote_metadata.clear();
}

ImportExporter::ImportExporter() {}
ImportExporter::~ImportExporter() {
	reset();
}
