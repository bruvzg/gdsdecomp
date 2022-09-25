

#ifndef V2_IMAGE_PARSER_H
#define V2_IMAGE_PARSER_H

#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/resource.h"
#include "core/variant/variant.h"
#include "core/variant/variant_parser.h"

namespace V2Image {
enum Format {
	IMAGE_ENCODING_EMPTY = 0,
	IMAGE_ENCODING_RAW = 1,
	IMAGE_ENCODING_LOSSLESS = 2,
	IMAGE_ENCODING_LOSSY = 3,

	IMAGE_FORMAT_GRAYSCALE = 0,
	IMAGE_FORMAT_INTENSITY = 1,
	IMAGE_FORMAT_GRAYSCALE_ALPHA = 2,
	IMAGE_FORMAT_RGB = 3,
	IMAGE_FORMAT_RGBA = 4,
	IMAGE_FORMAT_INDEXED = 5,
	IMAGE_FORMAT_INDEXED_ALPHA = 6,
	FORMAT_YUV_422 = 7,
	FORMAT_YUV_444 = 8,
	IMAGE_FORMAT_BC1 = 9,
	IMAGE_FORMAT_BC2 = 10,
	IMAGE_FORMAT_BC3 = 11,
	IMAGE_FORMAT_BC4 = 12,
	IMAGE_FORMAT_BC5 = 13,
	IMAGE_FORMAT_PVRTC2 = 14,
	IMAGE_FORMAT_PVRTC2_ALPHA = 15,
	IMAGE_FORMAT_PVRTC4 = 16,
	IMAGE_FORMAT_PVRTC4_ALPHA = 17,
	IMAGE_FORMAT_ETC = 18,
	IMAGE_FORMAT_ATC = 19,
	IMAGE_FORMAT_ATC_ALPHA_EXPLICIT = 20,
	IMAGE_FORMAT_ATC_ALPHA_INTERPOLATED = 21,
	IMAGE_FORMAT_V2_MAX = 22,
	IMAGE_FORMAT_CUSTOM = 30
};

} // namespace V2Image

class ImageParserV2 {
public:
	static V2Image::Format new_format_to_v2_format(Image::Format p_format);
	static Image::Format v2_format_to_new_format(V2Image::Format p_format);
	static String get_format_name(V2Image::Format p_format);
	static String get_format_identifier(V2Image::Format p_format);
	static String get_format_identifier_pcfg(V2Image::Format p_format, int p_img_size);

	static V2Image::Format get_format_from_string(const String &fmt_name);
	static Ref<Image> convert_indexed_image(const Vector<uint8_t> &p_imgdata, int p_width, int p_height, int p_mipmaps, V2Image::Format p_format, Error *r_error);

	static String image_v2_to_string(const Variant &r_v, bool is_pcfg = false);
	static Error parse_image_construct_v2(VariantParser::Stream *f, Variant &r_v, bool convert_indexed, int &line, String &p_err_str);

	static Error decode_image_v2(Ref<FileAccess> f, Variant &r_v, bool convert_indexed = false);
	static Error write_image_v2_to_bin(Ref<FileAccess> f, const Variant &r_v, const PropertyHint p_hint);
};

#endif // V2_IMAGE_PARSER_H
