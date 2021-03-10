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
		set(key, value);
	}

	f->close();
	memdelete(f);
	return OK;
}



void ProjectConfigLoader::set_initial_value(const String &p_name, const Variant &p_value) {
	props[p_name].initial = p_value;
}
void ProjectConfigLoader::set_restart_if_changed(const String &p_name, bool p_restart) {
	props[p_name].restart_if_changed = p_restart;
}

void ProjectConfigLoader::set_builtin_order(const String &p_name) {
	if (props[p_name].order >= 65536) {
		props[p_name].order = last_builtin_order++;
	}
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

Error ProjectConfigLoader::_save_settings_text(const String &p_file, const Map<String, List<String> > &props) {

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

	for (Map<String, List<String> >::Element *E = props.front(); E; E = E->next()) {

		if (E != props.front())
			file->store_string("\n");

		if (E->key() != "")
			file->store_string("[" + E->key() + "]\n\n");
		for (List<String>::Element *F = E->get().front(); F; F = F->next()) {

			String key = F->get();
			if (E->key() != "")
				key = E->key() + "/" + key;
			Variant value;
			value = get(key);

			String vstr;
			VariantWriter::write_to_string(value, vstr);
			file->store_string(F->get().property_name_encode() + "=" + vstr + "\n");
		}
	}

	file->close();
	memdelete(file);

	return OK;
}

bool ProjectConfigLoader::has_setting(String p_var) const {
	return props.has(p_var);
}

Variant ProjectConfigLoader::g_set(const String &p_var, const Variant &p_default, bool p_restart_if_changed) {

	Variant ret;
	if (!has_setting(p_var)) {
		set(p_var, p_default);
	}
	ret = get(p_var);

	set_initial_value(p_var, p_default);
	set_builtin_order(p_var);
	set_restart_if_changed(p_var, p_restart_if_changed);
	return ret;
}

ProjectConfigLoader::ProjectConfigLoader() {
	// Initialization of engine variables should be done in the setup() method,
	// so that the values can be overridden from project.godot or project.binary.
	/*
	last_order = NO_BUILTIN_ORDER_BASE;
	last_builtin_order = 0;
	disable_feature_overrides = false;
	registering_order = true;

	Array events;
	Dictionary action;
	Ref<InputEventKey> key;
	Ref<InputEventJoypadButton> joyb;

	g_set("application/config/name", "");
	g_set("application/config/description", "");
	custom_prop_info["application/config/description"] = PropertyInfo(Variant::STRING, "application/config/description", PROPERTY_HINT_MULTILINE_TEXT);
	g_set("application/run/main_scene", "");
	custom_prop_info["application/run/main_scene"] = PropertyInfo(Variant::STRING, "application/run/main_scene", PROPERTY_HINT_FILE, "*.tscn,*.scn,*.res");
	g_set("application/run/disable_stdout", false);
	g_set("application/run/disable_stderr", false);
	g_set("application/config/use_custom_user_dir", false);
	g_set("application/config/custom_user_dir_name", "");
	g_set("application/config/project_settings_override", "");
	g_set("audio/default_bus_layout", "res://default_bus_layout.tres");
	custom_prop_info["audio/default_bus_layout"] = PropertyInfo(Variant::STRING, "audio/default_bus_layout", PROPERTY_HINT_FILE, "*.tres");

	PoolStringArray extensions = PoolStringArray();
	extensions.push_back("gd");
	if (Engine::get_singleton()->has_singleton("GodotSharp"))
		extensions.push_back("cs");
	extensions.push_back("shader");

	g_set("editor/search_in_file_extensions", extensions);
	custom_prop_info["editor/search_in_file_extensions"] = PropertyInfo(Variant::POOL_STRING_ARRAY, "editor/search_in_file_extensions");

	g_set("editor/script_templates_search_path", "res://script_templates");
	custom_prop_info["editor/script_templates_search_path"] = PropertyInfo(Variant::STRING, "editor/script_templates_search_path", PROPERTY_HINT_DIR);

	action = Dictionary();
	action["deadzone"] = Variant(0.5f);
	events = Array();
	key.instance();
	key->set_scancode(KEY_ENTER);
	events.push_back(key);
	key.instance();
	key->set_scancode(KEY_KP_ENTER);
	events.push_back(key);
	key.instance();
	key->set_scancode(KEY_SPACE);
	events.push_back(key);
	joyb.instance();
	joyb->set_button_index(JOY_BUTTON_0);
	events.push_back(joyb);
	action["events"] = events;
	g_set("input/ui_accept", action);
	input_presets.push_back("input/ui_accept");

	action = Dictionary();
	action["deadzone"] = Variant(0.5f);
	events = Array();
	key.instance();
	key->set_scancode(KEY_SPACE);
	events.push_back(key);
	joyb.instance();
	joyb->set_button_index(JOY_BUTTON_3);
	events.push_back(joyb);
	action["events"] = events;
	g_set("input/ui_select", action);
	input_presets.push_back("input/ui_select");

	action = Dictionary();
	action["deadzone"] = Variant(0.5f);
	events = Array();
	key.instance();
	key->set_scancode(KEY_ESCAPE);
	events.push_back(key);
	joyb.instance();
	joyb->set_button_index(JOY_BUTTON_1);
	events.push_back(joyb);
	action["events"] = events;
	g_set("input/ui_cancel", action);
	input_presets.push_back("input/ui_cancel");

	action = Dictionary();
	action["deadzone"] = Variant(0.5f);
	events = Array();
	key.instance();
	key->set_scancode(KEY_TAB);
	events.push_back(key);
	action["events"] = events;
	g_set("input/ui_focus_next", action);
	input_presets.push_back("input/ui_focus_next");

	action = Dictionary();
	action["deadzone"] = Variant(0.5f);
	events = Array();
	key.instance();
	key->set_scancode(KEY_TAB);
	key->set_shift(true);
	events.push_back(key);
	action["events"] = events;
	g_set("input/ui_focus_prev", action);
	input_presets.push_back("input/ui_focus_prev");

	action = Dictionary();
	action["deadzone"] = Variant(0.5f);
	events = Array();
	key.instance();
	key->set_scancode(KEY_LEFT);
	events.push_back(key);
	joyb.instance();
	joyb->set_button_index(JOY_DPAD_LEFT);
	events.push_back(joyb);
	action["events"] = events;
	g_set("input/ui_left", action);
	input_presets.push_back("input/ui_left");

	action = Dictionary();
	action["deadzone"] = Variant(0.5f);
	events = Array();
	key.instance();
	key->set_scancode(KEY_RIGHT);
	events.push_back(key);
	joyb.instance();
	joyb->set_button_index(JOY_DPAD_RIGHT);
	events.push_back(joyb);
	action["events"] = events;
	g_set("input/ui_right", action);
	input_presets.push_back("input/ui_right");

	action = Dictionary();
	action["deadzone"] = Variant(0.5f);
	events = Array();
	key.instance();
	key->set_scancode(KEY_UP);
	events.push_back(key);
	joyb.instance();
	joyb->set_button_index(JOY_DPAD_UP);
	events.push_back(joyb);
	action["events"] = events;
	g_set("input/ui_up", action);
	input_presets.push_back("input/ui_up");

	action = Dictionary();
	action["deadzone"] = Variant(0.5f);
	events = Array();
	key.instance();
	key->set_scancode(KEY_DOWN);
	events.push_back(key);
	joyb.instance();
	joyb->set_button_index(JOY_DPAD_DOWN);
	events.push_back(joyb);
	action["events"] = events;
	g_set("input/ui_down", action);
	input_presets.push_back("input/ui_down");

	action = Dictionary();
	action["deadzone"] = Variant(0.5f);
	events = Array();
	key.instance();
	key->set_scancode(KEY_PAGEUP);
	events.push_back(key);
	action["events"] = events;
	g_set("input/ui_page_up", action);
	input_presets.push_back("input/ui_page_up");

	action = Dictionary();
	action["deadzone"] = Variant(0.5f);
	events = Array();
	key.instance();
	key->set_scancode(KEY_PAGEDOWN);
	events.push_back(key);
	action["events"] = events;
	g_set("input/ui_page_down", action);
	input_presets.push_back("input/ui_page_down");

	action = Dictionary();
	action["deadzone"] = Variant(0.5f);
	events = Array();
	key.instance();
	key->set_scancode(KEY_HOME);
	events.push_back(key);
	action["events"] = events;
	g_set("input/ui_home", action);
	input_presets.push_back("input/ui_home");

	action = Dictionary();
	action["deadzone"] = Variant(0.5f);
	events = Array();
	key.instance();
	key->set_scancode(KEY_END);
	events.push_back(key);
	action["events"] = events;
	g_set("input/ui_end", action);
	input_presets.push_back("input/ui_end");

	custom_prop_info["display/window/handheld/orientation"] = PropertyInfo(Variant::STRING, "display/window/handheld/orientation", PROPERTY_HINT_ENUM, "landscape,portrait,reverse_landscape,reverse_portrait,sensor_landscape,sensor_portrait,sensor");
	custom_prop_info["rendering/threads/thread_model"] = PropertyInfo(Variant::INT, "rendering/threads/thread_model", PROPERTY_HINT_ENUM, "Single-Unsafe,Single-Safe,Multi-Threaded");
	custom_prop_info["physics/2d/thread_model"] = PropertyInfo(Variant::INT, "physics/2d/thread_model", PROPERTY_HINT_ENUM, "Single-Unsafe,Single-Safe,Multi-Threaded");
	custom_prop_info["rendering/quality/intended_usage/framebuffer_allocation"] = PropertyInfo(Variant::INT, "rendering/quality/intended_usage/framebuffer_allocation", PROPERTY_HINT_ENUM, "2D,2D Without Sampling,3D,3D Without Effects");

	g_set("debug/settings/profiler/max_functions", 16384);
	custom_prop_info["debug/settings/profiler/max_functions"] = PropertyInfo(Variant::INT, "debug/settings/profiler/max_functions", PROPERTY_HINT_RANGE, "128,65535,1");

	g_set("compression/formats/zstd/long_distance_matching", Compression::zstd_long_distance_matching);
	custom_prop_info["compression/formats/zstd/long_distance_matching"] = PropertyInfo(Variant::BOOL, "compression/formats/zstd/long_distance_matching");
	g_set("compression/formats/zstd/compression_level", Compression::zstd_level);
	custom_prop_info["compression/formats/zstd/compression_level"] = PropertyInfo(Variant::INT, "compression/formats/zstd/compression_level", PROPERTY_HINT_RANGE, "1,22,1");
	g_set("compression/formats/zstd/window_log_size", Compression::zstd_window_log_size);
	custom_prop_info["compression/formats/zstd/window_log_size"] = PropertyInfo(Variant::INT, "compression/formats/zstd/window_log_size", PROPERTY_HINT_RANGE, "10,30,1");

	g_set("compression/formats/zlib/compression_level", Compression::zlib_level);
	custom_prop_info["compression/formats/zlib/compression_level"] = PropertyInfo(Variant::INT, "compression/formats/zlib/compression_level", PROPERTY_HINT_RANGE, "-1,9,1");

	g_set("compression/formats/gzip/compression_level", Compression::gzip_level);
	custom_prop_info["compression/formats/gzip/compression_level"] = PropertyInfo(Variant::INT, "compression/formats/gzip/compression_level", PROPERTY_HINT_RANGE, "-1,9,1");

	// Would ideally be defined in an Android-specific file, but then it doesn't appear in the docs
	g_set("android/modules", "");

	using_datapack = false;*/
}



ProjectConfigLoader::~ProjectConfigLoader(){
}
