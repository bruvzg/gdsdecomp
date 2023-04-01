
/*************************************************************************/
/*  bytecode_3ea6d9f.h                                                   */
/*************************************************************************/

#ifndef GDSCRIPT_DECOMP_3ea6d9f_H
#define GDSCRIPT_DECOMP_3ea6d9f_H

#include "bytecode_base.h"

class GDScriptDecomp_3ea6d9f : public GDScriptDecomp {
	GDCLASS(GDScriptDecomp_3ea6d9f, GDScriptDecomp);

protected:
	static void _bind_methods(){};

	static const int bytecode_version = 12;

	enum {
		TOKEN_BYTE_MASK = 0x80,
		TOKEN_BITS = 8,
		TOKEN_MASK = (1 << TOKEN_BITS) - 1,
		TOKEN_LINE_BITS = 24,
		TOKEN_LINE_MASK = (1 << TOKEN_LINE_BITS) - 1,
	};

public:
	virtual Error decompile_buffer(Vector<uint8_t> p_buffer) override;
	virtual BYTECODE_TEST_RESULT test_bytecode(Vector<uint8_t> buffer) override { return BYTECODE_TEST_RESULT::BYTECODE_TEST_UNKNOWN; }; // not implemented
	GDScriptDecomp_3ea6d9f() {
		bytecode_rev = 0x3ea6d9f;
		engine_ver_major = 3;
		variant_ver_major = 3;
	}
};

#endif
