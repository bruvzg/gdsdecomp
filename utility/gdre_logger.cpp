#include "gdre_logger.h"
#include "editor/gdre_editor.h"
#include "gdre_settings.h"

#include "core/io/dir_access.h"

bool inGuiMode() {
	//check if we are in GUI mode
	if (GodotREEditor::get_singleton() && GDRESettings::get_singleton() && !GDRESettings::get_singleton()->is_headless()) {
		return true;
	}
	return false;
}

void GDRELogger::logv(const char *p_format, va_list p_list, bool p_err) {
	if (!should_log(p_err)) {
		return;
	}
	if (file.is_valid() || inGuiMode()) {
		const int static_buf_size = 512;
		char static_buf[static_buf_size];
		char *buf = static_buf;
		va_list list_copy;
		va_copy(list_copy, p_list);
		int len = vsnprintf(buf, static_buf_size, p_format, p_list);
		if (len >= static_buf_size) {
			buf = (char *)Memory::alloc_static(len + 1);
			vsnprintf(buf, len + 1, p_format, list_copy);
		}
		va_end(list_copy);

		if (inGuiMode()) {
			GodotREEditor::get_singleton()->call_deferred(SNAME("emit_signal"), "write_log_message", String(buf));
		}
		if (file.is_valid()) {
			file->store_buffer((uint8_t *)buf, len);

			if (p_err || _flush_stdout_on_print) {
				// Don't always flush when printing stdout to avoid performance
				// issues when `print()` is spammed in release builds.
				file->flush();
			}
		}
		if (len >= static_buf_size) {
			Memory::free_static(buf);
		}
	}
}

Error GDRELogger::open_file(const String &p_base_path) {
	if (file.is_valid()) {
		return ERR_ALREADY_IN_USE;
	}
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_USERDATA);
	if (da.is_valid()) {
		da->make_dir_recursive(p_base_path.get_base_dir());
	}
	Error err;
	file = FileAccess::open(p_base_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(file.is_null(), err, "Failed to open log file " + p_base_path + " for writing.");
	base_path = p_base_path;
	return OK;
}

void GDRELogger::close_file() {
	if (file.is_valid()) {
		file->flush();
		file = Ref<FileAccess>();
		base_path = "";
	}
}

GDRELogger::GDRELogger() {}
GDRELogger::~GDRELogger() {
	close_file();
}