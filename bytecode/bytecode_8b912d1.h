
/*************************************************************************/
/*  bytecode_8b912d1.h                                                   */
/*************************************************************************/

#ifndef GDSCRIPT_DECOMP_8b912d1_H
#define GDSCRIPT_DECOMP_8b912d1_H

#include "bytecode_base.h"

class GDScriptDecomp_8b912d1 : public GDScriptDecomp {
	GDCLASS(GDScriptDecomp_8b912d1, GDScriptDecomp);

protected:
	static void _bind_methods(){};

	static const int bytecode_version = 11;

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
