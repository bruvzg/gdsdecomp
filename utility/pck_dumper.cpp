#include "pck_dumper.h"
#include <core/os/file_access.h>
#include <core/io/file_access_encrypted.h>
#include <core/os/dir_access.h>
#include <core/os/os.h>
#include <core/version_generated.gen.h>
#include "bytecode/bytecode_versions.h"
#include "pcfg_loader.h"
#include "core/variant/variant_parser.h"
#include "resource_loader_compat.h"
#include "modules/regex/regex.h"
#include "file_access_pack_compat.h"
#if (VERSION_MAJOR == 4)
#include "core/crypto/crypto_core.h"
#else
#include "thirdparty/misc/md5.h"
#include "thirdparty/misc/sha256.h"
#endif

bool PckDumper::_get_magic_number(FileAccess * pck) {
	uint32_t magic = pck->get_32();

	if (magic != 0x43504447) {
		//maybe at he end.... self contained exe
		pck->seek_end();
		pck->seek(pck->get_position() - 4);
		magic = pck->get_32();
		if (magic != 0x43504447) {
			return false;
		}
		pck->seek(pck->get_position() - 12);

		uint64_t ds = pck->get_64();
		pck->seek(pck->get_position() - ds - 8);

		magic = pck->get_32();
		if (magic != 0x43504447) {
			return false;
		}
	}
	return true;
}

bool PckDumper::_pck_file_check_md5(PackedFileInfo & file) {
	FileAccess *pck_f = get_file_access(file.path, &file);
	if (!pck_f){
		return false;
	}
	// Loading an encrypted file automatically checks the md5
	if (file.encrypted){
		return true;
	}
#if (VERSION_MAJOR == 4)
	CryptoCore::MD5Context ctx;
	ctx.start();
#else
	MD5_CTX md5;
	MD5Init(&md5);
#endif

	int64_t rq_size = file.size;
	uint8_t buf[32768];

	while (rq_size > 0) {

		int got = pck_f->get_buffer(buf, MIN(32768, rq_size));
		if (got > 0) {
#if (VERSION_MAJOR == 4)
			ctx.update(buf, got);
#else
			MD5Update(&md5, buf, got);
#endif
		}
		if (got < 4096)
			break;
		rq_size -= 32768;
	}

#if (VERSION_MAJOR == 4)
	unsigned char hash[16];
	ctx.finish(hash);
#else
	MD5Final(&md5);
#endif

	String file_md5;
	String saved_md5;
	String error_msg = "";

	bool md5_match = true;
	for (int j = 0; j < 16; j++) {
#if (VERSION_MAJOR == 4)
		md5_match &= (hash[j] == file.md5[j]);
		file_md5 += String::num_uint64(hash[j], 16);
#else
		md5_match &= (md5.digest[j] == md5_saved[j]);
		file_md5 += String::num_uint64(md5.digest[j], 16);
#endif
		saved_md5 += String::num_uint64(file.md5[j], 16);
	}
	pck_f->close();
	memdelete(pck_f);
	return md5_match;
}

Error PckDumper::load_pck(const String& p_path) {
	path = p_path;
	FileAccess *pck = FileAccess::open(p_path, FileAccess::READ);
	if (!pck) {
		//printf("Error opening PCK file: " + p_path + "Read PCK");
		return ERR_FILE_NOT_FOUND;
	}

	if (!_get_magic_number(pck)) {
		memdelete(pck);
		return ERR_FILE_CORRUPT;
	}

	String failed_files;
	pck_ver = pck->get_32();

	if (pck_ver > 2) {
		//show_warning(RTR("Pack version unsupported: ") + itos(pck_ver), RTR("Read PCK"));
		memdelete(pck);
		return ERR_FILE_UNRECOGNIZED;
	}
	
	ver_major = pck->get_32();
	ver_minor = pck->get_32();
	ver_rev = pck->get_32();
	
	if (pck_ver == 2) {
		pack_flags = pck->get_32();
		file_base = pck->get_64();
	}

	bool enc_directory = (pack_flags & (1 << 0));
	//skip reserved fields
	for (int i = 0; i < 16; i++) {
		pck->get_32();
	}

	file_count = pck->get_32();
	Vector<uint8_t> key = get_key();
	if (enc_directory) {
		FileAccessEncrypted *fae = memnew(FileAccessEncrypted);
		if (!fae) {
			pck->close();
			memdelete(pck);
			ERR_FAIL_V_MSG(ERR_FILE_CANT_OPEN, "Can't open encrypted pack directory.");
		}
		Error err = fae->open_and_parse(pck, key, FileAccessEncrypted::MODE_READ, false);
		if (err) {
			pck->close();
			memdelete(pck);
			memdelete(fae);
			ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, "Can't open encrypted pack directory.");
		}
		pck = fae;
	}

	files.clear();
	for (int i = 0; i < file_count; i++) {
		uint32_t sl = pck->get_32();
		CharString cs;
		cs.resize(sl + 1);
		pck->get_buffer((uint8_t *)cs.ptr(), sl);
		cs[sl] = 0;

		String f_path;
		f_path.parse_utf8(cs.ptr());

		f_path = f_path.replace("res://", "");
		uint64_t ofs = pck->get_64();
		uint64_t size = pck->get_64();
		uint8_t md5_saved[16];
		pck->get_buffer(md5_saved, 16);
		uint32_t flags = 0;
		if (pck_ver == 2) {
			flags = pck->get_32();
		}
		PackedFileInfo p_file = PackedFileInfo(p_path, f_path, ofs, size, md5_saved, flags);
		files.push_back(p_file);
	}
	loaded = true;
	memdelete(pck);
	return OK;
}

