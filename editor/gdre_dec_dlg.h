/*************************************************************************/
/*  gdre_dec_dlg.h                                                       */
/*************************************************************************/

#ifndef GODOT_RE_DEC_DLG_H
#define GODOT_RE_DEC_DLG_H

#include "core/io/resource.h"
#include "core/templates/rb_map.h"

#include "scene/gui/control.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/file_dialog.h"
#include "scene/gui/item_list.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/text_edit.h"

#ifdef TOOLS_ENABLED
#include "editor/editor_scale.h"
#else
#define EDSCALE 1.0
#endif

class ScriptDecompDialog : public AcceptDialog {
	GDCLASS(ScriptDecompDialog, AcceptDialog)

	FileDialog *target_folder_selection;
	FileDialog *file_selection;

	ItemList *file_list;

	Button *add_file;
	Button *remove_file;
	Button *clear_files;

	OptionButton *scrver;

	Label *script_key_error;

	LineEdit *target_dir;
	Button *select_dir;

	void _validate_input();
	void _add_files_pressed();
	void _add_files_request(const Vector<String> &p_files);
	void _remove_file_pressed();
	void _clear_pressed();
	void _dir_select_pressed();
	void _dir_select_request(const String &p_path);
	void _bytcode_changed(int p_id);

protected:
	void _notification(int p_notification);
	static void _bind_methods();

public:
	Vector<String> get_file_list() const;
	String get_target_dir() const;
	int get_bytecode_version() const;

	ScriptDecompDialog();
	~ScriptDecompDialog();
};

#endif
