
#include "import_exporter.h"
#include "bytecode/bytecode_tester.h"
#include "bytecode/bytecode_versions.h"
#include "compat/oggstr_loader_compat.h"
#include "compat/optimized_translation_extractor.h"
#include "compat/resource_loader_compat.h"
#include "compat/sample_loader_compat.h"
#include "compat/texture_loader_compat.h"
#include "core/error/error_list.h"
#include "core/error/error_macros.h"
#include "core/io/resource_loader.h"
#include "core/object/class_db.h"
#include "core/string/print_string.h"
#include "exporters/oggstr_exporter.h"
#include "exporters/sample_exporter.h"
#include "exporters/scene_exporter.h"
#include "exporters/texture_exporter.h"
#include "gdre_settings.h"
#include "pcfg_loader.h"
#include "scene/resources/packed_scene.h"
#include "util_functions.h"

#include "core/crypto/crypto_core.h"
#include "core/io/config_file.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/math/audio_frame.h"
#include "core/os/os.h"
#include "core/string/optimized_translation.h"
#include "core/variant/variant_parser.h"
#include "core/version_generated.gen.h"
#include "modules/gltf/gltf_document.h"
#include "modules/minimp3/audio_stream_mp3.h"
#include "modules/regex/regex.h"
#include "modules/vorbis/audio_stream_ogg_vorbis.h"
#include "scene/resources/audio_stream_wav.h"
#include "scene/resources/font.h"
#include "thirdparty/minimp3/minimp3_ex.h"
#include "utility/glob.h"

GDRESettings *get_settings() {
	return GDRESettings::get_singleton();
}

int get_ver_major() {
	return get_settings()->get_ver_major();
}
int get_ver_minor() {
	return get_settings()->get_ver_minor();
}
int get_ver_rev() {
	return get_settings()->get_ver_rev();
}

// export all the imported resources
Error ImportExporter::export_imports(const String &p_out_dir, const Vector<String> &files_to_export) {
	String t;
	return _export_imports(p_out_dir, files_to_export, nullptr, t);
}

Ref<ImportExporterReport> ImportExporter::get_report() {
	return report;
}

/**
Sort the scenes so that they are exported last
 */
struct IInfoComparator {
	int is_glb_scene(const Ref<ImportInfo> &a) const {
		return a->get_importer() == "scene" && !a->is_auto_converted() ? 1 : 0;
	}
	bool operator()(const Ref<ImportInfo> &a, const Ref<ImportInfo> &b) const {
		return is_glb_scene(a) < is_glb_scene(b);
	}
};

HashSet<String> vector_to_hashset(const Vector<String> &vec) {
	HashSet<String> ret;
	for (int i = 0; i < vec.size(); i++) {
		ret.insert(vec[i]);
	}
	return ret;
}

Vector<String> hashset_to_vector(const HashSet<String> &hs) {
	Vector<String> ret;
	for (auto &s : hs) {
		ret.push_back(s);
	}
	return ret;
}

bool vectors_intersect(const Vector<String> &a, const Vector<String> &b) {
	const Vector<String> &bigger = a.size() > b.size() ? a : b;
	const Vector<String> &smaller = a.size() > b.size() ? b : a;
	for (int i = 0; i < smaller.size(); i++) {
		if (bigger.has(a[i])) {
			return true;
		}
	}
	return false;
}
// Error remove_remap(const String &src, const String &dst, const String &output_dir);
Error ImportExporter::handle_auto_converted_file(const String &autoconverted_file, const String &output_dir) {
	String prefix = autoconverted_file.replace_first("res://", "");
	if (!prefix.begins_with(".")) {
		String old_path = output_dir.path_join(prefix);
		if (FileAccess::exists(old_path)) {
			String new_path = output_dir.path_join(".autoconverted").path_join(prefix);
			Error err = ensure_dir(new_path.get_base_dir());
			ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to create directory for remap " + new_path);
			Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
			ERR_FAIL_COND_V_MSG(da.is_null(), ERR_CANT_CREATE, "Failed to create directory for remap " + new_path);
			return da->rename(old_path, new_path);
		}
	}
	return OK;
}

Error ImportExporter::remove_remap_and_autoconverted(const String &source_file, const String &autoconverted_file, const String &output_dir) {
	if (get_settings()->has_remap(source_file, autoconverted_file)) {
		Error err = get_settings()->remove_remap(source_file, autoconverted_file, output_dir);
		if (err != ERR_FILE_NOT_FOUND) {
			ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to remove remap for " + source_file + " -> " + autoconverted_file);
		}
		return handle_auto_converted_file(autoconverted_file, output_dir);
	}
	return OK;
}

void ImportExporter::_do_export(uint32_t i, ExportToken *tokens) {
	// Taken care of in the main thread
	if (unlikely(cancelled)) {
		return;
	}
	tokens[i].report = Exporter::export_resource(tokens[i].output_dir, tokens[i].iinfo);
	last_completed++;
}

