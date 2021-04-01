/*************************************************************************/
/*  file_access_pack.cpp                                                 */
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

#include "gdre_packed_data.h"

#include "core/io/file_access_encrypted.h"
#include "core/object/script_language.h"
#include "core/version.h"

#include <stdio.h>

Error GDREPackedData::add_pack(const String &p_path, bool p_replace_files, size_t p_offset) {
	for (int i = 0; i < sources.size(); i++) {
		if (sources[i]->try_open_pack(p_path, p_replace_files, p_offset)) {
			return OK;
		}
	}
	return ERR_FILE_UNRECOGNIZED;
}

void GDREPackedData::add_path(const String &pkg_path, const String &path, uint64_t ofs, uint64_t size, const uint8_t *p_md5, PackSource *p_src, bool p_replace_files, bool p_encrypted) {
	//printf("adding path %s, %lli, %lli\n", path.utf8().get_data(), pmd5.a, pmd5.b);
	Ref<PackedFileInfo> pf_info;
	pf_info.instance();
	pf_info->init(pkg_path, path, ofs, size, p_md5, p_encrypted);
	// Store the corrected path, not the raw path
	PathMD5 pmd5(pf_info->path.md5_buffer());
	bool exists = files.has(pmd5);

	if (!exists || p_replace_files) {
		files[pmd5] = pf_info;
	} else{
		WARN_PRINT("Warning: not adding already existing file " + path);
	}

	if (!exists) {
		//search for dir
		String p = path.replace_first("res://", "");
		PackedDir *cd = root;

		if (p.find("/") != -1) { //in a subdir
			Vector<String> ds = p.get_base_dir().split("/");
			for (int j = 0; j < ds.size(); j++) {
				if (!cd->subdirs.has(ds[j])) {
					PackedDir *pd = memnew(PackedDir);
					pd->name = ds[j];
					pd->parent = cd;
					cd->subdirs[pd->name] = pd;
					cd = pd;
				} else {
					cd = cd->subdirs[ds[j]];
				}
			}
		}

		String filename = pf_info->path.get_file();
		// Don't add as a file if the path points to a directory
		if (!filename.is_empty()) {
			cd->files.insert(filename);
		}
	}
}

void GDREPackedData::add_pack_source(PackSource *p_source) {
	if (p_source != nullptr) {
		sources.push_back(p_source);
	}
}

GDREPackedData *GDREPackedData::singleton = nullptr;

GDREPackedData::GDREPackedData() {
	singleton = this;
	root = memnew(PackedDir);

	add_pack_source(memnew(GDREPackedSource));
}

void GDREPackedData::_free_packed_dirs(PackedDir *p_dir) {
	for (Map<String, PackedDir *>::Element *E = p_dir->subdirs.front(); E; E = E->next()) {
		_free_packed_dirs(E->get());
	}
	memdelete(p_dir);
}

GDREPackedData::~GDREPackedData() {
	for (int i = 0; i < sources.size(); i++) {
		memdelete(sources[i]);
	}
	clear();
}

void GDREPackedData::clear(){
	

	files.clear();
	_free_packed_dirs(root);
	reset_encryption_key();
}

void GDREPackedData::reset_encryption_key(){
	if (set_key){
		memcpy(script_encryption_key, old_key, 32);
		set_key = false;
		enc_key_str = "";
		enc_key.clear();
	}
}

void GDREPackedData::set_encryption_key(const String &key_str){

	String skey = key_str.replace_first("0x","");
	ERR_FAIL_COND_MSG(!skey.is_valid_hex_number(false) || skey.size() < 64, "not a valid key");

	Vector<uint8_t> key;
	key.resize(32);
	for (int i = 0; i < 32; i++) {
		int v = 0;
		if (i * 2 < skey.length()) {
			char32_t ct = skey.to_lower()[i * 2];
			if (ct >= '0' && ct <= '9')
				ct = ct - '0';
			else if (ct >= 'a' && ct <= 'f')
				ct = 10 + ct - 'a';
			v |= ct << 4;
		}

		if (i * 2 + 1 < skey.length()) {
			char32_t ct = skey.to_lower()[i * 2 + 1];
			if (ct >= '0' && ct <= '9')
				ct = ct - '0';
			else if (ct >= 'a' && ct <= 'f')
				ct = 10 + ct - 'a';
			v |= ct;
		}
		key.write[i] = v;
	}
	set_encryption_key(key);
}


void GDREPackedData::set_encryption_key(Vector<uint8_t> key) {
	ERR_FAIL_COND_MSG(key.size() < 32, "key size incorrect!");
	if (!set_key){
		memcpy(old_key, script_encryption_key, 32);
	}
	memcpy(script_encryption_key, key.ptr(), 32);
	set_key = true;
	enc_key = key;
	enc_key_str = String::hex_encode_buffer(key.ptr(), 32);
}

Vector<Ref<PackedFileInfo>> GDREPackedData::get_file_info_list(){
	Vector<Ref<PackedFileInfo>> ret;
	for (auto *E = files.front(); E; E = E->next()) {
		ret.push_back(E->get());
	}
	return ret;
}
//////////////////////////////////////////////////////////////////

