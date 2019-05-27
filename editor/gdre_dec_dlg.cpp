/*************************************************************************/
/*  gdre_dec_dlg.cpp                                                     */
/*************************************************************************/

#include "gdre_dec_dlg.h"
#include "bytecode/bytecode_versions.h"

ScriptDecompDialog::ScriptDecompDialog() {

	set_title(RTR("Decompile GDScript"));
	set_resizable(true);

	target_folder_selection = memnew(FileDialog);
	target_folder_selection->set_access(FileDialog::ACCESS_FILESYSTEM);
	target_folder_selection->set_mode(FileDialog::MODE_OPEN_DIR);
	target_folder_selection->connect("dir_selected", this, "_dir_select_request");
	target_folder_selection->set_show_hidden_files(true);
	add_child(target_folder_selection);

	file_selection = memnew(FileDialog);
	file_selection->set_access(FileDialog::ACCESS_FILESYSTEM);
	file_selection->set_mode(FileDialog::MODE_OPEN_FILES);
	file_selection->connect("files_selected", this, "_add_files_request");
	file_selection->add_filter("*.gdc,*.gde;GDScript binary files");
	file_selection->set_show_hidden_files(true);
	add_child(file_selection);

	VBoxContainer *script_vb = memnew(VBoxContainer);

	//File list
	file_list = memnew(ItemList);
	file_list->set_custom_minimum_size(Size2(600, 400) * EDSCALE);

	HBoxContainer *file_list_hbc = memnew(HBoxContainer);
	add_file = memnew(Button);
	add_file->set_text(RTR("Add files..."));
	add_file->connect("pressed", this, "_add_files_pressed");
	file_list_hbc->add_child(add_file);

	remove_file = memnew(Button);
	remove_file->set_text(RTR("Remove files"));
	remove_file->connect("pressed", this, "_remove_file_pressed");
	file_list_hbc->add_child(remove_file);

	clear_files = memnew(Button);
	clear_files->set_text(RTR("Clear files"));
	clear_files->connect("pressed", this, "_clear_pressed");
	file_list_hbc->add_child(clear_files);

	script_vb->add_margin_child(RTR("Script files:"), file_list);
	script_vb->add_child(file_list_hbc);

	//Script version
	scrver = memnew(OptionButton);

	for (int i = 0; decomp_versions[i].commit != 0; i++) {
		scrver->add_item(decomp_versions[i].name, decomp_versions[i].commit);
	}

	script_vb->add_margin_child(RTR("Script bytecode version:"), scrver);
	scrver->connect("item_selected", this, "_bytcode_changed");

	//Encryption key
	script_key = memnew(LineEdit);
	script_key->connect("text_changed", this, "_script_encryption_key_changed");
	script_vb->add_margin_child(RTR("Script encryption key (256-bits as hex, optional):"), script_key);

	//Target directory
	HBoxContainer *dir_hbc = memnew(HBoxContainer);
	target_dir = memnew(LineEdit);
	target_dir->set_editable(false);
	target_dir->set_h_size_flags(SIZE_EXPAND_FILL);
	dir_hbc->add_child(target_dir);

	select_dir = memnew(Button);
	select_dir->set_text("...");
	select_dir->connect("pressed", this, "_dir_select_pressed");
	dir_hbc->add_child(select_dir);

	script_vb->add_margin_child(RTR("Destination folder:"), dir_hbc);

	script_key_error = memnew(Label);
	script_vb->add_child(script_key_error);

	add_child(script_vb);

	get_ok()->set_text(RTR("Decompile"));
	_validate_input();

	add_cancel(RTR("Cancel"));
}

ScriptDecompDialog::~ScriptDecompDialog() {
	//NOP
}

int ScriptDecompDialog::get_bytecode_version() const {

	return scrver->get_selected_id();
}

Vector<String> ScriptDecompDialog::get_file_list() const {

	Vector<String> ret;
	for (int i = 0; i < file_list->get_item_count(); i++) {
		ret.push_back(file_list->get_item_text(i));
	}
	return ret;
}

String ScriptDecompDialog::get_target_dir() const {

	return target_dir->get_text();
}

Vector<uint8_t> ScriptDecompDialog::get_key() const {

	Vector<uint8_t> key;

	if (script_key->get_text().empty() || !script_key->get_text().is_valid_hex_number(false) || script_key->get_text().length() != 64) {
		return key;
	}

	key.resize(32);
	for (int i = 0; i < 32; i++) {
		int v = 0;
		if (i * 2 < script_key->get_text().length()) {
			CharType ct = script_key->get_text().to_lower()[i * 2];
			if (ct >= '0' && ct <= '9')
				ct = ct - '0';
			else if (ct >= 'a' && ct <= 'f')
				ct = 10 + ct - 'a';
			v |= ct << 4;
		}

		if (i * 2 + 1 < script_key->get_text().length()) {
			CharType ct = script_key->get_text().to_lower()[i * 2 + 1];
			if (ct >= '0' && ct <= '9')
				ct = ct - '0';
			else if (ct >= 'a' && ct <= 'f')
				ct = 10 + ct - 'a';
			v |= ct;
		}
		key.write[i] = v;
	}
	return key;
}

