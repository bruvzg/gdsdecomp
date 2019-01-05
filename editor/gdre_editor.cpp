/*************************************************************************/
/*  gdre_editor.cpp                                                      */
/*************************************************************************/

#include "gdre_editor.h"

//#include "modules/svg/image_loader_svg.h"
//#include "icons/icons.gen.h"

GodotREEditor *GodotREEditor::singleton = NULL;

GodotREEditor::GodotREEditor(EditorNode *p_editor) {

	singleton = this;

	editor = p_editor;

	//Init editor icons
	/*
	for (int i = 0; i < gdre_icons_count; i++) {
		Ref<ImageTexture> icon = memnew(ImageTexture);
		Ref<Image> img = memnew(Image);

		ImageLoaderSVG::create_image_from_string(img, gdre_icons_sources[i], EDSCALE, true, false);
		icon->create_from_image(img);
		if (String(gdre_icons_names[i]).begins_with("GDRE")) {
			type_icons[gdre_icons_names[i]] = icon;
		} else {
			gui_icons[gdre_icons_names[i]] = icon;
		}
	}
	*/

	//Init menu
	menu_button = memnew(MenuButton);
	menu_button->set_text(TTR("RE Tools"));
	menu_popup = menu_button->get_popup();

	menu_popup->add_item(TTR("About Godot RE Tools"), MENU_ABOUT_RE);

	//Init about/warning dialog
	{
		about_dialog = memnew(AcceptDialog);
		editor->get_gui_base()->add_child(about_dialog);
		about_dialog->set_title("Important: Legal Notice");

		VBoxContainer *about_vbc = memnew(VBoxContainer);
		about_dialog->add_child(about_vbc);

		HBoxContainer *about_hbc = memnew(HBoxContainer);
		about_vbc->add_child(about_hbc);

		TextureRect *about_icon = memnew(TextureRect);
		about_hbc->add_child(about_icon);
		Ref<Texture> about_icon_tex = about_icon->get_icon("NodeWarning", "EditorIcons");
		about_icon->set_texture(about_icon_tex);

		Label *about_label = memnew(Label);
		about_hbc->add_child(about_label);
		about_label->set_custom_minimum_size(Size2(600, 150) * EDSCALE);
		about_label->set_v_size_flags(Control::SIZE_EXPAND_FILL);
		about_label->set_autowrap(true);
		//TODO add about info!
		String about_text =
				String("Resources, binary code and source code might be protected by copyright and trademark\n") +
					"laws. Before using this software make sure that decompilation is not prohibited by the\n" +
					"applicable license agreement, permitted under applicable law or you obtained explicit\n" +
					"permission from the copyright owner.\n\n" +
					"The authors and copyright holders of this software do neither encourage nor condone\n" +
					"the use of this software, and disclaim any liability for use of the software in violation of\n" +
					"applicable laws.\n\n" +
					"This software in a pre-alpha stage and is not suitable for use in production.\n\n";
		about_label->set_text(about_text);

		EDITOR_DEF("re/editor/show_info_on_start", true);

		about_dialog_checkbox = memnew(CheckBox);
		about_vbc->add_child(about_dialog_checkbox);
		about_dialog_checkbox->set_text("Show this warning when starting the editor");
		about_dialog_checkbox->connect("toggled", this, "_toggle_about_dialog_on_start");
	}

	menu_popup->add_separator(); //1

	menu_popup->add_item(TTR("Create PCK archive"), MENU_CREATE_PCK); //2
	menu_popup->set_item_disabled(2, true); //TODO remove when implemented
	menu_popup->add_item(TTR("Test integrity of PCK archive"), MENU_TEST_PCK); //3
	menu_popup->set_item_disabled(3, true); //TODO remove when implemented
	menu_popup->add_item(TTR("Extract files from PCK archive"), MENU_EXT_PCK); //4
	menu_popup->set_item_disabled(4, true); //TODO remove when implemented

	menu_popup->add_separator(); //5

	menu_popup->add_item(TTR("Decompile GDC/GDE binary script"), MENU_DECOMP_GDS); //6
	menu_popup->set_item_disabled(6, true); //TODO remove when implemented
	menu_popup->add_item(TTR("Compile GDC/GDE binary script"), MENU_COMP_GDS); //7
	menu_popup->set_item_disabled(7, true); //TODO remove when implemented

	menu_popup->add_separator(); //8

	menu_popup->add_item(TTR("Convert binary resource to text"), MENU_CONV_TO_TXT); //9
	menu_popup->set_item_disabled(9, true); //TODO remove when implemented
	menu_popup->add_item(TTR("Convert text resource to binary"), MENU_CONV_TO_BIN); //10
	menu_popup->set_item_disabled(10, true); //TODO remove when implemented

	menu_popup->connect("id_pressed", this, "_menu_option_pressed");

	editor->get_menu_hb()->add_child(menu_button);
	if (editor->get_menu_hb()->get_child_count() >= 2) {
		editor->get_menu_hb()->move_child(menu_button, editor->get_menu_hb()->get_child_count() - 2);
	}
}

GodotREEditor::~GodotREEditor() {

	singleton = NULL;
}

void GodotREEditor::_show_about_dialog() {

	bool show_on_start = EDITOR_GET("re/editor/show_info_on_start");
	about_dialog_checkbox->set_pressed(show_on_start);
	about_dialog->popup_centered_minsize();
}

void GodotREEditor::_toggle_about_dialog_on_start(bool p_enabled) {

	bool show_on_start = EDITOR_GET("re/editor/show_info_on_start");
	if (show_on_start != p_enabled) {
		EditorSettings::get_singleton()->set_setting("re/editor/show_info_on_start", p_enabled);
	}
}

void GodotREEditor::_menu_option_pressed(int p_id) {

	switch (p_id) {
		case MENU_CREATE_PCK: {
			//TODO
		} break;
		case MENU_TEST_PCK: {
			//TODO
		} break;
		case MENU_EXT_PCK: {
			//TODO
		} break;
		case MENU_DECOMP_GDS: {
			//TODO
		} break;
		case MENU_COMP_GDS: {
			//TODO
		} break;
		case MENU_CONV_TO_TXT: {
			//TODO
		} break;
		case MENU_CONV_TO_BIN: {
			//TODO
		} break;
		case MENU_ABOUT_RE: {

			_show_about_dialog();
		} break;
		default:
			ERR_FAIL();
	}
}

void GodotREEditor::_notification(int p_notification) {

	switch (p_notification) {

		case NOTIFICATION_READY: {

			bool show_info_dialog = EDITOR_GET("re/editor/show_info_on_start");
			if (show_info_dialog) {
				about_dialog->set_exclusive(true);
				_show_about_dialog();

				about_dialog->set_exclusive(false);
			}
		}
		case EditorSettings::NOTIFICATION_EDITOR_SETTINGS_CHANGED: {

			//Register node type icons
			//for (Map<String, Ref<ImageTexture> >::Element *E = type_icons.front(); E; E = E->next()) {
			//	editor->get_theme_base()->get_theme()->set_icon(E->key(), "EditorIcons", E->get());
			//}
		}
	}
}

void GodotREEditor::_bind_methods() {

	ClassDB::bind_method(D_METHOD("_menu_option_pressed", "id"), &GodotREEditor::_menu_option_pressed);
	ClassDB::bind_method(D_METHOD("_toggle_about_dialog_on_start"), &GodotREEditor::_toggle_about_dialog_on_start);
};
