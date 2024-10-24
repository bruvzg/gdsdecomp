#include "resource_loader_compat.h"
#include "compat/resource_compat_binary.h"
#include "compat/resource_compat_text.h"
#include "core/error/error_list.h"
#include "core/error/error_macros.h"
#include "utility/gdre_settings.h"
#include "utility/resource_info.h"

Ref<CompatFormatLoader> ResourceCompatLoader::loader[ResourceCompatLoader::MAX_LOADERS];
int ResourceCompatLoader::loader_count = 0;
bool ResourceCompatLoader::doing_gltf_load = false;

#define FAIL_LOADER_NOT_FOUND(loader)                                                                                                                        \
	if (loader.is_null()) {                                                                                                                                  \
		if (r_error) {                                                                                                                                       \
			*r_error = ERR_FILE_NOT_FOUND;                                                                                                                   \
		}                                                                                                                                                    \
		ERR_FAIL_V_MSG(Ref<Resource>(), "Failed to load resource '" + p_path + "'. ResourceFormatLoader::load was not implemented for this resource type."); \
		return Ref<Resource>();                                                                                                                              \
	}

Ref<Resource> ResourceCompatLoader::fake_load(const String &p_path, const String &p_type_hint, Error *r_error) {
	Ref<CompatFormatLoader> loadr;
	for (int i = 0; i < loader_count; i++) {
		if (loader[i]->recognize_path(p_path, p_type_hint) && loader[i]->handles_fake_load()) {
			loadr = loader[i];
		}
	}

	FAIL_LOADER_NOT_FOUND(loadr);
	return loadr->custom_load(p_path, ResourceInfo::LoadType::FAKE_LOAD, r_error);
}

Ref<Resource> ResourceCompatLoader::non_global_load(const String &p_path, const String &p_type_hint, Error *r_error) {
	auto loader = get_loader_for_path(p_path, p_type_hint);
	FAIL_LOADER_NOT_FOUND(loader);
	return loader->custom_load(p_path, ResourceInfo::LoadType::NON_GLOBAL_LOAD, r_error);
}

Ref<Resource> ResourceCompatLoader::gltf_load(const String &p_path, const String &p_type_hint, Error *r_error) {
	auto loader = get_loader_for_path(p_path, p_type_hint);
	FAIL_LOADER_NOT_FOUND(loader);
	doing_gltf_load = true;
	auto ret = loader->custom_load(p_path, ResourceInfo::LoadType::GLTF_LOAD, r_error);
	doing_gltf_load = false;
	return ret;
}

Ref<Resource> ResourceCompatLoader::real_load(const String &p_path, const String &p_type_hint, ResourceFormatLoader::CacheMode p_cache_mode, Error *r_error) {
	auto loader = get_loader_for_path(p_path, p_type_hint);
	FAIL_LOADER_NOT_FOUND(loader);
	return loader->custom_load(p_path, ResourceInfo::LoadType::REAL_LOAD, r_error);
}

void ResourceCompatLoader::add_resource_format_loader(Ref<CompatFormatLoader> p_format_loader, bool p_at_front) {
	ERR_FAIL_COND(p_format_loader.is_null());
	ERR_FAIL_COND(loader_count >= MAX_LOADERS);

	if (p_at_front) {
		for (int i = loader_count; i > 0; i--) {
			loader[i] = loader[i - 1];
		}
		loader[0] = p_format_loader;
		loader_count++;
	} else {
		loader[loader_count++] = p_format_loader;
	}
}

void ResourceCompatLoader::remove_resource_format_loader(Ref<CompatFormatLoader> p_format_loader) {
	ERR_FAIL_COND(p_format_loader.is_null());

	// Find loader
	int i = 0;
	for (; i < loader_count; ++i) {
		if (loader[i] == p_format_loader) {
			break;
		}
	}

	ERR_FAIL_COND(i >= loader_count); // Not found

	// Shift next loaders up
	for (; i < loader_count - 1; ++i) {
		loader[i] = loader[i + 1];
	}
	loader[loader_count - 1].unref();
	--loader_count;
}

//get_loader_for_path
Ref<CompatFormatLoader> ResourceCompatLoader::get_loader_for_path(const String &p_path, const String &p_type_hint) {
	for (int i = 0; i < loader_count; i++) {
		if (loader[i]->recognize_path(p_path, p_type_hint)) {
			return loader[i];
		}
	}
	return Ref<CompatFormatLoader>();
}

Error ResourceCompatLoader::to_text(const String &p_path, const String &p_dst, uint32_t p_flags) {
	auto loader = get_loader_for_path(p_path, "");
	ERR_FAIL_COND_V_MSG(loader.is_null(), ERR_FILE_NOT_FOUND, "Failed to load resource '" + p_path + "'. ResourceFormatLoader::load was not implemented for this resource type.");
	Error err;

	auto res = loader->custom_load(p_path, ResourceInfo::LoadType::FAKE_LOAD, &err);
	ERR_FAIL_COND_V_MSG(err != OK || res.is_null(), err, "Failed to load " + p_path);
	ResourceFormatSaverCompatTextInstance saver;
	return saver.save(p_dst, res, p_flags);
}

Error ResourceCompatLoader::to_binary(const String &p_path, const String &p_dst, uint32_t p_flags) {
	auto loader = get_loader_for_path(p_path, "");
	ERR_FAIL_COND_V_MSG(loader.is_null(), ERR_FILE_NOT_FOUND, "Failed to load resource '" + p_path + "'. ResourceFormatLoader::load was not implemented for this resource type.");
	Error err;

	auto res = loader->custom_load(p_path, ResourceInfo::LoadType::FAKE_LOAD, &err);
	ERR_FAIL_COND_V_MSG(err != OK || res.is_null(), err, "Failed to load " + p_path);
	ResourceFormatSaverCompatBinaryInstance saver;
	return saver.save(p_dst, res, p_flags);
}

// static ResourceInfo get_resource_info(const String &p_path, const String &p_type_hint = "", Error *r_error = nullptr);
ResourceInfo ResourceCompatLoader::get_resource_info(const String &p_path, const String &p_type_hint, Error *r_error) {
	auto loader = get_loader_for_path(p_path, p_type_hint);
	if (loader.is_null()) {
		if (r_error) {
			*r_error = ERR_FILE_NOT_FOUND;
		}
		ERR_PRINT("Failed to load resource '" + p_path + "'. ResourceFormatLoader::load was not implemented for this resource type.");
		return ResourceInfo();
	}
	return loader->get_resource_info(p_path, r_error);
}