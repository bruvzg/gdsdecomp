
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
	virtual Error decompile_buffer(Vector<uint8_t> p_buffer);
};

#endif
