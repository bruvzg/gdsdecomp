#ifndef RE_PCK_INFO_H
#define RE_PCK_INFO_H

#include "core/map.h"
#include "core/resource.h"
#include "core/object.h"
#include "core/os/file_access.h"


class PackedFile {
public:
	String path;
	String raw_path;
	uint64_t offset;
	uint64_t size;
	uint8_t md5[16];
	bool malformed_path;
	bool md5_match;
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
	PackedFile(const String path, const uint64_t ofs, const uint64_t sz, const uint8_t md5arr[16]) {
		raw_path = path;
		offset = ofs;
		size = sz;
		for (int i = 0; i < 16; i++) {
			md5[i] = md5arr[i];
		}
		malformed_path = false;
		md5_match = false;
		fix_path();
	}

private:
	void fix_path();
};

class PckInfo : public Object {
	GDCLASS(PckInfo, Object)
public:
	String path;
	uint32_t pck_ver;
	Vector<Ref<PackedFile>> *files;
	uint32_t ver_major;
	uint32_t ver_minor;
	uint32_t ver_rev;
	int file_count;
	int add_file(const PackedFile f);

	PckInfo() {
		pck_ver = 1;
		file_count = 0;
		ver_major = 0;
		ver_minor = 0;
		ver_rev = 0;
	}
	PckInfo(String p) {
		path = p;
	}
};

class PckDumper : public Object {
	GDCLASS(PckDumper, Object)

	String path;
	uint32_t pck_ver;
	Vector<Ref<PackedFile>> files;
	uint32_t ver_major;
	uint32_t ver_minor;
	uint32_t ver_rev;
	int file_count;
	bool loaded = false;
	bool _get_magic_number(FileAccess *pck);
	bool _pck_file_check_md5(FileAccess *pck, const Ref<PackedFile> f);
	Vector<String> dumped_files;

public:
	
	bool skip_malformed_paths = false;
	bool skip_failed_md5 = false;
	Error load_pck(const String &p_path);
	bool check_md5_all_files();
	Error pck_dump_to_dir(const String &dir);
	Error pck_load_and_dump(const String &p_path, const String &dir);
	bool is_loaded();
	String get_engine_version();
	int get_file_count();
	Vector<String> get_loaded_files();
	Vector<String> get_dumped_files();

};


#endif //RE_PCK_INFO_H
