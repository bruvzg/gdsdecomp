#include "resource_loader_compat.h"
#include "core/io/file_access_compressed.h"
#include "variant_writer_compat.h"
#include "core/string/ustring.h"
#include "V2ImageParser.h"
struct ExtResource {
    String path;
    String type;
    RES cache;
};
struct IntResource {
    String path;
    uint64_t offset;
};
Error ResourceFormatLoaderBinaryCompat::convert_bin_to_txt(const String &p_path, const String &dst, const String &output_dir , float *r_progress){
	Error error = OK;
	FileAccess *f = FileAccess::open(p_path, FileAccess::READ, &error);

	ERR_FAIL_COND_V_MSG(error != OK, error, "Cannot open file '" + p_path + "'.");

	ResourceLoaderBinaryCompat loader;
    //Vector<uint32_t> versions = loader.get_version(f);
	//f->seek(0);
	loader.progress = r_progress;
	loader.no_ext_load = true;
	String path = p_path;
	loader.local_path = path;
	loader.res_path = loader.local_path;
	//loader.set_local_path( Globals::get_singleton()->localize_path(p_path) );
	loader.open(f);

	error = loader.load();
	ERR_FAIL_COND_V_MSG(error != OK, error, "Cannot load resource '" + p_path + "'.");
	error = loader.save_as_text(dst);

	ERR_FAIL_COND_V_MSG(error != OK, error, "failed to save resource '" + p_path + "' as '"+ dst +"'.");
	return OK;
}

