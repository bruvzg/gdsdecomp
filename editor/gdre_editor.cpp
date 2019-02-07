/*************************************************************************/
/*  gdre_editor.cpp                                                      */
/*************************************************************************/

#include "gdre_editor.h"

#include "modules/gdscript/gdscript.h"
#include "modules/gdscript/gdscript_functions.h"
#include "modules/gdscript/gdscript_tokenizer.h"

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

#include "core/io/file_access_encrypted.h"
#include "core/io/resource_format_binary.h"
#include "icons/icons.gen.h"
#include "modules/svg/image_loader_svg.h"
#include "scene/resources/scene_format_text.h"

#include "thirdparty/misc/md5.h"
#include "thirdparty/misc/sha256.h"

GodotREEditor *GodotREEditor::singleton = NULL;
/*************************************************************************/

ResultDialog::ResultDialog() {

	set_title(TTR("OK"));
	set_resizable(false);

	VBoxContainer *script_vb = memnew(VBoxContainer);

	lbl = memnew(Label);
	script_vb->add_child(lbl);

	message = memnew(TextEdit);
	message->set_readonly(true);
	message->set_custom_minimum_size(Size2(1000, 300) * EDSCALE);
	script_vb->add_child(message);

	add_child(script_vb);
};

ResultDialog::~ResultDialog() {
	//NOP
}

void ResultDialog::_notification(int p_notification) {
	//NOP
}

void ResultDialog::_bind_methods() {
	//NOP
}

void ResultDialog::set_message(const String &p_text, const String &p_title) {

	lbl->set_text(p_title);
	message->set_text(p_text);
}

/*************************************************************************/

OverwriteDialog::OverwriteDialog() {

	set_title(TTR("Files already exist!"));
	set_resizable(false);

	VBoxContainer *script_vb = memnew(VBoxContainer);

	lbl = memnew(Label);
	lbl->set_text(TTR("Following files already exist and will be overwritten:"));
	script_vb->add_child(lbl);

	message = memnew(TextEdit);
	message->set_readonly(true);
	message->set_custom_minimum_size(Size2(1000, 300) * EDSCALE);
	script_vb->add_child(message);

	add_child(script_vb);

	get_ok()->set_text(TTR("Overwrite"));
	add_cancel(TTR("Cancel"));
};

OverwriteDialog::~OverwriteDialog() {
	//NOP
}

void OverwriteDialog::_notification(int p_notification) {
	//NOP
}

void OverwriteDialog::_bind_methods() {
	//NOP
}

void OverwriteDialog::set_message(const String &p_text) {

	message->set_text(p_text);
}

/*************************************************************************/

static int _get_pad(int p_alignment, int p_n) {

	int rest = p_n % p_alignment;
	int pad = 0;
	if (rest > 0) {
		pad = p_alignment - rest;
	};

	return pad;
}

GodotREEditor::GodotREEditor(EditorNode *p_editor) {

	singleton = this;
	editor = p_editor;
	ne_parent = NULL;

	init_gui(editor->get_gui_base(), editor->get_menu_hb(), false);
}

GodotREEditor::GodotREEditor(Control *p_control, HBoxContainer *p_menu) {

	editor = NULL;
	ne_parent = p_control;

	init_gui(p_control, p_menu, true);
}

