

#ifndef V2_IMAGE_PARSER_H
#define V2_IMAGE_PARSER_H

#include "core/io/file_access.h"
#include "core/io/resource.h"
#include "core/variant/variant.h"

class ImageParserV2 {
public:
	static String image_v2_to_string(const Variant &r_v, bool is_pcfg = false);
	static Error parse_image_v2(Ref<FileAccess> f, Variant &r_v, bool hacks_for_dropped_fmt = true, bool convert_indexed = false);
	static Error write_image_v2_to_bin(Ref<FileAccess> f, const Variant &r_v, const PropertyHint p_hint);
};

#endif //V2_IMAGE_PARSER_H
