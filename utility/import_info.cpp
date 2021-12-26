#include "import_info.h"
#include "gdre_settings.h"
#include "resource_loader_compat.h"

String ImportInfo::to_string() {
	String s = "ImportInfo: {";
	s += "\n\timport_md_path: " + import_md_path;
	s += "\n\tpath: " + import_path;
	s += "\n\ttype: " + type;
	s += "\n\timporter: " + importer;
	s += "\n\tsource_file: " + source_file;
	s += "\n\tdest_files: [";
	for (int i = 0; i < dest_files.size(); i++) {
		if (i > 0) {
			s += ", ";
		}
		s += " " + dest_files[i];
	}
	s += " ]";
	s += "\n\tparams: {";
	List<Variant> *keys = memnew(List<Variant>);
	params.get_key_list(keys);
	for (int i = 0; i < keys->size(); i++) {
		// skip excessively long options list
		if (i == 8) {
			s += "\n\t\t[..." + itos(keys->size() - i) + " others...]";
			break;
		}
		String t = (*keys)[i];
		s += "\n\t\t" + t + "=" + params[t];
	}
	memdelete(keys);
	s += "\n\t}\n}";
	return s;
}

int ImportInfo::get_import_loss_type() const {
	if (importer == "scene" || importer == "ogg_vorbis" || importer == "mp3" || importer == "wavefront_obj") {
		//These are always imported as either native files or losslessly
		return LOSSLESS;
	}
	if (!has_import_data()) {
		return UNKNOWN;
	}

	if (importer == "texture") {
		int stat = 0;
		String ext = source_file.get_extension();
		// These are always imported in such a way that it is impossible to recover the original file
		// SVG in particular is converted to raster from vector
		// However, you can convert these and rewrite the imports such that they will be imported losslessly next time
		if (ext == "svg" || ext == "jpg") {
			stat |= IMPORTED_LOSSY;
		}
		// Not possible to recover asset used to import losslessly
		if (params.has("compress/mode") && params["compress/mode"].is_num()) {
			stat |= (int)params["compress/mode"] <= 1 ? LOSSLESS : STORED_LOSSY;
		} else if (params.has("storage") && params["storage"].is_num()) {
			stat |= (int)params["storage"] != V2ImportEnums::Storage::STORAGE_COMPRESS_LOSSY ? LOSSLESS : STORED_LOSSY;
		}
		return LossType(stat);
	} else if (importer == "wav" || (ver_major == 2 && importer == "sample")) {
		// Not possible to recover asset used to import losslessly
		if (params.has("compress/mode") && params["compress/mode"].is_num()) {
			return (int)params["compress/mode"] == 0 ? LOSSLESS : STORED_LOSSY;
		}
	}

	if (ver_major == 2 && importer == "font") {
		// Not possible to recover asset used to import losslessly
		if (params.has("mode/mode") && params["mode/mode"].is_num()) {
			return (int)params["mode/mode"] == V2ImportEnums::FontMode::FONT_DISTANCE_FIELD ? LOSSLESS : STORED_LOSSY;
		}
	}

	// We can't say for sure
	return UNKNOWN;
}

void ImportInfo::_init() {
	import_path = "";
	import_md_path = "";
	type = "";
	importer = "";
	source_file = "";
	ver_major = 0;
	ver_minor = 0;
	dest_files = Vector<String>();
	preferred_dest = "";
	params = Dictionary();
	cf.instantiate();
	v2metadata.instantiate();
	v3metadata_prop = Dictionary();
}

