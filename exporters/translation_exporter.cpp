#include "translation_exporter.h"
#include "compat/optimized_translation_extractor.h"
#include "compat/resource_loader_compat.h"
#include "core/error/error_list.h"
#include "core/string/optimized_translation.h"
#include "core/string/translation.h"
#include "exporters/export_report.h"
#include "utility/gdre_settings.h"

Error TranslationExporter::export_file(const String &out_path, const String &res_path) {
	// Implementation for exporting translation files
	String iinfo_path = res_path.get_basename().get_basename() + ".csv.import";
	auto iinfo = ImportInfo::load_from_file(iinfo_path);
	ERR_FAIL_COND_V_MSG(iinfo.is_null(), ERR_CANT_OPEN, "Cannot find import info for translation.");
	Ref<ExportReport> report = export_resource(out_path.get_base_dir(), iinfo);
	ERR_FAIL_COND_V_MSG(report->get_error(), report->get_error(), "Failed to export translation resource.");
	return OK;
}

#define TEST_TR_KEY(key)                          \
	test = default_translation->get_message(key); \
	if (test == s) {                              \
		return key;                               \
	}                                             \
	key = key.to_upper();                         \
	test = default_translation->get_message(key); \
	if (test == s) {                              \
		return key;                               \
	}                                             \
	key = key.to_lower();                         \
	test = default_translation->get_message(key); \
	if (test == s) {                              \
		return key;                               \
	}

String guess_key_from_tr(String s, Ref<Translation> default_translation) {
	static const Vector<String> prefixes = { "$$", "##", "TR_", "KEY_TEXT_" };
	String key = s;
	String test;
	TEST_TR_KEY(key);
	String str = s;
	//remove punctuation
	str = str.replace("\n", "").replace(".", "").replace("…", "").replace("!", "").replace("?", "");
	str = str.strip_escapes().strip_edges().replace(" ", "_");
	key = str;
	TEST_TR_KEY(key);
	// Try adding prefixes
	for (String prefix : prefixes) {
		key = prefix + str;
		TEST_TR_KEY(key);
	}
	// failed
	return "";
}

