/*************************************************************************/
/*  file_access_pack.h                                                   */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2021 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2021 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#ifndef GDRE_PACKED_DATA_H
#define GDRE_PACKED_DATA_H

#include "core/object/reference.h"
#include "core/io/file_access_pack.h"


class GDREPackedSource : public PackSource {
public:
	virtual bool try_open_pack(const String &p_path, bool p_replace_files, size_t p_offset);
	virtual FileAccess *get_file(const String &p_path, PackedData::PackedFile *p_file);
};

class PackedFileInfo : public Reference{
	GDCLASS(PackedFileInfo, Reference);
	friend class GDREPackedData;
	friend class PckDumper;

	String path;
	String raw_path;
	PackedData::PackedFile pf;
	bool malformed_path;
	bool md5_passed;
	uint32_t flags;

	void init(const String &pck_path, const String &p_path, const uint64_t ofs, const uint64_t sz, const uint8_t md5arr[16], const bool encrypted = false){
		pf.pack = pck_path;
		raw_path = p_path;
		pf.offset = ofs;
		pf.size = sz;
		pf.src = memnew(GDREPackedSource);
		memcpy(pf.md5, md5arr, 16);
		malformed_path = false;
		pf.encrypted = encrypted;
		fix_path();
	}
	void set_md5_match(bool pass){md5_passed = pass;}

public:
	String get_pack(){return pf.pack;}
	String get_path(){return path;}
	String get_raw_path(){return raw_path;}
	uint64_t get_offset(){return pf.offset;}
	uint64_t get_size(){return pf.size;}
	Vector<uint8_t> get_md5(){
		Vector<uint8_t> ret;
		ret.resize(16);
		memcpy(ret.ptrw(),pf.md5, 16);
		return ret;
	}
	bool is_malformed(){return !malformed_path;}
	bool is_encrypted(){return pf.encrypted;}
	bool is_checksum_validated(){return md5_passed;}
private:
	void fix_path();
};


class GDREPackedData: public PackedData{
	friend class FileAccessPack;
	friend class DirAccessGDRE;
	friend class PackSource;
    friend class GDREPackedSource;
public:

	struct PackInfo {
		String pack_file = "";
		uint32_t ver_major = 0;
		uint32_t ver_minor = 0;
		uint32_t ver_rev = 0;
		uint32_t fmt_version = 0;
		uint32_t pack_flags = 0;
		uint64_t file_base = 0;
		uint32_t file_count = 0;
		
		void clear(){
			pack_file = "";
			ver_major = 0;
			ver_minor = 0;
			ver_rev = 0;
			fmt_version = 0;
			pack_flags = 0;
			file_base = 0;
			file_count = 0;
		}
		PackInfo (String f, uint32_t vmaj, uint32_t vmin, uint32_t vrev, uint32_t fver, uint32_t flags, uint64_t base, uint32_t count){
			ver_major = vmaj;
			ver_minor = vmin;
			ver_rev = vrev;
			fmt_version = fver;
			pack_flags = flags;
			file_base = base;
			file_count = count;
		}
		PackInfo(){
			clear();
		}
	};


private:
	struct PackedDir {
		PackedDir *parent = nullptr;
		String name;
		Map<String, PackedDir *> subdirs;
		Set<String> files;
	};

	struct PathMD5 {
		uint64_t a = 0;
		uint64_t b = 0;
		bool operator<(const PathMD5 &p_md5) const {
			if (p_md5.a == a) {
				return b < p_md5.b;
			} else {
				return a < p_md5.a;
			}
		}

		bool operator==(const PathMD5 &p_md5) const {
			return a == p_md5.a && b == p_md5.b;
		}

		PathMD5() {}

		PathMD5(const Vector<uint8_t> &p_buf) {
			a = *((uint64_t *)&p_buf[0]);
			b = *((uint64_t *)&p_buf[8]);
		}
	};
	Map<PathMD5, Ref<PackedFileInfo>> files;


	Vector<PackSource *> sources;

	PackedDir *root;
	PackInfo pack_info;
	uint8_t old_key[32] ={0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0};
	bool set_key = false;
	Vector<uint8_t> enc_key;
	String enc_key_str = "";

    bool encrypted = false;
	static GDREPackedData *singleton;
	bool disabled = false;

	void _free_packed_dirs(PackedDir *p_dir);

public:

