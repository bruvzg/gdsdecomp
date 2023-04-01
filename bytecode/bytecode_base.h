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
	uint64_t bytecode_rev;
	int engine_ver_major;
	int variant_ver_major; // Some early dev versions of 3.0 used v2 variants, and early dev versions of 4.0 used v3 variants

	Pair<int, int> get_arg_count_for_builtin(String builtin_func_name);

public:
	enum BYTECODE_TEST_RESULT {
		BYTECODE_TEST_PASS,
		BYTECODE_TEST_FAIL,
		BYTECODE_TEST_CORRUPT,
		BYTECODE_TEST_UNKNOWN,
	};

	virtual Error decompile_buffer(Vector<uint8_t> p_buffer) = 0;
	virtual BYTECODE_TEST_RESULT test_bytecode(Vector<uint8_t> p_buffer) = 0;

	Error decompile_byte_code_encrypted(const String &p_path, Vector<uint8_t> p_key);
	Error decompile_byte_code(const String &p_path);

	static Error get_buffer_encrypted(const String &p_path, int engine_ver_major, Vector<uint8_t> p_key, Vector<uint8_t> &r_buffer);
	String get_script_text();
	String get_error_message();
	String get_constant_string(Vector<Variant> &constants, uint32_t constId);
	Error get_ids_consts_tokens(const Vector<uint8_t> &p_buffer, int bytecode_version, Vector<StringName> &r_identifiers, Vector<Variant> &r_constants, Vector<uint32_t> &r_tokens);
};

#endif
