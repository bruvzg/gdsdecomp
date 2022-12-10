/*************************************************************************/
/*  gdre_enc_key.h                                                       */
/*************************************************************************/

#ifndef GODOT_RE_ENC_KEY_H
#define GODOT_RE_ENC_KEY_H

#include "core/io/resource.h"
#include "core/templates/rb_map.h"

#include "scene/gui/control.h"
#include "scene/gui/dialogs.h"
#include "scene/gui/file_dialog.h"
#include "scene/gui/item_list.h"
#include "scene/gui/line_edit.h"
#include "scene/gui/spin_box.h"
#include "scene/gui/text_edit.h"

#ifdef TOOLS_ENABLED

#include "editor/editor_scale.h"
#else
#define EDSCALE 1.0
#endif

class EncKeyDialog : public AcceptDialog {
	GDCLASS(EncKeyDialog, AcceptDialog)

	LineEdit *script_key;
	Label *script_key_error;

	void _validate_input();
	void _script_encryption_key_changed(const String &p_key);

protected:
	void _notification(int p_notification);
	static void _bind_methods();

public:
	Vector<uint8_t> get_key() const;

	EncKeyDialog();
	~EncKeyDialog();
};

#endif
