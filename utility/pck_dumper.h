#ifndef RE_PCK_DUMPER_H
#define RE_PCK_DUMPER_H

#include "core/templates/map.h"
#include "core/io/resource.h"
#include "core/object/object.h"
#include "core/os/file_access.h"
#include "core/object/reference.h"
#include "core/io/resource_importer.h"



class PackedFile {

public:
	String name;
	String path;
	String raw_path;
	uint64_t offset;
	uint64_t size;
	uint8_t md5[16];
	bool malformed_path;
	bool md5_match;
	uint32_t flags;
	bool encrypted;

	void set_stuff(const String path, const uint64_t ofs, const uint64_t sz, const uint8_t md5arr[16]);
	PackedFile() {
		name = "";
		path = "";
		raw_path = "";
		offset = 0;
		size = 0;
		malformed_path = false;
		md5_match = false;
	}

	PackedFile(uint64_t p_offset, uint64_t p_size) {
		path = "";
		raw_path = "";
		offset = p_offset;
		size = p_size;
		malformed_path = false;
		md5_match = false;
	}
	PackedFile(const String p_path, const uint64_t ofs, const uint64_t sz, const uint8_t md5arr[16], const uint32_t fl = 0) {
		raw_path = p_path;
		offset = ofs;
		size = sz;
		for (int i = 0; i < 16; i++) {
			md5[i] = md5arr[i];
		}
		malformed_path = false;
		md5_match = false;
		flags = fl;
		fix_path();
		name = path.get_file();
	}

private:
	void fix_path();
};

class PckDumper : public Reference {
	GDCLASS(PckDumper, Reference)
	String script_key;
	String path;
	uint32_t pck_ver;
	Vector<PackedFile> files;
	uint32_t ver_major;
	uint32_t ver_minor;
	uint32_t ver_rev;
	uint32_t pack_flags = 0;
	uint64_t file_base = 0;

	int file_count;
	bool loaded = false;
	bool _get_magic_number(FileAccess *pck);
	bool _pck_file_check_md5(const PackedFile & f);
	Vector<String> dumped_files;
protected:
	static void _bind_methods();

public:
	
	bool skip_malformed_paths = false;
	bool skip_failed_md5 = false;
	void PckDumper::set_key(const String &s);
	Vector<uint8_t> PckDumper::get_key() const;

	Error load_pck(const String &p_path);
	bool check_md5_all_files();
	Error pck_dump_to_dir(const String &dir);
	Error pck_load_and_dump(const String &p_path, const String &dir);
	bool is_loaded();
	String get_engine_version();
	int get_file_count();
	Vector<String> get_loaded_files();

};

#endif //RE_PCK_DUMPER_H
