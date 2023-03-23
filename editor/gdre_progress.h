#ifndef GDRE_PROGRESS_H
#define GDRE_PROGRESS_H
#include "core/string/ustring.h"
#ifdef TOOLS_ENABLED
struct EditorProgress;
class ProgressDialog;
class Node;
#endif
#ifndef TOOLS_ENABLED
#include "scene/gui/box_container.h"
#include "scene/gui/button.h"
#include "scene/gui/popup.h"
#include "scene/gui/progress_bar.h"
class ProgressDialog : public Popup {
	GDCLASS(ProgressDialog, Popup);
	struct Task {
		String task;
		VBoxContainer *vb;
		ProgressBar *progress;
		Label *state;
	};
	bool canceled;
	HBoxContainer *cancel_hb;
	Button *cancel;

	RBMap<String, Task> tasks;
	VBoxContainer *main;
	uint64_t last_progress_tick;

	static ProgressDialog *singleton;
	void _popup();

	void _cancel_pressed();
	bool cancelled;

protected:
	void _notification(int p_what);
	static void _bind_methods();

public:
	static ProgressDialog *get_singleton() { return singleton; }
	void add_task(const String &p_task, const String &p_label, int p_steps, bool p_can_cancel = false);
	bool task_step(const String &p_task, const String &p_state, int p_step = -1, bool p_force_redraw = true);
	void end_task(const String &p_task);

	ProgressDialog();
};
#endif

struct EditorProgressGDDC {
	String task;
	ProgressDialog *progress_dialog;
#ifdef TOOLS_ENABLED
	EditorProgress *ep;
#endif
	bool step(const String &p_state, int p_step = -1, bool p_force_refresh = true);
	EditorProgressGDDC(Node *p_parent, const String &p_task, const String &p_label, int p_amount, bool p_can_cancel = false);
	~EditorProgressGDDC();
};

#endif // GDRE_PROGRESS_H