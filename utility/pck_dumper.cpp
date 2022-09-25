#include "pck_dumper.h"
#include "gdre_settings.h"
#include "pcfg_loader.h"
#include "resource_loader_compat.h"

#include "core/crypto/crypto_core.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/file_access_encrypted.h"
#include "core/os/os.h"
#include "core/variant/variant_parser.h"
#include "core/version_generated.gen.h"
#include "modules/regex/regex.h"

bool PckDumper::_pck_file_check_md5(Ref<PackedFileInfo> &file) {
	// Loading an encrypted file automatically checks the md5
	if (file->is_encrypted()) {
		return true;
	}
	auto hash = FileAccess::get_md5(file->get_path());
	auto p_md5 = String::md5(file->get_md5().ptr());
	return hash == p_md5;
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
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	auto files = GDRESettings::get_singleton()->get_file_info_list();
	Vector<uint8_t> key = get_key();
	if (da.is_null()) {
		return ERR_FILE_CANT_WRITE;
	}
	String failed_files;
	Error err;
	for (int i = 0; i < files.size(); i++) {
		Ref<FileAccess> pck_f = FileAccess::open(files.get(i)->get_path(), FileAccess::READ, &err);
		if (pck_f.is_null()) {
			failed_files += files.get(i)->get_path() + " (FileAccess error)\n";
			continue;
		}
		String target_name = dir.path_join(files.get(i)->get_path().replace("res://", ""));
		da->make_dir_recursive(target_name.get_base_dir());
		Ref<FileAccess> fa = FileAccess::open(target_name, FileAccess::WRITE);
		if (fa.is_null()) {
			failed_files += files.get(i)->get_path() + " (FileWrite error)\n";
			continue;
		}

		int64_t rq_size = files.get(i)->get_size();
		uint8_t buf[16384];
		while (rq_size > 0) {
			int got = pck_f->get_buffer(buf, MIN(16384, rq_size));
			fa->store_buffer(buf, got);
			rq_size -= 16384;
		}
		fa->flush();
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
	ClassDB::bind_method(D_METHOD("set_key"), &PckDumper::set_key);
	//ClassDB::bind_method(D_METHOD("get_dumped_files"), &PckDumper::get_dumped_files);
}