void GodotREEditor::init_gui(Control *p_control, HBoxContainer *p_menu, bool p_long_menu) {

	//Init editor icons

	for (int i = 0; i < gdre_icons_count; i++) {
		Ref<ImageTexture> icon = memnew(ImageTexture);
		Ref<Image> img = memnew(Image);

		ImageLoaderSVG::create_image_from_string(img, gdre_icons_sources[i], EDSCALE, true, false);
		icon->create_from_image(img);
		gui_icons[gdre_icons_names[i]] = icon;
	}

	//Init dialogs

	ovd = memnew(OverwriteDialog);
	p_control->add_child(ovd);

	rdl = memnew(ResultDialog);
	p_control->add_child(rdl);

	script_dialog_d = memnew(ScriptDecompDialog);
	script_dialog_d->connect("confirmed", this, "_decompile_files");
	p_control->add_child(script_dialog_d);

	script_dialog_c = memnew(ScriptCompDialog);
	script_dialog_c->connect("confirmed", this, "_compile_files");
	p_control->add_child(script_dialog_c);

	pck_dialog = memnew(PackDialog);
	pck_dialog->connect("confirmed", this, "_pck_extract_files");
	p_control->add_child(pck_dialog);

	pck_source_folder = memnew(FileDialog);
	pck_source_folder->set_access(FileDialog::ACCESS_FILESYSTEM);
	pck_source_folder->set_mode(FileDialog::MODE_OPEN_DIR);
	pck_source_folder->connect("dir_selected", this, "_pck_create_request");
	p_control->add_child(pck_source_folder);

	pck_save_dialog = memnew(NewPackDialog);
	pck_save_dialog->connect("confirmed", this, "_pck_save_prep");
	p_control->add_child(pck_save_dialog);

	pck_save_file_selection = memnew(FileDialog);
	pck_save_file_selection->set_access(FileDialog::ACCESS_FILESYSTEM);
	pck_save_file_selection->set_mode(FileDialog::MODE_SAVE_FILE);
	pck_save_file_selection->add_filter("*.pck;PCK archive files");
	pck_save_file_selection->connect("file_selected", this, "_pck_save_request");
	p_control->add_child(pck_save_file_selection);

	pck_file_selection = memnew(FileDialog);
	pck_file_selection->set_access(FileDialog::ACCESS_FILESYSTEM);
	pck_file_selection->set_mode(FileDialog::MODE_OPEN_FILE);
	pck_file_selection->add_filter("*.pck;PCK archive files");
	pck_file_selection->add_filter("*.exe,*.bin,*.32,*.64;Self contained executable files");
	pck_file_selection->connect("file_selected", this, "_pck_select_request");
	p_control->add_child(pck_file_selection);

	bin_res_file_selection = memnew(FileDialog);
	bin_res_file_selection->set_access(FileDialog::ACCESS_FILESYSTEM);
	bin_res_file_selection->set_mode(FileDialog::MODE_OPEN_FILES);
	bin_res_file_selection->add_filter("*.scn,*.res;Binary resource files");
	bin_res_file_selection->connect("files_selected", this, "_res_bin_2_txt_request");
	p_control->add_child(bin_res_file_selection);

	txt_res_file_selection = memnew(FileDialog);
	txt_res_file_selection->set_access(FileDialog::ACCESS_FILESYSTEM);
	txt_res_file_selection->set_mode(FileDialog::MODE_OPEN_FILES);
	txt_res_file_selection->add_filter("*.tscn,*.tres;Text resource files");
	txt_res_file_selection->connect("files_selected", this, "_res_txt_2_bin_request");
	p_control->add_child(txt_res_file_selection);

	//Init about/warning dialog
	{
		about_dialog = memnew(AcceptDialog);
		p_control->add_child(about_dialog);
		about_dialog->set_title(TTR("Important: Legal Notice"));

		VBoxContainer *about_vbc = memnew(VBoxContainer);
		about_dialog->add_child(about_vbc);

		HBoxContainer *about_hbc = memnew(HBoxContainer);
		about_vbc->add_child(about_hbc);

		TextureRect *about_icon = memnew(TextureRect);
		about_hbc->add_child(about_icon);
		about_icon->set_texture(gui_icons["LogoBig"]);

		Label *about_label = memnew(Label);
		about_hbc->add_child(about_label);
		about_label->set_custom_minimum_size(Size2(600, 150) * EDSCALE);
		about_label->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		about_label->set_autowrap(true);
		String about_text =
				String("Godot RE Tools, v0.0.2-poc \n\n") +
				TTR(String("Resources, binary code and source code might be protected by copyright and trademark\n") +
						"laws. Before using this software make sure that decompilation is not prohibited by the\n" +
						"applicable license agreement, permitted under applicable law or you obtained explicit\n" +
						"permission from the copyright owner.\n\n" +
						"The authors and copyright holders of this software do neither encourage nor condone\n" +
						"the use of this software, and disclaim any liability for use of the software in violation of\n" +
						"applicable laws.\n\n" +
						"This software in a pre-alpha stage and is not suitable for use in production.\n\n");
		about_label->set_text(about_text);

		if (EditorSettings::get_singleton()) {
			EDITOR_DEF("re/editor/show_info_on_start", true);
		}

		about_dialog_checkbox = memnew(CheckBox);
		about_vbc->add_child(about_dialog_checkbox);
		about_dialog_checkbox->set_text(TTR("Show this warning when starting the editor"));
		about_dialog_checkbox->connect("toggled", this, "_toggle_about_dialog_on_start");
	}

	//Init menu
	if (p_long_menu) {
		menu_button = memnew(MenuButton);
		menu_button->set_text(TTR("RE Tools"));
		menu_button->set_icon(gui_icons["Logo"]);
		menu_popup = menu_button->get_popup();
		menu_popup->add_icon_item(gui_icons["Logo"], TTR("About Godot RE Tools"), MENU_ABOUT_RE);
		menu_popup->add_icon_item(gui_icons["Logo"], TTR("Quit"), MENU_EXIT_RE);
		menu_popup->connect("id_pressed", this, "menu_option_pressed");
		p_menu->add_child(menu_button);

		menu_button = memnew(MenuButton);
		menu_button->set_text(TTR("PCK"));
		menu_button->set_icon(gui_icons["Pack"]);
		menu_popup = menu_button->get_popup();
		menu_popup->add_icon_item(gui_icons["Pack"], TTR("Create PCK archive from folder..."), MENU_CREATE_PCK);
		menu_popup->add_icon_item(gui_icons["Pack"], TTR("Explore PCK archive..."), MENU_EXT_PCK);
		menu_popup->connect("id_pressed", this, "menu_option_pressed");
		p_menu->add_child(menu_button);

		menu_button = memnew(MenuButton);
		menu_button->set_text(TTR("GDScript"));
		menu_button->set_icon(gui_icons["Script"]);
		menu_popup = menu_button->get_popup();
		menu_popup->add_icon_item(gui_icons["Script"], TTR("Decompile .GDC/.GDE script files..."), MENU_DECOMP_GDS);
		menu_popup->add_icon_item(gui_icons["Script"], TTR("Compile .GD script files..."), MENU_COMP_GDS);
		menu_popup->connect("id_pressed", this, "menu_option_pressed");
		p_menu->add_child(menu_button);

		menu_button = memnew(MenuButton);
		menu_button->set_text(TTR("Resources"));
		menu_button->set_icon(gui_icons["ResBT"]);
		menu_popup = menu_button->get_popup();
		menu_popup->add_icon_item(gui_icons["ResBT"], TTR("Convert binary resources to text..."), MENU_CONV_TO_TXT);
		menu_popup->add_icon_item(gui_icons["ResTB"], TTR("Convert text resources to binary..."), MENU_CONV_TO_BIN);
		menu_popup->connect("id_pressed", this, "menu_option_pressed");
		p_menu->add_child(menu_button);
	} else {
		menu_button = memnew(MenuButton);
		menu_button->set_text(TTR("RE Tools"));
		menu_button->set_icon(gui_icons["Logo"]);
		menu_popup = menu_button->get_popup();
		menu_popup->add_icon_item(gui_icons["Logo"], TTR("About Godot RE Tools"), MENU_ABOUT_RE);
		menu_popup->add_separator();
		menu_popup->add_icon_item(gui_icons["Pack"], TTR("Create PCK archive from folder..."), MENU_CREATE_PCK);
		menu_popup->add_icon_item(gui_icons["Pack"], TTR("Explore PCK archive..."), MENU_EXT_PCK);
		menu_popup->add_separator();
		menu_popup->add_icon_item(gui_icons["Script"], TTR("Decompile .GDC/.GDE script files..."), MENU_DECOMP_GDS);
		menu_popup->add_icon_item(gui_icons["Script"], TTR("Compile .GD script files..."), MENU_COMP_GDS);
		menu_popup->add_separator();
		menu_popup->add_icon_item(gui_icons["ResBT"], TTR("Convert binary resources to text..."), MENU_CONV_TO_TXT); //10
		menu_popup->add_icon_item(gui_icons["ResTB"], TTR("Convert text resources to binary..."), MENU_CONV_TO_BIN); //11
		menu_popup->connect("id_pressed", this, "menu_option_pressed");
		p_menu->add_child(menu_button);
		if (p_menu->get_child_count() >= 2) {
			p_menu->move_child(menu_button, p_menu->get_child_count() - 2);
		}
	}
}

