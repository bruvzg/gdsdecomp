/*************************************************************************/
/*  register_types.cpp                                                   */
/*************************************************************************/

#include "register_types.h"
#include "core/class_db.h"

#include "bytecode/bytecode_versions.h"
#include "editor/gdre_editor.h"

#ifdef TOOLS_ENABLED
void gdsdecomp_init_callback() {

	EditorNode *editor = EditorNode::get_singleton();
	editor->add_child(memnew(GodotREEditor(editor)));
};
#endif

void register_gdsdecomp_types() {

	ClassDB::register_virtual_class<GDScriptDecomp>();
	register_decomp_versions();

	ClassDB::register_class<GodotREEditorStandalone>();

	ClassDB::register_class<PackDialog>();
	ClassDB::register_class<NewPackDialog>();
	ClassDB::register_class<ScriptCompDialog>();
	ClassDB::register_class<ScriptDecompDialog>();
#ifdef TOOLS_ENABLED
	EditorNode::add_init_callback(&gdsdecomp_init_callback);
#endif
}

void unregister_gdsdecomp_types() {
	//NOP
}
