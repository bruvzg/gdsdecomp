#include "resource_loader_compat.h"
#include "core/crypto/crypto_core.h"
#include "core/io/dir_access.h"
#include "core/io/file_access_compressed.h"
#include "core/string/ustring.h"
#include "core/variant/variant_parser.h"
#include "core/version.h"
#include "gdre_packed_data.h"
#include "gdre_settings.h"
#include "image_parser_v2.h"
#include "texture_loader_compat.h"
#include "variant_writer_compat.h"

Error ResourceFormatLoaderCompat::convert_bin_to_txt(const String &p_path, const String &dst, const String &output_dir, float *r_progress) {
	Error error = OK;
	String dst_path = dst;

	//Relative path
	if (!output_dir.is_empty() && GDRESettings::get_singleton()) {
		dst_path = output_dir.plus_file(dst.replace_first("res://", ""));
	}

	ResourceLoaderBinaryCompat *loader = _open(p_path, output_dir, true, &error, r_progress);
	ERR_RFLBC_COND_V_MSG_CLEANUP(error != OK, error, "Cannot open resource '" + p_path + "'.", loader);

	error = loader->load();
	ERR_RFLBC_COND_V_MSG_CLEANUP(error != OK, error, "Cannot load resource '" + p_path + "'.", loader);

	error = loader->save_as_text_unloaded(dst_path);

	memdelete(loader);
	ERR_FAIL_COND_V_MSG(error != OK, error, "failed to save resource '" + p_path + "' as '" + dst + "'.");
	return OK;
}

// This is really only for loading certain resources to view them, and debugging.
// This is not suitable for conversion of resources
RES ResourceFormatLoaderCompat::load(const String &p_path, const String &project_dir, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	Error err = ERR_FILE_CANT_OPEN;
	if (!r_error) {
		r_error = &err;
	} else {
		*r_error = err;
	}
	if (p_path.get_extension() == "tex" || p_path.get_extension() == "stex") {
		TextureLoaderCompat rflct;
		return rflct.load_texture2d(p_path, r_error);
	}
	String local_path = GDRESettings::get_singleton()->localize_path(p_path, project_dir);
	ResourceLoaderBinaryCompat *loader = _open(p_path, project_dir, false, r_error, r_progress);
	ERR_FAIL_COND_V_MSG(*r_error != OK, RES(), "Cannot open file '" + p_path + "'.");
	loader->cache_mode = p_cache_mode;
	loader->use_sub_threads = p_use_sub_threads;
	if (loader->engine_ver_major != VERSION_MAJOR) {
		WARN_PRINT(p_path + ": Doing real load on incompatible version " + itos(loader->engine_ver_major));
	}

	*r_error = loader->load();
	ERR_RFLBC_COND_V_MSG_CLEANUP(*r_error != OK, RES(), "failed to load resource '" + p_path + "'.", loader);

	RES ret = loader->resource;
	memdelete(loader);
	return ret;
}

Error ResourceFormatLoaderCompat::rewrite_v2_import_metadata(const String &p_path, const String &p_dst, Ref<ResourceImportMetadatav2> imd) {
	Error error = OK;
	float prog;
	auto loader = _open(p_path, "", true, &error, &prog);
	ERR_RFLBC_COND_V_MSG_CLEANUP(error != OK, error, "Cannot open resource '" + p_path + "'.", loader);
	ERR_RFLBC_COND_V_MSG_CLEANUP(loader->engine_ver_major != 2, ERR_INVALID_DATA, "Not a V2 Resource: '" + p_path + "'.", loader);
	// In case this is a imageTexture resource, we have to set this
	loader->convert_v2image_indexed = true;
	loader->hacks_for_deprecated_v2img_formats = false;

	error = loader->load();
	ERR_RFLBC_COND_V_MSG_CLEANUP(error != OK, error, "Cannot load resource '" + p_path + "'.", loader);
	loader->imd = imd;
	error = loader->save_to_bin(p_dst);
	memdelete(loader);
	ERR_FAIL_COND_V_MSG(error != OK, error, "Cannot save resource to '" + p_dst + "'.");
	return error;
}

Error ResourceFormatLoaderCompat::get_import_info(const String &p_path, const String &base_dir, Ref<ImportInfo> &i_info) {
	Error error = OK;

	ResourceLoaderBinaryCompat *loader = _open(p_path, base_dir, true, &error, nullptr);
	ERR_RFLBC_COND_V_MSG_CLEANUP(error != OK, error, "failed to open resource '" + p_path + "'.", loader);

	if (i_info == nullptr) {
		i_info.instantiate();
	}

	i_info->import_path = loader->local_path;
	i_info->type = loader->type;
	i_info->ver_major = loader->engine_ver_major;
	i_info->ver_minor = loader->engine_ver_minor;
	if (loader->engine_ver_major == 2) {
		i_info->dest_files.push_back(loader->local_path);
		i_info->import_md_path = loader->res_path;
		//these do not have any metadata info in them
		if (i_info->import_path.find(".converted.") != -1) {
			memdelete(loader);
			return OK;
		}
		error = loader->load_import_metadata();
		ERR_RFLBC_COND_V_MSG_CLEANUP(error != OK, ERR_PRINTER_ON_FIRE, "failed to get metadata for '" + loader->res_path + "'", loader);

		if (loader->imd->get_source_count() > 1) {
			WARN_PRINT("More than one source?!?!");
		}
		ERR_RFLBC_COND_V_MSG_CLEANUP(loader->imd->get_source_count() == 0, ERR_FILE_CORRUPT, "metadata corrupt for '" + loader->res_path + "'", loader);
		i_info->v2metadata = loader->imd;
		i_info->source_file = loader->imd->get_source_path(0);
		i_info->importer = loader->imd->get_editor();
		i_info->params = loader->imd->get_options_as_dictionary();
	}
	memdelete(loader);
	return OK;
}

ResourceLoaderBinaryCompat *ResourceFormatLoaderCompat::_open(const String &p_path, const String &base_dir, bool fake_load, Error *r_error, float *r_progress) {
	Error error = OK;
	if (!r_error) {
		r_error = &error;
	}
	String res_path = GDRESettings::get_singleton()->get_res_path(p_path, base_dir);
	FileAccess *f = nullptr;
	if (res_path != "") {
		f = FileAccess::open(res_path, FileAccess::READ, r_error);
	} else {
		*r_error = ERR_FILE_NOT_FOUND;
		ERR_FAIL_COND_V_MSG(!f, nullptr, "Cannot open file '" + res_path + "'.");
	}
	// TODO: remove this extra check
	ERR_FAIL_COND_V_MSG(!f, nullptr, "Cannot open file '" + res_path + "' (Even after get_res_path() returned a path?).");

	ResourceLoaderBinaryCompat *loader = memnew(ResourceLoaderBinaryCompat);
	loader->project_dir = base_dir;
	loader->progress = r_progress;
	loader->fake_load = fake_load;
	loader->local_path = GDRESettings::get_singleton()->localize_path(p_path, base_dir);
	loader->res_path = res_path;

	*r_error = loader->open(f);

	ERR_FAIL_COND_V_MSG(error != OK, loader, "Cannot open resource '" + p_path + "'.");

	return loader;
}

Error ResourceLoaderBinaryCompat::load_import_metadata() {
	if (!f) {
		return ERR_CANT_ACQUIRE_RESOURCE;
	}
	if (importmd_ofs == 0) {
		return ERR_UNAVAILABLE;
	}
	if (imd.is_null()) {
		imd.instantiate();
	}
	f->seek(importmd_ofs);
	imd->set_editor(get_unicode_string());
	int sc = f->get_32();
	for (int i = 0; i < sc; i++) {
		String src = get_unicode_string();
		String md5 = get_unicode_string();
		imd->add_source(src, md5);
	}
	int pc = f->get_32();

	for (int i = 0; i < pc; i++) {
		String name = get_unicode_string();
		Variant val;
		parse_variant(val);
		imd->set_option(name, val);
	}
	return OK;
}

StringName ResourceLoaderBinaryCompat::_get_string() {
	uint32_t id = f->get_32();
	if (id & 0x80000000) {
		uint32_t len = id & 0x7FFFFFFF;
		if ((int)len > str_buf.size()) {
			str_buf.resize(len);
		}
		if (len == 0) {
			return StringName();
		}
		f->get_buffer((uint8_t *)&str_buf[0], len);
		String s;
		s.parse_utf8(&str_buf[0]);
		return s;
	}

	return string_map[id];
}

