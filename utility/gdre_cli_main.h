#ifndef GDRE_CLI_MAIN_H
#define GDRE_CLI_MAIN_H

#include "core/object/object.h"

#include "gdre_settings.h"
class GDRECLIMain : public Object {
	GDCLASS(GDRECLIMain, Object);

private:
	static GDRECLIMain *singleton;
	// private:
	// 	GDRESettings *gdres_singleton;

protected:
	static void _bind_methods();

public:
	GDRECLIMain *get_singleton();
	Error set_key(const String &key);
	Error load_pack(const String &path);

	Error open_log(const String &path);
	Error close_log();
	String get_cli_abs_path(const String &path);
	Error copy_dir(const String &src_path, const String dst_path);
	Vector<uint8_t> get_key() const;
	String get_key_str() const;
	void clear_data();
	bool is_loaded();
	String get_engine_version();
	int get_file_count();
	String get_gdre_version();
	Vector<String> get_loaded_files();

	GDRECLIMain();
	~GDRECLIMain();
};

#endif
