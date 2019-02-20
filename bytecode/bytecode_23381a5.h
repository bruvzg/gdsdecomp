
/*************************************************************************/
/*  bytecode_23381a5.h                                                   */
/*************************************************************************/

#ifndef GDSCRIPT_DECOMP_23381a5_H
#define GDSCRIPT_DECOMP_23381a5_H

#include "bytecode_base.h"

class GDScriptDecomp_23381a5 : public GDScriptDecomp {
	GDCLASS(GDScriptDecomp_23381a5, GDScriptDecomp);

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