Error ResourceLoaderBinaryCompat::open(FileAccess *p_f, bool p_no_resources, bool p_keep_uuid_paths) {
	error = OK;

	f = p_f;
	uint8_t header[4];
	Error r_error;
	f->get_buffer(header, 4);
	if (header[0] == 'R' && header[1] == 'S' && header[2] == 'C' && header[3] == 'C') {
		// Compressed.
		FileAccessCompressed *fac = memnew(FileAccessCompressed);
		r_error = fac->open_after_magic(f);

		if (r_error != OK) {
			memdelete(fac);
			f->close();
			ERR_FAIL_COND_V_MSG(r_error != OK, r_error, "Cannot decompress compressed resource file '" + f->get_path() + "'.");
		}
		f = fac;

	} else if (header[0] != 'R' || header[1] != 'S' || header[2] != 'R' || header[3] != 'C') {
		// Not normal.
		r_error = ERR_FILE_UNRECOGNIZED;
		f->close();
		ERR_FAIL_COND_V_MSG(r_error != OK, r_error, "Unable to recognize  '" + f->get_path() + "'.");
	}

	bool big_endian = f->get_32();
	bool use_real64 = f->get_32();

	f->set_big_endian(big_endian != 0); //read big endian if saved as big endian
	stored_big_endian = big_endian;
	engine_ver_major = f->get_32();
	engine_ver_minor = f->get_32();
	ver_format = f->get_32();
	stored_use_real64 = use_real64;
	print_bl("big endian: " + itos(big_endian));
#ifdef BIG_ENDIAN_ENABLED
	print_bl("endian swap: " + itos(!big_endian));
#else
	print_bl("endian swap: " + itos(big_endian));
#endif
	print_bl("real64: " + itos(use_real64));
	print_bl("major: " + itos(engine_ver_major));
	print_bl("minor: " + itos(engine_ver_minor));
	print_bl("format: " + itos(ver_format));
	ERR_FAIL_COND_V_MSG(engine_ver_major > 4, ERR_FILE_UNRECOGNIZED,
			"Unsupported engine version " + itos(engine_ver_major) + " used to create resource '" + res_path + "'.");
	ERR_FAIL_COND_V_MSG(ver_format > VariantBin::FORMAT_VERSION, ERR_FILE_UNRECOGNIZED,
			"Unsupported binary resource format '" + res_path + "'.");

	type = get_unicode_string();

	print_bl("type: " + type);

	importmd_ofs = f->get_64();
	uint32_t flags = f->get_32();
	if (flags & ResourceFormatSaverBinaryInstance::FORMAT_FLAG_NAMED_SCENE_IDS) {
		using_named_scene_ids = true;
	}

	if (flags & ResourceFormatSaverBinaryInstance::FORMAT_FLAG_UIDS) {
		using_uids = true;
		uid = f->get_64();
	} else {
		// skip over uid field
		f->get_64();
		uid = ResourceUID::INVALID_ID;
	}

	for (int i = 0; i < ResourceFormatSaverBinaryInstance::RESERVED_FIELDS; i++) {
		f->get_32(); //skip a few reserved fields
	}

	uint32_t string_table_size = f->get_32();
	string_map.resize(string_table_size);
	for (uint32_t i = 0; i < string_table_size; i++) {
		StringName s = get_unicode_string();
		string_map.write[i] = s;
	}

	print_bl("strings: " + itos(string_table_size));

	uint32_t ext_resources_size = f->get_32();
	for (uint32_t i = 0; i < ext_resources_size; i++) {
		ExtResource er;
		er.type = get_unicode_string();
		er.path = get_unicode_string();

		if (using_uids) {
			er.uid = f->get_64();
			if (!p_keep_uuid_paths && er.uid != ResourceUID::INVALID_ID) {
				if (ResourceUID::get_singleton()->has_id(er.uid)) {
					// If a UID is found and the path is valid, it will be used, otherwise, it falls back to the path.
					er.path = ResourceUID::get_singleton()->get_id_path(er.uid);
				} else {
					WARN_PRINT(String(res_path + ": In external resource #" + itos(i) + ", invalid UUID: " + ResourceUID::get_singleton()->id_to_text(er.uid) + " - using text path instead: " + er.path).utf8().get_data());
				}
			}
		}
		external_resources.push_back(er);
	}

	print_bl("ext resources: " + itos(ext_resources_size));
	uint32_t int_resources_size = f->get_32();

	for (uint32_t i = 0; i < int_resources_size; i++) {
		IntResource ir;
		ir.path = get_unicode_string();
		ir.offset = f->get_64();
		internal_resources.push_back(ir);
	}

	print_bl("int resources: " + itos(int_resources_size));

	if (f->eof_reached()) {
		error = ERR_FILE_CORRUPT;
		f->close();
		ERR_FAIL_V_MSG(error, "Premature end of file (EOF): " + local_path + ".");
	}

	return OK;
}

void ResourceLoaderBinaryCompat::debug_print_properties(String res_name, String res_type, List<ResourceProperty> lrp) {
	String valstring;
	print_bl("Resource name: " + res_name);
	print_bl("type: " + res_type);
	for (List<ResourceProperty>::Element *PE = lrp.front(); PE; PE = PE->next()) {
		ResourceProperty pe = PE->get();
		String vars;
		VariantWriterCompat::write_to_string(pe.value, vars, engine_ver_major, _write_rlc_resources, this);
		Vector<String> vstrs = vars.split("\n");
		for (int i = 0; i < vstrs.size(); i++) {
			print_bl(vstrs[i]);
		}
	}
}

// By default we do not load the external resources, we just load a "dummy" resource that has the path and the type.
// This is so we can convert the currently loading resource to binary/text without potentially loading an
// unavailable and/or incompatible resource
Error ResourceLoaderBinaryCompat::load_ext_resource(const uint32_t i) {
	if (fake_load) {
		set_dummy_ext(i);
		return OK;
	}
	// If this is a real load...
	ResourceFormatLoaderCompat rl;
	Error err;
	// We never load scripts, because they are likely incompatible and it's a security issue
	if (external_resources[i].type.find("Script") != -1) {
		ERR_FAIL_COND_V_MSG(!no_abort_on_ext_load_fail, ERR_CANT_ACQUIRE_RESOURCE, "Refusing to load external script " + external_resources[i].path);
		set_dummy_ext(i);
		return OK;
	}

	external_resources.write[i].cache = rl.load(external_resources[i].path, project_dir, &err);
	if (external_resources.write[i].cache.is_null()) {
		if (!no_abort_on_ext_load_fail) {
			ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to load external resource " + external_resources[i].path);
		} else {
			if (external_resources.write[i].cache.is_null()) {
				// just do a dummy resource and hope this works anyway
				set_dummy_ext(i);
			}
		}
	}

	return OK;
}

RES ResourceLoaderBinaryCompat::get_external_resource(const int subindex) {
	if (external_resources[subindex - 1].cache.is_valid()) {
		return external_resources[subindex - 1].cache;
	}
	// We don't do multithreading, so if this external resource is not cached (either dummy or real)
	// then we return a blank resource
	return RES();
}

RES ResourceLoaderBinaryCompat::get_external_resource(const String &path) {
	for (int i = 0; i < external_resources.size(); i++) {
		if (external_resources[i].path == path) {
			return external_resources[i].cache;
		}
	}
	// We don't do multithreading, so if this external resource is not cached (either dummy or real)
	// then we return a blank resource
	return RES();
}

bool ResourceLoaderBinaryCompat::has_external_resource(const String &path) {
	for (int i = 0; i < external_resources.size(); i++) {
		if (external_resources[i].path == path) {
			return true;
		}
	}
	return false;
}

// by default, we don't instance an internal resource.
// This is done for compatibility reasons. Class names and constructors, and their properties, have changed between versions.
// So we instead just make a dummy resource and keep a list of properties, which would all be Variants
RES ResourceLoaderBinaryCompat::instance_internal_resource(const String &path, const String &type, const String &id) {
	RES res;
	// if this is a fake load, create a dummy instead
	if (fake_load) {
		res = make_dummy(path, type, id);
		return res;
	}
	// If this is a real load...

	// TODO: rename godot v2 resources to godot v3 resources
	// we don't populate the cache by default, so we likely won't hit this (?)
	if (cache_mode == ResourceFormatLoader::CACHE_MODE_REPLACE && ResourceCache::has(path)) {
		//use the existing one
		Resource *r = ResourceCache::get(path);
		if (r->get_class() == type) {
			r->reset_state();
			res = Ref<Resource>(r);
		}
	}

	if (res.is_null()) {
		//did not replace
		Object *obj = ClassDB::instantiate(type);
		if (!obj) {
			error = ERR_FILE_CORRUPT;
			ERR_FAIL_V_MSG(RES(), local_path + ":Resource of unrecognized type in file: " + type + ".");
		}

		Resource *r = Object::cast_to<Resource>(obj);
		if (!r) {
			String obj_class = obj->get_class();
			error = ERR_FILE_CORRUPT;
			memdelete(obj); //bye
			ERR_FAIL_V_MSG(RES(), local_path + ":Resource type in resource field not a resource, type is: " + obj_class + ".");
		}

		if (path != String() && cache_mode != ResourceFormatLoader::CACHE_MODE_IGNORE) {
			r->set_path(path, cache_mode == ResourceFormatLoader::CACHE_MODE_REPLACE); //if got here because the resource with same path has different type, replace it
		}
		r->set_scene_unique_id(id);
		res = RES(r);
	}
	return res;
}

RES ResourceLoaderBinaryCompat::get_internal_resource(const int subindex) {
	for (auto R = internal_index_cache.front(); R; R = R->next()) {
		if (R->value()->get_scene_unique_id() == itos(subindex)) {
			return R->value();
		}
	}
	return RES();
}

RES ResourceLoaderBinaryCompat::get_internal_resource(const String &path) {
	if (has_internal_resource(path)) {
		return internal_index_cache[path];
	}
	return RES();
}

String ResourceLoaderBinaryCompat::get_internal_resource_type(const String &path) {
	if (has_internal_resource(path)) {
		return internal_type_cache[path];
	}
	return "None";
}

List<ResourceProperty> ResourceLoaderBinaryCompat::get_internal_resource_properties(const String &path) {
	if (has_internal_resource(path)) {
		return internal_index_cached_properties[path];
	}
	return List<ResourceProperty>();
}

bool ResourceLoaderBinaryCompat::has_internal_resource(const String &path) {
	return internal_index_cache.has(path);
}

String ResourceLoaderBinaryCompat::get_resource_path(const RES &res) {
	if (res.is_null()) {
		return "";
	}
	if (res->is_class("FakeResource")) {
		return ((Ref<FakeResource>)res)->get_real_path();
	} else if (res->is_class("FakeScript")) {
		return ((Ref<FakeScript>)res)->get_real_path();
	} else {
		return res->get_path();
	}
}

bool check_old_type_name(Variant::Type type, int ver_major, String &old_name) {
	if (4 == VERSION_MAJOR) {
		return true;
	}
	String new_name = Variant::get_type_name(type);
	if (new_name.begins_with("Packed")) {
		old_name = new_name.replace_first("Packed", "");
		return false;
	}
	if (ver_major == 2) {
		if (new_name == "Transform2D") {
			old_name = "Matrix32";
			return false;
		}
		if (new_name == "Basis") {
			old_name = "Matrix3";
			return false;
		}
	}
	return true;
}

