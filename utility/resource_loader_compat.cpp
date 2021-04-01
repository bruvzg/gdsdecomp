#include "resource_loader_compat.h"
#include "core/io/file_access_compressed.h"
#include "variant_writer_compat.h"
#include "core/string/ustring.h"
#include "image_parser_v2.h"
#include "godot3_export.h"
#include "core/version.h"
#include "core/os/dir_access.h"
#include "core/variant/variant_parser.h"
#include "core/crypto/crypto_core.h"
#include "gdre_packed_data.h"

Error ResourceFormatLoaderBinaryCompat::convert_bin_to_txt(const String &p_path, const String &dst, const String &output_dir , float *r_progress){
	Error error = OK;
	String dst_path = dst;
	//Relative path
	if (!output_dir.is_empty()) {
		dst_path = output_dir.plus_file(dst.replace_first("res://", ""));
	}
	ResourceLoaderBinaryCompat * loader = _open(p_path, output_dir, true, &error, r_progress);
	ERR_RFLBC_COND_V_MSG_CLEANUP(error != OK, error, "Cannot open resource '" + p_path + "'.", loader);

	error = loader->fake_load();
	ERR_RFLBC_COND_V_MSG_CLEANUP(error != OK, error, "Cannot load resource '" + p_path + "'.", loader);

	error = loader->save_as_text_unloaded(dst_path);

	memdelete(loader);
	ERR_FAIL_COND_V_MSG(error != OK, error, "failed to save resource '" + p_path + "' as '"+ dst +"'.");
	return OK;
}

Error ResourceFormatLoaderBinaryCompat::convert_v2tex_to_png(const String &p_path, const String &dst, const String &output_dir, const bool rewrite_metadata, float *r_progress) {
	Error error = OK;
	String rel_dst_path = dst.replace_first("res://", "");
	String dst_path = dst;
	//Relative path
	if (!output_dir.is_empty() && !dst_path.is_abs_path()) {
		dst_path = output_dir.plus_file(dst.replace_first("res://", ""));
	}
	ResourceLoaderBinaryCompat * loader = _open(p_path, output_dir, true, &error, r_progress);
	ERR_RFLBC_COND_V_MSG_CLEANUP(error != OK, error, "Cannot open file '" + p_path + "'.", loader);
	
	loader->convert_v2image_indexed = true;
	loader->hacks_for_deprecated_v2img_formats = false;
	error = loader->fake_load();
	ERR_RFLBC_COND_V_MSG_CLEANUP(error != OK, error, "Cannot load resource '" + p_path + "'.", loader);
	
	Ref<Image> img;
	String name;
	
	//Load the main resource, which should be the ImageTexture
	List<ResourceProperty> lrp = loader->internal_index_cached_properties[loader->res_path];
	for (List<ResourceProperty>::Element *PE = lrp.front(); PE; PE = PE->next()) {
		ResourceProperty pe = PE->get();
		if (pe.name == "resource/name") {
			name = pe.value;
		}
		else if (pe.name == "image") {
			img = pe.value;
		}
	}
	ERR_RFLBC_COND_V_MSG_CLEANUP(img.is_null(), error, "failed to load image from '" + p_path + "'.", loader);

	error = img->save_png(dst_path);
	ERR_RFLBC_COND_V_MSG_CLEANUP(error != OK, error, "failed to save resource '" + p_path + "' as '" + dst + "'.", loader);

	if (rewrite_metadata){
		error = _rewrite_import_metadata(loader, name, rel_dst_path);
	}
	memdelete(loader);
	ERR_FAIL_COND_V_MSG(error == ERR_UNAVAILABLE, error, "Unable to read import metadata, did not save new import metadata");
	ERR_FAIL_COND_V_MSG(error != OK, error, "failed to save resource '" + p_path + "' with rewritten import metadata.");
	return OK;
}

