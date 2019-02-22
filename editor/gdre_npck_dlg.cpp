/*************************************************************************/
/*  gdre_npck_dlg.cpp                                                     */
/*************************************************************************/

#include "gdre_npck_dlg.h"

#include "core/version.h"

NewPackDialog::NewPackDialog() {

	set_title(RTR("Create new PCK..."));
	set_resizable(true);

	VBoxContainer *script_vb = memnew(VBoxContainer);

	message = memnew(TextEdit);
	message->set_readonly(true);
	message->set_custom_minimum_size(Size2(1000, 300) * EDSCALE);

	script_vb->add_margin_child(RTR("Files:"), message);

	ver_base = memnew(SpinBox);
	ver_base->set_min(0);
	ver_base->set_max(1);
	ver_base->set_step(1);
	ver_base->set_value(1);
	script_vb->add_margin_child(RTR("PCK version (0 - Godot 2.x; 1 - Godot 3.x):"), ver_base);

	HBoxContainer *dir_hbc = memnew(HBoxContainer);

	ver_major = memnew(SpinBox);
	ver_major->set_min(0);
	ver_major->set_max(99999);
	ver_major->set_step(1);
	ver_major->set_value(VERSION_MAJOR);
	dir_hbc->add_child(ver_major);

	ver_minor = memnew(SpinBox);
	ver_minor->set_min(0);
	ver_minor->set_max(99999);
	ver_minor->set_step(1);
	ver_minor->set_value(VERSION_MINOR);
	dir_hbc->add_child(ver_minor);
	ver_rev = memnew(SpinBox);
	ver_rev->set_min(0);
	ver_rev->set_max(99999);
	ver_rev->set_step(1);
	ver_rev->set_value(0);
	dir_hbc->add_child(ver_rev);

	script_vb->add_margin_child(RTR("Target Godot engine version:"), dir_hbc);

	wmark = memnew(LineEdit);
	wmark->set_text(String("Created with Godot RE tools, v0.0.1-poc!"));
	script_vb->add_margin_child(RTR("Extra tag:"), wmark);

	add_child(script_vb);

	get_ok()->set_text(RTR("Save..."));
	add_cancel(RTR("Cancel"));
}

NewPackDialog::~NewPackDialog() {
	//NOP
}

void NewPackDialog::set_message(const String &p_text) {
	message->set_text(p_text);
}

int NewPackDialog::get_version_pack() const {
	return ver_base->get_value();
}

int NewPackDialog::get_version_major() const {
	return ver_major->get_value();
}

int NewPackDialog::get_version_minor() const {
	return ver_minor->get_value();
}

int NewPackDialog::get_version_rev() const {
	return ver_rev->get_value();
}

String NewPackDialog::get_watermark() const {
	return wmark->get_text();
}

void NewPackDialog::_notification(int p_notification) {
	//NOP
}

void NewPackDialog::_bind_methods() {
	//NOP
}
