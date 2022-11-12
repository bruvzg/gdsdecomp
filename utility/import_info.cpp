#include "import_info.h"
#include "compat/resource_loader_compat.h"
#include "gdre_settings.h"

String ImportInfo::to_string() {
	return as_text(false);
}

String ImportInfo::as_text(bool full) {
	String s = "ImportInfo: {";
	s += "\n\timport_md_path: " + import_md_path;
	s += "\n\tpath: " + get_path();
	s += "\n\ttype: " + get_type();
	s += "\n\timporter: " + get_importer();
	s += "\n\tsource_file: " + get_source_file();
	s += "\n\tdest_files: [";
	Vector<String> dest_files = get_dest_files();
	for (int i = 0; i < dest_files.size(); i++) {
		if (i > 0) {
			s += ", ";
		}
		s += " " + dest_files[i];
	}
	s += " ]";
	s += "\n\tparams: {";
	List<Variant> *keys = memnew(List<Variant>);
	Dictionary params = get_params();
	params.get_key_list(keys);
	for (int i = 0; i < keys->size(); i++) {
		// skip excessively long options list
		if (!full && i == 8) {
			s += "\n\t\t[..." + itos(keys->size() - i) + " others...]";
			break;
		}
		String t = (*keys)[i];
		s += "\n\t\t" + t + "=" + (String)params[t];
	}
	memdelete(keys);
	s += "\n\t}\n}";
	return s;
}