Error ImportExporter::_export_imports(const String &p_out_dir, const Vector<String> &files_to_export, EditorProgressGDDC *pr, String &error_string) {
	reset_log();
	ResourceCompatLoader::make_globally_available();
	ResourceCompatLoader::set_default_gltf_load(true);
	report = Ref<ImportExporterReport>(memnew(ImportExporterReport(get_settings()->get_version_string())));
	report->log_file_location = get_settings()->get_log_file_path();
	ERR_FAIL_COND_V_MSG(!get_settings()->is_pack_loaded(), ERR_DOES_NOT_EXIST, "pack/dir not loaded!");
	uint64_t last_progress_upd = OS::get_singleton()->get_ticks_usec();
	String output_dir = !p_out_dir.is_empty() ? p_out_dir : get_settings()->get_project_path();
	Error err = OK;
	if (opt_lossy) {
		WARN_PRINT_ONCE("Converting lossy imports, you may lose fidelity for indicated assets when re-importing upon loading the project");
	}
	// TODO: make this use "copy"
	Array _files = get_settings()->get_import_files();
	Ref<DirAccess> dir = DirAccess::open(output_dir);
	bool partial_export = files_to_export.size() > 0;
	report->session_files_total = partial_export ? files_to_export.size() : _files.size();
	if (opt_decompile) {
		if (pr) {
			pr->step("Decompiling scripts...", 0, true);
		}
		Vector<String> code_files = get_settings()->get_code_files();
		Vector<String> to_decompile;
		if (files_to_export.size() > 0) {
			for (int i = 0; i < code_files.size(); i++) {
				if (files_to_export.has(code_files[i])) {
					to_decompile.append(code_files[i]);
				}
			}
		} else {
			to_decompile = code_files;
		}

		// check if res://addons exists
		Ref<DirAccess> res_da = DirAccess::open("res://");
		Vector<String> addon_first_level_dirs = Glob::glob("res://addons/*", true);
		if (res_da->dir_exists("res://addons")) {
			// Only recreate plugin configs if we are exporting files within the addons directory
			if (files_to_export.size() != 0) {
				Vector<String> addon_code_files = Glob::rglob_list({ "res://addons/**/*.gdc", "res://addons/**/*.gde", "res://addons/**/*.gd" });
				addon_first_level_dirs = Glob::dirs_in_names(files_to_export, addon_first_level_dirs);
				auto new_code_files = Glob::names_in_dirs(addon_code_files, addon_first_level_dirs);
				for (auto &code_file : new_code_files) {
					if (!to_decompile.has(code_file)) {
						to_decompile.push_back(code_file);
					}
				}
			}
			// we need to copy the addons to the output directory
		}
		if (to_decompile.size() > 0) {
			decompile_scripts(output_dir, to_decompile);
			// This only works if we decompile the scripts first
			recreate_plugin_configs(output_dir, addon_first_level_dirs);
		}
	}
	if (pr) {
		if (pr->step("Exporting resources...", 0, true)) {
			return ERR_PRINTER_ON_FIRE;
		}
	}
	HashMap<String, Ref<ResourceExporter>> exporter_map;
	for (int i = 0; i < Exporter::exporter_count; i++) {
		Ref<ResourceExporter> exporter = Exporter::exporters[i];
		List<String> handled_importers;
		exporter->get_handled_importers(&handled_importers);
		for (const String &importer : handled_importers) {
			exporter_map[importer] = exporter;
		}
	}
	Vector<ExportToken> tokens;
	Vector<ExportToken> non_multithreaded_tokens;
	Vector<String> paths_to_export;
	paths_to_export.resize(_files.size());
	for (int i = 0; i < _files.size(); i++) {
		Ref<ImportInfo> iinfo = _files[i];
		if (partial_export) {
			auto dest_files = iinfo->get_dest_files();
			bool has_path = false;
			for (auto dest : dest_files) {
				if (files_to_export.has(dest)) {
					has_path = true;
					break;
				}
			}
			if (!has_path) {
				continue;
			}
		}
		String importer = iinfo->get_importer();
		if (importer == "script_bytecode") {
			continue;
		}
		// ***** Set export destination *****
		iinfo->set_export_dest(iinfo->get_source_file());
		// This is a Godot asset that was imported outside of project directory
		if (!iinfo->get_source_file().begins_with("res://")) {
			if (get_ver_major() <= 2) {
				// import_md_path is the resource path in v2
				iinfo->set_export_dest(String("res://.assets").path_join(iinfo->get_import_md_path().get_base_dir().path_join(iinfo->get_source_file().get_file()).replace("res://", "")));
			} else {
				// import_md_path is the .import/.remap path in v3-v4
				iinfo->set_export_dest(iinfo->get_import_md_path().get_basename());
				// If the source_file path was not actually in the project structure, save it elsewhere
				if (iinfo->get_source_file().find(iinfo->get_export_dest().replace("res://", "")) == -1) {
					iinfo->set_export_dest(iinfo->get_export_dest().replace("res://", "res://.assets"));
				}
			}
		}
		bool supports_multithreading = opt_multi_thread;
		if (exporter_map.has(iinfo->get_importer())) {
			if (!exporter_map.get(iinfo->get_importer())->supports_multithread()) {
				supports_multithreading = false;
			}
		} else {
			supports_multithreading = false;
		}
		paths_to_export.write[i] = iinfo->get_path();
		if (supports_multithreading) {
			tokens.push_back({ iinfo, nullptr, output_dir, supports_multithreading });
		} else {
			non_multithreaded_tokens.push_back({ iinfo, nullptr, output_dir, supports_multithreading });
		}
	}
	int64_t num_multithreaded_tokens = tokens.size();
	paths_to_export.resize(tokens.size());
	// ***** Export resources *****
	if (opt_multi_thread) {
		last_completed = -1;
		cancelled = false;
		print_line("Exporting resources in parallel...");
		WorkerThreadPool::GroupID group_task = WorkerThreadPool::get_singleton()->add_template_group_task(
				this,
				&ImportExporter::_do_export,
				tokens.ptrw(),
				tokens.size(), -1, true, SNAME("ImportExporter::export_imports"));
		if (pr) {
			while (!WorkerThreadPool::get_singleton()->is_group_task_completed(group_task)) {
				OS::get_singleton()->delay_usec(10000);
				int i = last_completed;
				if (i < 0) {
					i = 0;
				} else if (i >= num_multithreaded_tokens) {
					i = num_multithreaded_tokens - 1;
				}
				bool cancel = pr->step(paths_to_export[i], i, true);
				if (cancel) {
					cancelled = true;
					WorkerThreadPool::get_singleton()->wait_for_group_task_completion(group_task);
					return ERR_PRINTER_ON_FIRE;
				}
			}
		}
		// Always wait for completion; otherwise we leak memory.
		WorkerThreadPool::get_singleton()->wait_for_group_task_completion(group_task);
	}
	String last_path = tokens[tokens.size() - 1].iinfo->get_path();
	for (int i = 0; i < non_multithreaded_tokens.size(); i++) {
		ExportToken &token = non_multithreaded_tokens.write[i];
		Ref<ImportInfo> iinfo = token.iinfo;
		if (pr) {
			if (OS::get_singleton()->get_ticks_usec() - last_progress_upd > 10000) {
				last_progress_upd = OS::get_singleton()->get_ticks_usec();
				if (pr->step(iinfo->get_path(), num_multithreaded_tokens + i, false)) {
					return ERR_PRINTER_ON_FIRE;
				}
			}
		}
		String importer = iinfo->get_importer();
		if (!exporter_map.has(importer)) {
			token.report = memnew(ExportReport(iinfo));
			token.report->set_message("No exporter found for importer " + importer + " and type: " + iinfo->get_type());
			token.report->set_error(ERR_UNAVAILABLE);
		} else {
			token.report = exporter_map.get(importer)->export_resource(output_dir, iinfo);
		}
	}
	tokens.append_array(non_multithreaded_tokens);
	if (pr) {
		pr->step("Finalizing...", tokens.size() - 1, true);
	}
	for (int i = 0; i < tokens.size(); i++) {
		const ExportToken &token = tokens[i];
		Ref<ImportInfo> iinfo = token.iinfo;
		String importer = iinfo->get_importer();
		if (importer == "script_bytecode") {
			continue;
		}
		String path = iinfo->get_path();
		String source = iinfo->get_source_file();
		String type = iinfo->get_type();
		auto loss_type = iinfo->get_import_loss_type();
		String src_ext = iinfo->get_source_file().get_extension().to_lower();
		Ref<ExportReport> ret = token.report;
		if (ret.is_null()) {
			ERR_PRINT("Exporter returned null report for " + path);
			report->failed.push_back(iinfo);
			continue;
		}
		err = ret->get_error();
		if (err == ERR_SKIP) {
			report->not_converted.push_back(iinfo);
			if (iinfo->get_ver_major() >= 4 && iinfo->is_dirty()) {
				iinfo->save_to(output_dir.path_join(iinfo->get_import_md_path().replace("res://", "")));
			}
			continue;
		}
		if (err == ERR_UNAVAILABLE) {
			String type = iinfo->get_type();
			String format_type = src_ext;
			if (ret->get_unsupported_format_type() != "") {
				format_type = ret->get_unsupported_format_type();
			}
			if (iinfo->get_ver_major() >= 4 && iinfo->is_dirty()) {
				iinfo->save_to(output_dir.path_join(iinfo->get_import_md_path().replace("res://", "")));
			}
			report_unsupported_resource(type, format_type, path);
			report->not_converted.push_back(iinfo);
			continue;
		}
		if (importer == "scene" && src_ext != "escn" && src_ext != "tscn") {
			report->exported_scenes = true;
		}

		// ****REWRITE METADATA****
		bool should_rewrite_metadata = !iinfo->get_source_file().begins_with("res://");
		if (err == OK && iinfo->is_import() && (should_rewrite_metadata || ret->get_source_path() != ret->get_new_source_path())) {
			if (iinfo->get_ver_major() <= 2 && opt_rewrite_imd_v2) {
				// TODO: handle v2 imports with more than one source, like atlas textures
				err = rewrite_import_source(ret->get_new_source_path(), output_dir, iinfo);
			} else if (iinfo->get_ver_major() >= 3 && opt_rewrite_imd_v3 && (iinfo->get_source_file().find(iinfo->get_export_dest().replace("res://", "")) != -1)) {
				// Currently, we only rewrite the import data for v3 if the source file was somehow recorded as an absolute file path,
				// But is still in the project structure
				err = rewrite_import_source(ret->get_new_source_path(), output_dir, iinfo);
			} else if (iinfo->is_dirty()) {
				err = iinfo->save_to(output_dir.path_join(iinfo->get_import_md_path().replace("res://", "")));
				if (iinfo->get_export_dest() != iinfo->get_source_file()) {
					// we still report this as unwritten so that we can report to the user that it was saved to a non-original path
					err = ERR_PRINTER_ON_FIRE;
				}
			} else {
				err = ERR_PRINTER_ON_FIRE;
			}
			// if we didn't rewrite the metadata, the err will still be ERR_PRINTER_ON_FIRE
			// if we failed, it won't be OK
			if (err != OK) {
				if (err == ERR_PRINTER_ON_FIRE) {
					print_line("Did not rewrite import metadata for " + iinfo->get_source_file());
				} else {
					print_line("Failed to rewrite import metadata for " + iinfo->get_source_file());
				}
				err = ERR_DATABASE_CANT_WRITE;
			} else {
				// we successfully rewrote the import data
				err = ERR_PRINTER_ON_FIRE;
			}
			// saved to non-original path, but we exporter knows we can't rewrite the metadata
		} else if (err == ERR_DATABASE_CANT_WRITE) {
			print_line("Did not rewrite import metadata for " + iinfo->get_source_file());
		} else if (err == OK && iinfo->is_dirty()) {
			err = iinfo->save_to(output_dir.path_join(iinfo->get_import_md_path().replace("res://", "")));
			if (err == OK) {
				err = ERR_PRINTER_ON_FIRE;
			} else {
				print_line("Failed to rewrite import metadata for " + iinfo->get_source_file());
				err = ERR_DATABASE_CANT_WRITE;
			}
		}

		// write md5 files
		if (opt_write_md5_files && iinfo->is_import() && (err == OK || err == ERR_PRINTER_ON_FIRE) && get_ver_major() > 2) {
			err = ((Ref<ImportInfoModern>)iinfo)->save_md5_file(output_dir);
			if (err && err != ERR_PRINTER_ON_FIRE) {
				err = ERR_LINK_FAILED;
			} else {
				err = OK;
			}
		}
		// ***** Record export result *****

		// the following are successful exports, but we failed to rewrite metadata or write md5 files
		// we had to rewrite the import metadata
		if (err == ERR_PRINTER_ON_FIRE) {
			report->rewrote_metadata.push_back(iinfo);
			err = OK;
			// necessary to rewrite import metadata but failed
		} else if (err == ERR_DATABASE_CANT_WRITE) {
			report->failed_rewrite_md.push_back(iinfo);
			err = OK;
		} else if (err == ERR_LINK_FAILED) {
			report->failed_rewrite_md5.push_back(iinfo);
			err = OK;
		}

		if (err == ERR_UNAVAILABLE) {
			// already reported in exporters below
			report->not_converted.push_back(iinfo);
		} else if (err != OK) {
			report->failed.push_back(iinfo);
			print_line("Failed to convert " + type + " resource " + path);
		} else {
			if (ret->get_loss_type() != ImportInfo::LOSSLESS) {
				report->lossy_imports.push_back(iinfo);
			}
			report->success.push_back(iinfo);
		}
		// remove remaps
		if (!err && get_settings()->has_any_remaps()) {
			remove_remap_and_autoconverted(iinfo->get_export_dest(), iinfo->get_path(), output_dir);
		}
	}

	if (get_settings()->is_project_config_loaded()) { // some pcks do not have project configs
		if (get_settings()->save_project_config(output_dir) != OK) {
			print_line("ERROR: Failed to save project config!");
		} else {
			// Remove binary project config, as editors will load from it instead of the text one
			dir->remove(get_settings()->get_project_config_path().get_file());
		}
	}
	report->print_report();
	ResourceCompatLoader::set_default_gltf_load(false);
	ResourceCompatLoader::unmake_globally_available();
	return OK;
}

