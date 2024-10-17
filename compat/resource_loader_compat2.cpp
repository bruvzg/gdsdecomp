#include "resource_loader_compat2.h"

Ref<Resource> ResourceCompatLoader::fake_load(const String &p_path, const String &p_type_hint, Error *r_error) {
	return Ref<Resource>();
}

Ref<Resource> ResourceCompatLoader::non_global_load(const String &p_path, const String &p_type_hint, Error *r_error) {
	return Ref<Resource>();
}

Ref<Resource> ResourceCompatLoader::gltf_load(const String &p_path, const String &p_type_hint, Error *r_error) {
	return Ref<Resource>();
}

void ResourceCompatLoader::add_resource_format_loader(Ref<CompatFormatLoader> p_format_loader, bool p_at_front) {
}

void ResourceCompatLoader::remove_resource_format_loader(Ref<CompatFormatLoader> p_format_loader) {
}

Ref<Resource> ResourceCompatLoader::real_load(const String &p_path, const String &p_type_hint, ResourceFormatLoader::CacheMode p_cache_mode, Error *r_error) {
	return Ref<Resource>();
}
