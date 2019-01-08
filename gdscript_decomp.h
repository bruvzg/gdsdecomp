/*************************************************************************/
/*  gdscript_decomp.h                                                    */
/*************************************************************************/

#ifndef GDSCRIPT_DECOMP_H
#define GDSCRIPT_DECOMP_H

#include "core/map.h"
#include "core/object.h"

#include "modules/gdscript/gdscript.h"
#include "modules/gdscript/gdscript_functions.h"
#include "modules/gdscript/gdscript_tokenizer.h"

class GDScriptDeComp : public Reference {

	GDCLASS(GDScriptDeComp, Reference);

protected:
	static void _bind_methods();

	String _parse(Vector<uint8_t> p_bytecode);

	_FORCE_INLINE_ void _ensure_space(String &p_code);

public:
	String load_byte_code_encrypted(const String &p_path, Vector<uint8_t> p_key);
	String load_byte_code(const String &p_path);
};

#endif