GodotREEditor::~GodotREEditor() {

	singleton = NULL;
}

Ref<ImageTexture> GodotREEditor::get_gui_icon(const String &p_name) {

	return gui_icons[p_name];
}

void GodotREEditor::show_about_dialog() {

	if (EditorSettings::get_singleton()) {
		bool show_on_start = EDITOR_GET("re/editor/show_info_on_start");
		about_dialog_checkbox->set_pressed(show_on_start);
	} else {
		about_dialog_checkbox->set_disabled(true);
	}
	about_dialog->popup_centered_minsize();
}

void GodotREEditor::_toggle_about_dialog_on_start(bool p_enabled) {

	if (EditorSettings::get_singleton()) {
		bool show_on_start = EDITOR_GET("re/editor/show_info_on_start");
		if (show_on_start != p_enabled) {
			EditorSettings::get_singleton()->set_setting("re/editor/show_info_on_start", p_enabled);
		}
	}
}

void GodotREEditor::menu_option_pressed(int p_id) {

	switch (p_id) {
		case MENU_ONE_CLICK_UNEXPORT: {

		} break;
		case MENU_CREATE_PCK: {
			pck_source_folder->popup_centered(Size2(800, 600));
		} break;
		case MENU_EXT_PCK: {
			pck_file_selection->popup_centered(Size2(800, 600));
		} break;
		case MENU_DECOMP_GDS: {
			script_dialog_d->popup_centered(Size2(800, 600));
		} break;
		case MENU_COMP_GDS: {
			script_dialog_c->popup_centered(Size2(800, 600));
		} break;
		case MENU_CONV_TO_TXT: {
			bin_res_file_selection->popup_centered(Size2(800, 600));
		} break;
		case MENU_CONV_TO_BIN: {
			txt_res_file_selection->popup_centered(Size2(800, 600));
		} break;
		case MENU_ABOUT_RE: {
			show_about_dialog();
		} break;
		case MENU_EXIT_RE: {
			get_tree()->quit();
		} break;
		default:
			ERR_FAIL();
	}
}

void GodotREEditor::show_warning(const String &p_text, const String &p_title) {

	rdl->set_title(p_title);
	rdl->set_message(p_text, "");
	rdl->popup_centered();
}

/*************************************************************************/
/* Decompile                                                             */
/*************************************************************************/

void GodotREEditor::_decompile_files() {

	Vector<String> files = script_dialog_d->get_file_list();
	String dir = script_dialog_d->get_target_dir();

	DirAccess *da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	String overwrite_list = String();
	for (int i = 0; i < files.size(); i++) {
		String target_name = dir.plus_file(files[i].get_file().get_basename() + ".gd");
		if (da->file_exists(target_name)) {
			overwrite_list += target_name + "\n";
		}
	}

	if (overwrite_list.length() == 0) {
		_decompile_process();
	} else {
		ovd->set_message(overwrite_list);
		ovd->connect("confirmed", this, "_decompile_process", Vector<Variant>(), CONNECT_ONESHOT);
		ovd->popup_centered();
	}
}