Error ImportExporter::decompile_scripts(const String &p_out_dir, const Vector<String> &to_decompile) {
	GDScriptDecomp *decomp;
	// we have to remove remaps if they exist
	bool has_remaps = get_settings()->has_any_remaps();
	Vector<String> code_files = to_decompile;
	if (code_files.is_empty()) {
		code_files = get_settings()->get_code_files();
	}
	if (code_files.is_empty()) {
		return OK;
	}
	uint64_t revision = 0;

	// we don't consider the dev versions here, only the release/beta versions
	switch (get_ver_major()) {
		case 1:
			switch (get_ver_minor()) {
				case 0:
					revision = 0xe82dc40;
					break;
				case 1:
					revision = 0x65d48d6;
					break;
			}
			break;
		case 2:
			switch (get_ver_minor()) {
				case 0:
					revision = 0x23441ec;
					break;
				case 1: {
					switch (get_settings()->get_ver_rev()) {
						case 0:
						case 1:
							revision = 0x7124599;
							break;
						case 2:
							revision = 0x85585c7;
							break;
						case 3:
						case 4:
						case 5:
						case 6:
							revision = 0xed80f45;
							break;
					}
				} break;
			}
			break;
		case 3:
			switch (get_ver_minor()) {
				case 0:
					revision = 0x54a2ac;
					break;
				case 1: {
					// They broke compatibility for 3.1.x in successive patch releases.
					if (get_settings()->get_version_string().contains("beta")) {
						revision = 0x1ca61a3;
					} else {
						switch (get_settings()->get_ver_rev()) {
							case 0:
								revision = 0x1a36141;
								break;
							case 1:
								revision = 0x514a3fb;
								break;
						}
					}
				} break;
				case 2:
				case 3:
				case 4:
					revision = 0x5565f55;
					break;
				case 5:
					revision = 0xa7aad78;
					break;
				default:
					// We do not anticipate further GSScript changes in 3.x because GDScript 2.0 in 4.x can't be backported
					// but just in case...
					WARN_PRINT("Unsupported version 3." + itos(get_ver_minor()) + "." + itos(get_ver_rev()) + " of Godot detected");
					WARN_PRINT("If your scripts are mangled, please report this on the Github issue tracker");
					revision = 0xa7aad78;
					break;
			}
			break;
		case 4:
			switch (get_ver_minor()) {
				// Compiled mode was removed in 4.0; if this is part of a 4.0 project, might be in one of the pre GDScript 2.0 4.0-dev bytecodes
				// but we can't really detect which mutually incompatible revision it is (there are several), so we just fail
				case 0:
					ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED, "Support for Godot 4.0 pre-release dev GDScript not implemented, failed to decompile");
					break;
				case 1: // might be added back in 4.1; if so, this will need to be updated
					ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED, "Support for Godot 4.1 GDScript not yet implemented, failed to decompile");
					break;
				case 2:
				case 3:
				case 4:
					revision = 0x77af6ca;
					break;
				default:
					ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED, "Unsupported version 4." + itos(get_ver_minor()) + "." + itos(get_ver_rev()) + " of Godot detected, failed to decompile");
					break;
			}
			break;
		default:
			ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED, "Unknown version, failed to decompile");
	}
	if (revision > 0) {
		decomp = create_decomp_for_commit(revision);
	} else {
		ERR_FAIL_V_MSG(ERR_FILE_UNRECOGNIZED, "Unknown version, failed to decompile");
	}

	print_line("Script version " + get_settings()->get_version_string() + " (rev 0x" + String::num_int64(revision, 16) + ") detected");
	Error err;
	for (String f : code_files) {
		String dest_file = f.replace(".gdc", ".gd").replace(".gde", ".gd");
		Ref<DirAccess> da = DirAccess::open(p_out_dir);
		print_verbose("decompiling " + f);
		bool encrypted = false;
		if (f.get_extension().to_lower() == "gde") {
			encrypted = true;
			err = decomp->decompile_byte_code_encrypted(f, get_settings()->get_encryption_key());
		} else {
			err = decomp->decompile_byte_code(f);
		}
		if (err) {
			String err_string = decomp->get_error_message();
			memdelete(decomp);
			report->failed_scripts.push_back(f);
			// TODO: make it not fail hard on the first script that fails to decompile
			if (encrypted) {
				report->had_encryption_error = true;
				ERR_FAIL_V_MSG(err, "error decompiling encrypted script " + f + ": " + err_string);
			} else {
				ERR_FAIL_V_MSG(err, "error decompiling " + f + ": " + err_string);
			}
		} else {
			String text = decomp->get_script_text();
			String out_path = p_out_dir.path_join(dest_file.replace("res://", ""));
			Ref<FileAccess> fa = FileAccess::open(out_path, FileAccess::WRITE);
			if (fa.is_null()) {
				report->failed_scripts.push_back(f);
				memdelete(decomp);
				ERR_FAIL_V_MSG(ERR_FILE_CANT_WRITE, "error failed to save " + f);
			}
			fa->store_string(text);
			if (has_remaps && get_settings()->has_remap(dest_file, f)) {
				remove_remap_and_autoconverted(dest_file, f, p_out_dir);
			} else {
				handle_auto_converted_file(f, p_out_dir);
			}
			// TODO: make "remove_remap" do this instead
			if (da->file_exists(f.replace(".gdc", ".gd.remap").replace("res://", ""))) {
				da->remove(f.replace(".gdc", ".gd.remap").replace("res://", ""));
			}
			print_verbose("successfully decompiled " + f);
			report->decompiled_scripts.push_back(f);
		}
	}
	memdelete(decomp);
	// save changed config file
	if (get_settings()->is_project_config_loaded()) { // some game pcks do not have project configs
		err = get_settings()->save_project_config(p_out_dir);
	}

	if (err) {
		WARN_PRINT("Failed to save changed project config!");
	}
	return OK;
}

