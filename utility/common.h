
#include "core/io/file_access.h"
#include "core/io/marshalls.h"

class Image;
namespace gdre {
bool check_header(const Vector<uint8_t> &p_buffer, const char *p_expected_header, int p_expected_len);
Error ensure_dir(const String &dst_dir);
Error save_image_as_tga(const String &p_path, const Ref<Image> &p_img);
Error save_image_as_webp(const String &p_path, const Ref<Image> &p_img, bool lossy = false);
Error save_image_as_jpeg(const String &p_path, const Ref<Image> &p_img);
static Ref<FileAccess> _____tmp_file;
} // namespace gdre

// Can only pass in string literals
#define _GDRE_CHECK_HEADER(p_buffer, p_expected_header) gdre::check_header(p_buffer, p_expected_header, sizeof(p_expected_header) - 1)