//woop

#ifndef PCFG_LOADER_H
#define PCFG_LOADER_H

#include <core/object/object.h>
#include "core/object/class_db.h"


typedef Map<String, Variant> CustomMap;

class ProjectConfigLoader : Object {
    GDCLASS(ProjectConfigLoader, Object);
	struct VariantContainer {
		int order;
		bool persist;
		Variant variant;
		Variant initial;
		bool hide_from_editor;
		bool overridden;
		bool restart_if_changed;
		VariantContainer() :
				order(0),
				persist(false),
				hide_from_editor(false),
				overridden(false),
				restart_if_changed(false) {
		}
		VariantContainer(const Variant &p_variant, int p_order, bool p_persist = false) :
				order(p_order),
				persist(p_persist),
				variant(p_variant),
				hide_from_editor(false),
				overridden(false),
				restart_if_changed(false) {
		}
	};
	Map<StringName, VariantContainer> props;
	Map<StringName, PropertyInfo> custom_prop_info;
	String cfb_path;
	int last_builtin_order;

    public:
        Error load_cfb(const String path, uint32_t ver_major, uint32_t ver_minor);
		Error save_cfb(const String dir, uint32_t ver_major, uint32_t ver_minor);
		Error _load_settings_binary(const String &p_path, uint32_t ver_major);
		void set_initial_value(const String &p_name, const Variant &p_value);
		void set_restart_if_changed(const String &p_name, bool p_restart);
		void set_builtin_order(const String &p_name);
		
		Error save_custom(const String &p_path, const uint32_t ver_major, const uint32_t ver_minor);
		Error _save_settings_text(const String &p_file, const Map<String, List<String> > &props, const uint32_t ver_major, const uint32_t ver_minor);
		Error _save_settings_text(const String &p_file);
		bool has_setting(String p_var) const;
		Variant g_set(const String &p_var, const Variant &p_default, bool p_restart_if_changed = false);
    ProjectConfigLoader();
    ~ProjectConfigLoader();
};
#endif
