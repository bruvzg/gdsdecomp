/*************************************************************************/
/*  gdre_editor.h                                                        */
/*************************************************************************/

#ifndef GODOT_RE_EDITOR_H
#define GODOT_RE_EDITOR_H

#include "core/map.h"
#include "core/resource.h"
#include "editor/editor_export.h"
#include "editor/editor_node.h"

#include "gdre_cmp_dlg.h"
#include "gdre_dec_dlg.h"
#include "gdre_npck_dlg.h"
#include "gdre_pck_dlg.h"

class ResultDialog : public AcceptDialog {
	GDCLASS(ResultDialog, AcceptDialog)

	Label *lbl;
	TextEdit *message;

protected:
	void _notification(int p_notification);
	static void _bind_methods();

public:
	void set_message(const String &p_text, const String &p_title);

	ResultDialog();
	~ResultDialog();
};

class OverwriteDialog : public AcceptDialog {
	GDCLASS(OverwriteDialog, AcceptDialog)

	Label *lbl;
	TextEdit *message;

protected:
	void _notification(int p_notification);
	static void _bind_methods();

public:
	void set_message(const String &p_text);

	OverwriteDialog();
	~OverwriteDialog();
};

struct EditorProgressGDDC {

	String task;
	ProgressDialog *progress_dialog;
	bool step(const String &p_state, int p_step = -1, bool p_force_refresh = true) {

		if (EditorNode::get_singleton()) {
			return EditorNode::progress_task_step(task, p_state, p_step, p_force_refresh);
		} else {
			return (progress_dialog) ? progress_dialog->task_step(task, p_state, p_step, p_force_refresh) : false;
		}
	}

	EditorProgressGDDC(Control *p_parent, const String &p_task, const String &p_label, int p_amount, bool p_can_cancel = false) {

		if (EditorNode::get_singleton()) {
			progress_dialog = NULL;
			EditorNode::progress_add_task(p_task, p_label, p_amount, p_can_cancel);
		} else {
			if (!ProgressDialog::get_singleton()) {
				progress_dialog = memnew(ProgressDialog);
				if (p_parent)
					p_parent->add_child(progress_dialog);
			} else {
				progress_dialog = ProgressDialog::get_singleton();
			}
			if (progress_dialog)
				progress_dialog->add_task(p_task, p_label, p_amount, p_can_cancel);
		}
		task = p_task;
	}
	~EditorProgressGDDC() {

		if (EditorNode::get_singleton()) {
			EditorNode::progress_end_task(task);
		} else {
			if (progress_dialog)
				progress_dialog->end_task(task);
		}
	}
};

class GodotREEditor : public Node {
	GDCLASS(GodotREEditor, Node)

private:
	struct PackedFile {

		String name;
		uint64_t offset;
		uint64_t size;
		uint8_t md5[16];

		PackedFile() {
			offset = 0;
			size = 0;
		}

		PackedFile(uint64_t p_offset, uint64_t p_size) {
			offset = p_offset;
			size = p_size;
		}
	};

	EditorNode *editor;
	Control *ne_parent;

	Map<String, Ref<ImageTexture> > gui_icons;

	OverwriteDialog *ovd;
	ResultDialog *rdl;

	ScriptDecompDialog *script_dialog_d;
	ScriptCompDialog *script_dialog_c;

	PackDialog *pck_dialog;
	FileDialog *pck_file_selection;
	String pck_file;
	Map<String, PackedFile> pck_files;
	Vector<PackedFile> pck_save_files;

	NewPackDialog *pck_save_dialog;
	FileDialog *pck_source_folder;

	FileDialog *pck_save_file_selection;

	FileDialog *bin_res_file_selection;
	FileDialog *txt_res_file_selection;

	MenuButton *menu_button;
	PopupMenu *menu_popup;

	AcceptDialog *about_dialog;
	CheckBox *about_dialog_checkbox;

	Ref<ImageTexture> get_gui_icon(const String &p_name);

	void _toggle_about_dialog_on_start(bool p_enabled);

	void _decompile_files();
	void _decompile_process();

	void _compile_files();
	void _compile_process();

	void _pck_select_request(const String &p_path);
	void _pck_extract_files();
	void _pck_extract_files_process();

	void _pck_create_request(const String &p_path);
	void _pck_save_prep();
	uint64_t _pck_create_process_folder(EditorProgressGDDC *p_pr, const String &p_path, const String &p_rel, uint64_t p_offset, bool &p_cancel);
	void _pck_save_request(const String &p_path);

	PoolVector<String> res_files;

	void _res_bin_2_txt_request(const PoolVector<String> &p_files);
	void _res_bin_2_txt_process();
	void _res_txt_2_bin_request(const PoolVector<String> &p_files);
	void _res_txt_2_bin_process();

	Error convert_file_to_binary(const String &p_src_path, const String &p_dst_path);
	Error convert_file_to_text(const String &p_src_path, const String &p_dst_path);

	void show_warning(const String &p_text, const String &p_title = "Warning!");

	static GodotREEditor *singleton;

protected:
	void _notification(int p_notification);
	static void _bind_methods();

public:
	enum MenuOptions {
		MENU_ONE_CLICK_UNEXPORT,
		MENU_CREATE_PCK,
		MENU_EXT_PCK,
		MENU_DECOMP_GDS,
		MENU_COMP_GDS,
		MENU_CONV_TO_TXT,
		MENU_CONV_TO_BIN,
		MENU_ABOUT_RE,
		MENU_EXIT_RE
	};

	_FORCE_INLINE_ static GodotREEditor *get_singleton() { return singleton; }

	void init_gui(Control *p_control, HBoxContainer *p_menu, bool p_long_menu);

	void show_about_dialog();
	void menu_option_pressed(int p_id);

	GodotREEditor(Control *p_control, HBoxContainer *p_menu);
	GodotREEditor(EditorNode *p_editor);
	~GodotREEditor();
};

class GodotREEditorSt : public Control {
	GDCLASS(GodotREEditorSt, Control)

	GodotREEditor *editor_ctx;
	HBoxContainer *menu_hb;

protected:
	void _notification(int p_notification);
	static void _bind_methods();

public:
	GodotREEditorSt();
	~GodotREEditorSt();
};

#endif // GODOT_RE_EDITOR_H
