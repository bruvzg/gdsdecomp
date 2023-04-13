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
	if (importer == "scene" || importer == "ogg_vorbis" || importer == "mp3" || importer == "wavefront_obj" || auto_converted_export) {
		//These are always imported as either native files or losslessly
		return LOSSLESS;
	}
	if (!is_import()) {
		return UNKNOWN;
	}
	String ext = source_file.get_extension();

	// textures and layered textures
	if (importer.begins_with("texture")) {
		int stat = 0;
		// These are always imported in such a way that it is impossible to recover the original file
		// SVG in particular is converted to raster from vector
		// However, you can convert these and rewrite the imports such that they will be imported losslessly next time
		if (ext == "svg" || ext == "jpg") {
			stat |= IMPORTED_LOSSY;
		}
		bool has_compress_param = params.has("compress/mode") && params["compress/mode"].is_num();
		if (ver_major == 2) { //v2 all textures
			if (params.has("storage") && params["storage"].is_num()) { //v2
				stat |= (int)params["storage"] != V2ImportEnums::Storage::STORAGE_COMPRESS_LOSSY ? LOSSLESS : STORED_LOSSY;
			}
		} else if (importer == "texture" && has_compress_param) { // non-layered textures
			if (ver_major == 4 || ver_major == 3) {
				// COMPRESSED_LOSSLESS or COMPRESS_VRAM_UNCOMPRESSED, same in v3 and v4
				stat |= ((int)params["compress/mode"] == 0 || (int)params["compress/mode"] == 3) ? LOSSLESS : STORED_LOSSY;
			}
		} else if (has_compress_param) { // layered textures
			if (ver_major == 4) {
				// COMPRESSED_LOSSLESS or COMPRESS_VRAM_UNCOMPRESSED
				stat |= ((int)params["compress/mode"] == 0 || (int)params["compress/mode"] == 3) ? LOSSLESS : STORED_LOSSY;
			} else if (ver_major == 3) {
				stat |= ((int)params["compress/mode"] != V3LTexCompressMode::COMPRESS_VIDEO_RAM) ? LOSSLESS : STORED_LOSSY;
			}
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
	r_cf.instantiate();
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
	switch (p_iinfo->iitype) {
		case IInfoType::MODERN:
			r_iinfo = Ref<ImportInfo>(memnew(ImportInfoModern));
			((Ref<ImportInfoModern>)r_iinfo)->src_md5 = ((Ref<ImportInfoModern>)p_iinfo)->src_md5;
			((Ref<ImportInfoModern>)r_iinfo)->cf = copy_config_file(((Ref<ImportInfoModern>)p_iinfo)->cf);
			break;
		case IInfoType::V2:
			r_iinfo = Ref<ImportInfo>(memnew(ImportInfov2));
			((Ref<ImportInfov2>)r_iinfo)->type = ((Ref<ImportInfov2>)p_iinfo)->type;
			((Ref<ImportInfov2>)r_iinfo)->dest_files = ((Ref<ImportInfov2>)p_iinfo)->dest_files;
			((Ref<ImportInfov2>)r_iinfo)->v2metadata = copy_imd_v2(((Ref<ImportInfov2>)p_iinfo)->v2metadata);
			break;
		case IInfoType::DUMMY:
		case IInfoType::REMAP:
			r_iinfo = Ref<ImportInfo>(memnew(ImportInfoDummy));
			((Ref<ImportInfoDummy>)r_iinfo)->type = ((Ref<ImportInfoDummy>)p_iinfo)->type;
			((Ref<ImportInfoDummy>)r_iinfo)->source_file = ((Ref<ImportInfoDummy>)p_iinfo)->source_file;
			((Ref<ImportInfoDummy>)r_iinfo)->src_md5 = ((Ref<ImportInfoDummy>)p_iinfo)->src_md5;
			((Ref<ImportInfoDummy>)r_iinfo)->dest_files = ((Ref<ImportInfoDummy>)p_iinfo)->dest_files;
			break;
		default:
			break;
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

ImportInfo::ImportInfo() :
		RefCounted() {
	import_md_path = "";
	ver_major = 0;
	ver_minor = 0;
	export_dest = "";
	iitype = IInfoType::BASE;
}

ImportInfoModern::ImportInfoModern() :
		ImportInfo() {
	cf.instantiate();
	iitype = IInfoType::MODERN;
}

ImportInfov2::ImportInfov2() :
		ImportInfo() {
	v2metadata.instantiate();
	iitype = IInfoType::V2;
}

ImportInfoDummy::ImportInfoDummy() :
		ImportInfo() {
	iitype = IInfoType::DUMMY;
}

ImportInfoRemap::ImportInfoRemap() :
		ImportInfoDummy() {
	iitype = IInfoType::REMAP;
}

Error ImportInfo::get_resource_info(const String &p_path, _ResourceInfo &res_info) {
	ResourceFormatLoaderCompat rlc;
	ERR_FAIL_COND_V_MSG(!GDRESettings::get_singleton()->has_res_path(p_path), ERR_FILE_NOT_FOUND, "Could not load " + p_path);
	Error err = rlc.get_import_info(p_path, GDRESettings::get_singleton()->get_project_path(), res_info);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not load resource info from " + p_path);
	return OK;
}

Ref<ImportInfo> ImportInfo::load_from_file(const String &p_path, int ver_major, int ver_minor) {
	Ref<ImportInfo> iinfo;
	Error err = OK;
	_ResourceInfo res_info;
	if (p_path.get_extension() == "import") {
		iinfo = Ref<ImportInfo>(memnew(ImportInfoModern));
		err = iinfo->_load(p_path);
		if (ver_major == 0) {
			err = get_resource_info(iinfo->get_path(), res_info);
			if (err) {
				WARN_PRINT("ImportInfo: Version major not specified and could not load binary resource file!");
				err = OK;
			} else {
				iinfo->ver_major = res_info.ver_major;
				iinfo->ver_minor = res_info.ver_minor;
				if (res_info.type != iinfo->get_type()) {
					WARN_PRINT(p_path + ": binary resource type " + res_info.type + " does not equal import type " + iinfo->get_type() + "???");
				}
				if (res_info.is_text) {
					WARN_PRINT_ONCE("ImportInfo: Attempted to load a text resource file, cannot determine minor version!");
				}
			}
		} else {
			iinfo->ver_major = ver_major;
			iinfo->ver_minor = ver_minor;
		}

	} else if (p_path.get_extension() == "remap") {
		// .remap file for an autoconverted export
		iinfo = Ref<ImportInfoRemap>(memnew(ImportInfoRemap));
		err = iinfo->_load(p_path);
	} else if (ver_major >= 3) {
		iinfo = Ref<ImportInfo>(memnew(ImportInfoDummy));
		err = iinfo->_load(p_path);
	} else {
		iinfo = Ref<ImportInfo>(memnew(ImportInfov2));
		err = iinfo->_load(p_path);
	}
	ERR_FAIL_COND_V_MSG(err != OK, Ref<ImportInfo>(), "Could not load " + p_path);
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
	if (!cf->has_section_key("remap", "path")) {
		List<String> remap_keys;
		cf->get_section_keys("remap", &remap_keys);
		// if set, we likely have multiple paths
		if (get_metadata_prop().has("imported_formats")) {
			for (int i = 0; i < p_dest_files.size(); i++) {
				Vector<String> spl = p_dest_files[i].split(".");
				// second to last split
				ERR_FAIL_COND_MSG(spl.size() >= 4, "Expected to see format in path " + p_dest_files[i]);
				String ext = spl[spl.size() - 2];
				List<String>::Element *E = remap_keys.find("path." + ext);

				if (!E) {
					WARN_PRINT("Did not find key path." + ext + " in remap metadata");
					continue;
				}
				cf->set_value("remap", E->get(), p_dest_files[i]);
			}
		} else {
			ERR_FAIL_MSG("we don't have imported_formats in the remap metadata...????");
		}
	} else {
		cf->set_value("remap", "path", p_dest_files[0]);
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
	if (err) {
		cf = Ref<ConfigFile>();
	}
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not load " + path);
	import_md_path = path;
	preferred_import_path = cf->get_value("remap", "path", "");

	bool missing_deps = false;
	Vector<String> dest_files;

	// Godot 4.x started stripping the deps section from the .import file, need to recreate it
	if (!cf->has_section("deps")) {
		missing_deps = true;
		// the source file is the import_md path minus ".import"
		cf->set_value("deps", "source_file", path.substr(0, path.length() - 7));
		if (!preferred_import_path.is_empty()) {
			cf->set_value("deps", "dest_files", Vector<String>({ preferred_import_path }));
		} else {
			// this is a multi-path import, get all the "path.*" key values
			List<String> remap_keys;
			cf->get_section_keys("remap", &remap_keys);
			ERR_FAIL_COND_V_MSG(remap_keys.size() == 0, ERR_FILE_CORRUPT, "Failed to load remap data from " + path);
			// iterate over keys in remap section
			for (auto E = remap_keys.front(); E; E = E->next()) {
				// if we find a path key, we have a match
				if (E->get().begins_with("path.")) {
					auto try_path = cf->get_value("remap", E->get(), "");
					dest_files.append(try_path);
				}
			}
			cf->set_value("deps", "dest_files", dest_files);
		}
	} else {
		dest_files = get_dest_files();
	}

	// "remap.path" does not exist if there are two or more destination files
	if (preferred_import_path.is_empty()) {
		//check destination files
		ERR_FAIL_COND_V_MSG(dest_files.size() == 0, ERR_FILE_CORRUPT, p_path + ": no destination files found in import data");
		for (int i = 0; i < dest_files.size(); i++) {
			if (GDRESettings::get_singleton()->has_res_path(dest_files[i])) {
				preferred_import_path = dest_files[i];
				break;
			}
		}
	}
	// If we fail to find the import path, throw error
	ERR_FAIL_COND_V_MSG(preferred_import_path.is_empty() || get_type().is_empty(), ERR_FILE_CORRUPT, p_path + ": file is corrupt");

	return OK;
}

Error ImportInfoDummy::_load(const String &p_path) {
	_ResourceInfo res_info;
	Error err = get_resource_info(p_path, res_info);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not load resource " + p_path);
	preferred_import_path = p_path;
	source_file = "";
	not_an_import = true;
	ver_major = res_info.ver_major;
	ver_minor = res_info.ver_minor;
	type = res_info.type;
	dest_files = Vector<String>({ p_path });
	import_md_path = "";
	return OK;
}

Error ImportInfoRemap::_load(const String &p_path) {
	Ref<ConfigFile> cf;
	cf.instantiate();
	source_file = p_path.get_basename(); // res://scene.tscn.remap -> res://scene.tscn
	String path = GDRESettings::get_singleton()->get_res_path(p_path);
	Error err = cf->load(path);
	if (err) {
		cf = Ref<ConfigFile>();
	}
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not load " + path);
	List<String> remap_keys;
	cf->get_section_keys("remap", &remap_keys);
	if (remap_keys.size() == 0) {
		ERR_FAIL_V_MSG(ERR_BUG, "Failed to load import data from " + path);
	}
	preferred_import_path = cf->get_value("remap", "path", "");
	_ResourceInfo res_info;
	err = get_resource_info(preferred_import_path, res_info);
	if (err) {
		cf = Ref<ConfigFile>();
	}
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not load " + preferred_import_path);
	type = res_info.type;
	ver_major = res_info.ver_major;
	ver_minor = res_info.ver_minor;
	dest_files = Vector<String>({ preferred_import_path });
	not_an_import = true;
	import_md_path = p_path;
	auto_converted_export = true;
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
	ver_major = res_info.ver_major;
	ver_minor = res_info.ver_minor;
	if (res_info.v2metadata.is_valid()) {
		v2metadata = res_info.v2metadata;
		return OK;
	}
	Vector<String> spl = p_path.get_file().split(".");
	// Otherwise, we dont have any meta data, and we have to guess what it is
	// If this is a "converted" file, then it won't have import metadata, and we expect that
	if (!res_info.auto_converted_export) {
		// The file loaded, but there was no metadata and it was not a ".converted." file
		//WARN_PRINT("Could not load metadata from " + p_path);
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
	if (v2metadata->get_source_count() > 0) {
		return v2metadata->get_source_path(0);
	}
	return "";
}

void ImportInfov2::set_source_file(const String &p_path) {
	set_source_and_md5(p_path, "");
}

void ImportInfov2::set_source_and_md5(const String &path, const String &md5) {
	if (v2metadata->get_source_count() > 0) {
		v2metadata->remove_source(0);
	}
	v2metadata->add_source_at(path, md5, 0);
}

String ImportInfov2::get_source_md5() const {
	if (v2metadata->get_source_count() > 0) {
		return v2metadata->get_source_md5(0);
	}
	return "";
}

void ImportInfov2::set_source_md5(const String &md5) {
	v2metadata->set_source_md5(0, md5);
}

Vector<String> ImportInfov2::get_dest_files() const {
	return dest_files;
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
		if (v2metadata->get_source_count() >= i) {
			v2metadata->remove_source(i);
		}
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

Error ImportInfoModern::save_md5_file(const String &output_dir) {
	String md5_file_path;
	Vector<String> dest_files = get_dest_files();
	if (dest_files.size() == 0) {
		return ERR_PRINTER_ON_FIRE;
	}
	// Only imports under these paths have .md5 files
	if (!dest_files[0].begins_with("res://.godot") && !dest_files[0].begins_with("res://.import")) {
		return ERR_PRINTER_ON_FIRE;
	}
	String actual_source = get_source_file();
	if (export_dest != actual_source) {
		return ERR_PRINTER_ON_FIRE;
	}
	Vector<String> spl = dest_files[0].split("-");
	ERR_FAIL_COND_V_MSG(spl.size() < 2, ERR_FILE_BAD_PATH, "Weird import path!");
	md5_file_path = output_dir.path_join(spl[0].replace_first("res://", "") + "-" + spl[1].get_basename() + ".md5");
	// check if each exists
	for (int i = 0; i < dest_files.size(); i++) {
		if (!FileAccess::exists(dest_files[i])) {
			//WARN_PRINT("Cannot find " + dest_files[i] + ", cannot compute dest_md5.");
			return ERR_PRINTER_ON_FIRE;
		}
	}
	// dest_md5 is the md5 of all the destination files together
	String dst_md5 = FileAccess::get_multiple_md5(dest_files);
	ERR_FAIL_COND_V_MSG(dst_md5.is_empty(), ERR_FILE_BAD_PATH, "Can't open import resources to check md5!");

	if (src_md5.is_empty()) {
		String src_path = export_dest.is_empty() ? get_source_file() : export_dest;
		src_md5 = FileAccess::get_md5(output_dir.path_join(src_path.replace_first("res://", "")));
		ERR_FAIL_COND_V_MSG(src_md5.is_empty(), ERR_FILE_BAD_PATH, "Can't open exported resource to check md5!");
	}
	Ref<FileAccess> md5_file = FileAccess::open(md5_file_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(md5_file.is_null(), ERR_FILE_CANT_OPEN, "Can't open exported resource to check md5!");
	md5_file->store_string("source_md5=\"" + src_md5 + "\"\ndest_md5=\"" + dst_md5 + "\"\n\n");
	md5_file->flush();
	return OK;
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

	ClassDB::bind_method(D_METHOD("get_params"), &ImportInfo::get_params);
	ClassDB::bind_method(D_METHOD("set_params", "params"), &ImportInfo::set_params);
	ADD_PROPERTY(PropertyInfo(Variant::DICTIONARY, "params"), "set_params", "get_params");
	ClassDB::bind_method(D_METHOD("as_text", "full"), &ImportInfo::as_text, DEFVAL(true));

	ClassDB::bind_method(D_METHOD("save_to"), &ImportInfo::save_to);
}

void ImportInfoModern::_bind_methods() {
}
void ImportInfov2::_bind_methods() {
}