RES ResourceFormatLoaderBinaryCompat::_load(const String &p_path, const String &base_dir, bool no_ext_load, Error *r_error, float *r_progress) {
	
	Error error = OK;
	FileAccess *f = FileAccess::open(p_path, FileAccess::READ, &error);

	ERR_FAIL_COND_V_MSG(error != OK, RES(), "Cannot open file '" + p_path + "'.");

	ResourceLoaderBinaryCompat loader;
    Vector<uint32_t> versions = loader.get_version(f);

	loader.progress = r_progress;
	String path = p_path;
	loader.local_path = base_dir;
	loader.res_path = loader.local_path;
	//loader.set_local_path( Globals::get_singleton()->localize_path(p_path) );
	loader.open(f);

	error = loader.load();

	if (r_error) {
		*r_error = error;
	}

	if (error) {
		return RES();
	}
	return loader.resource;

}
Vector<uint32_t> ResourceLoaderBinaryCompat::get_version(FileAccess * p_f){
	error = OK;

	f = p_f;
	uint8_t header[4];
	f->get_buffer(header, 4);
	if (header[0] == 'R' && header[1] == 'S' && header[2] == 'C' && header[3] == 'C') {
		// Compressed.
		FileAccessCompressed *fac = memnew(FileAccessCompressed);
		error = fac->open_after_magic(f);
		if (error != OK) {
			memdelete(fac);
			f->close();
			return Vector<uint32_t>();
		}
		f = fac;

	} else if (header[0] != 'R' || header[1] != 'S' || header[2] != 'R' || header[3] != 'C') {
		// Not normal.
		error = ERR_FILE_UNRECOGNIZED;
		f->close();
		return Vector<uint32_t>();
	}

	bool big_endian = f->get_32();
	f->get_32(); // use_real64

	f->set_endian_swap(big_endian != 0); //read big endian if saved as big endian
    Vector<uint32_t> ret;
    uint32_t ver_major = f->get_32();
	uint32_t ver_minor = f->get_32(); // ver_minor
	uint32_t ver_format = f->get_32();
    ret.push_back(ver_major);
    ret.push_back(ver_minor);
    ret.push_back(ver_format);

    return ret;
	
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

void ResourceLoaderBinaryCompat::open(FileAccess *p_f) {
	error = OK;

	f = p_f;
	uint8_t header[4];
	f->get_buffer(header, 4);
	if (header[0] == 'R' && header[1] == 'S' && header[2] == 'C' && header[3] == 'C') {
		// Compressed.
		FileAccessCompressed *fac = memnew(FileAccessCompressed);
		error = fac->open_after_magic(f);
		if (error != OK) {
			memdelete(fac);
			f->close();
			ERR_FAIL_MSG("Failed to open binary resource file: " + local_path + ".");
		}
		f = fac;

	} else if (header[0] != 'R' || header[1] != 'S' || header[2] != 'R' || header[3] != 'C') {
		// Not normal.
		error = ERR_FILE_UNRECOGNIZED;
		f->close();
		ERR_FAIL_MSG("Unrecognized binary resource file: " + local_path + ".");
	}

	bool big_endian = f->get_32();
	bool use_real64 = f->get_32();

	f->set_endian_swap(big_endian != 0); //read big endian if saved as big endian

	engine_ver_major = f->get_32();
	uint32_t ver_minor = f->get_32();
	ver_format = f->get_32();

	print_bl("big endian: " + itos(big_endian));
#ifdef BIG_ENDIAN_ENABLED
	print_bl("endian swap: " + itos(!big_endian));
#else
	print_bl("endian swap: " + itos(big_endian));
#endif
	print_bl("real64: " + itos(use_real64));
	print_bl("major: " + itos(engine_ver_major));
	print_bl("minor: " + itos(ver_minor));
	print_bl("format: " + itos(ver_format));


	type = get_unicode_string();

	print_bl("type: " + type);

	importmd_ofs = f->get_64();
	for (int i = 0; i < 14; i++) {
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
		ERR_FAIL_MSG("Premature end of file (EOF): " + local_path + ".");
	}
}

Error ResourceLoaderBinaryCompat::load(){

	for (int i = 0; i < 26; i++){
		typeV2toV3Renames[type_v2_to_v3renames[i][0]] = type_v2_to_v3renames[i][1];
	}
	for (int i = 0; i < 26; i++){
		typeV3toV2Renames[type_v2_to_v3renames[i][1]] = type_v2_to_v3renames[i][0];
	}

	if (error != OK) {
		return error;
	}

	int stage = 0;

	for (int i = 0; i < external_resources.size(); i++) {
		String path = external_resources[i].path;

		if (remaps.has(path)) {
			path = remaps[path];
		}

		if (path.find("://") == -1 && path.is_rel_path()) {
			// path is relative to file being loaded, so convert to a resource path
			// Figure this shit out
			//path = ProjectSettings::get_singleton()->localize_path(path.get_base_dir().plus_file(external_resources[i].path));
		}

		external_resources.write[i].path = path; //remap happens here, not on load because on load it can actually be used for filesystem dock resource remap

		//are we loading the externs?
		if (!no_ext_load) {
			external_resources.write[i].cache = ResourceLoader::load(path, external_resources[i].type);

			if (external_resources[i].cache.is_null()) {
				if (!ResourceLoader::get_abort_on_missing_resources()) {
					ResourceLoader::notify_dependency_error(local_path, path, external_resources[i].type);
				} else {
					error = ERR_FILE_MISSING_DEPENDENCIES;
					ERR_FAIL_V_MSG(error, "Can't load dependency: " + path + ".");
				}
			}
		}
		stage++;
	}

	for (int i = 0; i < internal_resources.size(); i++) {
		bool main = i == (internal_resources.size() - 1);

		//maybe it is loaded already
		String path;
		int subindex = 0;

		if (!main) {
			path = internal_resources[i].path;

			if (path.begins_with("local://")) {
			 	path = path.replace_first("local://", "");
			 	subindex = path.to_int();
			 	path = res_path + "::" + path;
			}

			// if (cache_mode == ResourceFormatLoader::CACHE_MODE_REUSE) {
			// 	if (ResourceCache::has(path)) {
			// 		//already loaded, don't do anything
			// 		stage++;
			// 		error = OK;
			// 		continue;
			// 	}
			// }
		} else {
			if (cache_mode != ResourceFormatLoader::CACHE_MODE_IGNORE && !ResourceCache::has(res_path)) {
				path = res_path;
			}
		}

		uint64_t offset = internal_resources[i].offset;

		f->seek(offset);

		String t = get_unicode_string();

		RES res;

		if (cache_mode == ResourceFormatLoader::CACHE_MODE_REPLACE && ResourceCache::has(path)) {
			//use the existing one
			Resource *r = ResourceCache::get(path);
		 	if (r->get_class() == t) {
		 		r->reset_state();
		 		res = Ref<Resource>(r);
			}
		}

		if (res.is_null()) {
			//did not replace
			if (typeV2toV3Renames.has(t)){
				t = typeV2toV3Renames[t];
			}
			Object *obj = ClassDB::instance(t);
			if (!obj) {
				error = ERR_FILE_CORRUPT;
				ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, local_path + ":Resource of unrecognized type in file: " + t + ".");
			}
			
			Resource *r = Object::cast_to<Resource>(obj);
			if (!r) {
				String obj_class = obj->get_class();
				error = ERR_FILE_CORRUPT;
				memdelete(obj); //bye
				ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, local_path + ":Resource type in resource field not a resource, type is: " + obj_class + ".");
			}

			res = RES(r);
			if (path != String() && cache_mode != ResourceFormatLoader::CACHE_MODE_IGNORE) {
				r->set_path(path, cache_mode == ResourceFormatLoader::CACHE_MODE_REPLACE); //if got here because the resource with same path has different type, replace it
			}
			r->set_subindex(subindex);
		}
		
		internal_resources.write[i].path = path;
		if (!main) {
			internal_index_cache[path] = res;
		}

		int pc = f->get_32();

		//set properties

		for (int j = 0; j < pc; j++) {
			StringName name = _get_string();

			if (name == StringName()) {
				error = ERR_FILE_CORRUPT;
				ERR_FAIL_V(ERR_FILE_CORRUPT);
			}

			Variant value;
			if (ver_format == 1){
				error = parse_variantv2(value);
			} else if (ver_format == 3){
				error = parse_variantv3v4(value);
			} else {
				ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "Binary resource format v2, and v4 and above, are not supported");
			}
			if (error) {
				return error;
			}

			res->set(name, value);
		}
