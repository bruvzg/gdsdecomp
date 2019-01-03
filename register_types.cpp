/*************************************************************************/
/*  register_types.cpp                                                   */
/*************************************************************************/

#include "register_types.h"
#include "core/class_db.h"

#include "gdscript_decomp.h"

void register_gdsdecomp_types() {

	ClassDB::register_class<GDScriptDeComp>();
}

void unregister_gdsdecomp_types() {
	//NOP
}