Error ImportExporter::recreate_plugin_config(const String &output_dir, const String &plugin_dir) {
	Error err;
	static const Vector<String> wildcards = { "*.gd" };
	String rel_plugin_path = String("addons").path_join(plugin_dir);
	String abs_plugin_path = output_dir.path_join(rel_plugin_path);
	auto gd_scripts = gdreutil::get_recursive_dir_list(abs_plugin_path, wildcards, false);
	String main_script;

	auto copy_gdext_dll_func = [&](String gdextension_path) -> Error {
		bool is_gdnative = gdextension_path.ends_with(".gdnlib");
		String lib_section_name = is_gdnative ? "entry" : "libraries";
		// check if directory; we can't do anything here if we're working from an unpacked dir
		if (get_settings()->get_pack_type() == GDRESettings::PackInfo::PackType::DIR) {
			return OK;
		}
		String parent_dir = get_settings()->get_pack_path().get_base_dir();
		// open gdextension_path like a config file
		Vector<String> libs_to_find;
		List<String> platforms;
		Dictionary lib_to_parentdir;
		Dictionary lib_to_platform;
		// what the decomp is running on
		String our_platform = OS::get_singleton()->get_name().to_lower();
		Ref<ConfigFile> cf;
		cf.instantiate();
		cf->load(gdextension_path);
		if (!cf->has_section(lib_section_name)) {
			if (cf->has_section("entry")) {
				lib_section_name = "entry";
			} else if (cf->has_section("libraries")) {
				lib_section_name = "libraries";
			} else {
				WARN_PRINT("Failed to find library section in " + gdextension_path);
				return ERR_BUG;
			}
		}
		cf->get_section_keys(lib_section_name, &platforms);
		auto add_path_func = [&](String path, String platform) mutable {
			String plugin_pardir = path.get_base_dir();
			if (!plugin_pardir.is_absolute_path()) {
				plugin_pardir = rel_plugin_path.path_join(plugin_pardir);
			} else {
				plugin_pardir = plugin_pardir.replace("res://", "");
			}
			libs_to_find.push_back(path.get_file());
			lib_to_parentdir[path.get_file()] = plugin_pardir;
			auto splits = platform.split(".");
			if (!splits.is_empty()) {
				platform = splits[0];
			}
			platform = platform.to_lower();
			// normalize old platform names
			if (platform == "x11") {
				platform = "linux";
			} else if (platform == "osx") {
				platform = "macos";
			}
			lib_to_platform[path.get_file()] = platform;
		};
		for (String &platform : platforms) {
			add_path_func(cf->get_value(lib_section_name, platform), platform);
		}
		platforms.clear();
		if (cf->has_section("dependencies")) {
			cf->get_section_keys("dependencies", &platforms);
		}
		for (String &platform : platforms) {
			Array keys;
			if (!is_gdnative) {
				Dictionary val = cf->get_value("dependencies", platform);
				// the keys in the dict are the library names
				keys = val.keys();
			} else {
				// they're just arrays in gdnlib
				keys = cf->get_value("dependencies", platform);
			}
			for (int i = 0; i < keys.size(); i++) {
				add_path_func(keys[i], platform);
			}
		}
		// search for the libraries
		Vector<String> lib_paths;
		// auto paths = gdreutil::get_recursive_dir_list(parent_dir, libs_to_find, true);

		// get a non-recursive dir listing
		{
			Ref<DirAccess> da = DirAccess::open(parent_dir, &err);
			ERR_FAIL_COND_V_MSG(err, ERR_FILE_CANT_OPEN, "Failed to open directory " + parent_dir);
			da->list_dir_begin();
			String f = da->get_next();
			while (!f.is_empty()) {
				if (f != "." && f != "..") {
					if (libs_to_find.has(f)) {
						lib_paths.push_back(parent_dir.path_join(f));
					}
				}
				f = da->get_next();
			}
		}
		if (lib_paths.size() == 0) {
			// if we're on MacOS, try one path up
			if (parent_dir.get_file() == "Resources") {
				parent_dir = parent_dir.get_base_dir();
				lib_paths = gdreutil::get_recursive_dir_list(parent_dir, libs_to_find, true);
			}
			if (lib_paths.size() == 0) {
				WARN_PRINT("Failed to find gdextension libraries for plugin " + plugin_dir);
				return ERR_BUG;
			}
		}
		// copy all of the libraries to the plugin directory
		Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
		bool found_our_platform = false;
		for (String path : lib_paths) {
			String lib_name = path.get_file();
			String lib_dir_path = output_dir.path_join(lib_to_parentdir[lib_name]);
			if (!da->dir_exists(lib_dir_path)) {
				ERR_FAIL_COND_V_MSG(da->make_dir_recursive(lib_dir_path), ERR_FILE_CANT_WRITE, "Failed to make plugin directory " + lib_dir_path);
			}
			String dest_path = lib_dir_path.path_join(lib_name);
			ERR_FAIL_COND_V_MSG(da->copy(path, dest_path), ERR_FILE_CANT_WRITE, "Failed to copy library " + path + " to " + dest_path);
			if (lib_to_platform[lib_name] == our_platform) {
				found_our_platform = true;
			}
		}
		if (!found_our_platform) {
			WARN_PRINT("Failed to find library for our platform for plugin " + plugin_dir);
			return ERR_PRINTER_ON_FIRE;
		}
		return OK;
	};
	bool found_our_platform = true;
	if (get_ver_major() >= 3) {
		// check if this is a gdextension/gdnative plugin
		static const Vector<String> gdext_wildcards = { "*.gdextension", "*.gdnlib" };
		auto gdexts = gdreutil::get_recursive_dir_list(abs_plugin_path, gdext_wildcards, true);
		for (String gdext : gdexts) {
			err = copy_gdext_dll_func(gdext);
			if (err) {
				found_our_platform = false;
			}
		}
		// gdextension/gdnative plugins may not have a main script, and no plugin.cfg
		if (gd_scripts.is_empty()) {
			return found_our_platform ? OK : ERR_PRINTER_ON_FIRE;
		}
	}

	for (int j = 0; j < gd_scripts.size(); j++) {
		String gd_script_abs_path = abs_plugin_path.path_join(gd_scripts[j]);
		String gd_text = FileAccess::get_file_as_string(gd_script_abs_path, &err);
		ERR_FAIL_COND_V_MSG(err, err, "failed to open gd_script " + gd_script_abs_path + "!");
		if (gd_text.find("extends EditorPlugin") != -1) {
			main_script = gd_scripts[j];
			break;
		}
	}
	ERR_FAIL_COND_V_MSG(main_script == "", ERR_FILE_NOT_FOUND, "Failed to find main script for plugin " + plugin_dir + "!");
	String plugin_cfg_text = String("[plugin]\n\n") +
			"name=\"" + plugin_dir.replace("_", " ").replace(".", " ") + "\"\n" +
			"description=\"" + plugin_dir.replace("_", " ").replace(".", " ") + " plugin\"\n" +
			"author=\"Unknown\"\n" +
			"version=\"1.0\"\n" +
			"script=\"" + main_script + "\"";
	Ref<FileAccess> f = FileAccess::open(abs_plugin_path.path_join("plugin.cfg"), FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err, err, "can't open plugin.cfg for writing");
	f->store_string(plugin_cfg_text);
	print_line("Recreated plugin config for " + plugin_dir);
	return found_our_platform ? OK : ERR_PRINTER_ON_FIRE;
}

