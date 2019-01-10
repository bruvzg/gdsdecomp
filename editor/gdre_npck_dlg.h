/*************************************************************************/
/*  gdre_npck_dlg.h                                                       */
/*************************************************************************/

#ifndef GODOT_RE_NPCK_DLG_H
#define GODOT_RE_NPCK_DLG_H

#include "core/map.h"
#include "core/resource.h"
#include "editor/editor_export.h"
#include "editor/editor_node.h"

class NewPackDialog : public AcceptDialog {
	GDCLASS(NewPackDialog, AcceptDialog)

	TextEdit *message;

	SpinBox *ver_base;
	SpinBox *ver_major;
	SpinBox *ver_minor;
	SpinBox *ver_rev;

	LineEdit *wmark;

protected:
	void _notification(int p_notification);
	static void _bind_methods();

public:
	void set_message(const String &p_text);

	int get_version_pack() const;
	int get_version_major() const;
	int get_version_minor() const;
	int get_version_rev() const;
	String get_watermark() const;

	NewPackDialog();
	~NewPackDialog();
};

#endif
