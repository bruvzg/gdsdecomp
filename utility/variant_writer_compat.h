#ifndef VARIANT_WRITER_COMPAT_H
#define VARIANT_WRITER_COMPAT_H

#include "core/io/file_access.h"
#include "core/io/resource.h"
#include "core/variant/variant.h"

class VariantWriterCompat {
public:
	typedef Error (*StoreStringFunc)(void *ud, const String &p_string);
	typedef String (*EncodeResourceFunc)(void *ud, const Ref<Resource> &p_resource);
	static Error write_compat(const Variant &p_variant, const uint32_t ver_major, StoreStringFunc p_store_string_func, void *p_store_string_ud, EncodeResourceFunc p_encode_res_func, void *p_encode_res_ud, bool is_pcfg);
	static Error write_to_string(const Variant &p_variant, String &r_string, const uint32_t ver_major, EncodeResourceFunc p_encode_res_func = nullptr, void *p_encode_res_ud = nullptr);
	static Error write_to_string_pcfg(const Variant &p_variant, String &r_string, const uint32_t ver_major, EncodeResourceFunc p_encode_res_func = nullptr, void *p_encode_res_ud = nullptr);
};

#endif // VARIANT_WRITER_COMPAT_H