Ref<ExportReport> TranslationExporter::export_resource(const String &output_dir, Ref<ImportInfo> iinfo) {
	// Implementation for exporting resources related to translations
	Error err = OK;
	// translation files are usually imported from one CSV and converted to multiple "<LOCALE>.translation" files
	String default_locale = GDRESettings::get_singleton()->pack_has_project_config() && GDRESettings::get_singleton()->has_project_setting("locale/fallback")
			? GDRESettings::get_singleton()->get_project_setting("locale/fallback")
			: "en";
	Vector<Ref<Translation>> translations;
	Vector<Vector<StringName>> translation_messages;
	Ref<Translation> default_translation;
	Vector<StringName> default_messages;
	String header = "key";
	Vector<StringName> keys;
	Ref<ExportReport> report = memnew(ExportReport(iinfo));
	report->set_error(ERR_CANT_ACQUIRE_RESOURCE);
	for (String path : iinfo->get_dest_files()) {
		Ref<Translation> tr = ResourceCompatLoader::real_load(path, "", ResourceFormatLoader::CACHE_MODE_IGNORE, &err);
		ERR_FAIL_COND_V_MSG(err != OK, report, "Could not load translation file " + iinfo->get_path());
		ERR_FAIL_COND_V_MSG(!tr.is_valid(), report, "Translation file " + iinfo->get_path() + " was not valid");
		String locale = tr->get_locale();
		header += "," + locale;
		List<StringName> message_list;
		Vector<StringName> messages;
		if (tr->get_class_name() == "OptimizedTranslation") {
			Ref<OptimizedTranslation> otr = tr;
			Ref<OptimizedTranslationExtractor> ote;
			ote.instantiate();
			ote->set("locale", locale);
			ote->set("hash_table", otr->get("hash_table"));
			ote->set("bucket_table", otr->get("bucket_table"));
			ote->set("strings", otr->get("strings"));
			ote->get_message_value_list(&message_list);
			for (auto message : message_list) {
				messages.push_back(message);
			}
		} else {
			// We have a real translation class, get the keys
			if (locale == default_locale) {
				List<StringName> key_list;
				tr->get_message_list(&key_list);
				for (auto key : key_list) {
					keys.push_back(key);
				}
			}
			Dictionary msgdict = tr->get("messages");
			Array values = msgdict.values();
			for (int i = 0; i < values.size(); i++) {
				messages.push_back(values[i]);
			}
		}
		if (locale == default_locale) {
			default_messages = messages;
			default_translation = tr;
		}
		translation_messages.push_back(messages);
		translations.push_back(tr);
	}
	// We can't recover the keys from Optimized translations, we have to guess
	int missing_keys = 0;

	if (default_translation.is_null()) {
		if (translations.size() == 1) {
			report->set_error(ERR_UNAVAILABLE);
			report->set_unsupported_format_type("Dynamic multi-csv");
			return report;
		}
		report->set_error(ERR_FILE_MISSING_DEPENDENCIES);
		ERR_FAIL_V_MSG(report, "No default translation found for " + iinfo->get_path());
	}
	if (keys.size() == 0) {
		for (const StringName &s : default_messages) {
			String key = guess_key_from_tr(s, default_translation);
			if (key.is_empty()) {
				missing_keys++;
				keys.push_back("<MISSING KEY " + s + ">");
			} else {
				keys.push_back(key);
			}
		}
	}
	header += "\n";
	String export_dest = iinfo->get_export_dest();
	if (missing_keys) {
		iinfo->set_export_dest("res://.assets/" + iinfo->get_export_dest().replace("res://", ""));
	}
	String output_path = output_dir.simplify_path().path_join(iinfo->get_export_dest().replace("res://", ""));
	err = gdre::ensure_dir(output_path.get_base_dir());
	ERR_FAIL_COND_V(err, report);
	Ref<FileAccess> f = FileAccess::open(output_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V(err, report);
	ERR_FAIL_COND_V(f.is_null(), report);
	// Set UTF-8 BOM (required for opening with Excel in UTF-8 format, works with all Godot versions)
	f->store_8(0xef);
	f->store_8(0xbb);
	f->store_8(0xbf);
	f->store_string(header);
	for (int i = 0; i < keys.size(); i++) {
		Vector<String> line_values;
		line_values.push_back(keys[i]);
		for (auto messages : translation_messages) {
			if (i >= messages.size()) {
				line_values.push_back("");
			} else {
				line_values.push_back(messages[i]);
			}
		}
		f->store_csv_line(line_values, ",");
	}
	f->flush();
	f = Ref<FileAccess>();
	report->set_error(OK);
	if (missing_keys) {
		String translation_export_message = "WARNING: Could not recover " + itos(missing_keys) + " keys for translation.csv" + "\n";
		translation_export_message += "Saved " + iinfo->get_source_file().get_file() + " to " + iinfo->get_export_dest() + "\n";
		WARN_PRINT("Could not guess all keys in translation.csv");
		report->set_message(translation_export_message);
	}
	report->set_new_source_path(iinfo->get_export_dest());
	report->set_saved_path(output_path);
	print_line("Recreated translation.csv");
	return report;
}

bool TranslationExporter::handles_import(const String &importer, const String &resource_type) const {
	// Check if the exporter can handle the given importer and resource type
	return importer == "translation_csv" || resource_type == "Translation";
}

void TranslationExporter::get_handled_types(List<String> *out) const {
	// Add the types of resources that this exporter can handle
	out->push_back("Translation");
}

void TranslationExporter::get_handled_importers(List<String> *out) const {
	// Add the importers that this exporter can handle
	out->push_back("translation_csv");
}