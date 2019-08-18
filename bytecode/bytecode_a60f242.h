/*************************************************************************/
/*  bytecode_a60f242.h                                                   */
/*************************************************************************/

#ifndef GDSCRIPT_DECOMP_a60f242_H
#define GDSCRIPT_DECOMP_a60f242_H

#include "bytecode_base.h"

class GDScriptDecomp_a60f242 : public GDScriptDecomp {
	GDCLASS(GDScriptDecomp_a60f242, GDScriptDecomp);

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
	virtual Error decompile_buffer(Vector<uint8_t> p_buffer);
};

#endif