void print_class_prop_list(const StringName &cltype) {
	List<PropertyInfo> *listpinfo = memnew(List<PropertyInfo>);

	ClassDB::get_property_list(StringName(cltype), listpinfo);
	if (!listpinfo || listpinfo->size() == 0) {
		if (listpinfo) {
			memdelete(listpinfo);
		}
		ERR_FAIL_COND_MSG((!listpinfo || listpinfo->size() == 0), "No properties in class of type " + cltype + " ?");
	}
	print_line("List of valid properties in class" + cltype + " : \n[");
	for (auto I = listpinfo->front(); I; I = I->next()) {
		auto p = I->get();
		print_line("\t{");
		print_line("\t\tname:        " + p.name);
		print_line("\t\ttype:        " + itos(p.type));
		print_line("\t\thint_string: " + p.hint_string);
		print_line("\t}");
	}
	print_line("]");
	memdelete(listpinfo);
}

/*
 * Convert properties of old stored resources to new resource properties
 *
 * Primarily for debugging; as development continues, we'll likely have special handlers
 * for old resources we care about.
 *
 * Common failures like properties that no longer exist, or changed type (like String to StringName),
 * can be fixed. Otherwise we return an error so that we stop attempting to load the resource.
 *
 * This is a recursive function; if the property value is a resource,
 * we iterate through all ITS properties and attempt repairs.
 */
Error ResourceLoaderBinaryCompat::repair_property(const String &rtype, const StringName &name, Variant &value) {
	bool class_exists = ClassDB::class_exists(StringName(rtype));
	ERR_FAIL_COND_V_MSG(!class_exists, ERR_UNAVAILABLE, "Class does not exist in ClassDB");

	PropertyInfo *pinfo = memnew(PropertyInfo);
	Error err = OK;
	bool has_property = ClassDB::get_property_info(StringName(rtype), name, pinfo);

	// If it just doesn't have the property, this isn't fatal
	if (!has_property) {
		// handle this below
		memdelete(pinfo);
		return ERR_CANT_RESOLVE;
	} else {
		List<PropertyInfo> *listpinfo = memnew(List<PropertyInfo>);
#define ERR_EXIT_REPAIR_PROPERTY(err, errmsg) \
	if (pinfo)                                \
		memdelete(pinfo);                     \
	if (listpinfo)                            \
		memdelete(listpinfo);                 \
	ERR_PRINT(errmsg);                        \
	print_class_prop_list(rtype);             \
	return err;

		// Check that the property type matches the one we have
		String class_prop_type = Variant::get_type_name(pinfo->type);
		String value_type = Variant::get_type_name(value.get_type());

		// If type values don't match
		if (pinfo->type != value.get_type()) {
			//easy fixes for String/StringName swaps
			if (pinfo->type == Variant::STRING_NAME && value.get_type() == Variant::STRING) {
				value = StringName(value);
			} else if (pinfo->type == Variant::STRING && value.get_type() == Variant::STRING_NAME) {
				value = String(value);

				// Arrays of different bitsize
			} else if (pinfo->type == Variant::PACKED_INT64_ARRAY && value.get_type() == Variant::PACKED_INT32_ARRAY) {
				PackedInt64Array arr;
				PackedInt32Array srcarr = value;
				for (int i = 0; i < srcarr.size(); i++) {
					arr.push_back(srcarr[i]);
				}
				value = srcarr;
				//TODO: rest of 32/64 byte type diffs

				// Convert old vector floats into vector integers
			} else if (pinfo->type == Variant::VECTOR2I && value.get_type() == Variant::VECTOR2) {
				Vector2i ret;
				Vector2 val = value;
				ret.x = val.x;
				ret.y = val.y;
			} else if (pinfo->type == Variant::VECTOR3I && value.get_type() == Variant::VECTOR3) {
				Vector3i ret;
				Vector3 val = value;
				ret.x = val.x;
				ret.y = val.y;
				ret.z = val.z;
			} else {
				ERR_EXIT_REPAIR_PROPERTY(ERR_FILE_UNRECOGNIZED, "Property " + name + " type does not match!" + "\n\tclass prop type:          " + class_prop_type + "\n\tattempted set value type: " + value_type);
			}

			// TODO: implement these
		} else if (pinfo->type == Variant::ARRAY) {
			WARN_PRINT("Property '" + name + "' type is a array (?)");
			print_class_prop_list(StringName(rtype));
			err = ERR_PRINTER_ON_FIRE;
		} else if (pinfo->type == Variant::DICTIONARY) {
			WARN_PRINT("Property '" + name + "' type is a dictionary (?)");
			print_class_prop_list(StringName(rtype));
			err = ERR_PRINTER_ON_FIRE;

			// If this is an object, we have to load the cached resource and walk the property list
		} else if (pinfo->type == Variant::OBJECT) {
			//let us hope that this is the name of the object class
			String hint = pinfo->class_name;
			//String hint = pinfo->hint_string;
			if (hint.is_empty()) {
				ERR_EXIT_REPAIR_PROPERTY(ERR_FILE_UNRECOGNIZED, "Possible type difference in property " + name + "\nclass property type is of an unknown object, attempted set value type is " + value_type);
			}
			ClassDB::get_property_list(StringName(hint), listpinfo);
			if (!listpinfo || listpinfo->size() == 0) {
				ERR_EXIT_REPAIR_PROPERTY(ERR_FILE_CANT_OPEN, "Failed to get list of properties for property " + name + " of object type " + hint);
			}
			RES res = value;
			if (res.is_null()) {
				ERR_EXIT_REPAIR_PROPERTY(ERR_FILE_MISSING_DEPENDENCIES, "Property " + name + " is not a resource of type " + hint);
			}
			String path = get_resource_path(res);
			StringName res_type = res->get_class_name();

			// If it's not a converted v2 image, AND we don't have the resource cached...
			if (String(res_type) != "Image" && !has_external_resource(path) && !has_internal_resource(path)) {
				ERR_EXIT_REPAIR_PROPERTY(ERR_CANT_ACQUIRE_RESOURCE, "We do not have resource " + path + " cached!");
			}

			// Can't handle classes of different types
			if (hint != res_type) {
				ERR_EXIT_REPAIR_PROPERTY(ERR_FILE_UNRECOGNIZED, "Property " + name + " type does not match!" + "\n\tclass prop type:          " + hint + "\n\tattempted set value type: " + res_type);
			}
			// Walk the property list
			for (auto I = listpinfo->front(); I; I->next()) {
				auto spinfo = I->get();
				bool valid;
				Variant val_value = res->get(spinfo.name, &valid);
				// doesn't have the property, which is often the case because the editor
				// puts a bunch of garbage properties that never get set when saved
				// just continue
				if (!valid || !val_value) {
					continue;
				}
				err = repair_property(res_type, StringName(spinfo.name), val_value);
				// Worst error was that the value had an extra property, we continue on
				if (err == ERR_CANT_RESOLVE) {
					WARN_PRINT("Property " + name + " value of type " + res_type + " had an extra property in " + spinfo.name);
					continue;
				} else if (err != OK) {
					//otherwise, fail
					ERR_EXIT_REPAIR_PROPERTY(err, "lol i dunno");
				}
			}
		} else {
			// Unknown why this failed, just report it
			ERR_EXIT_REPAIR_PROPERTY(ERR_BUG, "Property '" + name + "' failed to be set for some unknown reason");
		}
		memdelete(listpinfo);
	}
	memdelete(pinfo);
	return err;
}

Error ResourceLoaderBinaryCompat::load() {
	if (error != OK) {
		return error;
	}

	int stage = 0;
	Vector<String> lines;
	for (int i = 0; i < external_resources.size(); i++) {
		error = load_ext_resource(i);
		ERR_FAIL_COND_V_MSG(error != OK, error, "Can't load external resource " + external_resources[i].path);
		stage++;
	}

	// On a fake load, we don't instance the internal resources here.
	// We instead store the name, type and properties
	for (int i = 0; i < internal_resources.size(); i++) {
		bool main = i == (internal_resources.size() - 1);

		String path;
		String id;

		if (!main) {
			path = internal_resources[i].path;
			if (path.begins_with("local://")) {
				path = path.replace_first("local://", "");
				id = path;
				path = local_path + "::" + path;
			}
		} else {
			path = local_path;
		}
		internal_resources.write[i].path = path;

		uint64_t offset = internal_resources[i].offset;
		f->seek(offset);

		String rtype = get_unicode_string();
		internal_type_cache[path] = rtype;

		// set properties
		List<ResourceProperty> lrp;

		// if this a fake load, this is a dummy
		// if it's a real load, it's an instance of the resource class
		RES res = instance_internal_resource(path, rtype, id);
		ERR_FAIL_COND_V_MSG(res.is_null(), ERR_CANT_ACQUIRE_RESOURCE, "Can't load internal resource " + path);
		int pc = f->get_32();

		// Now we iterate though all the properties of this resource
		for (int j = 0; j < pc; j++) {
			StringName name = _get_string();

			if (name == StringName()) {
				error = ERR_FILE_CORRUPT;
				ERR_FAIL_V(ERR_FILE_CORRUPT);
			}

			Variant value;

			error = parse_variant(value);
			if (error) {
				return error;
			}
			ResourceProperty rp;
			rp.name = name;
			rp.value = value;
			rp.type = value.get_type();
			lrp.push_back(rp);
			// If not a fake_load, set the properties of the instanced resource
			if (!fake_load) {
				// TODO: Translate V2 resource names into V3 resource names
				bool valid;
				res->set(name, value, &valid);
				if (!valid) {
					Error rerr = repair_property(rtype, name, value);
					// Worst case was an extra property, we can continue on
					if (rerr == ERR_CANT_RESOLVE) {
						WARN_PRINT("Got error trying to set extra property " + name + " on " + type + " resource " + path);
					} else {
						ERR_FAIL_COND_V_MSG(rerr != OK, ERR_CANT_ACQUIRE_RESOURCE, "Can't load internal resource " + path);
						// Otherwise, property repair was successful
						res->set(name, value, &valid);
						// Dammit...
						ERR_FAIL_COND_V_MSG(!valid, ERR_CANT_ACQUIRE_RESOURCE, "Can't load resource, property repair failed for " + path);
					}
				}
			}
		}
		// We keep a list of the properties loaded (which are only variants) in case of a fake load
		internal_index_cached_properties[path] = lrp;
		// If fake_load, this is a FakeResource
		internal_index_cache[path] = res;

		// packed scenes with instances for nodes won't work right without creating an instance of it
		// So we always instance them regardless if this is a fake load or not.
		if (main && fake_load && type == "PackedScene") {
			Ref<PackedScene> ps;
			ps.instantiate();
			String valstring;
			// this is the "_embedded" prop
			ps->set(lrp.front()->get().name, lrp.front()->get().value);
			resource = ps;
		} else if (main) {
			resource = res;
		}

		stage++;

		if (progress) {
			*progress = (i + 1) / float(internal_resources.size());
		}

		if (main) {
			// Get the import metadata, if we're able to
			if (engine_ver_major == 2) {
				Error limperr = load_import_metadata();
				// if this was an error other than the metadata being unavailable...
				if (limperr != OK && limperr != ERR_UNAVAILABLE) {
					error = limperr;
					ERR_FAIL_V_MSG(error, "Failed to load");
				}
			}
			error = OK;
			return OK;
		}
	}
	//If we got here, we never loaded the main resource
	return ERR_FILE_EOF;
}

