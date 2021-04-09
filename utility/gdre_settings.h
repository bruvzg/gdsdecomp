#ifndef GDRE_SETTINGS_H
#define GDRE_SETTINGS_H
#include "core/object/object.h"
#include "core/object/class_db.h"
#include "core/os/thread_safe.h"
#include "core/templates/set.h"
#include "packed_file_info.h"
#include "gdre_packed_data.h"

class GDRESettings : public Object {
	GDCLASS(GDRESettings, Object);
	_THREAD_SAFE_CLASS_
public:
	class PackInfo : public Reference{
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
		void init(String f, uint32_t vmaj, uint32_t vmin, uint32_t vrev, uint32_t fver, uint32_t flags, uint64_t base, uint32_t count){
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
    Vector<Ref<PackInfo>> packs;
    PackInfo * current_pack = nullptr;
    PackedData * old_pack_data_singleton = nullptr;
    PackedData * new_singleton = nullptr;

    String current_project_path = "";

	uint8_t old_key[32] ={0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0, 0,0,0,0,0,0,0,0};
	bool set_key = false;
	Vector<uint8_t> enc_key;
	String enc_key_str = "";
	bool in_editor = false;
    bool encrypted = false;
	bool first_load = true;
	String project_path = "";
    static GDRESettings *singleton;
    void remove_current_pack();
	Vector<Ref<PackedFileInfo>> files;

public:
	
    Error GDRESettings::load_pack(const String& p_path);
	Error GDRESettings::unload_pack();

    Vector<uint8_t> get_encryption_key(){return enc_key;}
	String get_encryption_key_string(){return enc_key_str;}
	bool is_pack_loaded(){return current_pack != nullptr;};
	void set_encryption_key(Vector<uint8_t> key);
	void set_encryption_key(const String &key);
	void reset_encryption_key();
    void add_pack_info(Ref<PackInfo> packinfo);
    void add_pack_file(const Ref<PackedFileInfo> &f_info){files.push_back(f_info);};
	Vector<String> get_file_list(const Vector<String> &filters = Vector<String>());
	Vector<Ref<PackedFileInfo>> get_file_info_list(const Vector<String> &filters = Vector<String>());
	uint32_t get_pack_version(){return is_pack_loaded() ? current_pack->fmt_version : 0;}
	String   get_version_string(){return is_pack_loaded() ? String(itos(current_pack->ver_major) +"."+ itos(current_pack->ver_minor) +"."+ itos(current_pack->ver_rev)) : String();}
	uint32_t get_ver_major(){return is_pack_loaded() ? current_pack->ver_major : 0;}
	uint32_t get_ver_minor(){return is_pack_loaded() ? current_pack->ver_minor : 0;}
	uint32_t get_ver_rev(){return is_pack_loaded() ? current_pack->ver_rev : 0;}
	uint32_t get_file_count(){return is_pack_loaded() ? current_pack->file_count : 0;}
	String globalize_path(const String &p_path, const String &resource_path = "") const;
	String localize_path(const String &p_path, const String &resource_path = "") const;
	void set_project_path(const String & p_path){project_path = p_path;}
	String get_project_path(){return project_path;}
	String GDRESettings::get_res_path(const String &p_path, const String &resource_dir = "");
	bool is_fs_path(const String &p_path);

    static GDRESettings *get_singleton() {
		// TODO: remove this hack
		if (!singleton){
			memnew(GDRESettings);
		}
		return singleton; 
	}
    GDRESettings();
    ~GDRESettings(){
		remove_current_pack();
		if (new_singleton != nullptr){
			memdelete(new_singleton);
		}
        singleton = nullptr;
    }
};
#endif