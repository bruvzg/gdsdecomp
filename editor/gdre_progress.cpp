#include "gdre_progress.h"

#include "core/string/ustring.h"
#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"
#include "editor/progress_dialog.h"
#endif
#ifndef TOOLS_ENABLED
#include "core/object/message_queue.h"
#include "core/os/os.h"
#include "editor/gdre_editor.h"
#include "main/main.h"
#include "scene/gui/label.h"
#include "scene/resources/style_box.h"

#define EDSCALE 1.0

ProgressDialog *ProgressDialog::singleton = nullptr;

void ProgressDialog::_notification(int p_what) {
	if (p_what == NOTIFICATION_VISIBILITY_CHANGED) {
		if (!is_visible()) {
			Node *p = get_parent();
			if (p) {
				p->remove_child(this);
			}
		}
	}
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

	auto *ed = GodotREEditor::get_singleton();
	if (ed && !is_inside_tree()) {
		Window *w = ed->get_window();
		while (w && w->get_exclusive_child()) {
			w = w->get_exclusive_child();
		}
		if (w && w != this) {
			w->add_child(this);
			popup_centered(ms);
		}
	}
}

void ProgressDialog::add_task(const String &p_task, const String &p_label, int p_steps, bool p_can_cancel) {
	if (MessageQueue::get_singleton()->is_flushing()) {
		ERR_PRINT("Do not use progress dialog (task) while flushing the message queue or using call_deferred()!");
		return;
	}

	ERR_FAIL_COND_MSG(tasks.has(p_task), "Task '" + p_task + "' already exists.");
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
	canceled = false;
	_popup();
	if (p_can_cancel) {
		cancel->grab_focus();
	}
}

bool ProgressDialog::task_step(const String &p_task, const String &p_state, int p_step, bool p_force_redraw) {
	ERR_FAIL_COND_V(!tasks.has(p_task), canceled);

	if (!p_force_redraw) {
		uint64_t tus = OS::get_singleton()->get_ticks_usec();
		if (tus - last_progress_tick < 200000) { //200ms
			return canceled;
		}
	}

	Task &t = tasks[p_task];
	if (p_step < 0) {
		t.progress->set_value(t.progress->get_value() + 1);
	} else {
		t.progress->set_value(p_step);
	}

	t.state->set_text(p_state);
	last_progress_tick = OS::get_singleton()->get_ticks_usec();
	DisplayServer::get_singleton()->process_events();

#ifndef ANDROID_ENABLED
	Main::iteration(); // this will not work on a lot of platforms, so it's only meant for the editor
#endif
	return canceled;
}

void ProgressDialog::end_task(const String &p_task) {
	ERR_FAIL_COND(!tasks.has(p_task));
	Task &t = tasks[p_task];

	memdelete(t.vb);
	tasks.erase(p_task);

	if (tasks.is_empty()) {
		hide();
	} else {
		_popup();
	}
}

void ProgressDialog::_cancel_pressed() {
	canceled = true;
}

void ProgressDialog::_bind_methods() {
}

ProgressDialog::ProgressDialog() {
	main = memnew(VBoxContainer);
	add_child(main);
	main->set_anchors_and_offsets_preset(Control::PRESET_FULL_RECT);
	set_exclusive(true);
	set_flag(Window::FLAG_POPUP, false);
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
		return ep->step(p_state, p_step, p_force_refresh);
		//return EditorNode::progress_task_step(task, p_state, p_step, p_force_refresh);
	}
#endif
	if (progress_dialog) {
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
			progress_dialog->popup_centered();
		}

		task = p_task;
		return;
	}
	ep = nullptr;
#endif
	progress_dialog = ProgressDialog::get_singleton();
	if (progress_dialog) {
		if (!progress_dialog->is_inside_tree() && p_parent) {
			p_parent->add_child(progress_dialog);
		}
		progress_dialog->add_task(p_task, p_label, p_amount, p_can_cancel);
		progress_dialog->popup_centered();
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
	// if no EditorNode...
	if (progress_dialog) {
		progress_dialog->end_task(task);
	}
}