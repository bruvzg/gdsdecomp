#include "resource_loader_compat.h"
#include "core/io/file_access_compressed.h"
#include "variant_writer_compat.h"
#include "core/string/ustring.h"
#include "V2ImageParser.h"
#include "godot3_export.h"
#include "core/version.h"

Error ResourceFormatLoaderBinaryCompat::convert_bin_to_txt(const String &p_path, const String &dst, const String &output_dir , float *r_progress){
	Error error = OK;
	ResourceLoaderBinaryCompat loader = _open(p_path, output_dir, true, &error, r_progress);
	ERR_FAIL_COND_V(error != OK, error);
	error = loader.fake_load();

	ERR_FAIL_COND_V_MSG(error != OK, error, "Cannot load resource '" + p_path + "'.");

	error = loader.save_as_text_unloaded(dst);
	ERR_FAIL_COND_V_MSG(error != OK, error, "failed to save resource '" + p_path + "' as '"+ dst +"'.");
	return OK;
}

ResourceLoaderBinaryCompat ResourceFormatLoaderBinaryCompat::_open(const String &p_path, const String &base_dir, bool no_ext_load, Error *r_error, float *r_progress){
	Error error = OK;
	FileAccess *f = FileAccess::open(p_path, FileAccess::READ, &error);
	if (r_error) {
		*r_error = error;
	}
	ResourceLoaderBinaryCompat loader;

	ERR_FAIL_COND_V_MSG(error != OK, loader, "Cannot open file '" + p_path + "'.");

	loader.progress = r_progress;
	loader.no_ext_load = no_ext_load;
	String path = p_path;
	loader.local_path = p_path;
	loader.res_path = loader.local_path;
	//loader.set_local_path( Globals::get_singleton()->localize_path(p_path) );
	error = loader.open(f);
	if (r_error) {
		*r_error = error;
	}

	if (error != OK){
		if (f && f->is_open()){
			f->close();
			memdelete(f);
		}
		ERR_FAIL_COND_V_MSG(error != OK, loader, "Cannot load resource '" + p_path + "'.");
	}

	return loader;
}


Error ResourceLoaderBinaryCompat::_get_resource_header(FileAccess *f){
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
	return OK;
}

Map<String,String> ResourceLoaderBinaryCompat::_get_file_info(FileAccess *f, Error *r_error){
    Map<String,String> ret;
	
	Error err = _get_resource_header(f);
	if (r_error){
		*r_error = err;
	}
	ERR_FAIL_COND_V(err != OK, ret);

	bool big_endian = f->get_32();
	f->get_32(); // use_real64

	f->set_endian_swap(big_endian != 0); //read big endian if saved as big endian
	ret["ver_major"] = itos(f->get_32());
	ret["ver_minor"] = itos(f->get_32());
	ret["ver_format"] = itos(f->get_32());
	ret["type"] = get_ustring(f);
	return ret;
}

Map<String,String> ResourceLoaderBinaryCompat::get_version_and_type(const String &p_path, Error *r_error){
	Error err;
	Map<String,String> ret;
	FileAccess *f = FileAccess::open(p_path, FileAccess::READ, &err);
	if (r_error){
		*r_error = err;
	}
	ERR_FAIL_COND_V_MSG(err != OK, ret, "Cannot open file '" + p_path + "'.");
	ret = _get_file_info(f, &err);
	if (r_error){
		*r_error = err;
	}

	f->close();
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


Error ResourceLoaderBinaryCompat::open(FileAccess *p_f) {
	error = OK;

	f = p_f;
	error = _get_resource_header(f);
	ERR_FAIL_COND_V(error != OK, error);

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
	if (engine_ver_major < 2 || engine_ver_major > 4){
			error = ERR_FILE_UNRECOGNIZED;
			ERR_FAIL_COND_V_MSG(engine_ver_major < 2 || engine_ver_major > 4, error, 
				"Unsupported engine version used to create resource '" + res_path + "'.");
	}
	if(ver_format == 2 || ver_format > 3){
			error = ERR_FILE_UNRECOGNIZED;
			ERR_FAIL_COND_V_MSG(ver_format < 2 || ver_format > 4, error, 
				"Unsupported binary resource format '" + res_path + "'.");
	}

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
		ERR_FAIL_V_MSG(error, "Premature end of file (EOF): " + local_path + ".");
	}

	return OK;
}