void GodotREEditor::_decompile_process() {

	Vector<String> files = script_dialog_d->get_file_list();
	Vector<uint8_t> key = script_dialog_d->get_key();
	String dir = script_dialog_d->get_target_dir();

	String failed_files;
	GDScriptDecomp *dce = NULL;

	switch (script_dialog_d->get_bytecode_version()) {
		case 3100: {
			dce = memnew(GDScriptDecomp_3_1_0);
		} break;
		case 3060: {
			dce = memnew(GDScriptDecomp_3_0_6);
		} break;
		case 2150: {
			dce = memnew(GDScriptDecomp_2_1_5);
		} break;
		case 2120: {
			dce = memnew(GDScriptDecomp_2_1_2);
		} break;
		case 2110: {
			dce = memnew(GDScriptDecomp_2_1_1);
		} break;
		case 2040: {
			dce = memnew(GDScriptDecomp_2_0_4);
		} break;
		case 1100: {
			dce = memnew(GDScriptDecomp_1_1_0);
		} break;
		case 1000: {
			dce = memnew(GDScriptDecomp_1_0_0);
		} break;
		case 0000: {
			dce = memnew(GDScriptDecomp_0_0_0);
		} break;
		default: {
			rdl->set_message(failed_files, TTR("Invalid bytecode version!"));
			rdl->popup_centered();
			return;
		}
	}
	EditorProgressGDDC *pr = memnew(EditorProgressGDDC(ne_parent, "re_decompile", TTR("Decompiling files..."), files.size(), true));
	for (int i = 0; i < files.size(); i++) {
		String target_name = dir.plus_file(files[i].get_file().get_basename() + ".gd");

		bool cancel = pr->step(files[i].get_file(), i, true);
		if (cancel) {
			break;
		}

		Error err;
		if (files[i].ends_with(".gde")) {
			err = dce->decompile_byte_code_encrypted(files[i], key);
		} else {
			err = dce->decompile_byte_code(files[i]);
		}

		if (err == OK) {
			String scr = dce->get_script_text();
			FileAccess *file = FileAccess::open(target_name, FileAccess::WRITE, &err);
			if (err) {
				failed_files += files[i] + " (FileAccess error)\n";
			}
			file->store_string(scr);
			file->close();
			memdelete(file);
		} else {
			failed_files += files[i] + " (GDSDecomp error)\n";
		}
	}

	memdelete(pr);
	memdelete(dce);

	rdl->set_title(TTR("Decompile"));
	if (failed_files.length() > 0) {
		rdl->set_message(failed_files, TTR("At least one error was detected!"));
		rdl->popup_centered();
	} else {
		rdl->set_message(TTR("No errors detected."), TTR("The operation completed successfully!"));
		rdl->popup_centered();
	}
}

/*************************************************************************/
/* Compile                                                               */
/*************************************************************************/

void GodotREEditor::_compile_files() {

	Vector<String> files = script_dialog_c->get_file_list();
	Vector<uint8_t> key = script_dialog_c->get_key();
	String dir = script_dialog_c->get_target_dir();
	String ext = (key.size() == 32) ? ".gde" : ".gds";

	DirAccess *da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	String overwrite_list = String();
	for (int i = 0; i < files.size(); i++) {
		String target_name = dir.plus_file(files[i].get_file().get_basename() + ext);
		if (da->file_exists(target_name)) {
			overwrite_list += target_name + "\n";
		}
	}

	if (overwrite_list.length() == 0) {
		_compile_process();
	} else {
		ovd->set_message(overwrite_list);
		ovd->connect("confirmed", this, "_compile_process", Vector<Variant>(), CONNECT_ONESHOT);
		ovd->popup_centered();
	}
}

void GodotREEditor::_compile_process() {

	Vector<String> files = script_dialog_c->get_file_list();
	Vector<uint8_t> key = script_dialog_c->get_key();
	String dir = script_dialog_c->get_target_dir();
	String ext = (key.size() == 32) ? ".gde" : ".gdc";

	String failed_files;

	EditorProgressGDDC *pr = memnew(EditorProgressGDDC(ne_parent, "re_compile", TTR("Compiling files..."), files.size(), true));

	for (int i = 0; i < files.size(); i++) {
		String target_name = dir.plus_file(files[i].get_file().get_basename() + ext);

		bool cancel = pr->step(files[i].get_file(), i, true);
		if (cancel) {
			break;
		}

		Vector<uint8_t> file = FileAccess::get_file_as_array(files[i]);
		if (file.size() > 0) {
			String txt;
			txt.parse_utf8((const char *)file.ptr(), file.size());
			file = GDScriptTokenizerBuffer::parse_code_string(txt);

			FileAccess *fa = FileAccess::open(target_name, FileAccess::WRITE);
			if (fa) {
				if (key.size() == 32) {
					FileAccessEncrypted *fae = memnew(FileAccessEncrypted);
					Error err = fae->open_and_parse(fa, key, FileAccessEncrypted::MODE_WRITE_AES256);
					if (err == OK) {
						fae->store_buffer(file.ptr(), file.size());
						fae->close();
					} else {
						failed_files += files[i] + " (FileAccessEncrypted error)\n";
					}
					memdelete(fae);
				} else {
					fa->store_buffer(file.ptr(), file.size());
					fa->close();
					memdelete(fa);
				}
			} else {
				failed_files += files[i] + " (FileAccess error)\n";
			}
		} else {
			failed_files += files[i] + " (Empty file)\n";
		}
	}

	memdelete(pr);

	rdl->set_title(TTR("Compile"));
	if (failed_files.length() > 0) {
		rdl->set_message(failed_files, TTR("At least one error was detected!"));
		rdl->popup_centered();
	} else {
		rdl->set_message(TTR("No errors detected."), TTR("The operation completed successfully!"));
		rdl->popup_centered();
	}
}

/*************************************************************************/
/* PCK explorer                                                          */
/*************************************************************************/

