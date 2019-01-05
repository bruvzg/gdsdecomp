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
#include "gdre_pck_dlg.h"

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
	GDCLASS(GodotREEditor, Object)

private:
	struct PackedFile {

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

	Map<String, Ref<ImageTexture> > gui_icons;

	OverwriteDialog *ovd;

	ScriptDecompDialog *script_dialog_d;
	ScriptCompDialog *script_dialog_c;

	PackDialog *pck_dialog;
	EditorFileDialog *pck_file_selection;
	String pck_file;
	Map<String, PackedFile> pck_files;

	EditorFileDialog *bin_res_file_selection;
	EditorFileDialog *txt_res_file_selection;

	MenuButton *menu_button;
	PopupMenu *menu_popup;

	AcceptDialog *about_dialog;
	CheckBox *about_dialog_checkbox;

	void _show_about_dialog();
	void _toggle_about_dialog_on_start(bool p_enabled);
	void _menu_option_pressed(int p_id);

	void _decompile_files();
	void _decompile_process();

	void _compile_files();
	void _compile_process();

	void _pck_select_request(const String &p_path);
	void _pck_extract_files();
	void _pck_extract_files_process();

	static GodotREEditor *singleton;

protected:
	void _notification(int p_notification);
	static void _bind_methods();

public:
	enum MenuOptions {
		MENU_ONE_CLICK_UNEXPORT,
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
