#ifndef GDRE_SETTINGS_H
#define GDRE_SETTINGS_H
#include "core/io/logger.h"
#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/os/os.h"
#include "core/os/thread_safe.h"
#include "core/templates/set.h"
#include "gdre_packed_data.h"
#include "packed_file_info.h"
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

class GDRELogger : public Logger {
	String base_path;

	FileAccess *file = nullptr;

public:
	GDRELogger();
	Error open_file(const String &p_base_path);
	void close_file();
	virtual void logv(const char *p_format, va_list p_list, bool p_err) _PRINTF_FORMAT_ATTRIBUTE_2_0;

	virtual ~GDRELogger();
};

class GDRESettings : public Object {
	GDCLASS(GDRESettings, Object);
	_THREAD_SAFE_CLASS_
public:
	class PackInfo : public Reference {
		GDCLASS(PackInfo, Reference);

		friend class GDRESettings;
		String pack_file = "";
		uint32_t ver_major = 0;
		uint32_t ver_minor = 0;
		uint32_t ver_rev = 0;
		uint32_t fmt_version = 0;
		uint32_t pack_flags = 0;
		uint64_t file_base = 0;
		uint32_t file_count = 0;

	public:
		void init(String f, uint32_t vmaj, uint32_t vmin, uint32_t vrev, uint32_t fver, uint32_t flags, uint64_t base, uint32_t count) {
			ver_major = vmaj;
			ver_minor = vmin;
			ver_rev = vrev;
			fmt_version = fver;
			pack_flags = flags;
			file_base = base;
			file_count = count;
		}
	};

private:
	Vector<Ref<PackInfo> > packs;
	PackInfo *current_pack = nullptr;
	PackedData *old_pack_data_singleton = nullptr;
	PackedData *new_singleton = nullptr;
	GDRELogger *logger;
	void *gdre_os;
	String current_project_path = "";

	uint8_t old_key[32] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
	bool set_key = false;
	Vector<uint8_t> enc_key;
	String enc_key_str = "";
	bool in_editor = false;
	bool encrypted = false;
	bool first_load = true;
	String project_path = "";
	static GDRESettings *singleton;
	void remove_current_pack();
	Vector<Ref<PackedFileInfo> > files;
	String _get_res_path(const String &p_path, const String &resource_dir, const bool suppress_errors);
	void add_logger();

public:
	Error load_pack(const String &p_path);
	Error unload_pack();

	Vector<uint8_t> get_encryption_key() { return enc_key; }
	String get_encryption_key_string() { return enc_key_str; }
	bool is_pack_loaded() { return current_pack != nullptr; };
	void set_encryption_key(Vector<uint8_t> key);
	void set_encryption_key(const String &key);
	void reset_encryption_key();
	void add_pack_info(Ref<PackInfo> packinfo);
	void add_pack_file(const Ref<PackedFileInfo> &f_info) { files.push_back(f_info); };
	Vector<String> get_file_list(const Vector<String> &filters = Vector<String>());
	Vector<Ref<PackedFileInfo> > get_file_info_list(const Vector<String> &filters = Vector<String>());
	uint32_t get_pack_version() { return is_pack_loaded() ? current_pack->fmt_version : 0; }
	String get_version_string() { return is_pack_loaded() ? String(itos(current_pack->ver_major) + "." + itos(current_pack->ver_minor) + "." + itos(current_pack->ver_rev)) : String(); }
	uint32_t get_ver_major() { return is_pack_loaded() ? current_pack->ver_major : 0; }
	uint32_t get_ver_minor() { return is_pack_loaded() ? current_pack->ver_minor : 0; }
	uint32_t get_ver_rev() { return is_pack_loaded() ? current_pack->ver_rev : 0; }
	uint32_t get_file_count() { return is_pack_loaded() ? current_pack->file_count : 0; }
	String globalize_path(const String &p_path, const String &resource_path = "") const;
	String localize_path(const String &p_path, const String &resource_path = "") const;
	void set_project_path(const String &p_path) { project_path = p_path; }
	String get_project_path() { return project_path; }
	String get_res_path(const String &p_path, const String &resource_dir = "");
	bool has_res_path(const String &p_path, const String &resource_dir = "");
	Error open_log_file(const String &output_dir);
	bool is_fs_path(const String &p_path);
	Error close_log_file();

	static GDRESettings *get_singleton() {
		// TODO: remove this hack
		if (!singleton) {
			memnew(GDRESettings);
		}
		return singleton;
	}
	GDRESettings();
	~GDRESettings() {
		remove_current_pack();
		if (new_singleton != nullptr) {
			memdelete(new_singleton);
		}
		singleton = nullptr;
		// logger doesn't get memdeleted because the OS singleton will do so
		// gdre_os doesn't get deleted because it's the OS singleton
	}
};
#endif
