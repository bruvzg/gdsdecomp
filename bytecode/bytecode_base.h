/*************************************************************************/
/*  bytecode_base.h                                                      */
/*************************************************************************/

#ifndef GDSCRIPT_DECOMP_H
#define GDSCRIPT_DECOMP_H

#include "compat/variant_decoder_compat.h"

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/templates/rb_map.h"

class GDScriptDecomp : public Object {
	GDCLASS(GDScriptDecomp, Object);

protected:
	static void _bind_methods();
	static void _ensure_space(String &p_code);

	String script_text;
	String error_message;

public:
	int engine_ver_major = 4;
	virtual Error decompile_buffer(Vector<uint8_t> p_buffer) = 0;
	Error decompile_byte_code_encrypted(const String &p_path, Vector<uint8_t> p_key, bool is_version_3 = false);
	Error decompile_byte_code(const String &p_path);

	String get_script_text();
	String get_error_message();
	String get_constant_string(Vector<Variant> &constants, uint32_t constId);
};

#endif
