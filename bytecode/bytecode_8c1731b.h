
/*************************************************************************/
/*  bytecode_8c1731b.h                                                   */
/*************************************************************************/

#ifndef GDSCRIPT_DECOMP_8c1731b_H
#define GDSCRIPT_DECOMP_8c1731b_H

#include "bytecode_base.h"

class GDScriptDecomp_8c1731b : public GDScriptDecomp {
	GDCLASS(GDScriptDecomp_8c1731b, GDScriptDecomp);

protected:
	static void _bind_methods(){};

	static const int bytecode_version = 2;

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
	GDScriptDecomp_8c1731b() {
		bytecode_rev = 0x8c1731b;
		engine_ver_major = 1;
		variant_ver_major = 2; // we just use variant parser/writer for v2
	}
};

#endif
