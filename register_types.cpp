/*************************************************************************/
/*  register_types.cpp                                                   */
/*************************************************************************/

#include "register_types.h"
#include "core/class_db.h"

#include "bytecode/bytecode_0_0_0.h"
#include "bytecode/bytecode_1_0_0.h"
#include "bytecode/bytecode_1_1_0.h"
#include "bytecode/bytecode_2_0_4.h"
#include "bytecode/bytecode_2_1_1.h"
#include "bytecode/bytecode_2_1_2.h"
#include "bytecode/bytecode_2_1_5.h"
#include "bytecode/bytecode_3_0_6.h"
#include "bytecode/bytecode_3_1_0.h"
#include "bytecode/bytecode_base.h"

#ifdef TOOLS_ENABLED
#include "editor/gdre_editor.h"

void gdsdecomp_init_callback() {

	EditorNode *editor = EditorNode::get_singleton();
	editor->add_child(memnew(GodotREEditor(editor)));
};
#endif

void register_gdsdecomp_types() {

	ClassDB::register_virtual_class<GDScriptDecomp>();
	ClassDB::register_class<GDScriptDecomp_3_1_0>();
	ClassDB::register_class<GDScriptDecomp_3_0_6>();
	ClassDB::register_class<GDScriptDecomp_2_1_5>();
	ClassDB::register_class<GDScriptDecomp_2_1_2>();
	ClassDB::register_class<GDScriptDecomp_2_1_1>();
	ClassDB::register_class<GDScriptDecomp_2_0_4>();
	ClassDB::register_class<GDScriptDecomp_1_1_0>();
	ClassDB::register_class<GDScriptDecomp_1_0_0>();
	ClassDB::register_class<GDScriptDecomp_0_0_0>();

#ifdef TOOLS_ENABLED
	ClassDB::register_class<GodotREEditorSt>();

	ClassDB::register_class<PackDialog>();
	ClassDB::register_class<NewPackDialog>();
	ClassDB::register_class<ScriptCompDialog>();
	ClassDB::register_class<ScriptDecompDialog>();

	EditorNode::add_init_callback(&gdsdecomp_init_callback);
#endif
}

void unregister_gdsdecomp_types() {
	//NOP
}
