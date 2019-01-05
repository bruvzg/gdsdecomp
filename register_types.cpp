/*************************************************************************/
/*  register_types.cpp                                                   */
/*************************************************************************/

#include "register_types.h"
#include "core/class_db.h"

#include "gdscript_decomp.h"

#ifdef TOOLS_ENABLED
#include "editor/gdre_editor.h"

void gdsdecomp_init_callback() {

	EditorNode *editor = EditorNode::get_singleton();
	editor->add_child(memnew(GodotREEditor(editor)));
};
#endif

void register_gdsdecomp_types() {

	ClassDB::register_class<GDScriptDeComp>();

#ifdef TOOLS_ENABLED
	ClassDB::register_class<PackDialog>();
	ClassDB::register_class<ScriptCompDialog>();
	ClassDB::register_class<ScriptDecompDialog>();

	EditorNode::add_init_callback(&gdsdecomp_init_callback);
#endif
}

void unregister_gdsdecomp_types() {
	//NOP
}
