

#ifndef RESOURCE_LOADER_COMPAT_H
#define RESOURCE_LOADER_COMPAT_H
#include "core/io/resource.h"
#include "core/io/resource_format_binary.h"
#include "core/io/file_access.h"
#include "core/variant/variant.h"
#include "import_info.h"
#include "resource_import_metadatav2.h"
#include "scene/resources/packed_scene.h"

#ifdef TOOLS_ENABLED
#define print_bl(m_what) (void)(m_what)
//#define print_bl(m_what) print_line(m_what)
#else
#define print_bl(m_what) (void)(m_what)
#endif
namespace VariantBin {
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
	VARIANT_MATRIX3 = 16, // Old name for Basis
	VARIANT_TRANSFORM = 17,
	VARIANT_MATRIX32 = 18, // Old name for Transform2D
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

	// Version 2: added 64 bits support for float and int.
	// Version 3: changed nodepath encoding.
	// Version 4: new string ID for ext/subresources, breaks forward compat.
	FORMAT_VERSION = 4,
	FORMAT_VERSION_CAN_RENAME_DEPS = 1,
	FORMAT_VERSION_NO_NODEPATH_PROPERTY = 3,
};
}
// Used to strip debug messages in release mode
#ifdef DEBUG_ENABLED
#define DEBUG_STR(m_msg) m_msg
#else
#define DEBUG_STR(m_msg) ""
#endif

/**
 * Ensures `m_cond` is false.
 * If `m_cond` is true, prints `m_msg` and the current function returns `m_retval`.
 *
 * If checking for null use ERR_FAIL_NULL_V_MSG instead.
 * If checking index bounds use ERR_FAIL_INDEX_V_MSG instead.
 */
#define ERR_RFLBC_COND_V_MSG_CLEANUP(m_cond, m_retval, m_msg, loader)                                                                               \
	if (unlikely(m_cond)) {                                                                                                                         \
		if (loader != nullptr) memdelete(loader);                                                                                                   \
		_err_print_error(FUNCTION_STR, __FILE__, __LINE__, "Condition \"" _STR(m_cond) "\" is true. Returning: " _STR(m_retval), DEBUG_STR(m_msg)); \
		return m_retval;                                                                                                                            \
	} else                                                                                                                                          \
		((void)0)

#define ERR_RFLBC_COND_V_CLEANUP(m_cond, m_retval, loader) \
	if (unlikely(m_cond)) {                                \
		if (loader != nullptr) memdelete(loader);          \
		return m_retval;                                   \
	} else                                                 \
		((void)0)

struct ResourceProperty {
	String name;
	Variant::Type type;
	Variant value;
};

class ResourceLoaderBinaryCompat {
	bool translation_remapped = false;
	bool fake_load = false;
	// The localized (i.e. res://) path to the resource
	String local_path;
	// The actual path to the resource (either "res://" in pack or file system)
	String res_path;

	// Godot 4.x UID
	ResourceUID::ID uid;

	String type;
	String project_dir;
	Ref<Resource> resource;
	Ref<ResourceImportMetadatav2> imd;
	uint32_t ver_format = 0;
	uint32_t engine_ver_major = 0;
	uint32_t engine_ver_minor = 0;

	bool stored_big_endian = false;
	bool stored_use_real64 = false;
	bool convert_v2image_indexed = false;
	bool hacks_for_deprecated_v2img_formats = true;
	bool no_abort_on_ext_load_fail = true;

	//Godot 4.x flags
	bool using_named_scene_ids = false;
	bool using_uids = false;

	FileAccess *f = nullptr;

	uint64_t importmd_ofs = 0;

	Vector<char> str_buf;
	Vector<StringName> string_map;

	StringName _get_string();

	struct ExtResource {
		String path;
		String type;
		ResourceUID::ID uid = ResourceUID::INVALID_ID;
		RES cache;
	};

	bool use_sub_threads = false;
	float *progress = nullptr;
	Vector<ExtResource> external_resources;
	struct PropPair {
		StringName name;
		Variant value;
	};
	struct IntResource {
		String path;
		uint64_t offset;
	};

	Vector<IntResource> internal_resources;
	Map<String, RES> internal_index_cache;
	Map<String, String> internal_type_cache;
	Map<String, List<ResourceProperty> > internal_index_cached_properties;

	ResourceFormatLoader::CacheMode cache_mode = ResourceFormatLoader::CACHE_MODE_IGNORE;
	void save_unicode_string(const String &p_string);
	static void save_ustring(FileAccess *f, const String &p_string);
	Error repair_property(const String &rtype, const StringName &name, Variant &value);

