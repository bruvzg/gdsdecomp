/*************************************************************************/
/*  gdre_editor.h                                                        */
/*************************************************************************/

#ifndef GODOT_RE_EDITOR_H
#define GODOT_RE_EDITOR_H

#include "core/resource.h"
#include "core/map.h"
#include "editor/editor_node.h"
#include "editor/editor_export.h"

class GodotREEditor : public Node {
	GDCLASS(GodotREEditor, Object)

	EditorNode *editor;

	MenuButton *menu_button;
	PopupMenu *menu_popup;

	AcceptDialog *about_dialog;
	CheckBox *about_dialog_checkbox;

	void _show_about_dialog();
	void _toggle_about_dialog_on_start(bool p_enabled);

	void _menu_option_pressed(int p_id);

	static GodotREEditor *singleton;

protected:
	void _notification(int p_notification);
	static void _bind_methods();

public:
	enum MenuOptions {
		MENU_CREATE_PCK,
		MENU_TEST_PCK,
		MENU_EXT_PCK,
		MENU_DECOMP_GDS,
		MENU_COMP_GDS,
		MENU_CONV_TO_TXT,
		MENU_CONV_TO_BIN,
		MENU_ABOUT_RE
	};

	_FORCE_INLINE_ static GodotREEditor *get_singleton() { return singleton; }

	GodotREEditor(EditorNode *p_editor);
	~GodotREEditor();
};

#endif // GODOT_RE_EDITOR_H