String ResourceLoaderBinaryCompat::get_ustring(FileAccess *f) {
	int len = f->get_32();
	Vector<char> str_buf;
	if (len == 0) {
		return String();
	}
	str_buf.resize(len);
	f->get_buffer((uint8_t *)&str_buf[0], len);
	String s;
	s.parse_utf8(&str_buf[0]);
	return s;
}

String ResourceLoaderBinaryCompat::get_unicode_string() {
	return get_ustring(f);
}

RES ResourceLoaderBinaryCompat::make_dummy(const String &path, const String &type, const String &id) {
	Ref<FakeResource> dummy;
	dummy.instantiate();
	dummy->set_real_path(path);
	dummy->set_real_type(type);
	dummy->set_scene_unique_id(id);
	return dummy;
}

RES ResourceLoaderBinaryCompat::set_dummy_ext(const uint32_t erindex) {
	if (external_resources[erindex].cache.is_valid()) {
		return external_resources[erindex].cache;
	}
	String id;
	if (using_uids) {
		id = itos(external_resources[erindex].uid);
	} else {
		id = itos(erindex + 1);
	}
	RES dummy = make_dummy(external_resources[erindex].path, external_resources[erindex].type, id);
	external_resources.write[erindex].cache = dummy;

	return dummy;
}

RES ResourceLoaderBinaryCompat::set_dummy_ext(const String &path, const String &exttype) {
	for (int i = 0; i < external_resources.size(); i++) {
		if (external_resources[i].path == path) {
			if (external_resources[i].cache.is_valid()) {
				return external_resources[i].cache;
			}
			return set_dummy_ext(i);
		}
	}
	//If not found in cache...
	WARN_PRINT("External resource not found in cache???? Making dummy anyway...");
	ExtResource er;
	er.path = path;
	er.type = exttype;
	er.cache = make_dummy(path, exttype, itos(external_resources.size() + 1));
	external_resources.push_back(er);

	return er.cache;
}

void ResourceLoaderBinaryCompat::advance_padding(FileAccess *f, uint32_t p_len) {
	uint32_t extra = 4 - (p_len % 4);
	if (extra < 4) {
		for (uint32_t i = 0; i < extra; i++) {
			f->get_8(); //pad to 32
		}
	}
}

void ResourceLoaderBinaryCompat::_advance_padding(uint32_t p_len) {
	advance_padding(f, p_len);
}

String ResourceLoaderBinaryCompat::_write_rlc_resources(void *ud, const RES &p_resource) {
	ResourceLoaderBinaryCompat *rsi = (ResourceLoaderBinaryCompat *)ud;
	return rsi->_write_rlc_resource(p_resource);
}

String ResourceLoaderBinaryCompat::_write_rlc_resource(const RES &res) {
	String path = get_resource_path(res);
	String id = res->get_scene_unique_id();
	// Godot 4.x ids are strings, Godot 3.x are integers
	if (engine_ver_major >= 4) {
		id = "\"" + id + "\"";
	}
	if (has_internal_resource(path)) {
		return "SubResource( " + id + " )";
	} else if (has_external_resource(path)) {
		return "ExtResource( " + id + " )";
	}
	ERR_FAIL_V_MSG("null", "Resource was not pre cached for the resource section, bug?");
}

