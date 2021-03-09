/*************************************************************************/
/*  gdre_dec_dlg.cpp                                                     */
/*************************************************************************/

#include "gdre_dec_rec_dlg.h"
#include "bytecode/bytecode_versions.h"

ScriptDecompRecursiveDialog::ScriptDecompRecursiveDialog() {

	set_title(RTR("Decompile GDScript"));
	set_flag(Window::Flags::FLAG_RESIZE_DISABLED, false);

	source_folder_selection = memnew(FileDialog);
	source_folder_selection->set_access(FileDialog::ACCESS_FILESYSTEM);
	source_folder_selection->set_file_mode(FileDialog::FILE_MODE_OPEN_DIR);
	source_folder_selection->connect("dir_selected", callable_mp(this, &ScriptDecompRecursiveDialog::_dir_select_request));
	source_folder_selection->set_show_hidden_files(true);
	add_child(source_folder_selection);

	VBoxContainer *script_vb = memnew(VBoxContainer);
	//Source directory
	HBoxContainer *dir_hbc = memnew(HBoxContainer);
	source_dir = memnew(LineEdit);
	source_dir->set_editable(false);
	source_dir->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	dir_hbc->add_child(source_dir);

	select_dir = memnew(Button);
	select_dir->set_text("...");
	select_dir->connect("pressed", callable_mp(this, &ScriptDecompRecursiveDialog::_dir_select_pressed));
	dir_hbc->add_child(select_dir);

	script_vb->add_margin_child(RTR("Source folder:"), dir_hbc);

	script_key_error = memnew(Label);
	script_vb->add_child(script_key_error);

	//Script version
	scrver = memnew(OptionButton);

	for (int i = 0; decomp_versions[i].commit != 0; i++) {
		scrver->add_item(decomp_versions[i].name, decomp_versions[i].commit);
	}

	script_vb->add_margin_child(RTR("Script bytecode version:"), scrver);
	scrver->connect("item_selected", callable_mp(this, &ScriptDecompRecursiveDialog::_bytcode_changed));



	add_child(script_vb);

	get_ok_button()->set_text(RTR("Decompile"));
	_validate_input();

	add_cancel_button(RTR("Cancel"));
}

ScriptDecompRecursiveDialog::~ScriptDecompRecursiveDialog() {
	//NOP
}

int ScriptDecompRecursiveDialog::get_bytecode_version() const {

	return scrver->get_selected_id();
}

String ScriptDecompRecursiveDialog::get_source_dir() const {

	return source_dir->get_text();
}

void ScriptDecompRecursiveDialog::_bytcode_changed(int p_id) {

	_validate_input();
}

void ScriptDecompRecursiveDialog::_validate_input() {
	
	bool ok = true;
	String error_message;

#ifdef TOOLS_ENABLED
	Color error_color = (EditorNode::get_singleton()) ? EditorNode::get_singleton()->get_gui_base()->get_theme_color("error_color", "Editor") : Color(1, 0, 0);
#else
	Color error_color = Color(1, 0, 0);
#endif

	if (source_dir->get_text().is_empty()) {
		error_message += RTR("No source folder selected") + "\n";
		script_key_error->add_theme_color_override("font_color", error_color);
		ok = false;
	}

	if (scrver->get_selected_id() == 0xfffffff) {
		error_message += RTR("No bytecode version selected") + "\n";
		script_key_error->add_theme_color_override("font_color", error_color);
		ok = false;
	}

	script_key_error->set_text(error_message);

	get_ok_button()->set_disabled(!ok);
}

void ScriptDecompRecursiveDialog::_dir_select_pressed() {

	source_folder_selection->popup_centered(Size2(800, 600));
}

void ScriptDecompRecursiveDialog::_dir_select_request(const String &p_path) {

	source_dir->set_text(p_path);
	_validate_input();
}

void ScriptDecompRecursiveDialog::_notification(int p_notification) {
	//NOP
}

void ScriptDecompRecursiveDialog::_bind_methods() {

	ClassDB::bind_method(D_METHOD("get_source_dir"), &ScriptDecompRecursiveDialog::get_source_dir);
	ClassDB::bind_method(D_METHOD("_dir_select_pressed"), &ScriptDecompRecursiveDialog::_dir_select_pressed);
	ClassDB::bind_method(D_METHOD("_dir_select_request", "path"), &ScriptDecompRecursiveDialog::_dir_select_request);
	ClassDB::bind_method(D_METHOD("_bytcode_changed", "id"), &ScriptDecompRecursiveDialog::_bytcode_changed);
}
