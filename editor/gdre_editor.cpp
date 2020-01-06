/*************************************************************************/
/*  gdre_editor.cpp                                                      */
/*************************************************************************/

#include "gdre_version.h"

#include "gdre_editor.h"

#include "modules/gdscript/gdscript.h"
#include "modules/gdscript/gdscript_functions.h"
#include "modules/gdscript/gdscript_tokenizer.h"

#include "bytecode/bytecode_versions.h"

#include "core/io/file_access_encrypted.h"
#include "core/io/resource_format_binary.h"
#include "icons/icons.gen.h"
#include "modules/svg/image_loader_svg.h"
#include "scene/resources/resource_format_text.h"

#include "modules/stb_vorbis/audio_stream_ogg_vorbis.h"
#include "scene/resources/audio_stream_sample.h"

#include "core/version_generated.gen.h"

#if VERSION_MAJOR < 3
#error Unsupported Godot version
#endif

#if ((VERSION_MAJOR == 3) && (VERSION_MINOR == 2)) || (VERSION_MAJOR == 4)
#include "core/crypto/crypto_core.h"
#else
#include "thirdparty/misc/md5.h"
#include "thirdparty/misc/sha256.h"
#endif

/*************************************************************************/

#ifndef TOOLS_ENABLED
#include "core/message_queue.h"
#include "core/os/os.h"
#include "main/main.h"

ProgressDialog *ProgressDialog::singleton = NULL;

void ProgressDialog::_notification(int p_what) {

	switch (p_what) {

		case NOTIFICATION_DRAW: {

			Ref<StyleBox> style = get_stylebox("panel", "PopupMenu");
			draw_style_box(style, Rect2(Point2(), get_size()));

		} break;
	}
}

void ProgressDialog::_popup() {

	Size2 ms = main->get_combined_minimum_size();
	ms.width = MAX(500 * EDSCALE, ms.width);

	Ref<StyleBox> style = get_stylebox("panel", "PopupMenu");
	ms += style->get_minimum_size();
	main->set_margin(MARGIN_LEFT, style->get_margin(MARGIN_LEFT));
	main->set_margin(MARGIN_RIGHT, -style->get_margin(MARGIN_RIGHT));
	main->set_margin(MARGIN_TOP, style->get_margin(MARGIN_TOP));
	main->set_margin(MARGIN_BOTTOM, -style->get_margin(MARGIN_BOTTOM));

	popup_centered(ms);
}

void ProgressDialog::add_task(const String &p_task, const String &p_label, int p_steps, bool p_can_cancel) {

	if (MessageQueue::get_singleton()->is_flushing()) {
		ERR_PRINT("Do not use progress dialog (task) while flushing the message queue or using call_deferred()!");
		return;
	}

	ERR_FAIL_COND(tasks.has(p_task));
	ProgressDialog::Task t;
	t.vb = memnew(VBoxContainer);
	VBoxContainer *vb2 = memnew(VBoxContainer);
	t.vb->add_margin_child(p_label, vb2);
	t.progress = memnew(ProgressBar);
	t.progress->set_max(p_steps);
	t.progress->set_value(p_steps);
	vb2->add_child(t.progress);
	t.state = memnew(Label);
	t.state->set_clip_text(true);
	vb2->add_child(t.state);
	main->add_child(t.vb);

	tasks[p_task] = t;
	if (p_can_cancel) {
		cancel_hb->show();
	} else {
		cancel_hb->hide();
	}
	cancel_hb->raise();
	cancelled = false;
	_popup();
	if (p_can_cancel) {
		cancel->grab_focus();
	}
}

bool ProgressDialog::task_step(const String &p_task, const String &p_state, int p_step, bool p_force_redraw) {

	ERR_FAIL_COND_V(!tasks.has(p_task), cancelled);

	if (!p_force_redraw) {
		uint64_t tus = OS::get_singleton()->get_ticks_usec();
		if (tus - last_progress_tick < 200000) //200ms
			return cancelled;
	}

	Task &t = tasks[p_task];
	if (p_step < 0)
		t.progress->set_value(t.progress->get_value() + 1);
	else
		t.progress->set_value(p_step);

	t.state->set_text(p_state);
	last_progress_tick = OS::get_singleton()->get_ticks_usec();
	if (cancel_hb->is_visible()) {
		OS::get_singleton()->force_process_input();
	}
	Main::iteration(); // this will not work on a lot of platforms, so it's only meant for the editor
	return cancelled;
}

void ProgressDialog::end_task(const String &p_task) {

	ERR_FAIL_COND(!tasks.has(p_task));
	Task &t = tasks[p_task];

	memdelete(t.vb);
	tasks.erase(p_task);

	if (tasks.empty())
		hide();
	else
		_popup();
}

void ProgressDialog::_cancel_pressed() {
	cancelled = true;
}

void ProgressDialog::_bind_methods() {
	ClassDB::bind_method("_cancel_pressed", &ProgressDialog::_cancel_pressed);
}

ProgressDialog::ProgressDialog() {

	main = memnew(VBoxContainer);
	add_child(main);
	main->set_anchors_and_margins_preset(Control::PRESET_WIDE);
	set_exclusive(true);
	last_progress_tick = 0;
	singleton = this;
	cancel_hb = memnew(HBoxContainer);
	main->add_child(cancel_hb);
	cancel_hb->hide();
	cancel = memnew(Button);
	cancel_hb->add_spacer();
	cancel_hb->add_child(cancel);
	cancel->set_text(RTR("Cancel"));
	cancel_hb->add_spacer();
	cancel->connect("pressed", this, "_cancel_pressed");
}
#endif

/*************************************************************************/

GodotREEditor *GodotREEditor::singleton = NULL;

/*************************************************************************/