Error ResourceLoaderBinaryCompat::save_as_text_unloaded(const String &dest_path, uint32_t p_flags) {
	bool is_scene = false;
	// main resource
	RES res = resource;
	Ref<PackedScene> packed_scene;
	if (dest_path.ends_with(".tscn") || dest_path.ends_with(".escn")) {
		is_scene = true;
		packed_scene = resource;
	}

	Error err;
	FileAccess *wf = FileAccess::open(dest_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err != OK, ERR_CANT_OPEN, "Cannot save file '" + dest_path + "'.");

	String main_res_path = get_resource_path(res);
	if (main_res_path == "") {
		main_res_path = local_path;
	}
	// the actual type in case this is a fake resource
	String main_type = get_internal_resource_type(main_res_path);

	// Version 1 (Godot 2.x)
	// Version 2 (Godot 3.x): changed names for Basis, AABB, Vectors, etc.
	// Version 3 (Godot 4.x): new string ID for ext/subresources, breaks forward compat.
	int text_format_version = 1;
	if (engine_ver_major == 3) {
		text_format_version = 2;
	} else if (engine_ver_major == 4) {
		text_format_version = 3;
	}

	// save resources
	{
		String title = is_scene ? "[gd_scene " : "[gd_resource ";
		if (!is_scene) {
			title += "type=\"" + main_type + "\" ";
		}
		int load_steps = internal_resources.size() + external_resources.size();

		if (load_steps > 1) {
			title += "load_steps=" + itos(load_steps) + " ";
		}
		title += "format=" + itos(text_format_version) + "";
		// if v3 (Godot 4.x), store uid

		if (text_format_version >= 3) {
			if (uid == ResourceUID::INVALID_ID) {
				uid = ResourceSaver::get_resource_id_for_path(local_path, true);
			}
			if (uid != ResourceUID::INVALID_ID) {
				title += " uid=\"" + ResourceUID::get_singleton()->id_to_text(uid) + "\"";
			}
		}
		wf->store_string(title);
		wf->store_line("]\n"); //one empty line
	}

	for (int i = 0; i < external_resources.size(); i++) {
		String p = external_resources[i].path;
		// Godot 4.x: store uid tag
		if (text_format_version >= 3) {
			String s = "[ext_resource type=\"" + external_resources[i].type + "\"";
			ResourceUID::ID er_uid = external_resources[i].uid;

			if (er_uid == ResourceUID::INVALID_ID) {
				er_uid = ResourceSaver::get_resource_id_for_path(p, false);
			}
			if (er_uid != ResourceUID::INVALID_ID) {
				s += " uid=\"" + ResourceUID::get_singleton()->id_to_text(er_uid) + "\"";
			}
			// id is a string in Godot 4.x
			s += " path=\"" + p + "\" id=\"" + itos(i + 1) + "\"]\n";
			wf->store_string(s); // Bundled.

			// Godot 3.x (and below)
		} else {
			wf->store_string("[ext_resource path=\"" + p + "\" type=\"" + external_resources[i].type +
					"\" id=" + external_resources[i].cache->get_scene_unique_id() + "]\n"); //bundled
		}
	}

	if (external_resources.size()) {
		wf->store_line(String()); //separate
	}
	Set<String> used_unique_ids;
	// Godot 4.x: Get all the unique ids for lookup
	if (text_format_version >= 3) {
		for (int i = 0; i < internal_resources.size(); i++) {
			RES intres = get_internal_resource(internal_resources[i].path);
			if (i != internal_resources.size() - 1 && (res->get_path() == "" || res->get_path().find("::") != -1)) {
				if (intres->get_scene_unique_id() != "") {
					if (used_unique_ids.has(intres->get_scene_unique_id())) {
						intres->set_scene_unique_id(""); // Repeated.
					} else {
						used_unique_ids.insert(intres->get_scene_unique_id());
					}
				}
			}
		}
	}

	for (int i = 0; i < internal_resources.size(); i++) {
		String path = internal_resources[i].path;
		RES intres = get_internal_resource(path);
		bool main = i == (internal_resources.size() - 1);

		if (main && is_scene) {
			break; //save as a scene
		}

		if (main) {
			wf->store_line("[resource]");
		} else {
			String line = "[sub_resource ";
			String type = get_internal_resource_type(path);
			String id = intres->get_scene_unique_id();

			line += "type=\"" + type + "\" ";
			// Godot 4.x
			if (text_format_version >= 3) {
				//if unique id == "", generate and then store
				if (id == "") {
					String new_id;
					while (true) {
						new_id = type + "_" + Resource::generate_scene_unique_id();

						if (!used_unique_ids.has(new_id)) {
							break;
						}
					}
					intres->set_scene_unique_id(new_id);
					used_unique_ids.insert(new_id);
					id = new_id;
				}
				// id is a string in Godot 4.x
				line += "id=\"" + id + "\"]";
				// For Godot 3.x and lower resources, the unique id will just be the numerical index
			} else {
				line += "id=" + id + "]";
			}

			if (text_format_version == 1) {
				// Godot 2.x quirk: newline between subresource and properties
				line += "\n";
			}
			wf->store_line(line);
		}

		List<ResourceProperty> properties = internal_index_cached_properties[path];
		for (List<ResourceProperty>::Element *PE = properties.front(); PE; PE = PE->next()) {
			ResourceProperty pe = PE->get();
			String vars;
			VariantWriterCompat::write_to_string(pe.value, vars, engine_ver_major, _write_rlc_resources, this);
			wf->store_string(pe.name.property_name_encode() + " = " + vars + "\n");
		}

		if (i < internal_resources.size() - 1) {
			wf->store_line(String());
		}
	}

	//if this is a scene, save nodes and connections!
	if (is_scene) {
		Ref<SceneState> state = packed_scene->get_state();
		ERR_FAIL_COND_V_MSG(!state.is_valid(), ERR_FILE_CORRUPT, "Packed scene is corrupt!");
		for (int i = 0; i < state->get_node_count(); i++) {
			StringName type = state->get_node_type(i);
			StringName name = state->get_node_name(i);
			int index = state->get_node_index(i);
			NodePath path = state->get_node_path(i, true);
			NodePath owner = state->get_node_owner_path(i);
			RES instance = state->get_node_instance(i);

			String instance_placeholder = state->get_node_instance_placeholder(i);
			Vector<StringName> groups = state->get_node_groups(i);

			String header = "[node";
			header += " name=\"" + String(name).c_escape() + "\"";
			if (type != StringName()) {
				header += " type=\"" + String(type) + "\"";
			}
			if (path != NodePath()) {
				header += " parent=\"" + String(path.simplified()).c_escape() + "\"";
			}
			if (owner != NodePath() && owner != NodePath(".")) {
				header += " owner=\"" + String(owner.simplified()).c_escape() + "\"";
			}
			if (index >= 0) {
				header += " index=\"" + itos(index) + "\"";
			}

			if (groups.size()) {
				groups.sort_custom<StringName::AlphCompare>();
				String sgroups = " groups=[\n";
				for (int j = 0; j < groups.size(); j++) {
					sgroups += "\"" + String(groups[j]).c_escape() + "\",\n";
				}
				sgroups += "]";
				header += sgroups;
			}

			wf->store_string(header);

			if (instance_placeholder != String()) {
				String vars;
				wf->store_string(" instance_placeholder=");
				VariantWriterCompat::write_to_string(instance_placeholder, vars, engine_ver_major, _write_rlc_resources, this);
				wf->store_string(vars);
			}

			if (instance.is_valid()) {
				String vars;
				wf->store_string(" instance=");
				VariantWriterCompat::write_to_string(instance, vars, engine_ver_major, _write_rlc_resources, this);
				wf->store_string(vars);
			}

			wf->store_line("]");
			if (text_format_version == 1 && state->get_node_property_count(i) != 0) {
				// Godot 2.x quirk: newline between header and properties
				// We're emulating these whitespace quirks to enable easy diffs for regression testing
				wf->store_line("");
			}

			for (int j = 0; j < state->get_node_property_count(i); j++) {
				String vars;
				VariantWriterCompat::write_to_string(state->get_node_property_value(i, j), vars, 2, _write_rlc_resources, this);

				wf->store_string(String(state->get_node_property_name(i, j)).property_name_encode() + " = " + vars + "\n");
			}

			if (i < state->get_node_count() - 1) {
				wf->store_line(String());
			}
		}

		for (int i = 0; i < state->get_connection_count(); i++) {
			if (i == 0) {
				wf->store_line("");
			}

			String connstr = "[connection";
			connstr += " signal=\"" + String(state->get_connection_signal(i)) + "\"";
			connstr += " from=\"" + String(state->get_connection_source(i).simplified()) + "\"";
			connstr += " to=\"" + String(state->get_connection_target(i).simplified()) + "\"";
			connstr += " method=\"" + String(state->get_connection_method(i)) + "\"";
			int flags = state->get_connection_flags(i);
			if (flags != Object::CONNECT_PERSIST) {
				connstr += " flags=" + itos(flags);
			}

			Array binds = state->get_connection_binds(i);
			wf->store_string(connstr);
			if (binds.size()) {
				String vars;
				VariantWriterCompat::write_to_string(binds, vars, engine_ver_major, _write_rlc_resources, this);
				wf->store_string(" binds= " + vars);
			}

			wf->store_line("]");
			if (text_format_version == 1) {
				// Godot 2.x has this particular quirk, don't know why
				wf->store_line("");
			}
		}

		Vector<NodePath> editable_instances = state->get_editable_instances();
		for (int i = 0; i < editable_instances.size(); i++) {
			if (i == 0) {
				wf->store_line("");
			}
			wf->store_line("[editable path=\"" + editable_instances[i].operator String() + "\"]");
		}
	}

	if (wf->get_error() != OK && wf->get_error() != ERR_FILE_EOF) {
		wf->close();
		return ERR_CANT_CREATE;
	}

	wf->close();

	return OK;
}