bool GDREPackedSource::try_open_pack(const String &p_path, bool p_replace_files, size_t p_offset) {
	FileAccess *f = FileAccess::open(p_path, FileAccess::READ);
	if (!f) {
		return false;
	}

	f->seek(p_offset);

	uint32_t magic = f->get_32();

	if (magic != PACK_HEADER_MAGIC) {
		// loading with offset feature not supported for self contained exe files
		if (p_offset != 0) {
			f->close();
			memdelete(f);
			ERR_FAIL_V_MSG(false, "Loading self-contained executable with offset not supported.");
		}

		//maybe at the end.... self contained exe
		f->seek_end();
		f->seek(f->get_position() - 4);
		magic = f->get_32();
		if (magic != PACK_HEADER_MAGIC) {
			f->close();
			memdelete(f);
			return false;
		}
		f->seek(f->get_position() - 12);

		uint64_t ds = f->get_64();
		f->seek(f->get_position() - ds - 8);

		magic = f->get_32();
		if (magic != PACK_HEADER_MAGIC) {
			f->close();
			memdelete(f);
			return false;
		}
	}
    
	uint32_t version = f->get_32();
	uint32_t ver_major = f->get_32();
	uint32_t ver_minor = f->get_32();
	uint32_t ver_rev = f->get_32(); // patch number, not used for validation.

	if (version > PACK_FORMAT_VERSION) {
		f->close();
	 	memdelete(f);
	 	ERR_FAIL_V_MSG(false, "Pack version unsupported: " + itos(version) + ".");
	}

    uint32_t pack_flags = 0;
	uint64_t file_base = 0;
    
    if (version == 2){
        pack_flags = f->get_32();
	    file_base = f->get_64();
    }

	bool enc_directory = (pack_flags & PACK_DIR_ENCRYPTED);
	
	for (int i = 0; i < 16; i++) {
		//reserved
		f->get_32();
	}

	int file_count = f->get_32();

	if (enc_directory) {
		FileAccessEncrypted *fae = memnew(FileAccessEncrypted);
		if (!fae) {
			f->close();
			memdelete(f);
			ERR_FAIL_V_MSG(false, "Can't open encrypted pack directory.");
		}

		Vector<uint8_t> key;
		key.resize(32);
		for (int i = 0; i < key.size(); i++) {
			key.write[i] = script_encryption_key[i];
		}

		Error err = fae->open_and_parse(f, key, FileAccessEncrypted::MODE_READ, false);
		if (err) {
			f->close();
			memdelete(f);
			memdelete(fae);
			ERR_FAIL_V_MSG(false, "Can't open encrypted pack directory.");
		}
		f = fae;
	}

	// Everything worked, now set the data
	GDREPackedData::get_singleton()->pack_info = GDREPackedData::PackInfo::PackInfo(p_path, ver_major, ver_minor, ver_rev, version, pack_flags, file_base, file_count);

	for (int i = 0; i < file_count; i++) {
		uint32_t sl = f->get_32();
		CharString cs;
		cs.resize(sl + 1);
		f->get_buffer((uint8_t *)cs.ptr(), sl);
		cs[sl] = 0;

		String path;
		path.parse_utf8(cs.ptr());

		uint64_t ofs = file_base + f->get_64();
		uint64_t size = f->get_64();
		uint8_t md5[16];
		uint32_t flags = 0;
		f->get_buffer(md5, 16);
	    if (version == 2){
			flags = f->get_32();
		}
		GDREPackedData::get_singleton()->add_path(p_path, path, ofs + p_offset, size, md5, this, p_replace_files, (flags & PACK_FILE_ENCRYPTED));
	}

	f->close();
	memdelete(f);
	return true;
}

FileAccess *GDREPackedSource::get_file(const String &p_path, GDREPackedData::PackedFile *p_file) {
	return memnew(FileAccessPack(p_path, *p_file));
}

////////

