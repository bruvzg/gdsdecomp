/*************************************************************************/
/*  gdre_dec_dlg.h                                                       */
/*************************************************************************/

#ifndef GODOT_RE_DEC_DLG_H
#define GODOT_RE_DEC_DLG_H

#include "core/map.h"
#include "core/resource.h"
#include "editor/editor_export.h"
#include "editor/editor_node.h"

class ScriptDecompDialog : public AcceptDialog {
	GDCLASS(ScriptDecompDialog, AcceptDialog)

	FileDialog *target_folder_selection;
	FileDialog *file_selection;

	ItemList *file_list;

	Button *add_file;
	Button *remove_file;

	OptionButton *scrver;

	LineEdit *script_key;
	Label *script_key_error;

	LineEdit *target_dir;
	Button *select_dir;

	void _validate_input();
	void _add_files_pressed();
	void _add_files_request(const PoolVector<String> &p_files);
	void _remove_file_pressed();
	void _script_encryption_key_changed(const String &p_key);
	void _dir_select_pressed();
	void _dir_select_request(const String &p_path);

protected:
	void _notification(int p_notification);
	static void _bind_methods();

public:
	Vector<String> get_file_list() const;
	String get_target_dir() const;
	Vector<uint8_t> get_key() const;
	int get_bytecode_version() const;

	ScriptDecompDialog();
	~ScriptDecompDialog();
};

#endif