	String get_unicode_string();
	static void advance_padding(FileAccess *f, uint32_t p_len);
	void _advance_padding(uint32_t p_len);

	Map<String, String> remaps;
	Error error = OK;

	friend class ResourceFormatLoaderCompat;
	//TODO: make a fake_load() or something so we don't have to do this
	friend class TextureLoaderCompat;
	friend class OggStreamLoaderCompat;
	static Map<String, String> _get_file_info(FileAccess *f, Error *r_error);
	Error load_import_metadata();
	static Error _get_resource_header(FileAccess *f);
	RES set_dummy_ext(const String &path, const String &exttype);
	RES set_dummy_ext(const uint32_t erindex);
	RES make_dummy(const String &path, const String &type, const uint32_t subidx);
	void debug_print_properties(String res_name, String res_type, List<ResourceProperty> lrp);

	Error load_ext_resource(const uint32_t i);
	RES get_external_resource(const int subindex);
	RES get_external_resource(const String &path);
	bool has_external_resource(const String &path);

	RES instance_internal_resource(const String &path, const String &type, const int subindex);
	RES get_internal_resource(const int subindex);
	RES get_internal_resource(const String &path);
	String get_internal_resource_type(const String &path);
	bool has_internal_resource(const String &path);
	List<ResourceProperty> get_internal_resource_properties(const String &path);

	static String get_resource_path(const RES &res);

	Error load_internal_resource(const int i);
	Error real_load_internal_resource(const int i);
	Error open_text(FileAccess *p_f, bool p_skip_first_tag);
	Error write_variant_bin(FileAccess *fa, const Variant &p_property, const PropertyInfo &p_hint = PropertyInfo());
	Error parse_variant(Variant &r_v);
	Map<String, RES> dependency_cache;

public:
	void get_dependencies(FileAccess *p_f, List<String> *p_dependencies, bool p_add_types, bool only_paths = false);
	static Error write_variant_bin(FileAccess *f, const Variant &p_property, Map<String, RES> internal_index_cache, Vector<IntResource> &internal_resources, Vector<ExtResource> &external_resources, Vector<StringName> &string_map, const uint32_t ver_format, const PropertyInfo &p_hint = PropertyInfo());
	Error save_to_bin(const String &p_path, uint32_t p_flags = 0);
	static Map<String, String> get_version_and_type(const String &p_path, Error *r_error);
	Error open(FileAccess *p_f, bool p_no_resources = false, bool p_keep_uuid_paths = false);
	Error load();
	static String get_ustring(FileAccess *f);
	Error save_as_text_unloaded(const String &p_path, uint32_t p_flags = 0);
	static String _write_rlc_resources(void *ud, const RES &p_resource);
	String _write_rlc_resource(const RES &res);
	Error _rewrite_new_import_md(const String &p_path, Ref<ResourceImportMetadatav2> new_imd);
	ResourceLoaderBinaryCompat();
	~ResourceLoaderBinaryCompat();
};

//this is derived from PackedScene because node instances in PackedScene cannot be returned otherwise
class FakeResource : public PackedScene {
	GDCLASS(FakeResource, PackedScene);
	String real_path;
	String real_type;

public:
	String get_path() const { return real_path; }
	String get_real_path() { return real_path; }
	String get_real_type() { return real_type; }
	void set_real_type(const String &t) { real_type = t; }
	void set_real_path(const String &p) { real_path = p; }
};

class FakeScript : public Script {
	GDCLASS(FakeScript, Script);
	String real_path;
	String real_type;

public:
	String get_path() const { return real_path; }
	String get_real_path() { return real_path; }
	String get_real_type() { return real_type; }
	void set_real_type(const String &t) { real_type = t; }
	void set_real_path(const String &p) { real_path = p; }
};

class ResourceFormatLoaderCompat : public ResourceFormatLoader {
private:
	ResourceLoaderBinaryCompat *_open(const String &p_path, const String &base_dir, bool fake_load, Error *r_error, float *r_progress);

public:
	Error get_import_info(const String &p_path, const String &base_dir, Ref<ImportInfo> &i_info);
	Error rewrite_v2_import_metadata(const String &p_path, const String &p_dst, Ref<ResourceImportMetadatav2> imd);
	Error convert_bin_to_txt(const String &p_path, const String &dst, const String &output_dir = "", float *r_progress = nullptr);
	RES load(const String &p_path, const String &project_dir = "", Error *r_error = nullptr, bool p_use_sub_threads = false, float *r_progress = nullptr, CacheMode p_cache_mode = CACHE_MODE_IGNORE);
};

#endif // RESOURCE_LOADER_COMPAT_H
