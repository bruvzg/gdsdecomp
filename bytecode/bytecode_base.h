/*************************************************************************/
/*  bytecode_base.h                                                      */
/*************************************************************************/

#ifndef GDSCRIPT_DECOMP_H
#define GDSCRIPT_DECOMP_H

#include "core/map.h"
#include "core/object.h"

class GDScriptDecomp : public Object {
	GDCLASS(GDScriptDecomp, Object);

protected:
	static void _bind_methods();
	static void _ensure_space(String &p_code);

	String script_text;
	String error_message;

public:
	virtual Error decompile_buffer(Vector<uint8_t> p_buffer) = 0;
	Error decompile_byte_code_encrypted(const String &p_path, Vector<uint8_t> p_key);
	Error decompile_byte_code(const String &p_path);

	String get_script_text();
	String get_error_message();
	String get_constant_string(Vector<Variant>& constants, uint32_t constId);

	Error decode_variant_3(Variant &r_variant, const uint8_t *p_buffer, int p_len, int *r_len = nullptr, bool p_allow_objects = false);
	Error decode_variant_2(Variant &r_variant, const uint8_t *p_buffer, int p_len, int *r_len = nullptr, bool p_allow_objects = false);
};

#endif
