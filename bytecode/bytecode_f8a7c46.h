
/*************************************************************************/
/*  bytecode_f8a7c46.h                                                   */
/*************************************************************************/

#ifndef GDSCRIPT_DECOMP_f8a7c46_H
#define GDSCRIPT_DECOMP_f8a7c46_H

#include "bytecode_base.h"

class GDScriptDecomp_f8a7c46 : public GDScriptDecomp {
	GDCLASS(GDScriptDecomp_f8a7c46, GDScriptDecomp);

protected:
	static void _bind_methods(){};

	static const int bytecode_version = 12;

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