// Recreates the "plugin.cfg" files for each plugin to avoid loading errors.
Error ImportExporter::recreate_plugin_configs(const String &output_dir, const Vector<String> &plugin_dirs) {
	Error err;
	if (!DirAccess::exists(output_dir.path_join("addons"))) {
		return OK;
	}
	Vector<String> dirs;
	if (plugin_dirs.is_empty()) {
		dirs = Glob::glob("res://addons/*", true);
	} else {
		dirs = plugin_dirs;
		for (int i = 0; i < dirs.size(); i++) {
			if (!dirs[i].is_absolute_path()) {
				dirs.write[i] = String("res://addons/").path_join(dirs[i]);
			}
		}
	}
	for (int i = 0; i < dirs.size(); i++) {
		String path = dirs[i];
		if (!DirAccess::dir_exists_absolute(path)) {
			continue;
		}
		String dir = dirs[i].get_file();
		if (dir.contains("godotsteam")) {
			report->godotsteam_detected = true;
		}
		err = recreate_plugin_config(output_dir, dir);
		if (err == ERR_PRINTER_ON_FIRE) {
			// we successfully copied the dlls, but failed to find one for our platform
			WARN_PRINT("Failed to find library for this platform for plugin " + dir);
			report->failed_gdnative_copy.push_back(dir);
		} else if (err) {
			WARN_PRINT("Failed to recreate plugin.cfg for " + dir);
			report->failed_plugin_cfg_create.push_back(dir);
		}
	}
	return OK;
}

// Godot v3-v4 import data rewriting
// TODO: We have to rewrite the resources to remap to the new destination
// However, we currently only rewrite the import data if the source file was recorded as an absolute file path,
// but is still in the project directory structure, which means no resource rewriting is necessary
Error ImportExporter::rewrite_import_source(const String &rel_dest_path, const String &output_dir, const Ref<ImportInfo> &iinfo) {
	String new_source = rel_dest_path;
	String new_import_file = output_dir.path_join(iinfo->get_import_md_path().replace("res://", ""));
	String abs_file_path = GDRESettings::get_singleton()->globalize_path(new_source, output_dir);
	Array new_dest_files;
	Ref<ImportInfo> new_import = ImportInfo::copy(iinfo);
	new_import->set_source_and_md5(new_source, FileAccess::get_md5(abs_file_path));
	Error err = ensure_dir(new_import_file.get_base_dir());
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to create directory for import metadata " + new_import_file);
	return new_import->save_to(new_import_file);
}

Error ImportExporter::ensure_dir(const String &dst_dir) {
	Error err;
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	ERR_FAIL_COND_V(da.is_null(), ERR_FILE_CANT_OPEN);
	err = da->make_dir_recursive(dst_dir);
	return err;
}

Error ImportExporter::convert_res_txt_2_bin(const String &output_dir, const String &p_path, const String &p_dst) {
	String dest = output_dir.path_join(p_dst.replace("res://", ""));
	return ResourceCompatLoader::to_binary(p_path, dest);
}

Error ImportExporter::convert_res_bin_2_txt(const String &output_dir, const String &p_path, const String &p_dst) {
	String dest = output_dir.path_join(p_dst.replace("res://", ""));
	return ResourceCompatLoader::to_text(p_path, dest);
}

Error ImportExporter::convert_tex_to_png(const String &output_dir, const String &p_path, const String &p_dst) {
	String src_path = _get_path(output_dir, p_path);
	String dst_path = output_dir.path_join(p_dst.replace("res://", ""));
	TextureExporter te;
	return te.export_file(dst_path, src_path);
}

String ImportExporter::_get_path(const String &output_dir, const String &p_path) {
	if (get_settings()->get_project_path() == "" && !get_settings()->is_pack_loaded()) {
		if (p_path.is_absolute_path()) {
			return p_path;
		} else {
			return output_dir.path_join(p_path.replace("res://", ""));
		}
	}
	if (get_settings()->has_res_path(p_path)) {
		return get_settings()->get_res_path(p_path);
	} else if (get_settings()->has_res_path(p_path, output_dir)) {
		return get_settings()->get_res_path(p_path, output_dir);
	} else {
		return output_dir.path_join(p_path.replace("res://", ""));
	}
}

void ImportExporter::report_unsupported_resource(const String &type, const String &format_name, const String &import_path, bool suppress_warn, bool suppress_print) {
	String type_format_str = type + "%" + format_name.to_lower();
	if (report->unsupported_types.find(type_format_str) == -1) {
		WARN_PRINT("Conversion for Resource of type " + type + " and format " + format_name + " not implemented");
		report->unsupported_types.push_back(type_format_str);
	}
	if (!suppress_print)
		print_line("Did not convert " + type + " resource " + import_path);
}

Error ImportExporter::convert_sample_to_wav(const String &output_dir, const String &p_path, const String &p_dst) {
	String src_path = _get_path(output_dir, p_path);
	String dst_path = output_dir.path_join(p_dst.replace("res://", ""));
	Error err;
	SampleExporter se;
	err = se.export_file(dst_path, src_path);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not export " + p_dst);

	print_verbose("Converted " + src_path + " to " + dst_path);
	return OK;
}

