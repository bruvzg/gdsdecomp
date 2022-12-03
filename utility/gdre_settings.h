#ifndef GDRE_SETTINGS_H
#define GDRE_SETTINGS_H
#include "import_info.h"
#include "packed_file_info.h"
#include "pcfg_loader.h"

#include "core/config/project_settings.h"
#include "core/io/logger.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/os/os.h"
#include "core/os/thread_safe.h"
#include "core/templates/rb_set.h"

#ifdef WINDOWS_ENABLED
#include "platform/windows/os_windows.h"
#endif
#ifdef X11_ENABLED
#include "platform/linuxbsd/os_linuxbsd.h"
#endif
#ifdef OSX_ENABLED
#include "platform/osx/os_osx.h"
#endif
#ifdef UWP_ENABLED
#include "platform/uwp/os_uwp.h"
#endif
#ifdef JAVASCRIPT_ENABLED
#include "platform/javascript/os_javascript.h"
#endif
#if defined(__ANDROID__)
#include "platform/android/os_android.h"
#endif
#ifdef IPHONE_ENABLED
#include "platform/iphone/os_iphone.h"
#endif
// A hack to add another logger to the OS singleton
template <class T>
class GDREOS : public T {
	static_assert(std::is_base_of<OS, T>::value, "T must derive from OS");

public:
	void _add_logger(Logger *p_logger) { T::add_logger(p_logger); }
};

class GDREPackSettings : public ProjectSettings {
	GDCLASS(GDREPackSettings, ProjectSettings);
	// So this can't be instantiated, only derived from current project settings
	// private:
	// 	GDREPackSettings(){};
	// 	~GDREPackSettings(){};

public:
	// void set_singleton() {
	// 	singleton = this;
	// }
	void set_resource_path(const String &p_path) {
		resource_path = p_path;
	}
};
class GDRELogger : public Logger {
	Ref<FileAccess> file;
	String base_path;

public:
	String get_path() { return base_path; };
	GDRELogger();
	Error open_file(const String &p_base_path);
	void close_file();
	virtual void logv(const char *p_format, va_list p_list, bool p_err) _PRINTF_FORMAT_ATTRIBUTE_2_0;

	virtual ~GDRELogger();
};

class GDREPackedSource : public PackSource {
public:
	virtual bool try_open_pack(const String &p_path, bool p_replace_files, uint64_t p_offset);
	virtual Ref<FileAccess> get_file(const String &p_path, PackedData::PackedFile *p_file);
};

class GDRESettings : public Object {
	GDCLASS(GDRESettings, Object);
	_THREAD_SAFE_CLASS_
public:
	class PackInfo : public RefCounted {
		GDCLASS(PackInfo, RefCounted);

		friend class GDRESettings;

	public:
		enum PackType {
			PCK,
			APK,
			ZIP,
			DIR,
			UNKNOWN
		};

	private:
		String pack_file = "";
		uint32_t ver_major = 0;
		uint32_t ver_minor = 0;
		uint32_t ver_rev = 0;
		uint32_t fmt_version = 0;
		uint32_t pack_flags = 0;
		uint64_t file_base = 0;
		uint32_t file_count = 0;
		PackType type = PCK;
		String version_string = "unknown";
		Ref<ProjectConfigLoader> pcfg;

	public:
		void init(
				String f, uint32_t vmaj, uint32_t vmin, uint32_t vrev, uint32_t fver, uint32_t flags, uint64_t base, uint32_t count, String ver_string, PackType tp) {
			pack_file = f;
			ver_major = vmaj;
			ver_minor = vmin;
			ver_rev = vrev;
			fmt_version = fver;
			pack_flags = flags;
			file_base = base;
			file_count = count;
			version_string = ver_string;
			type = tp;
			pcfg.instantiate();
		}
		void set_project_config() {
		}
	};

private:
	Vector<Ref<PackInfo>> packs;
	HashMap<String, Ref<PackedFileInfo>> file_map;
	Ref<PackInfo> current_pack = Ref<PackInfo>();
	PackedData *old_pack_data_singleton = nullptr;
	PackedData *new_singleton = nullptr;
	GDRELogger *logger;
	Array import_files;
	Vector<String> code_files;
	String gdre_resource_path = "";

	String current_project_path = "";

	uint8_t old_key[32] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	bool set_key = false;
	Vector<uint8_t> enc_key;
	String enc_key_str = "";
	bool in_editor = false;
	bool first_load = true;
	bool error_encryption = false;
	String project_path = "";
	static GDRESettings *singleton;
	static String exec_dir;
	void remove_current_pack();
	Vector<Ref<PackedFileInfo>> files;
	String _get_res_path(const String &p_path, const String &resource_dir, const bool suppress_errors);
	void add_logger();

	static String _get_cwd();
	Error get_version_from_bin_resources();
	bool check_if_dir_is_v4();
	bool check_if_dir_is_v3();
	bool check_if_dir_is_v2();
	int get_ver_major_from_dir();
	Error _load_import_file(const String &p_path, bool should_load_md5);

public:
	Error load_dir(const String &p_path);
	Error unload_dir();
	Error load_pack(const String &p_path);
	Error unload_pack();
	String get_gdre_resource_path() const;

	Vector<uint8_t> get_encryption_key();
	String get_encryption_key_string();
	bool is_pack_loaded() const;

	void _set_error_encryption(bool is_encryption_error);
	void set_encryption_key(Vector<uint8_t> key);
	void set_encryption_key(const String &key);
	void reset_encryption_key();
	void add_pack_info(Ref<PackInfo> packinfo);
	void add_pack_file(const Ref<PackedFileInfo> &f_info);

	Vector<String> get_file_list(const Vector<String> &filters = Vector<String>());
	Vector<Ref<PackedFileInfo>> get_file_info_list(const Vector<String> &filters = Vector<String>());
	PackInfo::PackType get_pack_type();
	String get_pack_path();
	uint32_t get_pack_version();
	String get_version_string();
	uint32_t get_ver_major();
	uint32_t get_ver_minor();
	uint32_t get_ver_rev();
	uint32_t get_file_count();
	String globalize_path(const String &p_path, const String &resource_path = "") const;
	String localize_path(const String &p_path, const String &resource_path = "") const;
	void set_project_path(const String &p_path);
	String get_project_path();
	String get_res_path(const String &p_path, const String &resource_dir = "");
	bool has_res_path(const String &p_path, const String &resource_dir = "");
	Error open_log_file(const String &output_dir);
	String get_log_file_path();
	bool is_fs_path(const String &p_path) const;
	Error close_log_file();
	bool has_any_remaps() const;
	bool has_remap(const String &src, const String &dst) const;
	Error add_remap(const String &src, const String &dst);
	Error remove_remap(const String &src, const String &dst);
	Variant get_project_setting(const String &p_setting);
	bool has_project_setting(const String &p_setting);
	String get_project_config_path();
	String get_cwd();
	Array get_import_files(bool copy = false);
	bool has_file(const String &p_path);
	Error load_import_files();
	Error load_import_file(const String &p_path);
	Ref<ImportInfo> get_import_info(const String &p_path);
	Vector<String> get_code_files();
	String get_exec_dir();
	bool are_imports_loaded() const;
	bool is_project_config_loaded() const;

	Error load_project_config();
	Error save_project_config(const String &p_out_dir);

	static GDRESettings *get_singleton();
	GDRESettings();
	~GDRESettings();
};

#endif
