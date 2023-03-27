#include "pck_dumper.h"
#include "compat/resource_loader_compat.h"
#include "gdre_settings.h"

#include "core/crypto/crypto_core.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/file_access_encrypted.h"
#include "core/os/os.h"
#include "core/variant/variant_parser.h"
#include "core/version_generated.gen.h"
#include "modules/regex/regex.h"

const static Vector<uint8_t> empty_md5 = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };

bool PckDumper::_pck_file_check_md5(Ref<PackedFileInfo> &file) {
	// Loading an encrypted file automatically checks the md5
	if (file->is_encrypted()) {
		return true;
	}
	auto hash = FileAccess::get_md5(file->get_path());
	auto p_md5 = String::md5(file->get_md5().ptr());
	return hash == p_md5;
}

Error PckDumper::check_md5_all_files() {
	Vector<String> f;
	int ch = 0;
	return _check_md5_all_files(f, ch, nullptr);
}

Error PckDumper::_check_md5_all_files(Vector<String> &broken_files, int &checked_files, EditorProgressGDDC *pr) {
	String ext = GDRESettings::get_singleton()->get_pack_path().get_extension().to_lower();
	uint64_t last_progress_upd = OS::get_singleton()->get_ticks_usec();

	if (ext != "pck" && ext != "exe") {
		print_verbose("Not a pack file, skipping MD5 check...");
		return ERR_SKIP;
	}
	Error err = OK;
	auto files = GDRESettings::get_singleton()->get_file_info_list();
	int skipped_files = 0;
	for (int i = 0; i < files.size(); i++) {
		if (pr) {
			if (OS::get_singleton()->get_ticks_usec() - last_progress_upd > 20000) {
				last_progress_upd = OS::get_singleton()->get_ticks_usec();
				bool cancel = pr->step(files[i]->path, i, true);
				if (cancel) {
					return ERR_PRINTER_ON_FIRE;
				}
			}
		}
		if (files[i]->get_md5() == empty_md5) {
			print_verbose("Skipping MD5 check for " + files[i]->path + " (no MD5 hash found)");
			skipped_files++;
			continue;
		}
		files.write[i]->set_md5_match(_pck_file_check_md5(files.write[i]));
		if (files[i]->md5_passed) {
			print_verbose("Verified " + files[i]->path);
		} else {
			print_error("Checksum failed for " + files[i]->path);
			broken_files.push_back(files[i]->path);
			err = ERR_BUG;
		}
		checked_files++;
	}

	if (err) {
		print_error("At least one error was detected while verifying files in pack!\n");
		//show_warning(failed_files, RTR("Read PCK"), RTR("At least one error was detected!"));
	} else if (skipped_files > 0) {
		print_line("Verified " + itos(checked_files) + " files, " + itos(skipped_files) + " files skipped (MD5 hash entry was empty)");
		if (skipped_files == files.size()) {
			return ERR_SKIP;
		}
	} else {
		print_line("Verified " + itos(checked_files) + " files, no errors detected!");
		//show_warning(RTR("No errors detected."), RTR("Read PCK"), RTR("The operation completed successfully!"));
	}
	return err;
}
Error PckDumper::pck_dump_to_dir(const String &dir, const Vector<String> &files_to_extract = Vector<String>()) {
	String t;
	return _pck_dump_to_dir(dir, files_to_extract, nullptr, t);
}

Error PckDumper::_pck_dump_to_dir(
		const String &dir,
		const Vector<String> &files_to_extract,
		EditorProgressGDDC *pr,
		String &error_string) {
	ERR_FAIL_COND_V_MSG(!GDRESettings::get_singleton()->is_pack_loaded(), ERR_DOES_NOT_EXIST,
			"Pack not loaded!");
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	auto files = GDRESettings::get_singleton()->get_file_info_list();
	Vector<uint8_t> key = GDRESettings::get_singleton()->get_encryption_key();
	uint64_t last_progress_upd = OS::get_singleton()->get_ticks_usec();

	if (da.is_null()) {
		return ERR_FILE_CANT_WRITE;
	}
	int files_extracted = 0;
	Error err;
	for (int i = 0; i < files.size(); i++) {
		if (files_to_extract.size() && !files_to_extract.has(files.get(i)->get_path())) {
			continue;
		}
		if (pr) {
			if (OS::get_singleton()->get_ticks_usec() - last_progress_upd > 20000) {
				last_progress_upd = OS::get_singleton()->get_ticks_usec();
				bool cancel = pr->step(files.get(i)->get_path(), i, true);
				if (cancel) {
					return ERR_PRINTER_ON_FIRE;
				}
			}
		}
		Ref<FileAccess> pck_f = FileAccess::open(files.get(i)->get_path(), FileAccess::READ, &err);
		if (pck_f.is_null()) {
			error_string += files.get(i)->get_path() + " (FileAccess error)\n";
			continue;
		}
		String target_name = dir.path_join(files.get(i)->get_path().replace("res://", ""));
		da->make_dir_recursive(target_name.get_base_dir());
		Ref<FileAccess> fa = FileAccess::open(target_name, FileAccess::WRITE);
		if (fa.is_null()) {
			error_string += files.get(i)->get_path() + " (FileWrite error)\n";
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
		files_extracted++;
		print_verbose("Extracted " + target_name);
	}

	if (error_string.length() > 0) {
		print_error("At least one error was detected while extracting pack!\n" + error_string);
		//show_warning(failed_files, RTR("Read PCK"), RTR("At least one error was detected!"));
	} else {
		print_line("Extracted " + itos(files_extracted) + " files, no errors detected!");
		//show_warning(RTR("No errors detected."), RTR("Read PCK"), RTR("The operation completed successfully!"));
	}
	return OK;
}

void PckDumper::_bind_methods() {
	ClassDB::bind_method(D_METHOD("check_md5_all_files"), &PckDumper::check_md5_all_files);
	ClassDB::bind_method(D_METHOD("pck_dump_to_dir", "dir", "files_to_extract"), &PckDumper::pck_dump_to_dir, DEFVAL(Vector<String>()));
	//ClassDB::bind_method(D_METHOD("get_dumped_files"), &PckDumper::get_dumped_files);
}