Error ImportInfo::load_from_file(const String &p_path, int v_major, int v_minor = 0) {
	_init();
	ver_major = v_major;
	ver_minor = v_minor; // TODO: we could get the minor version number from the binary resource on v3-v4?
	if (ver_major == 2) {
		return load_from_file_v2(p_path);
	}
	String path = GDRESettings::get_singleton()->get_res_path(p_path);
	Error err = cf->load(path);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not load " + path);
	import_md_path = path;
	import_path = cf->get_value("remap", "path", "");
	type = ClassDB::get_compatibility_remapped_class(cf->get_value("remap", "type", ""));
	importer = cf->get_value("remap", "importer", "");
	source_file = cf->get_value("deps", "source_file", "");
	dest_files = cf->get_value("deps", "dest_files", Array());
	v3metadata_prop = cf->get_value("remap", "metadata", Dictionary());
	// special handler for imports with more than one path
	// path won't be found if there are two or more
	if (import_path == "") {
		bool lossy_texture = false;
		List<String> remap_keys;
		cf->get_section_keys("remap", &remap_keys);
		ERR_FAIL_COND_V_MSG(remap_keys.size() == 0, ERR_BUG, "Failed to load import data from " + path);

		// check metadata first
		if (v3metadata_prop.size() != 0 && v3metadata_prop.has("imported_formats")) {
			Array fmts = v3metadata_prop["imported_formats"];
			for (int i = 0; i < fmts.size(); i++) {
				String f_fmt = fmts[i];
				if (remap_keys.find("path." + f_fmt)) {
					import_path = cf->get_value("remap", "path." + f_fmt, "");
					// if it's a texture that's vram compressed, there's a chance that it may have a ghost .stex file
					if (v3metadata_prop.get("vram_texture", false)) {
						lossy_texture = true;
					}
					break;
				}
				WARN_PRINT("Did not find path for imported format " + f_fmt);
			}
		}
		//otherwise, check destination files
		if (import_path == "" && dest_files.size() != 0) {
			import_path = dest_files[0];
		}
		// special case for textures: if we have multiple formats, and it's a lossy import,
		// we look to see if there's a ghost .stex file with the same prefix.
		// Godot 3.x and 4.x will often import it as a lossless stex first, then imports it
		// as lossy textures, and this sometimes ends up in the exported project.
		// It's just not listed in the .import file.
		if (import_path != "" && lossy_texture) {
			String basedir = import_path.get_base_dir();
			Vector<String> split = import_path.get_file().split(".");
			if (split.size() == 4) {
				// for example, "res://.import/Texture.png-cefbe538e1226e204b4081ac39cf177b.s3tc.stex"
				// will become "res://.import/Texture.png-cefbe538e1226e204b4081ac39cf177b.stex"
				String new_path = basedir.plus_file(split[0] + "." + split[1] + "." + split[3]);
				// if we have the ghost .stex, set it to be the import path
				if (GDRESettings::get_singleton()->has_res_path(new_path)) {
					import_path = new_path;
				}
			}
		}
	}

	// If we fail to find the import path, throw error
	ERR_FAIL_COND_V_MSG(import_path == "" || type == String(), ERR_FILE_CORRUPT, p_path + ": file is corrupt");
	if (cf->has_section("params")) {
		List<String> param_keys;
		cf->get_section_keys("params", &param_keys);
		params = Dictionary();
		for (auto E = param_keys.front(); E; E = E->next()) {
			params[E->get()] = cf->get_value("params", E->get(), "");
		}
	}

	return OK;
}

Error ImportInfo::load_from_file_v2(const String &p_path) {
	Error err;
	String dest;
	String type;
	Vector<String> spl = p_path.get_file().split(".");
	// This is an import file, possibly has import metadata
	ResourceFormatLoaderCompat rlc;
	Ref<ImportInfo> t = this;

	err = rlc.get_import_info(p_path, GDRESettings::get_singleton()->get_project_path(), t);

	if (err == OK) {
		// If this is a "converted" file, then it won't have import metadata...
		if (has_import_data()) {
			// If this is a path outside of the project directory, we change it to the ".assets" directory in the project dir
			if (get_source_file().begins_with("../") ||
					(get_source_file().is_absolute_path() && GDRESettings::get_singleton()->is_fs_path(get_source_file()))) {
				dest = String(".assets").plus_file(p_path.replace("res://", "").get_base_dir().plus_file(get_source_file().get_file()));
				source_file = dest;
			}
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
		// others??
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
	source_file = dest;
	// If it's a converted file without metadata, it won't have this, and we need it for checking if the file is lossy or not
	if (importer == "") {
		if (p_path.get_extension() == "scn") {
			importer = "scene";
		} else if (p_path.get_extension() == "res") {
			importer = "resource";
		} else if (p_path.get_extension() == "tex") {
			importer = "texture";
		} else if (p_path.get_extension() == "smp") {
			importer = "sample";
		} else if (p_path.get_extension() == "fnt") {
			importer = "font";
		} else if (p_path.get_extension() == "msh") {
			importer = "mesh";
		} else if (p_path.get_extension() == "xl") {
			importer = "translation";
		} else if (p_path.get_extension() == "pbm") {
			importer = "bitmask";
		} else if (p_path.get_extension() == "cbm") {
			importer = "cubemap";
		} else {
			importer = "none";
		}
	}

	return OK;
}

Error ImportInfo::rename_source(const String &p_new_source) {
	ERR_FAIL_COND_V_MSG(ver_major <= 2, ERR_BUG, "Don't use this for version <= 2 ");
	String old_import_path = import_md_path;
	String new_import_path = import_md_path.get_base_dir().plus_file(p_new_source.get_file() + ".import");

	Ref<ConfigFile> import_file;
	import_file.instantiate();
	Error err = import_file->load(import_md_path);
	ERR_FAIL_COND_V_MSG(err, ERR_BUG, "Failed to load import file " + import_md_path);

	import_file->set_value("deps", "source_file", p_new_source);

	ERR_FAIL_COND_V_MSG(import_file->save(new_import_path) != OK, ERR_BUG, "Failed to save changed import data");
	cf->set_value("deps", "source_file", p_new_source);
	source_file = p_new_source;
	//ERR_FAIL_COND_V_MSG(load_from_file(new_import_path, ver_major, ver_minor) != OK, ERR_BUG, "Failed to reload changed import file");
	DirAccess::remove_file_or_error(old_import_path);
	return OK;
}

Error ImportInfo::reload() {
	return load_from_file(import_md_path, ver_major, ver_minor);
}