void PackedFileInfo::fix_path() {
	path = raw_path;
	malformed_path = false;
	String prefix = "";

	if (path.begins_with("res://")){
		path = path.replace_first("res://", "");
		prefix = "res://";
	} else if (path.begins_with("local://")){
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
	if (prefix != ""){
		path = prefix + path;
	}
}


////
FileAccess *FileAccessGDRE::open(const String &p_path, int p_mode_flags, Error *r_error) {
	//try packed data first

	FileAccess *ret = nullptr;
	if (!(p_mode_flags & WRITE) && GDREPackedData::get_singleton() && !GDREPackedData::get_singleton()->is_disabled()) {
		ret = GDREPackedData::get_singleton()->try_open_path(p_path);
		if (ret) {
			if (r_error) {
				*r_error = OK;
			}
			return ret;
		}
	}

	ret = create_for_path(p_path);
	Error err;
	ret->open(p_path, p_mode_flags, &err);

	if (r_error) {
		*r_error = err;
	}
	if (err != OK) {
		memdelete(ret);
		ret = nullptr;
	}

	return ret;
}

bool FileAccessGDRE::exists(const String &p_name) {
	if (GDREPackedData::get_singleton() && !GDREPackedData::get_singleton()->is_disabled() && GDREPackedData::get_singleton()->has_path(p_name)) {
		return true;
	}

	FileAccess *f = open(p_name, READ);
	if (!f) {
		return false;
	}
	memdelete(f);
	return true;
}


Error DirAccessGDRE::list_dir_begin() {
	list_dirs.clear();
	list_files.clear();

	for (Map<String, GDREPackedData::PackedDir *>::Element *E = current->subdirs.front(); E; E = E->next()) {
		list_dirs.push_back(E->key());
	}

	for (Set<String>::Element *E = current->files.front(); E; E = E->next()) {
		list_files.push_back(E->get());
	}

	return OK;
}

String DirAccessGDRE::get_next() {
	if (list_dirs.size()) {
		cdir = true;
		String d = list_dirs.front()->get();
		list_dirs.pop_front();
		return d;
	} else if (list_files.size()) {
		cdir = false;
		String f = list_files.front()->get();
		list_files.pop_front();
		return f;
	} else {
		return String();
	}
}

bool DirAccessGDRE::current_is_dir() const {
	return cdir;
}

bool DirAccessGDRE::current_is_hidden() const {
	return false;
}

void DirAccessGDRE::list_dir_end() {
	list_dirs.clear();
	list_files.clear();
}

int DirAccessGDRE::get_drive_count() {
	return 0;
}

String DirAccessGDRE::get_drive(int p_drive) {
	return "";
}

GDREPackedData::PackedDir *DirAccessGDRE::_find_dir(String p_dir) {
	String nd = p_dir.replace("\\", "/");

	// Special handling since simplify_path() will forbid it
	if (p_dir == "..") {
		return current->parent;
	}

	bool absolute = false;
	if (nd.begins_with("res://")) {
		nd = nd.replace_first("res://", "");
		absolute = true;
	}

	nd = nd.simplify_path();

	if (nd == "") {
		nd = ".";
	}

	if (nd.begins_with("/")) {
		nd = nd.replace_first("/", "");
		absolute = true;
	}

	Vector<String> paths = nd.split("/");

	GDREPackedData::PackedDir *pd;

	if (absolute) {
		pd = GDREPackedData::get_singleton()->root;
	} else {
		pd = current;
	}

	for (int i = 0; i < paths.size(); i++) {
		String p = paths[i];
		if (p == ".") {
			continue;
		} else if (p == "..") {
			if (pd->parent) {
				pd = pd->parent;
			}
		} else if (pd->subdirs.has(p)) {
			pd = pd->subdirs[p];

		} else {
			return nullptr;
		}
	}

	return pd;
}

Error DirAccessGDRE::change_dir(String p_dir) {
	GDREPackedData::PackedDir *pd = _find_dir(p_dir);
	if (pd) {
		current = pd;
		return OK;
	} else {
		return ERR_INVALID_PARAMETER;
	}
}

String DirAccessGDRE::get_current_dir(bool p_include_drive) {
	GDREPackedData::PackedDir *pd = current;
	String p = current->name;

	while (pd->parent) {
		pd = pd->parent;
		p = pd->name.plus_file(p);
	}

	return "res://" + p;
}

bool DirAccessGDRE::file_exists(String p_file) {
	p_file = fix_path(p_file);

	GDREPackedData::PackedDir *pd = _find_dir(p_file.get_base_dir());
	if (!pd) {
		return false;
	}
	return pd->files.has(p_file.get_file());
}

bool DirAccessGDRE::dir_exists(String p_dir) {
	p_dir = fix_path(p_dir);

	return _find_dir(p_dir) != nullptr;
}

Error DirAccessGDRE::make_dir(String p_dir) {
	return ERR_UNAVAILABLE;
}

Error DirAccessGDRE::rename(String p_from, String p_to) {
	return ERR_UNAVAILABLE;
}

Error DirAccessGDRE::remove(String p_name) {
	return ERR_UNAVAILABLE;
}

size_t DirAccessGDRE::get_space_left() {
	return 0;
}

String DirAccessGDRE::get_filesystem_type() const {
	return "PCK";
}

DirAccessGDRE::DirAccessGDRE() {
	current = GDREPackedData::get_singleton()->root;
}

DirAccess *DirAccessGDRE::open(const String &p_path, Error *r_error) {
	DirAccess *da;
	if (GDREPackedData::get_singleton() && !GDREPackedData::get_singleton()->is_disabled()){
		da = GDREPackedData::get_singleton()->try_open_directory(p_path);
		if (da){
			return da;
		}
	}
	da = create_for_path(p_path);

	ERR_FAIL_COND_V_MSG(!da, nullptr, "Cannot create DirAccess for path '" + p_path + "'.");
	Error err = da->change_dir(p_path);
	if (r_error) {
		*r_error = err;
	}
	if (err != OK) {
		memdelete(da);
		return nullptr;
	}

	return da;
}