Error ResourceLoaderBinaryCompat::parse_variant(Variant &r_v) {
	uint32_t type = f->get_32();
	//print_bl("find property of type: %d", type);

	switch (type) {
		case VariantBin::VARIANT_NIL: {
			r_v = Variant();
		} break;
		case VariantBin::VARIANT_BOOL: {
			r_v = bool(f->get_32());
		} break;
		case VariantBin::VARIANT_INT: {
			r_v = int(f->get_32());
		} break;
		case VariantBin::VARIANT_INT64: {
			r_v = int64_t(f->get_64());
		} break;
		case VariantBin::VARIANT_FLOAT: {
			r_v = f->get_real();
		} break;
		case VariantBin::VARIANT_DOUBLE: {
			r_v = f->get_double();
		} break;
		case VariantBin::VARIANT_STRING: {
			r_v = get_unicode_string();
		} break;
		case VariantBin::VARIANT_VECTOR2: {
			Vector2 v;
			v.x = f->get_real();
			v.y = f->get_real();
			r_v = v;

		} break;
		case VariantBin::VARIANT_VECTOR2I: {
			Vector2i v;
			v.x = f->get_32();
			v.y = f->get_32();
			r_v = v;

		} break;
		case VariantBin::VARIANT_RECT2: {
			Rect2 v;
			v.position.x = f->get_real();
			v.position.y = f->get_real();
			v.size.x = f->get_real();
			v.size.y = f->get_real();
			r_v = v;

		} break;
		case VariantBin::VARIANT_RECT2I: {
			Rect2i v;
			v.position.x = f->get_32();
			v.position.y = f->get_32();
			v.size.x = f->get_32();
			v.size.y = f->get_32();
			r_v = v;

		} break;
		case VariantBin::VARIANT_VECTOR3: {
			Vector3 v;
			v.x = f->get_real();
			v.y = f->get_real();
			v.z = f->get_real();
			r_v = v;
		} break;
		case VariantBin::VARIANT_VECTOR3I: {
			Vector3i v;
			v.x = f->get_32();
			v.y = f->get_32();
			v.z = f->get_32();
			r_v = v;
		} break;
		case VariantBin::VARIANT_PLANE: {
			Plane v;
			v.normal.x = f->get_real();
			v.normal.y = f->get_real();
			v.normal.z = f->get_real();
			v.d = f->get_real();
			r_v = v;
		} break;
		case VariantBin::VARIANT_QUAT: {
			Quaternion v;
			v.x = f->get_real();
			v.y = f->get_real();
			v.z = f->get_real();
			v.w = f->get_real();
			r_v = v;

		} break;
		case VariantBin::VARIANT_AABB: {
			AABB v;
			v.position.x = f->get_real();
			v.position.y = f->get_real();
			v.position.z = f->get_real();
			v.size.x = f->get_real();
			v.size.y = f->get_real();
			v.size.z = f->get_real();
			r_v = v;

		} break;
		case VariantBin::VARIANT_MATRIX32: {
			Transform2D v;
			v.elements[0].x = f->get_real();
			v.elements[0].y = f->get_real();
			v.elements[1].x = f->get_real();
			v.elements[1].y = f->get_real();
			v.elements[2].x = f->get_real();
			v.elements[2].y = f->get_real();
			r_v = v;

		} break;
		case VariantBin::VARIANT_MATRIX3: {
			Basis v;
			v.elements[0].x = f->get_real();
			v.elements[0].y = f->get_real();
			v.elements[0].z = f->get_real();
			v.elements[1].x = f->get_real();
			v.elements[1].y = f->get_real();
			v.elements[1].z = f->get_real();
			v.elements[2].x = f->get_real();
			v.elements[2].y = f->get_real();
			v.elements[2].z = f->get_real();
			r_v = v;

		} break;
		case VariantBin::VARIANT_TRANSFORM: {
			Transform3D v;
			v.basis.elements[0].x = f->get_real();
			v.basis.elements[0].y = f->get_real();
			v.basis.elements[0].z = f->get_real();
			v.basis.elements[1].x = f->get_real();
			v.basis.elements[1].y = f->get_real();
			v.basis.elements[1].z = f->get_real();
			v.basis.elements[2].x = f->get_real();
			v.basis.elements[2].y = f->get_real();
			v.basis.elements[2].z = f->get_real();
			v.origin.x = f->get_real();
			v.origin.y = f->get_real();
			v.origin.z = f->get_real();
			r_v = v;
		} break;
		case VariantBin::VARIANT_COLOR: {
			Color v; // Colors should always be in single-precision.
			v.r = f->get_float();
			v.g = f->get_float();
			v.b = f->get_float();
			v.a = f->get_float();
			r_v = v;

		} break;
		case VariantBin::VARIANT_STRING_NAME: {
			r_v = StringName(get_unicode_string());
		} break;
		//Old Godot 2.x Image variant, convert into an object
		case VariantBin::VARIANT_IMAGE: {
			//Have to parse Image here
			if (ImageParserV2::parse_image_v2(f, r_v, hacks_for_deprecated_v2img_formats, convert_v2image_indexed) != OK) {
				ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, "Couldn't load resource: embedded image");
				//WARN_PRINT(String("Couldn't load resource: embedded image").utf8().get_data());
			}
		} break;
		case VariantBin::VARIANT_NODE_PATH: {
			Vector<StringName> names;
			Vector<StringName> subnames;
			bool absolute;

			int name_count = f->get_16();
			uint32_t subname_count = f->get_16();
			absolute = subname_count & 0x8000;
			subname_count &= 0x7FFF;
			bool has_property = ver_format < VariantBin::FORMAT_VERSION_NO_NODEPATH_PROPERTY;
			if (has_property) {
				subname_count += 1; // has a property field, so we should count it as well
			}
			for (int i = 0; i < name_count; i++) {
				names.push_back(_get_string());
			}
			for (uint32_t i = 0; i < subname_count; i++) {
				subnames.push_back(_get_string());
			}
			//empty property field, remove it
			if (has_property && subnames[subnames.size() - 1] == "") {
				subnames.remove_at(subnames.size() - 1);
			}
			NodePath np = NodePath(names, subnames, absolute);

			r_v = np;

		} break;
		case VariantBin::VARIANT_RID: {
			r_v = f->get_32();
		} break;

		case VariantBin::VARIANT_OBJECT: {
			uint32_t objtype = f->get_32();

			switch (objtype) {
				case VariantBin::OBJECT_EMPTY: {
					//do none

				} break;
				case VariantBin::OBJECT_INTERNAL_RESOURCE: {
					uint32_t index = f->get_32();
					//String path = local_path + "::" + itos(index);
					String path;

					if (using_named_scene_ids) { // New format.
						ERR_FAIL_INDEX_V((int)index, internal_resources.size(), ERR_PARSE_ERROR);
						path = internal_resources[index].path;
						r_v = get_internal_resource(path);
					} else {
						path += res_path + "::" + itos(index);
						r_v = get_internal_resource(index);
					}

					if (!r_v) {
						WARN_PRINT(String("Couldn't load internal resource (no cache): Subresource " + itos(index)).utf8().get_data());
						r_v = Variant();
					}
				} break;
				case VariantBin::OBJECT_EXTERNAL_RESOURCE: {
					//old file format, still around for compatibility

					String exttype = get_unicode_string();
					String path = get_unicode_string();

					// don't bother setting it, just try and find it and fail if we can't
					r_v = get_external_resource(path);
					ERR_FAIL_COND_V_MSG(r_v.is_null(), ERR_FILE_MISSING_DEPENDENCIES, "Can't load dependency: " + path + ".");

				} break;
				case VariantBin::OBJECT_EXTERNAL_RESOURCE_INDEX: {
					//new file format, just refers to an index in the external list
					int erindex = f->get_32();
					if (erindex < 0 || erindex >= external_resources.size()) {
						WARN_PRINT("Broken external resource! (index out of size)");
						r_v = Variant();
					} else {
						r_v = get_external_resource(erindex + 1);
					}
					ERR_FAIL_COND_V_MSG(r_v.is_null(), ERR_FILE_MISSING_DEPENDENCIES, "Can't load dependency: " + external_resources[erindex].path + ".");
				} break;
				default: {
					ERR_FAIL_V(ERR_FILE_CORRUPT);
				} break;
			}
		} break;

		// Old Godot 2.x InputEvent variant, should never encounter these
		// They were never saved into the binary resource files.
		case VariantBin::VARIANT_INPUT_EVENT: {
			WARN_PRINT("Encountered a Input event variant?!?!?");
		} break;
		case VariantBin::VARIANT_CALLABLE: {
			r_v = Callable();
		} break;
		case VariantBin::VARIANT_SIGNAL: {
			r_v = Signal();
		} break;
		case VariantBin::VARIANT_DICTIONARY: {
			uint32_t len = f->get_32();
			Dictionary d; //last bit means shared
			len &= 0x7FFFFFFF;
			for (uint32_t i = 0; i < len; i++) {
				Variant key;
				Error err = parse_variant(key);
				ERR_FAIL_COND_V_MSG(err, ERR_FILE_CORRUPT, "Error when trying to parse Variant.");
				Variant value;
				err = parse_variant(value);
				ERR_FAIL_COND_V_MSG(err, ERR_FILE_CORRUPT, "Error when trying to parse Variant.");
				d[key] = value;
			}
			r_v = d;
		} break;
		case VariantBin::VARIANT_ARRAY: {
			uint32_t len = f->get_32();
			Array a; //last bit means shared
			len &= 0x7FFFFFFF;
			a.resize(len);
			for (uint32_t i = 0; i < len; i++) {
				Variant val;
				Error err = parse_variant(val);
				ERR_FAIL_COND_V_MSG(err, ERR_FILE_CORRUPT, "Error when trying to parse Variant.");
				a[i] = val;
			}
			r_v = a;

		} break;
		case VariantBin::VARIANT_RAW_ARRAY: {
			uint32_t len = f->get_32();

			Vector<uint8_t> array;
			array.resize(len);
			uint8_t *w = array.ptrw();
			f->get_buffer(w, len);
			_advance_padding(len);

			r_v = array;

		} break;
		case VariantBin::VARIANT_INT32_ARRAY: {
			uint32_t len = f->get_32();

			Vector<int32_t> array;
			array.resize(len);
			if (len == 0) {
				r_v = array;
				break;
			}
			int32_t *w = array.ptrw();
			f->get_buffer((uint8_t *)w, len * sizeof(int32_t));
#ifdef BIG_ENDIAN_ENABLED
			{
				uint32_t *ptr = (uint32_t *)w.ptr();
				for (int i = 0; i < len; i++) {
					ptr[i] = BSWAP32(ptr[i]);
				}
			}

#endif

			r_v = array;
		} break;
		case VariantBin::VARIANT_INT64_ARRAY: {
			uint32_t len = f->get_32();

			Vector<int64_t> array;
			array.resize(len);
			if (len == 0) {
				r_v = array;
				break;
			}
			int64_t *w = array.ptrw();
			f->get_buffer((uint8_t *)w, len * sizeof(int64_t));
#ifdef BIG_ENDIAN_ENABLED
			{
				uint64_t *ptr = (uint64_t *)w.ptr();
				for (int i = 0; i < len; i++) {
					ptr[i] = BSWAP64(ptr[i]);
				}
			}

#endif

			r_v = array;
		} break;
		case VariantBin::VARIANT_FLOAT32_ARRAY: {
			uint32_t len = f->get_32();

			Vector<float> array;
			array.resize(len);
			if (len == 0) {
				r_v = array;
				break;
			}
			float *w = array.ptrw();

			f->get_buffer((uint8_t *)w, len * sizeof(float));
#ifdef BIG_ENDIAN_ENABLED
			{
				uint32_t *ptr = (uint32_t *)w.ptr();
				for (int i = 0; i < len; i++) {
					ptr[i] = BSWAP32(ptr[i]);
				}
			}

#endif

			r_v = array;
		} break;
		case VariantBin::VARIANT_FLOAT64_ARRAY: {
			uint32_t len = f->get_32();

			Vector<double> array;
			array.resize(len);
			if (len == 0) {
				r_v = array;
				break;
			}

			double *w = array.ptrw();
			f->get_buffer((uint8_t *)w, len * sizeof(double));
#ifdef BIG_ENDIAN_ENABLED
			{
				uint64_t *ptr = (uint64_t *)w.ptr();
				for (int i = 0; i < len; i++) {
					ptr[i] = BSWAP64(ptr[i]);
				}
			}

#endif

			r_v = array;
		} break;
		case VariantBin::VARIANT_STRING_ARRAY: {
			uint32_t len = f->get_32();
			Vector<String> array;
			array.resize(len);
			String *w = array.ptrw();
			for (uint32_t i = 0; i < len; i++) {
				w[i] = get_unicode_string();
			}

			r_v = array;

		} break;
		case VariantBin::VARIANT_VECTOR2_ARRAY: {
			uint32_t len = f->get_32();

			Vector<Vector2> array;
			array.resize(len);
			Vector2 *w = array.ptrw();
			if (sizeof(Vector2) == 8) {
				f->get_buffer((uint8_t *)w, len * sizeof(real_t) * 2);
#ifdef BIG_ENDIAN_ENABLED
				{
					uint32_t *ptr = (uint32_t *)w.ptr();
					for (int i = 0; i < len * 2; i++) {
						ptr[i] = BSWAP32(ptr[i]);
					}
				}

#endif

			} else {
				ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "Vector2 size is NOT 8!");
			}

			r_v = array;

		} break;
		case VariantBin::VARIANT_VECTOR3_ARRAY: {
			uint32_t len = f->get_32();

			Vector<Vector3> array;
			array.resize(len);
			Vector3 *w = array.ptrw();
			if (sizeof(Vector3) == 12) {
				f->get_buffer((uint8_t *)w, len * sizeof(real_t) * 3);
#ifdef BIG_ENDIAN_ENABLED
				{
					uint32_t *ptr = (uint32_t *)w.ptr();
					for (int i = 0; i < len * 3; i++) {
						ptr[i] = BSWAP32(ptr[i]);
					}
				}

#endif

			} else {
				ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "Vector3 size is NOT 12!");
			}

			r_v = array;

		} break;
		case VariantBin::VARIANT_COLOR_ARRAY: {
			uint32_t len = f->get_32();

			Vector<Color> array;
			array.resize(len);
			Color *w = array.ptrw();
			if (sizeof(Color) == 16) {
				f->get_buffer((uint8_t *)w, len * sizeof(real_t) * 4);
#ifdef BIG_ENDIAN_ENABLED
				{
					uint32_t *ptr = (uint32_t *)w.ptr();
					for (int i = 0; i < len * 4; i++) {
						ptr[i] = BSWAP32(ptr[i]);
					}
				}

#endif

			} else {
				ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "Color size is NOT 16!");
			}

			r_v = array;
		} break;
		default: {
			ERR_FAIL_V(ERR_FILE_CORRUPT);
		} break;
	}

	return OK;
}

