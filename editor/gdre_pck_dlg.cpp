/*************************************************************************/
/*  gdre_pck_dlg.cpp                                                     */
/*************************************************************************/

#include "gdre_pck_dlg.h"

PackDialog::PackDialog() {

	set_title(RTR("PCK explorer"));
	set_resizable(true);
	updating = false;
	have_malformed_names = false;

	target_folder_selection = memnew(FileDialog);
	target_folder_selection->set_access(FileDialog::ACCESS_FILESYSTEM);
	target_folder_selection->set_mode(FileDialog::MODE_OPEN_DIR);
	target_folder_selection->connect("dir_selected", this, "_dir_select_request");
	target_folder_selection->set_show_hidden_files(true);
	add_child(target_folder_selection);

	VBoxContainer *script_vb = memnew(VBoxContainer);

	//Version inf label
	vernfo = memnew(Label);
	script_vb->add_margin_child(RTR("Version:"), vernfo);

	//PCK stats
	gennfo = memnew(Label);
	script_vb->add_margin_child(RTR("Information:"), gennfo);

	//File list
	file_list = memnew(Tree);
	file_list->set_custom_minimum_size(Size2(1000, 600) * EDSCALE);

	file_list->set_v_size_flags(SIZE_EXPAND_FILL);
	file_list->set_columns(2);
	file_list->set_column_titles_visible(true);

	file_list->set_column_title(0, RTR("File name"));
	file_list->set_column_title(1, RTR("Size"));

	file_list->set_column_expand(0, true);
	file_list->set_column_expand(1, false);
	file_list->set_column_min_width(1, 120 * EDSCALE);

	file_list->add_constant_override("draw_relationship_lines", 1);

	root = file_list->create_item();
	root->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
	root->set_checked(0, true);
	root->set_editable(0, true);
	root->set_icon(0, get_icon("folder", "FileDialog"));
	root->set_text(0, "res://");
	root->set_metadata(0, String());

	file_list->connect("item_edited", this, "_item_edited");

	script_vb->add_margin_child(RTR("Files:"), file_list);

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

	get_ok()->set_text(RTR("Extract..."));
	_validate_selection();

	add_cancel(RTR("Cancel"));
}

PackDialog::~PackDialog() {
	//NOP
}

void PackDialog::clear() {

	file_list->clear();
	root = file_list->create_item();
	root->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
	root->set_checked(0, true);
	root->set_editable(0, true);
	root->set_icon(0, get_icon("folder", "FileDialog"));
	root->set_text(0, "res://");
	root->set_metadata(0, String());

	have_malformed_names = false;

	_validate_selection();
}

void PackDialog::add_file(const String &p_name, uint64_t p_size, Ref<Texture> p_icon, String p_error, bool p_malformed_name) {

	if (p_malformed_name) {
		have_malformed_names = true;
	}

	updating = true;
	add_file_to_item(root, p_name, p_name, p_size, p_icon, p_error);
	updating = false;
}

void PackDialog::add_file_to_item(TreeItem *p_item, const String &p_fullname, const String &p_name, uint64_t p_size, Ref<Texture> p_icon, String p_error) {

	int pp = p_name.find("/");
	if (pp == -1) {
		//Add file
		TreeItem *item = file_list->create_item(p_item);

		item->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
		item->set_checked(0, true);
		item->set_editable(0, true);
		item->set_icon(0, p_icon);
		item->set_text(0, p_name);
		item->set_metadata(0, p_fullname);
		if (p_size < (1024)) {
			item->set_text(1, String::num_int64(p_size) + " B");
		} else if (p_size < (1024 * 1024)) {
			item->set_text(1, String::num((double)p_size / 1024, 2) + " KiB");
		} else if (p_size < (1024 * 1024 * 1024)) {
			item->set_text(1, String::num((double)p_size / (1024 * 1024), 2) + " MiB");
		} else {
			item->set_text(1, String::num((double)p_size / (1024 * 1024 * 1024), 2) + " GiB");
		}
		item->set_tooltip(0, p_error);
		item->set_tooltip(1, p_error);

		_validate_selection();
	} else {
		String fld_name = p_name.substr(0, pp);
		String path = p_name.substr(pp + 1, p_name.length());
		//Add folder if any
		TreeItem *it = p_item->get_children();
		while (it) {
			if (it->get_text(0) == fld_name) {
				add_file_to_item(it, p_fullname, path, p_size, p_icon, p_error);
				return;
			}
			it = it->get_next();
		}
		TreeItem *item = file_list->create_item(p_item);

		item->set_cell_mode(0, TreeItem::CELL_MODE_CHECK);
		item->set_checked(0, true);
		item->set_editable(0, true);
		item->set_icon(0, get_icon("folder", "FileDialog"));
		item->set_text(0, fld_name);
		item->set_metadata(0, String());
		add_file_to_item(item, p_fullname, path, p_size, p_icon, p_error);
	}
}

