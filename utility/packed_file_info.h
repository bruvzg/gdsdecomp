#ifndef GDRE_PACKED_FILE_INFO_H
#define GDRE_PACKED_FILE_INFO_H

#include "core/io/file_access_pack.h"
#include "core/object/object.h"
#include "core/object/ref_counted.h"

class PackedFileInfo : public RefCounted {
	GDCLASS(PackedFileInfo, RefCounted);
	friend class GDRESettings;
	friend class PckDumper;
	friend class GDREPackedSource;

	String path;
	String raw_path;
	PackedData::PackedFile pf;
	bool malformed_path;
	bool md5_passed;
	uint32_t flags;

	void init(const String &p_path, const PackedData::PackedFile *pfstruct) {
		pf = *pfstruct;
		raw_path = p_path;
		malformed_path = false;
		fix_path();
	}
	void init(const String &pck_path, const String &p_path, const uint64_t ofs, const uint64_t sz, const uint8_t md5arr[16], PackSource *p_src, const bool encrypted = false) {
		pf.pack = pck_path;
		raw_path = p_path;
		pf.offset = ofs;
		pf.size = sz;
		memcpy(pf.md5, md5arr, 16);
		malformed_path = false;
		pf.encrypted = encrypted;
		pf.src = p_src;
		fix_path();
	}
	void set_md5_match(bool pass) { md5_passed = pass; }

public:
	String get_pack() { return pf.pack; }
	String get_path() { return path; }
	String get_raw_path() { return raw_path; }
	uint64_t get_offset() { return pf.offset; }
	uint64_t get_size() { return pf.size; }
	Vector<uint8_t> get_md5() {
		Vector<uint8_t> ret;
		ret.resize(16);
		memcpy(ret.ptrw(), pf.md5, 16);
		return ret;
	}
	bool is_malformed() { return !malformed_path; }
	bool is_encrypted() { return pf.encrypted; }
	bool is_checksum_validated() { return md5_passed; }

private:
	void fix_path() {
		path = raw_path;
		malformed_path = false;
		String prefix = "";

		//remove prefix first
		if (path.begins_with("res://")) {
			path = path.replace_first("res://", "");
			prefix = "res://";
		} else if (path.begins_with("local://")) {
			path = path.replace_first("local://", "");
			prefix = "local://";
		}

		while (path.begins_with("~")) {
			path = path.substr(1, path.length() - 1);
			malformed_path = true;
		}
		while (path.begins_with("/")) {
			path = path.substr(1, path.length() - 1);
			malformed_path = true;
		}
		while (path.find("...") >= 0) {
			path = path.replace("...", "_");
			malformed_path = true;
		}
		while (path.find("..") >= 0) {
			path = path.replace("..", "_");
			malformed_path = true;
		}
		if (path.find("\\.") >= 0) {
			path = path.replace("\\.", "_");
			malformed_path = true;
		}
		if (path.find("//") >= 0) {
			path = path.replace("//", "_");
			malformed_path = true;
		}
		if (path.find("\\") >= 0) {
			path = path.replace("\\", "_");
			malformed_path = true;
		}
		if (path.find(":") >= 0) {
			path = path.replace(":", "_");
			malformed_path = true;
		}
		if (path.find("|") >= 0) {
			path = path.replace("|", "_");
			malformed_path = true;
		}
		if (path.find("?") >= 0) {
			path = path.replace("?", "_");
			malformed_path = true;
		}
		if (path.find(">") >= 0) {
			path = path.replace(">", "_");
			malformed_path = true;
		}
		if (path.find("<") >= 0) {
			path = path.replace("<", "_");
			malformed_path = true;
		}
		if (path.find("*") >= 0) {
			path = path.replace("*", "_");
			malformed_path = true;
		}
		if (path.find("\"") >= 0) {
			path = path.replace("\"", "_");
			malformed_path = true;
		}
		if (path.find("\'") >= 0) {
			path = path.replace("\'", "_");
			malformed_path = true;
		}

		// add the prefix back
		if (prefix != "") {
			path = prefix + path;
		}
	}
};

#endif