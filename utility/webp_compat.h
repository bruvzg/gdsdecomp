
#ifndef WEBP_COMMON_H
#define WEBP_COMMON_H

#include "core/io/image.h"

namespace WebPCompat {
Ref<Image> webp_unpack_v2v3(const Vector<uint8_t> &p_buffer);
}
#endif //WEBP_COMMON_H