#include "resource_loader_compat.h"
#include "image_parser_v2.h"
#include "texture_loader_compat.h"
#include "variant_writer_compat.h"

#include "utility/gdre_settings.h"

#include "core/crypto/crypto_core.h"
#include "core/io/dir_access.h"
#include "core/io/file_access_compressed.h"
#include "core/string/ustring.h"
#include "core/version.h"
#include "scene/resources/resource_format_text.h"

Error ResourceFormatLoaderCompat::convert_txt_to_bin(const String &p_path, const String &dst, const String &output_dir, float *r_progress) {
	Error error = OK;
	String dst_path = dst;

	// Relative path

	if (GDRESettings::get_singleton() && !output_dir.is_empty()) {
		if (!(dst.is_absolute_path() && GDRESettings::get_singleton()->is_fs_path(dst))) {
			dst_path = output_dir.path_join(dst.replace_first("res://", ""));
		}
	}

	ResourceLoaderCompat *loader = _open_text(p_path, output_dir, true, &error, r_progress);
	ERR_RFLBC_COND_V_MSG_CLEANUP(error != OK, error, "Cannot open resource '" + p_path + "'.", loader);

	error = loader->fake_load_text();
	ERR_RFLBC_COND_V_MSG_CLEANUP(error != OK, error, "Cannot load resource '" + p_path + "'.", loader);

	error = loader->save_to_bin(dst_path);

	memdelete(loader);
	ERR_FAIL_COND_V_MSG(error != OK, error, "failed to save resource '" + p_path + "' as '" + dst + "'.");
	return OK;
}

Error ResourceFormatLoaderCompat::convert_bin_to_txt(const String &p_path, const String &dst, const String &output_dir, float *r_progress) {
	Error error = OK;
	String dst_path = dst;

	// Relative path
	if (!output_dir.is_empty() && GDRESettings::get_singleton()) {
		dst_path = output_dir.path_join(dst.replace_first("res://", ""));
	}

	ResourceLoaderCompat *loader = _open_bin(p_path, output_dir, true, &error, r_progress);
	ERR_RFLBC_COND_V_MSG_CLEANUP(error != OK, error, "Cannot open resource '" + p_path + "'.", loader);
	// TODO: We need to rewrite ResourceLoaderCompat to allow the use of Double RealT dynamically
	// Right now, just fail
	ERR_RFLBC_COND_V_MSG_CLEANUP(loader->using_real_t_double, ERR_UNAVAILABLE, "Double RealT is not supported in ResourceFormatLoaderCompat.", loader);

	error = loader->load();
	ERR_RFLBC_COND_V_MSG_CLEANUP(error != OK, error, "Cannot load resource '" + p_path + "'.", loader);

	error = loader->save_as_text_unloaded(dst_path);

	memdelete(loader);
	ERR_FAIL_COND_V_MSG(error != OK, error, "failed to save resource '" + p_path + "' as '" + dst + "'.");
	return OK;
}

// This is really only for loading certain resources to view them, and debugging.
// This is not suitable for conversion of resources
Ref<Resource> ResourceFormatLoaderCompat::load(const String &p_path, const String &project_dir, Error *r_error, bool p_use_sub_threads, float *r_progress, CacheMode p_cache_mode) {
	Error err = ERR_FILE_CANT_OPEN;
	if (!r_error) {
		r_error = &err;
	} else {
		*r_error = err;
	}
	if (p_path.get_extension() == "tex" || // 2.x
			p_path.get_extension() == "stex" || // 3.x
			p_path.get_extension() == "ctex") { // 4.x
		TextureLoaderCompat rflct;
		return rflct.load_texture2d(p_path, r_error);
	}
	String local_path = GDRESettings::get_singleton()->localize_path(p_path, project_dir);
	ResourceFormatLoaderCompat::FormatType ftype = recognize(p_path, project_dir);
	ResourceLoaderCompat *loader;
	if (ftype == ResourceFormatLoaderCompat::FormatType::BINARY) {
		loader = _open_bin(p_path, project_dir, false, r_error, r_progress);
		// TODO: We need to rewrite ResourceLoaderCompat to allow the use of Double RealT dynamically
		// Right now, just fail
		if (loader->using_real_t_double) {
			*r_error = ERR_UNAVAILABLE;
			ERR_FAIL_V_MSG(Ref<Resource>(), "Double RealT is not supported in ResourceFormatLoaderCompat.");
		}
	} else if (ftype == ResourceFormatLoaderCompat::FormatType::TEXT) {
		loader = _open_text(p_path, project_dir, false, r_error, r_progress);
	} else {
		ERR_FAIL_V_MSG(Ref<Resource>(), "failed to open resource '" + p_path + "'.");
	}

	ERR_RFLBC_COND_V_MSG_CLEANUP(*r_error != OK, Ref<Resource>(), "Cannot open file '" + p_path + "'.", loader);
	loader->cache_mode = p_cache_mode;
	loader->use_sub_threads = p_use_sub_threads;
	if (loader->engine_ver_major != VERSION_MAJOR) {
		WARN_PRINT_ONCE(p_path + ": Doing real load on incompatible version " + itos(loader->engine_ver_major));
	}
	if (ftype == ResourceFormatLoaderCompat::FormatType::BINARY) {
		*r_error = loader->load();
	} else {
		// Real text loading not implemented yet
		ERR_RFLBC_COND_V_MSG_CLEANUP(true, Ref<Resource>(), "Real loads on text resources not implemented yet: '" + p_path + "'.", loader);
		// *r_error = loader->fake_load_text();
	}

	ERR_RFLBC_COND_V_MSG_CLEANUP(*r_error != OK, Ref<Resource>(), "failed to load resource '" + p_path + "'.", loader);

	Ref<Resource> ret = loader->resource;
	memdelete(loader);
	return ret;
}

