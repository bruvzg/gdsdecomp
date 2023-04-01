/*************************************************************************/
/*  bytecode_506df14.h                                                   */
/*************************************************************************/

#ifndef GDSCRIPT_DECOMP_506df14_H
#define GDSCRIPT_DECOMP_506df14_H

#include "bytecode_base.h"

class GDScriptDecomp_506df14 : public GDScriptDecomp {
	GDCLASS(GDScriptDecomp_506df14, GDScriptDecomp);

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
	GDScriptDecomp_506df14() {
		bytecode_rev = 0x506df14;
		engine_ver_major = 4;
		variant_ver_major = 3;
	}
};

#endif
