/*************************************************************************/
/*  bytecode_514a3fb.h                                                   */
/*************************************************************************/

#ifndef GDSCRIPT_DECOMP_514a3fb_H
#define GDSCRIPT_DECOMP_514a3fb_H

#include "bytecode_base.h"

class GDScriptDecomp_514a3fb : public GDScriptDecomp {
	GDCLASS(GDScriptDecomp_514a3fb, GDScriptDecomp);

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
	virtual BYTECODE_TEST_RESULT test_bytecode(Vector<uint8_t> buffer) override;
	GDScriptDecomp_514a3fb() {
		bytecode_rev = 0x514a3fb;
		engine_ver_major = 3;
		variant_ver_major = 3;
	}
};

#endif
