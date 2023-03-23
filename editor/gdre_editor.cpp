/*************************************************************************/
/*  gdre_editor.cpp                                                      */
/*************************************************************************/

#include "gdre_version.gen.h"
/*************************************************************************/
#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"
#include "editor/editor_settings.h"
#endif
/*************************************************************************/
#include "gdre_editor.h"

#include "bytecode/bytecode_versions.h"
#include "compat/oggstr_loader_compat.h"
#include "compat/resource_loader_compat.h"
#include "compat/texture_loader_compat.h"
#include "utility/gdre_settings.h"
#include "utility/import_exporter.h"
#include "utility/pcfg_loader.h"
#include "utility/pck_dumper.h"

#include "modules/gdscript/gdscript.h"
#include "modules/gdscript/gdscript_utility_functions.h"

#include "core/io/file_access_encrypted.h"
#include "core/io/resource_format_binary.h"
#include "modules/svg/image_loader_svg.h"
#include "modules/vorbis/audio_stream_ogg_vorbis.h"
#include "scene/main/canvas_item.h"
#include "scene/resources/audio_stream_wav.h"
#include "scene/resources/resource_format_text.h"

#include "core/version_generated.gen.h"

#include "gdre_icons.gen.h"

#if VERSION_MAJOR < 4
#error Unsupported Godot version
#endif

#include "core/crypto/crypto_core.h"

#ifndef TOOLS_ENABLED
#include "core/object/message_queue.h"
#include "core/os/os.h"
#include "main/main.h"

#endif

/*************************************************************************/

GodotREEditor *GodotREEditor::singleton = NULL;

/*************************************************************************/

