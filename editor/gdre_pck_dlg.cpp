/*************************************************************************/
/*  gdre_pck_dlg.cpp                                                     */
/*************************************************************************/

#include "gdre_pck_dlg.h"

PackDialog::PackDialog() {

	set_title(TTR("PCK explorer"));
	set_resizable(true);

	target_folder_selection = memnew(EditorFileDialog);
	target_folder_selection->set_access(EditorFileDialog::ACCESS_FILESYSTEM);
	target_folder_selection->set_mode(EditorFileDialog::MODE_OPEN_DIR);
	target_folder_selection->connect("dir_selected", this, "_dir_select_request");
	add_child(target_folder_selection);

	VBoxContainer *script_vb = memnew(VBoxContainer);

	//Version inf label
	vernfo = memnew(Label);
	script_vb->add_margin_child(TTR("Version:"), vernfo);

	//File list
	file_list = memnew(Tree);
	file_list->set_custom_minimum_size(Size2(1000, 600) * EDSCALE);

	file_list->set_v_size_flags(SIZE_EXPAND_FILL);
	file_list->set_columns(4);
	file_list->set_column_titles_visible(true);

	file_list->set_column_title(0, String());
	file_list->set_column_title(1, String());
	file_list->set_column_title(2, TTR("File name"));
	file_list->set_column_title(3, TTR("Size"));

	file_list->set_column_expand(0, false);
	file_list->set_column_min_width(0, 40 * EDSCALE);
	file_list->set_column_expand(1, false);
	file_list->set_column_min_width(1, 40 * EDSCALE);

	file_list->set_column_expand(3, false);
	file_list->set_column_min_width(3, 80 * EDSCALE);

	root = file_list->create_item();
	file_list->set_hide_root(true);

	file_list->connect("column_title_pressed", this, "_select_all_toggle");
	file_list->connect("item_edited", this, "_validate_selection");

	script_vb->add_margin_child(TTR("Files:"), file_list);

	//Target directory
	HBoxContainer *dir_hbc = memnew(HBoxContainer);
	target_dir = memnew(LineEdit);
	target_dir->set_editable(false);
	target_dir->set_h_size_flags(SIZE_EXPAND_FILL);
	dir_hbc->add_child(target_dir);

	select_dir = memnew(Button);
	select_dir->set_text(TTR("Select folder..."));
	select_dir->connect("pressed", this, "_dir_select_pressed");
	dir_hbc->add_child(select_dir);

	script_vb->add_margin_child(TTR("Target directory:"), dir_hbc);

	add_child(script_vb);

	get_ok()->set_text(TTR("Extract..."));
	_validate_selection();

	add_cancel(TTR("Cancel"));
}

PackDialog::~PackDialog() {
	//NOP
}

void PackDialog::clear() {

	file_list->clear();
	root = file_list->create_item();
}

void PackDialog::add_file(const String &p_name, int64_t p_size, Ref<Texture> p_icon) {

	TreeItem *item = file_list->create_item(root);

	item->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
	item->set_checked(0, true);
	item->set_editable(0, true);

	item->set_cell_mode(1, TreeItem::CELL_MODE_ICON);
	item->set_icon(1, p_icon);
	item->set_text(2, p_name);
	item->set_text(3, String::num_int64(p_size));
}

void PackDialog::_dir_select_pressed() {

	target_folder_selection->popup_centered(Size2(800, 600));
}

void PackDialog::_dir_select_request(const String &p_path) {

	target_dir->set_text(p_path);
	_validate_selection();
}

void PackDialog::_validate_selection() {

	bool nothing_selected = true;
	TreeItem *it = root->get_children();
	while (it) {
		if (it->is_checked(0)) {
			nothing_selected = false;
		}
		it = it->get_next();
	}
	get_ok()->set_disabled(nothing_selected || target_dir->get_text().empty());
}

void PackDialog::_select_all_toggle(int p_col) {

	if (p_col == 0) {
		TreeItem *it = root->get_children();
		while (it) {
			it->set_checked(0, !it->is_checked(0));
			it = it->get_next();
		}
	}
	_validate_selection();
}

Vector<String> PackDialog::get_selected_files() const {

	Vector<String> ret;
	TreeItem *it = root->get_children();
	while (it) {
		if (it->is_checked(0)) {
			ret.push_back(it->get_text(2));
		}
		it = it->get_next();
	}
	return ret;
}

String PackDialog::get_target_dir() const {

	return target_dir->get_text();
}

void PackDialog::set_version(const String &p_version) {

	vernfo->set_text(p_version);
}

void PackDialog::_notification(int p_notification) {
	//NOP
}

void PackDialog::_bind_methods() {

	ClassDB::bind_method(D_METHOD("get_selected_files"), &PackDialog::get_selected_files);
	ClassDB::bind_method(D_METHOD("get_target_dir"), &PackDialog::get_target_dir);
	ClassDB::bind_method(D_METHOD("set_version", "version"), &PackDialog::set_version);

	ClassDB::bind_method(D_METHOD("_dir_select_pressed"), &PackDialog::_dir_select_pressed);
	ClassDB::bind_method(D_METHOD("_dir_select_request", "path"), &PackDialog::_dir_select_request);
	ClassDB::bind_method(D_METHOD("_select_all_toggle", "col"), &PackDialog::_select_all_toggle);
	ClassDB::bind_method(D_METHOD("_validate_selection"), &PackDialog::_validate_selection);
}
