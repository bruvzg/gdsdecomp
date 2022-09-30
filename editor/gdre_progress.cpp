#include "gdre_progress.h"

#include "core/string/ustring.h"
#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"
#endif
#ifndef TOOLS_ENABLED
#include "core/object/message_queue.h"
#include "core/os/os.h"
#include "main/main.h"
#include "scene/gui/label.h"
#include "scene/resources/style_box.h"

#define EDSCALE 1.0

ProgressDialog *ProgressDialog::singleton = NULL;

void ProgressDialog::_notification(int p_what) {
}

void ProgressDialog::_popup() {
	Size2 ms = main->get_combined_minimum_size();
	ms.width = MAX(500 * EDSCALE, ms.width);

	Ref<StyleBox> style = get_theme_stylebox("panel", "PopupMenu");
	ms += style->get_minimum_size();
	main->set_offset(SIDE_LEFT, style->get_margin(SIDE_LEFT));
	main->set_offset(SIDE_RIGHT, -style->get_margin(SIDE_RIGHT));
	main->set_offset(SIDE_TOP, style->get_margin(SIDE_TOP));
	main->set_offset(SIDE_BOTTOM, -style->get_margin(SIDE_BOTTOM));

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
	cancel_hb->move_to_front();
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
		DisplayServer::get_singleton()->process_events();
	}
	Main::iteration(); // this will not work on a lot of platforms, so it's only meant for the editor
	return cancelled;
}

void ProgressDialog::end_task(const String &p_task) {
	ERR_FAIL_COND(!tasks.has(p_task));
	Task &t = tasks[p_task];

	memdelete(t.vb);
	tasks.erase(p_task);

	if (tasks.is_empty())
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
	main->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
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
	cancel->connect("pressed", callable_mp(this, &ProgressDialog::_cancel_pressed));
}

#endif

bool EditorProgressGDDC::step(const String &p_state, int p_step, bool p_force_refresh) {
#ifdef TOOLS_ENABLED
	if (EditorNode::get_singleton() && ep != nullptr) {
		if (progress_dialog) {
			progress_dialog->set_visible(true);
		}
		return ep->step(p_state, p_step, p_force_refresh);
		//return EditorNode::progress_task_step(task, p_state, p_step, p_force_refresh);
	}
#endif
	if (progress_dialog) {
		progress_dialog->set_visible(true);
		return progress_dialog->task_step(task, p_state, p_step, p_force_refresh);
	}
	return false;
}

EditorProgressGDDC::EditorProgressGDDC(Node *p_parent, const String &p_task, const String &p_label, int p_amount, bool p_can_cancel) {
#ifdef TOOLS_ENABLED
	if (EditorNode::get_singleton()) {
		progress_dialog = NULL;
		ep = memnew(EditorProgress(p_task, p_label, p_amount, p_can_cancel));
		//EditorNode::progress_add_task(p_task, p_label, p_amount, p_can_cancel);
		progress_dialog = progress_dialog->get_singleton();
		if (progress_dialog) {
			progress_dialog->set_visible(true);
		}

		task = p_task;
		return;
	}
	ep = nullptr;
#endif
	if (!ProgressDialog::get_singleton()) {
		progress_dialog = memnew(ProgressDialog);
		if (p_parent)
			p_parent->add_child(progress_dialog);
	} else {
		progress_dialog = ProgressDialog::get_singleton();
	}
	if (progress_dialog) {
		progress_dialog->add_task(p_task, p_label, p_amount, p_can_cancel);
		progress_dialog->set_visible(true);
	}
	task = p_task;
}

EditorProgressGDDC::~EditorProgressGDDC() {
#ifdef TOOLS_ENABLED
	if (ep) {
		memdelete(ep);
		return;
	}
#endif
	if (progress_dialog) {
		progress_dialog->end_task(task);
	}
}