void PackDialog::_dir_select_pressed() {

	target_folder_selection->popup_centered(Size2(800, 600));
}

void PackDialog::_dir_select_request(const String &p_path) {

	target_dir->set_text(p_path);
	_validate_selection();
}

size_t PackDialog::_selected(TreeItem *p_item) {

	size_t selected = 0;
	String path = p_item->get_metadata(0);
	if ((path != String()) && p_item->is_checked(0)) { //not a dir
		selected = 1;
	}

	TreeItem *it = p_item->get_children();
	while (it) {
		selected += _selected(it);
		it = it->get_next();
	}
	return selected;
}

void PackDialog::_update_subitems(TreeItem *p_item, bool p_check, bool p_first) {

	if (p_check) {
		p_item->set_checked(0, true);
	} else {
		p_item->set_checked(0, false);
	}

	if (p_item->get_children()) {
		_update_subitems(p_item->get_children(), p_check);
	}

	if (!p_first && p_item->get_next()) {
		_update_subitems(p_item->get_next(), p_check);
	}
}

void PackDialog::_item_edited() {

	if (updating)
		return;

	TreeItem *item = file_list->get_edited();
	if (!item)
		return;

	String path = item->get_metadata(0);
	updating = true;
	if (path == String()) { //a dir
		_update_subitems(item, item->is_checked(0), true);
	}

	if (item->is_checked(0)) {
		while (item) {
			item->set_checked(0, true);
			item = item->get_parent();
		}
	}
	updating = false;

	_validate_selection();
}

void PackDialog::_validate_selection() {

	bool nothing_selected = (_selected(root) == 0);
	bool ok = true;
	String error_message;

#ifdef TOOLS_ENABLED
	Color error_color = (EditorNode::get_singleton()) ? EditorNode::get_singleton()->get_gui_base()->get_color("error_color", "Editor") : Color(1, 0, 0);
#else
	Color error_color = Color(1, 0, 0);
#endif

	if (have_malformed_names) {
		error_message += RTR("Some files have malformed names") + "\n";
		script_key_error->add_color_override("font_color", error_color);
	}
	if (target_dir->get_text().empty()) {
		error_message += RTR("No destination folder selected") + "\n";
		script_key_error->add_color_override("font_color", error_color);
		ok = false;
	}
	if (nothing_selected) {
		error_message += RTR("No files selected") + "\n";
		script_key_error->add_color_override("font_color", error_color);
		ok = false;
	}

	script_key_error->set_text(error_message);

	get_ok()->set_disabled(!ok);
}

Vector<String> PackDialog::get_selected_files() const {

	Vector<String> ret;
	_get_selected_files(ret, root);
	return ret;
}

void PackDialog::_get_selected_files(Vector<String> &p_list, TreeItem *p_item) const {

	String name = p_item->get_metadata(0);
	if (p_item->is_checked(0) && (name != String())) {
		p_list.push_back(name);
	}
	TreeItem *it = p_item->get_children();
	while (it) {
		_get_selected_files(p_list, it);
		it = it->get_next();
	}
}

String PackDialog::get_target_dir() const {

	return target_dir->get_text();
}

void PackDialog::set_version(const String &p_version) {

	vernfo->set_text(p_version);
}

void PackDialog::set_info(const String &p_info) {

	gennfo->set_text(p_info);
}

void PackDialog::_notification(int p_notification) {
	//NOP
}

void PackDialog::_bind_methods() {

	ClassDB::bind_method(D_METHOD("get_selected_files"), &PackDialog::get_selected_files);
	ClassDB::bind_method(D_METHOD("get_target_dir"), &PackDialog::get_target_dir);
	ClassDB::bind_method(D_METHOD("set_version", "version"), &PackDialog::set_version);
	ClassDB::bind_method(D_METHOD("set_info", "info"), &PackDialog::set_info);

	ClassDB::bind_method(D_METHOD("_dir_select_pressed"), &PackDialog::_dir_select_pressed);
	ClassDB::bind_method(D_METHOD("_dir_select_request", "path"), &PackDialog::_dir_select_request);
	ClassDB::bind_method(D_METHOD("_item_edited"), &PackDialog::_item_edited);
}
