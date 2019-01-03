/*************************************************************************/
/*  gdscript_decomp.h                                                    */
/*************************************************************************/

#ifndef GDSCRIPT_DECOMP_H
#define GDSCRIPT_DECOMP_H

#include "core/map.h"
#include "core/object.h"

#include "modules/gdscript/gdscript.h"
#include "modules/gdscript/gdscript_tokenizer.h"
#include "modules/gdscript/gdscript_functions.h"

class GDScriptDeComp : public Reference {

	GDCLASS(GDScriptDeComp, Reference);

protected:
	static void _bind_methods();

	String _parse(Vector<uint8_t> p_bytecode);

public:
	String load_byte_code_encrypted(const String &p_path, Vector<uint8_t> p_key);
	String load_byte_code(const String &p_path);
};

#endif