#ifdef TOOLS_ENABLED
		res->set_edited(false);
#endif
		stage++;

		if (progress) {
			*progress = (i + 1) / float(internal_resources.size());
		}

		resource_cache.push_back(res);

		if (main) {
			f->close();
			resource = res;
			resource->set_as_translation_remapped(translation_remapped);
			error = OK;
			return OK;
		}
	}

	return ERR_FILE_EOF;

}
String ResourceLoaderBinaryCompat::get_ustring(FileAccess *f) {
	int len = f->get_32();
	Vector<char> str_buf;
	str_buf.resize(len);
	f->get_buffer((uint8_t *)&str_buf[0], len);
	String s;
	s.parse_utf8(&str_buf[0]);
	return s;
}

String ResourceLoaderBinaryCompat::get_unicode_string() {
	return get_ustring(f);
}

Error ResourceLoaderBinaryCompat::parse_variantv2(Variant &r_v) {
	ResourceLoader::set_abort_on_missing_resources(false);
	uint32_t type = f->get_32();
	print_bl("find property of type: " + itos(type));
    bool p_for_export_data = false;
	switch (type) {

		case VariantBinV1::Type::VARIANT_NIL: {

			r_v = Variant();
		} break;
		case VariantBinV1::Type::VARIANT_BOOL: {

			r_v = bool(f->get_32());
		} break;
		case VariantBinV1::Type::VARIANT_INT: {

			r_v = int(f->get_32());
		} break;
		case VariantBinV1::Type::VARIANT_REAL: {

			r_v = f->get_real();
		} break;
		case VariantBinV1::Type::VARIANT_STRING: {
			r_v = get_unicode_string();
		} break;
		case VariantBinV1::Type::VARIANT_VECTOR2: {

			Vector2 v;
			v.x = f->get_real();
			v.y = f->get_real();
			r_v = v;

		} break;
		case VariantBinV1::Type::VARIANT_RECT2: {

			Rect2 v;
			v.position.x = f->get_real();
			v.position.y = f->get_real();
			v.size.x = f->get_real();
			v.size.y = f->get_real();
			r_v = v;

		} break;
		case VariantBinV1::Type::VARIANT_VECTOR3: {

			Vector3 v;
			v.x = f->get_real();
			v.y = f->get_real();
			v.z = f->get_real();
			r_v = v;
		} break;
		case VariantBinV1::Type::VARIANT_PLANE: {

			Plane v;
			v.normal.x = f->get_real();
			v.normal.y = f->get_real();
			v.normal.z = f->get_real();
			v.d = f->get_real();
			r_v = v;
		} break;
		case VariantBinV1::Type::VARIANT_QUAT: {
			Quat v;
			v.x = f->get_real();
			v.y = f->get_real();
			v.z = f->get_real();
			v.w = f->get_real();
			r_v = v;

		} break;
		case VariantBinV1::Type::VARIANT_AABB: {

			AABB v;
			v.position.x = f->get_real();
			v.position.y = f->get_real();
			v.position.z = f->get_real();
			v.size.x = f->get_real();
			v.size.y = f->get_real();
			v.size.z = f->get_real();
			r_v = v;

		} break;
		case VariantBinV1::Type::VARIANT_MATRIX32: {

			Transform2D v;
			v.elements[0].x = f->get_real();
			v.elements[0].y = f->get_real();
			v.elements[1].x = f->get_real();
			v.elements[1].y = f->get_real();
			v.elements[2].x = f->get_real();
			v.elements[2].y = f->get_real();
			r_v = v;

		} break;
		case VariantBinV1::Type::VARIANT_MATRIX3: {

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
		case VariantBinV1::Type::VARIANT_TRANSFORM: {

			Transform v;
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
		case VariantBinV1::Type::VARIANT_COLOR: {

			Color v;
			v.r = f->get_real();
			v.g = f->get_real();
			v.b = f->get_real();
			v.a = f->get_real();
			r_v = v;

		} break;
		case VariantBinV1::Type::VARIANT_IMAGE: {
			//Have to parse Image here
			if (V2ImageParser::parse_image_v2(f, r_v) != OK){
				WARN_PRINT(String("Couldn't load resource: embedded image").utf8().get_data());
			}
		} break;
		case VariantBinV1::Type::VARIANT_NODE_PATH: {

			Vector<StringName> names;
			Vector<StringName> subnames;
			//StringName property;
			bool absolute;

			int name_count = f->get_16();
			uint32_t subname_count = f->get_16();
			absolute = subname_count & 0x8000;
			subname_count &= 0x7FFF;

			for (int i = 0; i < name_count; i++)
				names.push_back(_get_string());
			for (uint32_t i = 0; i < subname_count; i++)
				subnames.push_back(_get_string());
			//skip property
			f->get_32();
			//property = string_map[f->get_32()];

			NodePath np = NodePath(names, subnames, absolute);
			//print_line("got path: "+String(np));

			r_v = np;

		} break;
		case VariantBinV1::Type::VARIANT_RID: {

			r_v = f->get_32();
		} break;
		case VariantBinV1::Type::VARIANT_OBJECT: {

			uint32_t type = f->get_32();

			switch (type) {

				case VariantBinV1::Type::OBJECT_EMPTY: {
					//do none

				} break;
				case VariantBinV1::Type::OBJECT_INTERNAL_RESOURCE: {
					uint32_t index = f->get_32();
					String path = res_path + "::" + itos(index);

					//always use internal cache for loading internal resources
					if (!internal_index_cache.has(path)) {
						WARN_PRINT(String("Couldn't load resource (no cache): " + path).utf8().get_data());
						r_v = Variant();
					} else {
						r_v = internal_index_cache[path];
					}
				} break;
				case VariantBinV1::Type::OBJECT_EXTERNAL_RESOURCE: {
					//old file format, still around for compatibility

					String exttype = get_unicode_string();
					String path = get_unicode_string();

					if (no_ext_load){
						Ref<DummyExtResource> dummy;
						dummy.instance();
						dummy->set_path(path);
						dummy->set_type(exttype);
						for (int i = 0; i < external_resources.size(); i++){
							if (external_resources[i].path == path){
								dummy->set_id(i);
								external_resources.write[i].cache = dummy;
							}
						}
						r_v = dummy;
						break;
					}

					if (path.find("://") == -1 && path.is_rel_path()) {
						// path is relative to file being loaded, so convert to a resource path
						path = ProjectSettings::get_singleton()->localize_path(res_path.get_base_dir().plus_file(path));
					}

					if (remaps.find(path)) {
						path = remaps[path];
					}

					RES res = ResourceLoader::load(path, exttype);

					if (res.is_null()) {
						WARN_PRINT(String("Couldn't load resource: " + path).utf8().get_data());
					}
					r_v = res;

				} break;
				case VariantBinV1::Type::OBJECT_EXTERNAL_RESOURCE_INDEX: {
					//new file format, just refers to an index in the external list
					int erindex = f->get_32();

					if (erindex < 0 || erindex >= external_resources.size()) {
						WARN_PRINT("Broken external resource! (index out of size)");
						r_v = Variant();
					} else {
						//Don't load External resources
						if (no_ext_load){
							Ref<DummyExtResource> dummy;
							dummy.instance();
							dummy->set_id(erindex);
							dummy->set_path(external_resources[erindex].path);
							dummy->set_type(external_resources[erindex].type);
							r_v = dummy;
							break;
						}

						if (external_resources[erindex].cache.is_null()) {
							//cache not here yet, wait for it?
							if (use_sub_threads) {
								Error err;
								external_resources.write[erindex].cache = ResourceLoader::load_threaded_get(external_resources[erindex].path, &err);

								if (err != OK || external_resources[erindex].cache.is_null()) {
									if (!ResourceLoader::get_abort_on_missing_resources()) {
										ResourceLoader::notify_dependency_error(local_path, external_resources[erindex].path, external_resources[erindex].type);
									} else {
										error = ERR_FILE_MISSING_DEPENDENCIES;
										ERR_FAIL_V_MSG(error, "Can't load dependency: " + external_resources[erindex].path + ".");
									}
								}
							}
						}

						r_v = external_resources[erindex].cache;
					}

				} break;
				default: {

					ERR_FAIL_V(ERR_FILE_CORRUPT);
				} break;
			}

		} break;
		case VariantBinV1::Type::VARIANT_INPUT_EVENT: {
			// No need, they're not saved in binary files
		} break;
		case VariantBinV1::Type::VARIANT_DICTIONARY: {

			uint32_t len = f->get_32();
			Dictionary d = Dictionary(); //last bit means shared
			len &= 0x7FFFFFFF;
			for (uint32_t i = 0; i < len; i++) {
				Variant key;
				Error err = parse_variantv2(key);
				ERR_FAIL_COND_V(err, ERR_FILE_CORRUPT);
				Variant value;
				err = parse_variantv2(value);
				ERR_FAIL_COND_V(err, ERR_FILE_CORRUPT);
				d[key] = value;
			}
			r_v = d;
		} break;
		case VariantBinV1::Type::VARIANT_ARRAY: {

			uint32_t len = f->get_32();
			Array a = Array(); //last bit means shared
			len &= 0x7FFFFFFF;
			a.resize(len);
			for (uint32_t i = 0; i < len; i++) {
				Variant val;
				Error err = parse_variantv2(val);
				ERR_FAIL_COND_V(err, ERR_FILE_CORRUPT);
				a[i] = val;
			}
			r_v = a;

		} break;
		case VariantBinV1::Type::VARIANT_RAW_ARRAY: {

			uint32_t len = f->get_32();

			Vector<uint8_t> array;
			array.resize(len);
			uint8_t *w = array.ptrw();
			f->get_buffer(w, len);
			_advance_padding(len);
			r_v = array;

		} break;
		case VariantBinV1::Type::VARIANT_INT_ARRAY: {

			uint32_t len = f->get_32();

			Vector<int32_t> array;
			array.resize(len);
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
		case VariantBinV1::Type::VARIANT_REAL_ARRAY: {

			uint32_t len = f->get_32();

			Vector<float> array;
			array.resize(len);
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
		case VariantBinV1::Type::VARIANT_STRING_ARRAY: {

			uint32_t len = f->get_32();
			Vector<String> array;
			array.resize(len);
			String *w = array.ptrw();
			for (uint32_t i = 0; i < len; i++) {
				w[i] = get_unicode_string();
			}
			r_v = array;
		} break;
		case VariantBinV1::Type::VARIANT_VECTOR2_ARRAY: {

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
		case VariantBinV1::Type::VARIANT_VECTOR3_ARRAY: {
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
		case VariantBinV1::Type::VARIANT_COLOR_ARRAY: {

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

	return OK; //never reach anyway
}



Error ResourceLoaderBinaryCompat::parse_variantv3v4(Variant &r_v) {
	uint32_t type = f->get_32();
	printf("find property of type: %d", type);

	switch (type) {
		case VariantBinV3::Type::VARIANT_NIL: {
			r_v = Variant();
		} break;
		case VariantBinV3::Type::VARIANT_BOOL: {
			r_v = bool(f->get_32());
		} break;
		case VariantBinV3::Type::VARIANT_INT: {
			r_v = int(f->get_32());
		} break;
		case VariantBinV3::Type::VARIANT_INT64: {
			r_v = int64_t(f->get_64());
		} break;
		case VariantBinV3::Type::VARIANT_FLOAT: {
			r_v = f->get_real();
		} break;
		case VariantBinV3::Type::VARIANT_DOUBLE: {
			r_v = f->get_double();
		} break;
		case VariantBinV3::Type::VARIANT_STRING: {
			r_v = get_unicode_string();
		} break;
		case VariantBinV3::Type::VARIANT_VECTOR2: {
			Vector2 v;
			v.x = f->get_real();
			v.y = f->get_real();
			r_v = v;

		} break;
		case VariantBinV3::Type::VARIANT_VECTOR2I: {
			Vector2i v;
			v.x = f->get_32();
			v.y = f->get_32();
			r_v = v;

		} break;
		case VariantBinV3::Type::VARIANT_RECT2: {
			Rect2 v;
			v.position.x = f->get_real();
			v.position.y = f->get_real();
			v.size.x = f->get_real();
			v.size.y = f->get_real();
			r_v = v;

		} break;
		case VariantBinV3::Type::VARIANT_RECT2I: {
			Rect2i v;
			v.position.x = f->get_32();
			v.position.y = f->get_32();
			v.size.x = f->get_32();
			v.size.y = f->get_32();
			r_v = v;

		} break;
		case VariantBinV3::Type::VARIANT_VECTOR3: {
			Vector3 v;
			v.x = f->get_real();
			v.y = f->get_real();
			v.z = f->get_real();
			r_v = v;
		} break;
		case VariantBinV3::Type::VARIANT_VECTOR3I: {
			Vector3i v;
			v.x = f->get_32();
			v.y = f->get_32();
			v.z = f->get_32();
			r_v = v;
		} break;
		case VariantBinV3::Type::VARIANT_PLANE: {
			Plane v;
			v.normal.x = f->get_real();
			v.normal.y = f->get_real();
			v.normal.z = f->get_real();
			v.d = f->get_real();
			r_v = v;
		} break;
		case VariantBinV3::Type::VARIANT_QUAT: {
			Quat v;
			v.x = f->get_real();
			v.y = f->get_real();
			v.z = f->get_real();
			v.w = f->get_real();
			r_v = v;

		} break;
		case VariantBinV3::Type::VARIANT_AABB: {
			AABB v;
			v.position.x = f->get_real();
			v.position.y = f->get_real();
			v.position.z = f->get_real();
			v.size.x = f->get_real();
			v.size.y = f->get_real();
			v.size.z = f->get_real();
			r_v = v;

		} break;
		case VariantBinV3::Type::VARIANT_MATRIX32: {
			Transform2D v;
			v.elements[0].x = f->get_real();
			v.elements[0].y = f->get_real();
			v.elements[1].x = f->get_real();
			v.elements[1].y = f->get_real();
			v.elements[2].x = f->get_real();
			v.elements[2].y = f->get_real();
			r_v = v;

		} break;
		case VariantBinV3::Type::VARIANT_MATRIX3: {
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
		case VariantBinV3::Type::VARIANT_TRANSFORM: {
			Transform v;
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
		case VariantBinV3::Type::VARIANT_COLOR: {
			Color v; // Colors should always be in single-precision.
			v.r = f->get_float();
			v.g = f->get_float();
			v.b = f->get_float();
			v.a = f->get_float();
			r_v = v;

		} break;
		case VariantBinV3::Type::VARIANT_STRING_NAME: {
			r_v = StringName(get_unicode_string());
		} break;

		case VariantBinV3::Type::VARIANT_NODE_PATH: {
			Vector<StringName> names;
			Vector<StringName> subnames;
			bool absolute;

			int name_count = f->get_16();
			uint32_t subname_count = f->get_16();
			absolute = subname_count & 0x8000;
			subname_count &= 0x7FFF;
			int ver_format = 2;
			if (ver_format < VariantBinV3::FORMAT_VERSION_NO_NODEPATH_PROPERTY) {
				subname_count += 1; // has a property field, so we should count it as well
			}

			for (int i = 0; i < name_count; i++) {
				names.push_back(_get_string());
			}
			for (uint32_t i = 0; i < subname_count; i++) {
				subnames.push_back(_get_string());
			}

			NodePath np = NodePath(names, subnames, absolute);

			r_v = np;

		} break;
		case VariantBinV3::Type::VARIANT_RID: {
			r_v = f->get_32();
		} break;
		case VariantBinV3::Type::VARIANT_OBJECT: {
			uint32_t objtype = f->get_32();

			switch (objtype) {
				case VariantBinV3::Type::OBJECT_EMPTY: {
					//do none

				} break;
				case VariantBinV3::Type::OBJECT_INTERNAL_RESOURCE: {
					uint32_t index = f->get_32();
					String path = res_path + "::" + itos(index);

					//always use internal cache for loading internal resources
					if (!internal_index_cache.has(path)) {
						WARN_PRINT(String("Couldn't load resource (no cache): " + path).utf8().get_data());
						r_v = Variant();
					} else {
						r_v = internal_index_cache[path];
					}

				} break;
				case VariantBinV3::Type::OBJECT_EXTERNAL_RESOURCE: {
					//old file format, still around for compatibility

					String exttype = get_unicode_string();
					String path = get_unicode_string();

					if (no_ext_load){
						Ref<DummyExtResource> dummy;
						dummy.instance();
						dummy->set_path(path);
						dummy->set_type(exttype);
						for (int i = 0; i < external_resources.size(); i++){
							if (external_resources[i].path == path){
								dummy->set_id(i);
								external_resources.write[i].cache = dummy;
							}
						}
						r_v = dummy;
						break;
					}

					if (path.find("://") == -1 && path.is_rel_path()) {
						// path is relative to file being loaded, so convert to a resource path
						path = ProjectSettings::get_singleton()->localize_path(res_path.get_base_dir().plus_file(path));
					}

					if (remaps.find(path)) {
						path = remaps[path];
					}

					RES res = ResourceLoader::load(path, exttype);

					if (res.is_null()) {
						WARN_PRINT(String("Couldn't load resource: " + path).utf8().get_data());
					}
					r_v = res;

				} break;
				case VariantBinV3::Type::OBJECT_EXTERNAL_RESOURCE_INDEX: {
					//new file format, just refers to an index in the external list
					int erindex = f->get_32();

					if (erindex < 0 || erindex >= external_resources.size()) {
						WARN_PRINT("Broken external resource! (index out of size)");
						r_v = Variant();
					} else {
						//Don't load External resources
						if (no_ext_load){
							Ref<DummyExtResource> dummy;
							dummy.instance();
							dummy->set_id(erindex);
							dummy->set_path(external_resources[erindex].path);
							dummy->set_type(external_resources[erindex].type);
							r_v = dummy;
							break;
						}

						if (external_resources[erindex].cache.is_null()) {
							//cache not here yet, wait for it?
							if (use_sub_threads) {
								Error err;
								external_resources.write[erindex].cache = ResourceLoader::load_threaded_get(external_resources[erindex].path, &err);

								if (err != OK || external_resources[erindex].cache.is_null()) {
									if (!ResourceLoader::get_abort_on_missing_resources()) {
										ResourceLoader::notify_dependency_error(local_path, external_resources[erindex].path, external_resources[erindex].type);
									} else {
										error = ERR_FILE_MISSING_DEPENDENCIES;
										ERR_FAIL_V_MSG(error, "Can't load dependency: " + external_resources[erindex].path + ".");
									}
								}
							}
						}

						r_v = external_resources[erindex].cache;
					}

				} break;
				default: {
					ERR_FAIL_V(ERR_FILE_CORRUPT);
				} break;
			}

		} break;
		case VariantBinV3::Type::VARIANT_CALLABLE: {
			r_v = Callable();
		} break;
		case VariantBinV3::Type::VARIANT_SIGNAL: {
			r_v = Signal();
		} break;

		case VariantBinV3::Type::VARIANT_DICTIONARY: {
			uint32_t len = f->get_32();
			Dictionary d; //last bit means shared
			len &= 0x7FFFFFFF;
			for (uint32_t i = 0; i < len; i++) {
				Variant key;
				Error err = parse_variantv3v4(key);
				ERR_FAIL_COND_V_MSG(err, ERR_FILE_CORRUPT, "Error when trying to parse Variant.");
				Variant value;
				err = parse_variantv3v4(value);
				ERR_FAIL_COND_V_MSG(err, ERR_FILE_CORRUPT, "Error when trying to parse Variant.");
				d[key] = value;
			}
			r_v = d;
		} break;
		case VariantBinV3::Type::VARIANT_ARRAY: {
			uint32_t len = f->get_32();
			Array a; //last bit means shared
			len &= 0x7FFFFFFF;
			a.resize(len);
			for (uint32_t i = 0; i < len; i++) {
				Variant val;
				Error err = parse_variantv3v4(val);
				ERR_FAIL_COND_V_MSG(err, ERR_FILE_CORRUPT, "Error when trying to parse Variant.");
				a[i] = val;
			}
			r_v = a;

		} break;
		case VariantBinV3::Type::VARIANT_RAW_ARRAY: {
			uint32_t len = f->get_32();

			Vector<uint8_t> array;
			array.resize(len);
			uint8_t *w = array.ptrw();
			f->get_buffer(w, len);
			_advance_padding(len);

			r_v = array;

		} break;
		case VariantBinV3::Type::VARIANT_INT32_ARRAY: {
			uint32_t len = f->get_32();

			Vector<int32_t> array;
			array.resize(len);
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
		case VariantBinV3::Type::VARIANT_INT64_ARRAY: {
			uint32_t len = f->get_32();

			Vector<int64_t> array;
			array.resize(len);
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
		case VariantBinV3::Type::VARIANT_FLOAT32_ARRAY: {
			uint32_t len = f->get_32();

			Vector<float> array;
			array.resize(len);
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
		case VariantBinV3::Type::VARIANT_FLOAT64_ARRAY: {
			uint32_t len = f->get_32();

			Vector<double> array;
			array.resize(len);
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
		case VariantBinV3::Type::VARIANT_STRING_ARRAY: {
			uint32_t len = f->get_32();
			Vector<String> array;
			array.resize(len);
			String *w = array.ptrw();
			for (uint32_t i = 0; i < len; i++) {
				w[i] = get_unicode_string();
			}

			r_v = array;

		} break;
		case VariantBinV3::Type::VARIANT_VECTOR2_ARRAY: {
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
		case VariantBinV3::Type::VARIANT_VECTOR3_ARRAY: {
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
		case VariantBinV3::Type::VARIANT_COLOR_ARRAY: {
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

	return OK; //never reach anyway
}


void ResourceLoaderBinaryCompat::_advance_padding(uint32_t p_len) {
	uint32_t extra = 4 - (p_len % 4);
	if (extra < 4) {
		for (uint32_t i = 0; i < extra; i++) {
			f->get_8(); //pad to 32
		}
	}
}

String ResourceLoaderBinaryCompat::_write_resources(void *ud, const RES &p_resource) {
	ResourceLoaderBinaryCompat *rsi = (ResourceLoaderBinaryCompat *)ud;
	return rsi->_write_resource(p_resource);
}

String ResourceLoaderBinaryCompat::_write_resource(const RES &res) {
	if (res->is_class("DummyExtResource")){
		Ref<DummyExtResource> dummy = res;
		return "ExtResource( " + itos(dummy->get_id() + 1) + " )";
	} else {
		for (int i = 0; i < external_resources.size(); i++){
			if (external_resources[i].path == res->get_path()){
				return "ExtResource( " + itos(i+1) + " )";
			}
		}
		
		if (internal_index_cache.has(res->get_path())) {
			return "SubResource( " + itos(res->get_subindex()) + " )";
		} else {
			ERR_FAIL_V_MSG("null", "Resource was not pre cached for the resource section, bug?");
			//internal resource
		}
	}
}

Error ResourceLoaderBinaryCompat::save_as_text(const String &dest_path, uint32_t p_flags) {
	RES p_resource = resource;
	Ref<PackedScene> packed_scene;
	if (dest_path.ends_with(".tscn") || dest_path.ends_with(".escn")) {
		packed_scene = p_resource;
	}

	Error err;
	f = FileAccess::open(dest_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err, ERR_CANT_OPEN, "Cannot save file '" + dest_path + "'.");
	FileAccessRef _fref(f);
	// do something with this path??
	String local_path = dest_path;

	bool relative_paths = p_flags & ResourceSaver::FLAG_RELATIVE_PATHS;
	bool skip_editor = p_flags & ResourceSaver::FLAG_OMIT_EDITOR_PROPERTIES;
	bool bundle_resources = p_flags & ResourceSaver::FLAG_BUNDLE_RESOURCES;
	bool takeover_paths = p_flags & ResourceSaver::FLAG_REPLACE_SUBRESOURCE_PATHS;

	int FORMAT_VERSION = 1;
	if (engine_ver_major > 2){
		FORMAT_VERSION= 3;
	}
	if (!dest_path.begins_with("res://")) {
		takeover_paths = false;
	}

	// save resources
	{
		String title = packed_scene.is_valid() ? "[gd_scene " : "[gd_resource ";
		if (packed_scene.is_null()) {
			title += "type=\"" + p_resource->get_class() + "\" ";
		}
		int load_steps = internal_resources.size() + external_resources.size();

		if (load_steps > 1) {
			title += "load_steps=" + itos(load_steps) + " ";
		}
		title += "format=" + itos(FORMAT_VERSION) + "";

		f->store_string(title);
		f->store_line("]\n"); //one empty line
	}

	for (int i = 0; i < external_resources.size(); i++) {
		String p = external_resources[i].path;
		f->store_string("[ext_resource path=\"" + p + "\" type=\"" + external_resources[i].type + "\" id=" + itos(i + 1) + "]\n"); //bundled
	}

	if (external_resources.size()) {
		f->store_line(String()); //separate
	}

	Set<int> used_indices;

	for (int i = 0; i < internal_resources.size(); i++) {

		RES res = internal_index_cache[internal_resources[i].path];
		//ERR_CONTINUE(!resource_set.has(res));
		bool main = i == (internal_resources.size() - 1);

		if (main && packed_scene.is_valid()) {
			break; //save as a scene
		}

		if (main) {
			f->store_line("[resource]");
		} else {
			String line = "[sub_resource ";
			int idx = res->get_subindex();
			String type = res->get_class();
			if (typeV3toV2Renames.has(type)){
				type = typeV3toV2Renames[type];
			}
			line += "type=\"" + type + "\" id=" + itos(idx);
			f->store_line(line + "]");
			if (takeover_paths) {
				res->set_path(dest_path + "::" + itos(idx), true);
			}
		}

		List<PropertyInfo> property_list;
		res->get_property_list(&property_list);
		//property_list.sort();
		for (List<PropertyInfo>::Element *PE = property_list.front(); PE; PE = PE->next()) {
			if (skip_editor && PE->get().name.begins_with("__editor")) {
				continue;
			}

			if (PE->get().usage & PROPERTY_USAGE_STORAGE) {
				String name = PE->get().name;
				Variant value;
				value = res->get(name);
				Variant default_value = ClassDB::class_get_default_property_value(res->get_class(), name);

				if (default_value.get_type() != Variant::NIL && bool(Variant::evaluate(Variant::OP_EQUAL, value, default_value))) {
					continue;
				}

				if (PE->get().type == Variant::OBJECT && value.is_zero() && !(PE->get().usage & PROPERTY_USAGE_STORE_IF_NULL)) {
					continue;
				}

				String vars;
				VariantWriterCompat::write_to_string(value, vars, engine_ver_major, _write_resources, this);
				f->store_string(name.property_name_encode() + " = " + vars + "\n");
			}
		}

		if (i < internal_resources.size() - 1) {
			f->store_line(String());
		}
	}

	if (packed_scene.is_valid()) {
		//if this is a scene, save nodes and connections!
		Ref<SceneState> state = packed_scene->get_state();
		for (int i = 0; i < state->get_node_count(); i++) {
			StringName type = state->get_node_type(i);
			StringName name = state->get_node_name(i);
			int index = state->get_node_index(i);
			NodePath path = state->get_node_path(i, true);
			NodePath owner = state->get_node_owner_path(i);
			Ref<PackedScene> instance = state->get_node_instance(i);
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

			f->store_string(header);

			if (instance_placeholder != String()) {
				String vars;
				f->store_string(" instance_placeholder=");
				VariantWriterCompat::write_to_string(instance_placeholder, vars, engine_ver_major, _write_resources, this);
				f->store_string(vars);
			}

			if (instance.is_valid()) {
				String vars;
				f->store_string(" instance=");
				VariantWriterCompat::write_to_string(instance, vars, engine_ver_major, _write_resources, this);
				f->store_string(vars);
			}

			f->store_line("]");

			for (int j = 0; j < state->get_node_property_count(i); j++) {
				String vars;
				VariantWriterCompat::write_to_string(state->get_node_property_value(i, j), vars,2, _write_resources, this);

				f->store_string(String(state->get_node_property_name(i, j)).property_name_encode() + " = " + vars + "\n");
			}

			if (i < state->get_node_count() - 1) {
				f->store_line(String());
			}
		}

		for (int i = 0; i < state->get_connection_count(); i++) {
			if (i == 0) {
				f->store_line("");
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
			f->store_string(connstr);
			if (binds.size()) {
				String vars;
				VariantWriterCompat::write_to_string(binds, vars, engine_ver_major, _write_resources, this);
				f->store_string(" binds= " + vars);
			}

			f->store_line("]");
		}

		Vector<NodePath> editable_instances = state->get_editable_instances();
		for (int i = 0; i < editable_instances.size(); i++) {
			if (i == 0) {
				f->store_line("");
			}
			f->store_line("[editable path=\"" + editable_instances[i].operator String() + "\"]");
		}
	}

	if (f->get_error() != OK && f->get_error() != ERR_FILE_EOF) {
		f->close();
		return ERR_CANT_CREATE;
	}

	f->close();
	//memdelete(f);

	return OK;
}
