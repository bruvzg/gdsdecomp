/*************************************************************************/
/*  gdre_editor.h                                                        */
/*************************************************************************/

#ifndef GODOT_RE_EDITOR_H
#define GODOT_RE_EDITOR_H
#include "utility/packed_file_info.h"

#include "core/io/resource.h"
#include "core/templates/rb_map.h"

#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/check_box.h"
#include "scene/gui/control.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/file_dialog.h"
#include "scene/gui/item_list.h"
#include "scene/gui/label.h"
#include "scene/gui/menu_button.h"
#include "scene/gui/popup.h"
#include "scene/gui/progress_bar.h"
#include "scene/gui/text_edit.h"
#include "scene/gui/texture_rect.h"

#ifdef TOOLS_ENABLED
class EditorNode;
#include "editor/editor_scale.h"
#include "editor/progress_dialog.h"
#else
#define EDSCALE 1.0
#endif
#include "gdre_cmp_dlg.h"
#include "gdre_dec_dlg.h"
#include "gdre_enc_key.h"
#include "gdre_npck_dlg.h"
#include "gdre_pck_dlg.h"
#include "gdre_progress.h"

class GDRESettings;
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

class GodotREEditor : public Node {
	GDCLASS(GodotREEditor, Node)

private:
#ifdef TOOLS_ENABLED
	EditorNode *editor = nullptr;
#else
	Node *editor;
#endif
	GDRESettings *gdres_singleton;
	ProgressDialog *pdialog_singleton;

	Control *ne_parent;
	Dictionary icons;

	OverwriteDialog *ovd;
	ResultDialog *rdl;

	ScriptDecompDialog *script_dialog_d;
	ScriptCompDialog *script_dialog_c;

	EncKeyDialog *key_dialog;
	PackDialog *pck_dialog;
	FileDialog *pck_file_selection;
	String pck_file;
	uint32_t pck_ver_major;
	uint32_t pck_ver_minor;
	uint32_t pck_ver_rev;
	RBMap<String, Ref<PackedFileInfo>> pck_files;
	Vector<Ref<PackedFileInfo>> pck_save_files;

	NewPackDialog *pck_save_dialog;
	FileDialog *pck_source_folder;

	FileDialog *pck_save_file_selection;

	FileDialog *bin_res_file_selection;
	FileDialog *txt_res_file_selection;

	FileDialog *stex_file_selection;
	FileDialog *ostr_file_selection;
	FileDialog *smpl_file_selection;

	MenuButton *menu_button;
	PopupMenu *menu_popup;

	AcceptDialog *about_dialog;
	CheckBox *about_dialog_checkbox;

	void _toggle_about_dialog_on_start(bool p_enabled);

	void _decompile_files();
	void _decompile_process();

	void _compile_files();
	void _compile_process();

	void _pck_select_request(const String &p_path);
	void _pck_unload();
	void _pck_extract_files();
	void _pck_extract_files_process();

	void _pck_create_request(const String &p_path);
	void _pck_save_prep();
	uint64_t _pck_create_process_folder(EditorProgressGDDC *p_pr, const String &p_path, const String &p_rel, uint64_t p_offset, bool &p_cancel);
	void _pck_save_request(const String &p_path);

	Vector<String> res_files;

	void _res_bin_2_txt_request(const Vector<String> &p_files);
	void _res_bin_2_txt_process();
	void _res_txt_2_bin_request(const Vector<String> &p_files);
	void _res_txt_2_bin_process();

	void _res_stex_2_png_request(const Vector<String> &p_files);
	void _res_stxt_2_png_process();

	void _res_ostr_2_ogg_request(const Vector<String> &p_files);
	void _res_ostr_2_ogg_process();

	void _res_smpl_2_wav_request(const Vector<String> &p_files);
	void _res_smpl_2_wav_process();

	Error convert_file_to_binary(const String &p_src_path, const String &p_dst_path);
	Error convert_file_to_text(const String &p_src_path, const String &p_dst_path);

	void print_warning(const String &p_text, const String &p_title, const String &p_sub_text = "");
	void show_warning(const String &p_text, const String &p_title = "Warning!", const String &p_sub_text = "");
	void show_report(const String &p_text, const String &p_title = "Report:", const String &p_sub_text = "");
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
		MENU_STEX_TO_PNG,
		MENU_OSTR_TO_OGG,
		MENU_SMPL_TO_WAV,
		MENU_ABOUT_RE,
		MENU_REPORT_ISSUE,
		MENU_EXIT_RE,
		MENU_KEY
	};

	_FORCE_INLINE_ static GodotREEditor *get_singleton() { return singleton; }

	void init_gui(Control *p_control, HBoxContainer *p_menu, bool p_long_menu);

	void show_about_dialog();
	void menu_option_pressed(int p_id);
	void print_log(const String &p_text);
	GodotREEditor(Control *p_control, HBoxContainer *p_menu);
#ifdef TOOLS_ENABLED
	GodotREEditor(EditorNode *p_editor);
#endif
	~GodotREEditor();
};

/*************************************************************************/

class GodotREEditorStandalone : public Control {
	GDCLASS(GodotREEditorStandalone, Control)

	GodotREEditor *editor_ctx;
	HBoxContainer *menu_hb;

protected:
	void _notification(int p_notification);
	static void _bind_methods();

public:
	void _write_log_message(String p_message);
	String get_version();

	GodotREEditorStandalone();
	~GodotREEditorStandalone();
};

#endif // GODOT_RE_EDITOR_H