void ResourceLoaderBinaryCompat::save_ustring(FileAccess *f, const String &p_string) {
	CharString utf8 = p_string.utf8();
	f->store_32(utf8.length() + 1);
	f->store_buffer((const uint8_t *)utf8.get_data(), utf8.length() + 1);
}

void ResourceLoaderBinaryCompat::save_unicode_string(const String &p_string) {
	save_ustring(f, p_string);
}

Error ResourceLoaderBinaryCompat::write_variant_bin(FileAccess *fa, const Variant &p_property, const PropertyInfo &p_hint) {
	switch (p_property.get_type()) {
		case Variant::NIL: {
			fa->store_32(VariantBin::VARIANT_NIL);
			// don't store anything
		} break;
		case Variant::BOOL: {
			fa->store_32(VariantBin::VARIANT_BOOL);
			bool val = p_property;
			fa->store_32(val);
		} break;
		case Variant::INT: {
			fa->store_32(VariantBin::VARIANT_INT);
			int val = p_property;
			fa->store_32(val);
		} break;
		case Variant::FLOAT: {
			fa->store_32(VariantBin::VARIANT_REAL);
			real_t val = p_property;
			fa->store_real(val);

		} break;
		case Variant::STRING: {
			fa->store_32(VariantBin::VARIANT_STRING);
			String val = p_property;
			save_ustring(fa, val);

		} break;
		case Variant::VECTOR2: {
			fa->store_32(VariantBin::VARIANT_VECTOR2);
			Vector2 val = p_property;
			fa->store_real(val.x);
			fa->store_real(val.y);

		} break;
		case Variant::RECT2: {
			fa->store_32(VariantBin::VARIANT_RECT2);
			Rect2 val = p_property;
			fa->store_real(val.position.x);
			fa->store_real(val.position.y);
			fa->store_real(val.size.x);
			fa->store_real(val.size.y);

		} break;
		case Variant::VECTOR3: {
			fa->store_32(VariantBin::VARIANT_VECTOR3);
			Vector3 val = p_property;
			fa->store_real(val.x);
			fa->store_real(val.y);
			fa->store_real(val.z);

		} break;
		case Variant::PLANE: {
			fa->store_32(VariantBin::VARIANT_PLANE);
			Plane val = p_property;
			fa->store_real(val.normal.x);
			fa->store_real(val.normal.y);
			fa->store_real(val.normal.z);
			fa->store_real(val.d);

		} break;
		case Variant::QUATERNION: {
			fa->store_32(VariantBin::VARIANT_QUAT);
			Quaternion val = p_property;
			fa->store_real(val.x);
			fa->store_real(val.y);
			fa->store_real(val.z);
			fa->store_real(val.w);

		} break;
		case Variant::AABB: {
			fa->store_32(VariantBin::VARIANT_AABB);
			AABB val = p_property;
			fa->store_real(val.position.x);
			fa->store_real(val.position.y);
			fa->store_real(val.position.z);
			fa->store_real(val.size.x);
			fa->store_real(val.size.y);
			fa->store_real(val.size.z);

		} break;
		case Variant::TRANSFORM2D: {
			fa->store_32(VariantBin::VARIANT_MATRIX32);
			Transform2D val = p_property;
			fa->store_real(val.elements[0].x);
			fa->store_real(val.elements[0].y);
			fa->store_real(val.elements[1].x);
			fa->store_real(val.elements[1].y);
			fa->store_real(val.elements[2].x);
			fa->store_real(val.elements[2].y);

		} break;
		case Variant::BASIS: {
			fa->store_32(VariantBin::VARIANT_MATRIX3);
			Basis val = p_property;
			fa->store_real(val.elements[0].x);
			fa->store_real(val.elements[0].y);
			fa->store_real(val.elements[0].z);
			fa->store_real(val.elements[1].x);
			fa->store_real(val.elements[1].y);
			fa->store_real(val.elements[1].z);
			fa->store_real(val.elements[2].x);
			fa->store_real(val.elements[2].y);
			fa->store_real(val.elements[2].z);

		} break;
		case Variant::TRANSFORM3D: {
			fa->store_32(VariantBin::VARIANT_TRANSFORM);
			Transform3D val = p_property;
			fa->store_real(val.basis.elements[0].x);
			fa->store_real(val.basis.elements[0].y);
			fa->store_real(val.basis.elements[0].z);
			fa->store_real(val.basis.elements[1].x);
			fa->store_real(val.basis.elements[1].y);
			fa->store_real(val.basis.elements[1].z);
			fa->store_real(val.basis.elements[2].x);
			fa->store_real(val.basis.elements[2].y);
			fa->store_real(val.basis.elements[2].z);
			fa->store_real(val.origin.x);
			fa->store_real(val.origin.y);
			fa->store_real(val.origin.z);

		} break;
		case Variant::COLOR: {
			fa->store_32(VariantBin::VARIANT_COLOR);
			Color val = p_property;
			fa->store_real(val.r);
			fa->store_real(val.g);
			fa->store_real(val.b);
			fa->store_real(val.a);

		} break;
		case Variant::NODE_PATH: {
			fa->store_32(VariantBin::VARIANT_NODE_PATH);
			NodePath np = p_property;
			fa->store_16(np.get_name_count());
			uint16_t snc = np.get_subname_count();
			// If ver_format 1-2 (i.e. godot 2.x)
			int property_idx = -1;
			if (ver_format < VariantBin::FORMAT_VERSION_NO_NODEPATH_PROPERTY) {
				// If there is a property, decrement the subname counter and store the property idx.
				if (np.get_subname_count() > 1 &&
						String(np.get_concatenated_subnames()).split(":").size() >= 2) {
					property_idx = np.get_subname_count() - 1;
					snc--;
				}
			}

			if (np.is_absolute()) {
				fa->store_16(snc | 0x8000);
			} else {
				fa->store_16(snc);
			}

			for (int i = 0; i < np.get_name_count(); i++)
				fa->store_32(string_map.find(np.get_name(i)));
			// store all subnames minus any property fields if need be
			for (int i = 0; i < snc; i++)
				fa->store_32(string_map.find(np.get_subname(i)));
			// If ver_format 1-2 (i.e. godot 2.x)
			if (ver_format < VariantBin::FORMAT_VERSION_NO_NODEPATH_PROPERTY) {
				// If we found a property, store it
				if (property_idx > -1) {
					fa->store_32(string_map.find(np.get_subname(property_idx)));
					// otherwise, store zero-length string
				} else {
					// 0x80000000 will resolve to a zero length string in the binary parser for any version
					uint32_t zlen = 0x80000000;
					fa->store_32(zlen);
				}
			}
		} break;
		case Variant::RID: {
			fa->store_32(VariantBin::VARIANT_RID);
			WARN_PRINT("Can't save RIDs");
			RID val = p_property;
			fa->store_32(val.get_id());
		} break;
		case Variant::OBJECT: {
			RES res = p_property;
			// This will only be triggered on godot 2.x, where the image variant is loaded as an image object vs. a resource
			if (ver_format == 1 && res->is_class("Image")) {
				fa->store_32(VariantBin::VARIANT_IMAGE);
				// storing lossless compressed by default
				ImageParserV2::write_image_v2_to_bin(fa, p_property, PROPERTY_HINT_IMAGE_COMPRESS_LOSSLESS);
			} else {
				fa->store_32(VariantBin::VARIANT_OBJECT);
				if (res.is_null()) {
					fa->store_32(VariantBin::OBJECT_EMPTY);
					return OK; // don't save it
				}
				String rpath = get_resource_path(res);
				if (rpath.length() && rpath.find("::") == -1) {
					if (!has_external_resource(rpath)) {
						fa->store_32(VariantBin::OBJECT_EMPTY);
						ERR_FAIL_COND_V_MSG(!has_external_resource(rpath), ERR_BUG, "Cannot find external resource");
					}
					//RES res = get_external_resource(rpath);
					fa->store_32(VariantBin::OBJECT_EXTERNAL_RESOURCE_INDEX);
					fa->store_32(res->get_scene_unique_id().to_int());
				} else {
					if (!has_internal_resource(rpath)) {
						fa->store_32(VariantBin::OBJECT_EMPTY);
						ERR_FAIL_V_MSG(ERR_BUG, "Resource was not pre cached for the resource section, bug?");
					}

					fa->store_32(VariantBin::OBJECT_INTERNAL_RESOURCE);
					fa->store_32(res->get_scene_unique_id().to_int());
				}
			}
		} break;
		case Variant::DICTIONARY: {
			fa->store_32(VariantBin::VARIANT_DICTIONARY);
			Dictionary d = p_property;
			fa->store_32(uint32_t(d.size()) | (p_property.is_shared() ? 0x80000000 : 0));

			List<Variant> keys;
			d.get_key_list(&keys);

			for (List<Variant>::Element *E = keys.front(); E; E = E->next()) {
				write_variant_bin(fa, E->get(), p_hint);
				write_variant_bin(fa, d[E->get()], p_hint);
			}

		} break;
		case Variant::ARRAY: {
			fa->store_32(VariantBin::VARIANT_ARRAY);
			Array a = p_property;
			fa->store_32(uint32_t(a.size()) | (p_property.is_shared() ? 0x80000000 : 0));
			for (int i = 0; i < a.size(); i++) {
				write_variant_bin(fa, a[i], p_hint);
			}

		} break;
		case Variant::PACKED_BYTE_ARRAY: {
			fa->store_32(VariantBin::VARIANT_RAW_ARRAY);
			Vector<uint8_t> arr = p_property;
			int len = arr.size();
			fa->store_32(len);
			fa->store_buffer(arr.ptr(), len);
			advance_padding(fa, len);

		} break;
		case Variant::PACKED_INT32_ARRAY: {
			fa->store_32(VariantBin::VARIANT_INT_ARRAY);
			Vector<int> arr = p_property;
			int len = arr.size();
			fa->store_32(len);
			for (int i = 0; i < len; i++)
				fa->store_32(arr.ptr()[i]);

		} break;
		case Variant::PACKED_FLOAT32_ARRAY: {
			fa->store_32(VariantBin::VARIANT_REAL_ARRAY);
			Vector<real_t> arr = p_property;
			int len = arr.size();
			fa->store_32(len);
			for (int i = 0; i < len; i++) {
				fa->store_real(arr.ptr()[i]);
			}

		} break;
		case Variant::PACKED_STRING_ARRAY: {
			fa->store_32(VariantBin::VARIANT_STRING_ARRAY);
			Vector<String> arr = p_property;
			int len = arr.size();
			fa->store_32(len);
			for (int i = 0; i < len; i++) {
				save_ustring(fa, arr.ptr()[i]);
			}

		} break;
		case Variant::PACKED_VECTOR3_ARRAY: {
			fa->store_32(VariantBin::VARIANT_VECTOR3_ARRAY);
			Vector<Vector3> arr = p_property;
			int len = arr.size();
			fa->store_32(len);

			for (int i = 0; i < len; i++) {
				fa->store_real(arr.ptr()[i].x);
				fa->store_real(arr.ptr()[i].y);
				fa->store_real(arr.ptr()[i].z);
			}

		} break;
		case Variant::PACKED_VECTOR2_ARRAY: {
			fa->store_32(VariantBin::VARIANT_VECTOR2_ARRAY);
			Vector<Vector2> arr = p_property;
			int len = arr.size();
			fa->store_32(len);
			for (int i = 0; i < len; i++) {
				fa->store_real(arr.ptr()[i].x);
				fa->store_real(arr.ptr()[i].y);
			}

		} break;
		case Variant::PACKED_COLOR_ARRAY: {
			fa->store_32(VariantBin::VARIANT_COLOR_ARRAY);
			Vector<Color> arr = p_property;
			int len = arr.size();
			fa->store_32(len);
			for (int i = 0; i < len; i++) {
				fa->store_real(arr.ptr()[i].r);
				fa->store_real(arr.ptr()[i].g);
				fa->store_real(arr.ptr()[i].b);
				fa->store_real(arr.ptr()[i].a);
			}

		} break;
		default: {
			ERR_FAIL_V_MSG(ERR_CANT_CREATE, "Invalid variant type");
		}
	}
	return OK;
}