ResultDialog::ResultDialog() {
	set_title(RTR("OK"));
	set_flag(Window::Flags::FLAG_RESIZE_DISABLED, false);

	VBoxContainer *script_vb = memnew(VBoxContainer);

	lbl = memnew(Label);
	script_vb->add_child(lbl);

	message = memnew(TextEdit);
	message->set_editable(false);
	message->set_custom_minimum_size(Size2(600, 200) * EDSCALE);
	message->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	message->set_v_size_flags(Control::SIZE_EXPAND_FILL);
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
	set_flag(Window::Flags::FLAG_RESIZE_DISABLED, false);

	VBoxContainer *script_vb = memnew(VBoxContainer);

	lbl = memnew(Label);
	lbl->set_text(RTR("Following files already exist and will be overwritten:"));
	script_vb->add_child(lbl);

	message = memnew(TextEdit);
	message->set_editable(false);
	message->set_custom_minimum_size(Size2(600, 100) * EDSCALE);
	message->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	message->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	script_vb->add_child(message);

	add_child(script_vb);

	get_ok_button()->set_text(RTR("Overwrite"));
	add_cancel_button(RTR("Cancel"));
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

static Ref<ImageTexture> generate_icon(int p_index) {
	Ref<Image> img = memnew(Image);

#ifdef MODULE_SVG_ENABLED
	// Upsample icon generation only if the scale isn't an integer multiplier.
	// Generating upsampled icons is slower, and the benefit is hardly visible
	// with integer scales.
	ImageLoaderSVG img_loader;
	img_loader.create_image_from_string(img, gdre_icons_sources[p_index], 1.0, false, false);
#endif

	return ImageTexture::create_from_image(img);
}

#ifdef TOOLS_ENABLED
GodotREEditor::GodotREEditor(EditorNode *p_editor) {
	singleton = this;
	editor = p_editor;
	ne_parent = editor->get_gui_base();
	gdres_singleton = memnew(GDRESettings);

	init_gui(editor->get_gui_base(), editor->get_menu_hb(), false);
}
#endif

GodotREEditor::GodotREEditor(Control *p_control, HBoxContainer *p_menu) {
	singleton = this;
	ne_parent = p_control;
	gdres_singleton = memnew(GDRESettings);
	pdialog_singleton = memnew(ProgressDialog);
	p_control->add_child(pdialog_singleton);

	init_gui(p_control, p_menu, true);
}

void init_icons() {
}

void GodotREEditor::init_gui(Control *p_control, HBoxContainer *p_menu, bool p_long_menu) {
	//Init dialogs

	// Convert the generated icon sources to a dictionary for easier access.
	// Unlike the editor icons, there is no central repository of icons in the Theme resource itself to keep it tidy.
	for (int i = 0; i < gdre_icons_count; i++) {
		icons[gdre_icons_names[i]] = generate_icon(i);
		ne_parent->add_theme_icon_override(gdre_icons_names[i], icons[gdre_icons_names[i]]);
	}

	ovd = memnew(OverwriteDialog);
	p_control->add_child(ovd);

	rdl = memnew(ResultDialog);
	p_control->add_child(rdl);

	key_dialog = memnew(EncKeyDialog);
	p_control->add_child(key_dialog);

	script_dialog_d = memnew(ScriptDecompDialog);
	script_dialog_d->connect("confirmed", callable_mp(this, &GodotREEditor::_decompile_files));
	p_control->add_child(script_dialog_d);

	script_dialog_c = memnew(ScriptCompDialog);
	script_dialog_c->connect("confirmed", callable_mp(this, &GodotREEditor::_compile_files));
	p_control->add_child(script_dialog_c);

	pck_dialog = memnew(PackDialog);
	pck_dialog->connect("confirmed", callable_mp(this, &GodotREEditor::_pck_extract_files));
	pck_dialog->connect("canceled", callable_mp(this, &GodotREEditor::_pck_unload));
	p_control->add_child(pck_dialog);

	pck_source_folder = memnew(FileDialog);
	pck_source_folder->set_access(FileDialog::ACCESS_FILESYSTEM);
	pck_source_folder->set_file_mode(FileDialog::FILE_MODE_OPEN_DIR);
	pck_source_folder->connect("dir_selected", callable_mp(this, &GodotREEditor::_pck_create_request));
	pck_source_folder->set_show_hidden_files(true);
	p_control->add_child(pck_source_folder);

	pck_save_dialog = memnew(NewPackDialog);
	pck_save_dialog->connect("confirmed", callable_mp(this, &GodotREEditor::_pck_save_prep));
	p_control->add_child(pck_save_dialog);

	pck_save_file_selection = memnew(FileDialog);
	pck_save_file_selection->set_access(FileDialog::ACCESS_FILESYSTEM);
	pck_save_file_selection->set_file_mode(FileDialog::FILE_MODE_SAVE_FILE);
	pck_save_file_selection->connect("file_selected", callable_mp(this, &GodotREEditor::_pck_save_request));
	pck_save_file_selection->set_show_hidden_files(true);
	p_control->add_child(pck_save_file_selection);

	pck_file_selection = memnew(FileDialog);
	pck_file_selection->set_access(FileDialog::ACCESS_FILESYSTEM);
	pck_file_selection->set_file_mode(FileDialog::FILE_MODE_OPEN_FILE);
	pck_file_selection->add_filter("*.pck;PCK archive files");
	pck_file_selection->add_filter("*.exe,*.bin,*.32,*.64;Self contained executable files");
	pck_file_selection->add_filter("*.apk;Android application files");
	pck_file_selection->add_filter("*.zip;Zipped Godot project files");
	pck_file_selection->connect("file_selected", callable_mp(this, &GodotREEditor::_pck_select_request));
	pck_file_selection->set_show_hidden_files(true);
	p_control->add_child(pck_file_selection);

	bin_res_file_selection = memnew(FileDialog);
	bin_res_file_selection->set_access(FileDialog::ACCESS_FILESYSTEM);
	bin_res_file_selection->set_file_mode(FileDialog::FILE_MODE_OPEN_FILES);
	bin_res_file_selection->add_filter("*.scn,*.res;Binary resource files");
	bin_res_file_selection->connect("files_selected", callable_mp(this, &GodotREEditor::_res_bin_2_txt_request));
	bin_res_file_selection->set_show_hidden_files(true);
	p_control->add_child(bin_res_file_selection);

	txt_res_file_selection = memnew(FileDialog);
	txt_res_file_selection->set_access(FileDialog::ACCESS_FILESYSTEM);
	txt_res_file_selection->set_file_mode(FileDialog::FILE_MODE_OPEN_FILES);
	txt_res_file_selection->add_filter("*.escn,*.tscn,*.tres;Text resource files");
	txt_res_file_selection->connect("files_selected", callable_mp(this, &GodotREEditor::_res_txt_2_bin_request));
	txt_res_file_selection->set_show_hidden_files(true);
	p_control->add_child(txt_res_file_selection);

	stex_file_selection = memnew(FileDialog);
	stex_file_selection->set_access(FileDialog::ACCESS_FILESYSTEM);
	stex_file_selection->set_file_mode(FileDialog::FILE_MODE_OPEN_FILES);
	stex_file_selection->add_filter("*.ctex,*.stex,*.tex;Stream texture files");
	stex_file_selection->connect("files_selected", callable_mp(this, &GodotREEditor::_res_stex_2_png_request));
	stex_file_selection->set_show_hidden_files(true);
	p_control->add_child(stex_file_selection);

	ostr_file_selection = memnew(FileDialog);
	ostr_file_selection->set_access(FileDialog::ACCESS_FILESYSTEM);
	ostr_file_selection->set_file_mode(FileDialog::FILE_MODE_OPEN_FILES);
	ostr_file_selection->add_filter("*.oggstr;OGG Sample files");
	ostr_file_selection->connect("files_selected", callable_mp(this, &GodotREEditor::_res_ostr_2_ogg_request));
	ostr_file_selection->set_show_hidden_files(true);
	p_control->add_child(ostr_file_selection);

	smpl_file_selection = memnew(FileDialog);
	smpl_file_selection->set_access(FileDialog::ACCESS_FILESYSTEM);
	smpl_file_selection->set_file_mode(FileDialog::FILE_MODE_OPEN_FILES);
	smpl_file_selection->add_filter("*.sample;WAV Sample files");
	smpl_file_selection->connect("files_selected", callable_mp(this, &GodotREEditor::_res_smpl_2_wav_request));
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
		about_icon->set_stretch_mode(TextureRect::STRETCH_KEEP_ASPECT_CENTERED);
		about_icon->set_texture(icons["RELogoBig"]);

		Label *about_label = memnew(Label);
		about_hbc->add_child(about_label);
		about_label->set_custom_minimum_size(Size2(600, 100) * EDSCALE);
		about_label->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		about_label->set_h_size_flags(Control::SIZE_EXPAND_FILL);
		about_label->set_autowrap_mode(TextServer::AUTOWRAP_WORD_SMART);
		String about_text =
				String("Godot RE Tools, ") + String(GDRE_VERSION) + String(" \n\n") +
				RTR(String("Resources, binary code and source code might be protected by copyright and trademark ") +
						"laws. Before using this software make sure that decompilation is not prohibited by the " +
						"applicable license agreement, permitted under applicable law or you obtained explicit " +
						"permission from the copyright owner.\n\n" +
						"The authors and copyright holders of this software do neither encourage nor condone " +
						"the use of this software, and disclaim any liability for use of the software in violation of " +
						"applicable laws.\n\n" +
						"This software in an alpha stage. Please report any bugs to the GitHub repository\n");
		about_label->set_text(about_text);

#ifdef TOOLS_ENABLED
		if (EditorSettings::get_singleton()) {
			EDITOR_DEF("re/editor/show_info_on_start", true);
		}
#endif
		about_dialog_checkbox = memnew(CheckBox);
		about_vbc->add_child(about_dialog_checkbox);
		about_dialog_checkbox->set_text(RTR("Show this warning when starting the editor"));
		about_dialog_checkbox->connect("toggled", callable_mp(this, &GodotREEditor::_toggle_about_dialog_on_start));
	}

	//Init menu
	if (p_long_menu) {
		p_menu->set_anchor(Side::SIDE_TOP, 0);
		menu_button = memnew(MenuButton);
		menu_button->set_text(RTR("RE Tools"));
		menu_button->set_icon(icons["RELogo"]);
		menu_popup = menu_button->get_popup();
		menu_popup->add_icon_item(icons["RELogo"], RTR("Recover project..."), MENU_EXT_PCK);
		menu_popup->add_separator();
		menu_popup->add_icon_item(icons["RELogo"], RTR("Set encryption key..."), MENU_KEY);
		menu_popup->add_separator();
		menu_popup->add_icon_item(icons["RELogo"], RTR("About Godot RE Tools"), MENU_ABOUT_RE);
		menu_popup->add_icon_item(icons["RELogo"], RTR("Report a bug..."), MENU_REPORT_ISSUE);

		menu_popup->add_icon_item(icons["RELogo"], RTR("Quit"), MENU_EXIT_RE);
		menu_popup->connect("id_pressed", callable_mp(this, &GodotREEditor::menu_option_pressed));
		menu_button->set_anchor(Side::SIDE_TOP, 0);
		p_menu->add_child(menu_button);

		menu_button = memnew(MenuButton);
		menu_button->set_text(RTR("PCK"));
		menu_button->set_icon(icons["REPack"]);
		menu_popup = menu_button->get_popup();
		menu_popup->add_icon_item(icons["REPack"], RTR("Create PCK archive from folder..."), MENU_CREATE_PCK);
		menu_button->set_anchor(Side::SIDE_TOP, 0);
		menu_popup->connect("id_pressed", callable_mp(this, &GodotREEditor::menu_option_pressed));
		p_menu->add_child(menu_button);

		menu_button = memnew(MenuButton);
		menu_button->set_text(RTR("GDScript"));
		menu_button->set_icon(icons["REScript"]);
		menu_popup = menu_button->get_popup();
		menu_popup->add_icon_item(icons["REScript"], RTR("Decompile .GDC/.GDE script files..."), MENU_DECOMP_GDS);
		menu_popup->add_icon_item(icons["REScript"], RTR("Compile .GD script files..."), MENU_COMP_GDS);
		menu_popup->set_item_disabled(menu_popup->get_item_index(MENU_COMP_GDS), true); //TEMP RE-ENABLE WHEN IMPLEMENTED
		menu_button->set_anchor(Side::SIDE_TOP, 0);
		menu_popup->connect("id_pressed", callable_mp(this, &GodotREEditor::menu_option_pressed));
		p_menu->add_child(menu_button);

		menu_button = memnew(MenuButton);
		menu_button->set_text(RTR("Resources"));
		menu_button->set_icon(icons["REResBT"]);
		menu_popup = menu_button->get_popup();
		menu_popup->add_icon_item(icons["REResBT"], RTR("Convert binary resources to text..."), MENU_CONV_TO_TXT);
		menu_popup->add_icon_item(icons["REResTB"], RTR("Convert text resources to binary..."), MENU_CONV_TO_BIN);
		menu_popup->add_separator();
		menu_popup->add_icon_item(icons["REResOther"], RTR("Convert stream textures to PNG..."), MENU_STEX_TO_PNG);
		menu_popup->add_icon_item(icons["REResOther"], RTR("Convert OGG Samples to OGG..."), MENU_OSTR_TO_OGG);
		menu_popup->add_icon_item(icons["REResOther"], RTR("Convert WAV Samples to WAV..."), MENU_SMPL_TO_WAV);
		menu_popup->connect("id_pressed", callable_mp(this, &GodotREEditor::menu_option_pressed));
		menu_button->set_anchor(Side::SIDE_TOP, 0);
		p_menu->add_child(menu_button);
	} else {
		menu_button = memnew(MenuButton);
		menu_button->set_text(RTR("RE Tools"));
		menu_button->set_icon(icons["RELogo"]);
		menu_popup = menu_button->get_popup();

		menu_popup->add_icon_item(icons["RELogo"], RTR("Recover project..."), MENU_EXT_PCK);
		menu_popup->add_separator();
		menu_popup->add_icon_item(icons["RELogo"], RTR("Set encryption key..."), MENU_KEY);
		menu_popup->add_separator();
		menu_popup->add_icon_item(icons["RELogo"], RTR("About Godot RE Tools"), MENU_ABOUT_RE);
		menu_popup->add_icon_item(icons["RELogo"], RTR("Report a bug..."), MENU_REPORT_ISSUE);
		menu_popup->add_separator();

		menu_popup->add_icon_item(icons["REPack"], RTR("Create PCK archive from folder..."), MENU_CREATE_PCK);
		menu_popup->add_separator();

		menu_popup->add_icon_item(icons["REScript"], RTR("Decompile .GDC/.GDE script files..."), MENU_DECOMP_GDS);
		menu_popup->add_icon_item(icons["REScript"], RTR("Compile .GD script files..."), MENU_COMP_GDS);
		menu_popup->set_item_disabled(menu_popup->get_item_index(MENU_COMP_GDS), true); //TEMP RE-ENABLE WHEN IMPLEMENTED
		menu_popup->add_separator();
		menu_popup->add_icon_item(icons["REResBT"], RTR("Convert binary resources to text..."), MENU_CONV_TO_TXT);
		menu_popup->add_icon_item(icons["REResTB"], RTR("Convert text resources to binary..."), MENU_CONV_TO_BIN);
		menu_popup->add_separator();
		menu_popup->add_icon_item(icons["REResOther"], RTR("Convert stream textures to PNG..."), MENU_STEX_TO_PNG);
		menu_popup->add_icon_item(icons["REResOther"], RTR("Convert OGG Samples to OGG..."), MENU_OSTR_TO_OGG);
		menu_popup->add_icon_item(icons["REResOther"], RTR("Convert WAV Samples to WAV..."), MENU_SMPL_TO_WAV);
		menu_popup->connect("id_pressed", callable_mp(this, &GodotREEditor::menu_option_pressed));
		menu_button->set_anchor(Side::SIDE_TOP, 0);
		p_menu->add_child(menu_button);
		if (p_menu->get_child_count() >= 2) {
			menu_button->set_anchor(Side::SIDE_TOP, 0);
			p_menu->move_child(menu_button, p_menu->get_child_count() - 2);
		}
	}
}

