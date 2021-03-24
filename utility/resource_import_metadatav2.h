
#ifndef RESOURCE_IMPORT_METADATAV2_H
#define RESOURCE_IMPORT_METADATAV2_H

#include "core/io/resource.h"
#include "core/object/reference.h"

class ResourceImportMetadatav2 : public Reference {
	GDCLASS(ResourceImportMetadatav2, Reference);

	struct Source {
		String path;
		String md5;
	};

	Vector<Source> sources;
	String editor;

	Map<String, Variant> options;

	PackedStringArray _get_options() const;

protected:
	virtual bool _use_builtin_script() const { return false; }
	static void _bind_methods();

public:
	void set_editor(const String &p_editor);
	String get_editor() const;
	void add_source(const String &p_path, const String &p_md5 = "");
	String get_source_path(int p_idx) const;
	String get_source_md5(int p_idx) const;
	void set_source_md5(int p_idx, const String &p_md5);
	void remove_source(int p_idx);
	int get_source_count() const;
	void set_option(const String &p_key, const Variant &p_value);
	Variant get_option(const String &p_key) const;
	bool has_option(const String &p_key) const;
	void get_options(List<String> *r_options) const;
	ResourceImportMetadatav2();
};

#endif //RESOURCE_IMPORT_METADATAV2_H