ResultDialog::ResultDialog() {

	set_title(RTR("OK"));
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

	set_title(RTR("Files already exist!"));
	set_resizable(false);

	VBoxContainer *script_vb = memnew(VBoxContainer);

	lbl = memnew(Label);
	lbl->set_text(RTR("Following files already exist and will be overwritten:"));
	script_vb->add_child(lbl);

	message = memnew(TextEdit);
	message->set_readonly(true);
	message->set_custom_minimum_size(Size2(1000, 300) * EDSCALE);
	script_vb->add_child(message);

	add_child(script_vb);

	get_ok()->set_text(RTR("Overwrite"));
	add_cancel(RTR("Cancel"));
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

#ifdef TOOLS_ENABLED
GodotREEditor::GodotREEditor(EditorNode *p_editor) {

	singleton = this;
	editor = p_editor;
	ne_parent = NULL;

	init_gui(editor->get_gui_base(), editor->get_menu_hb(), false);
}
#endif

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
	pck_source_folder->set_show_hidden_files(true);
	p_control->add_child(pck_source_folder);

	pck_save_dialog = memnew(NewPackDialog);
	pck_save_dialog->connect("confirmed", this, "_pck_save_prep");
	p_control->add_child(pck_save_dialog);

	pck_save_file_selection = memnew(FileDialog);
	pck_save_file_selection->set_access(FileDialog::ACCESS_FILESYSTEM);
	pck_save_file_selection->set_mode(FileDialog::MODE_SAVE_FILE);
	pck_save_file_selection->connect("file_selected", this, "_pck_save_request");
	pck_save_file_selection->set_show_hidden_files(true);
	p_control->add_child(pck_save_file_selection);

	pck_file_selection = memnew(FileDialog);
	pck_file_selection->set_access(FileDialog::ACCESS_FILESYSTEM);
	pck_file_selection->set_mode(FileDialog::MODE_OPEN_FILE);
	pck_file_selection->add_filter("*.pck;PCK archive files");
	pck_file_selection->add_filter("*.exe,*.bin,*.32,*.64;Self contained executable files");
	pck_file_selection->connect("file_selected", this, "_pck_select_request");
	pck_file_selection->set_show_hidden_files(true);
	p_control->add_child(pck_file_selection);

	bin_res_file_selection = memnew(FileDialog);
	bin_res_file_selection->set_access(FileDialog::ACCESS_FILESYSTEM);
	bin_res_file_selection->set_mode(FileDialog::MODE_OPEN_FILES);
	bin_res_file_selection->add_filter("*.scn,*.res;Binary resource files");
	bin_res_file_selection->connect("files_selected", this, "_res_bin_2_txt_request");
	bin_res_file_selection->set_show_hidden_files(true);
	p_control->add_child(bin_res_file_selection);

	txt_res_file_selection = memnew(FileDialog);
	txt_res_file_selection->set_access(FileDialog::ACCESS_FILESYSTEM);
	txt_res_file_selection->set_mode(FileDialog::MODE_OPEN_FILES);
	txt_res_file_selection->add_filter("*.tscn,*.tres;Text resource files");
	txt_res_file_selection->connect("files_selected", this, "_res_txt_2_bin_request");
	txt_res_file_selection->set_show_hidden_files(true);
	p_control->add_child(txt_res_file_selection);

	stex_file_selection = memnew(FileDialog);
	stex_file_selection->set_access(FileDialog::ACCESS_FILESYSTEM);
	stex_file_selection->set_mode(FileDialog::MODE_OPEN_FILES);
	stex_file_selection->add_filter("*.stex;Stream texture files");
	stex_file_selection->connect("files_selected", this, "_res_stex_2_png_request");
	stex_file_selection->set_show_hidden_files(true);
	p_control->add_child(stex_file_selection);

	ostr_file_selection = memnew(FileDialog);
	ostr_file_selection->set_access(FileDialog::ACCESS_FILESYSTEM);
	ostr_file_selection->set_mode(FileDialog::MODE_OPEN_FILES);
	ostr_file_selection->add_filter("*.oggstr;OGG Sample files");
	ostr_file_selection->connect("files_selected", this, "_res_ostr_2_ogg_request");
	ostr_file_selection->set_show_hidden_files(true);
	p_control->add_child(ostr_file_selection);

	smpl_file_selection = memnew(FileDialog);
	smpl_file_selection->set_access(FileDialog::ACCESS_FILESYSTEM);
	smpl_file_selection->set_mode(FileDialog::MODE_OPEN_FILES);
	smpl_file_selection->add_filter("*.sample;WAV Sample files");
	smpl_file_selection->connect("files_selected", this, "_res_smpl_2_wav_request");
	smpl_file_selection->set_show_hidden_files(true);
	p_control->add_child(smpl_file_selection);

	//Init about/warning dialog
	{
		about_dialog = memnew(AcceptDialog);
		p_control->add_child(about_dialog);
		about_dialog->set_title(RTR("Important: Legal Notice"));

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
				String("Godot RE Tools, ") + String(GDRE_VERSION) + String(" \n\n") +
				RTR(String("Resources, binary code and source code might be protected by copyright and trademark\n") +
						"laws. Before using this software make sure that decompilation is not prohibited by the\n" +
						"applicable license agreement, permitted under applicable law or you obtained explicit\n" +
						"permission from the copyright owner.\n\n" +
						"The authors and copyright holders of this software do neither encourage nor condone\n" +
						"the use of this software, and disclaim any liability for use of the software in violation of\n" +
						"applicable laws.\n\n" +
						"This software in a pre-alpha stage and is not suitable for use in production.\n\n");
		about_label->set_text(about_text);

#ifdef TOOLS_ENABLED
		if (EditorSettings::get_singleton()) {
			EDITOR_DEF("re/editor/show_info_on_start", true);
		}
#endif
		about_dialog_checkbox = memnew(CheckBox);
		about_vbc->add_child(about_dialog_checkbox);
		about_dialog_checkbox->set_text(RTR("Show this warning when starting the editor"));
		about_dialog_checkbox->connect("toggled", this, "_toggle_about_dialog_on_start");
	}

	//Init menu
	if (p_long_menu) {
		menu_button = memnew(MenuButton);
		menu_button->set_text(RTR("RE Tools"));
		menu_button->set_icon(gui_icons["Logo"]);
		menu_popup = menu_button->get_popup();
		menu_popup->add_icon_item(gui_icons["Logo"], RTR("About Godot RE Tools"), MENU_ABOUT_RE);
		menu_popup->add_icon_item(gui_icons["Logo"], RTR("Quit"), MENU_EXIT_RE);
		menu_popup->connect("id_pressed", this, "menu_option_pressed");
		p_menu->add_child(menu_button);

		menu_button = memnew(MenuButton);
		menu_button->set_text(RTR("PCK"));
		menu_button->set_icon(gui_icons["Pack"]);
		menu_popup = menu_button->get_popup();
		menu_popup->add_icon_item(gui_icons["Pack"], RTR("Create PCK archive from folder..."), MENU_CREATE_PCK);
		menu_popup->add_icon_item(gui_icons["Pack"], RTR("Explore PCK archive..."), MENU_EXT_PCK);
		menu_popup->connect("id_pressed", this, "menu_option_pressed");
		p_menu->add_child(menu_button);

		menu_button = memnew(MenuButton);
		menu_button->set_text(RTR("GDScript"));
		menu_button->set_icon(gui_icons["Script"]);
		menu_popup = menu_button->get_popup();
		menu_popup->add_icon_item(gui_icons["Script"], RTR("Decompile .GDC/.GDE script files..."), MENU_DECOMP_GDS);
		menu_popup->add_icon_item(gui_icons["Script"], RTR("Compile .GD script files..."), MENU_COMP_GDS);
		menu_popup->connect("id_pressed", this, "menu_option_pressed");
		p_menu->add_child(menu_button);

		menu_button = memnew(MenuButton);
		menu_button->set_text(RTR("Resources"));
		menu_button->set_icon(gui_icons["ResBT"]);
		menu_popup = menu_button->get_popup();
		menu_popup->add_icon_item(gui_icons["ResBT"], RTR("Convert binary resources to text..."), MENU_CONV_TO_TXT);
		menu_popup->add_icon_item(gui_icons["ResTB"], RTR("Convert text resources to binary..."), MENU_CONV_TO_BIN);
		menu_popup->add_separator();
		menu_popup->add_icon_item(gui_icons["ResOther"], RTR("Convert stream textures to PNG..."), MENU_STEX_TO_PNG);
		menu_popup->add_icon_item(gui_icons["ResOther"], RTR("Convert OGG Samples to OGG..."), MENU_OSTR_TO_OGG);
		menu_popup->add_icon_item(gui_icons["ResOther"], RTR("Convert WAV Samples to WAV..."), MENU_SMPL_TO_WAV);
		menu_popup->connect("id_pressed", this, "menu_option_pressed");
		p_menu->add_child(menu_button);
	} else {
		menu_button = memnew(MenuButton);
		menu_button->set_text(RTR("RE Tools"));
		menu_button->set_icon(gui_icons["Logo"]);
		menu_popup = menu_button->get_popup();
		menu_popup->add_icon_item(gui_icons["Logo"], RTR("About Godot RE Tools"), MENU_ABOUT_RE);
		menu_popup->add_separator();
		menu_popup->add_icon_item(gui_icons["Pack"], RTR("Create PCK archive from folder..."), MENU_CREATE_PCK);
		menu_popup->add_icon_item(gui_icons["Pack"], RTR("Explore PCK archive..."), MENU_EXT_PCK);
		menu_popup->add_separator();
		menu_popup->add_icon_item(gui_icons["Script"], RTR("Decompile .GDC/.GDE script files..."), MENU_DECOMP_GDS);
		menu_popup->add_icon_item(gui_icons["Script"], RTR("Compile .GD script files..."), MENU_COMP_GDS);
		menu_popup->add_separator();
		menu_popup->add_icon_item(gui_icons["ResBT"], RTR("Convert binary resources to text..."), MENU_CONV_TO_TXT);
		menu_popup->add_icon_item(gui_icons["ResTB"], RTR("Convert text resources to binary..."), MENU_CONV_TO_BIN);
		menu_popup->add_separator();
		menu_popup->add_icon_item(gui_icons["ResOther"], RTR("Convert stream textures to PNG..."), MENU_STEX_TO_PNG);
		menu_popup->add_icon_item(gui_icons["ResOther"], RTR("Convert OGG Samples to OGG..."), MENU_OSTR_TO_OGG);
		menu_popup->add_icon_item(gui_icons["ResOther"], RTR("Convert WAV Samples to WAV..."), MENU_SMPL_TO_WAV);
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
#ifdef TOOLS_ENABLED
	if (EditorSettings::get_singleton()) {
		bool show_on_start = EDITOR_GET("re/editor/show_info_on_start");
		about_dialog_checkbox->set_pressed(show_on_start);
	} else {
#else
	{
#endif
		about_dialog_checkbox->set_visible(false);
	}
	about_dialog->popup_centered_minsize();
}

void GodotREEditor::_toggle_about_dialog_on_start(bool p_enabled) {

#ifdef TOOLS_ENABLED
	if (EditorSettings::get_singleton()) {
		bool show_on_start = EDITOR_GET("re/editor/show_info_on_start");
		if (show_on_start != p_enabled) {
			EditorSettings::get_singleton()->set_setting("re/editor/show_info_on_start", p_enabled);
		}
	}
#endif
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
		case MENU_STEX_TO_PNG: {
			stex_file_selection->popup_centered(Size2(800, 600));
		} break;
		case MENU_OSTR_TO_OGG: {
			ostr_file_selection->popup_centered(Size2(800, 600));
		} break;
		case MENU_SMPL_TO_WAV: {
			smpl_file_selection->popup_centered(Size2(800, 600));
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

void GodotREEditor::print_warning(const String &p_text, const String &p_title, const String &p_sub_text) {

	char timestamp[21];
	OS::Date date = OS::get_singleton()->get_date();
	OS::Time time = OS::get_singleton()->get_time();
	sprintf(timestamp, "-%04d-%02d-%02d-%02d-%02d-%02d", date.year, date.month, date.day, time.hour, time.min, time.sec);

	Vector<String> lines = p_text.split("\n");
	if (lines.size() > 1) {
		emit_signal("write_log_message", String(timestamp) + " [" + p_title + "]: " + "\n");
		for (int i = 0; i < lines.size(); i++) {
			emit_signal("write_log_message", String(timestamp) + "       " + lines[i] + "\n");
		}
	} else {
		emit_signal("write_log_message", String(timestamp) + " [" + p_title + "]: " + p_text + "\n");
	}
	if (p_sub_text != String()) {
		Vector<String> slines = p_sub_text.split("\n");
		if (slines.size() > 1) {
			for (int i = 0; i < slines.size(); i++) {
				emit_signal("write_log_message", String(timestamp) + "       " + slines[i] + "\n");
			}
		} else {
			emit_signal("write_log_message", String(timestamp) + " [" + p_title + "]: " + p_sub_text + "\n");
		}
	}
}

void GodotREEditor::show_warning(const String &p_text, const String &p_title, const String &p_sub_text) {

	print_warning(p_text, p_title, p_sub_text);

	rdl->set_title(p_title);
	rdl->set_message(p_text, p_sub_text);
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

	print_warning("GDScriptDecomp{" + itos(script_dialog_d->get_bytecode_version()) + "}", RTR("Decompile"));

	String failed_files;
	GDScriptDecomp *dce = create_decomp_for_commit(script_dialog_d->get_bytecode_version());

	if (!dce) {
		show_warning(failed_files, RTR("Decompile"), RTR("Invalid bytecode version!"));
		return;
	}

	EditorProgressGDDC *pr = memnew(EditorProgressGDDC(ne_parent, "re_decompile", RTR("Decompiling files..."), files.size(), true));
	for (int i = 0; i < files.size(); i++) {
		print_warning(RTR("decompiling") + " " + files[i].get_file(), RTR("Decompile"));

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
			failed_files += files[i] + " (" + dce->get_error_message() + ")\n";
		}
	}

	memdelete(pr);
	memdelete(dce);

	if (failed_files.length() > 0) {
		show_warning(failed_files, RTR("Decompile"), RTR("At least one error was detected!"));
	} else {
		show_warning(RTR("No errors detected."), RTR("Decompile"), RTR("The operation completed successfully!"));
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

	EditorProgressGDDC *pr = memnew(EditorProgressGDDC(ne_parent, "re_compile", RTR("Compiling files..."), files.size(), true));

	for (int i = 0; i < files.size(); i++) {

		print_warning(RTR("compiling") + " " + files[i].get_file(), RTR("Compile"));
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

	if (failed_files.length() > 0) {
		show_warning(failed_files, RTR("Compile"), RTR("At least one error was detected!"));
	} else {
		show_warning(RTR("No errors detected."), RTR("Compile"), RTR("The operation completed successfully!"));
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
		show_warning(RTR("Error opening PCK file: ") + p_path, RTR("Read PCK"));
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
			show_warning(RTR("Invalid PCK file"), RTR("Read PCK"));
			memdelete(pck);
			return;
		}
		pck->seek(pck->get_position() - 12);

		uint64_t ds = pck->get_64();
		pck->seek(pck->get_position() - ds - 8);

		magic = pck->get_32();
		if (magic != 0x43504447) {
			show_warning(RTR("Invalid PCK file"), RTR("Read PCK"));
			memdelete(pck);
			return;
		}
		is_emb = true;
	}

	print_warning(RTR("filename") + " " + p_path, RTR("Read PCK"));

	uint32_t version = pck->get_32();

	if (version > 1) {
		show_warning(RTR("Pack version unsupported: ") + itos(version), RTR("Read PCK"));
		memdelete(pck);
		return;
	}

	uint32_t ver_major = pck->get_32();
	uint32_t ver_minor = pck->get_32();
	uint32_t ver_rev = pck->get_32();

	if ((ver_major < 3) || ((ver_major == 3) && (ver_minor < 2))) {
		pck_dialog->set_version(String("    ") + RTR("PCK version: ") + itos(version) + "; " + RTR("created by Godot engine: ") + itos(ver_major) + "." + itos(ver_minor) + ".x; " + ((is_emb) ? RTR("self contained exe") : RTR("standalone")));
		print_warning(RTR("PCK version: ") + itos(version) + "; " + RTR("created by Godot engine: ") + itos(ver_major) + "." + itos(ver_minor) + ".x; " + ((is_emb) ? RTR("self contained exe") : RTR("standalone")), RTR("Read PCK"));
	} else {
		pck_dialog->set_version(String("    ") + RTR("PCK version: ") + itos(version) + "; " + RTR("created by Godot engine: ") + itos(ver_major) + "." + itos(ver_minor) + "." + itos(ver_rev) + "; " + ((is_emb) ? RTR("self contained exe") : RTR("standalone")));
		print_warning(RTR("PCK version: ") + itos(version) + "; " + RTR("created by Godot engine: ") + itos(ver_major) + "." + itos(ver_minor) + "." + itos(ver_rev) + "; " + ((is_emb) ? RTR("self contained exe") : RTR("standalone")), RTR("Read PCK"));
	}
	for (int i = 0; i < 16; i++) {
		pck->get_32();
	}

	int file_count = pck->get_32();

	EditorProgressGDDC *pr = memnew(EditorProgressGDDC(ne_parent, "re_read_pck_md5", RTR("Reading PCK archive, click cancel to skip MD5 checking..."), file_count, true));

	bool p_check_md5 = true;
	int files_checked = 0;
	int files_broken = 0;

	uint64_t last_progress_upd = OS::get_singleton()->get_ticks_usec();

	for (int i = 0; i < file_count; i++) {

		uint32_t sl = pck->get_32();
		CharString cs;
		cs.resize(sl + 1);
		pck->get_buffer((uint8_t *)cs.ptr(), sl);
		cs[sl] = 0;

		String path;
		path.parse_utf8(cs.ptr());

		path = path.replace("res://", "");
		String raw_path = path;

		bool malformed = false;
		while (path.begins_with("~")) {
			path = path.substr(1, path.length() - 1);
			malformed = true;
		}
		while (path.begins_with("/")) {
			path = path.substr(1, path.length() - 1);
			malformed = true;
		}
		while (path.find("...") >= 0) {
			path = path.replace("...", "_");
			malformed = true;
		}
		while (path.find("..") >= 0) {
			path = path.replace("..", "_");
			malformed = true;
		}
		if (path.find("\\.") >= 0) {
			path = path.replace("\\.", "_");
			malformed = true;
		}
		if (path.find("//") >= 0) {
			path = path.replace("//", "_");
			malformed = true;
		}
		if (path.find("\\") >= 0) {
			path = path.replace("\\", "_");
			malformed = true;
		}
		if (path.find(":") >= 0) {
			path = path.replace(":", "_");
			malformed = true;
		}
		if (path.find("|") >= 0) {
			path = path.replace("|", "_");
			malformed = true;
		}
		if (path.find("?") >= 0) {
			path = path.replace("?", "_");
			malformed = true;
		}
		if (path.find(">") >= 0) {
			path = path.replace(">", "_");
			malformed = true;
		}
		if (path.find("<") >= 0) {
			path = path.replace("<", "_");
			malformed = true;
		}
		if (path.find("*") >= 0) {
			path = path.replace("*", "_");
			malformed = true;
		}
		if (path.find("\"") >= 0) {
			path = path.replace("\"", "_");
			malformed = true;
		}
		if (path.find("\'") >= 0) {
			path = path.replace("\'", "_");
			malformed = true;
		}

		uint64_t ofs = pck->get_64();
		uint64_t size = pck->get_64();
		uint8_t md5_saved[16];
		pck->get_buffer(md5_saved, 16);

		if (OS::get_singleton()->get_ticks_usec() - last_progress_upd > 20000) {
			last_progress_upd = OS::get_singleton()->get_ticks_usec();

			bool cancel = pr->step(path, i, true);
			if (cancel) {
				if (p_check_md5) {
					memdelete(pr);
					pr = memnew(EditorProgressGDDC(ne_parent, "re_read_pck_no_md5", RTR("Reading PCK archive, click cancel again to abort..."), file_count, true));
					p_check_md5 = false;
				} else {
					memdelete(pr);
					memdelete(pck);

					pck_dialog->clear();
					pck_files.clear();
					return;
				}
			}
		}

		if (p_check_md5) {
			size_t oldpos = pck->get_position();
			pck->seek(ofs);

			files_checked++;

#if ((VERSION_MAJOR == 3) && (VERSION_MINOR == 2)) || (VERSION_MAJOR == 4)
			CryptoCore::MD5Context ctx;
			ctx.start();
#else
			MD5_CTX md5;
			MD5Init(&md5);
#endif

			int64_t rq_size = size;
			uint8_t buf[32768];

			while (rq_size > 0) {

				int got = pck->get_buffer(buf, MIN(32768, rq_size));
				if (got > 0) {
#if ((VERSION_MAJOR == 3) && (VERSION_MINOR == 2)) || (VERSION_MAJOR == 4)
					ctx.update(buf, got);
#else
					MD5Update(&md5, buf, got);
#endif
				}
				if (got < 4096)
					break;
				rq_size -= 32768;
			}

#if ((VERSION_MAJOR == 3) && (VERSION_MINOR == 2)) || (VERSION_MAJOR == 4)
			unsigned char hash[16];
			ctx.finish(hash);
#else
			MD5Final(&md5);
#endif
			pck->seek(oldpos);

			String file_md5;
			String saved_md5;
			String error_msg = "";

			bool md5_match = true;
			for (int j = 0; j < 16; j++) {
#if ((VERSION_MAJOR == 3) && (VERSION_MINOR == 2)) || (VERSION_MAJOR == 4)
				md5_match &= (hash[j] == md5_saved[j]);
				file_md5 += String::num_uint64(hash[j], 16);
#else
				md5_match &= (md5.digest[j] == md5_saved[j]);
				file_md5 += String::num_uint64(md5.digest[j], 16);
#endif
				saved_md5 += String::num_uint64(md5_saved[j], 16);
			}
			if (!md5_match) {
				files_broken++;
				error_msg += RTR("MD5 mismatch, calculated") + " [" + file_md5 + "] " + RTR("expected") + " [" + saved_md5 + "]\n";
			}
			if (malformed) {
				files_broken++;
				error_msg += RTR("Malformed path") + " \"" + raw_path + "\"";
			}
			pck_dialog->add_file(path, size, (!md5_match || malformed) ? gui_icons["FileBroken"] : gui_icons["FileOk"], error_msg, malformed);
		} else {
			String error_msg = RTR("MD5 check skipped") + "\n";
			if (malformed) {
				files_broken++;
				error_msg += RTR("Malformed path") + " \"" + raw_path + "\"";
			}
			pck_dialog->add_file(path, size, (malformed) ? gui_icons["FileBroken"] : gui_icons["File"], error_msg, malformed);
		}

		pck_files[path] = PackedFile(ofs, size);
	};

	pck_dialog->set_info(String("    ") + RTR("Total files: ") + itos(file_count) + "; " + RTR("Checked: ") + itos(files_checked) + "; " + RTR("Broken: ") + itos(files_broken));
	print_warning(RTR("Total files: ") + itos(file_count) + "; " + RTR("Checked: ") + itos(files_checked) + "; " + RTR("Broken: ") + itos(files_broken), RTR("Read PCK"));

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
		show_warning(RTR("Error opening PCK file: ") + pck_file, RTR("Read PCK"));
		return;
	}

	Vector<String> files = pck_dialog->get_selected_files();
	String dir = pck_dialog->get_target_dir();

	String failed_files;

	EditorProgressGDDC *pr = memnew(EditorProgressGDDC(ne_parent, "re_ext_pck", RTR("Extracting files..."), files.size(), true));
	DirAccess *da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);

	for (int i = 0; i < files.size(); i++) {
		String target_name = dir.plus_file(files[i]);

		da->make_dir_recursive(target_name.get_base_dir());

		print_warning("extracting " + files[i], RTR("Read PCK"));
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

	if (failed_files.length() > 0) {
		show_warning(failed_files, RTR("Read PCK"), RTR("At least one error was detected!"));
	} else {
		show_warning(RTR("No errors detected."), RTR("Read PCK"), RTR("The operation completed successfully!"));
	}
}

/*************************************************************************/
/* Res convert                                                           */
/*************************************************************************/

void GodotREEditor::_res_smpl_2_wav_request(const PoolVector<String> &p_files) {

	DirAccess *da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	String overwrite_list = String();
	for (int i = 0; i < p_files.size(); i++) {
		if (da->file_exists(p_files[i].get_basename() + ".wav")) {
			overwrite_list += p_files[i].get_basename() + ".wav" + "\n";
		}
	}

	res_files = p_files;

	if (overwrite_list.length() == 0) {
		_res_smpl_2_wav_process();
	} else {
		ovd->set_message(overwrite_list);
		ovd->connect("confirmed", this, "_res_smpl_2_wav_process", Vector<Variant>(), CONNECT_ONESHOT);
		ovd->popup_centered();
	}
}

void GodotREEditor::_res_smpl_2_wav_process() {

	EditorProgressGDDC *pr = memnew(EditorProgressGDDC(ne_parent, "re_wav2_res", RTR("Converting files..."), res_files.size(), true));

	String failed_files;
	Ref<ResourceFormatLoaderBinary> rl = memnew(ResourceFormatLoaderBinary);
	for (int i = 0; i < res_files.size(); i++) {

		print_warning("converting " + res_files[i], RTR("Convert WAV samples"));
		bool cancel = pr->step(res_files[i], i, true);
		if (cancel) {
			break;
		}

		Ref<ResourceInteractiveLoaderBinary> ria = rl->load_interactive(res_files[i]);
		Error err = ria->poll();
		while (err == OK) {
			err = ria->poll();
		}
		if (ria->get_resource().is_null()) {
			failed_files += res_files[i] + " (load AudioStreamSample error)\n";
			continue;
		}

		Ref<AudioStreamSample> sample = ria->get_resource();

		sample->save_to_wav(res_files[i].get_basename() + ".wav");
	}

	memdelete(pr);
	res_files = PoolVector<String>();

	if (failed_files.length() > 0) {
		show_warning(failed_files, RTR("Convert WAV samples"), RTR("At least one error was detected!"));
	} else {
		show_warning(RTR("No errors detected."), RTR("Convert WAV samples"), RTR("The operation completed successfully!"));
	}
}

void GodotREEditor::_res_ostr_2_ogg_request(const PoolVector<String> &p_files) {

	DirAccess *da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	String overwrite_list = String();
	for (int i = 0; i < p_files.size(); i++) {
		if (da->file_exists(p_files[i].get_basename() + ".ogg")) {
			overwrite_list += p_files[i].get_basename() + ".ogg" + "\n";
		}
	}

	res_files = p_files;

	if (overwrite_list.length() == 0) {
		_res_ostr_2_ogg_process();
	} else {
		ovd->set_message(overwrite_list);
		ovd->connect("confirmed", this, "_res_ostr_2_ogg_process", Vector<Variant>(), CONNECT_ONESHOT);
		ovd->popup_centered();
	}
}

void GodotREEditor::_res_ostr_2_ogg_process() {

	EditorProgressGDDC *pr = memnew(EditorProgressGDDC(ne_parent, "re_ogg2_res", RTR("Converting files..."), res_files.size(), true));

	String failed_files;
	Ref<ResourceFormatLoaderBinary> rl = memnew(ResourceFormatLoaderBinary);
	for (int i = 0; i < res_files.size(); i++) {

		bool cancel = pr->step(res_files[i], i, true);
		if (cancel) {
			break;
		}

		print_warning("converting " + res_files[i], RTR("Convert OGG samples"));
		Ref<ResourceInteractiveLoaderBinary> ria = rl->load_interactive(res_files[i]);
		Error err = ria->poll();
		while (err == OK) {
			err = ria->poll();
		}
		if (ria->get_resource().is_null()) {
			failed_files += res_files[i] + " (load AudioStreamOGGVorbis error)\n";
			continue;
		}
		Ref<AudioStreamOGGVorbis> sample = ria->get_resource();

		PoolVector<uint8_t> buf = sample->get_data();

		FileAccess *res = FileAccess::open(res_files[i].get_basename() + ".ogg", FileAccess::WRITE);
		if (!res) {
			failed_files += res_files[i] + " (write error)\n";
			continue;
		}
		res->store_buffer(buf.read().ptr(), buf.size());
		res->close();
	}

	memdelete(pr);
	res_files = PoolVector<String>();

	if (failed_files.length() > 0) {
		show_warning(failed_files, RTR("Convert OGG samples"), RTR("At least one error was detected!"));
	} else {
		show_warning(RTR("No errors detected."), RTR("Convert OGG samples"), RTR("The operation completed successfully!"));
	}
}

void GodotREEditor::_res_stex_2_png_request(const PoolVector<String> &p_files) {

	DirAccess *da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	String overwrite_list = String();
	for (int i = 0; i < p_files.size(); i++) {
		if (da->file_exists(p_files[i].get_basename() + ".png")) {
			overwrite_list += p_files[i].get_basename() + ".png" + "\n";
		}
	}

	res_files = p_files;

	if (overwrite_list.length() == 0) {
		_res_stxt_2_png_process();
	} else {
		ovd->set_message(overwrite_list);
		ovd->connect("confirmed", this, "_res_stxt_2_png_process", Vector<Variant>(), CONNECT_ONESHOT);
		ovd->popup_centered();
	}
}

void GodotREEditor::_res_stxt_2_png_process() {

	EditorProgressGDDC *pr = memnew(EditorProgressGDDC(ne_parent, "re_st2pnh_res", RTR("Converting files..."), res_files.size(), true));

	String failed_files;
	for (int i = 0; i < res_files.size(); i++) {

		print_warning("converting " + res_files[i], RTR("Convert textures"));
		bool cancel = pr->step(res_files[i], i, true);
		if (cancel) {
			break;
		}

		Ref<StreamTexture> stex;
		stex.instance();
		Error err = stex->load(res_files[i]);
		if (err != OK) {
			failed_files += res_files[i] + " (load StreamTexture error)\n";
			continue;
		}

		Ref<Image> img = stex->get_data();
		if (img.is_null()) {
			failed_files += res_files[i] + " (invalid texture data)\n";
			continue;
		}

		err = img->save_png(res_files[i].get_basename() + ".png");
		if (err != OK) {
			failed_files += res_files[i] + " (write error)\n";
		}
	}

	memdelete(pr);
	res_files = PoolVector<String>();

	if (failed_files.length() > 0) {
		show_warning(failed_files, RTR("Convert textures"), RTR("At least one error was detected!"));
	} else {
		show_warning(RTR("No errors detected."), RTR("Convert textures"), RTR("The operation completed successfully!"));
	}
}

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

	EditorProgressGDDC *pr = memnew(EditorProgressGDDC(ne_parent, "re_b2t_res", RTR("Converting files..."), res_files.size(), true));

	String failed_files;
	for (int i = 0; i < res_files.size(); i++) {

		print_warning("converting " + res_files[i], RTR("Convert resources"));
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

	if (failed_files.length() > 0) {
		show_warning(failed_files, RTR("Convert resources"), RTR("At least one error was detected!"));
	} else {
		show_warning(RTR("No errors detected."), RTR("Convert resources"), RTR("The operation completed successfully!"));
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

	EditorProgressGDDC *pr = memnew(EditorProgressGDDC(ne_parent, "re_t2b_res", RTR("Converting files..."), res_files.size(), true));

	String failed_files;
	for (int i = 0; i < res_files.size(); i++) {

		print_warning("converting " + res_files[i], RTR("Convert resources"));
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

	if (failed_files.length() > 0) {
		show_warning(failed_files, RTR("Convert resources"), RTR("At least one error was detected!"));
	} else {
		show_warning(RTR("No errors detected."), RTR("Convert resources"), RTR("The operation completed successfully!"));
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

	EditorProgressGDDC *pr = memnew(EditorProgressGDDC(ne_parent, "re_read_folder", RTR("Reading folder structure..."), 1, true));
	bool cancel = false;
	uint64_t size = _pck_create_process_folder(pr, p_path, String(), 0, cancel);
	memdelete(pr);

	if (cancel) {
		return;
	}

	if (size == 0) {
		show_warning(RTR("Error opening folder (or empty folder): ") + p_path, RTR("New PCK"));
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

	pck_save_file_selection->clear_filters();
	if (pck_save_dialog->get_is_emb()) {
		pck_save_file_selection->add_filter("*.exe,*.bin,*.32,*.64;Self contained executable files");
	} else {
		pck_save_file_selection->add_filter("*.pck;PCK archive files");
	}
	pck_save_file_selection->popup_centered(Size2(800, 600));
}

#define PCK_PADDING 16

uint64_t GodotREEditor::_pck_create_process_folder(EditorProgressGDDC *p_pr, const String &p_path, const String &p_rel, uint64_t p_offset, bool &p_cancel) {

	uint64_t offset = p_offset;

	DirAccess *da = DirAccess::open(p_path.plus_file(p_rel));
	if (!da) {
		show_warning(RTR("Error opening folder: ") + p_path.plus_file(p_rel), RTR("New PCK"));
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

#if ((VERSION_MAJOR == 3) && (VERSION_MINOR == 2)) || (VERSION_MAJOR == 4)
			CryptoCore::MD5Context ctx;
			ctx.start();
#else
			MD5_CTX md5;
			MD5Init(&md5);
#endif

			int64_t rq_size = file->get_len();
			uint8_t buf[32768];

			while (rq_size > 0) {

				int got = file->get_buffer(buf, MIN(32768, rq_size));
				if (got > 0) {
#if ((VERSION_MAJOR == 3) && (VERSION_MINOR == 2)) || (VERSION_MAJOR == 4)
					ctx.update(buf, got);
#else
					MD5Update(&md5, buf, got);
#endif
				}
				if (got < 4096)
					break;
				rq_size -= 32768;
			}

#if ((VERSION_MAJOR == 3) && (VERSION_MINOR == 2)) || (VERSION_MAJOR == 4)
			unsigned char hash[16];
			ctx.finish(hash);
#else
			MD5Final(&md5);
#endif

			PackedFile finfo = PackedFile(offset, file->get_len());
			finfo.name = p_rel.plus_file(f);
			for (int j = 0; j < 16; j++) {
#if ((VERSION_MAJOR == 3) && (VERSION_MINOR == 2)) || (VERSION_MAJOR == 4)
				finfo.md5[j] = hash[j];
#else
				finfo.md5[j] = md5.digest[j];
#endif
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

	int64_t embedded_start = 0;
	int64_t embedded_size = 0;

	FileAccess *f = FileAccess::open(p_path, FileAccess::WRITE);
	if (!f) {
		show_warning(RTR("Error opening PCK file: ") + p_path, RTR("New PCK"));
		return;
	}

	EditorProgressGDDC *pr = memnew(EditorProgressGDDC(ne_parent, "re_write_pck", RTR("Writing PCK archive..."), pck_save_files.size() + 2, true));

	if (pck_save_dialog->get_is_emb()) {
		// append to exe
		FileAccess *fs = FileAccess::open(pck_save_dialog->get_emb_source(), FileAccess::READ);
		if (!fs) {
			show_warning(RTR("Error opening source executable file: ") + pck_save_dialog->get_emb_source(), RTR("New PCK"));
			return;
		}

		pr->step("Exec...", 0, true);

		fs->seek_end();
		fs->seek(fs->get_position() - 4);
		int32_t magic = fs->get_32();
		if (magic == 0x43504447) {
			// exe already have embedded pck
			fs->seek(fs->get_position() - 12);
			uint64_t ds = f->get_64();
			fs->seek(fs->get_position() - ds - 8);
		} else {
			fs->seek_end();
		}
		int64_t exe_end = fs->get_position();
		fs->seek(0);
		// copy executable data
		for (int i = 0; i < exe_end; i++) {
			f->store_8(fs->get_8());
		}
		memdelete(fs);

		embedded_start = f->get_position();

		// ensure embedded PCK starts at a 64-bit multiple
		int pad = f->get_position() % 8;
		for (int i = 0; i < pad; i++) {
			f->store_8(0);
		}
	}
	int64_t pck_start_pos = f->get_position();

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
		print_warning("saving " + pck_save_files[i].name, RTR("New PCK"));
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

	if (pck_save_dialog->get_is_emb()) {
		// ensure embedded data ends at a 64-bit multiple
		int64_t embed_end = f->get_position() - embedded_start + 12;
		int pad = embed_end % 8;
		for (int i = 0; i < pad; i++) {
			f->store_8(0);
		}

		int64_t pck_size = f->get_position() - pck_start_pos;
		f->store_64(pck_size);
		f->store_32(0x43504447); //GDPC

		embedded_size = f->get_position() - embedded_start;

		// fixup headers
		pr->step("Exec header fix...", pck_save_files.size() + 2, true);
		f->seek(0);
		int16_t exe1_magic = f->get_16();
		int16_t exe2_magic = f->get_16();
		if (exe1_magic == 0x5A4D) {
			//windows (pe) - copy from "platform/windows/export/export.cpp"
			f->seek(0x3c);
			uint32_t pe_pos = f->get_32();

			f->seek(pe_pos);
			uint32_t magic = f->get_32();
			if (magic != 0x00004550) {
				show_warning(RTR("Invalid PE magic"), RTR("New PCK"), RTR("At least one error was detected!"));
				pck_save_files.clear();
				pck_file = String();
				memdelete(f);
				memdelete(pr);
				return;
			}

			// Process header
			int num_sections;
			{
				int64_t header_pos = f->get_position();

				f->seek(header_pos + 2);
				num_sections = f->get_16();
				f->seek(header_pos + 16);
				uint16_t opt_header_size = f->get_16();

				// Skip rest of header + optional header to go to the section headers
				f->seek(f->get_position() + 2 + opt_header_size);
			}

			// Search for the "pck" section
			int64_t section_table_pos = f->get_position();

			bool found = false;
			for (int i = 0; i < num_sections; ++i) {

				int64_t section_header_pos = section_table_pos + i * 40;
				f->seek(section_header_pos);

				uint8_t section_name[9];
				f->get_buffer(section_name, 8);
				section_name[8] = '\0';

				if (strcmp((char *)section_name, "pck") == 0) {
					// "pck" section found, let's patch!

					// Set virtual size to a little to avoid it taking memory (zero would give issues)
					f->seek(section_header_pos + 8);
					f->store_32(8);

					f->seek(section_header_pos + 16);
					f->store_32(embedded_size);
					f->seek(section_header_pos + 20);
					f->store_32(embedded_start);

					found = true;
					break;
				}
			}
		} else if ((exe1_magic == 0x457F) && (exe2_magic == 0x467C)) {
			// linux (elf) - copy from "platform/x11/export/export.cpp"
			// Read program architecture bits from class field
			int bits = f->get_8() * 32;

			if (bits == 32 && embedded_size >= 0x100000000) {
				show_warning(RTR("32-bit executables cannot have embedded data >= 4 GiB"), RTR("New PCK"), RTR("At least one error was detected!"));
				pck_save_files.clear();
				pck_file = String();
				memdelete(f);
				memdelete(pr);
				return;
			}

			// Get info about the section header table
			int64_t section_table_pos;
			int64_t section_header_size;
			if (bits == 32) {
				section_header_size = 40;
				f->seek(0x20);
				section_table_pos = f->get_32();
				f->seek(0x30);
			} else { // 64
				section_header_size = 64;
				f->seek(0x28);
				section_table_pos = f->get_64();
				f->seek(0x3c);
			}
			int num_sections = f->get_16();
			int string_section_idx = f->get_16();

			// Load the strings table
			uint8_t *strings;
			{
				// Jump to the strings section header
				f->seek(section_table_pos + string_section_idx * section_header_size);

				// Read strings data size and offset
				int64_t string_data_pos;
				int64_t string_data_size;
				if (bits == 32) {
					f->seek(f->get_position() + 0x10);
					string_data_pos = f->get_32();
					string_data_size = f->get_32();
				} else { // 64
					f->seek(f->get_position() + 0x18);
					string_data_pos = f->get_64();
					string_data_size = f->get_64();
				}

				// Read strings data
				f->seek(string_data_pos);
				strings = (uint8_t *)memalloc(string_data_size);
				if (!strings) {
					show_warning(RTR("Out of memory"), RTR("New PCK"), RTR("At least one error was detected!"));
					pck_save_files.clear();
					pck_file = String();
					memdelete(f);
					memdelete(pr);
					return;
				}
				f->get_buffer(strings, string_data_size);
			}

			// Search for the "pck" section
			bool found = false;
			for (int i = 0; i < num_sections; ++i) {

				int64_t section_header_pos = section_table_pos + i * section_header_size;
				f->seek(section_header_pos);

				uint32_t name_offset = f->get_32();
				if (strcmp((char *)strings + name_offset, "pck") == 0) {
					// "pck" section found, let's patch!

					if (bits == 32) {
						f->seek(section_header_pos + 0x10);
						f->store_32(embedded_start);
						f->store_32(embedded_size);
					} else { // 64
						f->seek(section_header_pos + 0x18);
						f->store_64(embedded_start);
						f->store_64(embedded_size);
					}

					found = true;
					break;
				}
			}
			memfree(strings);
		}
	}

	pck_save_files.clear();
	pck_file = String();
	memdelete(f);
	memdelete(pr);

	if (failed_files.length() > 0) {
		show_warning(failed_files, RTR("New PCK"), RTR("At least one error was detected!"));
	} else {
		show_warning(RTR("No errors detected."), RTR("New PCK"), RTR("The operation completed successfully!"));
	}
}

/*************************************************************************/

void GodotREEditor::_notification(int p_notification) {

	switch (p_notification) {

		case NOTIFICATION_READY: {
#ifdef TOOLS_ENABLED
			if (EditorSettings::get_singleton()) {
				bool show_info_dialog = EDITOR_GET("re/editor/show_info_on_start");
				if (show_info_dialog) {
					about_dialog->set_exclusive(true);
					show_about_dialog();
					about_dialog->set_exclusive(false);
				}
			} else {
#else
			{
#endif
				about_dialog->set_exclusive(true);
				show_about_dialog();
				about_dialog->set_exclusive(false);
			}
			emit_signal("write_log_message", String("****\nGodot RE Tools, ") + String(GDRE_VERSION) + String("\n****\n\n"));
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

	ClassDB::bind_method(D_METHOD("_res_stex_2_png_request", "files"), &GodotREEditor::_res_stex_2_png_request);
	ClassDB::bind_method(D_METHOD("_res_stxt_2_png_process"), &GodotREEditor::_res_stxt_2_png_process);

	ClassDB::bind_method(D_METHOD("_res_ostr_2_ogg_request", "files"), &GodotREEditor::_res_ostr_2_ogg_request);
	ClassDB::bind_method(D_METHOD("_res_ostr_2_ogg_process"), &GodotREEditor::_res_ostr_2_ogg_process);

	ClassDB::bind_method(D_METHOD("_res_smpl_2_wav_request", "files"), &GodotREEditor::_res_smpl_2_wav_request);
	ClassDB::bind_method(D_METHOD("_res_smpl_2_wav_process"), &GodotREEditor::_res_smpl_2_wav_process);

	ClassDB::bind_method(D_METHOD("show_about_dialog"), &GodotREEditor::show_about_dialog);
	ClassDB::bind_method(D_METHOD("get_gui_icon", "name"), &GodotREEditor::get_gui_icon);

	ClassDB::bind_method(D_METHOD("menu_option_pressed", "id"), &GodotREEditor::menu_option_pressed);
	ClassDB::bind_method(D_METHOD("convert_file_to_binary", "src_path", "dst_path"), &GodotREEditor::convert_file_to_binary);
	ClassDB::bind_method(D_METHOD("convert_file_to_text", "src_path", "dst_path"), &GodotREEditor::convert_file_to_text);

	ADD_SIGNAL(MethodInfo("write_log_message", PropertyInfo(Variant::STRING, "message")));
};

/*************************************************************************/

void GodotREEditorStandalone::_notification(int p_notification) {

	if (p_notification == MainLoop::NOTIFICATION_WM_ABOUT) {
		if (editor_ctx)
			editor_ctx->show_about_dialog();
	}
}

void GodotREEditorStandalone::_write_log_message(String p_message) {

	emit_signal("write_log_message", p_message);
}

String GodotREEditorStandalone::get_version() {

	return String(GDRE_VERSION);
}

void GodotREEditorStandalone::_bind_methods() {

	ClassDB::bind_method(D_METHOD("_write_log_message"), &GodotREEditorStandalone::_write_log_message);
	ADD_SIGNAL(MethodInfo("write_log_message", PropertyInfo(Variant::STRING, "message")));

	ClassDB::bind_method(D_METHOD("get_version"), &GodotREEditorStandalone::get_version);
}

GodotREEditorStandalone::GodotREEditorStandalone() {

	menu_hb = memnew(HBoxContainer);
	add_child(menu_hb);

	editor_ctx = memnew(GodotREEditor(this, menu_hb));
	editor_ctx->connect("write_log_message", this, "_write_log_message");

	add_child(editor_ctx);
}

GodotREEditorStandalone::~GodotREEditorStandalone() {

	editor_ctx = NULL;
}
