#include "webp_compat.h"
#include <webp/decode.h>
#include <webp/encode.h>

namespace WebPCompat {
Ref<Image> webp_unpack_v2v3(const Vector<uint8_t> &p_buffer) {
	int size = p_buffer.size() - 4;
	ERR_FAIL_COND_V(size <= 0, Ref<Image>());
	const uint8_t *r = p_buffer.ptr();

	ERR_FAIL_COND_V(r[0] != 'W' || r[1] != 'E' || r[2] != 'B' || r[3] != 'P', Ref<Image>());
	WebPBitstreamFeatures features;
	if (WebPGetFeatures(&r[4], size, &features) != VP8_STATUS_OK) {
		ERR_FAIL_V_MSG(Ref<Image>(), "Error unpacking WEBP image.");
	}

	Vector<uint8_t> dst_image;
	int datasize = features.width * features.height * (features.has_alpha ? 4 : 3);
	dst_image.resize(datasize);

	uint8_t *dst_w = dst_image.ptrw();

	bool errdec = false;
	if (features.has_alpha) {
		errdec = WebPDecodeRGBAInto(&r[4], size, dst_w, datasize, 4 * features.width) == nullptr;
	} else {
		errdec = WebPDecodeRGBInto(&r[4], size, dst_w, datasize, 3 * features.width) == nullptr;
	}

	ERR_FAIL_COND_V_MSG(errdec, Ref<Image>(), "Failed decoding WebP image.");

	Ref<Image> img = memnew(Image(features.width, features.height, 0, features.has_alpha ? Image::FORMAT_RGBA8 : Image::FORMAT_RGB8, dst_image));
	return img;
}

} //namespace WebPCompat