void ScriptDecompDialog::_add_files_pressed() {

	file_selection->popup_centered(Size2(800, 600));
}

void ScriptDecompDialog::_add_files_request(const PoolVector<String> &p_files) {

	for (int i = 0; i < p_files.size(); i++) {
		file_list->add_item(p_files[i]);
	}
	_validate_input();
}

void ScriptDecompDialog::_clear_pressed() {

	file_list->clear();
	_validate_input();
}

void ScriptDecompDialog::_bytcode_changed(int p_id) {

	_validate_input();
}

void ScriptDecompDialog::_remove_file_pressed() {

	Vector<int> items = file_list->get_selected_items();
	for (int i = items.size() - 1; i >= 0; i--) {
		file_list->remove_item(items[i]);
	}
	_validate_input();
}

void ScriptDecompDialog::_validate_input() {

	bool need_key = false;

	for (int i = 0; i < file_list->get_item_count(); i++) {
		if (file_list->get_item_text(i).ends_with(".gde")) {
			need_key = true;
		}
	}

	bool ok = true;
	String error_message;

#ifdef TOOLS_ENABLED
	Color error_color = (EditorNode::get_singleton()) ? EditorNode::get_singleton()->get_gui_base()->get_color("error_color", "Editor") : Color(1, 0, 0);
#else
	Color error_color = Color(1, 0, 0);
#endif

	if (script_key->get_text().empty()) {
		if (need_key) {
			error_message += RTR("No encryption key") + "\n";
			script_key_error->add_color_override("font_color", error_color);
			ok = false;
		}
	} else if (!script_key->get_text().is_valid_hex_number(false) || script_key->get_text().length() != 64) {
		error_message += RTR("Invalid encryption key (must be 64 characters long hex)") + "\n";
		script_key_error->add_color_override("font_color", error_color);
		ok = false;
	}
	if (target_dir->get_text().empty()) {
		error_message += RTR("No destination folder selected") + "\n";
		script_key_error->add_color_override("font_color", error_color);
		ok = false;
	}
	if (file_list->get_item_count() == 0) {
		error_message += RTR("No files selected") + "\n";
		script_key_error->add_color_override("font_color", error_color);
		ok = false;
	}

	if (scrver->get_selected_id() == 0xfffffff) {
		error_message += RTR("No bytecode version selected") + "\n";
		script_key_error->add_color_override("font_color", error_color);
		ok = false;
	}

	script_key_error->set_text(error_message);

	get_ok()->set_disabled(!ok);
}

void ScriptDecompDialog::_script_encryption_key_changed(const String &p_key) {

	_validate_input();
}

void ScriptDecompDialog::_dir_select_pressed() {

	target_folder_selection->popup_centered(Size2(800, 600));
}

void ScriptDecompDialog::_dir_select_request(const String &p_path) {

	target_dir->set_text(p_path);
	_validate_input();
}

void ScriptDecompDialog::_notification(int p_notification) {
	//NOP
}

void ScriptDecompDialog::_bind_methods() {

	ClassDB::bind_method(D_METHOD("get_file_list"), &ScriptDecompDialog::get_file_list);
	ClassDB::bind_method(D_METHOD("get_target_dir"), &ScriptDecompDialog::get_target_dir);
	ClassDB::bind_method(D_METHOD("get_key"), &ScriptDecompDialog::get_key);

	ClassDB::bind_method(D_METHOD("_add_files_pressed"), &ScriptDecompDialog::_add_files_pressed);
	ClassDB::bind_method(D_METHOD("_add_files_request", "files"), &ScriptDecompDialog::_add_files_request);
	ClassDB::bind_method(D_METHOD("_remove_file_pressed"), &ScriptDecompDialog::_remove_file_pressed);
	ClassDB::bind_method(D_METHOD("_clear_pressed"), &ScriptDecompDialog::_clear_pressed);
	ClassDB::bind_method(D_METHOD("_script_encryption_key_changed", "key"), &ScriptDecompDialog::_script_encryption_key_changed);
	ClassDB::bind_method(D_METHOD("_dir_select_pressed"), &ScriptDecompDialog::_dir_select_pressed);
	ClassDB::bind_method(D_METHOD("_dir_select_request", "path"), &ScriptDecompDialog::_dir_select_request);
	ClassDB::bind_method(D_METHOD("_bytcode_changed", "id"), &ScriptDecompDialog::_bytcode_changed);
}
