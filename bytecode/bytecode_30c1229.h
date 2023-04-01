
/*************************************************************************/
/*  bytecode_30c1229.h                                                   */
/*************************************************************************/

#ifndef GDSCRIPT_DECOMP_30c1229_H
#define GDSCRIPT_DECOMP_30c1229_H

#include "bytecode_base.h"

class GDScriptDecomp_30c1229 : public GDScriptDecomp {
	GDCLASS(GDScriptDecomp_30c1229, GDScriptDecomp);

protected:
	static void _bind_methods(){};

	static const int bytecode_version = 6;

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
	GDScriptDecomp_30c1229() {
		bytecode_rev = 0x30c1229;
		engine_ver_major = 2;
		variant_ver_major = 2;
	}
};

#endif
