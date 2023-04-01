
/*************************************************************************/
/*  bytecode_64872ca.h                                                   */
/*************************************************************************/

#ifndef GDSCRIPT_DECOMP_64872ca_H
#define GDSCRIPT_DECOMP_64872ca_H

#include "bytecode_base.h"

class GDScriptDecomp_64872ca : public GDScriptDecomp {
	GDCLASS(GDScriptDecomp_64872ca, GDScriptDecomp);

protected:
	static void _bind_methods(){};

	static const int bytecode_version = 8;

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
	GDScriptDecomp_64872ca() {
		bytecode_rev = 0x64872ca;
		engine_ver_major = 2;
		variant_ver_major = 2;
	}
};

#endif
