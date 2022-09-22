/*************************************************************************/
/*  bytecode_base.h                                                      */
/*************************************************************************/

#ifndef GDSCRIPT_DECOMP_H
#define GDSCRIPT_DECOMP_H

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/templates/rb_map.h"
namespace V2Type {
enum Type {

	NIL,

	// atomic types
	BOOL,
	INT,
	REAL,
	STRING,

	// math types

	VECTOR2, // 5
	RECT2,
	VECTOR3,
	MATRIX32,
	PLANE,
	QUAT, // 10
	_AABB, //sorry naming convention fail :( not like it's used often
	MATRIX3,
	TRANSFORM,

	// misc types
	COLOR,
	IMAGE, // 15
	NODE_PATH,
	_RID,
	OBJECT,
	INPUT_EVENT,
	DICTIONARY, // 20
	ARRAY,

	// arrays
	RAW_ARRAY,
	INT_ARRAY,
	REAL_ARRAY,
	STRING_ARRAY, // 25
	VECTOR2_ARRAY,
	VECTOR3_ARRAY,
	COLOR_ARRAY,

	VARIANT_MAX

};
}

namespace V3Type {
enum Type {

	NIL,
	// atomic types
	BOOL,
	INT,
	REAL,
	STRING,
	// math types
	VECTOR2, // 5
	RECT2,
	VECTOR3,
	TRANSFORM2D,
	PLANE,
	QUAT, // 10
	AABB,
	BASIS,
	TRANSFORM,
	// misc types
	COLOR,
	NODE_PATH, // 15
	_RID,
	OBJECT,
	DICTIONARY,
	ARRAY,
	// arrays
	POOL_BYTE_ARRAY, // 20
	POOL_INT_ARRAY,
	POOL_REAL_ARRAY,
	POOL_STRING_ARRAY,
	POOL_VECTOR2_ARRAY,
	POOL_VECTOR3_ARRAY, // 25
	POOL_COLOR_ARRAY,
	VARIANT_MAX
};
}
class GDScriptDecomp : public Object {
	GDCLASS(GDScriptDecomp, Object);

protected:
	static void _bind_methods();
	static void _ensure_space(String &p_code);

	String script_text;
	String error_message;

public:
	virtual Error decompile_buffer(Vector<uint8_t> p_buffer) = 0;
	Error decompile_byte_code_encrypted(const String &p_path, Vector<uint8_t> p_key, bool is_version_3 = false);
	Error decompile_byte_code(const String &p_path);

	String get_script_text();
	String get_error_message();
	String get_constant_string(Vector<Variant> &constants, uint32_t constId);
	static String get_type_name_v3(int p_type);
	static String get_type_name_v2(int p_type);

	static Variant::Type variant_type_enum_v3_to_v4(const uint32_t type);

	static Error decode_variant_3(Variant &r_variant, const uint8_t *p_buffer, int p_len, int *r_len = nullptr, bool p_allow_objects = false);
	static Error decode_variant_2(Variant &r_variant, const uint8_t *p_buffer, int p_len, int *r_len = nullptr, bool p_allow_objects = false);
};

#endif
