#include "register_types.h"

#include "image_decompress_etcpak.h"

#include "core/io/image.h"

void initialize_etcpak_decompress_module(ModuleInitializationLevel p_level) {
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	DEV_ASSERT(Image::_image_decompress_etc1 == nullptr);
	DEV_ASSERT(Image::_image_decompress_etc2 == nullptr);
	Image::_image_decompress_etc1 = image_decompress_etc;
	Image::_image_decompress_etc2 = image_decompress_etc;
}

void uninitialize_etcpak_decompress_module(ModuleInitializationLevel p_level) {
	Image::_image_decompress_etc1 = nullptr;
	Image::_image_decompress_etc2 = nullptr;
}