Error ImportExporter::convert_oggstr_to_ogg(const String &output_dir, const String &p_path, const String &p_dst) {
	String src_path = _get_path(output_dir, p_path);
	String dst_path = output_dir.path_join(p_dst.replace("res://", ""));
	Error err;
	OggStrExporter oslc;
	err = oslc.export_file(dst_path, src_path);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not export oggstr file " + p_path);
	print_verbose("Converted " + src_path + " to " + dst_path);
	return OK;
}

Error ImportExporter::convert_mp3str_to_mp3(const String &output_dir, const String &p_path, const String &p_dst) {
	String src_path = _get_path(output_dir, p_path);
	String dst_path = output_dir.path_join(p_dst.replace("res://", ""));
	Error err;

	Ref<AudioStreamMP3> sample = ResourceCompatLoader::non_global_load(src_path, "", &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not load mp3str file " + p_path);

	err = ensure_dir(dst_path.get_base_dir());
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to create dirs for " + dst_path);

	Ref<FileAccess> f = FileAccess::open(dst_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not open " + p_dst + " for saving");

	PackedByteArray data = sample->get_data();
	f->store_buffer(data.ptr(), data.size());

	print_verbose("Converted " + src_path + " to " + dst_path);
	return OK;
}

void ImportExporter::set_multi_thread(bool p_enable) {
	opt_multi_thread = p_enable;
}

void ImportExporter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("decompile_scripts", "output_dir", "files_to_decompile"), &ImportExporter::decompile_scripts, DEFVAL(PackedStringArray()));
	ClassDB::bind_method(D_METHOD("export_imports"), &ImportExporter::export_imports, DEFVAL(""), DEFVAL(PackedStringArray()));
	ClassDB::bind_method(D_METHOD("convert_res_txt_2_bin"), &ImportExporter::convert_res_txt_2_bin);
	ClassDB::bind_method(D_METHOD("convert_res_bin_2_txt"), &ImportExporter::convert_res_bin_2_txt);
	ClassDB::bind_method(D_METHOD("convert_tex_to_png"), &ImportExporter::convert_tex_to_png);
	ClassDB::bind_method(D_METHOD("convert_sample_to_wav"), &ImportExporter::convert_sample_to_wav);
	ClassDB::bind_method(D_METHOD("convert_oggstr_to_ogg"), &ImportExporter::convert_oggstr_to_ogg);
	ClassDB::bind_method(D_METHOD("convert_mp3str_to_mp3"), &ImportExporter::convert_mp3str_to_mp3);
	ClassDB::bind_method(D_METHOD("get_report"), &ImportExporter::get_report);
	ClassDB::bind_method(D_METHOD("set_multi_thread", "p_enable"), &ImportExporter::set_multi_thread);
	ClassDB::bind_method(D_METHOD("reset"), &ImportExporter::reset);
}

void ImportExporter::reset_log() {
	report = Ref<ImportExporterReport>(memnew(ImportExporterReport));
}

void ImportExporter::reset() {
	opt_bin2text = true;
	opt_export_textures = true;
	opt_export_samples = true;
	opt_export_ogg = true;
	opt_export_mp3 = true;
	opt_lossy = true;
	opt_export_jpg = true;
	opt_export_webp = true;
	opt_rewrite_imd_v2 = true;
	opt_rewrite_imd_v3 = true;
	opt_decompile = true;
	opt_only_decompile = false;
	reset_log();
}

ImportExporter::ImportExporter() {
	reset_log();
}
ImportExporter::~ImportExporter() {
	reset();
}

void ImportExporterReport::set_ver(String ver) {
	this->ver = GodotVer::parse(ver);
}

String ImportExporterReport::get_ver() {
	return ver->as_text();
}

Dictionary ImportExporterReport::get_totals() {
	Dictionary totals;
	totals["total"] = decompiled_scripts.size() + failed_scripts.size() + lossy_imports.size() + rewrote_metadata.size() + failed_rewrite_md.size() + failed_rewrite_md5.size() + failed.size() + success.size() + not_converted.size() + failed_plugin_cfg_create.size() + failed_gdnative_copy.size() + unsupported_types.size();
	totals["decompiled_scripts"] = decompiled_scripts.size();
	totals["success"] = success.size();
	totals["failed"] = failed.size();
	totals["not_converted"] = not_converted.size();
	totals["failed_scripts"] = failed_scripts.size();
	totals["lossy_imports"] = lossy_imports.size();
	totals["rewrote_metadata"] = rewrote_metadata.size();
	totals["failed_rewrite_md"] = failed_rewrite_md.size();
	totals["failed_rewrite_md5"] = failed_rewrite_md5.size();

	totals["failed_plugin_cfg_create"] = failed_plugin_cfg_create.size();
	totals["failed_gdnative_copy"] = failed_gdnative_copy.size();
	totals["unsupported_types"] = unsupported_types.size();
	return totals;
}

Dictionary ImportExporterReport::get_unsupported_types() {
	Dictionary unsupported;
	for (int i = 0; i < unsupported_types.size(); i++) {
		auto split = unsupported_types[i].split("%");
		unsupported[i] = unsupported_types[i];
	}
	return unsupported;
}

Dictionary ImportExporterReport::get_session_notes() {
	Dictionary notes;
	String unsup = get_detected_unsupported_resource_string();
	if (!unsup.is_empty()) {
		Dictionary unsupported;
		unsupported["title"] = "Unsupported Resources Detected";
		String message = "The following resource types were detected in the project that conversion is not implemented for yet.\n";
		message += "See Export Report to see which resources were not exported.\n";
		message += "You will still be able to edit the project in the editor regardless.";
		unsupported["message"] = message;
		PackedStringArray list;
		for (int i = 0; i < unsupported_types.size(); i++) {
			auto split = unsupported_types[i].split("%");
			list.push_back("Resource Type: " + split[0] + ", Format: " + split[1]);
		}
		unsupported["details"] = list;
		notes["unsupported_types"] = unsupported;
	}
	if (exported_scenes) {
		Dictionary export_scenes_note;
		export_scenes_note["title"] = "Experimental Scene Export";
		export_scenes_note["message"] = "Scene export is EXPERIMENTAL and exported scenes may be inaccurate.\n"
										"Thus, all exported scenes have been saved to the .assets directory, which will not be picked up by the editor for import.\n"
										"Please report any issues you encounter with exported scenes to the Github page Github.";
		notes["export_scenes"] = export_scenes_note;
	}

	if (had_encryption_error) {
		// notes["encryption_error"] = "Failed to decompile encrypted scripts!\nSet the correct key and try again!";
		Dictionary encryption_error;
		encryption_error["title"] = "Encryption Error";
		encryption_error["message"] = "Failed to decompile encrypted scripts!\nSet the correct key and try again!";
		encryption_error["details"] = PackedStringArray();
		notes["encryption_error"] = encryption_error;
	}

	if (!translation_export_message.is_empty()) {
		// notes["translation_export_message"] = translation_export_message;
		Dictionary translation_export;
		translation_export["title"] = "Translation Export Incomplete";
		translation_export["message"] = translation_export_message;
		translation_export["details"] = PackedStringArray();
		notes["translation_export_message"] = translation_export;
	}

	if (!failed_gdnative_copy.is_empty() || !failed_plugin_cfg_create.is_empty()) {
		Dictionary failed_plugins;
		failed_plugins["title"] = "Incomplete Plugin Export";
		String message = "The following addons failed to have their libraries copied or plugin.cfg regenerated\n";
		message += "You may encounter editor errors due to this.\n";
		message += "Tip: Try finding the plugin in the Godot Asset Library or Github.\n";
		failed_plugins["message"] = message;
		PackedStringArray list;
		for (int i = 0; i < failed_gdnative_copy.size(); i++) {
			list.push_back(failed_gdnative_copy[i]);
		}
		for (int i = 0; i < failed_plugin_cfg_create.size(); i++) {
			list.push_back(failed_plugin_cfg_create[i]);
		}
		failed_plugins["details"] = list;
		notes["failed_plugins"] = failed_plugins;
	}
	if (ver->get_major() == 2) {
		// Godot 2.x's assets are all exported to .assets
		Dictionary godot_2_assets;
		godot_2_assets["title"] = "Godot 2.x Assets";
		godot_2_assets["message"] = "All exported assets can be found in the '.assets' directory in the project folder.";
		godot_2_assets["details"] = PackedStringArray();
		notes["godot_2_assets"] = godot_2_assets;
	}
	return notes;
}

