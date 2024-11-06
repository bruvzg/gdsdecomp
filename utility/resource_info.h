#pragma once
#include "compat/resource_import_metadatav2.h"

#include "core/variant/dictionary.h"

struct ResourceInfo {
	enum LoadType {
		ERR = -1,
		FAKE_LOAD,
		NON_GLOBAL_LOAD,
		GLTF_LOAD,
		REAL_LOAD
	};

	enum ResTopologyType {
		MAIN_RESOURCE,
		INTERNAL_RESOURCE,
		UNLOADED_EXTERNAL_RESOURCE
	};
	ResourceUID::ID uid = ResourceUID::INVALID_ID;
	int ver_format = 0;
	int ver_major = 0; //2, 3, 4
	int ver_minor = 0;
	int packed_scene_version = -1;
	LoadType load_type = ERR;
	String type;
	String resource_format;
	String script_class;
	String cached_id;
	Ref<ResourceImportMetadatav2> v2metadata = nullptr;
	ResTopologyType topology_type = MAIN_RESOURCE;
	bool suspect_version = false;
	bool using_real_t_double = false;
	bool using_named_scene_ids = false;
	bool stored_use_real64 = false;
	bool using_uids = false;
	bool stored_big_endian = false;
	bool is_compressed = false;
	bool using_script_class() {
		return !script_class.is_empty();
	}
	static ResourceInfo from_dict(const Dictionary &dict) {
		ResourceInfo ri;
		ri.uid = dict.get("uid", ResourceUID::INVALID_ID);
		ri.ver_format = dict.get("ver_format", 0);
		ri.ver_major = dict.get("ver_major", 0);
		ri.ver_minor = dict.get("ver_minor", 0);
		ri.packed_scene_version = dict.get("packed_scene_version", 0);
		ri.type = dict.get("type", "");
		ri.resource_format = dict.get("format_type", "");
		ri.script_class = dict.get("script_class", "");
		ri.cached_id = dict.get("cached_id", "");
		ri.v2metadata = dict.get("v2metadata", Ref<ResourceImportMetadatav2>());
		ri.topology_type = static_cast<ResTopologyType>(int(dict.get("topology_type", MAIN_RESOURCE)));
		ri.suspect_version = dict.get("suspect_version", false);
		ri.using_real_t_double = dict.get("using_real_t_double", false);
		ri.using_named_scene_ids = dict.get("using_named_scene_ids", false);
		ri.stored_use_real64 = dict.get("stored_use_real64", false);
		ri.stored_big_endian = dict.get("stored_big_endian", false);
		ri.using_uids = dict.get("using_uids", false);
		ri.is_compressed = dict.get("is_compressed", false);
		return ri;
	}

	Dictionary to_dict() {
		Dictionary dict;
		dict["uid"] = uid;
		dict["ver_format"] = ver_format;
		dict["ver_major"] = ver_major;
		dict["ver_minor"] = ver_minor;
		dict["packed_scene_version"] = packed_scene_version;
		dict["type"] = type;
		dict["format_type"] = resource_format;
		dict["script_class"] = script_class;
		dict["cached_id"] = cached_id;
		dict["v2metadata"] = v2metadata;
		dict["topology_type"] = int(topology_type);
		dict["suspect_version"] = suspect_version;
		dict["using_real_t_double"] = using_real_t_double;
		dict["using_named_scene_ids"] = using_named_scene_ids;
		dict["stored_use_real64"] = stored_use_real64;
		dict["stored_big_endian"] = stored_big_endian;
		dict["using_uids"] = using_uids;
		dict["is_compressed"] = is_compressed;
		return dict;
	}
};