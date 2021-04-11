#ifndef RE_PCK_DUMPER_H
#define RE_PCK_DUMPER_H

#include "core/templates/map.h"
#include "core/io/resource.h"
#include "core/object/object.h"
#include "core/os/file_access.h"
#include "core/object/reference.h"
#include "core/io/resource_importer.h"
#include "core/io/file_access_pack.h"

#include "gdre_packed_data.h"

class PckDumper : public Reference {
	GDCLASS(PckDumper, Reference)
	bool skip_malformed_paths = false;
	bool skip_failed_md5 = false;
	bool should_check_md5 = false;
	bool loaded = false;
	bool _get_magic_number(FileAccess *pck);
	bool _pck_file_check_md5(Ref<PackedFileInfo> & file);
protected:
	static void _bind_methods();

public:
	
	void set_key(const String &s);
	Vector<uint8_t> get_key() const;
	String get_key_str() const;
	void clear_data();
	FileAccess *get_file_access(const String &p_path, PackedFileInfo *p_file);

	Error load_pck(const String &p_path);
	Error check_md5_all_files();
	Error pck_dump_to_dir2(const String &dir);
	Error pck_dump_to_dir(const String &dir);
	Error pck_load_and_dump(const String &p_path, const String &dir);
	bool is_loaded();
	String get_engine_version();
	int get_file_count();
	Vector<String> get_loaded_files();

};

#endif //RE_PCK_DUMPER_H
