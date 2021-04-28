#include "pck_dumper.h"
#include "bytecode/bytecode_versions.h"
#include "core/variant/variant_parser.h"
#include "modules/regex/regex.h"
#include "pcfg_loader.h"
#include "resource_loader_compat.h"
#include <core/io/file_access_encrypted.h>
#include <core/os/dir_access.h>
#include <core/os/file_access.h>
#include <core/os/os.h>
#include <core/version_generated.gen.h>

#include "core/crypto/crypto_core.h"
#include "gdre_settings.h"

bool PckDumper::_pck_file_check_md5(Ref<PackedFileInfo> &file) {
	Error err;
	FileAccess *pck_f = FileAccess::open(file->get_path(), FileAccess::READ, &err);
	ERR_FAIL_COND_V_MSG(!pck_f, false, "Can't open file '" + file->get_path() + "'.");

	// Loading an encrypted file automatically checks the md5
	if (file->is_encrypted()) {
		return true;
	}

	CryptoCore::MD5Context ctx;
	ctx.start();

	int64_t rq_size = file->get_size();
	uint8_t buf[32768];

	while (rq_size > 0) {
		int got = pck_f->get_buffer(buf, MIN(32768, rq_size));
		if (got > 0) {
			ctx.update(buf, got);
		}
		if (got < 4096)
			break;
		rq_size -= 32768;
	}

	unsigned char hash[16];
	ctx.finish(hash);

	auto p_md5 = file->get_md5();
	String file_md5;
	String saved_md5;
	String error_msg = "";

	bool md5_match = true;
	for (int j = 0; j < 16; j++) {
		md5_match &= (hash[j] == p_md5[j]);
		file_md5 += String::num_uint64(hash[j], 16);
		saved_md5 += String::num_uint64(p_md5[j], 16);
	}
	memdelete(pck_f);
	return md5_match;
}

Error PckDumper::load_pck(const String &p_path) {
	return GDRESettings::get_singleton()->load_pack(p_path);
}

Error PckDumper::check_md5_all_files() {
	Error err = OK;
	auto files = GDRESettings::get_singleton()->get_file_info_list();
	for (int i = 0; i < files.size(); i++) {
		files.write[i]->set_md5_match(_pck_file_check_md5(files.write[i]));
		if (files[i]->md5_passed) {
			print_line("Verified " + files[i]->path);
		} else {
			print_error("Checksum failed for " + files[i]->path);
			err = ERR_BUG;
		}
	}
	return err;
}

Error PckDumper::pck_dump_to_dir(const String &dir) {
	DirAccess *da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	auto files = GDRESettings::get_singleton()->get_file_info_list();
	Vector<uint8_t> key = get_key();
	if (!da) {
		return ERR_FILE_CANT_WRITE;
	}
	String failed_files;
	Error err;
	for (int i = 0; i < files.size(); i++) {
		FileAccess *pck_f = FileAccess::open(files.get(i)->get_path(), FileAccess::READ, &err);
		if (!pck_f) {
			failed_files += files.get(i)->get_path() + " (FileAccess error)\n";
			continue;
		}
		String target_name = dir.plus_file(files.get(i)->get_path().replace("res://", ""));
		da->make_dir_recursive(target_name.get_base_dir());
		FileAccess *fa = FileAccess::open(target_name, FileAccess::WRITE);
		if (!fa) {
			failed_files += files.get(i)->get_path() + " (FileWrite error)\n";
			memdelete(pck_f);
			continue;
		}

		int64_t rq_size = files.get(i)->get_size();
		uint8_t buf[16384];
		while (rq_size > 0) {
			int got = pck_f->get_buffer(buf, MIN(16384, rq_size));
			fa->store_buffer(buf, got);
			rq_size -= 16384;
		}
		memdelete(fa);
		memdelete(pck_f);
		print_line("Extracted " + target_name);
		if (target_name.get_file() == "engine.cfb" || target_name.get_file() == "project.binary") {
			ProjectConfigLoader *pcfgldr = memnew(ProjectConfigLoader);
			uint32_t ver_major = GDRESettings::get_singleton()->get_ver_major();
			uint32_t ver_minor = GDRESettings::get_singleton()->get_ver_minor();
			Error e1 = pcfgldr->load_cfb(target_name, ver_major, ver_minor);
			if (e1 != OK) {
				WARN_PRINT("Failed to load project file");
				memdelete(pcfgldr);
				continue;
			}
			Error e2 = pcfgldr->save_cfb(target_name.get_base_dir(), ver_major, ver_minor);
			if (e2 != OK) {
				WARN_PRINT("Failed to save project file");
			} else {
				print_line("Exported project file " + target_name);
			}
			memdelete(pcfgldr);
		}
	}
	memdelete(da);

	if (failed_files.length() > 0) {
		print_error("At least one error was detected while extracting pack!\n" + failed_files);
		//show_warning(failed_files, RTR("Read PCK"), RTR("At least one error was detected!"));
	} else {
		print_line("No errors detected!");
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

void PckDumper::set_key(const String &key) {
	GDRESettings::get_singleton()->set_encryption_key(key);
}

Vector<uint8_t> PckDumper::get_key() const {
	return GDRESettings::get_singleton()->get_encryption_key();
}

String PckDumper::get_key_str() const {
	return GDRESettings::get_singleton()->get_encryption_key_string();
}

int PckDumper::get_file_count() {
	return GDRESettings::get_singleton()->get_file_count();
}

void PckDumper::clear_data() {
	GDRESettings::get_singleton()->unload_pack();
}

Vector<String> PckDumper::get_loaded_files() {
	Vector<String> filenames;
	auto files = GDRESettings::get_singleton()->get_file_info_list();

	for (int i = 0; i < files.size(); i++) {
		String p = files.get(i)->get_path();
		filenames.push_back(files.get(i)->get_path());
	}
	return filenames;
}

String PckDumper::get_engine_version() {
	return GDRESettings::get_singleton()->get_version_string();
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
	ClassDB::bind_method(D_METHOD("clear_data"), &PckDumper::clear_data);
	//ClassDB::bind_method(D_METHOD("get_dumped_files"), &PckDumper::get_dumped_files);
}
