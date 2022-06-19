#ifndef GDRE_CLI_MAIN_H
#define GDRE_CLI_MAIN_H

#include "core/object/ref_counted.h"
#include "gdre_settings.h"
class GDRECLIMain : public RefCounted {
	GDCLASS(GDRECLIMain, RefCounted);

private:
	GDRESettings *gdres_singleton;

protected:
	static void _bind_methods();

public:
	Error open_log(const String &path);
	Error close_log();
	String get_cli_abs_path(const String &path);
	Error copy_dir(const String &src_path, const String dst_path);

	GDRECLIMain() {
		gdres_singleton = memnew(GDRESettings);
	}
	~GDRECLIMain() {
		close_log();
		memdelete(gdres_singleton);
	}
};

#endif
