/*************************************************************************/
/*  gdre_pck_dlg.h                                                       */
/*************************************************************************/

#ifndef GODOT_RE_PCK_DLG_H
#define GODOT_RE_PCK_DLG_H

#include "core/map.h"
#include "core/resource.h"
#include "editor/editor_export.h"
#include "editor/editor_node.h"

class PackDialog : public AcceptDialog {
	GDCLASS(PackDialog, AcceptDialog)

	FileDialog *target_folder_selection;

	Label *vernfo;
	Label *gennfo;

	Tree *file_list;
	TreeItem *root;

	Label *script_key_error;

	LineEdit *target_dir;
	Button *select_dir;

	void _dir_select_pressed();
	void _dir_select_request(const String &p_path);

	void _select_all_toggle(int p_col);
	void _validate_selection();

protected:
	void _notification(int p_notification);
	static void _bind_methods();

public:
	void clear();
	void add_file(const String &p_name, int64_t p_size, Ref<Texture> p_icon);
	void set_version(const String &p_version);
	void set_info(const String &p_info);

	Vector<String> get_selected_files() const;
	String get_target_dir() const;

	PackDialog();
	~PackDialog();
};

#endif
