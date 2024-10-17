#include "resource_loader_compat2.h"

Ref<MissingResource> ResourceCompatLoader::fake_load(const String &p_path, Error *r_error) {
	return Ref<MissingResource>();
}

Ref<Resource> ResourceCompatLoader::non_global_load(const String &p_path, bool special, Error *r_error) {
	return Ref<Resource>();
}

void ResourceCompatLoader::add_resource_format_loader(Ref<CompatFormatLoader> p_format_loader, bool p_at_front) {
}

void ResourceCompatLoader::remove_resource_format_loader(Ref<CompatFormatLoader> p_format_loader) {
}

Ref<Resource> ResourceCompatLoader::load(const String &p_path, const String &p_original_path, Error *r_error, bool p_use_sub_threads, float *r_progress, ResourceFormatLoader::CacheMode p_cache_mode) {
	return Ref<Resource>();
}