void GodotREEditor::_pck_select_request(const String &p_path) {

	pck_file = String();
	pck_dialog->clear();
	pck_files.clear();

	FileAccess *pck = FileAccess::open(p_path, FileAccess::READ);
	if (!pck) {
		show_warning(TTR("Error opening PCK file: ") + p_path, TTR("OK"));
		return;
	}

	uint32_t magic = pck->get_32();
	bool is_emb = false;

	if (magic != 0x43504447) {
		//maybe at he end.... self contained exe
		pck->seek_end();
		pck->seek(pck->get_position() - 4);
		magic = pck->get_32();
		if (magic != 0x43504447) {
			show_warning(TTR("Invalid PCK file"));
			memdelete(pck);
			return;
		}
		pck->seek(pck->get_position() - 12);

		uint64_t ds = pck->get_64();
		pck->seek(pck->get_position() - ds - 8);

		magic = pck->get_32();
		if (magic != 0x43504447) {
			show_warning(TTR("Invalid PCK file"));
			memdelete(pck);
			return;
		}
		is_emb = true;
	}

	uint32_t version = pck->get_32();

	if (version > 1) {
		show_warning(TTR("Pack version unsupported: ") + itos(version));
		memdelete(pck);
		return;
	}

	uint32_t ver_major = pck->get_32();
	uint32_t ver_minor = pck->get_32();
	uint32_t ver_rev = pck->get_32();

	pck_dialog->set_version(String("    ") + TTR("PCK version: ") + itos(version) + "; " + TTR("created by Godot engine: ") + itos(ver_major) + "." + itos(ver_minor) + TTR(" rev ") + itos(ver_rev) + "; " + ((is_emb) ? TTR("self contained exe") : TTR("standalone")));
	for (int i = 0; i < 16; i++) {
		pck->get_32();
	}

	int file_count = pck->get_32();

	EditorProgressGDDC *pr = memnew(EditorProgressGDDC(ne_parent, "re_read_pck_md5", TTR("Reading PCK archive, click cancel to skip MD5 checking..."), file_count, true));

	bool p_check_md5 = true;
	int files_checked = 0;
	int files_broken = 0;

	for (int i = 0; i < file_count; i++) {

		uint32_t sl = pck->get_32();
		CharString cs;
		cs.resize(sl + 1);
		pck->get_buffer((uint8_t *)cs.ptr(), sl);
		cs[sl] = 0;

		String path;
		path.parse_utf8(cs.ptr());

		uint64_t ofs = pck->get_64();
		uint64_t size = pck->get_64();
		uint8_t md5_saved[16];
		pck->get_buffer(md5_saved, 16);

		bool cancel = pr->step(path, i, true);
		if (cancel) {
			if (p_check_md5) {
				memdelete(pr);
				pr = memnew(EditorProgressGDDC(ne_parent, "re_read_pck_no_md5", TTR("Reading PCK archive, click cancel again to abort..."), file_count, true));
				p_check_md5 = false;
			} else {
				memdelete(pr);
				memdelete(pck);

				pck_dialog->clear();
				pck_files.clear();
				return;
			}
		}

		if (p_check_md5) {
			size_t oldpos = pck->get_position();
			pck->seek(ofs);

			files_checked++;

			MD5_CTX md5;
			MD5Init(&md5);

			int64_t rq_size = size;
			uint8_t buf[32768];

			while (rq_size > 0) {

				int got = pck->get_buffer(buf, MIN(32768, rq_size));
				if (got > 0) {
					MD5Update(&md5, buf, got);
				}
				if (got < 4096)
					break;
				rq_size -= 32768;
			}

			MD5Final(&md5);
			pck->seek(oldpos);

			bool md5_match = true;
			for (int j = 0; j < 16; j++) {
				md5_match &= (md5.digest[j] == md5_saved[j]);
			}
			if (!md5_match) {
				files_broken++;
			}
			pck_dialog->add_file(path, size, (md5_match) ? gui_icons["FileOk"] : gui_icons["FileBroken"]);
		} else {
			pck_dialog->add_file(path, size, gui_icons["File"]);
		}

		pck_files[path] = PackedFile(ofs, size);
	};

	pck_dialog->set_info(String("    ") + TTR("Total files: ") + itos(file_count) + "; " + TTR("Checked: ") + itos(files_checked) + "; " + TTR("Broken: ") + itos(files_broken));

	memdelete(pr);
	memdelete(pck);
	pck_file = p_path;

	pck_dialog->popup_centered_minsize();
}

void GodotREEditor::_pck_extract_files() {

	Vector<String> files = pck_dialog->get_selected_files();
	String dir = pck_dialog->get_target_dir();

	DirAccess *da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	String overwrite_list = String();
	for (int i = 0; i < files.size(); i++) {
		String target_name = dir.plus_file(files[i].get_file());
		if (da->file_exists(target_name)) {
			overwrite_list += target_name + "\n";
		}
	}

	if (overwrite_list.length() == 0) {
		_pck_extract_files_process();
	} else {
		ovd->set_message(overwrite_list);
		ovd->connect("confirmed", this, "_pck_extract_files_process", Vector<Variant>(), CONNECT_ONESHOT);
		ovd->popup_centered();
	}
}

void GodotREEditor::_pck_extract_files_process() {

	FileAccess *pck = FileAccess::open(pck_file, FileAccess::READ);
	if (!pck) {
		show_warning(TTR("Error opening PCK file: ") + pck_file, TTR("OK"));
		return;
	}

	Vector<String> files = pck_dialog->get_selected_files();
	String dir = pck_dialog->get_target_dir();

	String failed_files;

	EditorProgressGDDC *pr = memnew(EditorProgressGDDC(ne_parent, "re_ext_pck", TTR("Extracting files..."), files.size(), true));
	DirAccess *da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);

	for (int i = 0; i < files.size(); i++) {
		String target_name = dir.plus_file(files[i].replace("res://", ""));

		da->make_dir_recursive(target_name.get_base_dir());

		bool cancel = pr->step(files[i], i, true);
		if (cancel) {
			break;
		}

		pck->seek(pck_files[files[i]].offset);

		FileAccess *fa = FileAccess::open(target_name, FileAccess::WRITE);
		if (fa) {
			int64_t rq_size = pck_files[files[i]].size;
			uint8_t buf[16384];

			while (rq_size > 0) {

				int got = pck->get_buffer(buf, MIN(16384, rq_size));
				fa->store_buffer(buf, got);
				rq_size -= 16384;
			}
			memdelete(fa);
		} else {
			failed_files += files[i] + " (FileAccess error)\n";
		}
	}
	memdelete(pr);
	memdelete(pck);

	pck_files.clear();
	pck_file = String();

	rdl->set_title(TTR("Extract files"));
	if (failed_files.length() > 0) {
		rdl->set_message(failed_files, TTR("At least one error was detected!"));
		rdl->popup_centered();
	} else {
		rdl->set_message(TTR("No errors detected."), TTR("The operation completed successfully!"));
		rdl->popup_centered();
	}
}

