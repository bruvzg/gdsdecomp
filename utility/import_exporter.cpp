
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

Vector<String> get_recursive_dir_list(const String dir, const Vector<String> &wildcards, const bool absolute = true, const String rel = ""){
	Vector<String> ret;
	DirAccess *da = DirAccess::open(dir.plus_file(rel));
	String base = absolute ? dir : "res://";

	if (!da) {
		return ret;
	}
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

Vector<String> get_recursive_dir_list(const String dir){
	Vector<String> temp;
	return get_recursive_dir_list(dir, temp);
}



Array ImportExporter::get_import_files(){
	return files;
}

Error ImportExporter::load_v2_converted_file(const String& p_path) {
	String dest;
	String type;
	Vector<String> spl = p_path.get_file().split(".");
	Ref<ResourceImportMetadatav2> metadata;
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
		// Just get it into the import info for now
		ResourceFormatLoaderBinaryCompat rlc;
		//
		if (rlc.get_v2_import_metadata(p_path, base_dir, metadata) == OK) {
			//print_line("loaded metadata!");
		} else {
			//print_line("could not load metadata!");
		}
		//Assume that it's loaded from the assets dir in the base dir
		String new_ext;
		if (p_path.get_extension() == "tex") {
			new_ext = "png";
		} else if (p_path.get_extension() == "smp") {
			new_ext = "wav";
		}
		dest = String("assets").plus_file(p_path.replace("res://","").get_base_dir().plus_file(spl[0] + "." + new_ext));
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
	} else if (dest.get_extension() == "fnt") {
		iinfo["type"] = "Font";
	}//Others??
	Vector<String> dest_files;
	dest_files.push_back(p_path);
	iinfo["dest_files"] = dest_files;
	iinfo["metadata"] = metadata;
	iinfo["group_file"] = "";
	iinfo["importer"] = "";
	files.push_back(iinfo);

	return OK;
}

Error ImportExporter::load_import_files(const String &dir, const uint32_t ver_major){
	base_dir = dir;
	Vector<String> wildcards;
	Vector<String> file_names;
	// We instead look for file names with ".converted." in the name
	// Like "filename.tscn.converted.scn"
	if (ver_major <= 2) {
		wildcards.push_back("*.converted.*");
		wildcards.push_back("*.tex");
		file_names = get_recursive_dir_list(dir, wildcards, false);
		
		for (int i = 0; i < file_names.size(); i++) {
			if (load_v2_converted_file(file_names[i]) != OK) {
				WARN_PRINT("Can't load V2 converted file: " + file_names[i]);
			}
		}
	} else {
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
	DirAccess *dr = DirAccess::open(output_dir.plus_file(p_path.get_base_dir().replace("res://", "")));
	dr->remove(orig_file);
	dr->rename(orig_file + ".tmp", orig_file);
	print_line("Converted " + p_path + " to " + p_dst);
	memdelete(dr);
	return err;
}

Error ImportExporter::convert_v3stex_to_png(const String &output_dir, const String &p_path, const String &p_dst){
	String src_path = output_dir.plus_file(p_path.replace("res://",""));
	String dst_path = output_dir.plus_file(p_dst.replace("res://",""));
	StreamTextureV3 st;
	Error err;
	Ref<Image> img = st.load_image(src_path, err);
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
	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not load sample file " + p_path);
	FileAccess * f = FileAccess::open(dst_path, FileAccess::WRITE);

	ERR_FAIL_COND_V_MSG(err != OK, err, "Could not open " + p_dst + " for saving");
	PackedByteArray data = sample->get_data();
	f->store_buffer(data.ptr(), data.size());
	print_line("Converted " + src_path + " to " + dst_path);
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
	//ClassDB::bind_method(D_METHOD("get_dumped_files"), &PckDumper::get_dumped_files);
}
