#include "pck_info.h"
#include <core/os/file_access.h>
#include <core/os/dir_access.h>
#include <core/os/os.h>
#include <core/version_generated.gen.h>
#include "bytecode/bytecode_versions.h"
#include "pcfg_loader.h"
#include "core/variant/variant_parser.h"
#include "resource_loader_compat.h"
#include "modules/regex/regex.h"

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

bool PckDumper::_pck_file_check_md5(FileAccess *pck, const PackedFile & f) {
	size_t oldpos = pck->get_position();
	pck->seek(f.offset);

#if (VERSION_MAJOR == 4)
	CryptoCore::MD5Context ctx;
	ctx.start();
#else
	MD5_CTX md5;
	MD5Init(&md5);
#endif

	int64_t rq_size = f.size;
	uint8_t buf[32768];

	while (rq_size > 0) {

		int got = pck->get_buffer(buf, MIN(32768, rq_size));
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
	pck->seek(oldpos);

	String file_md5;
	String saved_md5;
	String error_msg = "";

	bool md5_match = true;
	for (int j = 0; j < 16; j++) {
#if (VERSION_MAJOR == 4)
		md5_match &= (hash[j] == f.md5[j]);
		file_md5 += String::num_uint64(hash[j], 16);
#else
		md5_match &= (md5.digest[j] == md5_saved[j]);
		file_md5 += String::num_uint64(md5.digest[j], 16);
#endif
		saved_md5 += String::num_uint64(f.md5[j], 16);
	}
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

	if (pck_ver > 1) {
		//show_warning(RTR("Pack version unsupported: ") + itos(pck_ver), RTR("Read PCK"));
		memdelete(pck);
		return ERR_FILE_UNRECOGNIZED;
	}
	ver_major = pck->get_32();
	ver_minor = pck->get_32();
	ver_rev = pck->get_32();
	//skip?
	for (int i = 0; i < 16; i++) {
		pck->get_32();
	}

	file_count = pck->get_32();
	DirAccess *da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	files.clear();
	for (int i = 0; i < file_count; i++) {
		uint32_t sl = pck->get_32();
		CharString cs;
		cs.resize(sl + 1);
		pck->get_buffer((uint8_t *)cs.ptr(), sl);
		cs[sl] = 0;

		String path;
		path.parse_utf8(cs.ptr());

		path = path.replace("res://", "");
		uint64_t ofs = pck->get_64();
		uint64_t size = pck->get_64();
		uint8_t md5_saved[16];
		pck->get_buffer(md5_saved, 16);
		PackedFile p_file = PackedFile(path, ofs, size, md5_saved);
		files.push_back(p_file);

	}
	loaded = true;
	memdelete(pck);
	return OK;
}

bool PckDumper::check_md5_all_files() {
	FileAccess *pck = FileAccess::open(path, FileAccess::READ);
	int bad_checksums = 0;
	for (int i = 0; i < files.size(); i++) {
		PackedFile p_file = files.get(i);
		p_file.md5_match = _pck_file_check_md5(pck, p_file);
		if (p_file.md5_match) {

		}
	}
	memdelete(pck);
	return true;
}

Error PckDumper::pck_dump_to_dir(const String &dir) {
	FileAccess *pck = FileAccess::open(path, FileAccess::READ);
	DirAccess *da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	if (!da) {
		return ERR_FILE_CANT_WRITE;
	}
	String failed_files;
	for (int i = 0; i < files.size(); i++) {
		String target_name = dir.plus_file(files.get(i).path);

		da->make_dir_recursive(target_name.get_base_dir());

		//print_warning("extracting " + files[i], RTR("Read PCK"));

		pck->seek(files.get(i).offset);

		FileAccess *fa = FileAccess::open(target_name, FileAccess::WRITE);
		if (fa) {
			int64_t rq_size = files.get(i).size;
			uint8_t buf[16384];

			while (rq_size > 0) {
				int got = pck->get_buffer(buf, MIN(16384, rq_size));
				fa->store_buffer(buf, got);
				rq_size -= 16384;
			}
			memdelete(fa);
		} else {
			failed_files += files.get(i).path + " (FileAccess error)\n";
		}
		if (target_name.get_file() == "engine.cfb" || target_name.get_file() == "project.binary") {
			ProjectConfigLoader pcfgldr;
			Error e1 = pcfgldr.load_cfb(target_name, ver_major, ver_minor);
			if (e1 == OK) {
				printf("good");
			}
			Error e2 = pcfgldr.save_cfb(target_name.get_base_dir(), ver_major, ver_minor);
			if (e2 == OK) {
				printf("good");
			}
		}
	}
	memdelete(da);
	memdelete(pck);

	if (failed_files.length() > 0) {
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

void PackedFile::set_stuff(const String path, const uint64_t ofs, const uint64_t sz, const uint8_t md5arr[16]) {
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

void PackedFile::fix_path() {
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

Vector<String> get_directory_listing3(const String dir, const Vector<String> &search_str, const String rel = "") {
	Vector<String> ret;
	DirAccess *da = DirAccess::open(dir.plus_file(rel));
	String base = "res://";
	if (!da) {
		return ret;
	}
	da->list_dir_begin();
	String f;
	while ((f = da->get_next()) != "") {
		if (f == "." || f == "..") {
			continue;
		} else if (da->current_is_dir()) {
			ret.append_array(get_directory_listing3(dir, search_str, rel.plus_file(f)));
		} else {
			if (search_str.size() > 0) {
				for (int i = 0; i < search_str.size(); i++) {
					if (f.get_file().find(search_str[i]) > -1) {
						ret.append(base.plus_file(rel).plus_file(f));
						break;
					}
				}
			} else {
				ret.append(base.plus_file(rel).plus_file(f));
			}
		}
	}
	memdelete(da);
	return ret;
}

Vector<String> get_directory_listing2(const String dir, const Vector<String> &filters, const String rel = ""){
	Vector<String> ret;
	DirAccess *da = DirAccess::open(dir.plus_file(rel));

	if (!da) {
		return ret;
	}
	da->list_dir_begin();
	String f;
	while ((f = da->get_next()) != "") {
		if (f == "." || f == ".."){
			continue;
		} else if (da->current_is_dir()){
			ret.append_array(get_directory_listing2(dir, filters, rel.plus_file(f)));
		} else {
			if (filters.size() > 0){
				for (int i = 0; i < filters.size(); i++){
					if (filters[i] == f.get_extension()){
						ret.append(dir.plus_file(rel).plus_file(f));
						break;
					}
				}
			} else {
				ret.append(dir.plus_file(rel).plus_file(f));
			}
			
		}
	}
	memdelete(da);
	return ret;
}

Vector<String> get_directory_listing2(const String dir){
	Vector<String> temp;
	return get_directory_listing2(dir, temp);
}



Array ImportExporter::get_import_files(){
	return files;
}

Error ImportExporter::load_v2_converted_file(const String& p_path) {
	String dest;
	String type;
	Vector<String> spl = p_path.get_file().split(".");
	//This is a converted file, no metadata
	if (p_path.get_file().find(".converted.") != -1) {
		
		if (spl.size() != 4) {
			// this doesn't match "filename.ext.converted.newext"
			return ERR_CANT_RESOLVE;
		}
		dest = p_path.get_base_dir().plus_file(spl[0] + "." + spl[1]);
	} else {
		// This is an import file, possibly has import metadata
		// Going to have to rewrite the import metadata in the resource file to make this work
		//ResourceFormatLoaderBinaryCompat rlc;
		//Dictionary metadata;
		//
		//if (rlc.get_v2_import_metadata(p_path, base_dir, metadata) == OK) {
		//	Dictionary iinfo;
		//	iinfo["path"] = p_path;
		//	//this is a relative path to this file
		//	iinfo["source_file"] = p_path.get_base_dir().plus_file(metadata["src"]);
		//	iinfo["type"] = metadata["type"];
		//	iinfo["dest_files"] = Vector<String>().push_back(p_path);
		//	iinfo["metadata"] = metadata;
		//	iinfo["group_file"] = "";
		//	iinfo["importer"] = "";
		//	files.push_back(iinfo);
		//	return OK;
		//}
		//No import metadata, assume that it's loaded from the assets dir in the base dir
		String new_ext;
		if (p_path.get_extension() == "tex") {
			new_ext = "png";
		} else if (p_path.get_extension() == "smp") {
			new_ext = "wav";
		}
		dest = String("assets").plus_file(p_path.get_base_dir().plus_file(spl[0] + "." + new_ext));
	}
	Dictionary iinfo;
	iinfo["path"] = p_path;
	iinfo["source_file"] = dest;
	if (p_path.get_extension() == "scn") {
		iinfo["type"] = "PackedScene";
	} else if (p_path.get_extension() == "res") {
		iinfo["type"] = "Resource";
	} else if (p_path.get_extension() == "tex") {
		iinfo["type"] = "ImageTexture";
	} else if (p_path.get_extension() == "smp") {
		iinfo["type"] = "Sample";
	} //Others??
	iinfo["dest_files"] = Vector<String>().push_back(p_path);
	iinfo["metadata"] = "";
	iinfo["group_file"] = "";
	iinfo["importer"] = "";
	files.push_back(iinfo);

	return OK;
}

Error ImportExporter::load_import_files(const String &dir, const uint32_t ver_major){
	base_dir = dir;
	Vector<String> filters;
	Vector<String> file_names;
	// We instead look for file names with ".converted." in the name
	// Like "filename.tscn.converted.scn"
	if (ver_major <= 2) {
		filters.push_back(".converted.");
		filters.push_back(".tex");
		file_names = get_directory_listing3(dir, filters);
		for (int i = 0; i < file_names.size(); i++) {
			if (load_v2_converted_file(file_names[i]) != OK) {
				WARN_PRINT("Can't load V2 converted file: " + file_names[i]);
			}
		}
	} else {
		filters.push_back("import");
		file_names = get_directory_listing2(dir, filters);
		for (int i = 0; i < file_names.size(); i++) {
			if (load_import_file(file_names[i]) != OK) {
				WARN_PRINT("Can't load import file: " + file_names[i]);
			}
		}
	}
	return OK;
}

Error ImportExporter::load_import_file(const String &p_path){
	Error err;
	FileAccess *f = FileAccess::open(p_path, FileAccess::READ, &err);
	//ImportInfo * i_info = memnew(ImportInfo);
	Dictionary iinfo;
	if (!f) {
		return err;
	}
	Dictionary thing;

	VariantParser::StreamFile stream;
	stream.f = f;

	String assign;
	Variant value;
	VariantParser::Tag next_tag;

	int lines = 0;
	String error_text;
	bool path_found = false; //first match must have priority
	while (true) {
		assign = Variant();
		next_tag.fields.clear();
		next_tag.name = String();

		err = VariantParser::parse_tag_assign_eof(&stream, lines, error_text, next_tag, assign, value, nullptr, true);
		if (err == ERR_FILE_EOF) {
			memdelete(f);
			return OK;
		} else if (err != OK) {
			ERR_PRINT("ResourceFormatImporter::load - " + p_path + ".import:" + itos(lines) + " error: " + error_text);
			memdelete(f);
			return err;
		}

		if (assign != String()) {
			if (!path_found && assign.begins_with("path.") && iinfo["path"] == String()) {
				String feature = assign.get_slicec('.', 1);
				if (OS::get_singleton()->has_feature(feature)) {
					iinfo["path"] = value;
					path_found = true; //first match must have priority
				}

			} else if (!path_found && assign == "path") {
				iinfo["path"] = value;
				path_found = true; //first match must have priority
			} else if (assign == "type") {
				iinfo["type"] = ClassDB::get_compatibility_remapped_class(value);
			} else if (assign == "importer") {
				iinfo["importer"] = value;
			} else if (assign == "group_file") {
				iinfo["group_file"] = value;
			} else if (assign == "metadata") {
				iinfo["metadata"] = value;
			} else if (assign == "source_file") {
				iinfo["source_file"] = value;
			} else if (assign == "dest_files") {
				iinfo["dest_files"] = value;
			}

		} else if (next_tag.name != "remap" && next_tag.name != "deps") {
			break;
		}
	}

	memdelete(f);

	if (iinfo["path"] == String() || iinfo["type"] == String()) {
		return ERR_FILE_CORRUPT;
	} else {
		files.push_back(iinfo);
	}
	return OK;
}

Error ImportExporter::convert_res_bin_2_txt(const String &output_dir, const String &p_path, const String &p_dst) {
	ResourceFormatLoaderBinaryCompat rlc;
	return rlc.convert_bin_to_txt(p_path, p_dst, output_dir);
}

Error ImportExporter::convert_v2tex_to_png(const String &output_dir, const String &p_path, const String &p_dst) {
	ResourceFormatLoaderBinaryCompat rlc;
	DirAccess *da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	String dst_dir = output_dir.plus_file(p_dst.get_base_dir());
	da->make_dir_recursive(dst_dir);
	memdelete(da);
	return rlc.convert_v2tex_to_png(p_path, p_dst, output_dir);
}

void ImportExporter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load_import_files"), &ImportExporter::load_import_files);
	ClassDB::bind_method(D_METHOD("get_import_files"), &ImportExporter::get_import_files);
	ClassDB::bind_method(D_METHOD("convert_res_bin_2_txt"), &ImportExporter::convert_res_bin_2_txt);
	ClassDB::bind_method(D_METHOD("convert_v2tex_to_png"), &ImportExporter::convert_v2tex_to_png);

	//ClassDB::bind_method(D_METHOD("get_dumped_files"), &PckDumper::get_dumped_files);
}