String ImportExporterReport::get_totals_string() {
	String report = "";
	report += vformat("%-40s", "Totals: ") + String("\n");
	report += vformat("%-40s", "Decompiled scripts: ") + itos(decompiled_scripts.size()) + String("\n");
	report += vformat("%-40s", "Failed scripts: ") + itos(failed_scripts.size()) + String("\n");
	report += vformat("%-40s", "Imported resources for export session: ") + itos(session_files_total) + String("\n");
	report += vformat("%-40s", "Successfully converted: ") + itos(success.size()) + String("\n");
	if (opt_lossy) {
		report += vformat("%-40s", "Lossy: ") + itos(lossy_imports.size()) + String("\n");
	} else {
		report += vformat("%-40s", "Lossy not converted: ") + itos(lossy_imports.size()) + String("\n");
	}
	report += vformat("%-40s", "Rewrote metadata: ") + itos(rewrote_metadata.size()) + String("\n");
	report += vformat("%-40s", "Non-importable conversions: ") + itos(failed_rewrite_md.size()) + String("\n");
	report += vformat("%-40s", "Not converted: ") + itos(not_converted.size()) + String("\n");
	report += vformat("%-40s", "Failed conversions: ") + itos(failed.size()) + String("\n");
	return report;
}

void add_to_dict(Dictionary &dict, const Vector<Ref<ImportInfo>> &vec) {
	for (int i = 0; i < vec.size(); i++) {
		dict[vec[i]->get_path()] = vec[i]->get_export_dest();
	}
}

Dictionary ImportExporterReport::get_report_sections() {
	Dictionary sections;
	// sections["totals"] = get_totals();
	// sections["unsupported_types"] = get_unsupported_types();
	// sections["session_notes"] = get_session_notes();
	sections["success"] = Dictionary();
	Dictionary success_dict = sections["success"];
	for (int i = 0; i < success.size(); i++) {
		success_dict[success[i]->get_path()] = success[i]->get_export_dest();
	}
	sections["decompiled_scripts"] = Dictionary();
	Dictionary decompiled_scripts_dict = sections["decompiled_scripts"];
	for (int i = 0; i < decompiled_scripts.size(); i++) {
		decompiled_scripts_dict[decompiled_scripts[i]] = decompiled_scripts[i];
	}

	if (!not_converted.is_empty()) {
		sections["not_converted"] = Dictionary();
		Dictionary not_converted_dict = sections["not_converted"];
		add_to_dict(not_converted_dict, not_converted);
	}
	if (!failed_scripts.is_empty()) {
		sections["failed_scripts"] = Dictionary();
		Dictionary failed_scripts_dict = sections["failed_scripts"];
		for (int i = 0; i < failed_scripts.size(); i++) {
			failed_scripts_dict[failed_scripts[i]] = failed_scripts[i];
		}
	}
	if (!failed.is_empty()) {
		sections["failed"] = Dictionary();
		Dictionary failed_dict = sections["failed"];
		add_to_dict(failed_dict, failed);
	}
	if (!lossy_imports.is_empty()) {
		sections["lossy_imports"] = Dictionary();
		Dictionary lossy_dict = sections["lossy_imports"];
		add_to_dict(lossy_dict, lossy_imports);
	}
	if (!rewrote_metadata.is_empty()) {
		sections["rewrote_metadata"] = Dictionary();
		Dictionary rewrote_metadata_dict = sections["rewrote_metadata"];
		add_to_dict(rewrote_metadata_dict, rewrote_metadata);
	}
	if (!failed_rewrite_md.is_empty()) {
		sections["failed_rewrite_md"] = Dictionary();
		Dictionary failed_rewrite_md_dict = sections["failed_rewrite_md"];
		add_to_dict(failed_rewrite_md_dict, failed_rewrite_md);
	}
	if (!failed_rewrite_md5.is_empty()) {
		sections["failed_rewrite_md5"] = Dictionary();
		Dictionary failed_rewrite_md5_dict = sections["failed_rewrite_md5"];
		add_to_dict(failed_rewrite_md5_dict, failed_rewrite_md5);
	}
	// plugins
	if (!failed_plugin_cfg_create.is_empty()) {
		sections["failed_plugin_cfg_create"] = Dictionary();
		Dictionary failed_plugin_cfg_create_dict = sections["failed_plugin_cfg_create"];
		for (int i = 0; i < failed_plugin_cfg_create.size(); i++) {
			failed_plugin_cfg_create_dict[failed_plugin_cfg_create[i]] = failed_plugin_cfg_create[i];
		}
	}
	if (!failed_gdnative_copy.is_empty()) {
		sections["failed_gdnative_copy"] = Dictionary();
		Dictionary failed_gdnative_copy_dict = sections["failed_gdnative_copy"];
		for (int i = 0; i < failed_gdnative_copy.size(); i++) {
			failed_gdnative_copy_dict[failed_gdnative_copy[i]] = failed_gdnative_copy[i];
		}
	}
	return sections;
}

String get_to_string(const Vector<Ref<ImportInfo>> &vec) {
	String str = "";
	for (auto &info : vec) {
		str += info->get_path() + " to " + info->get_export_dest() + String("\n");
	}
	return str;
}