Error ResourceFormatLoaderCompat::rewrite_v2_import_metadata(const String &p_path, const String &p_dst, Ref<ResourceImportMetadatav2> imd) {
	Error error = OK;
	float prog;
	auto loader = _open_bin(p_path, "", true, &error, &prog);
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

Error ResourceFormatLoaderCompat::get_import_info(const String &p_path, const String &base_dir, _ResourceInfo &i_info) {
	Error error = OK;
	ResourceLoaderCompat *loader;
	ResourceFormatLoaderCompat::FormatType ftype = recognize(p_path, base_dir);
	if (ftype == ResourceFormatLoaderCompat::FormatType::BINARY) {
		loader = _open_bin(p_path, base_dir, true, &error, nullptr);
	} else if (ftype == ResourceFormatLoaderCompat::FormatType::TEXT) {
		loader = _open_text(p_path, base_dir, true, &error, nullptr);
		i_info.is_text = true;
	} else {
		ERR_FAIL_V_MSG(ERR_FILE_CANT_OPEN, "Could not recognize resource '" + p_path + "'.");
	}
	ERR_RFLBC_COND_V_MSG_CLEANUP(error != OK, error, "Cannot load resource '" + p_path + "'.", loader);
	i_info.type = loader->res_type;
	i_info.ver_major = loader->engine_ver_major;
	i_info.ver_minor = loader->engine_ver_minor;
	if (loader->engine_ver_major == 2 && !i_info.is_text) {
		error = loader->load_import_metadata();
		ERR_RFLBC_COND_V_MSG_CLEANUP(error == ERR_CANT_ACQUIRE_RESOURCE, error, "Cannot load resource '" + p_path + "'.", loader);
		// If this is a 2.x resource with no imports, it will have no import metadata
		if (error != OK) {
			memdelete(loader);
			// if this is an auto converted resource, it's expected that it won't have any import metadata
			Vector<String> spl = p_path.get_file().split(".");
			if (spl.size() == 4 && loader->local_path.find(".converted.") != -1) {
				i_info.auto_converted_export = true;
				return OK;
			}
			// otherwise return an error to indicate that this is a non-autoconverted 2.x resource with no import metadata
			return ERR_PRINTER_ON_FIRE;
		}

		// Atlas and MODE_LARGE textures in 2.x had more than one source
		if (loader->imd->get_source_count() > 1) {
			WARN_PRINT("More than one source, likely a 2.x atlas/large texture");
		}
		// 2.0 resources often had import data but did not always have sources; handle this gracefully
		if (loader->imd->get_source_count() == 0 && loader->imd->get_editor().is_empty()) {
			memdelete(loader);
			return OK;
		}
		i_info.v2metadata = loader->imd;
		ERR_RFLBC_COND_V_MSG_CLEANUP(loader->imd->get_source_count() == 0, ERR_FILE_CORRUPT, "metadata corrupt for '" + loader->res_path + "'", loader);
	}
	memdelete(loader);
	return OK;
}

ResourceLoaderCompat *ResourceFormatLoaderCompat::_open_bin(const String &p_path, const String &base_dir, bool fake_load, Error *r_error, float *r_progress) {
	Error error = OK;
	if (!r_error) {
		r_error = &error;
	}
	String res_path = GDRESettings::get_singleton()->get_res_path(p_path, base_dir);
	Ref<FileAccess> f;
	if (res_path != "") {
		f = FileAccess::open(res_path, FileAccess::READ, r_error);
	} else {
		*r_error = ERR_FILE_NOT_FOUND;
		ERR_FAIL_COND_V_MSG(f.is_null(), nullptr, "Cannot open file '" + res_path + "'.");
	}
	// TODO: remove this extra check
	ERR_FAIL_COND_V_MSG(f.is_null(), nullptr, "Cannot open file '" + res_path + "' (Even after get_res_path() returned a path?).");

	ResourceLoaderCompat *loader = memnew(ResourceLoaderCompat);
	loader->project_dir = base_dir;
	loader->progress = r_progress;
	loader->fake_load = fake_load;
	loader->local_path = GDRESettings::get_singleton()->localize_path(p_path, base_dir);
	loader->res_path = res_path;

	*r_error = loader->open_bin(f);

	ERR_FAIL_COND_V_MSG(error != OK, loader, "Cannot open resource '" + p_path + "'.");

	return loader;
}

ResourceLoaderCompat *ResourceFormatLoaderCompat::_open_text(const String &p_path, const String &base_dir, bool fake_load, Error *r_error, float *r_progress) {
	Error error = OK;
	if (!r_error) {
		r_error = &error;
	}
	String res_path = GDRESettings::get_singleton()->get_res_path(p_path, base_dir);
	Ref<FileAccess> f = nullptr;
	if (res_path != "") {
		f = FileAccess::open(res_path, FileAccess::READ, r_error);
	} else {
		*r_error = ERR_FILE_NOT_FOUND;
		ERR_FAIL_COND_V_MSG(f.is_null(), nullptr, "Cannot open file '" + res_path + "'.");
	}
	// TODO: remove this extra check
	ERR_FAIL_COND_V_MSG(f.is_null(), nullptr, "Cannot open file '" + res_path + "' (Even after get_res_path() returned a path?).");

	ResourceLoaderCompat *loader = memnew(ResourceLoaderCompat);
	loader->project_dir = base_dir;
	loader->progress = r_progress;
	loader->fake_load = fake_load;
	loader->local_path = GDRESettings::get_singleton()->localize_path(p_path, base_dir);
	loader->res_path = res_path;

	*r_error = loader->open_text(f, false);

	ERR_FAIL_COND_V_MSG(error != OK, loader, "Cannot open resource '" + p_path + "'.");

	return loader;
}

ResourceLoaderCompat *ResourceFormatLoaderCompat::_open_after_recognizing(const String &p_path, const String &base_dir, bool fake_load, Error *r_error, float *r_progress) {
	ResourceFormatLoaderCompat::FormatType ftype = recognize(p_path, base_dir);
	if (ftype == ResourceFormatLoaderCompat::FormatType::BINARY) {
		return _open_bin(p_path, base_dir, fake_load, r_error, r_progress);
	} else if (ftype == ResourceFormatLoaderCompat::FormatType::TEXT) {
		return _open_text(p_path, base_dir, fake_load, r_error, r_progress);
	}
	ERR_FAIL_V_MSG(nullptr, "failed to open resource '" + p_path + "'.");
}

ResourceFormatLoaderCompat::FormatType ResourceFormatLoaderCompat::recognize(const String &p_path, const String &base_dir) {
	Error error = OK;
	String res_path = GDRESettings::get_singleton()->get_res_path(p_path, base_dir);
	Ref<FileAccess> f = nullptr;
	ERR_FAIL_COND_V_MSG(res_path.is_empty(), ResourceFormatLoaderCompat::FormatType::FILE_ERROR, "Cannot open file '" + res_path + "'.");
	f = FileAccess::open(res_path, FileAccess::READ, &error);
	// TODO: remove this extra check
	ERR_FAIL_COND_V_MSG(f.is_null(), ResourceFormatLoaderCompat::FormatType::FILE_ERROR, "Cannot open file '" + res_path + "' (Even after get_res_path() returned a path?).");

	uint8_t header[4];
	f->get_buffer(header, 4);
	if ((header[0] == 'R' && header[1] == 'S' && header[2] == 'R' && header[3] == 'C') ||
			(header[0] == 'R' && header[1] == 'S' && header[2] == 'C' && header[3] == 'C')) {
		return ResourceFormatLoaderCompat::FormatType::BINARY;
	}
	f->seek(0);
	VariantParser::StreamFile stream;
	stream.f = f;
	VariantParser::Tag tag;
	int lines = 1;
	String error_test;
	Error err = VariantParserCompat::parse_tag(&stream, lines, error_test, tag);
	if (tag.name == "gd_scene" || tag.name == "gd_resource") {
		return ResourceFormatLoaderCompat::FormatType::TEXT;
	} else {
		return ResourceFormatLoaderCompat::FormatType::UNKNOWN;
	}
}

Error ResourceLoaderCompat::load_import_metadata() {
	if (f.is_null()) {
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

StringName ResourceLoaderCompat::_get_string() {
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

Error ResourceLoaderCompat::open_bin(Ref<FileAccess> p_f, bool p_no_resources, bool p_keep_uuid_paths) {
	error = OK;

	f = p_f;
	uint8_t header[4];
	Error r_error;
	f->get_buffer(header, 4);
	if (header[0] == 'R' && header[1] == 'S' && header[2] == 'C' && header[3] == 'C') {
		// Compressed.
		Ref<FileAccessCompressed> fac;
		fac.instantiate();
		r_error = fac->open_after_magic(f);

		if (r_error != OK) {
			ERR_FAIL_COND_V_MSG(r_error != OK, r_error, "Cannot decompress compressed resource file '" + f->get_path() + "'.");
		}
		f = fac;

	} else if (header[0] != 'R' || header[1] != 'S' || header[2] != 'R' || header[3] != 'C') {
		// Not normal.
		r_error = ERR_FILE_UNRECOGNIZED;
		ERR_FAIL_COND_V_MSG(r_error != OK, r_error, "Unable to recognize  '" + f->get_path() + "'.");
	}

	bool big_endian = f->get_32();
	bool use_real64 = f->get_32();

	f->set_big_endian(big_endian != 0); // read big endian if saved as big endian
	stored_big_endian = big_endian;
	engine_ver_major = f->get_32();
	engine_ver_minor = f->get_32();
	ver_format_bin = f->get_32();
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
	print_bl("format: " + itos(ver_format_bin));
	ERR_FAIL_COND_V_MSG(engine_ver_major > 4, ERR_FILE_UNRECOGNIZED,
			"Unsupported engine version " + itos(engine_ver_major) + " used to create resource '" + res_path + "'.");
	ERR_FAIL_COND_V_MSG(ver_format_bin > VariantBin::FORMAT_VERSION, ERR_FILE_UNRECOGNIZED,
			"Unsupported binary resource format '" + res_path + "'.");

	res_type = get_unicode_string();

	print_bl("type: " + res_type);

	importmd_ofs = f->get_64();
	uint32_t flags = f->get_32();
	if (flags & ResourceFormatSaverBinaryInstance::FORMAT_FLAG_NAMED_SCENE_IDS) {
		using_named_scene_ids = true;
	}

	if (flags & ResourceFormatSaverBinaryInstance::FORMAT_FLAG_UIDS) {
		using_uids = true;
		res_uid = f->get_64();
	} else {
		// skip over res_uid field
		f->get_64();
		res_uid = ResourceUID::INVALID_ID;
	}
	if (flags & ResourceFormatSaverBinaryInstance::FORMAT_FLAG_REAL_T_IS_DOUBLE) {
		using_real_t_double = true;
		f->real_is_double = true;
	}

	if (flags & ResourceFormatSaverBinaryInstance::FORMAT_FLAG_HAS_SCRIPT_CLASS) {
		using_script_class = true;
		script_class = get_unicode_string();
	}

	for (int i = 0; i < ResourceFormatSaverBinaryInstance::RESERVED_FIELDS; i++) {
		f->get_32(); // skip a few reserved fields
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
			// if (!p_keep_uuid_paths && er.uid != ResourceUID::INVALID_ID) {
			// 	if (ResourceUID::get_singleton()->has_id(er.uid)) {
			// 		// If a UID is found and the path is valid, it will be used, otherwise, it falls back to the path.
			// 		er.path = ResourceUID::get_singleton()->get_id_path(er.uid);
			// 	} else {
			// 		WARN_PRINT(String(res_path + ": In external resource #" + itos(i) + ", invalid UUID: " + ResourceUID::get_singleton()->id_to_text(er.uid) + " - using text path instead: " + er.path).utf8().get_data());
			// 	}
			// }
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
		ERR_FAIL_V_MSG(error, "Premature end of file (EOF): " + local_path + ".");
	}

	return OK;
}

void ResourceLoaderCompat::debug_print_properties(String res_name, String res_type, List<ResourceProperty> lrp) {
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

// By default we do not load the external resources, we just load a "dummy" resource that has the path and the res_type.
// This is so we can convert the currently loading resource to binary/text without potentially loading an
// unavailable and/or incompatible resource
Error ResourceLoaderCompat::load_ext_resource(const uint32_t i) {
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

Ref<Resource> ResourceLoaderCompat::get_external_resource(const int subindex) {
	if (external_resources[subindex - 1].cache.is_valid()) {
		return external_resources[subindex - 1].cache;
	}
	// We don't do multithreading, so if this external resource is not cached (either dummy or real)
	// then we return a blank resource
	return Ref<Resource>();
}
String ResourceLoaderCompat::get_external_resource_path(const Ref<Resource> &res) {
	for (int i = 0; i < external_resources.size(); i++) {
		if (external_resources[i].cache == res) {
			return external_resources[i].path;
		}
	}
	return String();
}

int ResourceLoaderCompat::get_external_resource_save_order_by_path(const String &path) {
	for (int i = 0; i < external_resources.size(); i++) {
		if (external_resources[i].path == path) {
			return external_resources[i].save_order;
		}
	}
	return -1;
}

Ref<Resource> ResourceLoaderCompat::get_external_resource(const String &path) {
	for (int i = 0; i < external_resources.size(); i++) {
		if (external_resources[i].path == path) {
			return external_resources[i].cache;
		}
	}
	// We don't do multithreading, so if this external resource is not cached (either dummy or real)
	// then we return a blank resource
	return Ref<Resource>();
}

Ref<Resource> ResourceLoaderCompat::get_external_resource_by_id(const String &id) {
	for (int i = 0; i < external_resources.size(); i++) {
		if (external_resources[i].id == id) {
			return external_resources[i].cache;
		}
	}
	return Ref<Resource>();
}

bool ResourceLoaderCompat::has_external_resource(const Ref<Resource> &res) {
	for (int i = 0; i < external_resources.size(); i++) {
		if (external_resources[i].cache == res) {
			return true;
		}
	}
	return false;
}

bool ResourceLoaderCompat::has_external_resource(const String &path) {
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
Ref<Resource> ResourceLoaderCompat::instance_internal_resource(const String &path, const String &type, const String &id) {
	Ref<Resource> res;
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
		Ref<Resource> r = ResourceCache::get_ref(path);
		if (r->get_class() == type) {
			r->reset_state();
			res = Ref<Resource>(r);
		}
	}

	if (res.is_null()) {
		// did not replace
		Object *obj = ClassDB::instantiate(type);
		if (!obj) {
			error = ERR_FILE_CORRUPT;
			ERR_FAIL_V_MSG(Ref<Resource>(), local_path + ":Resource of unrecognized type in file: " + type + ".");
		}

		Resource *r = Object::cast_to<Resource>(obj);
		if (!r) {
			String obj_class = obj->get_class();
			error = ERR_FILE_CORRUPT;
			memdelete(obj); //bye
			ERR_FAIL_V_MSG(Ref<Resource>(), local_path + ":Resource type in resource field not a resource, type is: " + obj_class + ".");
		}

		if (path != String() && cache_mode != ResourceFormatLoader::CACHE_MODE_IGNORE) {
			r->set_path(path, cache_mode == ResourceFormatLoader::CACHE_MODE_REPLACE); // if got here because the resource with same path has different res_type, replace it
		}
		r->set_scene_unique_id(id);
		res = Ref<Resource>(r);
	}
	return res;
}

String ResourceLoaderCompat::get_internal_resource_path(const Ref<Resource> &res) {
	for (KeyValue<String, Ref<Resource>> E : internal_res_cache) {
		if (E.value == res) {
			return E.key;
		}
	}
	return String();
}

Ref<Resource> ResourceLoaderCompat::get_internal_resource_by_subindex(const int subindex) {
	for (auto R = internal_res_cache.front(); R; R = R->next()) {
		if (R->value()->get_scene_unique_id() == itos(subindex)) {
			return R->value();
		}
	}
	return Ref<Resource>();
}

int ResourceLoaderCompat::get_internal_resource_save_order_by_path(const String &path) {
	if (has_internal_resource(path)) {
		for (int i = 0; i < internal_resources.size(); i++) {
			if (internal_resources[i].path == path) {
				return internal_resources[i].save_order;
			}
		}
	}
	return -1;
}

Ref<Resource> ResourceLoaderCompat::get_internal_resource(const String &path) {
	if (has_internal_resource(path)) {
		return internal_res_cache[path];
	}
	return Ref<Resource>();
}

String ResourceLoaderCompat::get_internal_resource_type(const String &path) {
	if (has_internal_resource(path)) {
		return internal_type_cache[path];
	}
	return "None";
}

List<ResourceProperty> ResourceLoaderCompat::get_internal_resource_properties(const String &path) {
	if (has_internal_resource(path)) {
		return internal_index_cached_properties[path];
	}
	return List<ResourceProperty>();
}

bool ResourceLoaderCompat::has_internal_resource(const Ref<Resource> &res) {
	for (KeyValue<String, Ref<Resource>> E : internal_res_cache) {
		if (E.value == res) {
			return true;
		}
	}
	return false;
}

bool ResourceLoaderCompat::has_internal_resource(const String &path) {
	return internal_res_cache.has(path);
}

String ResourceLoaderCompat::get_resource_path(const Ref<Resource> &res) {
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
Error ResourceLoaderCompat::repair_property(const String &rtype, const StringName &name, Variant &value) {
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
			// easy fixes for String/StringName swaps
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
			// let us hope that this is the name of the object class
			String hint = pinfo->class_name;
			// String hint = pinfo->hint_string;
			if (hint.is_empty()) {
				ERR_EXIT_REPAIR_PROPERTY(ERR_FILE_UNRECOGNIZED, "Possible type difference in property " + name + "\nclass property type is of an unknown object, attempted set value type is " + value_type);
			}
			ClassDB::get_property_list(StringName(hint), listpinfo);
			if (!listpinfo || listpinfo->size() == 0) {
				ERR_EXIT_REPAIR_PROPERTY(ERR_FILE_CANT_OPEN, "Failed to get list of properties for property " + name + " of object type " + hint);
			}
			Ref<Resource> res = value;
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
					// otherwise, fail
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

Error ResourceLoaderCompat::load() {
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
		Ref<Resource> res = instance_internal_resource(path, rtype, id);
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
			if (rp.type == Variant::OBJECT) {
				Object *obj = value;
				if (obj) {
					rp.class_name = obj->get_class_name();
				} else {
					// We screwed up a load of an embedded object, just fail
					ERR_FAIL_V_MSG(error, "Failed to load resource " + path + ": Object property " + name + " was null, please report this!!");
				}
			}
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
						WARN_PRINT("Got error trying to set extra property " + name + " on " + res_type + " resource " + path);
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
		internal_res_cache[path] = res;

		// packed scenes with instances for nodes won't work right without creating an instance of it
		// So we always instance them regardless if this is a fake load or not.
		if (main && fake_load && res_type == "PackedScene") {
			// FakeResource inherits from PackedScene
			Ref<PackedScene> ps;
			ps.instantiate();
			Dictionary bundle;
			// iterate through linked list to find "_bundled"
			for (List<ResourceProperty>::Element *E = lrp.front(); E; E = E->next()) {
				if (E->get().name == "_bundled") {
					bundle = E->get().value;
					break;
				}
			}
			if (bundle.is_empty()) {
				error = ERR_FILE_CORRUPT;
				ERR_FAIL_V_MSG(error, "Failed to load packed scene");
			}
			ps->set("_bundled", bundle);
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
	// If we got here, we never loaded the main resource
	return ERR_FILE_EOF;
}

String ResourceLoaderCompat::get_ustring(Ref<FileAccess> f) {
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

String ResourceLoaderCompat::get_unicode_string() {
	return get_ustring(f);
}

Ref<Resource> ResourceLoaderCompat::make_dummy(const String &path, const String &type, const String &id) {
	Ref<FakeResource> dummy;
	dummy.instantiate();
	dummy->set_real_path(path);
	dummy->set_real_type(type);
	dummy->set_scene_unique_id(id);
	return dummy;
}

Ref<Resource> ResourceLoaderCompat::set_dummy_ext(const uint32_t erindex) {
	if (external_resources[erindex].cache.is_valid()) {
		return external_resources[erindex].cache;
	}
	String id;
	if (using_uids) {
		id = ResourceUID::get_singleton()->id_to_text(external_resources[erindex].uid);
	} else if (external_resources[erindex].id != "") {
		id = external_resources[erindex].id;
	} else {
		id = itos(erindex + 1);
	}
	Ref<Resource> dummy = make_dummy(external_resources[erindex].path, external_resources[erindex].type, id);
	external_resources.write[erindex].cache = dummy;

	return dummy;
}

Ref<Resource> ResourceLoaderCompat::set_dummy_ext(const String &path, const String &exttype) {
	for (int i = 0; i < external_resources.size(); i++) {
		if (external_resources[i].path == path) {
			if (external_resources[i].cache.is_valid()) {
				return external_resources[i].cache;
			}
			return set_dummy_ext(i);
		}
	}
	// If not found in cache...
	WARN_PRINT("External resource not found in cache???? Making dummy anyway...");
	ExtResource er;
	er.path = path;
	er.type = exttype;
	er.cache = make_dummy(path, exttype, itos(external_resources.size() + 1));
	external_resources.push_back(er);

	return er.cache;
}

void ResourceLoaderCompat::advance_padding(Ref<FileAccess> f, uint32_t p_len) {
	uint32_t extra = 4 - (p_len % 4);
	if (extra < 4) {
		for (uint32_t i = 0; i < extra; i++) {
			f->get_8(); // pad to 32
		}
	}
}

void ResourceLoaderCompat::_advance_padding(uint32_t p_len) {
	advance_padding(f, p_len);
}

String ResourceLoaderCompat::_write_rlc_resources(void *ud, const Ref<Resource> &p_resource) {
	ResourceLoaderCompat *rsi = (ResourceLoaderCompat *)ud;
	return rsi->_write_rlc_resource(p_resource);
}

String ResourceLoaderCompat::_write_rlc_resource(const Ref<Resource> &res) {
	String path = get_resource_path(res);
	String id;
	if (has_internal_resource(path)) {
		id = itos(get_internal_resource_save_order_by_path(path));
	} else if (has_external_resource(path)) {
		id = itos(get_external_resource_save_order_by_path(path));
	}
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

Error ResourceLoaderCompat::save_as_text_unloaded(const String &dest_path, uint32_t p_flags) {
	bool is_scene = false;
	// main resource
	Ref<Resource> res = resource;
	Ref<PackedScene> packed_scene;
	if (dest_path.ends_with(".tscn") || dest_path.ends_with(".escn")) {
		is_scene = true;
		packed_scene = resource;
	}

	Error err;
	Ref<FileAccess> wf = FileAccess::open(dest_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err != OK, ERR_CANT_OPEN, "Cannot save file '" + dest_path + "'.");

	String main_res_path = get_resource_path(res);
	if (main_res_path == "") {
		main_res_path = local_path;
	}
	// the actual res_type in case this is a fake resource
	String main_type = get_internal_resource_type(main_res_path);

	// Version 1 (Godot 2.x)
	// Version 2 (Godot 3.x): changed names for Basis, AABB, Vectors, etc.
	// Version 3 (Godot 4.x): new string ID for ext/subresources, breaks forward compat.
	ver_format_text = 1;
	if (engine_ver_major == 3 || dest_path.ends_with(".escn")) { // escn is always format version 2
		ver_format_text = 2;
	} else if (engine_ver_major == 4) {
		ver_format_text = 3;
	}

	// save resources
	{
		String title = is_scene ? "[gd_scene " : "[gd_resource ";
		if (!is_scene) {
			title += "type=\"" + main_type + "\" ";
			if (!script_class.is_empty() && ver_format_text > 2) { // only present on 4.x and above
				title += "script_class=\"" + script_class + "\" ";
			}
		}
		int load_steps = internal_resources.size() + external_resources.size();

		if (load_steps > 1) {
			title += "load_steps=" + itos(load_steps) + " ";
		}
		title += "format=" + itos(ver_format_text) + "";
		// if v3 (Godot 4.x), store res_uid

		if (ver_format_text >= 3) {
			if (res_uid == ResourceUID::INVALID_ID) {
				res_uid = ResourceSaver::get_resource_id_for_path(local_path, true);
			}
			if (res_uid != ResourceUID::INVALID_ID) {
				title += " uid=\"" + ResourceUID::get_singleton()->id_to_text(res_uid) + "\"";
			}
		}
		wf->store_string(title);
		wf->store_line("]\n"); // one empty line
	}

	for (int i = 0; i < external_resources.size(); i++) {
		String p = external_resources[i].path;
		external_resources.ptrw()[i].save_order = i + 1;
		// Godot 4.x: store res_uid tag
		if (ver_format_text >= 3) {
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
					"\" id=" + itos(i + 1) + "]\n"); // bundled
		}
	}

	if (external_resources.size()) {
		wf->store_line(String()); // separate
	}
	RBSet<String> used_unique_ids;
	// // Godot 4.x: Get all the unique ids for lookup
	// if (ver_format_text >= 3) {
	// 	for (int i = 0; i < internal_resources.size(); i++) {
	// 		Ref<Resource> intres = get_internal_resource(internal_resources[i].path);
	// 		if (i != internal_resources.size() - 1 && (res->get_path() == "" || res->get_path().find("::") != -1)) {
	// 			if (intres->get_scene_unique_id() != "") {
	// 				if (used_unique_ids.has(intres->get_scene_unique_id())) {
	// 					intres->set_scene_unique_id(""); // Repeated.
	// 				} else {
	// 					used_unique_ids.insert(intres->get_scene_unique_id());
	// 				}
	// 			}
	// 		}
	// 	}
	// }

	for (int i = 0; i < internal_resources.size(); i++) {
		String path = internal_resources[i].path;
		Ref<Resource> intres = get_internal_resource(path);
		bool main = i == (internal_resources.size() - 1);

		if (main && is_scene) {
			break; // save as a scene
		}

		if (main) {
			wf->store_line("[resource]");
		} else {
			String line = "[sub_resource ";
			String type = get_internal_resource_type(path);
			internal_resources.ptrw()[i].save_order = i + 1;
			String id = itos(i + 1);

			line += "type=\"" + type + "\" ";
			// Godot 4.x
			if (ver_format_text >= 3) {
				// // if unique id == "", generate and then store
				// if (id == "") {
				// 	String new_id;
				// 	while (true) {
				// 		new_id = type + "_" + Resource::generate_scene_unique_id();

				// 		if (!used_unique_ids.has(new_id)) {
				// 			break;
				// 		}
				// 	}
				// 	intres->set_scene_unique_id(new_id);
				// 	used_unique_ids.insert(new_id);
				// 	id = new_id;
				// }
				// id is a string in Godot 4.x
				line += "id=\"" + id + "\"]";
				// For Godot 3.x and lower resources, the unique id will just be the numerical index
			} else {
				line += "id=" + id + "]";
			}

			if (ver_format_text == 1) {
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

	// if this is a scene, save nodes and connections!
	if (is_scene) {
		Ref<SceneState> state = packed_scene->get_state();
		ERR_FAIL_COND_V_MSG(!state.is_valid(), ERR_FILE_CORRUPT, "Packed scene is corrupt!");
		for (int i = 0; i < state->get_node_count(); i++) {
			StringName type = state->get_node_type(i);
			StringName name = state->get_node_name(i);
			int index = state->get_node_index(i);
			NodePath path = state->get_node_path(i, true);
			NodePath owner = state->get_node_owner_path(i);
			Ref<Resource> instance = state->get_node_instance(i);

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
			if (ver_format_text == 1 && state->get_node_property_count(i) != 0) {
				// Godot 2.x quirk: newline between header and properties
				// We're emulating these whitespace quirks to enable easy diffs for regression testing
				wf->store_line("");
			}

			for (int j = 0; j < state->get_node_property_count(i); j++) {
				String vars;
				VariantWriterCompat::write_to_string(state->get_node_property_value(i, j), vars, engine_ver_major, _write_rlc_resources, this);

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
			if (ver_format_text == 1) {
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
	wf->flush();
	if (wf->get_error() != OK && wf->get_error() != ERR_FILE_EOF) {
		return ERR_CANT_CREATE;
	}

	return OK;
}

Error ResourceLoaderCompat::parse_variant(Variant &r_v) {
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
		case VariantBin::VARIANT_VECTOR4: {
			Vector4 v;
			v.x = f->get_real();
			v.y = f->get_real();
			v.z = f->get_real();
			v.w = f->get_real();
			r_v = v;
		} break;
		case VariantBin::VARIANT_VECTOR4I: {
			Vector4i v;
			v.x = f->get_32();
			v.y = f->get_32();
			v.z = f->get_32();
			v.w = f->get_32();
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
			v.columns[0].x = f->get_real();
			v.columns[0].y = f->get_real();
			v.columns[1].x = f->get_real();
			v.columns[1].y = f->get_real();
			v.columns[2].x = f->get_real();
			v.columns[2].y = f->get_real();
			r_v = v;

		} break;
		case VariantBin::VARIANT_MATRIX3: {
			Basis v;
			v.rows[0].x = f->get_real();
			v.rows[0].y = f->get_real();
			v.rows[0].z = f->get_real();
			v.rows[1].x = f->get_real();
			v.rows[1].y = f->get_real();
			v.rows[1].z = f->get_real();
			v.rows[2].x = f->get_real();
			v.rows[2].y = f->get_real();
			v.rows[2].z = f->get_real();
			r_v = v;

		} break;
		case VariantBin::VARIANT_TRANSFORM: {
			Transform3D v;
			v.basis.rows[0].x = f->get_real();
			v.basis.rows[0].y = f->get_real();
			v.basis.rows[0].z = f->get_real();
			v.basis.rows[1].x = f->get_real();
			v.basis.rows[1].y = f->get_real();
			v.basis.rows[1].z = f->get_real();
			v.basis.rows[2].x = f->get_real();
			v.basis.rows[2].y = f->get_real();
			v.basis.rows[2].z = f->get_real();
			v.origin.x = f->get_real();
			v.origin.y = f->get_real();
			v.origin.z = f->get_real();
			r_v = v;
		} break;
		case VariantBin::VARIANT_PROJECTION: {
			Projection v;
			v.columns[0].x = f->get_real();
			v.columns[0].y = f->get_real();
			v.columns[0].z = f->get_real();
			v.columns[0].w = f->get_real();
			v.columns[1].x = f->get_real();
			v.columns[1].y = f->get_real();
			v.columns[1].z = f->get_real();
			v.columns[1].w = f->get_real();
			v.columns[2].x = f->get_real();
			v.columns[2].y = f->get_real();
			v.columns[2].z = f->get_real();
			v.columns[2].w = f->get_real();
			v.columns[3].x = f->get_real();
			v.columns[3].y = f->get_real();
			v.columns[3].z = f->get_real();
			v.columns[3].w = f->get_real();
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
		// Old Godot 2.x Image variant, convert into an object
		case VariantBin::VARIANT_IMAGE: {
			//Have to decode the old Image variant here
			Error err = ImageParserV2::decode_image_v2(f, r_v, convert_v2image_indexed);
			if (err != OK) {
				if (err == ERR_UNAVAILABLE) {
					return err;
				}
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
			bool has_property = ver_format_bin < VariantBin::FORMAT_VERSION_NO_NODEPATH_PROPERTY;
			if (has_property) {
				subname_count += 1; // has a property field, so we should count it as well
			}
			for (int i = 0; i < name_count; i++) {
				names.push_back(_get_string());
			}
			for (uint32_t i = 0; i < subname_count; i++) {
				subnames.push_back(_get_string());
			}
			// empty property field, remove it
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
					// do none

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
						r_v = get_internal_resource_by_subindex(index);
					}

					if (!r_v) {
						WARN_PRINT(String("Couldn't load internal resource (no cache): Subresource " + itos(index)).utf8().get_data());
						r_v = Variant();
					}
				} break;
				case VariantBin::OBJECT_EXTERNAL_RESOURCE: {
					// old file format, still around for compatibility

					String exttype = get_unicode_string();
					String path = get_unicode_string();

					// don't bother setting it, just try and find it and fail if we can't
					r_v = get_external_resource(path);
					ERR_FAIL_COND_V_MSG(r_v.is_null(), ERR_FILE_MISSING_DEPENDENCIES, "Can't load dependency: " + path + ".");

				} break;
				case VariantBin::OBJECT_EXTERNAL_RESOURCE_INDEX: {
					// new file format, just refers to an index in the external list
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

		// Old Godot 2.x InputEvent variant
		// They were never saved into the binary resource files, we will only encounter the type number
		case VariantBin::VARIANT_INPUT_EVENT: {
			WARN_PRINT("Encountered a Input event variant, someone screwed up when exporting this project");
		} break;
		case VariantBin::VARIANT_CALLABLE: {
			r_v = Callable();
		} break;
		case VariantBin::VARIANT_SIGNAL: {
			r_v = Signal();
		} break;
		case VariantBin::VARIANT_DICTIONARY: {
			uint32_t len = f->get_32();
			Dictionary d; // last bit means shared
			len &= 0x7FFFFFFF;
			for (uint32_t i = 0; i < len; i++) {
				Variant key;
				Error err = parse_variant(key);
				if (err == ERR_UNAVAILABLE) {
					return err;
				}
				ERR_FAIL_COND_V_MSG(err, ERR_FILE_CORRUPT, "Error when trying to parse Variant.");
				Variant value;
				err = parse_variant(value);
				if (err == ERR_UNAVAILABLE) {
					return err;
				}
				ERR_FAIL_COND_V_MSG(err, ERR_FILE_CORRUPT, "Error when trying to parse Variant.");
				d[key] = value;
			}
			r_v = d;
		} break;
		case VariantBin::VARIANT_ARRAY: {
			uint32_t len = f->get_32();
			Array a; // last bit means shared
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
		case VariantBin::VARIANT_PACKED_INT64_ARRAY: {
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
		case VariantBin::VARIANT_PACKED_FLOAT64_ARRAY: {
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
				ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, "Vector2 size is NOT 8!");
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
				ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, "Vector3 size is NOT 12!");
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
				ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, "Color size is NOT 16!");
			}

			r_v = array;
		} break;
		default: {
			ERR_FAIL_V(ERR_FILE_CORRUPT);
		} break;
	}

	return OK;
}

void ResourceLoaderCompat::save_ustring(Ref<FileAccess> f, const String &p_string) {
	CharString utf8 = p_string.utf8();
	f->store_32(utf8.length() + 1);
	f->store_buffer((const uint8_t *)utf8.get_data(), utf8.length() + 1);
}

void ResourceLoaderCompat::save_unicode_string(const String &p_string) {
	save_ustring(f, p_string);
}

int ResourceLoaderCompat::get_string_index(const String &p_string, bool add) {
	StringName s = p_string;
	if (string_map.has(s)) {
		return string_map.find(p_string);
	} else if (!add) {
		return -1;
	}
	string_map.push_back(s);
	return string_map.size() - 1;
}

Error ResourceLoaderCompat::write_variant_bin(Ref<FileAccess> fa, const Variant &p_property, const PropertyInfo &p_hint) {
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
			int64_t val = p_property;
			if (val > 0x7FFFFFFF || val < -(int64_t)0x80000000) {
				fa->store_32(VariantBin::VARIANT_INT64);
				fa->store_64(val);
			} else {
				fa->store_32(VariantBin::VARIANT_INT);
				fa->store_32(int32_t(p_property));
			}
		} break;
		case Variant::FLOAT: {
			double d = p_property;
			float fl = d;
			if (double(fl) != d) {
				fa->store_32(VariantBin::VARIANT_DOUBLE);
				fa->store_double(d);
			} else {
				fa->store_32(VariantBin::VARIANT_FLOAT);
				fa->store_real(fl);
			}
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
		case Variant::VECTOR2I: {
			fa->store_32(VariantBin::VARIANT_VECTOR2I);
			Vector2i val = p_property;
			fa->store_32(val.x);
			fa->store_32(val.y);
		} break;
		case Variant::RECT2: {
			fa->store_32(VariantBin::VARIANT_RECT2);
			Rect2 val = p_property;
			fa->store_real(val.position.x);
			fa->store_real(val.position.y);
			fa->store_real(val.size.x);
			fa->store_real(val.size.y);

		} break;
		case Variant::RECT2I: {
			fa->store_32(VariantBin::VARIANT_RECT2I);
			Rect2i val = p_property;
			fa->store_32(val.position.x);
			fa->store_32(val.position.y);
			fa->store_32(val.size.x);
			fa->store_32(val.size.y);

		} break;
		case Variant::VECTOR3: {
			fa->store_32(VariantBin::VARIANT_VECTOR3);
			Vector3 val = p_property;
			fa->store_real(val.x);
			fa->store_real(val.y);
			fa->store_real(val.z);

		} break;
		case Variant::VECTOR3I: {
			fa->store_32(VariantBin::VARIANT_VECTOR3I);
			Vector3i val = p_property;
			fa->store_32(val.x);
			fa->store_32(val.y);
			fa->store_32(val.z);

		} break;
		case Variant::VECTOR4: {
			fa->store_32(VariantBin::VARIANT_VECTOR4);
			Vector4 val = p_property;
			fa->store_real(val.x);
			fa->store_real(val.y);
			fa->store_real(val.z);
			fa->store_real(val.w);

		} break;
		case Variant::VECTOR4I: {
			fa->store_32(VariantBin::VARIANT_VECTOR4I);
			Vector4i val = p_property;
			fa->store_32(val.x);
			fa->store_32(val.y);
			fa->store_32(val.z);
			fa->store_32(val.w);
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
			fa->store_real(val.columns[0].x);
			fa->store_real(val.columns[0].y);
			fa->store_real(val.columns[1].x);
			fa->store_real(val.columns[1].y);
			fa->store_real(val.columns[2].x);
			fa->store_real(val.columns[2].y);

		} break;
		case Variant::BASIS: {
			fa->store_32(VariantBin::VARIANT_MATRIX3);
			Basis val = p_property;
			fa->store_real(val.rows[0].x);
			fa->store_real(val.rows[0].y);
			fa->store_real(val.rows[0].z);
			fa->store_real(val.rows[1].x);
			fa->store_real(val.rows[1].y);
			fa->store_real(val.rows[1].z);
			fa->store_real(val.rows[2].x);
			fa->store_real(val.rows[2].y);
			fa->store_real(val.rows[2].z);

		} break;
		case Variant::TRANSFORM3D: {
			fa->store_32(VariantBin::VARIANT_TRANSFORM);
			Transform3D val = p_property;
			fa->store_real(val.basis.rows[0].x);
			fa->store_real(val.basis.rows[0].y);
			fa->store_real(val.basis.rows[0].z);
			fa->store_real(val.basis.rows[1].x);
			fa->store_real(val.basis.rows[1].y);
			fa->store_real(val.basis.rows[1].z);
			fa->store_real(val.basis.rows[2].x);
			fa->store_real(val.basis.rows[2].y);
			fa->store_real(val.basis.rows[2].z);
			fa->store_real(val.origin.x);
			fa->store_real(val.origin.y);
			fa->store_real(val.origin.z);

		} break;
		case Variant::PROJECTION: {
			fa->store_32(VariantBin::VARIANT_PROJECTION);
			Projection val = p_property;
			fa->store_real(val.columns[0].x);
			fa->store_real(val.columns[0].y);
			fa->store_real(val.columns[0].z);
			fa->store_real(val.columns[0].w);
			fa->store_real(val.columns[1].x);
			fa->store_real(val.columns[1].y);
			fa->store_real(val.columns[1].z);
			fa->store_real(val.columns[1].w);
			fa->store_real(val.columns[2].x);
			fa->store_real(val.columns[2].y);
			fa->store_real(val.columns[2].z);
			fa->store_real(val.columns[2].w);
			fa->store_real(val.columns[3].x);
			fa->store_real(val.columns[3].y);
			fa->store_real(val.columns[3].z);
			fa->store_real(val.columns[3].w);
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
			if (ver_format_bin < VariantBin::FORMAT_VERSION_NO_NODEPATH_PROPERTY) {
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

			for (int i = 0; i < np.get_name_count(); i++) {
				ERR_FAIL_COND_V_MSG(get_string_index(np.get_name(i)) == -1, ERR_BUG,
						"Not in string map!");
				fa->store_32(get_string_index(np.get_name(i)));
			}
			// store all subnames minus any property fields if need be
			for (int i = 0; i < snc; i++) {
				ERR_FAIL_COND_V_MSG(get_string_index(np.get_subname(i)) == -1, ERR_BUG,
						"Not in string map!");
				fa->store_32(get_string_index(np.get_subname(i)));
			}
			// If ver_format_bin 1-2 (i.e. godot 2.x)
			if (ver_format_bin < VariantBin::FORMAT_VERSION_NO_NODEPATH_PROPERTY) {
				// If we found a property, store it
				if (property_idx > -1) {
					ERR_FAIL_COND_V_MSG(get_string_index(np.get_subname(property_idx)) == -1, ERR_BUG,
							"Not in string map!");
					fa->store_32(get_string_index(np.get_subname(property_idx)));
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
			Ref<Resource> res = p_property;
			// This will only be triggered on godot 2.x, where the image variant is loaded as an image object vs. a resource
			if (ver_format_bin == 1 && res->is_class("Image")) {
				fa->store_32(VariantBin::VARIANT_IMAGE);
				// storing lossless compressed by default
				ImageParserV2::write_image_v2_to_bin(fa, p_property, true);
			} else {
				fa->store_32(VariantBin::VARIANT_OBJECT);
				if (res.is_null()) {
					fa->store_32(VariantBin::OBJECT_EMPTY);
					return OK; // don't save it
				}
				String rpath = get_resource_path(res);
				if (rpath.length() && rpath.find("::") == -1 && !rpath.begins_with("local://")) {
					if (!has_external_resource(rpath)) {
						fa->store_32(VariantBin::OBJECT_EMPTY);
						ERR_FAIL_COND_V_MSG(!has_external_resource(rpath), ERR_BUG, "Cannot find external resource");
					}
					//Ref<Resource> res = get_external_resource(rpath);
					fa->store_32(VariantBin::OBJECT_EXTERNAL_RESOURCE_INDEX);
					fa->store_32(get_external_resource_save_order_by_path(rpath));
				} else {
					if (!has_internal_resource(rpath)) {
						fa->store_32(VariantBin::OBJECT_EMPTY);
						ERR_FAIL_V_MSG(ERR_BUG, "Resource was not pre cached for the resource section, bug?");
					}

					fa->store_32(VariantBin::OBJECT_INTERNAL_RESOURCE);
					fa->store_32(get_internal_resource_save_order_by_path(rpath));
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
		case Variant::PACKED_INT64_ARRAY: {
			fa->store_32(VariantBin::VARIANT_PACKED_INT64_ARRAY);
			Vector<int64_t> arr = p_property;
			int len = arr.size();
			fa->store_32(len);
			const int64_t *r = arr.ptr();
			for (int i = 0; i < len; i++) {
				fa->store_64(r[i]);
			}

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
		case Variant::PACKED_FLOAT64_ARRAY: {
			fa->store_32(VariantBin::VARIANT_PACKED_FLOAT64_ARRAY);
			Vector<double> arr = p_property;
			int len = arr.size();
			fa->store_32(len);
			const double *r = arr.ptr();
			for (int i = 0; i < len; i++) {
				fa->store_double(r[i]);
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

void ResourceLoaderCompat::_populate_string_map(const Variant &p_variant, bool p_main = false) {
	switch (p_variant.get_type()) {
		case Variant::OBJECT: {
			Ref<Resource> res = p_variant;

			if (res.is_null() || has_external_resource(res)) {
				return;
			}

			List<ResourceProperty> property_list;
			String path = p_main ? local_path : get_resource_path(res);
			if (path.is_empty()) {
				// Image variant converted to image class, it's really loaded, we have to get the properties from this object
				if (res->get_class() == "Image" && engine_ver_major == 2) {
					path = local_path;
					List<PropertyInfo> p_list;
					((Ref<Image>)res)->get_property_list(&p_list);
					for (const PropertyInfo &E : p_list) {
						get_string_index(E.name, true);
					}
					break;
				}
				if (path.is_empty()) {
					ERR_PRINT("can't find resource " + res->get_name());
					return;
				}
			}
			property_list = get_internal_resource_properties(path);

			for (const ResourceProperty &E : property_list) {
				get_string_index(E.name, true);
				_populate_string_map(E.value, false);
			}
		} break;

		case Variant::ARRAY: {
			Array varray = p_variant;
			int len = varray.size();
			for (int i = 0; i < len; i++) {
				const Variant &v = varray.get(i);
				_populate_string_map(v);
			}

		} break;

		case Variant::DICTIONARY: {
			Dictionary d = p_variant;
			List<Variant> keys;
			d.get_key_list(&keys);
			for (const Variant &E : keys) {
				_populate_string_map(E);
				Variant v = d[E];
				_populate_string_map(v);
			}
		} break;
		case Variant::NODE_PATH: {
			// take the chance and save node path strings
			NodePath np = p_variant;
			for (int i = 0; i < np.get_name_count(); i++) {
				get_string_index(np.get_name(i), true);
			}
			for (int i = 0; i < np.get_subname_count(); i++) {
				get_string_index(np.get_subname(i), true);
			}

		} break;
		default: {
		}
	}
}

Error ResourceLoaderCompat::save_to_bin(const String &p_path, uint32_t p_flags) {
	Error err;
	Ref<FileAccess> fw;
	// We don't support writing this yet
	ERR_FAIL_COND_V_MSG(using_real_t_double, ERR_UNAVAILABLE, "Saving double precision resources is not supported yet.");

	if (p_flags & ResourceSaver::FLAG_COMPRESS) {
		Ref<FileAccessCompressed> fac;
		fac.instantiate();
		fac->configure("RSCC");
		fw = fac;
		err = fac->open_internal(p_path, FileAccess::WRITE);
	} else {
		fw = FileAccess::open(p_path, FileAccess::WRITE, &err);
	}

	ERR_FAIL_COND_V_MSG(err != OK, err, "can't open " + p_path + " for writing");

	//bool takeover_paths = p_flags & ResourceSaver::FLAG_REPLACE_SUBRESOURCE_PATHS;
	bool big_endian = p_flags & ResourceSaver::FLAG_SAVE_BIG_ENDIAN || stored_big_endian;
	// if (!p_path.begins_with("res://"))
	// 	takeover_paths = false;

	//bin_meta_idx = get_string_index("__bin_meta__"); //is often used, so create

	if (!(p_flags & ResourceSaver::FLAG_COMPRESS)) {
		// save header compressed
		static const uint8_t header[4] = { 'R', 'S', 'R', 'C' };
		fw->store_buffer(header, 4);
	}

	if (big_endian) {
		fw->store_32(1);
		fw->set_big_endian(true);
	} else {
		fw->store_32(0);
	}

	fw->store_32(stored_use_real64 ? 1 : 0); // 64 bits file

	fw->store_32(engine_ver_major);
	fw->store_32(engine_ver_minor);

	// If we're using named_scene_ids, it's version 4
	if (using_named_scene_ids) {
		ver_format_bin = 4;
		// else go by engine major version
	} else if (engine_ver_major == 3) {
		ver_format_bin = 3;
	} else if (engine_ver_major == 4) {
		ver_format_bin = 4;
	} else {
		ver_format_bin = 1;
	}

	fw->store_32(ver_format_bin);

	if (fw->get_error() != OK && fw->get_error() != ERR_FILE_EOF) {
		fw->flush();
		return ERR_CANT_CREATE;
	}

	// fw->store_32(saved_resources.size()+external_resources.size()); // load steps -not needed
	save_ustring(fw, res_type);
	uint64_t md_at = fw->get_position();
	fw->store_64(0); // offset to impoty metadata

	uint32_t flags = 0;
	if (using_named_scene_ids) {
		flags |= ResourceFormatSaverBinaryInstance::FORMAT_FLAG_NAMED_SCENE_IDS;
	}
	if (using_uids) {
		flags |= ResourceFormatSaverBinaryInstance::FORMAT_FLAG_UIDS;
		fw->store_32(flags);
		fw->store_64(res_uid);
	} else {
		fw->store_32(flags);
		fw->store_64(0);
	}

	if (using_real_t_double) {
		flags |= ResourceFormatSaverBinaryInstance::FORMAT_FLAG_REAL_T_IS_DOUBLE;
	}

	if (using_script_class && !script_class.is_empty()) {
		flags |= ResourceFormatSaverBinaryInstance::FORMAT_FLAG_HAS_SCRIPT_CLASS;
		save_ustring(fw, script_class);
	}

	for (int i = 0; i < ResourceFormatSaverBinaryInstance::RESERVED_FIELDS; i++) {
		fw->store_32(0); // reserved
	}
	_populate_string_map(resource, true);
	fw->store_32(string_map.size()); // string table size
	for (int i = 0; i < string_map.size(); i++) {
		//print_bl("saving string: "+strings[i]);
		save_ustring(fw, string_map[i]);
	}

	// save external resource table
	fw->store_32(external_resources.size()); // amount of external resources
	for (int i = 0; i < external_resources.size(); i++) {
		save_ustring(fw, external_resources[i].type);
		save_ustring(fw, external_resources[i].path);
		if (using_uids) {
			fw->store_64(external_resources[i].uid);
		}
		external_resources.ptrw()[i].save_order = i;
	}

	// save internal resource table
	fw->store_32(internal_resources.size()); // amount of internal resources
	Vector<uint64_t> ofs_pos;

	for (int i = 0; i < internal_resources.size(); i++) {
		Ref<Resource> re = get_internal_resource(internal_resources[i].path);

		ERR_FAIL_COND_V_MSG(re.is_null(), ERR_CANT_ACQUIRE_RESOURCE, "Can't find internal resource " + internal_resources[i].path);
		String path = get_resource_path(re);
		if (path == "" && internal_resources[i].path == local_path) {
			path = local_path;
		}
		if (path == "" || path.find("::") != -1) {
			save_ustring(fw, "local://" + re->get_scene_unique_id());
		} else if (path == local_path) {
			save_ustring(fw, local_path); // main resource
		} else {
			if (path.find("local://") == -1) {
				WARN_PRINT("Possible malformed path: " + path);
			}
			save_ustring(fw, path);
		}
		if (ver_format_bin == 4) {
			internal_resources.ptrw()[i].save_order = i;
		} else {
			internal_resources.ptrw()[i].save_order = i + 1;
		}

		ofs_pos.push_back(fw->get_position());
		fw->store_64(0); // offset in 64 bits
	}

	Vector<uint64_t> ofs_table;
	// now actually save the resources
	for (int i = 0; i < internal_resources.size(); i++) {
		Ref<Resource> re = get_internal_resource(internal_resources[i].path);
		ERR_FAIL_COND_V_MSG(re.is_null(), ERR_CANT_ACQUIRE_RESOURCE, "Can't find internal resource " + internal_resources[i].path);
		String path = get_resource_path(re);
		String rtype = get_internal_resource_type(path);
		List<ResourceProperty> lrp = get_internal_resource_properties(path);
		ofs_table.push_back(fw->get_position());
		save_ustring(fw, rtype);
		fw->store_32(lrp.size());
		for (auto F : lrp) {
			ERR_FAIL_COND_V_MSG(get_string_index(F.name) == -1, ERR_BUG,
					"Not in string map!");
			fw->store_32(get_string_index(F.name));
			write_variant_bin(fw, F.value);
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
	fw->flush();
	if (fw->get_error() != OK && fw->get_error() != ERR_FILE_EOF) {
		return ERR_CANT_CREATE;
	}

	return OK;
}

ResourceLoaderCompat::ResourceLoaderCompat() {}
ResourceLoaderCompat::~ResourceLoaderCompat() {}

void ResourceLoaderCompat::get_dependencies(Ref<FileAccess> p_f, List<String> *p_dependencies, bool p_add_types, bool only_paths) {
	open_bin(p_f);
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

Error ResourceLoaderCompat::open_text(Ref<FileAccess> p_f, bool p_skip_first_tag) {
	error = OK;

	lines = 1;
	f = p_f;

	stream.f = f;
	bool ignore_resource_parsing = false;
	resource_current = 0;

	VariantParser::Tag tag;
	Error err = VariantParserCompat::parse_tag(&stream, lines, error_text, tag);

	if (err) {
		error = err;
		_printerr();
		return err;
	}

	if (tag.fields.has("format")) {
		ver_format_text = tag.fields["format"];
		// text FORMAT_VERSION
		if (ver_format_text > 3) {
			error_text = "Saved with newer format version";
			_printerr();
			error = ERR_PARSE_ERROR;
			return error;
			// Text format
		} else if (ver_format_text == 3) {
			using_named_scene_ids = true;
			using_uids = true;
			using_script_class = true;
			engine_ver_major = engine_ver_major == 0 ? 4 : engine_ver_major;
			ver_format_bin = 5;
		} else if (ver_format_text == 2) {
			engine_ver_major = engine_ver_major == 0 ? 3 : engine_ver_major;
			ver_format_bin = 3;
		} else if (ver_format_text == 1) {
			engine_ver_major = engine_ver_major == 0 ? 2 : engine_ver_major;
			ver_format_bin = 1;
		} else {
			ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED, "illegal format version!");
		}
	}

	if (tag.name == "gd_scene") {
		is_scene = true;
		res_type = "PackedScene";
	} else if (tag.name == "gd_resource") {
		if (!tag.fields.has("type")) {
			error_text = "Missing 'type' field in 'gd_resource' tag";
			_printerr();
			error = ERR_PARSE_ERROR;
			return error;
		}

		if (tag.fields.has("script_class")) {
			script_class = tag.fields["script_class"];
		}

		res_type = tag.fields["type"];

	} else {
		error_text = "Unrecognized file type: " + tag.name;
		_printerr();
		error = ERR_PARSE_ERROR;
		return error;
	}

	if (tag.fields.has("uid")) {
		res_uid = ResourceUID::get_singleton()->text_to_id(tag.fields["uid"]);
	} else {
		res_uid = ResourceUID::INVALID_ID;
	}

	if (tag.fields.has("load_steps")) {
		resources_total = tag.fields["load_steps"];
	} else {
		resources_total = 0;
	}

	if (!p_skip_first_tag) {
		err = VariantParserCompat::parse_tag(&stream, lines, error_text, next_tag, &rp);

		if (err) {
			error_text = "Unexpected end of file";
			_printerr();
			error = ERR_FILE_CORRUPT;
		}
	}
	return error;
}

Error ResourceLoaderCompat::_parse_sub_resource_dummys(void *p_self, VariantParser::Stream *p_stream, Ref<Resource> &r_res, int &line, String &r_err_str) {
	ResourceLoaderCompat *rsi = (ResourceLoaderCompat *)p_self;
	return rsi->_parse_sub_resource_dummy(p_stream, r_res, line, r_err_str);
}

Error ResourceLoaderCompat::_parse_ext_resource_dummys(void *p_self, VariantParser::Stream *p_stream, Ref<Resource> &r_res, int &line, String &r_err_str) {
	ResourceLoaderCompat *rsi = (ResourceLoaderCompat *)p_self;
	return rsi->_parse_ext_resource_dummy(p_stream, r_res, line, r_err_str);
}

Error ResourceLoaderCompat::_parse_sub_resource_dummy(VariantParser::Stream *p_stream, Ref<Resource> &r_res, int &line, String &r_err_str) {
	VariantParser::Token token;
	VariantParser::get_token(p_stream, token, line, r_err_str);
	if (token.type != VariantParser::TK_NUMBER && token.type != VariantParser::TK_STRING) {
		r_err_str = "Expected number (old style) or string (sub-resource index)";
		return ERR_PARSE_ERROR;
	}

	String unique_id = token.value;
	String path = "local://" + unique_id;
	if (!has_internal_resource(path)) {
		r_err_str = "unique_id referenced before mapped, sub-resources stored out of order in resource file";
		return ERR_PARSE_ERROR;
	}

	r_res = get_internal_resource(path);

	VariantParser::get_token(p_stream, token, line, r_err_str);
	if (token.type != VariantParser::TK_PARENTHESIS_CLOSE) {
		r_err_str = "Expected ')'";
		return ERR_PARSE_ERROR;
	}

	return OK;
}

Error ResourceLoaderCompat::_parse_ext_resource_dummy(VariantParser::Stream *p_stream, Ref<Resource> &r_res, int &line, String &r_err_str) {
	VariantParser::Token token;
	VariantParser::get_token(p_stream, token, line, r_err_str);
	if (token.type != VariantParser::TK_NUMBER && token.type != VariantParser::TK_STRING) {
		r_err_str = "Expected number (old style sub-resource index) or String (ext-resource ID)";
		return ERR_PARSE_ERROR;
	}

	String id = token.value;

	r_res = get_external_resource_by_id(id);
	if (r_res.is_null()) {
		r_err_str = "External resource" + id + "not cached";
		return ERR_PARSE_ERROR;
	}
	VariantParser::get_token(p_stream, token, line, r_err_str);
	if (token.type != VariantParser::TK_PARENTHESIS_CLOSE) {
		r_err_str = "Expected ')'";
		return ERR_PARSE_ERROR;
	}

	return OK;
}

Ref<PackedScene> ResourceLoaderCompat::_parse_node_tag(VariantParser::ResourceParser &parser, List<ResourceProperty> &lrp) {
	Ref<PackedScene> packed_scene;
	if (fake_load == true) {
		Ref<FakeResource> fr;
		fr.instantiate();
		packed_scene = fr;
	} else {
		packed_scene.instantiate();
	}
	while (true) {
		if (next_tag.name == "node") {
			int parent = -1;
			int owner = -1;
			int type = -1;
			int name = -1;
			int instance = -1;
			int index = -1;
			// int base_scene=-1;

			if (next_tag.fields.has("name")) {
				name = packed_scene->get_state()->add_name(next_tag.fields["name"]);
			}

			if (next_tag.fields.has("parent")) {
				NodePath np = next_tag.fields["parent"];
				np.prepend_period(); // compatible to how it manages paths internally
				parent = packed_scene->get_state()->add_node_path(np);
			}

			if (next_tag.fields.has("type")) {
				type = packed_scene->get_state()->add_name(next_tag.fields["type"]);
			} else {
				type = SceneState::TYPE_INSTANTIATED; // no type? assume this was instantiated
			}

			if (next_tag.fields.has("instance")) {
				instance = packed_scene->get_state()->add_value(next_tag.fields["instance"]);

				if (packed_scene->get_state()->get_node_count() == 0 && parent == -1) {
					packed_scene->get_state()->set_base_scene(instance);
					instance = -1;
				}
			}

			if (next_tag.fields.has("instance_placeholder")) {
				String path = next_tag.fields["instance_placeholder"];

				int path_v = packed_scene->get_state()->add_value(path);

				if (packed_scene->get_state()->get_node_count() == 0) {
					error = ERR_FILE_CORRUPT;
					error_text = "Instance Placeholder can't be used for inheritance.";
					_printerr();
					return Ref<PackedScene>();
				}

				instance = path_v | SceneState::FLAG_INSTANCE_IS_PLACEHOLDER;
			}

			if (next_tag.fields.has("owner")) {
				owner = packed_scene->get_state()->add_node_path(next_tag.fields["owner"]);
			} else {
				if (parent != -1 && !(type == SceneState::TYPE_INSTANTIATED && instance == -1)) {
					owner = 0; // if no owner, owner is root
				}
			}

			if (next_tag.fields.has("index")) {
				index = next_tag.fields["index"];
			}

			int node_id = packed_scene->get_state()->add_node(parent, owner, type, name, instance, index);

			if (next_tag.fields.has("groups")) {
				Array groups = next_tag.fields["groups"];
				for (int i = 0; i < groups.size(); i++) {
					packed_scene->get_state()->add_node_group(node_id, packed_scene->get_state()->add_name(groups[i]));
				}
			}

			while (true) {
				String assign;
				Variant value;

				error = VariantParserCompat::parse_tag_assign_eof(&stream, lines, error_text, next_tag, assign, value, &parser);

				if (error) {
					if (error != ERR_FILE_EOF) {
						_printerr();
						return Ref<PackedScene>();
					} else {
						error = OK;
						return packed_scene;
					}
				}

				if (!assign.is_empty()) {
					int nameidx = packed_scene->get_state()->add_name(assign);
					int valueidx = packed_scene->get_state()->add_value(value);
					packed_scene->get_state()->add_node_property(node_id, nameidx, valueidx);

					// We add the values to the resourceproperty list too, as while object properties will be correctly set,
					// we can't get them out of the packed_scene if they're not instanced without traversing the node list,
					// packed_scene->get() will just return null
					ResourceProperty rp;
					rp.name = assign;
					rp.type = value.get_type();
					if (rp.type == Variant::OBJECT) {
						Object *obj = value;
						rp.class_name = obj->get_class_name();
					}
					rp.value = value;
					lrp.push_back(rp);
					// it's assignment
				} else if (!next_tag.name.is_empty()) {
					break;
				}
			}
		} else if (next_tag.name == "connection") {
			if (!next_tag.fields.has("from")) {
				error = ERR_FILE_CORRUPT;
				error_text = "missing 'from' field from connection tag";
				return Ref<PackedScene>();
			}

			if (!next_tag.fields.has("to")) {
				error = ERR_FILE_CORRUPT;
				error_text = "missing 'to' field from connection tag";
				return Ref<PackedScene>();
			}

			if (!next_tag.fields.has("signal")) {
				error = ERR_FILE_CORRUPT;
				error_text = "missing 'signal' field from connection tag";
				return Ref<PackedScene>();
			}

			if (!next_tag.fields.has("method")) {
				error = ERR_FILE_CORRUPT;
				error_text = "missing 'method' field from connection tag";
				return Ref<PackedScene>();
			}

			NodePath from = next_tag.fields["from"];
			NodePath to = next_tag.fields["to"];
			StringName method = next_tag.fields["method"];
			StringName signal = next_tag.fields["signal"];
			int flags = Object::CONNECT_PERSIST;
			Array binds;
			int unbinds = 0;

			if (next_tag.fields.has("flags")) {
				flags = next_tag.fields["flags"];
			}

			if (next_tag.fields.has("binds")) {
				binds = next_tag.fields["binds"];
			}
			if (next_tag.fields.has("unbinds")) {
				unbinds = next_tag.fields["unbinds"];
			}

			Vector<int> bind_ints;
			for (int i = 0; i < binds.size(); i++) {
				bind_ints.push_back(packed_scene->get_state()->add_value(binds[i]));
			}

			packed_scene->get_state()->add_connection(
					packed_scene->get_state()->add_node_path(from.simplified()),
					packed_scene->get_state()->add_node_path(to.simplified()),
					packed_scene->get_state()->add_name(signal),
					packed_scene->get_state()->add_name(method),
					flags,
					unbinds,
					bind_ints);

			error = VariantParserCompat::parse_tag(&stream, lines, error_text, next_tag, &parser);

			if (error) {
				if (error != ERR_FILE_EOF) {
					_printerr();
					return Ref<PackedScene>();
				} else {
					error = OK;
					return packed_scene;
				}
			}
		} else if (next_tag.name == "editable") {
			if (!next_tag.fields.has("path")) {
				error = ERR_FILE_CORRUPT;
				error_text = "missing 'path' field from editable tag";
				_printerr();
				return Ref<PackedScene>();
			}

			NodePath path = next_tag.fields["path"];

			packed_scene->get_state()->add_editable_instance(path.simplified());

			error = VariantParserCompat::parse_tag(&stream, lines, error_text, next_tag, &parser);

			if (error) {
				if (error != ERR_FILE_EOF) {
					_printerr();
					return Ref<PackedScene>();
				} else {
					error = OK;
					return packed_scene;
				}
			}
		} else {
			error = ERR_FILE_CORRUPT;
			_printerr();
			return Ref<PackedScene>();
		}
	}
}

Error ResourceLoaderCompat::fake_load_text() {
	if (error) {
		return error;
	}

	VariantParser::ResourceParser rp;
	String main_path;
	rp.ext_func = _parse_ext_resource_dummys;
	rp.sub_func = _parse_sub_resource_dummys;
	rp.userdata = this;
	int lexndex = 0;
	HashMap<int, int> ext_id_remap;
	while (next_tag.name == "ext_resource") {
		if (!next_tag.fields.has("path")) {
			error = ERR_FILE_CORRUPT;
			error_text = "Missing 'path' in external resource tag";
			_printerr();
			return error;
		}

		if (!next_tag.fields.has("type")) {
			error = ERR_FILE_CORRUPT;
			error_text = "Missing 'type' in external resource tag";
			_printerr();
			return error;
		}

		if (!next_tag.fields.has("id")) {
			error = ERR_FILE_CORRUPT;
			error_text = "Missing 'id' in external resource tag";
			_printerr();
			return error;
		}
		ExtResource er;
		er.path = next_tag.fields["path"];
		er.type = next_tag.fields["type"];
		er.id = next_tag.fields["id"];
		if (next_tag.fields.has("uid")) {
			String uidt = next_tag.fields["uid"];
			er.uid = ResourceUID::get_singleton()->text_to_id(uidt);
		}
		external_resources.push_back(er);
		error = load_ext_resource(external_resources.size() - 1);
		if (error) {
			_printerr();
			return error;
		}

		lexndex++;
		error = VariantParser::parse_tag(&stream, lines, error_text, next_tag, &rp);

		if (error) {
			_printerr();
			return error;
		}
	}

	// now, save resources to a separate file, for now

	while (next_tag.name == "sub_resource" || next_tag.name == "resource") {
		String type;
		String id;
		bool main_res;
		IntResource ir;
		Ref<Resource> sub_res;
		List<ResourceProperty> lrp;

		if (next_tag.name == "sub_resource") {
			if (!next_tag.fields.has("type")) {
				error = ERR_FILE_CORRUPT;
				error_text = "Missing 'type' in external resource tag";
				_printerr();
				return error;
			}

			if (!next_tag.fields.has("id")) {
				error = ERR_FILE_CORRUPT;
				error_text = "Missing 'id' in external resource tag";
				_printerr();
				return error;
			}

			type = next_tag.fields["type"];
			id = next_tag.fields["id"];
			ir.path = "local://" + id;
			main_res = false;
		} else {
			type = res_type;
			String uid_text = ResourceUID::get_singleton()->id_to_text(res_uid);
			id = type + "_" + uid_text.replace("uid://", "").replace("<invalid>", "0");
			main_res = true;
			local_path = "local://" + id;
			ir.path = local_path;
		}
		ir.offset = 0;
		internal_resources.push_back(ir);
		internal_type_cache[ir.path] = type;
		internal_res_cache[ir.path] = instance_internal_resource(ir.path, type, id);
		if (main_res) {
			resource = internal_res_cache[ir.path];
		}
		while (true) {
			String assign;
			Variant value;

			error = VariantParserCompat::parse_tag_assign_eof(&stream, lines, error_text, next_tag, assign, value, &rp);

			if (error) {
				if (main_res && error == ERR_FILE_EOF) {
					next_tag.name = ""; // exit
					break;
				}

				_printerr();
				return error;
			}

			if (!assign.is_empty()) {
				ResourceProperty rprop;
				rprop.name = assign;
				rprop.type = value.get_type();
				rprop.value = value;
				if (rprop.type == Variant::OBJECT) {
					rprop.class_name = ((Object *)value)->get_class_name();
				}
				lrp.push_back(rprop);
			} else if (!next_tag.name.is_empty()) {
				error = OK;
				break;
			} else {
				error = ERR_FILE_CORRUPT;
				error_text = "Premature end of file while parsing [sub_resource]";
				_printerr();
				return error;
			}
		}
		// We keep a list of the properties loaded (which are only variants) in case of a fake load
		internal_index_cached_properties[ir.path] = lrp;
	}

	if (next_tag.name == "node") {
		// this is a node, must save one more!

		if (!is_scene) {
			error_text += "found the 'node' tag on a resource file!";
			_printerr();
			error = ERR_FILE_CORRUPT;
			return error;
		}
		List<ResourceProperty> lrp;
		Ref<PackedScene> packed_scene = _parse_node_tag(rp, lrp);

		if (!packed_scene.is_valid()) {
			return error;
		}
		resource = (Ref<FakeResource>)packed_scene;
		error = OK;
		// get it here
		IntResource ir;
		String uid = ResourceUID::get_singleton()->id_to_text(res_uid);
		String id = "PackedScene_" + uid.replace("uid://", "").replace("<invalid>", "0");

		ir.path = "local://" + id;
		local_path = ir.path;
		ir.offset = 0;
		internal_resources.push_back(ir);
		internal_type_cache[ir.path] = "PackedScene";
		((Ref<FakeResource>)packed_scene)->set_real_path(local_path);
		((Ref<FakeResource>)packed_scene)->set_real_type("PackedScene");

		internal_res_cache[ir.path] = packed_scene;
		// Right now, we're just handling converting back to bin, so clear lrp and store "_bundled";
		lrp.clear();
		ResourceProperty embed;
		embed.value = packed_scene->get_state()->get_bundled_scene();
		embed.type = embed.value.get_type();
		embed.name = "_bundled";
		lrp.push_back(embed);

		internal_index_cached_properties[ir.path] = lrp;
	}
	return OK;
}

String ResourceLoaderCompat::recognize(Ref<FileAccess> p_f) {
	error = OK;

	lines = 1;
	f = p_f;

	stream.f = f;

	VariantParser::Tag tag;
	Error err = VariantParser::parse_tag(&stream, lines, error_text, tag);

	if (err) {
		_printerr();
		return "";
	}

	if (tag.fields.has("format")) {
		int fmt = tag.fields["format"];
		if (fmt > 3) {
			error_text = "Saved with newer format version";
			_printerr();
			return "";
		}
	}

	if (tag.name == "gd_scene") {
		return "PackedScene";
	}

	if (tag.name != "gd_resource") {
		return "";
	}

	if (!tag.fields.has("type")) {
		error_text = "Missing 'type' field in 'gd_resource' tag";
		_printerr();
		return "";
	}

	return tag.fields["type"];
}
