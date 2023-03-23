#pragma once

#include "core/io/logger.h"

class GDRELogger : public Logger {
	Ref<FileAccess> file;
	String base_path;

public:
	String get_path() { return base_path; };
	GDRELogger();
	Error open_file(const String &p_base_path);
	void close_file();
	virtual void logv(const char *p_format, va_list p_list, bool p_err) _PRINTF_FORMAT_ATTRIBUTE_2_0;

	virtual ~GDRELogger();
};