Error ResourceLoaderBinaryCompat::save_to_bin(const String &p_path, uint32_t p_flags) {
	Error err;
	FileAccess *fw;
	if (p_flags & ResourceSaver::FLAG_COMPRESS) {
		FileAccessCompressed *fac = memnew(FileAccessCompressed);
		fac->configure("RSCC");
		fw = fac;
		err = fac->_open(p_path, FileAccess::WRITE);
		if (err)
			memdelete(fw);
	} else {
		fw = FileAccess::open(p_path, FileAccess::WRITE, &err);
	}

	ERR_FAIL_COND_V_MSG(err != OK, err, "can't open " + p_path + " for writing");

	bool takeover_paths = p_flags & ResourceSaver::FLAG_REPLACE_SUBRESOURCE_PATHS;
	bool big_endian = p_flags & ResourceSaver::FLAG_SAVE_BIG_ENDIAN || stored_big_endian;
	if (!p_path.begins_with("res://"))
		takeover_paths = false;

	//bin_meta_idx = get_string_index("__bin_meta__"); //is often used, so create

	if (!(p_flags & ResourceSaver::FLAG_COMPRESS)) {
		//save header compressed
		static const uint8_t header[4] = { 'R', 'S', 'R', 'C' };
		fw->store_buffer(header, 4);
	}

	if (big_endian) {
		fw->store_32(1);
		fw->set_big_endian(true);
	} else {
		fw->store_32(0);
	}

	fw->store_32(stored_use_real64 ? 1 : 0); //64 bits file

	fw->store_32(engine_ver_major);
	fw->store_32(engine_ver_minor);
	fw->store_32(ver_format);

	if (fw->get_error() != OK && fw->get_error() != ERR_FILE_EOF) {
		fw->close();
		return ERR_CANT_CREATE;
	}

	//fw->store_32(saved_resources.size()+external_resources.size()); // load steps -not needed
	save_ustring(fw, type);
	uint64_t md_at = fw->get_position();
	fw->store_64(0); //offset to impoty metadata

	uint32_t flags = 0;
	if (using_named_scene_ids) {
		flags |= ResourceFormatSaverBinaryInstance::FORMAT_FLAG_NAMED_SCENE_IDS;
	}
	if (using_uids) {
		flags |= ResourceFormatSaverBinaryInstance::FORMAT_FLAG_UIDS;
		fw->store_32(flags);
		fw->store_64(uid);
	} else {
		fw->store_32(flags);
		fw->store_64(0);
	}

	for (int i = 0; i < ResourceFormatSaverBinaryInstance::RESERVED_FIELDS; i++) {
		fw->store_32(0); // reserved
	}

	fw->store_32(string_map.size()); //string table size
	for (int i = 0; i < string_map.size(); i++) {
		//print_bl("saving string: "+strings[i]);
		save_ustring(fw, string_map[i]);
	}

	// save external resource table
	fw->store_32(external_resources.size()); //amount of external resources
	for (int i = 0; i < external_resources.size(); i++) {
		save_ustring(fw, external_resources[i].type);
		save_ustring(fw, external_resources[i].path);
	}

	// save internal resource table
	fw->store_32(internal_resources.size()); //amount of internal resources
	Vector<uint64_t> ofs_pos;

	for (int i = 0; i < internal_resources.size(); i++) {
		RES re = get_internal_resource(internal_resources[i].path);
		ERR_FAIL_COND_V_MSG(re.is_null(), ERR_CANT_ACQUIRE_RESOURCE, "Can't find internal resource " + internal_resources[i].path);
		String path = get_resource_path(re);
		if (path == "" || path.find("::") != -1) {
			save_ustring(fw, "local://" + re->get_scene_unique_id());
		} else if (path == local_path) {
			save_ustring(fw, local_path); // main resource
		} else {
			WARN_PRINT("Possible malformed path: " + path);
			save_ustring(fw, path);
		}
		ofs_pos.push_back(fw->get_position());
		fw->store_64(0); //offset in 64 bits
	}

	Vector<uint64_t> ofs_table;
	//now actually save the resources
	for (int i = 0; i < internal_resources.size(); i++) {
		RES re = get_internal_resource(internal_resources[i].path);
		ERR_FAIL_COND_V_MSG(re.is_null(), ERR_CANT_ACQUIRE_RESOURCE, "Can't find internal resource " + internal_resources[i].path);
		String path = get_resource_path(re);
		String rtype = get_internal_resource_type(path);
		List<ResourceProperty> lrp = get_internal_resource_properties(path);
		ofs_table.push_back(fw->get_position());
		save_ustring(fw, rtype);
		fw->store_32(lrp.size());
		for (List<ResourceProperty>::Element *F = lrp.front(); F; F = F->next()) {
			ResourceProperty &p = F->get();
			fw->store_32(string_map.find(p.name));
			write_variant_bin(fw, p.value);
		}
	}

	for (int i = 0; i < ofs_table.size(); i++) {
		fw->seek(ofs_pos[i]);
		fw->store_64(ofs_table[i]);
	}
	fw->seek_end();
	//print_line("Saving: " + p_path);
	if (imd.is_valid()) {
		uint64_t md_pos = fw->get_position();
		save_ustring(fw, imd->get_editor());
		fw->store_32(imd->get_source_count());
		for (int i = 0; i < imd->get_source_count(); i++) {
			save_ustring(fw, imd->get_source_path(i));
			save_ustring(fw, imd->get_source_md5(i));
		}
		List<String> options;
		imd->get_options(&options);
		fw->store_32(options.size());
		for (List<String>::Element *E = options.front(); E; E = E->next()) {
			save_ustring(fw, E->get());
			write_variant_bin(fw, imd->get_option(E->get()));
		}

		fw->seek(md_at);
		fw->store_64(md_pos);
		fw->seek_end();
	}

	fw->store_buffer((const uint8_t *)"RSRC", 4); //magic at end

	if (fw->get_error() != OK && fw->get_error() != ERR_FILE_EOF) {
		fw->close();
		memdelete(fw);
		return ERR_CANT_CREATE;
	}

	fw->close();
	memdelete(fw);
	return OK;
}

ResourceLoaderBinaryCompat::ResourceLoaderBinaryCompat() {}
ResourceLoaderBinaryCompat::~ResourceLoaderBinaryCompat() {
	if (f != nullptr) {
		memdelete(f);
	}
}

void ResourceLoaderBinaryCompat::get_dependencies(FileAccess *p_f, List<String> *p_dependencies, bool p_add_types, bool only_paths) {
	open(p_f);
	if (error) {
		return;
	}

	for (int i = 0; i < external_resources.size(); i++) {
		String dep;
		if (!only_paths && external_resources[i].uid != ResourceUID::INVALID_ID) {
			dep = ResourceUID::get_singleton()->id_to_text(external_resources[i].uid);
		} else {
			dep = external_resources[i].path;
		}

		if (p_add_types && external_resources[i].type != String()) {
			dep += "::" + external_resources[i].type;
		}

		p_dependencies->push_back(dep);
	}
}