bool PckDumper::check_md5_all_files() {
	int bad_checksums = 0;
	for (int i = 0; i < files.size(); i++) {
		files.write[i].md5_match = _pck_file_check_md5(files.get(i));
	}
	return true;
}

Error PckDumper::pck_dump_to_dir(const String &dir) {
	DirAccess *da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	Vector<uint8_t> key = get_key();
	if (!da) {
		return ERR_FILE_CANT_WRITE;
	}
	String failed_files;
	for (int i = 0; i < files.size(); i++) {
		FileAccess *pck_f = get_file_access(files.get(i).path, &files.get(i));
		if (!pck_f){
			failed_files += files.get(i).path + " (FileAccess error)\n";
			continue;
		}
		String target_name = dir.plus_file(files.get(i).path);
		da->make_dir_recursive(target_name.get_base_dir());
		FileAccess *fa = FileAccess::open(target_name, FileAccess::WRITE);
		if (!fa) {
			failed_files += files.get(i).path + " (FileWrite error)\n";
			memdelete(pck_f);
			continue;
		}

		int64_t rq_size = files.get(i).size;
		uint8_t buf[16384];
		while (rq_size > 0) {
			int got = pck_f->get_buffer(buf, MIN(16384, rq_size));
			fa->store_buffer(buf, got);
			rq_size -= 16384;
		}
		memdelete(fa);
		memdelete(pck_f);
		if (target_name.get_file() == "engine.cfb" || target_name.get_file() == "project.binary") {
			ProjectConfigLoader * pcfgldr = memnew(ProjectConfigLoader);

			Error e1 = pcfgldr->load_cfb(target_name, ver_major, ver_minor);
			if (e1 != OK) {
				WARN_PRINT("Failed to load cfb");
				memdelete(pcfgldr);
				continue;
			}
			Error e2 = pcfgldr->save_cfb(target_name.get_base_dir(), ver_major, ver_minor);
			if (e2 != OK) {
				WARN_PRINT("Failed to save cfb");
			}
			memdelete(pcfgldr);
		}
	}
	memdelete(da);

	if (failed_files.length() > 0) {
		print_error(failed_files);
		//show_warning(failed_files, RTR("Read PCK"), RTR("At least one error was detected!"));
	} else {
		//show_warning(RTR("No errors detected."), RTR("Read PCK"), RTR("The operation completed successfully!"));
	}
	return OK;
}


Error PckDumper::pck_load_and_dump(const String &p_path, const String &dir) {
	Error result = load_pck(p_path);
	if (result != OK) {
		return result;
	}
	check_md5_all_files();
	result = pck_dump_to_dir(dir);
	if (result != OK) {
		return result;
	}
	return OK;
}

bool PckDumper::is_loaded() {
	return loaded;
}

void PckDumper::set_key(const String &s){
	script_key = s;
}

FileAccess *PckDumper::get_file_access(const String &p_path, PackedFileInfo *p_file){
	return memnew(FileAccessPackCompat(p_path, p_file->to_struct(), get_key()));
}

Vector<uint8_t> PckDumper::get_key() const {

	Vector<uint8_t> key;
	key.resize(32);

	if (script_key.is_empty() || !script_key.is_valid_hex_number(false) || script_key.length() != 64) {
		return key;
	}

	for (int i = 0; i < 32; i++) {
		int v = 0;
		if (i * 2 < script_key.length()) {
			char32_t ct = script_key.to_lower()[i * 2];
			if (ct >= '0' && ct <= '9')
				ct = ct - '0';
			else if (ct >= 'a' && ct <= 'f')
				ct = 10 + ct - 'a';
			v |= ct << 4;
		}

		if (i * 2 + 1 < script_key.length()) {
			char32_t ct = script_key.to_lower()[i * 2 + 1];
			if (ct >= '0' && ct <= '9')
				ct = ct - '0';
			else if (ct >= 'a' && ct <= 'f')
				ct = 10 + ct - 'a';
			v |= ct;
		}
		key.write[i] = v;
	}
	return key;
}


void PackedFileInfo::set_stuff(const String path, const uint64_t ofs, const uint64_t sz, const uint8_t md5arr[16]) {
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

void PackedFileInfo::fix_path() {
	path = raw_path;
	malformed_path = false;
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
}

int PckDumper::get_file_count(){
	return file_count;
}

Vector<String> PckDumper::get_loaded_files(){
	Vector<String> filenames;
	for (int i = 0; i < files.size(); i++){
		filenames.push_back(files[i].path);
	}
	return filenames;
}

String PckDumper::get_engine_version(){
	return String(itos(ver_major) + "." + itos(ver_minor) + "." +itos(ver_rev));
}

void PckDumper::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load_pck"), &PckDumper::load_pck);
	ClassDB::bind_method(D_METHOD("check_md5_all_files"), &PckDumper::check_md5_all_files);
	ClassDB::bind_method(D_METHOD("pck_dump_to_dir"), &PckDumper::pck_dump_to_dir);
	ClassDB::bind_method(D_METHOD("pck_load_and_dump"), &PckDumper::pck_load_and_dump);
	ClassDB::bind_method(D_METHOD("is_loaded"), &PckDumper::is_loaded);
	ClassDB::bind_method(D_METHOD("get_file_count"), &PckDumper::get_file_count);
	ClassDB::bind_method(D_METHOD("get_loaded_files"), &PckDumper::get_loaded_files);
	ClassDB::bind_method(D_METHOD("get_engine_version"), &PckDumper::get_engine_version);
	//ClassDB::bind_method(D_METHOD("get_dumped_files"), &PckDumper::get_dumped_files);
}
