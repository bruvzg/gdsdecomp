
/*************************************************************************/
/*  bytecode_d6b31da.h                                                   */
/*************************************************************************/

#ifndef GDSCRIPT_DECOMP_d6b31da_H
#define GDSCRIPT_DECOMP_d6b31da_H

#include "bytecode_base.h"

class GDScriptDecomp_d6b31da : public GDScriptDecomp {
	GDCLASS(GDScriptDecomp_d6b31da, GDScriptDecomp);

protected:
	static void _bind_methods(){};

	static const int bytecode_version = 13;

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
	GDScriptDecomp_d6b31da() {
		bytecode_rev = 0xd6b31da;
		engine_ver_major = 3;
		variant_ver_major = 3;
	}
};

#endif