/*************************************************************************/
/* Res convert                                                           */
/*************************************************************************/

void GodotREEditor::_res_bin_2_txt_request(const PoolVector<String> &p_files) {

	DirAccess *da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	String overwrite_list = String();
	for (int i = 0; i < p_files.size(); i++) {

		String ext = p_files[i].get_extension().to_lower();
		String new_ext = ".txt";
		if (ext == "scn") {
			new_ext = ".tscn";
		} else if (ext == "res") {
			new_ext = ".tres";
		}

		if (da->file_exists(p_files[i].get_basename() + new_ext)) {
			overwrite_list += p_files[i].get_basename() + new_ext + "\n";
		}
	}

	res_files = p_files;

	if (overwrite_list.length() == 0) {
		_res_bin_2_txt_process();
	} else {
		ovd->set_message(overwrite_list);
		ovd->connect("confirmed", this, "_res_bin_2_txt_process", Vector<Variant>(), CONNECT_ONESHOT);
		ovd->popup_centered();
	}
}

void GodotREEditor::_res_bin_2_txt_process() {

	EditorProgressGDDC *pr = memnew(EditorProgressGDDC(ne_parent, "re_b2t_res", TTR("Converting files..."), res_files.size(), true));

	String failed_files;
	for (int i = 0; i < res_files.size(); i++) {

		bool cancel = pr->step(res_files[i], i, true);
		if (cancel) {
			break;
		}

		String ext = res_files[i].get_extension().to_lower();
		String new_ext = ".txt";
		if (ext == "scn") {
			new_ext = ".tscn";
		} else if (ext == "res") {
			new_ext = ".tres";
		}

		Error err = convert_file_to_text(res_files[i], res_files[i].get_basename() + new_ext);
		if (err != OK) {
			failed_files += res_files[i] + " (ResourceFormatLoaderText error)\n";
		}
	}

	memdelete(pr);
	res_files = PoolVector<String>();

	rdl->set_title(TTR("Convert resources"));
	if (failed_files.length() > 0) {
		rdl->set_message(failed_files, TTR("At least one error was detected!"));
		rdl->popup_centered();
	} else {
		rdl->set_message(TTR("No errors detected."), TTR("The operation completed successfully!"));
		rdl->popup_centered();
	}
}

Error GodotREEditor::convert_file_to_text(const String &p_src_path, const String &p_dst_path) {

	Ref<ResourceFormatLoaderBinary> rl = memnew(ResourceFormatLoaderBinary);
	Ref<ResourceInteractiveLoaderBinary> ria = rl->load_interactive(p_src_path);
	Error err = ria->poll();
	while (err == OK) {
		err = ria->poll();
	}
	if (ria->get_resource().is_null()) {
		return ERR_CANT_OPEN;
	}
	return ResourceFormatSaverText::singleton->save(p_dst_path, ria->get_resource());
}

void GodotREEditor::_res_txt_2_bin_request(const PoolVector<String> &p_files) {

	DirAccess *da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	String overwrite_list = String();
	for (int i = 0; i < p_files.size(); i++) {
		String ext = p_files[i].get_extension().to_lower();
		String new_ext = ".bin";
		if (ext == "tscn") {
			new_ext = ".scn";
		} else if (ext == "tres") {
			new_ext = ".res";
		}

		if (da->file_exists(p_files[i].get_basename() + new_ext)) {
			overwrite_list += p_files[i].get_basename() + new_ext + "\n";
		}
	}

	res_files = p_files;

	if (overwrite_list.length() == 0) {
		_res_txt_2_bin_process();
	} else {
		ovd->set_message(overwrite_list);
		ovd->connect("confirmed", this, "_res_txt_2_bin_process", Vector<Variant>(), CONNECT_ONESHOT);
		ovd->popup_centered();
	}
}

void GodotREEditor::_res_txt_2_bin_process() {

	EditorProgressGDDC *pr = memnew(EditorProgressGDDC(ne_parent, "re_t2b_res", TTR("Converting files..."), res_files.size(), true));

	String failed_files;
	for (int i = 0; i < res_files.size(); i++) {

		bool cancel = pr->step(res_files[i], i, true);
		if (cancel) {
			break;
		}

		String ext = res_files[i].get_extension().to_lower();
		String new_ext = ".bin";
		if (ext == "tscn") {
			new_ext = ".scn";
		} else if (ext == "tres") {
			new_ext = ".res";
		}

		Error err = convert_file_to_binary(res_files[i], res_files[i].get_basename() + new_ext);
		if (err != OK) {
			failed_files += res_files[i] + " (ResourceFormatLoaderText error)\n";
		}
	}

	memdelete(pr);
	res_files = PoolVector<String>();

	rdl->set_title(TTR("Convert resources"));
	if (failed_files.length() > 0) {
		rdl->set_message(failed_files, TTR("At least one error was detected!"));
		rdl->popup_centered();
	} else {
		rdl->set_message(TTR("No errors detected."), TTR("The operation completed successfully!"));
		rdl->popup_centered();
	}
}

