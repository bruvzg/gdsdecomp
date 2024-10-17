#include "core/io/file_access.h"
#include "core/io/missing_resource.h"
#include "core/io/resource_loader.h"
#include "core/io/resource_saver.h"
class CompatFormatLoader : public ResourceFormatLoader {
public:
	virtual Ref<MissingResource> fake_load(const String &p_path, Error *r_error = nullptr) = 0;
	virtual Ref<Resource> non_global_load(const String &p_path, bool special = false, Error *r_error = nullptr) = 0;
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

class ResourceCompatLoader {
	static Ref<MissingResource> fake_load(const String &p_path, Error *r_error = nullptr);
	static Ref<Resource> non_global_load(const String &p_path, bool special = false, Error *r_error = nullptr);
	static void add_resource_format_loader(Ref<CompatFormatLoader> p_format_loader, bool p_at_front = false);
	static void remove_resource_format_loader(Ref<CompatFormatLoader> p_format_loader);
	static Ref<Resource> load(const String &p_path, const String &p_original_path = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, ResourceFormatLoader::CacheMode p_cache_mode = ResourceFormatLoader::CACHE_MODE_IGNORE);
};