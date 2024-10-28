#include "scene_exporter.h"
#include "compat/resource_loader_compat.h"
#include "core/error/error_list.h"
#include "core/error/error_macros.h"
#include "exporters/export_report.h"
#include "modules/gltf/gltf_document.h"
#include "scene/resources/packed_scene.h"
#include "utility/gdre_settings.h"

struct dep_info {
	ResourceUID::ID uid = ResourceUID::INVALID_ID;
	String dep;
	String remap;
	String type;
};

String get_remapped_path(const String &dep, const String &p_src_path) {
	String dep_path;
	Error err;
	Ref<ImportInfo> tex_iinfo;
	if (GDRESettings::get_singleton()->is_pack_loaded()) {
		tex_iinfo = GDRESettings::get_singleton()->get_import_info_by_source(dep);
		if (tex_iinfo.is_null()) {
			dep_path = GDRESettings::get_singleton()->get_remap(dep);
		} else {
			dep_path = tex_iinfo->get_path();
		}
		if (dep_path.is_empty()) {
			dep_path = dep;
		}
		return dep_path;
	}

	String iinfo_path = dep + ".import";
	if (FileAccess::exists(iinfo_path)) {
		tex_iinfo = ImportInfo::load_from_file(iinfo_path, 0, 0);
		dep_path = tex_iinfo->get_path();
	}
	if (dep_path.is_empty()) {
		String iinfo_path = dep + ".remap";
		if (FileAccess::exists(iinfo_path)) {
			tex_iinfo = ImportInfo::load_from_file(iinfo_path, 0, 0);
			dep_path = tex_iinfo->get_path();
		}
	}
	if (dep_path.is_empty()) {
		return dep;
	}
	return dep_path;
}

void get_deps_recursive(const String &p_path, HashMap<String, dep_info> &r_deps) {
	List<String> deps;
	ResourceCompatLoader::get_dependencies(p_path, &deps, true);
	for (const String &dep : deps) {
		if (!r_deps.has(dep)) {
			r_deps[dep] = dep_info{};
			dep_info &info = r_deps[dep];
			auto splits = dep.split("::");
			if (splits.size() == 3) {
				info.uid = splits[0].is_empty() ? ResourceUID::INVALID_ID : ResourceUID::get_singleton()->text_to_id(splits[0]);
				splits.remove_at(0);
			}
			info.dep = splits[1];
			info.type = splits[0];
			info.remap = get_remapped_path(info.dep, p_path);
			get_deps_recursive(info.remap, r_deps);
		}
	}
}

Error SceneExporter::_export_file(const String &p_dest_path, const String &p_src_path) {
	String dest_ext = p_dest_path.get_extension().to_lower();
	if (dest_ext == "escn" || dest_ext == "tscn") {
		return ResourceCompatLoader::to_text(p_src_path, p_dest_path);
	} else if (dest_ext != "glb") {
		ERR_FAIL_V_MSG(ERR_UNAVAILABLE, "Only .escn, .tscn, and .glb formats are supported for export.");
	}
	Vector<uint64_t> texture_uids;
	Error err;
	{
		List<String> get_deps;
		// We need to preload any Texture resources that are used by the scene with our own loader
		HashMap<String, dep_info> get_deps_map;
		get_deps_recursive(p_src_path, get_deps_map);

		Vector<Ref<Resource>> textures;

		for (auto &E : get_deps_map) {
			dep_info &info = E.value;
			if (info.uid != ResourceUID::INVALID_ID) {
				if (!ResourceUID::get_singleton()->has_id(info.uid)) {
					ResourceUID::get_singleton()->add_id(info.uid, info.remap);
					texture_uids.push_back(info.uid);
				}
			} else {
				String load_path = info.remap;
				Ref<Resource> texture = ResourceCompatLoader::gltf_load(load_path, info.type, &err);
				if (err || texture.is_null()) {
					// return ERR_FILE_MISSING_DEPENDENCIES;
					WARN_PRINT("Failed to load dependent resource " + load_path + " for scene " + p_src_path);
					continue;
				}
#ifdef TOOLS_ENABLED
				texture->set_import_path(info.remap);
#endif
				texture->set_path(info.dep, true);
				textures.push_back(texture);
			}
		}
		err = _export_scene(p_dest_path, p_src_path);
	}
	// remove the UIDs
	for (uint64_t id : texture_uids) {
		ResourceUID::get_singleton()->remove_id(id);
	}
	ERR_FAIL_COND_V_MSG(err, err, "Failed to export scene " + p_src_path);
	return err;
}

