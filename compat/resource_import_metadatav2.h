
#ifndef RESOURCE_IMPORT_METADATAV2_H
#define RESOURCE_IMPORT_METADATAV2_H

#include "core/io/resource.h"
#include "core/object/ref_counted.h"

class ResourceImportMetadatav2 : public RefCounted {
	GDCLASS(ResourceImportMetadatav2, RefCounted);

	struct Source {
		String path;
		String md5;
	};

	Vector<Source> sources;
	String editor;

	RBMap<String, Variant> options;

	PackedStringArray _get_options() const;
	friend class ImportExporter;

protected:
	virtual bool _use_builtin_script() const { return false; }
	static void _bind_methods();

public:
	void set_editor(const String &p_editor);
	String get_editor() const;
	void add_source(const String &p_path, const String &p_md5 = "");
	void add_source_at(const String &p_path, const String &p_md5, int p_idx);
	String get_source_path(int p_idx) const;
	String get_source_md5(int p_idx) const;
	void set_source_md5(int p_idx, const String &p_md5);
	void remove_source(int p_idx);
	int get_source_count() const;
	void set_option(const String &p_key, const Variant &p_value);
	Variant get_option(const String &p_key) const;
	bool has_option(const String &p_key) const;
	void get_options(List<String> *r_options) const;
	Dictionary get_options_as_dictionary() const {
		Dictionary opts;
		for (auto E = options.front(); E; E = E->next()) {
			opts[E->key()] = E->value();
		}
		return opts;
	}
	Dictionary get_as_dictionary() const {
		Dictionary ret;
		Array srcs;
		for (int i = 0; i < sources.size(); i++) {
			Dictionary src;
			src["path"] = sources[i].path;
			src["md5"] = sources[i].md5;
			srcs.push_back(src);
		}
		ret["sources"] = srcs;
		ret["editor"] = editor;

		ret["options"] = get_options_as_dictionary();
		return ret;
	}
	ResourceImportMetadatav2();
};

#endif //RESOURCE_IMPORT_METADATAV2_H