GodotREEditor::~GodotREEditor() {
	singleton = NULL;
	memdelete(gdres_singleton);
	if (pdialog_singleton) {
		auto parent = pdialog_singleton->get_parent();
		if (parent) {
			parent->remove_child(pdialog_singleton);
		}
		memdelete(pdialog_singleton);
	}
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
	about_dialog->popup_centered();
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
		case MENU_KEY: {
			key_dialog->popup_centered(Size2(600, 400));
		} break;
		case MENU_CREATE_PCK: {
			pck_source_folder->popup_centered(Size2(600, 400));
		} break;
		case MENU_EXT_PCK: {
			pck_file_selection->popup_centered(Size2(600, 400));
		} break;
		case MENU_DECOMP_GDS: {
			script_dialog_d->popup_centered(Size2(600, 400));
		} break;
		case MENU_COMP_GDS: {
			script_dialog_c->popup_centered(Size2(600, 400));
		} break;
		case MENU_CONV_TO_TXT: {
			bin_res_file_selection->popup_centered(Size2(600, 400));
		} break;
		case MENU_CONV_TO_BIN: {
			txt_res_file_selection->popup_centered(Size2(600, 400));
		} break;
		case MENU_STEX_TO_PNG: {
			stex_file_selection->popup_centered(Size2(600, 400));
		} break;
		case MENU_OSTR_TO_OGG: {
			ostr_file_selection->popup_centered(Size2(600, 400));
		} break;
		case MENU_SMPL_TO_WAV: {
			smpl_file_selection->popup_centered(Size2(600, 400));
		} break;
		case MENU_ABOUT_RE: {
			show_about_dialog();
		} break;
		case MENU_REPORT_ISSUE: {
			OS::get_singleton()->shell_open("https://github.com/bruvzg/gdsdecomp/issues/new?assignees=&labels=bug&template=bug_report.yml&sys_info=" + GDRESettings::get_singleton()->get_sys_info_string());
		} break;
		case MENU_EXIT_RE: {
			get_tree()->quit();
		} break;
		default:
			ERR_FAIL();
	}
}

