#ifndef VARIANT_DECODER_COMPAT_H
#define VARIANT_DECODER_COMPAT_H

#include "core/string/ustring.h"
#include <core/variant/variant.h>
namespace V2Type {
enum Type {

	NIL,

	// atomic types
	BOOL,
	INT,
	REAL, // changed to FLOAT in v4
	STRING,

	// math types

	VECTOR2, // 5
	RECT2,
	VECTOR3,
	MATRIX32, // changed to TRANSFORM2D in v3, v4
	PLANE,
	QUAT, // 10 -- changed to QUATERION in v3, v4
	_AABB, //sorry naming convention fail :( not like it's used often -- renamed to AABB in v3, v4
	MATRIX3, // changed to BASIS in v3, v4
	TRANSFORM, // changed to TRANSFORM3D in v4

	// misc types
	COLOR,
	IMAGE, // 15 -- removed in v3, v4
	NODE_PATH,
	_RID, // renamed to RID in v4
	OBJECT,
	INPUT_EVENT, // removed in v3, v4
	DICTIONARY, // 20
	ARRAY,

	// arrays
	RAW_ARRAY, // renamed to POOL_BYTE_ARRAY in v3, PACKED_BYTE_ARRAY in v4
	INT_ARRAY, // renamed to POOL_INT_ARRAY in v3, PACKED_INT32_ARRAY in v4
	REAL_ARRAY, // renamed to POOL_REAL_ARRAY in v3, PACKED_FLOAT32_ARRAY in v4
	STRING_ARRAY, // 25 -- renamed to POOL_STRING_ARRAY in v3, PACKED_STRING_ARRAY in v4
	VECTOR2_ARRAY, // renamed to POOL_VECTOR2_ARRAY in v3, PACKED_VECTOR2_ARRAY in v4
	VECTOR3_ARRAY, // renamed to POOL_VECTOR3_ARRAY in v3, PACKED_VECTOR3_ARRAY in v4
	COLOR_ARRAY, // renamed to POOL_COLOR_ARRAY in v3, PACKED_COLOR_ARRAY in v4

	VARIANT_MAX // 29

};
}

namespace V3Type {
enum Type {

	NIL,
	// atomic types
	BOOL,
	INT,
	REAL, // changed to FLOAT in v4
	STRING,
	// math types
	VECTOR2, // 5
	RECT2,
	VECTOR3,
	TRANSFORM2D,
	PLANE,
	QUAT, // 10 -- changed to QUATERION in v4
	AABB,
	BASIS,
	TRANSFORM, // changed to TRANSFORM3D in v4
	// misc types
	COLOR,
	NODE_PATH, // 15
	_RID, // renamed to RID in v4
	OBJECT,
	DICTIONARY,
	ARRAY,
	// arrays
	POOL_BYTE_ARRAY, // 20 -- renamed to PACKED_BYTE_ARRAY in v4
	POOL_INT_ARRAY, // renamed to PACKED_INT32_ARRAY in v4
	POOL_REAL_ARRAY, // renamed to PACKED_FLOAT32_ARRAY in v4
	POOL_STRING_ARRAY, // renamed to PACKED_STRING_ARRAY in v4
	POOL_VECTOR2_ARRAY, // renamed to PACKED_VECTOR2_ARRAY in v4
	POOL_VECTOR3_ARRAY, // 25 -- renamed to PACKED_VECTOR3_ARRAY in v4
	POOL_COLOR_ARRAY, // renamed to PACKED_COLOR_ARRAY in v4
	VARIANT_MAX // 27
};
}
/* PLEASE NOTE: This class is only used for decoding variants contained in compiled GDScript files
   and binary project files. This is NOT to be used for resources.*/
class VariantDecoderCompat {
public:
	static String get_variant_type_name_v3(int p_type);
	static String get_variant_type_name_v2(int p_type);
	static String get_variant_type_name(int p_type, int ver_major);

	static int get_variant_type_v3(String p_type_name);
	static int get_variant_type_v2(String p_type_name);
	static int get_variant_type(String p_type_name, int ver_major);

	static Variant::Type variant_type_enum_v2_to_v4(int type);
	static Variant::Type variant_type_enum_v3_to_v4(int type);
	static Variant::Type convert_variant_type_from_old(int type, int ver_major);

	static int variant_type_enum_v4_to_v2(Variant::Type type);
	static int variant_type_enum_v4_to_v3(Variant::Type type);
	static int convert_variant_type_to_old(Variant::Type type, int ver_major);

	static Error decode_variant_3(Variant &r_variant, const uint8_t *p_buffer, int p_len, int *r_len = nullptr, bool p_allow_objects = false);
	static Error decode_variant_2(Variant &r_variant, const uint8_t *p_buffer, int p_len, int *r_len = nullptr, bool p_allow_objects = false);
	static Error decode_variant_compat(int ver_major, Variant &r_variant, const uint8_t *p_buffer, int p_len, int *r_len = nullptr, bool p_allow_objects = false);

	static Error encode_variant_3(const Variant &p_variant, uint8_t *r_buffer, int &r_len, bool p_full_objects = false, int p_depth = 0);
	static Error encode_variant_2(const Variant &p_variant, uint8_t *r_buffer, int &r_len);
	static Error encode_variant_compat(int ver_major, const Variant &p_variant, uint8_t *r_buffer, int &r_len, bool p_full_objects = false, int p_depth = 0);
};
#endif // VARIANT_DECODER_COMPAT_H