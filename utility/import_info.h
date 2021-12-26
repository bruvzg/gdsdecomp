#ifndef GDRE_IMPORT_INFO_H
#define GDRE_IMPORT_INFO_H

#include "core/io/config_file.h"
#include "core/io/resource.h"
#include "core/io/resource_importer.h"
#include "core/object/object.h"
#include "core/object/ref_counted.h"
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

class ImportInfo : public RefCounted {
	GDCLASS(ImportInfo, RefCounted)
private:
	friend class ImportExporter;
	friend class ResourceFormatLoaderCompat;
	String import_path; // path to the imported file
	String import_md_path; // path to the ".import" file
	int ver_major; //2, 3, 4
	int ver_minor = 0;
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
		ver_major = 0;
	};
	Error load_from_file_v2(const String &p_path);
public:
	enum LossType {
		UNKNOWN = -1,
		LOSSLESS = 0,
		STORED_LOSSY = 1,
		IMPORTED_LOSSY = 2,
		STORED_AND_IMPORTED_LOSSY = 3,
	};
	int get_ver_major() const { return ver_major; }
	int get_ver_minor() const { return ver_minor; }
	String get_path() const { return import_path; }
	String get_import_md_path() const { return import_md_path; }
	String get_type() const { return type; }
	String get_source_file() const { return source_file; }
	Vector<String> get_dest_files() const { return dest_files; }
	bool has_import_data() const {
		if (ver_major == 2) {
			return v2metadata.is_valid();
		} else {
			return cf.is_valid();
		}
	}
	Dictionary get_params() const { return params; }
	Error load_from_file(const String &p_path, int ver_major, int ver_minor);
	Error reload();
	virtual String to_string() override;
	int get_import_loss_type() const;
	Error ImportInfo::rename_source(const String &p_new_source);
protected:
	static void _bind_methods() {
		ClassDB::bind_method(D_METHOD("get_ver_major"), &ImportInfo::get_ver_major);
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