Error GodotREEditor::convert_file_to_binary(const String &p_src_path, const String &p_dst_path) {

	Ref<ResourceFormatLoaderText> rl = memnew(ResourceFormatLoaderText);
	Ref<ResourceInteractiveLoaderText> ria = rl->load_interactive(p_src_path);
	Error err = ria->poll();
	while (err == OK) {
		err = ria->poll();
	}
	if (ria->get_resource().is_null()) {
		return ERR_CANT_OPEN;
	}
	return ResourceFormatSaverBinary::singleton->save(p_dst_path, ria->get_resource());
}

/*************************************************************************/
/* PCK create                                                            */
/*************************************************************************/

void GodotREEditor::_pck_create_request(const String &p_path) {

	pck_save_files.clear();
	pck_file = p_path;

	EditorProgressGDDC *pr = memnew(EditorProgressGDDC(ne_parent, "re_read_folder", TTR("Reading folder structure..."), 1, true));
	bool cancel = false;
	uint64_t size = _pck_create_process_folder(pr, p_path, String(), 0, cancel);
	memdelete(pr);

	if (cancel) {
		return;
	}

	if (size == 0) {
		show_warning(TTR("Error opening folder (or empty folder): ") + p_path, TTR("OK"));
		return;
	}

	String file_list;
	for (int i = 0; i < pck_save_files.size(); i++) {
		file_list += pck_save_files[i].name + " (" + itos(pck_save_files[i].size) + " bytes)\n";
	}
	pck_save_dialog->set_message(file_list);

	pck_save_dialog->popup_centered(Size2(800, 600));
}

void GodotREEditor::_pck_save_prep() {

	pck_save_file_selection->popup_centered(Size2(800, 600));
}

#define PCK_PADDING 16

uint64_t GodotREEditor::_pck_create_process_folder(EditorProgressGDDC *p_pr, const String &p_path, const String &p_rel, uint64_t p_offset, bool &p_cancel) {

	uint64_t offset = p_offset;

	DirAccess *da = DirAccess::open(p_path.plus_file(p_rel));
	if (!da) {
		show_warning(TTR("Error opening folder: ") + p_path.plus_file(p_rel), TTR("OK"));
		return offset;
	}
	da->list_dir_begin();
	String f;
	while ((f = da->get_next()) != "") {
		if (f == "." || f == "..")
			continue;
		if (p_pr->step(p_rel.plus_file(f), 0, true)) {
			p_cancel = true;
			return offset;
		}
		if (da->current_is_dir()) {
			offset = _pck_create_process_folder(p_pr, p_path, p_rel.plus_file(f), offset, p_cancel);
			if (p_cancel) {
				return offset;
			}

		} else {
			FileAccess *file = FileAccess::open(p_path.plus_file(p_rel).plus_file(f), FileAccess::READ);

			MD5_CTX md5;
			MD5Init(&md5);

			int64_t rq_size = file->get_len();
			uint8_t buf[32768];

			while (rq_size > 0) {

				int got = file->get_buffer(buf, MIN(32768, rq_size));
				if (got > 0) {
					MD5Update(&md5, buf, got);
				}
				if (got < 4096)
					break;
				rq_size -= 32768;
			}

			MD5Final(&md5);
			PackedFile finfo = PackedFile(offset, file->get_len());
			finfo.name = p_rel.plus_file(f);
			for (int j = 0; j < 16; j++) {
				finfo.md5[j] = md5.digest[j];
			}
			pck_save_files.push_back(finfo);

			offset += file->get_len();

			memdelete(file);
		}
	}
	memdelete(da);

	return offset;
}

void GodotREEditor::_pck_save_request(const String &p_path) {

	FileAccess *f = FileAccess::open(p_path, FileAccess::WRITE);
	if (!f) {
		show_warning(TTR("Error opening PCK file: ") + p_path, TTR("OK"));
		return;
	}

	EditorProgressGDDC *pr = memnew(EditorProgressGDDC(ne_parent, "re_write_pck", TTR("Writing PCK archive..."), pck_save_files.size() + 2, true));

	f->store_32(0x43504447); //GDPK
	f->store_32(pck_save_dialog->get_version_pack());
	f->store_32(pck_save_dialog->get_version_major());
	f->store_32(pck_save_dialog->get_version_minor());
	f->store_32(pck_save_dialog->get_version_rev());
	for (int i = 0; i < 16; i++) {
		//reserved
		f->store_32(0);
	}

	pr->step("Header...", 0, true);

	f->store_32(pck_save_files.size()); //amount of files

	size_t header_size = f->get_position();

	for (int i = 0; i < pck_save_files.size(); i++) {
		header_size += 4; // size of path string (32 bits is enough)
		uint32_t string_len = pck_save_files[i].name.utf8().length() + 6;
		header_size += string_len + _get_pad(4, string_len); ///size of path string
		header_size += 8; // offset to file _with_ header size included
		header_size += 8; // size of file
		header_size += 16; // md5
	}

	size_t header_padding = _get_pad(PCK_PADDING, header_size);

	pr->step("Directory...", 0, true);

	for (int i = 0; i < pck_save_files.size(); i++) {

		uint32_t string_len = pck_save_files[i].name.utf8().length() + 6;
		uint32_t pad = _get_pad(4, string_len);

		f->store_32(string_len + pad);
		String name = "res://" + pck_save_files[i].name;
		f->store_buffer((const uint8_t *)name.utf8().get_data(), string_len);
		for (uint32_t j = 0; j < pad; j++) {
			f->store_8(0);
		}

		f->store_64(pck_save_files[i].offset + header_padding + header_size);
		f->store_64(pck_save_files[i].size); // pay attention here, this is where file is
		f->store_buffer((const uint8_t *)&pck_save_files[i].md5, 16); //also save md5 for file
	}

	for (uint32_t j = 0; j < header_padding; j++) {
		f->store_8(0);
	}

	String failed_files;

	for (int i = 0; i < pck_save_files.size(); i++) {
		if (pr->step(pck_save_files[i].name, i + 2, true)) {
			break;
		}

		FileAccess *fa = FileAccess::open(pck_file.plus_file(pck_save_files[i].name), FileAccess::READ);
		if (fa) {
			int64_t rq_size = pck_save_files[i].size;
			uint8_t buf[16384];

			while (rq_size > 0) {

				int got = fa->get_buffer(buf, MIN(16384, rq_size));
				f->store_buffer(buf, got);
				rq_size -= 16384;
			}
			memdelete(fa);
		} else {
			failed_files += pck_save_files[i].name + " (FileAccess error)\n";
		}
	}

	f->store_32(0);
	f->store_32(0);
	f->store_string(pck_save_dialog->get_watermark());
	f->store_32(0);
	f->store_32(0);

	f->store_32(0x43504447); //GDPK

	pck_save_files.clear();
	pck_file = String();
	memdelete(f);
	memdelete(pr);

	rdl->set_title(TTR("Create PCK archive..."));
	if (failed_files.length() > 0) {
		rdl->set_message(failed_files, TTR("At least one error was detected!"));
		rdl->popup_centered();
	} else {
		rdl->set_message(TTR("No errors detected."), TTR("The operation completed successfully!"));
		rdl->popup_centered();
	}
}