Error ResourceLoaderBinaryCompat::fake_load(){
	if (error != OK) {
		return error;
	}

	int stage = 0;
	Vector<String> lines;
	for (int i = 0; i < external_resources.size(); i++) {
		set_dummy_ext(i);
		stage++;
	}
	

	for (int i = 0; i < internal_resources.size(); i++) {
		bool main = i == (internal_resources.size() - 1);

		String path;
		int subindex = 0;

		if (!main) {
			path = internal_resources[i].path;
			if (path.begins_with("local://")) {
			 	path = path.replace_first("local://", "");
			 	subindex = path.to_int();
			 	path = res_path + "::" + path;
			}
		} else {
			path = res_path;
		}
		internal_resources.write[i].path = path;

		uint64_t offset = internal_resources[i].offset;
		f->seek(offset);

		String rtype = get_unicode_string();
		internal_type_cache[path] = rtype;

		int pc = f->get_32();
		//set properties
		List<ResourceProperty> lrp;
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
			ResourceProperty rp;
			rp.name = name;
			rp.value = value;
			rp.type = value.get_type();
			lrp.push_back(rp);
		}

		if (!main ) {
			internal_index_cached_properties[path] = lrp;
			internal_index_cache[path] = make_dummy(path, rtype, subindex);
		} else {
			// packed scenes with instances for nodes won't work right without creating an instance of it
			if (type == "PackedScene"){
				Ref<PackedScene> ps;
				ps.instance();
				String valstring;
				VariantWriterCompat::write_to_string(lrp.front()->get().value,valstring, engine_ver_major, _write_fake_resources, this);
				Vector<String> vstrs = valstring.split("\n");
				for (int i = 0; i < vstrs.size(); i++){
					print_bl(vstrs[i]);
				}
				
				ps->set(lrp.front()->get().name, lrp.front()->get().value);
				resource = ps;
			} else {
				internal_index_cached_properties[path] = lrp;
				internal_index_cache[path] = make_dummy(path, rtype, subindex);
			}
		}

		stage++;

		if (progress) {
			*progress = (i + 1) / float(internal_resources.size());
		}

		if (main) {
			f->close();
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

RES ResourceLoaderBinaryCompat::set_dummy_ext(const String& path, const String& exttype){
	for (int i = 0; i < external_resources.size(); i++){
		if (external_resources[i].path == path){
			if (external_resources[i].cache.is_valid()){
				return external_resources[i].cache;
			}
			return set_dummy_ext(i);
		}
	}
	//If not found in cache...
	ExtResource er;
	er.path = path;
	er.type = exttype;
	er.cache = make_dummy(path, exttype, external_resources.size()+1);
	external_resources.push_back(er);
	
	return er.cache;
}

RES ResourceLoaderBinaryCompat::make_dummy(const String& path, const String& type, const uint32_t subidx){
	String realtypename = type;
	Ref<FakeResource> dummy;
	dummy.instance();
	dummy->set_real_path(path);
	dummy->set_real_type(type);
	dummy->set_subindex(subidx);
	return dummy;
}

RES ResourceLoaderBinaryCompat::set_dummy_ext(const uint32_t erindex){
	if (external_resources[erindex].cache.is_valid()){
		return external_resources[erindex].cache;
	}
	
	RES dummy = make_dummy(external_resources[erindex].path, external_resources[erindex].type, erindex+1);
	external_resources.write[erindex].cache = dummy;
	
	return dummy;
}

void ResourceLoaderBinaryCompat::_advance_padding(uint32_t p_len) {
	uint32_t extra = 4 - (p_len % 4);
	if (extra < 4) {
		for (uint32_t i = 0; i < extra; i++) {
			f->get_8(); //pad to 32
		}
	}
}

String ResourceLoaderBinaryCompat::_write_fake_resources(void *ud, const RES &p_resource) {
	ResourceLoaderBinaryCompat *rsi = (ResourceLoaderBinaryCompat *)ud;
	return rsi->_write_fake_resource(p_resource);
}

String ResourceLoaderBinaryCompat::_write_fake_resource(const RES &res) {
	ERR_FAIL_COND_V_MSG(!res->is_class("FakeResource"), "null", "A real resource?!?!?!?!");
	Ref<FakeResource> fr = res;
	//internal resource
	if (internal_index_cache.has(fr->get_real_path())) {
		return "SubResource( " + itos(fr->get_subindex()) + " )";
	} else {
		for (int i = 0; i < external_resources.size(); i++){
			if (external_resources[i].path == fr->get_real_path()){
				return "ExtResource( " + itos(fr->get_subindex()) + " )";
			}
		}
	}
	ERR_FAIL_V_MSG("null", "Resource was not pre cached for the resource section, bug?");
}

Error ResourceLoaderBinaryCompat::save_as_text_unloaded(const String &dest_path, uint32_t p_flags) {
	bool is_scene = false;
	Ref<PackedScene> packed_scene;
	if (dest_path.ends_with(".tscn") || dest_path.ends_with(".escn")) {
		is_scene = true;
		packed_scene = resource;
	}

	Error err;
	f = FileAccess::open(dest_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err, ERR_CANT_OPEN, "Cannot save file '" + dest_path + "'.");
	FileAccessRef _fref(f);
	// do something with this path??
	String local_path = dest_path;
	String main_res_path = internal_resources[internal_resources.size()-1].path;
	String main_type = internal_type_cache[main_res_path];
	int FORMAT_VERSION = 1;
	if (engine_ver_major > 2){
		FORMAT_VERSION= 2;
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

	for (int i = 0; i < internal_resources.size(); i++) {
		String path = internal_resources[i].path;
		bool main = i == (internal_resources.size() - 1);

		if (main && is_scene) {
			break; //save as a scene
		}

		if (main) {
			f->store_line("[resource]");
		} else {
			String line = "[sub_resource ";
			String type = internal_type_cache[path];
			int idx = internal_index_cache[path]->get_subindex();
			line += "type=\"" + type + "\" id=" + itos(idx) + "]";
			if (FORMAT_VERSION == 1){
				// Godot 2.x has this particular quirk, don't know why
				line+="\n";
			}
			f->store_line(line);
		}

		List<ResourceProperty> properties = internal_index_cached_properties[path];
		for (List<ResourceProperty>::Element *PE = properties.front(); PE; PE = PE->next()) {
			ResourceProperty pe = PE->get();
			String vars;
			VariantWriterCompat::write_to_string(pe.value, vars, engine_ver_major, _write_fake_resources, this);
			f->store_string(pe.name.property_name_encode() + " = " + vars + "\n");

				// Variant default_value = ClassDB::class_get_default_property_value(res->get_class(), name);
				// if (default_value.get_type() != Variant::NIL && bool(Variant::evaluate(Variant::OP_EQUAL, value, default_value))) {
				// 	continue;
				// }

				// if (PE->get().type == Variant::OBJECT && value.is_zero() && !(PE->get().usage & PROPERTY_USAGE_STORE_IF_NULL)) {
				// 	continue;
				// }
		}

		if (i < internal_resources.size() - 1) {
			f->store_line(String());
		}
	}

	//if this is a scene, save nodes and connections!
	if (is_scene) {
		Ref<SceneState> state = packed_scene->get_state();
		ERR_FAIL_COND_V_MSG(!state.is_valid(), ERR_FILE_CORRUPT, "What the fuck");
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

			f->store_string(header);

			if (instance_placeholder != String()) {
				String vars;
				f->store_string(" instance_placeholder=");
				VariantWriterCompat::write_to_string(instance_placeholder, vars, engine_ver_major, _write_fake_resources, this);
				f->store_string(vars);
			}

			if (instance.is_valid()) {
				String vars;
				f->store_string(" instance=");
				VariantWriterCompat::write_to_string(instance, vars, engine_ver_major, _write_fake_resources, this);
				f->store_string(vars);
			}

			f->store_line("]");
			if (FORMAT_VERSION == 1){
				// Godot 2.x has this particular quirk, don't know why
				header+="\n";
			}

			for (int j = 0; j < state->get_node_property_count(i); j++) {
				String vars;
				VariantWriterCompat::write_to_string(state->get_node_property_value(i, j), vars,2, _write_fake_resources, this);

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
				VariantWriterCompat::write_to_string(binds, vars, engine_ver_major, _write_fake_resources, this);
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

	return OK;
}


Error ResourceLoaderBinaryCompat::parse_variantv2(Variant &r_v) {
	ResourceLoader::set_abort_on_missing_resources(false);
	uint32_t type = f->get_32();
	//print_bl("find property of type: " + itos(type));
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
						r_v = set_dummy_ext(path,exttype);
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
							r_v = set_dummy_ext(erindex);
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
	//print_bl("find property of type: %d", type);

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
						r_v = set_dummy_ext(path,exttype);
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
							r_v = set_dummy_ext(erindex);
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

ResourceLoaderBinaryCompat::ResourceLoaderBinaryCompat(){}
ResourceLoaderBinaryCompat::~ResourceLoaderBinaryCompat(){}