#if defined(MINGW_ENABLED) || defined(_MSC_VER)
#define sprintf sprintf_s
#endif
// TODO: More robust logging
void GodotREEditor::print_log(const String &p_text) {
	Vector<String> lines = p_text.split("\n");
	for (int i = 0; i < lines.size(); i++) {
		if (!lines[i].is_empty()) {
			emit_signal("write_log_message", lines[i] + "\n");
		}
	}
}

void GodotREEditor::print_warning(const String &p_text, const String &p_title, const String &p_sub_text) {
	char timestamp[21];
	OS::DateTime date = OS::get_singleton()->get_datetime();
	sprintf(timestamp, "-%04d-%02d-%02d-%02d-%02d-%02d", (uint16_t)date.year, date.month, date.day, date.hour, date.minute, date.second);

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

void GodotREEditor::show_report(const String &p_text, const String &p_title, const String &p_sub_text) {
	rdl->set_title(p_title);
	rdl->set_message(p_text, p_sub_text);
	Size2i size = rdl->get_parent_rect().get_size();
	size.height -= (size.height / 3);
	size.width -= (size.width / 2);
	rdl->set_wrap_controls(true);
	rdl->popup_centered(size);
}
/*************************************************************************/
/* Decompile                                                             */
/*************************************************************************/

void GodotREEditor::_decompile_files() {
	Vector<String> files = script_dialog_d->get_file_list();
	String dir = script_dialog_d->get_target_dir();

	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	String overwrite_list = String();
	for (int i = 0; i < files.size(); i++) {
		String target_name = dir.path_join(files[i].get_file().get_basename() + ".gd");
		if (da->file_exists(target_name)) {
			overwrite_list += target_name + "\n";
		}
	}

	if (overwrite_list.length() == 0) {
		_decompile_process();
	} else {
		ovd->set_message(overwrite_list);
		ovd->connect("confirmed", callable_mp(this, &GodotREEditor::_decompile_process).bind(Vector<Variant>()), CONNECT_ONE_SHOT);
		ovd->popup_centered();
	}
}

void GodotREEditor::_decompile_process() {
	Vector<String> files = script_dialog_d->get_file_list();
	Vector<uint8_t> key = key_dialog->get_key();
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

		String target_name = dir.path_join(files[i].get_file().get_basename() + ".gd");

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
			Ref<FileAccess> file = FileAccess::open(target_name, FileAccess::WRITE, &err);
			if (err) {
				failed_files += files[i] + " (FileAccess error)\n";
			}
			file->store_string(scr);
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
	Vector<uint8_t> key = key_dialog->get_key();
	String dir = script_dialog_c->get_target_dir();
	String ext = (key.size() == 32) ? ".gde" : ".gds";

	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	String overwrite_list = String();
	for (int i = 0; i < files.size(); i++) {
		String target_name = dir.path_join(files[i].get_file().get_basename() + ext);
		if (da->file_exists(target_name)) {
			overwrite_list += target_name + "\n";
		}
	}

	if (overwrite_list.length() == 0) {
		_compile_process();
	} else {
		ovd->set_message(overwrite_list);
		ovd->connect("confirmed", callable_mp(this, &GodotREEditor::_compile_process).bind(Vector<Variant>()), CONNECT_ONE_SHOT);
		ovd->popup_centered();
	}
}

void GodotREEditor::_compile_process() {
	/*
	Vector<String> files = script_dialog_c->get_file_list();
	Vector<uint8_t> key = key_dialog->get_key(); = script_dialog_c->get_key();
	String dir = script_dialog_c->get_target_dir();
	String ext = (key.size() == 32) ? ".gde" : ".gdc";

	String failed_files;

	EditorProgressGDDC *pr = memnew(EditorProgressGDDC(ne_parent, "re_compile", RTR("Compiling files..."), files.size(), true));

	for (int i = 0; i < files.size(); i++) {

		print_warning(RTR("compiling") + " " + files[i].get_file(), RTR("Compile"));
		String target_name = dir.path_join(files[i].get_file().get_basename() + ext);

		bool cancel = pr->step(files[i].get_file(), i, true);
		if (cancel) {
			break;
		}

		Vector<uint8_t> file = FileAccess::get_file_as_bytes(files[i]);
		if (file.size() > 0) {
			String txt;
			txt.parse_utf8((const char *)file.ptr(), file.size());
			file = GDScriptTokenizerBuffer::parse_code_string(txt);

			Ref<FileAccess> fa = FileAccess::open(target_name, FileAccess::WRITE);
			if (fa) {
				if (key.size() == 32) {
					Ref<FileAccessEncrypted> fae;
					fae.instantiate();
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
*/
}

/*************************************************************************/
/* PCK explorer                                                          */
/*************************************************************************/

void GodotREEditor::_pck_select_request(const String &p_path) {
	pck_dialog->clear();
	pck_files.clear();
	pck_file = p_path;
	if (key_dialog->get_key().size() > 0) {
		GDRESettings::get_singleton()->set_encryption_key(key_dialog->get_key());
	}

	Error err = GDRESettings::get_singleton()->load_pack(p_path);
	if (err) {
		if (err == ERR_PRINTER_ON_FIRE) {
			show_warning(RTR("Error opening encrypted PCK file: ") + p_path + "\nSet correct encryption key and try again.", RTR("Read PCK"));
		} else {
			show_warning(RTR("Error opening PCK file: ") + p_path, RTR("Read PCK"));
		}
		return;
	}
	pck_file_selection->set_visible(false);
	EditorProgressGDDC *pr = memnew(EditorProgressGDDC(ne_parent, "re_read_pck_md5",
			RTR("Reading PCK archive, click cancel to skip MD5 checking..."),
			GDRESettings::get_singleton()->get_file_count(), true));

	Ref<PckDumper> pckdump;
	pckdump.instantiate();
	Vector<String> broken_files;
	int files_checked = 0;
	int files_broken = 0;

	err = pckdump->_check_md5_all_files(broken_files, files_checked, pr);
	memdelete(pr);
	// canceled
	bool p_check_md5 = true;
	pck_dialog->set_version(GDRESettings::get_singleton()->get_version_string());
	pck_dialog->set_info(String("    ") + RTR("Total files: ") +
			itos(GDRESettings::get_singleton()->get_file_count()) + "; " + RTR("Checked: ") +
			itos(files_checked) + "; " + RTR("Broken: ") + itos(broken_files.size()));
	print_line(RTR("Total files: ") + itos(GDRESettings::get_singleton()->get_file_count()) +
					"; " + RTR("Checked: ") + itos(files_checked) + "; " + RTR("Broken: ") + itos(files_broken),
			RTR("Read PCK"));
	if (err == ERR_PRINTER_ON_FIRE || err == ERR_SKIP) {
		//do things
		p_check_md5 = false;
	}
	auto files = GDRESettings::get_singleton()->get_file_info_list();
	for (auto file : files) {
		Ref<Texture2D> icon;
		String error_string = "";
		bool md5_error = !file->is_checksum_validated();
		bool is_malformed = file->is_malformed();
		if (p_check_md5 && md5_error) {
			icon = icons["REFileBroken"];
			error_string += "MD5 mismatch";
		} else if (file->is_malformed()) {
			icon = icons["REFileBroken"];
			error_string += String(error_string.length() > 0 ? ", " : "") + "Malformed_path";
		} else if (!p_check_md5) {
			icon = icons["REFile"];
		} else {
			icon = icons["REFileOk"];
		}
		pck_dialog->add_file(file->get_path(), file->get_size(), icon, error_string, file->is_malformed(), file->is_encrypted());
	}
	pck_dialog->popup_centered(Size2(600, 400));
}

void GodotREEditor::_pck_unload() {
	GDRESettings::get_singleton()->unload_pack();
}

void GodotREEditor::_pck_extract_files() {
	Vector<String> files = pck_dialog->get_selected_files();
	String dir = pck_dialog->get_target_dir();
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	String overwrite_list = String();
	for (int i = 0; i < files.size(); i++) {
		String target_name = dir.path_join(files[i].get_file());
		if (da->file_exists(target_name)) {
			overwrite_list += target_name + "\n";
		}
	}

	if (overwrite_list.length() == 0) {
		_pck_extract_files_process();
	} else {
		ovd->set_message(overwrite_list);
		ovd->connect("confirmed", callable_mp(this, &GodotREEditor::_pck_extract_files_process).bind(), CONNECT_ONE_SHOT);
		ovd->connect("canceled", callable_mp(this, &GodotREEditor::_pck_unload).bind(), CONNECT_ONE_SHOT);

		ovd->popup_centered();
	}
}

void GodotREEditor::_pck_extract_files_process() {
	Vector<String> files = pck_dialog->get_selected_files();
	String dir = pck_dialog->get_target_dir();
	bool is_full_recovery = pck_dialog->get_is_full_recovery();
	GDRESettings::get_singleton()->open_log_file(dir);
	String failed_files;
	pck_dialog->set_visible(false);
	ovd->set_visible(false);
	EditorProgressGDDC *pr = memnew(EditorProgressGDDC(ne_parent, "re_ext_pck", RTR("Extracting files..."), files.size(), true));
	Ref<PckDumper> pckdumper;
	pckdumper.instantiate();
	Error err = pckdumper->_pck_dump_to_dir(dir, files, pr, failed_files);
	Ref<ImportExporter> ie;

	if (is_full_recovery && !err) {
		memdelete(pr);
		ie.instantiate();
		pr = memnew(EditorProgressGDDC(ne_parent, "re_ext_pck_res", RTR("Exporting resources..."), GDRESettings::get_singleton()->get_import_files().size(), true));
		String error_string;
		err = ie->_export_imports(dir, files, pr, error_string);
	}
	memdelete(pr);
	pck_file = String();
	String log_path = GDRESettings::get_singleton()->get_log_file_path();
	String report = "Log file written to " + log_path;
	report += "\nPlease include this file when reporting an issue!\n\n";
	if (is_full_recovery) {
		report += ie->get_editor_message();
		String notes = ie->get_session_notes();
		if (!notes.is_empty()) {
			report += "\n------------IMPORTANT NOTES-----------\n";
			report += notes;
		}
		report += "\n************EXPORT REPORT************\n";

		report += ie->get_report();
	}

	_pck_unload();
	GDRESettings::get_singleton()->close_log_file();

	if (failed_files.length() > 0) {
		show_warning(report + failed_files, RTR("Read PCK"), RTR("At least one error was detected!") + (is_full_recovery ? "\nSkipping full recovery!" : ""));
	} else if (is_full_recovery && !err) {
		show_report(report, RTR("Recovery report"), RTR("Recovery report"));
	} else if (!is_full_recovery && !err) {
		show_report(RTR("No errors detected.") + String("\n") + report, RTR("Extract report"), RTR("Extract report"));
	} else if (err) {
		show_warning("Resource export failed." + String("\n") + report, RTR("Read PCK"), RTR("At least one error was detected!") + (is_full_recovery ? "\nSkipping full recovery!" : ""));
	}
	print_warning("Log file written to " + log_path, RTR("Read PCK"));
	print_warning("Please include this file when reporting an issue!", RTR("Read PCK"));
}

/*************************************************************************/
/* Res convert                                                           */
/*************************************************************************/

void GodotREEditor::_res_smpl_2_wav_request(const Vector<String> &p_files) {
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
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
		ovd->connect("confirmed", callable_mp(this, &GodotREEditor::_res_smpl_2_wav_process).bind(Vector<Variant>()), CONNECT_ONE_SHOT);
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

		Ref<AudioStreamWAV> sample = rl->load(res_files[i]);
		if (sample.is_null()) {
			failed_files += res_files[i] + " (load AudioStreamWAV error)\n";
			continue;
		}

		sample->save_to_wav(res_files[i].get_basename() + ".wav");
	}

	memdelete(pr);
	res_files = Vector<String>();

	if (failed_files.length() > 0) {
		show_warning(failed_files, RTR("Convert WAV samples"), RTR("At least one error was detected!"));
	} else {
		show_warning(RTR("No errors detected."), RTR("Convert WAV samples"), RTR("The operation completed successfully!"));
	}
}