int ImportInfo::get_import_loss_type() const {
	String importer = get_importer();
	String source_file = get_source_file();
	Dictionary params = get_params();
	if (importer == "scene" || importer == "ogg_vorbis" || importer == "mp3" || importer == "wavefront_obj") {
		//These are always imported as either native files or losslessly
		return LOSSLESS;
	}
	if (!has_import_data() || !is_import()) {
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

Ref<ConfigFile> copy_config_file(Ref<ConfigFile> p_cf) {
	Ref<ConfigFile> r_cf;
	List<String> *sections = memnew(List<String>);
	//	String sections_string;
	p_cf->get_sections(sections);
	// from bottom to top, because set_value() inserts new sections at top
	for (auto E = sections->back(); E; E = E->prev()) {
		String section = E->get();
		List<String> *section_keys = memnew(List<String>);
		p_cf->get_section_keys(section, section_keys);
		for (auto F = section_keys->front(); F; F->next()) {
			String key = F->get();
			r_cf->set_value(section, key, p_cf->get_value(section, key));
		}
	}
	return r_cf;
}

Ref<ResourceImportMetadatav2> copy_imd_v2(Ref<ResourceImportMetadatav2> p_cf) {
	Ref<ResourceImportMetadatav2> r_imd;
	r_imd.instantiate();
	r_imd->set_editor(p_cf->get_editor());
	for (int i = 0; p_cf->get_source_count(); i++) {
		r_imd->add_source(p_cf->get_source_path(i), p_cf->get_source_md5(i));
	}
	List<String> *r_options = memnew(List<String>);
	p_cf->get_options(r_options);
	for (auto E = r_options->front(); E; E = E->next()) {
		r_imd->set_option(E->get(), p_cf->get_option(E->get()));
	}
	return r_imd;
}

Ref<ImportInfo> ImportInfo::copy(const Ref<ImportInfo> &p_iinfo) {
	Ref<ImportInfo> r_iinfo;
	// int ver_major = 0; //2, 3, 4
	if (p_iinfo->ver_major > 2) {
		r_iinfo = Ref<ImportInfo>(memnew(ImportInfoModern));
		((Ref<ImportInfoModern>)r_iinfo)->src_md5 = ((Ref<ImportInfoModern>)p_iinfo)->src_md5;
		((Ref<ImportInfoModern>)r_iinfo)->cf = copy_config_file(((Ref<ImportInfoModern>)p_iinfo)->cf);
	} else {
		r_iinfo = Ref<ImportInfo>(memnew(ImportInfov2));
		((Ref<ImportInfov2>)r_iinfo)->type = ((Ref<ImportInfov2>)p_iinfo)->type;
		((Ref<ImportInfov2>)r_iinfo)->dest_files = ((Ref<ImportInfov2>)p_iinfo)->dest_files;
		((Ref<ImportInfov2>)r_iinfo)->v2metadata = copy_imd_v2(((Ref<ImportInfov2>)p_iinfo)->v2metadata);
	}
	r_iinfo->import_md_path = p_iinfo->import_md_path;
	r_iinfo->ver_major = p_iinfo->ver_major;
	r_iinfo->ver_minor = p_iinfo->ver_minor;
	r_iinfo->format_ver = p_iinfo->format_ver;
	r_iinfo->not_an_import = p_iinfo->not_an_import;
	r_iinfo->auto_converted_export = p_iinfo->auto_converted_export;
	r_iinfo->preferred_import_path = p_iinfo->preferred_import_path;
	r_iinfo->export_dest = p_iinfo->export_dest;
	r_iinfo->export_lossless_copy = p_iinfo->export_lossless_copy;
	return r_iinfo;
}

void ImportInfo::_init() {
	import_md_path = "";
	ver_major = 0;
	ver_minor = 0;
	export_dest = "";
}

Ref<ImportInfo> ImportInfo::load_from_file(const String &p_path, int ver_major, int ver_minor) {
	Ref<ImportInfo> iinfo;
	Error err = OK;

	if (p_path.get_extension() == "import") {
		iinfo = Ref<ImportInfo>(memnew(ImportInfoModern));
		err = iinfo->_load(p_path);
		ERR_FAIL_COND_V_MSG(err != OK, Ref<ImportInfo>(), "Could not load " + p_path);
		if (ver_major == 0) {
			ResourceFormatLoaderCompat rlc;
			_ResourceInfo res_info;
			String res_path = iinfo->get_path();
			if (!GDRESettings::get_singleton()->has_res_path(res_path)) {
				WARN_PRINT("ImportInfo: Version major not specified and could not load binary resource file!");
			} else {
				err = rlc.get_import_info(res_path, GDRESettings::get_singleton()->get_project_path(), res_info);
				ERR_FAIL_COND_V_MSG(err != OK, iinfo, "Version major not specified and could not load " + res_path);
				iinfo->ver_major = res_info.ver_major;
				iinfo->ver_minor = res_info.ver_minor;
				if (res_info.type != iinfo->get_type()) {
					WARN_PRINT(p_path + ": binary resource type " + res_info.type + " does not equal import type " + iinfo->get_type() + "???");
				}
			}
		} else {
			iinfo->ver_major = ver_major;
			iinfo->ver_minor = ver_minor;
		}
	} else {
		iinfo = Ref<ImportInfo>(memnew(ImportInfov2));
		err = iinfo->_load(p_path);
		ERR_FAIL_COND_V_MSG(err != OK, Ref<ImportInfo>(), "Could not load " + p_path);
	}
	return iinfo;
}

String ImportInfoModern::get_type() const {
	return cf->get_value("remap", "type", "");
}

void ImportInfoModern::set_type(const String &p_type) {
	cf->set_value("remap", "type", "");
}

String ImportInfoModern::get_compat_type() const {
	return ClassDB::get_compatibility_remapped_class(get_type());
}

String ImportInfoModern::get_importer() const {
	return cf->get_value("remap", "importer", "");
}

String ImportInfoModern::get_source_file() const {
	return cf->get_value("deps", "source_file", "");
}

void ImportInfoModern::set_source_file(const String &p_path) {
	cf->set_value("deps", "source_file", p_path);
}

void ImportInfoModern::set_source_and_md5(const String &path, const String &md5) {
	cf->set_value("deps", "source_file", path);
	src_md5 = md5;
	// TODO: change the md5 file?
}

String ImportInfoModern::get_source_md5() const {
	return src_md5;
}

void ImportInfoModern::set_source_md5(const String &md5) {
	src_md5 = md5;
}

String ImportInfoModern::get_uid() const {
	return cf->get_value("remap", "uid", "");
}

Vector<String> ImportInfoModern::get_dest_files() const {
	return cf->get_value("deps", "dest_files", Vector<String>());
}

void ImportInfoModern::set_dest_files(const Vector<String> p_dest_files) {
	cf->set_value("deps", "dest_files", p_dest_files);
	if (!cf->has_section("remap")) {
		return;
	}
	if (p_dest_files.size() > 1) {
		List<String> remap_keys;
		cf->get_section_keys("remap", &remap_keys);
		// if set, we likely have multiple paths
		if (get_metadata_prop().has("imported_formats")) {
			Vector<String> fmts = get_metadata_prop()["imported_formats"];
			for (int i = 0; i < p_dest_files.size(); i++) {
				Vector<String> spl = p_dest_files[i].split(".");
				// second to last split
				ERR_FAIL_COND_MSG(spl.size() >= 4, "Expected to see format in path " + p_dest_files[i]);
				String ext = spl[spl.size() - 2];
				List<String>::Element *E = remap_keys.find("path." + ext);

				if (!E) {
					WARN_PRINT("Dunno what happened here, setting remap path dumbly");
					E = remap_keys.find("path." + fmts[i]);
					ERR_FAIL_COND_MSG(!E, "failed to find path." + fmts[i] + " key in remaps section");
				}
				cf->set_value("remap", E->get(), p_dest_files[i]);
			}
		} else {
			ERR_FAIL_MSG("we don't have imported_formats in the remap metadata...????");
		}
	} else if (cf->has_section_key("remap", "path")) {
		cf->set_value("remap", "path", p_dest_files[0]);
	} else {
		ERR_FAIL_MSG("We don't have a path key...?");
	}
}

Dictionary ImportInfoModern::get_metadata_prop() const {
	return cf->get_value("remap", "metadata", Dictionary());
}

void ImportInfoModern::set_metadata_prop(Dictionary r_dict) {
	cf->set_value("remap", "metadata", Dictionary());
}

Variant ImportInfoModern::get_param(const String &p_key) const {
	return cf->get_value("params", p_key);
}

void ImportInfoModern::set_param(const String &p_key, const Variant &p_val) {
	cf->set_value("params", p_key, p_val);
}

bool ImportInfoModern::has_param(const String &p_key) const {
	return cf->has_section_key("params", p_key);
}

Variant ImportInfoModern::get_iinfo_val(const String &p_section, const String &p_prop) const {
	return cf->get_value(p_section, p_prop);
}

void ImportInfoModern::set_iinfo_val(const String &p_section, const String &p_prop, const Variant &p_val) {
	cf->set_value(p_section, p_prop, p_val);
}

bool ImportInfoModern::has_import_data() const {
	return cf.is_valid();
}

Dictionary ImportInfoModern::get_params() const {
	Dictionary params;
	if (cf->has_section("params")) {
		List<String> param_keys;
		cf->get_section_keys("params", &param_keys);
		params = Dictionary();
		for (auto E = param_keys.front(); E; E = E->next()) {
			params[E->get()] = cf->get_value("params", E->get(), "");
		}
	}
	return params;
}

void ImportInfoModern::set_params(Dictionary params) {
	List<Variant> param_keys;
	params.get_key_list(&param_keys);
	for (auto E = param_keys.front(); E; E = E->next()) {
		cf->set_value("params", E->get(), params[E->get()]);
	}
}

Error ImportInfoModern::_load(const String &p_path) {
	cf.instantiate();
	String path = GDRESettings::get_singleton()->get_res_path(p_path);
	Error err = cf->load(path);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not load " + path);
	import_md_path = path;
	preferred_import_path = cf->get_value("remap", "path", "");
	// special handler for imports with more than one path
	// path won't be found if there are two or more
	if (preferred_import_path.is_empty()) {
		bool lossy_texture = false;
		List<String> remap_keys;
		cf->get_section_keys("remap", &remap_keys);
		ERR_FAIL_COND_V_MSG(remap_keys.size() == 0, ERR_BUG, "Failed to load import data from " + path);

		// check metadata first
		if (get_metadata_prop().size() != 0 && get_metadata_prop().has("imported_formats")) {
			Dictionary mdprop = get_metadata_prop();
			Array fmts = mdprop["imported_formats"];
			for (int i = 0; i < fmts.size(); i++) {
				String f_fmt = fmts[i];

				if (remap_keys.find("path." + f_fmt)) {
					preferred_import_path = cf->get_value("remap", "path." + f_fmt, "");
					// Make sure the res exists first
					if (!GDRESettings::get_singleton()->has_res_path(preferred_import_path)) {
						continue;
					}
					// if it's a texture that's vram compressed, there's a chance that it may have a ghost texture file
					if (mdprop.get("vram_texture", false)) {
						lossy_texture = true;
					}
					break;
				}
				WARN_PRINT("Did not find path for imported format " + f_fmt);
			}
		}
		//otherwise, check destination files
		if (preferred_import_path == "" && preferred_import_path.size() != 0) {
			preferred_import_path = get_dest_files()[0];
		}
		// special case for textures: if we have multiple formats, and it's a lossy import,
		// we look to see if there's a ghost texture file with the same prefix.
		// Godot 3.x and 4.x will often import it as a lossless texture first, then imports it
		// as lossy textures, and this sometimes ends up in the exported project.
		// It's just not listed in the .import file.
		if (preferred_import_path != "" && lossy_texture) {
			String basedir = preferred_import_path.get_base_dir();
			Vector<String> split = preferred_import_path.get_file().split(".");
			if (split.size() == 4) {
				// for example, "res://.import/Texture.png-cefbe538e1226e204b4081ac39cf177b.s3tc.stex"
				// will become "res://.import/Texture.png-cefbe538e1226e204b4081ac39cf177b.stex"
				String new_path = basedir.path_join(split[0] + "." + split[1] + "." + split[3]);
				// if we have the ghost texture, set it to be the import path
				if (GDRESettings::get_singleton()->has_res_path(new_path)) {
					preferred_import_path = new_path;
				}
			}
		}
	}

	// If we fail to find the import path, throw error
	ERR_FAIL_COND_V_MSG(preferred_import_path.is_empty() || get_type().is_empty(), ERR_FILE_CORRUPT, p_path + ": file is corrupt");

	return OK;
}

Error ImportInfov2::_load(const String &p_path) {
	Error err;
	ResourceFormatLoaderCompat rlc;
	_ResourceInfo res_info;
	preferred_import_path = p_path;
	err = rlc.get_import_info(p_path, GDRESettings::get_singleton()->get_project_path(), res_info);
	String dest;
	String source_file;
	String importer;
	// This is an import file, possibly has import metadata
	type = res_info.type;
	import_md_path = p_path;
	dest_files.push_back(p_path);
	if (res_info.v2metadata.is_valid()) {
		v2metadata = res_info.v2metadata;
		return OK;
	}
	Vector<String> spl = p_path.get_file().split(".");
	// Otherwise, we dont have any meta data, and we have to guess what it is
	// If this is a "converted" file, then it won't have import metadata, and we expect that
	if (!res_info.auto_converted_export) {
		// The file loaded, but there was no metadata and it was not a ".converted." file
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
		source_file = String("res://.assets").path_join(p_path.replace("res://", "").get_base_dir().path_join(spl[0] + "." + new_ext));
	} else {
		auto_converted_export = true;
		// if this doesn't match "filename.ext.converted.newext"
		ERR_FAIL_COND_V_MSG(spl.size() != 4, ERR_CANT_RESOLVE, "Can't open imported file " + p_path);
		source_file = p_path.get_base_dir().path_join(spl[0] + "." + spl[1]);
	}

	not_an_import = true;
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
	v2metadata.instantiate();
	v2metadata->add_source(source_file);
	v2metadata->set_editor(importer);
	return OK;
}

String ImportInfov2::get_type() const {
	return type;
}

void ImportInfov2::set_type(const String &p_type) {
	type = p_type;
}

String ImportInfov2::get_compat_type() const {
	return ClassDB::get_compatibility_remapped_class(get_type());
}

String ImportInfov2::get_importer() const {
	return v2metadata->get_editor();
}

String ImportInfov2::get_source_file() const {
	return v2metadata->get_source_path(0);
}

void ImportInfov2::set_source_file(const String &p_path) {
	set_source_and_md5(p_path, "");
}

void ImportInfov2::set_source_and_md5(const String &path, const String &md5) {
	v2metadata->remove_source(0);
	v2metadata->add_source_at(path, md5, 0);
}

String ImportInfov2::get_source_md5() const {
	return v2metadata->get_source_md5(0);
}

void ImportInfov2::set_source_md5(const String &md5) {
	v2metadata->set_source_md5(0, md5);
}

Vector<String> ImportInfov2::get_dest_files() const {
	return Vector<String>({ dest_files[0] });
}

void ImportInfov2::set_dest_files(const Vector<String> p_dest_files) {
	dest_files = p_dest_files;
}

Vector<String> ImportInfov2::get_additional_sources() const {
	Vector<String> srcs;
	for (int i = 1; i < v2metadata->get_source_count(); i++) {
		srcs.push_back(v2metadata->get_source_path(i));
	}
	return srcs;
}

void ImportInfov2::set_additional_sources(const Vector<String> &p_add_sources) {
	// TODO: md5s
	for (int i = 1; i < p_add_sources.size(); i++) {
		v2metadata->remove_source(i);
		v2metadata->add_source_at(p_add_sources[i], "", i);
	}
}

Variant ImportInfov2::get_param(const String &p_key) const {
	return v2metadata->get_option(p_key);
}

void ImportInfov2::set_param(const String &p_key, const Variant &p_val) {
	return v2metadata->set_option(p_key, p_val);
}

bool ImportInfov2::has_param(const String &p_key) const {
	return v2metadata->has_option(p_key);
}

Variant ImportInfov2::get_iinfo_val(const String &p_section, const String &p_prop) const {
	if (p_section == "params" || p_section == "options") {
		return v2metadata->get_option(p_prop);
	}
	//TODO: others?
	return Variant();
}

void ImportInfov2::set_iinfo_val(const String &p_section, const String &p_prop, const Variant &p_val) {
	if (p_section == "params" || p_section == "options") {
		return v2metadata->set_option(p_prop, p_val);
	}
	//TODO: others?
}

bool ImportInfov2::has_import_data() const {
	return v2metadata.is_valid();
}

Dictionary ImportInfov2::get_params() const {
	return v2metadata->get_options_as_dictionary();
}

void ImportInfov2::set_params(Dictionary params) {
	List<Variant> param_keys;
	params.get_key_list(&param_keys);
	for (auto E = param_keys.front(); E; E = E->next()) {
		v2metadata->set_option(E->get(), params[E->get()]);
	}
}
Error ImportInfoModern::save_to(const String &new_import_file) {
	return cf->save(new_import_file);
}

Error ImportInfov2::save_to(const String &new_import_file) {
	Error err;
	ResourceFormatLoaderCompat rlc;
	err = rlc.rewrite_v2_import_metadata(import_md_path, new_import_file, v2metadata);
	ERR_FAIL_COND_V_MSG(err, err, "Failed to rename file " + import_md_path + ".tmp");
	return err;
}

void ImportInfo::_bind_methods() {
	ClassDB::bind_static_method(get_class_static(), D_METHOD("load_from_file", "path", "ver_major", "ver_minor"), &ImportInfo::load_from_file, DEFVAL(0), DEFVAL(0));
	ClassDB::bind_method(D_METHOD("get_ver_major"), &ImportInfo::get_ver_major);
	ClassDB::bind_method(D_METHOD("get_ver_minor"), &ImportInfo::get_ver_minor);

	ClassDB::bind_method(D_METHOD("get_import_loss_type"), &ImportInfo::get_import_loss_type);

	ClassDB::bind_method(D_METHOD("get_path"), &ImportInfo::get_path);
	ClassDB::bind_method(D_METHOD("set_preferred_resource_path", "path"), &ImportInfo::set_preferred_resource_path);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "preferred_import_path"), "set_preferred_resource_path", "get_path");

	ClassDB::bind_method(D_METHOD("is_auto_converted"), &ImportInfo::is_auto_converted);
	ClassDB::bind_method(D_METHOD("is_import"), &ImportInfo::is_import);

	ClassDB::bind_method(D_METHOD("get_import_md_path"), &ImportInfo::get_import_md_path);
	ClassDB::bind_method(D_METHOD("set_import_md_path", "path"), &ImportInfo::set_import_md_path);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "import_md_path"), "set_import_md_path", "get_import_md_path");

	ClassDB::bind_method(D_METHOD("get_export_dest"), &ImportInfo::get_export_dest);
	ClassDB::bind_method(D_METHOD("set_export_dest", "path"), &ImportInfo::set_export_dest);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "export_dest"), "set_export_dest", "get_export_dest");

	ClassDB::bind_method(D_METHOD("get_export_lossless_copy"), &ImportInfo::get_export_lossless_copy);
	ClassDB::bind_method(D_METHOD("set_export_lossless_copy", "path"), &ImportInfo::set_export_lossless_copy);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "export_lossless_copy"), "set_export_lossless_copy", "get_export_lossless_copy");

	ClassDB::bind_method(D_METHOD("get_type"), &ImportInfo::get_type);
	ClassDB::bind_method(D_METHOD("set_type", "path"), &ImportInfo::set_type);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "type"), "set_type", "get_type");

	ClassDB::bind_method(D_METHOD("get_compat_type"), &ImportInfo::get_compat_type);

	ClassDB::bind_method(D_METHOD("get_importer"), &ImportInfo::get_importer);

	ClassDB::bind_method(D_METHOD("get_source_file"), &ImportInfo::get_source_file);
	ClassDB::bind_method(D_METHOD("set_source_file", "path"), &ImportInfo::set_source_file);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "source_file"), "set_source_file", "get_source_file");

	ClassDB::bind_method(D_METHOD("get_source_md5"), &ImportInfo::get_source_md5);
	ClassDB::bind_method(D_METHOD("set_source_md5", "md5"), &ImportInfo::set_source_md5);
	ADD_PROPERTY(PropertyInfo(Variant::STRING, "source_md5"), "set_source_md5", "get_source_md5");

	ClassDB::bind_method(D_METHOD("get_uid"), &ImportInfo::get_uid);

	ClassDB::bind_method(D_METHOD("get_dest_files"), &ImportInfo::get_dest_files);
	ClassDB::bind_method(D_METHOD("set_dest_files", "dest_files"), &ImportInfo::set_dest_files);
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "dest_files"), "set_dest_files", "get_dest_files");

	ClassDB::bind_method(D_METHOD("get_additional_sources"), &ImportInfo::get_additional_sources);
	ClassDB::bind_method(D_METHOD("set_additional_sources", "additional_sources"), &ImportInfo::set_additional_sources);
	ADD_PROPERTY(PropertyInfo(Variant::PACKED_STRING_ARRAY, "additional_sources"), "set_additional_sources", "get_additional_sources");

	ClassDB::bind_method(D_METHOD("get_metadata_prop"), &ImportInfo::get_metadata_prop);
	ClassDB::bind_method(D_METHOD("set_metadata_prop", "metadata_prop"), &ImportInfo::set_metadata_prop);
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "metadata_prop"), "set_metadata_prop", "get_metadata_prop");

	ClassDB::bind_method(D_METHOD("get_param", "key"), &ImportInfo::get_param);
	ClassDB::bind_method(D_METHOD("set_param", "key", "value"), &ImportInfo::set_param);
	ClassDB::bind_method(D_METHOD("has_param", "key"), &ImportInfo::has_param);

	ClassDB::bind_method(D_METHOD("get_iinfo_val"), &ImportInfo::get_iinfo_val);
	ClassDB::bind_method(D_METHOD("set_iinfo_val"), &ImportInfo::set_iinfo_val);

	ClassDB::bind_method(D_METHOD("has_import_data"), &ImportInfo::has_import_data);
	ClassDB::bind_method(D_METHOD("get_params"), &ImportInfo::get_params);
	ClassDB::bind_method(D_METHOD("set_params", "params"), &ImportInfo::set_params);
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "params"), "set_params", "get_params");
	ClassDB::bind_method(D_METHOD("as_text", "full"), &ImportInfo::as_text, DEFVAL(true));

	ClassDB::bind_method(D_METHOD("save_to"), &ImportInfo::save_to);
}