String ImportExporterReport::get_report_string() {
	String report;
	report += get_totals_string();
	report += "-------------\n" + String("\n");
	if (lossy_imports.size() > 0) {
		if (opt_lossy) {
			report += "\nThe following files were converted from an import that was stored lossy." + String("\n");
			report += "You may lose fidelity when re-importing these files upon loading the project." + String("\n");
			report += get_to_string(lossy_imports);
		} else {
			report += "\nThe following files were not converted from a lossy import." + String("\n");
			for (int i = 0; i < lossy_imports.size(); i++) {
				report += lossy_imports[i]->get_path() + String("\n");
			}
		}
	}
	if (failed_plugin_cfg_create.size() > 0) {
		report += "------\n";
		report += "\nThe following plugins failed to have their plugin.cfg regenerated:" + String("\n");
		for (int i = 0; i < failed_plugin_cfg_create.size(); i++) {
			report += failed_plugin_cfg_create[i] + String("\n");
		}
	}

	if (failed_gdnative_copy.size() > 0) {
		report += "------\n";
		report += "\nThe following native plugins failed to have their libraries copied:" + String("\n");
		for (int i = 0; i < failed_gdnative_copy.size(); i++) {
			report += failed_gdnative_copy[i] + String("\n");
		}
	}
	// we skip this for version 2 because we have to rewrite the metadata for nearly all the converted resources
	if (rewrote_metadata.size() > 0 && ver->get_major() != 2) {
		report += "------\n";
		report += "\nThe following files had their import data rewritten:" + String("\n");
		report += get_to_string(rewrote_metadata);
	}
	if (failed_rewrite_md.size() > 0) {
		report += "------\n";
		report += "\nThe following files were converted and saved to a non-original path, but did not have their import data rewritten." + String("\n");
		report += "These files will not be re-imported when loading the project." + String("\n");
		report += get_to_string(failed_rewrite_md);
	}
	if (not_converted.size() > 0) {
		report += "------\n";
		report += "\nThe following files were not converted because support has not been implemented yet:" + String("\n");
		report += get_to_string(not_converted);
	}
	if (failed.size() > 0) {
		report += "------\n";
		report += "\nFailed conversions:" + String("\n");
		for (int i = 0; i < failed.size(); i++) {
			report += failed[i]->get_path() + String("\n");
		}
	}
	return report;
}
String ImportExporterReport::get_editor_message_string() {
	String report = "";
	report += "Use Godot editor version " + ver->as_text() + String(godotsteam_detected ? " (steam version)" : "") + " to edit the project." + String("\n");
	if (godotsteam_detected) {
		report += "GodotSteam can be found here: https://github.com/CoaguCo-Industries/GodotSteam/releases \n";
	}
	report += "Note: the project may be using a custom version of Godot. Detection for this has not been implemented yet." + String("\n");
	report += "If you find that you have many non-import errors upon opening the project " + String("\n");
	report += "(i.e. scripts or shaders have many errors), use the original game's binary as the export template." + String("\n");

	return report;
}
String ImportExporterReport::get_detected_unsupported_resource_string() {
	String str = "";
	for (auto type : unsupported_types) {
		Vector<String> spl = type.split("%");
		str += vformat("Type: %-20s", spl[0]) + "\tFormat: " + spl[1] + "\n";
	}
	return str;
}

String ImportExporterReport::get_session_notes_string() {
	String report = "";
	Dictionary notes = get_session_notes();
	auto keys = notes.keys();
	if (keys.size() == 0) {
		return report;
	}
	report += String("\n");
	for (int i = 0; i < keys.size(); i++) {
		Dictionary note = notes[keys[i]];
		if (i > 0) {
			report += String("------\n");
		}
		String title = note["title"];
		String message = note["message"];
		report += title + ":" + String("\n");
		report += message + String("\n");
		PackedStringArray details = note["details"];
		for (int j = 0; j < details.size(); j++) {
			report += " - " + details[j] + String("\n");
		}
		report += String("\n");
	}
	return report;
}
String ImportExporterReport::get_log_file_location() {
	return log_file_location;
}

Vector<String> ImportExporterReport::get_decompiled_scripts() {
	return decompiled_scripts;
}
Vector<String> ImportExporterReport::get_failed_scripts() {
	return failed_scripts;
}
TypedArray<ImportInfo> iinfo_vector_to_typedarray(const Vector<Ref<ImportInfo>> &vec) {
	TypedArray<ImportInfo> arr;
	arr.resize(vec.size());
	for (int i = 0; i < vec.size(); i++) {
		arr.set(i, vec[i]);
	}
	return arr;
}

TypedArray<ImportInfo> ImportExporterReport::get_successes() {
	return iinfo_vector_to_typedarray(success);
}
TypedArray<ImportInfo> ImportExporterReport::get_failed() {
	return iinfo_vector_to_typedarray(failed);
}
TypedArray<ImportInfo> ImportExporterReport::get_not_converted() {
	return iinfo_vector_to_typedarray(not_converted);
}
TypedArray<ImportInfo> ImportExporterReport::get_lossy_imports() {
	return iinfo_vector_to_typedarray(lossy_imports);
}
TypedArray<ImportInfo> ImportExporterReport::get_rewrote_metadata() {
	return iinfo_vector_to_typedarray(rewrote_metadata);
}
TypedArray<ImportInfo> ImportExporterReport::get_failed_rewrite_md() {
	return iinfo_vector_to_typedarray(failed_rewrite_md);
}
TypedArray<ImportInfo> ImportExporterReport::get_failed_rewrite_md5() {
	return iinfo_vector_to_typedarray(failed_rewrite_md5);
}
Vector<String> ImportExporterReport::get_failed_plugin_cfg_create() {
	return failed_plugin_cfg_create;
}
Vector<String> ImportExporterReport::get_failed_gdnative_copy() {
	return failed_gdnative_copy;
}

void ImportExporterReport::print_report() {
	print_line("\n\n********************************EXPORT REPORT********************************" + String("\n"));
	print_line(get_report_string());
	String notes = get_session_notes_string();
	if (!notes.is_empty()) {
		print_line("\n\n---------------------------------IMPORTANT NOTES----------------------------------" + String("\n"));
		print_line(notes);
	}
	print_line("\n------------------------------------------------------------------------------------" + String("\n"));
	print_line(get_editor_message_string());
	print_line("*******************************************************************************\n");
}

void ImportExporterReport::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_totals"), &ImportExporterReport::get_totals);
	ClassDB::bind_method(D_METHOD("get_unsupported_types"), &ImportExporterReport::get_unsupported_types);
	ClassDB::bind_method(D_METHOD("get_session_notes"), &ImportExporterReport::get_session_notes);
	ClassDB::bind_method(D_METHOD("get_totals_string"), &ImportExporterReport::get_totals_string);
	ClassDB::bind_method(D_METHOD("get_report_string"), &ImportExporterReport::get_report_string);
	ClassDB::bind_method(D_METHOD("get_detected_unsupported_resource_string"), &ImportExporterReport::get_detected_unsupported_resource_string);
	ClassDB::bind_method(D_METHOD("get_session_notes_string"), &ImportExporterReport::get_session_notes_string);
	ClassDB::bind_method(D_METHOD("get_editor_message_string"), &ImportExporterReport::get_editor_message_string);
	ClassDB::bind_method(D_METHOD("get_log_file_location"), &ImportExporterReport::get_log_file_location);
	ClassDB::bind_method(D_METHOD("get_decompiled_scripts"), &ImportExporterReport::get_decompiled_scripts);
	ClassDB::bind_method(D_METHOD("get_failed_scripts"), &ImportExporterReport::get_failed_scripts);
	ClassDB::bind_method(D_METHOD("get_successes"), &ImportExporterReport::get_successes);
	ClassDB::bind_method(D_METHOD("get_failed"), &ImportExporterReport::get_failed);
	ClassDB::bind_method(D_METHOD("get_not_converted"), &ImportExporterReport::get_not_converted);
	ClassDB::bind_method(D_METHOD("get_lossy_imports"), &ImportExporterReport::get_lossy_imports);
	ClassDB::bind_method(D_METHOD("get_rewrote_metadata"), &ImportExporterReport::get_rewrote_metadata);
	ClassDB::bind_method(D_METHOD("get_failed_rewrite_md"), &ImportExporterReport::get_failed_rewrite_md);
	ClassDB::bind_method(D_METHOD("get_failed_rewrite_md5"), &ImportExporterReport::get_failed_rewrite_md5);
	ClassDB::bind_method(D_METHOD("get_failed_plugin_cfg_create"), &ImportExporterReport::get_failed_plugin_cfg_create);
	ClassDB::bind_method(D_METHOD("get_failed_gdnative_copy"), &ImportExporterReport::get_failed_gdnative_copy);
	ClassDB::bind_method(D_METHOD("get_report_sections"), &ImportExporterReport::get_report_sections);
	ClassDB::bind_method(D_METHOD("print_report"), &ImportExporterReport::print_report);
	ClassDB::bind_method(D_METHOD("set_ver"), &ImportExporterReport::set_ver);
	ClassDB::bind_method(D_METHOD("get_ver"), &ImportExporterReport::get_ver);
}