void GodotREEditor::_res_ostr_2_ogg_request(const Vector<String> &p_files) {
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
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
		ovd->connect("confirmed", callable_mp(this, &GodotREEditor::_res_ostr_2_ogg_process).bind(Vector<Variant>()), CONNECT_ONE_SHOT);
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
		Error err;
		OggStreamLoaderCompat oslc;
		Vector<uint8_t> buf = oslc.get_ogg_stream_data(res_files[i], &err);
		if (err) {
			failed_files += res_files[i] + " (load AudioStreamOggVorbis error)\n";
			continue;
		}

		Ref<FileAccess> res = FileAccess::open(res_files[i].get_basename() + ".ogg", FileAccess::WRITE);
		if (res.is_null()) {
			failed_files += res_files[i] + " (write error)\n";
			continue;
		}
		res->store_buffer(buf.ptr(), buf.size());
	}

	memdelete(pr);
	res_files = Vector<String>();

	if (failed_files.length() > 0) {
		show_warning(failed_files, RTR("Convert OGG samples"), RTR("At least one error was detected!"));
	} else {
		show_warning(RTR("No errors detected."), RTR("Convert OGG samples"), RTR("The operation completed successfully!"));
	}
}

void GodotREEditor::_res_stex_2_png_request(const Vector<String> &p_files) {
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
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
		ovd->connect("confirmed", callable_mp(this, &GodotREEditor::_res_stxt_2_png_process).bind(Vector<Variant>()), CONNECT_ONE_SHOT);
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

		Error err;
		TextureLoaderCompat tlc;
		Ref<Image> img = tlc.load_image_from_tex(res_files[i], &err);
		if (err != OK) {
			failed_files += res_files[i] + " (load StreamTexture error)\n";
			continue;
		}
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
	res_files = Vector<String>();

	if (failed_files.length() > 0) {
		show_warning(failed_files, RTR("Convert textures"), RTR("At least one error was detected!"));
	} else {
		show_warning(RTR("No errors detected."), RTR("Convert textures"), RTR("The operation completed successfully!"));
	}
}

