#include "pcfg_loader.h"
#include <core/os/input_event.h>
#include <core/os/input_event.h>
#include <core/engine.h>
#include <core/os/keyboard.h>
#include <core/io/compression.h>
#include <core/os/file_access.h>
#include <core/io/marshalls.h>
#include <core/variant_parser.h>

Error ProjectConfigLoader::load_cfb(const String path){
    cfb_path = path;
    return _load_settings_binary(path);
}
Error ProjectConfigLoader::save_cfb(const String dir){
	String file = "project.godot";
	return save_custom(dir.plus_file(file));
}

Error ProjectConfigLoader::_load_settings_binary(const String &p_path) {

	Error err;
	FileAccess *f = FileAccess::open(p_path, FileAccess::READ, &err);
	if (err != OK) {
		return err;
	}

	uint8_t hdr[4];
	f->get_buffer(hdr, 4);
	if (hdr[0] != 'E' || hdr[1] != 'C' || hdr[2] != 'F' || hdr[3] != 'G') {

		memdelete(f);
		ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, "Corrupted header in binary project.binary (not ECFG).");
	}

	uint32_t count = f->get_32();

	for (uint32_t i = 0; i < count; i++) {

		uint32_t slen = f->get_32();
		CharString cs;
		cs.resize(slen + 1);
		cs[slen] = 0;
		f->get_buffer((uint8_t *)cs.ptr(), slen);
		String key;
		key.parse_utf8(cs.ptr());

		uint32_t vlen = f->get_32();
		Vector<uint8_t> d;
		d.resize(vlen);
		f->get_buffer(d.ptrw(), vlen);
		Variant value;
		err = decode_variant(value, d.ptr(), d.size(), NULL, true);
		ERR_CONTINUE_MSG(err != OK, "Error decoding property: " + key + ".");
		props[key] = VariantContainer(value, last_builtin_order++, true);
	}

	f->close();
	memdelete(f);
	return OK;
}

struct _VCSort {

	String name;
	Variant::Type type;
	int order;
	int flags;

	bool operator<(const _VCSort &p_vcs) const { return order == p_vcs.order ? name < p_vcs.name : order < p_vcs.order; }
};

Error ProjectConfigLoader::save_custom(const String &p_path) {

	ERR_FAIL_COND_V_MSG(p_path == "", ERR_INVALID_PARAMETER, "Project settings save path cannot be empty.");

	Set<_VCSort> vclist;

	for (Map<StringName, VariantContainer>::Element *G = props.front(); G; G = G->next()) {

		const VariantContainer *v = &G->get();

		if (v->hide_from_editor)
			continue;

		_VCSort vc;
		vc.name = G->key(); //*k;
		vc.order = v->order;
		vc.type = v->variant.get_type();
		vc.flags = PROPERTY_USAGE_EDITOR | PROPERTY_USAGE_STORAGE;
		if (v->variant == v->initial)
			continue;

		vclist.insert(vc);
	}
	Map<String, List<String> > proops;

	for (Set<_VCSort>::Element *E = vclist.front(); E; E = E->next()) {

		String category = E->get().name;
		String name = E->get().name;

		int div = category.find("/");

		if (div < 0)
			category = "";
		else {

			category = category.substr(0, div);
			name = name.substr(div + 1, name.size());
		}
		proops[category].push_back(name);
	}

	return _save_settings_text(p_path, proops);
}

Error ProjectConfigLoader::_save_settings_text(const String &p_file, const Map<String, List<String> > &proops) {

	Error err;
	FileAccess *file = FileAccess::open(p_file, FileAccess::WRITE, &err);

	ERR_FAIL_COND_V_MSG(err != OK, err, "Couldn't save project.godot - " + p_file + ".");

	file->store_line("; Engine configuration file.");
	file->store_line("; It's best edited using the editor UI and not directly,");
	file->store_line("; since the parameters that go here are not all obvious.");
	file->store_line(";");
	file->store_line("; Format:");
	file->store_line(";   [section] ; section goes between []");
	file->store_line(";   param=value ; assign values to parameters");
	file->store_line("");

	file->store_string("config_version=" + itos(4) + "\n");

	file->store_string("\n");

	for (Map<String, List<String> >::Element *E = proops.front(); E; E = E->next()) {

		if (E != proops.front())
			file->store_string("\n");

		if (E->key() != "")
			file->store_string("[" + E->key() + "]\n\n");
		for (List<String>::Element *F = E->get().front(); F; F = F->next()) {

			String key = F->get();
			if (E->key() != "")
				key = E->key() + "/" + key;
			Variant value;
			value = props[key].variant;

			String vstr;
			VariantWriter::write_to_string(value, vstr);
			file->store_string(F->get().property_name_encode() + "=" + vstr + "\n");
		}
	}

	file->close();
	memdelete(file);

	return OK;
}

ProjectConfigLoader::ProjectConfigLoader() {
}

ProjectConfigLoader::~ProjectConfigLoader(){
}
