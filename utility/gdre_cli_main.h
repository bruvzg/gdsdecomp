#ifndef GDRE_CLI_MAIN_H
#define GDRE_CLI_MAIN_H

#include "core/object/ref_counted.h"

#include "gdre_settings.h"
class GDRECLIMain : public RefCounted {
	GDCLASS(GDRECLIMain, RefCounted);

	// private:
	// 	GDRESettings *gdres_singleton;

protected:
	static void _bind_methods();

public:
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
	Vector<String> get_loaded_files();

	GDRECLIMain();
	~GDRECLIMain();
};

#endif
