/*************************************************************************/
/*  gdre_pck_dlg.h                                                       */
/*************************************************************************/

#ifndef GODOT_RE_PCK_DLG_H
#define GODOT_RE_PCK_DLG_H

#include "core/io/resource.h"
#include "core/templates/rb_map.h"

#include "scene/gui/check_box.h"
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

class PackDialog : public AcceptDialog {
	GDCLASS(PackDialog, AcceptDialog)

	FileDialog *target_folder_selection;

	Label *vernfo;
	Label *gennfo;

	Tree *file_list;
	TreeItem *root;

	Label *script_key_error;
	Label *opt_label;

	LineEdit *target_dir;
	Button *select_dir;

	CheckBox *extract_only;
	CheckBox *full_recovery;

	bool is_full_recovery = true;
	bool have_malformed_names;
	bool updating;
	void _update_subitems(TreeItem *p_item, bool p_check, bool p_first = false);
	void _item_edited();
	void _extract_only_pressed();
	void _full_recovery_pressed();

	void _dir_select_pressed();
	void _dir_select_request(const String &p_path);

	void _validate_selection();

	size_t _selected(TreeItem *p_item);
	void _get_selected_files(Vector<String> &p_list, TreeItem *p_item) const;

protected:
	void _notification(int p_notification);
	static void _bind_methods();

public:
	void clear();
	void add_file(const String &p_name, uint64_t p_size, Ref<Texture2D> p_icon, String p_error, bool p_malformed_name, bool p_enc);
	void add_file_to_item(TreeItem *p_item, const String &p_fullname, const String &p_name, uint64_t p_size, Ref<Texture2D> p_icon, String p_error, bool p_enc);
	void set_version(const String &p_version);
	void set_info(const String &p_info);

	Vector<String> get_selected_files() const;
	String get_target_dir() const;
	bool get_is_full_recovery() const;
	PackDialog();
	~PackDialog();
};

#endif