Error get_md5_hash(const String &path, String &hash_str){
	FileAccess *file = FileAccessGDRE::open(path, FileAccess::READ);
	if (!file){
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

Error ResourceFormatLoaderBinaryCompat::_rewrite_import_metadata(ResourceLoaderBinaryCompat * loader, const String &name, const String& rel_dst_path){
	bool rewrite_md5 = true;
	//Rewrite the metadata
	if(!loader->imd.is_valid() || loader->imd->get_source_count() <= 0){
		return ERR_UNAVAILABLE;
	}
	if (loader->imd->get_source_count() > 1){
		WARN_PRINT("multiple import sources detected!?");
	}
	int i;
	for (i = 0; i < loader->imd->get_source_count(); i++) {
		// case insensitive windows paths...
		if (loader->imd->get_source_path(i).get_file().to_lower() == name.to_lower()){
			String md5 = loader->imd->get_source_md5(i);
			// We won't recalculate here, to force a re-import upon load
			if (rewrite_md5){
				String dst_path = rel_dst_path;
				if (!loader->project_dir.is_empty()){
					dst_path = loader->project_dir.plus_file(rel_dst_path);
				}
				String new_hash;
				if (get_md5_hash(dst_path, new_hash) != OK){
					WARN_PRINT("Can't open exported file to calculate hash");
				} else {
					md5 = new_hash;
				}
			}
			loader->imd->remove_source(i);
			loader->imd->add_source(rel_dst_path, md5);
			break;
		}
	}
	// Did not find the file name in the metadata sources
	if (i == loader->imd->get_source_count()) {
		return ERR_CANT_RESOLVE;
	}
	return loader->save_to_bin(loader->res_path + ".tmp");
}

ResourceLoaderBinaryCompat * ResourceFormatLoaderBinaryCompat::_open(const String &p_path, const String &base_dir, bool no_ext_load, Error *r_error, float *r_progress){

	Error error = OK;
	String path = p_path;
	if (!base_dir.is_empty()) {
		path = base_dir.plus_file(p_path.replace_first("res://", ""));
	}
	FileAccess *f = FileAccessGDRE::open(path, FileAccess::READ, &error);
	
	// try it again with the local path
	// TODO: Don't do this
	if (!f){
		f = FileAccessGDRE::open(p_path, FileAccess::READ, &error);
	}
	if (r_error) {
		*r_error = error;
	}

	ERR_FAIL_COND_V_MSG(!f, nullptr, "Cannot open file '" + path + "'.");
	
	String path = p_path;
	if (!base_dir.is_empty()) {
		path = base_dir.plus_file(p_path.replace_first("res://", ""));
	}
	ResourceLoaderBinaryCompat *loader = memnew(ResourceLoaderBinaryCompat);
	loader->project_dir = base_dir;
	loader->progress = r_progress;
	loader->no_ext_load = no_ext_load;
	loader->local_path = p_path; // Local path
	
	// TODO: make this cleaner, support other cases like "user://"(?)
	if (!p_path.begins_with("res://")){
		loader->local_path = "res://" + p_path;
	}
	loader->res_path = path; // Absolute path
	//loader.set_local_path( Globals::get_singleton()->localize_path(p_path) );
	error = loader->open(f);
	if (r_error) {
		*r_error = error;
	}

	if (error != OK){
		ERR_FAIL_COND_V_MSG(error != OK, loader, "Cannot load resource '" + path + "'.");
	}

	return loader;
}

Error ResourceFormatLoaderBinaryCompat::get_import_info(const String &p_path, const String &base_dir, Ref<ImportInfo> &i_info) {
	Error error = OK;
	//Relative path
	ResourceLoaderBinaryCompat * loader = _open(p_path, base_dir, true, &error, nullptr);
	ERR_RFLBC_COND_V_MSG_CLEANUP(error != OK, error, "failed to open resource '" + p_path + "'.", loader);

	if (i_info == nullptr){
		i_info.instance();
	}

	i_info->import_path = loader->local_path;
	i_info->type = loader->type;
	i_info->version = loader->engine_ver_major;
	if (loader->engine_ver_major == 2){
		i_info->dest_files.push_back(loader->local_path);
		i_info->import_md_path = loader->res_path;
		//these do not have any metadata info in them
		if(i_info->import_path.find(".converted.") != -1)
		{
			memdelete(loader);
			return OK;
		}
		Error error = loader->load_import_metadata();
		ERR_RFLBC_COND_V_MSG_CLEANUP(error != OK, ERR_PRINTER_ON_FIRE, "failed to get metadata for '" + loader->res_path + "'",loader);
		
		if (loader->imd->get_source_count() > 1){
			WARN_PRINT("More than one source?!?!");
		}
		ERR_RFLBC_COND_V_MSG_CLEANUP(loader->imd->get_source_count() == 0, ERR_FILE_CORRUPT, "metadata corrupt for '" + loader->res_path + "'", loader);
		i_info->v2metadata = loader->imd;
		i_info->source_file = loader->imd->get_source_path(0);
		i_info->importer = loader->imd->get_editor();
		i_info->params = loader->imd->get_options_as_dictionary();
		i_info->import_data = loader->imd->get_as_dictionary();
	}
	memdelete(loader);
	return OK;
}


Error ResourceLoaderBinaryCompat::load_import_metadata() {
	if (!f) {
		return ERR_CANT_ACQUIRE_RESOURCE;
	}
	if (importmd_ofs == 0) {
		return ERR_UNAVAILABLE;
	}
	if (imd.is_null()){
		imd.instance();
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


Error ResourceLoaderBinaryCompat::open(FileAccess *p_f) {
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
	
	f->set_endian_swap(big_endian != 0); //read big endian if saved as big endian
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
	ERR_FAIL_COND_V_MSG(ver_format > 4, ERR_FILE_UNRECOGNIZED, 
				"Unsupported binary resource format '" + res_path + "'.");
	

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

void ResourceLoaderBinaryCompat::debug_print_properties(String res_name, String res_type, List<ResourceProperty> lrp){
	String valstring;
	print_bl("Resource name: " + res_name);
	print_bl("type: " + res_type);
	for (List<ResourceProperty>::Element *PE = lrp.front(); PE; PE = PE->next()) {
		ResourceProperty pe = PE->get();
		String vars;
		VariantWriterCompat::write_to_string(pe.value, vars, engine_ver_major, _write_fake_resources, this);
		Vector<String> vstrs = vars.split("\n");
		for (int i = 0; i < vstrs.size(); i++){
			print_bl(vstrs[i]);
		}
	}
}

Error ResourceLoaderBinaryCompat::fake_load(){
	if (error != OK) {
		return error;
	}

	int stage = 0;
	Vector<String> lines;
	for (int i = 0; i < external_resources.size(); i++) {
		// no_ext_load
		set_dummy_ext(i);
		stage++;
	}

	// We don't instance the internal resources here; We instead store the name, type and properties
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

			error = parse_variant(value);
			if (error) {
				return error;
			}
			ResourceProperty rp;
			rp.name = name;
			rp.value = value;
			rp.type = value.get_type();
			lrp.push_back(rp);
		}
		internal_index_cached_properties[path] = lrp;
		internal_index_cache[path] = make_dummy(path, rtype, subindex);
		
		// packed scenes with instances for nodes won't work right without creating an instance of it
		if (main && type == "PackedScene"){
			Ref<PackedScene> ps;
			ps.instance();
			String valstring;
			// Debug print
			// debug_print_properties(path, rtype, lrp);
			ps->set(lrp.front()->get().name, lrp.front()->get().value);
			resource = ps;
		}

		stage++;

		if (progress) {
			*progress = (i + 1) / float(internal_resources.size());
		}

		if (main) {
			// Get the import metadata, if we're able to
			if (engine_ver_major == 2){
				Error limperr = load_import_metadata();
				if (limperr != OK && limperr != ERR_UNAVAILABLE){
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

void ResourceLoaderBinaryCompat::advance_padding(FileAccess * f, uint32_t p_len) {
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
	FileAccess * wf = FileAccess::open(dest_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err != OK, ERR_CANT_OPEN, "Cannot save file '" + dest_path + "'.");

	String main_res_path = internal_resources[internal_resources.size()-1].path;
	String main_type = internal_type_cache[main_res_path];
	int text_format_version = 1;
	if (engine_ver_major > 2){
		text_format_version= 2;
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

		wf->store_string(title);
		wf->store_line("]\n"); //one empty line
	}

	for (int i = 0; i < external_resources.size(); i++) {
		String p = external_resources[i].path;
		wf->store_string("[ext_resource path=\"" + p + "\" type=\"" + external_resources[i].type + "\" id=" + itos(i + 1) + "]\n"); //bundled
	}

	if (external_resources.size()) {
		wf->store_line(String()); //separate
	}

	for (int i = 0; i < internal_resources.size(); i++) {
		String path = internal_resources[i].path;
		bool main = i == (internal_resources.size() - 1);

		if (main && is_scene) {
			break; //save as a scene
		}

		if (main) {
			wf->store_line("[resource]");
		} else {
			String line = "[sub_resource ";
			String type = internal_type_cache[path];
			int idx = internal_index_cache[path]->get_subindex();
			line += "type=\"" + type + "\" id=" + itos(idx) + "]";
			if (text_format_version == 1){
				// Godot 2.x quirk: newline between subresource and properties
				line+="\n";
			}
			wf->store_line(line);
		}

		List<ResourceProperty> properties = internal_index_cached_properties[path];
		for (List<ResourceProperty>::Element *PE = properties.front(); PE; PE = PE->next()) {
			ResourceProperty pe = PE->get();
			String vars;
			VariantWriterCompat::write_to_string(pe.value, vars, engine_ver_major, _write_fake_resources, this);
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
				VariantWriterCompat::write_to_string(instance_placeholder, vars, engine_ver_major, _write_fake_resources, this);
				wf->store_string(vars);
			}

			if (instance.is_valid()) {
				String vars;
				wf->store_string(" instance=");
				VariantWriterCompat::write_to_string(instance, vars, engine_ver_major, _write_fake_resources, this);
				wf->store_string(vars);
			}

			wf->store_line("]");
			if (text_format_version == 1 && state->get_node_property_count(i) != 0) {
				// Godot 2.x quirk: newline between header and properties
				wf->store_line("");
			}

			for (int j = 0; j < state->get_node_property_count(i); j++) {
				String vars;
				VariantWriterCompat::write_to_string(state->get_node_property_value(i, j), vars,2, _write_fake_resources, this);

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
				VariantWriterCompat::write_to_string(binds, vars, engine_ver_major, _write_fake_resources, this);
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
			Quat v;
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
			if (ImageParserV2::parse_image_v2(f, r_v, hacks_for_deprecated_v2img_formats, convert_v2image_indexed) != OK){
				WARN_PRINT(String("Couldn't load resource: embedded image").utf8().get_data());
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
				subnames.remove(subnames.size() - 1);
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
					String path = res_path + "::" + itos(index);

					//always use internal cache for loading internal resources
					if (!internal_index_cache.has(path)) {
						WARN_PRINT(String("Couldn't load resource (no cache): " + path).utf8().get_data());
						r_v = Variant();
					} else {
						r_v = internal_index_cache[path];
					}

				} break;
				case VariantBin::OBJECT_EXTERNAL_RESOURCE: {
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
				case VariantBin::OBJECT_EXTERNAL_RESOURCE_INDEX: {
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

		// Old Godot 2.x InputEvent variant, should never encounter these
		// They were never saved into the binary resource files.
		case VariantBin::VARIANT_INPUT_EVENT: {
				WARN_PRINT(String("Encountered a Input event variant?!?!?").utf8().get_data());
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
			if (len == 0){
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
			if (len == 0){
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
			if (len == 0){
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
			if (len == 0){
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

	return OK; //never reach anyway
}

void ResourceLoaderBinaryCompat::save_ustring(FileAccess * f, const String &p_string){
	CharString utf8 = p_string.utf8();
	f->store_32(utf8.length() + 1);
	f->store_buffer((const uint8_t *)utf8.get_data(), utf8.length() + 1);
}

void ResourceLoaderBinaryCompat::save_unicode_string(const String &p_string) {
	CharString utf8 = p_string.utf8();
	f->store_32(utf8.length() + 1);
	f->store_buffer((const uint8_t *)utf8.get_data(), utf8.length() + 1);
}

Error ResourceLoaderBinaryCompat::write_variant_bin(FileAccess *f,
													const Variant &p_property,
													Map<String, RES> internal_index_cache,
													Vector<IntResource> &internal_resources,
													Vector<ExtResource> &external_resources,
													Vector<StringName> &string_map,
													const uint32_t ver_format,
													const PropertyInfo &p_hint) {
	
	switch (p_property.get_type()) {

		case Variant::NIL: {

			f->store_32(VariantBin::VARIANT_NIL);
			// don't store anything
		} break;
		case Variant::BOOL: {

			f->store_32(VariantBin::VARIANT_BOOL);
			bool val = p_property;
			f->store_32(val);
		} break;
		case Variant::INT: {

			f->store_32(VariantBin::VARIANT_INT);
			int val = p_property;
			f->store_32(val);
		} break;
		case Variant::FLOAT: {

			f->store_32(VariantBin::VARIANT_REAL);
			real_t val = p_property;
			f->store_real(val);

		} break;
		case Variant::STRING: {

			f->store_32(VariantBin::VARIANT_STRING);
			String val = p_property;
			save_ustring(f, val);

		} break;
		case Variant::VECTOR2: {

			f->store_32(VariantBin::VARIANT_VECTOR2);
			Vector2 val = p_property;
			f->store_real(val.x);
			f->store_real(val.y);

		} break;
		case Variant::RECT2: {

			f->store_32(VariantBin::VARIANT_RECT2);
			Rect2 val = p_property;
			f->store_real(val.position.x);
			f->store_real(val.position.y);
			f->store_real(val.size.x);
			f->store_real(val.size.y);

		} break;
		case Variant::VECTOR3: {

			f->store_32(VariantBin::VARIANT_VECTOR3);
			Vector3 val = p_property;
			f->store_real(val.x);
			f->store_real(val.y);
			f->store_real(val.z);

		} break;
		case Variant::PLANE: {

			f->store_32(VariantBin::VARIANT_PLANE);
			Plane val = p_property;
			f->store_real(val.normal.x);
			f->store_real(val.normal.y);
			f->store_real(val.normal.z);
			f->store_real(val.d);

		} break;
		case Variant::QUAT: {

			f->store_32(VariantBin::VARIANT_QUAT);
			Quat val = p_property;
			f->store_real(val.x);
			f->store_real(val.y);
			f->store_real(val.z);
			f->store_real(val.w);

		} break;
		case Variant::AABB: {

			f->store_32(VariantBin::VARIANT_AABB);
			AABB val = p_property;
			f->store_real(val.position.x);
			f->store_real(val.position.y);
			f->store_real(val.position.z);
			f->store_real(val.size.x);
			f->store_real(val.size.y);
			f->store_real(val.size.z);

		} break;
		case Variant::TRANSFORM2D: {

			f->store_32(VariantBin::VARIANT_MATRIX32);
			Transform2D val = p_property;
			f->store_real(val.elements[0].x);
			f->store_real(val.elements[0].y);
			f->store_real(val.elements[1].x);
			f->store_real(val.elements[1].y);
			f->store_real(val.elements[2].x);
			f->store_real(val.elements[2].y);

		} break;
		case Variant::BASIS: {

			f->store_32(VariantBin::VARIANT_MATRIX3);
			Basis val = p_property;
			f->store_real(val.elements[0].x);
			f->store_real(val.elements[0].y);
			f->store_real(val.elements[0].z);
			f->store_real(val.elements[1].x);
			f->store_real(val.elements[1].y);
			f->store_real(val.elements[1].z);
			f->store_real(val.elements[2].x);
			f->store_real(val.elements[2].y);
			f->store_real(val.elements[2].z);

		} break;
		case Variant::TRANSFORM: {

			f->store_32(VariantBin::VARIANT_TRANSFORM);
			Transform val = p_property;
			f->store_real(val.basis.elements[0].x);
			f->store_real(val.basis.elements[0].y);
			f->store_real(val.basis.elements[0].z);
			f->store_real(val.basis.elements[1].x);
			f->store_real(val.basis.elements[1].y);
			f->store_real(val.basis.elements[1].z);
			f->store_real(val.basis.elements[2].x);
			f->store_real(val.basis.elements[2].y);
			f->store_real(val.basis.elements[2].z);
			f->store_real(val.origin.x);
			f->store_real(val.origin.y);
			f->store_real(val.origin.z);

		} break;
		case Variant::COLOR: {

			f->store_32(VariantBin::VARIANT_COLOR);
			Color val = p_property;
			f->store_real(val.r);
			f->store_real(val.g);
			f->store_real(val.b);
			f->store_real(val.a);

		} break;
		case Variant::NODE_PATH: {
			f->store_32(VariantBin::VARIANT_NODE_PATH);
			NodePath np = p_property;
			f->store_16(np.get_name_count());
			uint16_t snc = np.get_subname_count();
			// If ver_format 1-2 (i.e. godot 2.x)
			int property_idx = -1;
			if (ver_format < VariantBin::FORMAT_VERSION_NO_NODEPATH_PROPERTY) {
				// If there is a property, decrement the subname counter and store the property idx.
				if (np.get_subname_count() > 1 && 
						String(np.get_concatenated_subnames()).split(":").size() >= 2){
						property_idx = np.get_subname_count() - 1;
						snc--;
				}
			}

			if (np.is_absolute()){
				f->store_16(snc | 0x8000);
			} else {
				f->store_16(snc);
			}
			
			for (int i = 0; i < np.get_name_count(); i++)
				f->store_32(string_map.find(np.get_name(i)));
			// store all subnames minus any property fields if need be
			for (int i = 0; i < snc; i++)
				f->store_32(string_map.find(np.get_subname(i)));
			// If ver_format 1-2 (i.e. godot 2.x)
			if (ver_format < VariantBin::FORMAT_VERSION_NO_NODEPATH_PROPERTY) {
				// If there's a property, store it
				if (property_idx > -1){
					f->store_32(string_map.find(np.get_subname(property_idx)));
				// 0x80000000 will resolve to a zero length string in the binary parser for any version
				} else {
					uint32_t zlen = 0x80000000;
					f->store_32(zlen);
				}
			}
		} break;
		case Variant::RID: {
			f->store_32(VariantBin::VARIANT_RID);
			WARN_PRINT("Can't save RIDs");
			RID val = p_property;
			f->store_32(val.get_id());
		} break;
		case Variant::OBJECT: {
			RES res = p_property;
			// This will only be triggered on godot 2.x, where the image variant is loaded as an image object vs. a resource
			if (ver_format == 1 && res->is_class("Image")){
				f->store_32(VariantBin::VARIANT_IMAGE);
				ImageParserV2::write_v2image_to_bin(f, p_property, PROPERTY_HINT_IMAGE_COMPRESS_LOSSLESS);
			} else {
				f->store_32(VariantBin::VARIANT_OBJECT);
				if (res.is_null()) {
					f->store_32(VariantBin::OBJECT_EMPTY);
					return OK; // don't save it
				}
				String rpath;
				if (res->is_class("FakeResource")){
					Ref<FakeResource> fr = res;
					rpath = fr->get_real_path();
				} else {
					rpath = res->get_path();
				}
				if (rpath.length() && rpath.find("::") == -1) {
					int idx = -1;
					for (int i = 0; i < external_resources.size(); i++){
						if (external_resources[i].path == rpath){
							idx = i+1;
							break;
						}
					}
					if (idx == -1){
						f->store_32(VariantBin::OBJECT_EMPTY);
						ERR_FAIL_COND_V_MSG(idx == -1, ERR_BUG, "Cannot find external resource");
					}
					
					f->store_32(VariantBin::OBJECT_EXTERNAL_RESOURCE_INDEX);
					f->store_32(idx);
				} else {

					if (!internal_index_cache.has(rpath)) {
						f->store_32(VariantBin::OBJECT_EMPTY);
						ERR_FAIL_V_MSG(ERR_BUG, "Resource was not pre cached for the resource section, bug?");
					}

					f->store_32(VariantBin::OBJECT_INTERNAL_RESOURCE);
					f->store_32(res->get_subindex());
				}
			}
		} break;
		case Variant::DICTIONARY: {

			f->store_32(VariantBin::VARIANT_DICTIONARY);
			Dictionary d = p_property;
			f->store_32(uint32_t(d.size()) | (p_property.is_shared() ? 0x80000000 : 0));

			List<Variant> keys;
			d.get_key_list(&keys);

			for (List<Variant>::Element *E = keys.front(); E; E = E->next()) {

				//if (!_check_type(dict[E->get()]))
				//	continue;

				write_variant_bin(f, E->get(), internal_index_cache, internal_resources, external_resources, string_map, ver_format);
				write_variant_bin(f, d[E->get()], internal_index_cache, internal_resources, external_resources, string_map, ver_format);
			}

		} break;
		case Variant::ARRAY: {

			f->store_32(VariantBin::VARIANT_ARRAY);
			Array a = p_property;
			f->store_32(uint32_t(a.size()) | (p_property.is_shared() ? 0x80000000 : 0));
			for (int i = 0; i < a.size(); i++) {
				write_variant_bin(f, a[i], internal_index_cache, internal_resources, external_resources, string_map, ver_format);
			}

		} break;
		case Variant::PACKED_BYTE_ARRAY: {

			f->store_32(VariantBin::VARIANT_RAW_ARRAY);
			Vector<uint8_t> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			f->store_buffer(arr.ptr(), len);
			advance_padding(f, len);

		} break;
		case Variant::PACKED_INT32_ARRAY: {

			f->store_32(VariantBin::VARIANT_INT_ARRAY);
			Vector<int> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			for (int i = 0; i < len; i++)
				f->store_32(arr.ptr()[i]);

		} break;
		case Variant::PACKED_FLOAT32_ARRAY: {

			f->store_32(VariantBin::VARIANT_REAL_ARRAY);
			Vector<real_t> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			for (int i = 0; i < len; i++) {
				f->store_real(arr.ptr()[i]);
			}

		} break;
		case Variant::PACKED_STRING_ARRAY: {

			f->store_32(VariantBin::VARIANT_STRING_ARRAY);
			Vector<String> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			for (int i = 0; i < len; i++) {
				save_ustring(f, arr.ptr()[i]);
			}

		} break;
		case Variant::PACKED_VECTOR3_ARRAY: {

			f->store_32(VariantBin::VARIANT_VECTOR3_ARRAY);
			Vector<Vector3> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			
			for (int i = 0; i < len; i++) {
				f->store_real(arr.ptr()[i].x);
				f->store_real(arr.ptr()[i].y);
				f->store_real(arr.ptr()[i].z);
			}

		} break;
		case Variant::PACKED_VECTOR2_ARRAY: {

			f->store_32(VariantBin::VARIANT_VECTOR2_ARRAY);
			Vector<Vector2> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			for (int i = 0; i < len; i++) {
				f->store_real(arr.ptr()[i].x);
				f->store_real(arr.ptr()[i].y);
			}

		} break;
		case Variant::PACKED_COLOR_ARRAY: {

			f->store_32(VariantBin::VARIANT_COLOR_ARRAY);
			Vector<Color> arr = p_property;
			int len = arr.size();
			f->store_32(len);
			for (int i = 0; i < len; i++) {
				f->store_real(arr.ptr()[i].r);
				f->store_real(arr.ptr()[i].g);
				f->store_real(arr.ptr()[i].b);
				f->store_real(arr.ptr()[i].a);
			}

		} break;
		default: {
			ERR_FAIL_V_MSG(ERR_CANT_CREATE,"Invalid variant type");
		}
	}
	return OK;
}
Error ResourceLoaderBinaryCompat::write_variant_bin( FileAccess * fa, const Variant &p_property, const PropertyInfo &p_hint) {
	return write_variant_bin(fa, p_property, internal_index_cache, internal_resources, external_resources, string_map, ver_format, p_hint);
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

	ERR_FAIL_COND_V(err, err);
		
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
		fw->set_endian_swap(true);
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
	for (int i = 0; i < 14; i++)
		fw->store_32(0); // reserved


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
		IntResource r = internal_resources[i];
		int subidx = internal_index_cache[r.path]->get_subindex();
		if (r.path == "" || r.path.find("::") != -1) {
			save_ustring(fw, "local://" + itos(subidx));
		} else if (r.path == res_path) {
			save_ustring(fw, local_path); //actual external
		}
		ofs_pos.push_back(fw->get_position());
		fw->store_64(0); //offset in 64 bits
	}

	Vector<uint64_t> ofs_table;
	//	int saved_idx=0;
	//now actually save the resources
	for (int i = 0; i < internal_resources.size(); i++) {
		IntResource r = internal_resources[i];
		int subidx = internal_index_cache[r.path]->get_subindex();
		String rtype = internal_type_cache[r.path];
		List<ResourceProperty> lrp = internal_index_cached_properties[r.path];
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

ResourceLoaderBinaryCompat::ResourceLoaderBinaryCompat(){}
ResourceLoaderBinaryCompat::~ResourceLoaderBinaryCompat(){
	if (f != nullptr){
		memdelete(f);
	}
}