/*************************************************************************/

void GodotREEditor::_notification(int p_notification) {

	switch (p_notification) {

		case NOTIFICATION_READY: {

			if (EditorSettings::get_singleton()) {
				bool show_info_dialog = EDITOR_GET("re/editor/show_info_on_start");
				if (show_info_dialog) {
					about_dialog->set_exclusive(true);
					show_about_dialog();
					about_dialog->set_exclusive(false);
				}
			} else {
				about_dialog->set_exclusive(true);
				show_about_dialog();
				about_dialog->set_exclusive(false);
			}
		}
	}
}

void GodotREEditor::_bind_methods() {

	ClassDB::bind_method(D_METHOD("_decompile_files"), &GodotREEditor::_decompile_files);
	ClassDB::bind_method(D_METHOD("_decompile_process"), &GodotREEditor::_decompile_process);

	ClassDB::bind_method(D_METHOD("_compile_files"), &GodotREEditor::_compile_files);
	ClassDB::bind_method(D_METHOD("_compile_process"), &GodotREEditor::_compile_process);

	ClassDB::bind_method(D_METHOD("_pck_select_request", "path"), &GodotREEditor::_pck_select_request);
	ClassDB::bind_method(D_METHOD("_pck_extract_files"), &GodotREEditor::_pck_extract_files);
	ClassDB::bind_method(D_METHOD("_pck_extract_files_process"), &GodotREEditor::_pck_extract_files_process);

	ClassDB::bind_method(D_METHOD("_pck_create_request", "path"), &GodotREEditor::_pck_create_request);
	ClassDB::bind_method(D_METHOD("_pck_save_prep"), &GodotREEditor::_pck_save_prep);
	ClassDB::bind_method(D_METHOD("_pck_save_request", "path"), &GodotREEditor::_pck_save_request);

	ClassDB::bind_method(D_METHOD("_toggle_about_dialog_on_start"), &GodotREEditor::_toggle_about_dialog_on_start);

	ClassDB::bind_method(D_METHOD("_res_bin_2_txt_request", "files"), &GodotREEditor::_res_bin_2_txt_request);
	ClassDB::bind_method(D_METHOD("_res_bin_2_txt_process"), &GodotREEditor::_res_bin_2_txt_process);

	ClassDB::bind_method(D_METHOD("_res_txt_2_bin_request", "files"), &GodotREEditor::_res_txt_2_bin_request);
	ClassDB::bind_method(D_METHOD("_res_txt_2_bin_process"), &GodotREEditor::_res_txt_2_bin_process);

	ClassDB::bind_method(D_METHOD("show_about_dialog"), &GodotREEditor::show_about_dialog);
	ClassDB::bind_method(D_METHOD("get_gui_icon", "name"), &GodotREEditor::get_gui_icon);

	ClassDB::bind_method(D_METHOD("menu_option_pressed", "id"), &GodotREEditor::menu_option_pressed);
	ClassDB::bind_method(D_METHOD("convert_file_to_binary", "src_path", "dst_path"), &GodotREEditor::convert_file_to_binary);
	ClassDB::bind_method(D_METHOD("convert_file_to_text", "src_path", "dst_path"), &GodotREEditor::convert_file_to_text);
};

/*************************************************************************/

void GodotREEditorSt::_notification(int p_notification) {

	if (p_notification == MainLoop::NOTIFICATION_WM_ABOUT) {
		if (editor_ctx)
			editor_ctx->show_about_dialog();
	}
}

void GodotREEditorSt::_bind_methods() {

	//NOP
}

GodotREEditorSt::GodotREEditorSt() {

	menu_hb = memnew(HBoxContainer);
	add_child(menu_hb);

	editor_ctx = memnew(GodotREEditor(this, menu_hb));
	add_child(editor_ctx);
}

GodotREEditorSt::~GodotREEditorSt() {

	editor_ctx = NULL;
}