void ImportInfoModern::_bind_methods() {
	// ClassDB::bind_method(D_METHOD("get_type"), &ImportInfoModern::get_type);
	// ClassDB::bind_method(D_METHOD("set_type"), &ImportInfoModern::set_type);
	// ClassDB::bind_method(D_METHOD("get_compat_type"), &ImportInfoModern::get_compat_type);

	// ClassDB::bind_method(D_METHOD("get_importer"), &ImportInfoModern::get_importer);

	// ClassDB::bind_method(D_METHOD("get_source_file"), &ImportInfoModern::get_source_file);
	// ClassDB::bind_method(D_METHOD("set_source_file"), &ImportInfoModern::set_source_file);

	// ClassDB::bind_method(D_METHOD("get_source_md5"), &ImportInfoModern::get_source_md5);

	// ClassDB::bind_method(D_METHOD("get_uid"), &ImportInfoModern::get_uid);

	// ClassDB::bind_method(D_METHOD("get_dest_files"), &ImportInfoModern::get_dest_files);
	// ClassDB::bind_method(D_METHOD("set_dest_files"), &ImportInfoModern::set_dest_files);

	// ClassDB::bind_method(D_METHOD("get_metadata_prop"), &ImportInfoModern::get_metadata_prop);
	// ClassDB::bind_method(D_METHOD("set_metadata_prop"), &ImportInfoModern::set_metadata_prop);

	// ClassDB::bind_method(D_METHOD("get_param"), &ImportInfoModern::get_param);
	// ClassDB::bind_method(D_METHOD("set_param"), &ImportInfoModern::set_param);

	// ClassDB::bind_method(D_METHOD("get"), &ImportInfoModern::get);
	// ClassDB::bind_method(D_METHOD("set"), &ImportInfoModern::set);

	// ClassDB::bind_method(D_METHOD("has_import_data"), &ImportInfoModern::has_import_data);
	// ClassDB::bind_method(D_METHOD("get_params"), &ImportInfoModern::get_params);
	// ClassDB::bind_method(D_METHOD("set_params"), &ImportInfoModern::set_params);

	// ClassDB::bind_method(D_METHOD("save_to"), &ImportInfoModern::save_to);
}
void ImportInfov2::_bind_methods() {
	// ClassDB::bind_method(D_METHOD("get_type"), &ImportInfov2::get_type);
	// ClassDB::bind_method(D_METHOD("set_type"), &ImportInfov2::set_type);
	// ClassDB::bind_method(D_METHOD("get_compat_type"), &ImportInfov2::get_compat_type);

	// ClassDB::bind_method(D_METHOD("get_importer"), &ImportInfov2::get_importer);

	// ClassDB::bind_method(D_METHOD("get_source_file"), &ImportInfov2::get_source_file);
	// ClassDB::bind_method(D_METHOD("set_source_file"), &ImportInfov2::set_source_file);

	// ClassDB::bind_method(D_METHOD("get_source_md5"), &ImportInfov2::get_source_md5);

	// ClassDB::bind_method(D_METHOD("get_uid"), &ImportInfov2::get_uid);

	// ClassDB::bind_method(D_METHOD("get_dest_files"), &ImportInfov2::get_dest_files);
	// ClassDB::bind_method(D_METHOD("set_dest_files"), &ImportInfov2::set_dest_files);

	// ClassDB::bind_method(D_METHOD("get_metadata_prop"), &ImportInfov2::get_metadata_prop);
	// ClassDB::bind_method(D_METHOD("set_metadata_prop"), &ImportInfov2::set_metadata_prop);

	// ClassDB::bind_method(D_METHOD("get_param"), &ImportInfov2::get_param);
	// ClassDB::bind_method(D_METHOD("set_param"), &ImportInfov2::set_param);

	// ClassDB::bind_method(D_METHOD("get"), &ImportInfov2::get);
	// ClassDB::bind_method(D_METHOD("set"), &ImportInfov2::set);

	// ClassDB::bind_method(D_METHOD("has_import_data"), &ImportInfov2::has_import_data);
	// ClassDB::bind_method(D_METHOD("get_params"), &ImportInfov2::get_params);
	// ClassDB::bind_method(D_METHOD("set_params"), &ImportInfov2::set_params);

	// ClassDB::bind_method(D_METHOD("save_to"), &ImportInfov2::save_to);
}