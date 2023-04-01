
/*************************************************************************/
/*  bytecode_23441ec.h                                                   */
/*************************************************************************/

#ifndef GDSCRIPT_DECOMP_23441ec_H
#define GDSCRIPT_DECOMP_23441ec_H

#include "bytecode_base.h"

class GDScriptDecomp_23441ec : public GDScriptDecomp {
	GDCLASS(GDScriptDecomp_23441ec, GDScriptDecomp);

protected:
	static void _bind_methods(){};

	static const int bytecode_version = 10;

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
	GDScriptDecomp_23441ec() {
		bytecode_rev = 0x23441ec;
		engine_ver_major = 2;
		variant_ver_major = 2;
	}
};

#endif
