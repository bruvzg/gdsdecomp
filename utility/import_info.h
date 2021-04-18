#ifndef GDRE_IMPORT_INFO_H
#define GDRE_IMPORT_INFO_H

#include "core/io/config_file.h"
#include "core/io/resource.h"
#include "core/io/resource_importer.h"
#include "core/object/object.h"
#include "core/object/reference.h"
#include "core/templates/map.h"
#include "resource_import_metadatav2.h"

namespace V2ImportEnums {
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
enum Storage {
	STORAGE_RAW,
	STORAGE_COMPRESS_LOSSY,
	STORAGE_COMPRESS_LOSSLESS
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
} // namespace V2ImportEnums

class ImportInfo : public Reference {
	GDCLASS(ImportInfo, Reference)
private:
	friend class ImportExporter;
	friend class ResourceFormatLoaderCompat;
	String import_path; // path to the imported file
	String import_md_path; // path to the ".import" file
	int version; //2, 3, 4
	String type;
	String importer;
	String source_file; // file to import
	Vector<String> dest_files;
	String preferred_dest;
	Dictionary params; // import options (like compression mode, lossy quality, etc.)
	Ref<ConfigFile> cf; // raw v3-v4 import data
	Ref<ResourceImportMetadatav2> v2metadata; // Raw v2 import metadata
	Dictionary v3metadata_prop; // 'metadata' property of "remap" tag in an import file
	void _init() {
		import_path = "";
		type = "";
		importer = "";
		source_file = "";
		version = 0;
	};

public:
	enum LossType {
		UNKNOWN = -1,
		LOSSLESS = 0,
		STORED_LOSSY = 1,
		IMPORTED_LOSSY = 2,
		STORED_AND_IMPORTED_LOSSY = 3,
	};
	int get_version() const { return version; }
	String get_path() const { return import_path; }
	String get_import_md_path() const { return import_md_path; }
	String get_type() const { return type; }
	String get_source_file() const { return source_file; }
	Vector<String> get_dest_files() const { return dest_files; }
	bool has_import_data() const {
		if (version == 2) {
			return v2metadata.is_valid();
		} else {
			return cf.is_valid();
		}
	}
	Dictionary get_params() const { return params; }
	virtual String to_string() override {
		String s = "ImportInfo: {";
		s += "\n\timport_md_path: " + import_md_path;
		s += "\n\tpath: " + import_path;
		s += "\n\ttype: " + type;
		s += "\n\timporter: " + importer;
		s += "\n\tsource_file: " + source_file;
		s += "\n\tdest_files: [";
		for (int i = 0; i < dest_files.size(); i++) {
			if (i > 0) {
				s += ", ";
			}
			s += " " + dest_files[i];
		}
		s += " ]";
		s += "\n\tparams: {";
		List<Variant> *keys = memnew(List<Variant>);
		params.get_key_list(keys);
		for (int i = 0; i < keys->size(); i++) {
			// skip excessively long options list
			if (i == 8) {
				s += "\n\t\t[..." + itos(keys->size() - i) + " others...]";
				break;
			}
			String t = (*keys)[i];
			s += "\n\t\t" + t + "=" + params[t];
		}
		memdelete(keys);
		s += "\n\t}\n}";
		return s;
	}
	int get_import_loss_type() const {
		if (importer == "scene" || importer == "ogg_vorbis" || importer == "mp3" || importer == "wavefront_obj") {
			//These are always imported as either native files or losslessly
			return LOSSLESS;
		}
		if (!has_import_data()) {
			return UNKNOWN;
		}

		if (importer == "texture") {
			int stat = 0;
			String ext = source_file.get_extension();
			// These are always imported in such a way that it is impossible to recover the original file
			// SVG in particular is converted to raster from vector
			// However, you can convert these and rewrite the imports such that they will be imported losslessly next time
			if (ext == "svg" || ext == "jpg" || ext == "webp") {
				stat |= IMPORTED_LOSSY;
			}
			// Not possible to recover asset used to import losslessly
			if (params.has("compress/mode") && params["compress/mode"].is_num()) {
				stat |= (int)params["compress/mode"] <= 1 ? LOSSLESS : STORED_LOSSY;
			} else if (params.has("storage") && params["storage"].is_num()) {
				stat |= (int)params["storage"] != V2ImportEnums::Storage::STORAGE_COMPRESS_LOSSY ? LOSSLESS : STORED_LOSSY;
			}
			return LossType(stat);
		} else if (importer == "wav" || (version == 2 && importer == "sample")) {
			// Not possible to recover asset used to import losslessly
			if (params.has("compress/mode") && params["compress/mode"].is_num()) {
				return (int)params["compress/mode"] == 0 ? LOSSLESS : STORED_LOSSY;
			}
		}

		if (version == 2 && importer == "font") {
			// Not possible to recover asset used to import losslessly
			if (params.has("mode/mode") && params["mode/mode"].is_num()) {
				return (int)params["mode/mode"] == V2ImportEnums::FontMode::FONT_DISTANCE_FIELD ? LOSSLESS : STORED_LOSSY;
			}
		}

		// We can't say for sure
		return UNKNOWN;
	}

protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("get_version"), &ImportInfo::get_version);
		ClassDB::bind_method(D_METHOD("get_path"), &ImportInfo::get_path);
		ClassDB::bind_method(D_METHOD("get_import_md_path"), &ImportInfo::get_import_md_path);
		ClassDB::bind_method(D_METHOD("get_type"), &ImportInfo::get_type);
		ClassDB::bind_method(D_METHOD("get_source_file"), &ImportInfo::get_source_file);
		ClassDB::bind_method(D_METHOD("get_dest_files"), &ImportInfo::get_dest_files);
		ClassDB::bind_method(D_METHOD("get_params"), &ImportInfo::get_params);
		ClassDB::bind_method(D_METHOD("get_import_loss_type"), &ImportInfo::get_import_loss_type);
	}
};

#endif