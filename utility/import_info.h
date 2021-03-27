#ifndef GDRE_IMPORT_INFO_H
#define GDRE_IMPORT_INFO_H

#include "core/templates/map.h"
#include "core/io/resource.h"
#include "core/object/object.h"
#include "core/object/reference.h"
#include "core/io/resource_importer.h"
#include "resource_import_metadatav2.h"

namespace V2ImportEnums{
	enum TextureFormat {
		IMAGE_FORMAT_UNCOMPRESSED,
		IMAGE_FORMAT_COMPRESS_DISK_LOSSLESS,
		IMAGE_FORMAT_COMPRESS_DISK_LOSSY,
		IMAGE_FORMAT_COMPRESS_RAM,
	};
	enum FontMode {
		FONT_BITMAP,
		FONT_DISTANCE_FIELD
	};

	enum ImageFlags {
		IMAGE_FLAG_STREAM_FORMAT = 1,
		IMAGE_FLAG_FIX_BORDER_ALPHA = 2,
		IMAGE_FLAG_ALPHA_BIT = 4, //hint for compressions that use a bit for alpha
		IMAGE_FLAG_COMPRESS_EXTRA = 8, // used for pvrtc2
		IMAGE_FLAG_NO_MIPMAPS = 16, //normal for 2D games
		IMAGE_FLAG_REPEAT = 32, //usually disabled in 2D
		IMAGE_FLAG_FILTER = 64, //almost always enabled
		IMAGE_FLAG_PREMULT_ALPHA = 128, //almost always enabled
		IMAGE_FLAG_CONVERT_TO_LINEAR = 256, //convert image to linear
		IMAGE_FLAG_CONVERT_NORMAL_TO_XY = 512, //convert image to linear
		IMAGE_FLAG_USE_ANISOTROPY = 1024, //convert image to linear
	};
	enum TextureMode {
		MODE_TEXTURE_2D,
		MODE_TEXTURE_3D,
		MODE_ATLAS,
		MODE_LARGE,
		MODE_MAX
	};
	enum SampleCompressMode {
		COMPRESS_MODE_DISABLED,
		COMPRESS_MODE_RAM,
		COMPRESS_MODE_DISK
	};

	enum SceneFlags {
		IMPORT_SCENE = 1,
		IMPORT_ANIMATION = 2,
		IMPORT_ANIMATION_DETECT_LOOP = 4,
		IMPORT_ANIMATION_OPTIMIZE = 8,
		IMPORT_ANIMATION_FORCE_ALL_TRACKS_IN_ALL_CLIPS = 16,
		IMPORT_ANIMATION_KEEP_VALUE_TRACKS = 32,
		IMPORT_GENERATE_TANGENT_ARRAYS = 256,
		IMPORT_FAIL_ON_MISSING_DEPENDENCIES = 512
	};
}

class ImportInfo: public Reference {
	GDCLASS(ImportInfo, Reference)
private:
	friend class ImportExporter;
	friend class ResourceFormatLoaderBinaryCompat;
	String import_path; // path to the imported file
	String import_md_path; // path to the ".import" file
	int version; //2, 3
	String type;
	String importer;
	String source_file; // file to import
	Vector<String> dest_files;
	String preferred_dest;
	Dictionary params; // import options (like compression mode, lossy quality, etc.)
	Dictionary import_data; // Raw import metadata
	Ref<ResourceImportMetadatav2> v2metadata; // Raw v2 import metadata