Error SceneExporter::_export_scene(const String &p_dest_path, const String &p_src_path) {
	Error err;
	// All 3.x scenes that were imported from scenes/models SHOULD be compatible with 4.x
	// The "escn" format basically force Godot to have compatibility with 3.x scenes
	// This will also pull in any dependencies that were created by the importer (like textures and materials, which should also be similarly compatible)
	Ref<PackedScene> scene = ResourceCompatLoader::gltf_load(p_src_path, "", &err);
	ERR_FAIL_COND_V_MSG(err, err, "Failed to load scene " + p_src_path);
	// GLTF export can result in inaccurate models
	// save it under .assets, which won't be picked up for import by the godot editor
	// we only export glbs
	err = gdre::ensure_dir(p_dest_path.get_base_dir());
	Node *root;
	{
		List<String> deps;
		Ref<GLTFDocument> doc;
		doc.instantiate();
		Ref<GLTFState> state;
		state.instantiate();
		int32_t flags = 0;
		flags |= 16; // EditorSceneFormatImporter::IMPORT_USE_NAMED_SKIN_BINDS;
		root = scene->instantiate();
		err = doc->append_from_scene(root, state, flags);
		if (err) {
			memdelete(root);
			ERR_FAIL_COND_V_MSG(err, err, "Failed to append scene " + p_src_path + " to glTF document");
		}
		err = doc->write_to_filesystem(state, p_dest_path);
	}
	memdelete(root);
	ERR_FAIL_COND_V_MSG(err, err, "Failed to write glTF document to " + p_dest_path);
	return OK;
}
Error SceneExporter::export_file(const String &p_dest_path, const String &p_src_path) {
	String ext = p_dest_path.get_extension().to_lower();
	if (ext != "escn" && ext != "tscn") {
		int ver_major = get_ver_major(p_src_path);
		ERR_FAIL_COND_V_MSG(ver_major != 4, ERR_UNAVAILABLE, "Scene export for engine version " + itos(ver_major) + " is not currently supported.");
	}
	return _export_file(p_dest_path, p_src_path);
}
Ref<ExportReport> SceneExporter::export_resource(const String &output_dir, Ref<ImportInfo> iinfo) {
	Ref<ExportReport> report = memnew(ExportReport(iinfo));

	Error err;
	Vector<uint64_t> texture_uids;
	String new_path = iinfo->get_export_dest();
	String ext = new_path.get_extension().to_lower();
	bool to_text = ext == "escn" || ext == "tscn";
	if (!to_text) {
		new_path = new_path.replace("res://", "res://.assets/");
		if (ext != "glb") {
			new_path = new_path.get_basename() + ".glb";
		}
	}
	iinfo->set_export_dest(new_path);
	String dest_path = output_dir.path_join(new_path.replace("res://", ""));
	if (!to_text && iinfo->get_ver_major() != 4) {
		err = ERR_UNAVAILABLE;
		report->set_message("Scene export for engine version " + itos(iinfo->get_ver_major()) + " is not currently supported.");
		report->set_unsupported_format_type(itos(iinfo->get_ver_major()) + ".x PackedScene");
	} else {
		report->set_new_source_path(new_path);
		err = _export_file(dest_path, iinfo->get_path());
		// if (!err && to_text && iinfo->get_ver_major() == 4) {
		// 	// save .glb too
		// 	String glb_path = output_dir.path_join(new_path.replace("res://", ".assets/").get_basename() + ".glb");
		// 	_export_file(glb_path, iinfo->get_path());
		// }
	}
	if (err == OK) {
		report->set_saved_path(dest_path);
	}
	report->set_error(err);
	return report; // We always save to an unoriginal path
}

bool SceneExporter::handles_import(const String &importer, const String &resource_type) const {
	return importer == "scene" || resource_type == "PackedScene";
}

void SceneExporter::get_handled_types(List<String> *out) const {
	out->push_back("PackedScene");
}

void SceneExporter::get_handled_importers(List<String> *out) const {
	out->push_back("scene");
}