void GodotREEditor::_res_bin_2_txt_request(const Vector<String> &p_files) {
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	String overwrite_list = String();
	for (int i = 0; i < p_files.size(); i++) {
		String ext = p_files[i].get_extension().to_lower();
		String new_ext = ".txt";
		if (ext == "scn") {
			if (p_files[i].to_lower().find(".escn.") != -1) {
				new_ext = ".escn";
			} else {
				new_ext = ".tscn";
			}
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
		ovd->connect("confirmed", callable_mp(this, &GodotREEditor::_res_bin_2_txt_process).bind(Vector<Variant>()), CONNECT_ONE_SHOT);
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
			if (res_files[i].to_lower().find(".escn") != -1) {
				new_ext = ".escn";
			} else {
				new_ext = ".tscn";
			}
		} else if (ext == "res") {
			new_ext = ".tres";
		}

		Error err = convert_file_to_text(res_files[i], res_files[i].get_basename() + new_ext);
		if (err != OK) {
			failed_files += res_files[i] + " (ResourceFormatLoaderText error)\n";
		}
	}

	memdelete(pr);
	res_files = Vector<String>();

	if (failed_files.length() > 0) {
		show_warning(failed_files, RTR("Convert resources"), RTR("At least one error was detected!"));
	} else {
		show_warning(RTR("No errors detected."), RTR("Convert resources"), RTR("The operation completed successfully!"));
	}
}