	void _init() {
		import_path = "";
		type = "";
		importer = "";
		source_file = "";
		version = 0;
	};

public:
	int get_version() {return version;}
	String get_path() {return import_path;}
	String get_import_md_path() {return import_md_path;}
	String get_type() {return type;}
	String get_source_file() {return source_file;}
	Vector<String> get_dest_files() {return dest_files;}
	bool has_import_data() {return !import_data.is_empty();}
	Dictionary get_params() { return params;}
	Dictionary get_import_data() {return import_data;}
	void set_import_data(const Dictionary &data) { import_data = data; }
	virtual String to_string() override {
		String s = "ImportInfo: {";
		s += "\n\timport_md_path: " + import_md_path;
		s += "\n\tpath: " + import_path;
		s += "\n\ttype: " + type;
		s += "\n\timporter: " + importer;
		s += "\n\tsource_file: " + source_file;
		s += "\n\tdest_files: [";
		for (int i = 0; i < dest_files.size(); i++){
			if (i > 0){
				s += ", ";
			}
			s += " " + dest_files[i];
		}
		s += " ]";
		s += "\n\tparams: {";
		List<Variant> * keys = memnew(List<Variant>);
		params.get_key_list(keys);
		for (int i = 0; i < keys->size(); i++){
			// skip excessively long options list
			if (i == 8){
				s += "\n\t\t[..." + itos(keys->size() - i) +" others...]";
				break;
			}
			String t = (*keys)[i];
			s += "\n\t\t" + t + "=" + params[t];
		}
		memdelete(keys);
		s += "\n\t}\n}";
		return s;
	}
	bool is_lossless_import() {
		if (importer == "scene" || importer == "ogg_vorbis" || importer == "mp3" || importer == "wavefront_obj"){
			//These are always imported as either native files or losslessly
			return true;
		}
		if (!has_import_data()){
			ERR_FAIL_COND_V_MSG(!has_import_data(), false, "DOESNT HAVE IMPORT METADATA!!!!");
		}
		if (version == 2) {
			if (importer == "texture") {
				if (source_file != ""){
					String ext = source_file.get_extension();
					// These are always imported in such a way that it is impossible to recover the original file
					// SVG in particular is converted to raster from vector
					if (ext == "svg" || ext == "jpg" || ext == "webp"){
						return false;
					}
				}
				if (params.has("format") && params["format"].is_num()){
					return (int)params["format"] <= V2ImportEnums::TextureFormat::IMAGE_FORMAT_COMPRESS_DISK_LOSSLESS;
				}
			}
			else if (importer == "sample") {
				if (params.has("compress/mode") && params["compress/mode"].is_num()){
					return (int)params["compress/mode"] == V2ImportEnums::SampleCompressMode::COMPRESS_MODE_DISABLED;
				}
			}
			else if (importer == "font") {
				if (params.has("mode/mode") && params["mode/mode"].is_num()) {
					return (int)params["mode/mode"] == V2ImportEnums::FontMode::FONT_DISTANCE_FIELD;
				}
			}
		} else if (version > 2 && version < 4) {
			if (importer == "texture") {
				String ext = source_file.get_extension();
				// These are always imported in such a way that it is impossible to recover the original file
				// SVG in particular is converted to raster from vector
				if (ext == "svg" || ext == "jpg" || ext == "webp"){
					return false;
				}
				if (params.has("compress/mode") && params["compress/mode"].is_num()){
					return (int)params["compress/mode"] <= 1;
				}
			}
			else if (importer == "wav") {
				if (params.has("compress/mode") && params["compress/mode"].is_num()){
					return (int)params["compress/mode"] == 0;
				}
			}
		}
		// We can't say for sure
		return false;
	}
protected:
	static void _bind_methods(){
		ClassDB::bind_method(D_METHOD("get_version"), &ImportInfo::get_version);
		ClassDB::bind_method(D_METHOD("get_path"), &ImportInfo::get_path);
		ClassDB::bind_method(D_METHOD("get_import_md_path"), &ImportInfo::get_import_md_path);
		ClassDB::bind_method(D_METHOD("get_type"), &ImportInfo::get_type);
		ClassDB::bind_method(D_METHOD("get_source_file"), &ImportInfo::get_source_file);
		ClassDB::bind_method(D_METHOD("get_dest_files"), &ImportInfo::get_dest_files);
		ClassDB::bind_method(D_METHOD("get_import_data"), &ImportInfo::get_import_data);
		ClassDB::bind_method(D_METHOD("get_params"), &ImportInfo::get_params);
		ClassDB::bind_method(D_METHOD("set_import_data"), &ImportInfo::set_import_data);
		ClassDB::bind_method(D_METHOD("is_lossless_import"), &ImportInfo::is_lossless_import);
	}
};

#endif