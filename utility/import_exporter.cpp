
#include "import_exporter.h"
#include <core/os/file_access.h>
#include <core/os/dir_access.h>
#include <core/os/os.h>
#include <core/version_generated.gen.h>
#include "bytecode/bytecode_versions.h"
#include "pcfg_loader.h"
#include "core/variant/variant_parser.h"
#include "resource_loader_compat.h"
#include "modules/regex/regex.h"
#include "stream_texture_v3.h"
#include "scene/resources/audio_stream_sample.h"
#include "modules/stb_vorbis/audio_stream_ogg_vorbis.h"
#include "modules/minimp3/audio_stream_mp3.h"
#include "thirdparty/minimp3/minimp3_ex.h"
#include "gdre_packed_data.h"

Vector<String> get_recursive_dir_list(const String dir, const Vector<String> &wildcards = Vector<String>(), const bool absolute = true, const String rel = ""){
	Vector<String> ret;
	Error err;
	DirAccess *da = DirAccessGDRE::open(dir.plus_file(rel), &err);
	ERR_FAIL_COND_V_MSG(!da, ret, "Failed to open directory " + dir);

	if (!da) {
		return ret;
	}
	String base = absolute ? dir : "";
	da->list_dir_begin();
	String f;
	while ((f = da->get_next()) != "") {
		if (f == "." || f == ".."){
			continue;
		} else if (da->current_is_dir()){
			ret.append_array(get_recursive_dir_list(dir, wildcards, absolute, rel.plus_file(f)));
		} else {
			if (wildcards.size() > 0){
				for (int i = 0; i < wildcards.size(); i++){
					if (f.get_file().match(wildcards[i])){
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

Array ImportExporter::get_import_files(){
	return files;
}

Error ImportExporter::load_import_file_v2(const String& p_path) {
	Error err;
	String path = p_path;
	if (project_dir != ""){
		path = project_dir.plus_file(p_path.replace_first("res://", ""));
	}
	String dest;
	String type;
	Vector<String> spl = p_path.get_file().split(".");
	Ref<ResourceImportMetadatav2> metadata;
	Ref<ImportInfo> iinfo;
	iinfo.instance();

	// This is an import file, possibly has import metadata
	ResourceFormatLoaderBinaryCompat rlc;
	err = rlc.get_import_info(p_path, project_dir, iinfo);
	
	if (err == OK ) {
		// If this is a "converted" file, then it won't have import metadata...
		if (iinfo->has_import_data()) {
			// If this is a path outside of the project directory, we change it to the ".assets" directory in the project dir
			if (iinfo->get_source_file().is_abs_path() || iinfo->get_source_file().begins_with("../")){
				dest = String(".assets").plus_file(p_path.replace("res://","").get_base_dir().plus_file(iinfo->get_source_file().get_file()));
				iinfo->source_file = dest;
			}
			files.push_back(iinfo);
			return OK;
		}
	// The file loaded, but there was no metadata and it was not a ".converted." file
	} else if (err == ERR_PRINTER_ON_FIRE) {
		WARN_PRINT("Could not load metadata from " + p_path);
		String new_ext;
		if (p_path.get_extension() == "tex") {
			new_ext = "png";
		} else if (p_path.get_extension() == "smp") {
			new_ext = "wav";
		}
		dest = String(".assets").plus_file(p_path.replace("res://","").get_base_dir().plus_file(spl[0] + "." + new_ext));
	// File either didn't load or metadata was corrupt
	} else {
		ERR_FAIL_COND_V_MSG(err != OK, err, "Can't open imported file " + p_path);
	}

	//This is a converted file
	if (p_path.get_file().find(".converted.") != -1) {
		// if this doesn't match "filename.ext.converted.newext"
		ERR_FAIL_COND_V_MSG(spl.size() != 4, ERR_CANT_RESOLVE, "Can't open imported file " + p_path);
		dest = p_path.get_base_dir().plus_file(spl[0] + "." + spl[1]);
	}

	// either it's an import file without metadata or a converted file

	iinfo->source_file = dest;
	// If it's a converted file without metadata, it won't have this, and we need it for checking if the file is lossy or not
	// checking if it's a lossy or lossless import
	if (iinfo->importer == "") {
		if (p_path.get_extension() == "scn") {
			iinfo->importer = "scene";
		} else if (p_path.get_extension() == "res") {
			iinfo->importer = "resource";
		} else if (p_path.get_extension() == "tex") {
			iinfo->importer = "texture";
		} else if (p_path.get_extension() == "smp") {
			iinfo->importer = "sample";
		} else if (p_path.get_extension() == "fnt") {
			iinfo->importer = "font";
		} else if (p_path.get_extension() == "msh") {
			iinfo->importer = "mesh";
		} else if (p_path.get_extension() == "xl") {
			iinfo->importer = "translation";
		} else if (p_path.get_extension() == "pbm") {
			iinfo->importer = "bitmask";
		} else {
			iinfo->importer = "none";
		}
	}
	
	files.push_back(iinfo);

	return OK;
}

bool check_if_dir_is_v2(const String &dir){
	Vector<String> wildcards;
	// these are files that will only show up in version 2
	wildcards.push_back("*.converted.*");
	wildcards.push_back("*.tex");
	wildcards.push_back("*.smp");
	if (get_recursive_dir_list(dir, wildcards).size() > 0){
		return true;
	} else {
		return false;
	}
}

Vector<String> get_v2_wildcards(){
	Vector<String> wildcards;
	// We look for file names with ".converted." in the name
	// Like "filename.tscn.converted.scn"
	wildcards.push_back("*.converted.*");
	// The rest of these are imported resources
	wildcards.push_back("*.tex");
	wildcards.push_back("*.fnt");
	wildcards.push_back("*.msh");
	wildcards.push_back("*.scn");
	wildcards.push_back("*.res");
	wildcards.push_back("*.smp");
	wildcards.push_back("*.xl");
	wildcards.push_back("*.pbm");

	return wildcards;
}

Error ImportExporter::load_import_files(const String &dir, const uint32_t ver_major){
	project_dir = dir;
	Vector<String> file_names;
	int version = ver_major;
	if (version == 0) {
		version = check_if_dir_is_v2(dir) ? 2 : 3;
	}
	if (version <= 2) {
		
		file_names = get_recursive_dir_list(dir, get_v2_wildcards(), dir == "res://" ? true : false);
		for (int i = 0; i < file_names.size(); i++) {
			if (load_import_file_v2(file_names[i]) != OK) {
				WARN_PRINT("Can't load V2 converted file: " + file_names[i]);
			}
		}
	} else {
		Vector<String> wildcards;
		wildcards.push_back("*.import");
		file_names = get_recursive_dir_list(dir, wildcards);
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
	FileAccess *f = FileAccessGDRE::open(p_path, FileAccess::READ, &err);
	ERR_FAIL_COND_V_MSG(!f, err, "Could not open " + p_path);
	err = load_import(f, p_path);
	memdelete(f);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not load " + p_path);
	return OK;
}

Error ImportExporter::load_import(FileAccess * f, const String &p_path) {
	Ref<ImportInfo> i_info;
	i_info.instance();
	Dictionary im_data;
	Dictionary thing;

	VariantParser::StreamFile stream;
	stream.f = f;

	String assign;
	Variant value;
	VariantParser::Tag next_tag;
	
	int lines = 0;
	String error_text;
	bool path_found = false; //first match must have priority
	i_info->import_md_path = p_path;
	i_info->version = 3;
	String currentName;
	while (true) {
		assign = Variant();
		next_tag.fields.clear();
		next_tag.name = String();
		Error err = VariantParser::parse_tag_assign_eof(&stream, lines, error_text, next_tag, assign, value, nullptr, true);
		if (err == ERR_FILE_EOF) {
			break;
		}
		ERR_FAIL_COND_V_MSG(err != OK, err, "Parsing Error: " + p_path + ":" + itos(lines) + ": " + error_text);

		if (next_tag.name != String()){
			currentName = next_tag.name;
			if (!im_data.has(next_tag.name)){
				im_data[next_tag.name] = Dictionary();
			}
		}
		
		if (assign != String()) {
			if (!path_found && assign.begins_with("path.") && i_info->import_path == String()) {
				String feature = assign.get_slicec('.', 1);
				if (OS::get_singleton()->has_feature(feature)) {
					i_info->import_path = value;
					path_found = true; //first match must have priority
				}
			} else if (!path_found && assign == "path") {
				i_info->import_path = value;
				path_found = true; //first match must have priority
			} else if (assign == "type") {
				i_info->type = ClassDB::get_compatibility_remapped_class(value);
			} else if (assign == "importer") {
				i_info->importer = value;
			} else if (assign == "source_file") {
				i_info->source_file = value;
			} else if (assign == "dest_files") {
				i_info->dest_files = value;
			}
			((Dictionary)im_data[currentName])[assign] = value;
		}
	}
	ERR_FAIL_COND_V_MSG(i_info->import_path == String() || i_info->type == String(), ERR_FILE_CORRUPT, p_path + ": file is corrupt");
	if (im_data.has("params")){
		i_info->params = im_data["params"];
	}
	i_info->import_data = im_data;	
	files.push_back(i_info);
	return OK;
}

Error ImportExporter::convert_res_bin_2_txt(const String &output_dir, const String &p_path, const String &p_dst) {
	ResourceFormatLoaderBinaryCompat rlc;
	Error err = rlc.convert_bin_to_txt(p_path, p_dst, output_dir);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to convert " + p_path + " to " + p_dst);
	print_line("Converted " + p_path + " to " + p_dst);
	return err;
}

Error ImportExporter::convert_v2tex_to_png(const String &output_dir, const String &p_path, const String &p_dst) {
	ResourceFormatLoaderBinaryCompat rlc;
	DirAccess *da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	String dst_dir = output_dir.plus_file(p_dst.get_base_dir().replace("res://", ""));
	String orig_file = output_dir.plus_file(p_path.replace("res://",""));
	da->make_dir_recursive(dst_dir);
	memdelete(da);
	Error err = rlc.convert_v2tex_to_png(p_path, p_dst, output_dir, true);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to convert " + p_path + " to " + p_dst);
	DirAccess *dr = DirAccess::open(output_dir.plus_file(p_path.get_base_dir().replace("res://", "")), &err);
	ERR_FAIL_COND_V_MSG(!dr, err, "Failed to rename file " + orig_file + ".tmp");
	dr->remove(orig_file);
	dr->rename(orig_file + ".tmp", orig_file);
	print_line("Converted " + p_path + " to " + p_dst);
	memdelete(dr);
	return err;
}

Error ImportExporter::convert_v3stex_to_png(const String &output_dir, const String &p_path, const String &p_dst){
	String src_path = output_dir.plus_file(p_path.replace("res://",""));
	String dst_path = output_dir.plus_file(p_dst.replace("res://",""));
	RFLCTexture st;
	Error err;
	Ref<Image> img = st.load_image_from_tex(src_path, &err);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not load stex file " + p_path);

	err = img->save_png(dst_path);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not save " + p_dst);
	print_line("Converted " + src_path + " to " + dst_path);
	return OK;
}

Error ImportExporter::convert_sample_to_wav(const String &output_dir, const String &p_path, const String &p_dst){
	String src_path = output_dir.plus_file(p_path.replace("res://",""));
	String dst_path = output_dir.plus_file(p_dst.replace("res://",""));
	Error err;
	Ref<AudioStreamSample> sample = ResourceLoader::load(src_path,"",ResourceFormatLoader::CACHE_MODE_IGNORE,&err);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not load sample file " + p_path);
		err = sample->save_to_wav(dst_path);

	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not save " + p_dst);
	print_line("Converted " + src_path + " to " + dst_path);
	return OK;

}

Error ImportExporter::convert_oggstr_to_ogg(const String &output_dir, const String &p_path, const String &p_dst){
	String src_path = output_dir.plus_file(p_path.replace("res://",""));
	String dst_path = output_dir.plus_file(p_dst.replace("res://",""));
	Error err;
	Ref<AudioStreamOGGVorbis> sample = ResourceLoader::load(src_path,"",ResourceFormatLoader::CACHE_MODE_IGNORE,&err);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not load oggstr file " + p_path);
	FileAccess * f = FileAccess::open(dst_path, FileAccess::WRITE);

	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not open " + p_dst + " for saving");
	PackedByteArray data = sample->get_data();
	f->store_buffer(data.ptr(), data.size());
	print_line("Converted " + src_path + " to " + dst_path);
	return OK;

}


Error ImportExporter::convert_mp3str_to_mp3(const String &output_dir, const String &p_path, const String &p_dst){
	String src_path = output_dir.plus_file(p_path.replace("res://",""));
	String dst_path = output_dir.plus_file(p_dst.replace("res://",""));
	Error err;
	Ref<AudioStreamMP3> sample = ResourceLoader::load(src_path,"",ResourceFormatLoader::CACHE_MODE_IGNORE,&err);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not load mp3str file " + p_path);
	FileAccess * f = FileAccess::open(dst_path, FileAccess::WRITE);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not open " + p_dst + " for saving");
	PackedByteArray data = sample->get_data();
	f->store_buffer(data.ptr(), data.size());
	print_line("Converted " + src_path + " to " + dst_path);
	return OK;

}

Error ImportExporter::test_functions(){
	ResourceFormatLoaderBinaryCompat rlc;
	rlc.load("res://apt1_room1.tscn.converted.scn", "C:/workspace/godot-decomps/hc-test-another");
	return OK;
}

void ImportExporter::_bind_methods() {
	ClassDB::bind_method(D_METHOD("load_import_files"), &ImportExporter::load_import_files);
	ClassDB::bind_method(D_METHOD("get_import_files"), &ImportExporter::get_import_files);
	ClassDB::bind_method(D_METHOD("convert_res_bin_2_txt"), &ImportExporter::convert_res_bin_2_txt);
	ClassDB::bind_method(D_METHOD("convert_v2tex_to_png"), &ImportExporter::convert_v2tex_to_png);
	ClassDB::bind_method(D_METHOD("convert_v3stex_to_png"), &ImportExporter::convert_v3stex_to_png);
	ClassDB::bind_method(D_METHOD("convert_sample_to_wav"), &ImportExporter::convert_sample_to_wav);
	ClassDB::bind_method(D_METHOD("convert_oggstr_to_ogg"), &ImportExporter::convert_oggstr_to_ogg);
	ClassDB::bind_method(D_METHOD("convert_mp3str_to_mp3"), &ImportExporter::convert_mp3str_to_mp3);
	ClassDB::bind_method(D_METHOD("test_functions"), &ImportExporter::test_functions);
	//ClassDB::bind_method(D_METHOD("get_dumped_files"), &PckDumper::get_dumped_files);
}