Error GodotREEditor::convert_file_to_text(const String &p_src_path, const String &p_dst_path) {
	ResourceFormatLoaderCompat rl;
	Error err = rl.convert_bin_to_txt(p_src_path, p_dst_path);
	return err;
}

void GodotREEditor::_res_txt_2_bin_request(const Vector<String> &p_files) {
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	String overwrite_list = String();
	for (int i = 0; i < p_files.size(); i++) {
		String ext = p_files[i].get_extension().to_lower();
		String new_ext = ".bin";
		if (ext == "tscn" || ext == "escn") {
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
		ovd->connect("confirmed", callable_mp(this, &GodotREEditor::_res_txt_2_bin_process).bind(Vector<Variant>()), CONNECT_ONE_SHOT);
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
		if (ext == "tscn" || ext == "escn") {
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
	res_files = Vector<String>();

	if (failed_files.length() > 0) {
		show_warning(failed_files, RTR("Convert resources"), RTR("At least one error was detected!"));
	} else {
		show_warning(RTR("No errors detected."), RTR("Convert resources"), RTR("The operation completed successfully!"));
	}
}

Error GodotREEditor::convert_file_to_binary(const String &p_src_path, const String &p_dst_path) {
	ResourceFormatLoaderCompat rl;
	Error err = rl.convert_txt_to_bin(p_src_path, p_dst_path);
	return err;
}

/*************************************************************************/
/* PCK create                                                            */
/*************************************************************************/

void GodotREEditor::_pck_create_request(const String &p_path) {
	pck_save_files.clear();
	pck_file = p_path;

	pck_save_dialog->popup_centered(Size2(600, 400));
}

void GodotREEditor::_pck_save_prep() {
	pck_save_file_selection->clear_filters();
	if (pck_save_dialog->get_is_emb()) {
		pck_save_file_selection->add_filter("*.exe,*.bin,*.32,*.64,*.x86_64,*.x86,*.arm64,*.universal;Self contained executable files");
	} else {
		pck_save_file_selection->add_filter("*.pck;PCK archive files");
	}
	pck_save_file_selection->popup_centered(Size2(600, 400));
}

#define PCK_PADDING 16

uint64_t GodotREEditor::_pck_create_process_folder(EditorProgressGDDC *p_pr, const String &p_path, const String &p_rel, uint64_t p_offset, bool &p_cancel) {
	uint64_t offset = p_offset;

	Vector<String> enc_in_filters = pck_save_dialog->get_enc_filters_in().split(",");
	Vector<String> enc_ex_filters = pck_save_dialog->get_enc_filters_ex().split(",");

	Ref<DirAccess> da = DirAccess::open(p_path.path_join(p_rel));
	if (da.is_null()) {
		show_warning(RTR("Error opening folder: ") + p_path.path_join(p_rel), RTR("New PCK"));
		return offset;
	}
	da->list_dir_begin();
	String f = da->get_next();
	while (!f.is_empty()) {
		if (f == "." || f == "..") {
			f = da->get_next();
			continue;
		}
		if (p_pr->step(p_rel.path_join(f), 0, true)) {
			p_cancel = true;
			return offset;
		}
		if (da->current_is_dir()) {
			offset = _pck_create_process_folder(p_pr, p_path, p_rel.path_join(f), offset, p_cancel);
			if (p_cancel) {
				return offset;
			}

		} else {
			String path = p_path.path_join(p_rel).path_join(f);
			Ref<FileAccess> file = FileAccess::open(path, FileAccess::READ);

			CryptoCore::MD5Context ctx;
			ctx.start();

			int64_t rq_size = file->get_length();
			uint8_t buf[32768];

			while (rq_size > 0) {
				int got = file->get_buffer(buf, MIN(32768, rq_size));
				if (got > 0) {
					ctx.update(buf, got);
				}
				if (got < 4096)
					break;
				rq_size -= 32768;
			}

			unsigned char hash[16];
			ctx.finish(hash);

			int flags = 0;

			for (int i = 0; i < enc_in_filters.size(); ++i) {
				if (path.matchn(enc_in_filters[i]) || path.replace("res://", "").matchn(enc_in_filters[i])) {
					flags = (1 << 0);
					break;
				}
			}

			for (int i = 0; i < enc_ex_filters.size(); ++i) {
				if (path.matchn(enc_ex_filters[i]) || path.replace("res://", "").matchn(enc_ex_filters[i])) {
					flags = (1 << 0);
					break;
				}
			}

			Ref<PackedFileInfo> finfo;
			finfo.instantiate();

			String name = p_rel.path_join(f);
			finfo->init(p_path, name, p_offset, file->get_length(), hash, nullptr, flags == (1 << 0));

			pck_save_files.push_back(finfo);

			offset += file->get_length();
		}
		f = da->get_next();
	}
	da->list_dir_end();

	return offset;
}

void GodotREEditor::_pck_save_request(const String &p_path) {
	EditorProgressGDDC *pr = memnew(EditorProgressGDDC(ne_parent, "re_read_folder", RTR("Reading folder structure..."), 1, true));
	bool cancel = false;
	uint64_t size = _pck_create_process_folder(pr, pck_file, String(), 0, cancel);
	memdelete(pr);

	if (cancel) {
		return;
	}

	if (size == 0) {
		show_warning(RTR("Error opening folder (or empty folder): ") + pck_file, RTR("New PCK"));
		return;
	}

	int64_t embedded_start = 0;
	int64_t embedded_size = 0;

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::WRITE);
	if (f.is_null()) {
		show_warning(RTR("Error opening PCK file: ") + p_path, RTR("New PCK"));
		return;
	}

	pr = memnew(EditorProgressGDDC(ne_parent, "re_write_pck", RTR("Writing PCK archive..."), pck_save_files.size() + 2, true));

	if (pck_save_dialog->get_is_emb()) {
		// append to exe
		Ref<FileAccess> fs = FileAccess::open(pck_save_dialog->get_emb_source(), FileAccess::READ);
		if (fs.is_null()) {
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

		embedded_start = f->get_position();

		// ensure embedded PCK starts at a 64-bit multiple
		int pad = f->get_position() % 8;
		for (int i = 0; i < pad; i++) {
			f->store_8(0);
		}
	}
	int64_t pck_start_pos = f->get_position();

	int version = pck_save_dialog->get_version_pack();
	Vector<uint8_t> key = key_dialog->get_key();

	f->store_32(0x43504447); //GDPK
	f->store_32(version);
	f->store_32(pck_save_dialog->get_version_major());
	f->store_32(pck_save_dialog->get_version_minor());
	f->store_32(pck_save_dialog->get_version_rev());

	int64_t file_base_ofs = 0;
	if (version == 2) {
		uint32_t pack_flags = 0;
		if (pck_save_dialog->get_enc_dir()) {
			pack_flags |= (1 << 0);
		}
		f->store_32(pack_flags); // flags
		file_base_ofs = f->get_position();
		f->store_64(0); // files base
	}

	for (int i = 0; i < 16; i++) {
		//reserved
		f->store_32(0);
	}

	pr->step("Header...", 0, true);

	f->store_32(pck_save_files.size()); //amount of files

	size_t header_size = f->get_position();

	Ref<FileAccessEncrypted> fae;
	Ref<FileAccess> fhead = f;
	if (version == 2) {
		if (pck_save_dialog->get_enc_dir()) {
			fae.instantiate();
			ERR_FAIL_COND(fae.is_null());

			Error err = fae->open_and_parse(f, key, FileAccessEncrypted::MODE_WRITE_AES256, false);
			ERR_FAIL_COND(err != OK);

			fhead = fae;
		}
	}

	for (int i = 0; i < pck_save_files.size(); i++) {
		header_size += 4; // size of path string (32 bits is enough)
		uint32_t string_len = pck_save_files[i]->get_path().utf8().length() + 6;
		header_size += string_len + _get_pad(4, string_len); ///size of path string
		header_size += 8; // offset to file _with_ header size included
		header_size += 8; // size of file
		header_size += 16; // md5
	}

	size_t header_padding = _get_pad(PCK_PADDING, header_size);

	pr->step("Directory...", 0, true);

	for (int i = 0; i < pck_save_files.size(); i++) {
		uint32_t string_len = pck_save_files[i]->get_path().utf8().length() + 6;
		uint32_t pad = _get_pad(4, string_len);

		fhead->store_32(string_len + pad);
		String name = "res://" + pck_save_files[i]->get_path();
		fhead->store_buffer((const uint8_t *)name.utf8().get_data(), string_len);
		for (uint32_t j = 0; j < pad; j++) {
			fhead->store_8(0);
		}

		if (version == 2) {
			fhead->store_64(pck_save_files[i]->get_offset());
		} else {
			fhead->store_64(pck_save_files[i]->get_offset() + header_padding + header_size);
		}
		fhead->store_64(pck_save_files[i]->get_size()); // pay attention here, this is where file is
		fhead->store_buffer(pck_save_files[i]->get_md5().ptr(), 16); //also save md5 for file
		if (version == 2) {
			if (pck_save_files[i]->is_encrypted()) {
				fhead->store_32(1);
			} else {
				fhead->store_32(0);
			}
		}
	}

	if (fae.is_valid()) {
		fae.unref();
	}

	for (uint32_t j = 0; j < header_padding; j++) {
		if (version == 2) {
			f->store_8(Math::rand() % 256);
		} else {
			f->store_8(0);
		}
	}

	if (version == 2) {
		int64_t file_base = f->get_position();
		f->seek(file_base_ofs);
		f->store_64(file_base); // update files base
		f->seek(file_base);
	}

	String failed_files;

	for (int i = 0; i < pck_save_files.size(); i++) {
		print_warning("saving " + pck_save_files[i]->get_path(), RTR("New PCK"));
		if (pr->step(pck_save_files[i]->get_path(), i + 2, true)) {
			break;
		}

		fae = Ref<FileAccess>();
		Ref<FileAccess> ftmp = f;
		if (pck_save_files[i]->is_encrypted()) {
			fae.instantiate();
			ERR_FAIL_COND(fae.is_null());

			Error err = fae->open_and_parse(f, key, FileAccessEncrypted::MODE_WRITE_AES256, false);
			ERR_FAIL_COND(err != OK);
			ftmp = fae;
		}

		Ref<FileAccess> fa = FileAccess::open(pck_file.path_join(pck_save_files[i]->get_path()), FileAccess::READ);
		if (fa.is_valid()) {
			int64_t rq_size = pck_save_files[i]->get_size();
			uint8_t buf[16384];

			while (rq_size > 0) {
				int got = fa->get_buffer(buf, MIN(16384, rq_size));
				ftmp->store_buffer(buf, got);
				rq_size -= 16384;
			}
		} else {
			failed_files += pck_save_files[i]->get_path() + " (FileAccess error)\n";
		}

		if (fae.is_valid()) {
			fae.unref();
		}
	}

	if (pck_save_dialog->get_watermark() != "") {
		f->store_32(0);
		f->store_32(0);
		f->store_string(pck_save_dialog->get_watermark());
		f->store_32(0);
		f->store_32(0);
	}

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
					memdelete(pr);
					return;
				}
				f->get_buffer(strings, string_data_size);
			}

			// Search for the "pck" section
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

					break;
				}
			}
			memfree(strings);
		}
	}

	pck_save_files.clear();
	pck_file = String();
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
	if (GodotREEditor::get_singleton() == nullptr) {
		editor_ctx = memnew(GodotREEditor(this, menu_hb));
		editor_ctx->connect("write_log_message", callable_mp(this, &GodotREEditorStandalone::_write_log_message));
		add_child(editor_ctx);
	} else {
		editor_ctx = GodotREEditor::get_singleton();
		editor_ctx->connect("write_log_message", callable_mp(this, &GodotREEditorStandalone::_write_log_message));
	}
}

GodotREEditorStandalone::~GodotREEditorStandalone() {
	editor_ctx = NULL;
}
