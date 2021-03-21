

#include "core/io/resource.h"
#include "core/os/file_access.h"
#include "core/variant/variant.h"

class V2ImageParser{
public:
    static String ImageV2_to_string(const Variant &r_v);
    static Error parse_image_v2(FileAccess * f, Variant &r_v, bool hacks_for_dropped_fmt = true, bool convert_indexed = false);
    static Error V2ImageParser::write_v2image_to_bin(FileAccess* f, const Variant &r_v, const PropertyHint p_hint);
};
