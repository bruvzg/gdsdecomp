#include "core/io/file_access.h"
#include "core/io/missing_resource.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"

#define META_PROPERTY_COMPAT_DATA "metadata/compat"
#define META_COMPAT "compat"

class CompatFormatLoader;
class CompatFormatSaver;
class ResourceCompatLoader {
public:
	enum LoadType {
		FAKE_LOAD,
		NON_GLOBAL_LOAD,
		GLTF_LOAD,
		REAL_LOAD
	};
	static Ref<Resource> fake_load(const String &p_path, const String &p_type_hint = "", Error *r_error = nullptr);
	static Ref<Resource> non_global_load(const String &p_path, const String &p_type_hint = "", Error *r_error = nullptr);
	static Ref<Resource> gltf_load(const String &p_path, const String &p_type_hint = "", Error *r_error = nullptr);
	static Ref<Resource> real_load(const String &p_path, const String &p_type_hint = "", ResourceFormatLoader::CacheMode p_cache_mode = ResourceFormatLoader::CACHE_MODE_REUSE, Error *r_error = nullptr);
	static void add_resource_format_loader(Ref<CompatFormatLoader> p_format_loader, bool p_at_front = false);
	static void remove_resource_format_loader(Ref<CompatFormatLoader> p_format_loader);
};

class CompatFormatLoader : public ResourceFormatLoader {
public:
	virtual Ref<Resource> custom_load(const String &p_path, ResourceCompatLoader::LoadType p_type, Error *r_error = nullptr) = 0;
	static Ref<Resource> create_missing_external_resource(const String &path, const String &type, const ResourceUID::ID uid, const String &scene_id = "") {
		Ref<MissingResource> res{ memnew(MissingResource) };
		res->set_original_class(type);
		res->set_path_cache(path);
		Dictionary compat;
		compat["uid"] = uid;
		compat["type"] = type;
		compat["cached_id"] = scene_id;
		compat["unloaded_external_resource"] = true;
		res->set_meta("compat", compat);
		res->set_recording_properties(true);
		return res;
	}

	static MissingResource *create_missing_main_resource(const String &path, const String &type, const ResourceUID::ID uid) {
		MissingResource *res{ memnew(MissingResource) };
		res->set_original_class(type);
		res->set_path_cache(path);
		Dictionary compat;
		compat["uid"] = uid;
		compat["type"] = type;
		compat["main_resource"] = true;
		res->set_meta("compat", compat);

		res->set_recording_properties(true);
		return res;
	}

	static MissingResource *create_missing_internal_resource(const String &path, const String &type, const String &scene_id) {
		MissingResource *res{ memnew(MissingResource) };
		res->set_original_class(type);
		// res->set_path_cache(path);
		res->set_scene_unique_id(scene_id);
		Dictionary compat;
		compat["uid"] = ResourceUID::INVALID_ID;
		compat["type"] = type;
		compat["internal_resource"] = true;
		res->set_meta("compat", compat);
		res->set_recording_properties(true);
		return res;
	}

	// virtual Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_IGNORE);
	// virtual void get_recognized_extensions_for_type(const String &p_type, List<String> *p_extensions) const;
	// virtual void get_recognized_extensions(List<String> *p_extensions) const;
	// virtual bool handles_type(const String &p_type) const;
	// virtual String get_resource_type(const String &p_path) const;
	// virtual String get_resource_script_class(const String &p_path) const;
	// virtual void get_classes_used(const String &p_path, HashSet<StringName> *r_classes);
	// virtual ResourceUID::ID get_resource_uid(const String &p_path) const;
	// virtual void get_dependencies(const String &p_path, List<String> *p_dependencies, bool p_add_types = false);
	// virtual Error rename_dependencies(const String &p_path, const HashMap<String, String> &p_map);
};

class CompatFormatSaver : public ResourceFormatSaver {
public:
	virtual Error fake_save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags = 0) = 0;
	virtual Error non_global_save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags = 0) = 0;

	// virtual Error save(const Ref<Resource> &p_resource, const String &p_path, uint32_t p_flags = 0);
	// virtual Error set_uid(const String &p_path, ResourceUID::ID p_uid);
	// virtual bool recognize(const Ref<Resource> &p_resource) const;
	// virtual void get_recognized_extensions(const Ref<Resource> &p_resource, List<String> *p_extensions) const;
	// virtual bool recognize_path(const Ref<Resource> &p_resource, const String &p_path) const;
};