	void add_pack_source(PackSource *p_source);
	void add_path(const String &pkg_path, const String &path, uint64_t ofs, uint64_t size, const uint8_t *p_md5, PackSource *p_src, bool p_replace_files, bool p_encrypted = false); // for PackSource

	void set_disabled(bool p_disabled) { disabled = p_disabled; }
	_FORCE_INLINE_ bool is_disabled() const { return disabled; }

	static GDREPackedData *get_singleton() { 
		if (!singleton){
			memnew(GDREPackedData);
		}
		return singleton; 
	}
	Error add_pack(const String &p_path, bool p_replace_files, size_t p_offset);
	
	Vector<uint8_t> get_encryption_key(){return enc_key;}
	String get_encryption_key_string(){return enc_key_str;}
	void set_encryption_key(Vector<uint8_t> key);
	void set_encryption_key(const String &key);
	void reset_encryption_key();

	Vector<Ref<PackedFileInfo>> get_file_info_list();
	uint32_t get_pack_version(){return pack_info.fmt_version;}
	String   get_version_string(){return String(itos(pack_info.ver_major) +"."+ itos(pack_info.ver_minor) +"."+ itos(pack_info.ver_rev));}
	uint32_t get_ver_major(){return pack_info.ver_major;}
	uint32_t get_ver_minor(){return pack_info.ver_minor;}
	uint32_t get_ver_rev(){return pack_info.ver_rev;}
	uint32_t get_file_count(){return pack_info.file_count;}

	_FORCE_INLINE_ FileAccess *try_open_path(const String &p_path);
	_FORCE_INLINE_ bool has_path(const String &p_path);

	_FORCE_INLINE_ DirAccess *try_open_directory(const String &p_path);
	_FORCE_INLINE_ bool has_directory(const String &p_path);

	void clear();

	GDREPackedData();
	~GDREPackedData();
};


FileAccess *GDREPackedData::try_open_path(const String &p_path) {
	PathMD5 pmd5(p_path.md5_buffer());
	Map<PathMD5, Ref<PackedFileInfo>>::Element *E = files.find(pmd5);
	if (!E) {
		return nullptr; //not found
	}
	if (E->get()->pf.offset == 0) {
		return nullptr; //was erased
	}

	return E->get()->pf.src->get_file(p_path, &E->get()->pf);
}

bool GDREPackedData::has_path(const String &p_path) {
	return files.has(PathMD5(p_path.md5_buffer()));
}

bool GDREPackedData::has_directory(const String &p_path) {
	DirAccess *da = try_open_directory(p_path);
	if (da) {
		memdelete(da);
		return true;
	} else {
		return false;
	}
}

class FileAccessGDRE : public FileAccess{
public:
	static FileAccess *open(const String &p_path, int p_mode_flags, Error *r_error = nullptr); /// Create a file access (for the current platform) this is the only portable way of accessing files.
	static bool exists(const String &pname);
};

class DirAccessGDRE : public DirAccess {
	GDREPackedData::PackedDir *current;

	List<String> list_dirs;
	List<String> list_files;
	bool cdir = false;

	GDREPackedData::PackedDir *_find_dir(String p_dir);

public:
	virtual Error list_dir_begin();
	virtual String get_next();
	virtual bool current_is_dir() const;
	virtual bool current_is_hidden() const;
	virtual void list_dir_end();

	virtual int get_drive_count();
	virtual String get_drive(int p_drive);

	virtual Error change_dir(String p_dir);
	virtual String get_current_dir(bool p_include_drive = true);

	virtual bool file_exists(String p_file);
	virtual bool dir_exists(String p_dir);

	virtual Error make_dir(String p_dir);

	virtual Error rename(String p_from, String p_to);
	virtual Error remove(String p_name);
	
	size_t get_space_left();

	virtual String get_filesystem_type() const;
	
	static DirAccess * open(const String &p_path, Error *r_error = nullptr);

	DirAccessGDRE();
	~DirAccessGDRE() {}
};

DirAccess *GDREPackedData::try_open_directory(const String &p_path) {
	DirAccess *da = memnew(DirAccessGDRE());
	if (da->change_dir(p_path) != OK) {
		memdelete(da);
		da = nullptr;
	}
	return da;
}


#endif // GDRE_PACKED_DATA
