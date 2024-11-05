#pragma once
#include "core/io/file_access.h"
#include "core/io/marshalls.h"

class Image;
namespace gdre {
bool check_header(const Vector<uint8_t> &p_buffer, const char *p_expected_header, int p_expected_len);
Error ensure_dir(const String &dst_dir);
Error save_image_as_tga(const String &p_path, const Ref<Image> &p_img);
Error save_image_as_webp(const String &p_path, const Ref<Image> &p_img, bool lossy = false);
Error save_image_as_jpeg(const String &p_path, const Ref<Image> &p_img);
void get_strings_from_variant(const Variant &p_var, Vector<String> &r_strings, const String &engine_version = "");
Error decompress_image(const Ref<Image> &img);
template <class T>
Vector<T> hashset_to_vector(const HashSet<T> &hs) {
	Vector<T> ret;
	for (const T &E : hs) {
		ret.push_back(E);
	}
	return ret;
}

} // namespace gdre

#define GDRE_ERR_DECOMPRESS_OR_FAIL(img)                                    \
	{                                                                       \
		Error err = gdre::decompress_image(img);                            \
		if (err == ERR_UNAVAILABLE) {                                       \
			return ERR_UNAVAILABLE;                                         \
		}                                                                   \
		ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to decompress image."); \
	}
// Can only pass in string literals
#define _GDRE_CHECK_HEADER(p_buffer, p_expected_header) gdre::check_header(p_buffer, p_expected_header, sizeof(p_expected_header) - 1)