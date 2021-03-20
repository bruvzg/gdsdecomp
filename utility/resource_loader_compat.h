

#include "core/io/resource.h"
#include "core/os/file_access.h"
#include "core/variant/variant.h"
#include "core/io/resource_format_binary.h"
#include "scene/resources/packed_scene.h"

#ifdef TOOLS_ENABLED
#define print_bl(m_what) (void)(m_what)
//#define print_bl(m_what) print_line(m_what)
#else 
#define print_bl(m_what) (void)(m_what)
#endif
namespace VariantBin{
	
	enum Type {
		//numbering must be different from variant, in case new variant types are added (variant must be always contiguous for jumptable optimization)
		VARIANT_NIL = 1,
		VARIANT_BOOL = 2,
		VARIANT_INT = 3,
		VARIANT_REAL = 4, //Old name for VARIANT_FLOAT
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
		VARIANT_IMAGE = 21,
		VARIANT_NODE_PATH = 22,
		VARIANT_RID = 23,
		VARIANT_OBJECT = 24,
		VARIANT_INPUT_EVENT = 25,
		VARIANT_DICTIONARY = 26,
		VARIANT_ARRAY = 30,
		VARIANT_RAW_ARRAY = 31,
		VARIANT_INT_ARRAY = 32, //Old name for VARIANT_INT32_ARRAY
		VARIANT_INT32_ARRAY = 32,
		VARIANT_REAL_ARRAY = 33, //Old name for VARIANT_FLOAT32_ARRAY
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

		//Old variant image enums
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

		//version 2: added 64 bits support for float and int
		//version 3: changed nodepath encoding
		FORMAT_VERSION = 3,
		FORMAT_VERSION_CAN_RENAME_DEPS = 1,
		FORMAT_VERSION_NO_NODEPATH_PROPERTY = 3,
	};

}

class ResourceLoaderBinaryCompat {
    bool translation_remapped = false;
	bool no_ext_load = false;
	String local_path;
	String res_path;

	String type;
	Ref<Resource> resource;
	uint32_t ver_format = 0;
	uint32_t engine_ver_major = 0;
	bool convert_v2image_indexed = false;
	FileAccess *f = nullptr;

	uint64_t importmd_ofs = 0;

	Vector<char> str_buf;
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

	struct ResourceProperty{
		String name;
		Variant::Type type;
		Variant value;
	};

	Vector<IntResource> internal_resources;
	Map<String, RES> internal_index_cache;
	Map<String, String> internal_type_cache;
	Map<String, List<ResourceProperty>> internal_index_cached_properties;

	String get_unicode_string();
	void _advance_padding(uint32_t p_len);

	Map<String, String> remaps;
	Error error = OK;

	friend class ResourceFormatLoaderBinaryCompat;

	static Map<String,String> _get_file_info(FileAccess *f, Error *r_error);
	Error load_import_metadata(Dictionary &r_var);
	static Error _get_resource_header(FileAccess *f);
	RES set_dummy_ext(const String& path, const String& exttype);
	RES set_dummy_ext(const uint32_t erindex);
	RES make_dummy(const String& path, const String& type, const uint32_t subidx);
	void debug_print_properties(String res_name, String res_type, List<ResourceProperty> lrp);
	void clear_dummy_externals();

	Map<String, RES> dependency_cache;
    public:
		void set_local_path(const String &p_local_path);

        static Map<String,String> get_version_and_type(const String &p_path, Error *r_error);
        Error open(FileAccess *p_f);
		Error fake_load();
		Error parse_variantv2(Variant &r_v);
		static String get_ustring(FileAccess *f);
		Error save_as_text_unloaded(const String &p_path, uint32_t p_flags = 0);
		static String _write_fake_resources(void *ud, const RES &p_resource);
		String _write_fake_resource(const RES &res);
		Error parse_variant(Variant &r_v);
		ResourceLoaderBinaryCompat();
		~ResourceLoaderBinaryCompat();
};
class FakeResource : public PackedScene {
	GDCLASS(FakeResource, PackedScene);
	String real_path;
	String real_type;
	public:
		String get_path() const {return real_path;}
		String get_real_path(){return real_path;}
		String get_real_type(){return real_type;}
		void set_real_type(const String &t){real_type = t;}
		void set_real_path(const String &p){real_path = p;}
};

class ResourceFormatLoaderBinaryCompat: public ResourceFormatLoader {
private:
	ResourceLoaderBinaryCompat * _open(const String &p_path, const String &base_dir, bool no_ext_load, Error *r_error, float *r_progress);

public:
	Error get_v2_import_metadata(const String &p_path, const String &base_dir, Dictionary &r_var);
	Error convert_bin_to_txt(const String &p_path, const String &dst, const String &output_dir = "", float *r_progress = nullptr);
	Error convert_v2tex_to_png(const String &p_path, const String &dst, const String &output_dir = "", float *r_progress = nullptr);
};


