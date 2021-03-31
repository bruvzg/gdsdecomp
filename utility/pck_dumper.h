#ifndef RE_PCK_DUMPER_H
#define RE_PCK_DUMPER_H

#include "core/templates/map.h"
#include "core/io/resource.h"
#include "core/object/object.h"
#include "core/os/file_access.h"
#include "core/object/reference.h"
#include "core/io/resource_importer.h"
#include "core/io/file_access_pack.h"


class PackedFileInfo {

public:
	String name;
	String path;
	String raw_path;
	String pack; // absolute path to pack file
	uint64_t offset;
	uint64_t size;
	uint8_t md5[16];
	bool malformed_path;
	bool md5_match;
	uint32_t flags;
	bool encrypted = false;
	PackedData::PackedFile to_struct(){
		PackedData::PackedFile s;
		s.encrypted = encrypted;
		s.offset = offset;
		s.src = nullptr;
		s.pack = pack;
		s.encrypted = encrypted;
		memcpy(s.md5, md5, 16);
		return s;
	}

	void set_stuff(const String path, const uint64_t ofs, const uint64_t sz, const uint8_t md5arr[16]);
	PackedFileInfo() {
		name = "";
		path = "";
		raw_path = "";
		offset = 0;
		size = 0;
		malformed_path = false;
		md5_match = false;
	}
	PackedFileInfo(const String &pck_path, const String &p_path, const uint64_t ofs, const uint64_t sz, const uint8_t md5arr[16], const uint32_t fl = 0) {
		pack = pck_path;
		raw_path = p_path;
		offset = ofs;
		size = sz;
		memcpy(md5, md5arr, 16);
		malformed_path = false;
		md5_match = false;
		flags = fl;
		encrypted = flags & PACK_FILE_ENCRYPTED;
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
	Vector<PackedFileInfo> files;
	uint32_t ver_major;
	uint32_t ver_minor;
	uint32_t ver_rev;
	uint32_t pack_flags = 0;
	uint64_t file_base = 0;

	int file_count;
	bool loaded = false;
	bool _get_magic_number(FileAccess *pck);
	bool _pck_file_check_md5(PackedFileInfo & f);
	Vector<String> dumped_files;
protected:
	static void _bind_methods();

public:
	
	bool skip_malformed_paths = false;
	bool skip_failed_md5 = false;
	void set_key(const String &s);
	Vector<uint8_t> get_key() const;
	FileAccess *get_file_access(const String &p_path, PackedFileInfo *p_file);

	Error load_pck(const String &p_path);
	bool check_md5_all_files();
	Error pck_dump_to_dir2(const String &dir);
	Error pck_dump_to_dir(const String &dir);
	Error pck_load_and_dump(const String &p_path, const String &dir);
	bool is_loaded();
	String get_engine_version();
	int get_file_count();
	Vector<String> get_loaded_files();

};

#endif //RE_PCK_DUMPER_H
