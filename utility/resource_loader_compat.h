

#include "core/io/resource.h"
#include "core/os/file_access.h"
#include "core/variant/variant.h"
#include "core/io/resource_format_binary.h"
#include "scene/resources/packed_scene.h"

#define print_bl(m_what) (void)(m_what)

static const char *type_v2_to_v3renames[][2] = {
	{ "CanvasItemMaterial", "ShaderMaterial" },
	{ "CanvasItemShader", "Shader" },
	{ "ColorFrame", "ColorRect" },
	{ "ColorRamp", "Gradient" },
	{ "FixedMaterial", "SpatialMaterial" },
	{ "Patch9Frame", "NinePatchRect" },
	{ "ReferenceFrame", "ReferenceRect" },
	{ "SampleLibrary", "Resource" },
	{ "SamplePlayer2D", "AudioStreamPlayer2D" },
	{ "SamplePlayer", "Node" },
	{ "SoundPlayer2D", "Node2D" },
	{ "SpatialSamplePlayer", "AudioStreamPlayer3D" },
	{ "SpatialStreamPlayer", "AudioStreamPlayer3D" },
	{ "StreamPlayer", "AudioStreamPlayer" },
	{ "TestCube", "MeshInstance" },
	{ "TextureFrame", "TextureRect" },
	// Only for scripts
	{ "Matrix32", "Transform2D" },
	{ "Matrix3", "Basis" },
	{ "RawArray", "PoolByteArray" },
	{ "IntArray", "PoolIntArray" },
	{ "RealArray", "PoolRealArray" },
	{ "StringArray", "PoolStringArray" },
	{ "Vector2Array", "PoolVector2Array" },
	{ "Vector3Array", "PoolVector3Array" },
	{ "ColorArray", "PoolColorArray" },
	{ NULL, NULL }
};

namespace VariantBinV3{
	
	enum Type {
		//numbering must be different from variant, in case new variant types are added (variant must be always contiguous for jumptable optimization)
		VARIANT_NIL = 1,
		VARIANT_BOOL = 2,
		VARIANT_INT = 3,
		VARIANT_FLOAT = 4,
		VARIANT_STRING = 5,
		VARIANT_VECTOR2 = 10,
		VARIANT_RECT2 = 11,
		VARIANT_VECTOR3 = 12,
		VARIANT_PLANE = 13,
		VARIANT_QUAT = 14,
		VARIANT_AABB = 15,
		VARIANT_MATRIX3 = 16,
		VARIANT_TRANSFORM = 17,
		VARIANT_MATRIX32 = 18,
		VARIANT_COLOR = 20,
		VARIANT_NODE_PATH = 22,
		VARIANT_RID = 23,
		VARIANT_OBJECT = 24,
		VARIANT_INPUT_EVENT = 25,
		VARIANT_DICTIONARY = 26,
		VARIANT_ARRAY = 30,
		VARIANT_RAW_ARRAY = 31,
		VARIANT_INT32_ARRAY = 32,
		VARIANT_FLOAT32_ARRAY = 33,
		VARIANT_STRING_ARRAY = 34,
		VARIANT_VECTOR3_ARRAY = 35,
		VARIANT_COLOR_ARRAY = 36,
		VARIANT_VECTOR2_ARRAY = 37,
		VARIANT_INT64 = 40,
		VARIANT_DOUBLE = 41,
		VARIANT_CALLABLE = 42,
		VARIANT_SIGNAL = 43,
		VARIANT_STRING_NAME = 44,
		VARIANT_VECTOR2I = 45,
		VARIANT_RECT2I = 46,
		VARIANT_VECTOR3I = 47,
		VARIANT_INT64_ARRAY = 48,
		VARIANT_FLOAT64_ARRAY = 49,
		OBJECT_EMPTY = 0,
		OBJECT_EXTERNAL_RESOURCE = 1,
		OBJECT_INTERNAL_RESOURCE = 2,
		OBJECT_EXTERNAL_RESOURCE_INDEX = 3,
		//version 2: added 64 bits support for float and int
		//version 3: changed nodepath encoding
		FORMAT_VERSION = 3,
		FORMAT_VERSION_CAN_RENAME_DEPS = 1,
		FORMAT_VERSION_NO_NODEPATH_PROPERTY = 3,
	};

}

namespace VariantBinV1{
	
	enum Type {
		//numbering must be different from variant, in case new variant types are added (variant must be always contiguous for jumptable optimization)
		VARIANT_NIL = 1,
		VARIANT_BOOL = 2,
		VARIANT_INT = 3,
		VARIANT_REAL = 4,
		VARIANT_STRING = 5,
		VARIANT_VECTOR2 = 10,
		VARIANT_RECT2 = 11,
		VARIANT_VECTOR3 = 12,
		VARIANT_PLANE = 13,
		VARIANT_QUAT = 14,
		VARIANT_AABB = 15,
		VARIANT_MATRIX3 = 16,
		VARIANT_TRANSFORM = 17,
		VARIANT_MATRIX32 = 18,
		VARIANT_COLOR = 20,
		VARIANT_IMAGE = 21,
		VARIANT_NODE_PATH = 22,
		VARIANT_RID = 23,
		VARIANT_OBJECT = 24,
		VARIANT_INPUT_EVENT = 25,
		VARIANT_DICTIONARY = 26,
		VARIANT_ARRAY = 30,
		VARIANT_RAW_ARRAY = 31,
		VARIANT_INT_ARRAY = 32,
		VARIANT_REAL_ARRAY = 33,
		VARIANT_STRING_ARRAY = 34,
		VARIANT_VECTOR3_ARRAY = 35,
		VARIANT_COLOR_ARRAY = 36,
		VARIANT_VECTOR2_ARRAY = 37,

		IMAGE_ENCODING_EMPTY = 0,
		IMAGE_ENCODING_RAW = 1,
		IMAGE_ENCODING_LOSSLESS = 2,
		IMAGE_ENCODING_LOSSY = 3,

		IMAGE_FORMAT_GRAYSCALE = 0,
		IMAGE_FORMAT_INTENSITY = 1,
		IMAGE_FORMAT_GRAYSCALE_ALPHA = 2,
		IMAGE_FORMAT_RGB = 3,
		IMAGE_FORMAT_RGBA = 4,
		IMAGE_FORMAT_INDEXED = 5,
		IMAGE_FORMAT_INDEXED_ALPHA = 6,
		IMAGE_FORMAT_BC1 = 7,
		IMAGE_FORMAT_BC2 = 8,
		IMAGE_FORMAT_BC3 = 9,
		IMAGE_FORMAT_BC4 = 10,
		IMAGE_FORMAT_BC5 = 11,
		IMAGE_FORMAT_PVRTC2 = 12,
		IMAGE_FORMAT_PVRTC2_ALPHA = 13,
		IMAGE_FORMAT_PVRTC4 = 14,
		IMAGE_FORMAT_PVRTC4_ALPHA = 15,
		IMAGE_FORMAT_ETC = 16,
		IMAGE_FORMAT_ATC = 17,
		IMAGE_FORMAT_ATC_ALPHA_EXPLICIT = 18,
		IMAGE_FORMAT_ATC_ALPHA_INTERPOLATED = 19,
		IMAGE_FORMAT_CUSTOM = 30,

		OBJECT_EMPTY = 0,
		OBJECT_EXTERNAL_RESOURCE = 1,
		OBJECT_INTERNAL_RESOURCE = 2,
		OBJECT_EXTERNAL_RESOURCE_INDEX = 3,
		FORMAT_VERSION = 1,
		FORMAT_VERSION_CAN_RENAME_DEPS = 1
	};
}

struct NonPersistentKey { //for resource properties generated on the fly
	RES base;
	StringName property;
	bool operator<(const NonPersistentKey &p_key) const { return base == p_key.base ? property < p_key.property : base < p_key.base; }
};

class ResourceFormatLoaderBinaryCompat: public ResourceFormatLoader {
public:
	Error convert_bin_to_txt(const String &p_path, const String &dst, const String &output_dir = "", float *r_progress = nullptr );
    RES _load(const String &p_path, const String &base_dir, bool no_ext_load = false, Error *r_error = nullptr, float *r_progress = nullptr );
};

class ResourceLoaderBinaryCompat {
    bool translation_remapped = false;
	bool no_ext_load = false;
	String local_path;
	String res_path;
	String type;
	Ref<Resource> resource;
	uint32_t ver_format = 0;
	uint32_t engine_ver_major = 0;

	Map<String, String> typeV2toV3Renames;
	Map<String, String> typeV3toV2Renames;

	FileAccess *f = nullptr;

	uint64_t importmd_ofs = 0;

	Vector<char> str_buf;
	List<RES> resource_cache;

	Vector<StringName> string_map;

	StringName _get_string();

	struct ExtResource {
		String path;
		String type;
		RES cache;
	};

	bool use_sub_threads = false;
	float *progress = nullptr;
	Vector<ExtResource> external_resources;

	struct IntResource {
		String path;
		uint64_t offset;
	};

	Vector<IntResource> internal_resources;
	Map<String, RES> internal_index_cache;

	String get_unicode_string();
	void _advance_padding(uint32_t p_len);

	Map<String, String> remaps;
	Error error = OK;

	ResourceFormatLoader::CacheMode cache_mode = ResourceFormatLoader::CACHE_MODE_REUSE;

	friend class ResourceFormatLoaderBinaryCompat;

	Error parse_variant(Variant &r_v);

	Map<String, RES> dependency_cache;
    public:
		void set_local_path(const String &p_local_path);
        Vector<uint32_t> get_version(FileAccess * p_f);
        void open(FileAccess *p_f);
		Error load();
		Error parse_variantv2(Variant &r_v);
		static String get_ustring(FileAccess *f);
		Error save_as_text(const String &p_path, uint32_t p_flags = 0);
		static String _write_resources(void *ud, const RES &p_resource);
		String _write_resource(const RES &res);
		Error parse_variantv3v4(Variant &r_v);


};


class DummyExtResource: public Resource {
	GDCLASS(DummyExtResource, Resource)
	String path;
	String type;
	int id;
public:
	
	String get_type(){return type;}
	void set_type(const String &t){type = t;}
	String get_path(){return path;}
	void set_path(const String &p){path = p;}
	int get_id(){return id;}
	void set_id(const int i){id = i;}
};