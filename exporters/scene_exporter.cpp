#include "scene_exporter.h"
#include "compat/resource_loader_compat.h"
#include "core/error/error_list.h"
#include "exporters/export_report.h"
#include "modules/gltf/gltf_document.h"
#include "scene/resources/packed_scene.h"
#include "utility/gdre_settings.h"

Error SceneExporter::export_file(const String &p_dest_path, const String &p_src_path) {
	Vector<uint64_t> texture_uids;
	Error err;
	{
		List<String> get_deps;
		// We need to preload any Texture resources that are used by the scene with our own loader
		// TODO: use ResourceCompatLoader::get_dependencies instead
		ResourceLoader::get_dependencies(p_src_path, &get_deps, true);
		HashMap<String, Vector<String>> get_deps_map;
		for (auto &i : get_deps) {
			auto splits = i.split("::");
			if (splits.size() < 3) {
				continue;
			}
			get_deps_map.insert(splits[2], splits);
		}

		Vector<Ref<Resource>> textures;

		for (auto &E : get_deps_map) {
			String dep = E.key;
			auto &splits = E.value;
			String dep_path;
			Ref<ImportInfo> tex_iinfo;
			if (GDRESettings::get_singleton()->is_pack_loaded()) {
				tex_iinfo = GDRESettings::get_singleton()->get_import_info_by_source(dep);
			}
			if (tex_iinfo.is_null()) {
				String iinfo_path = dep + ".import";
				if (FileAccess::exists(iinfo_path)) {
					tex_iinfo = ImportInfo::load_from_file(iinfo_path, 0, err);
				}
			}
			if (tex_iinfo.is_null()) {
				String iinfo_path = dep + ".remap";
				if (FileAccess::exists(iinfo_path)) {
					tex_iinfo = ImportInfo::load_from_file(iinfo_path, 0, err);
				}
			}
			if (tex_iinfo.is_null()) {
				dep_path = dep;
			} else {
				dep_path = tex_iinfo->get_path();
			}
			String load_path = dep;
			auto id = ResourceUID::get_singleton()->text_to_id(splits[0]);
			if (!splits[0].is_empty() && id != ResourceUID::INVALID_ID) {
				if (!ResourceUID::get_singleton()->has_id(id)) {
					ResourceUID::get_singleton()->add_id(id, dep_path);
				} else {
					ResourceUID::get_singleton()->set_id(id, dep_path);
				}
				texture_uids.push_back(id);
			} else {
				load_path = dep_path;
				Ref<Resource> texture = ResourceCompatLoader::gltf_load(load_path, splits[1], &err);
				if (err || texture.is_null()) {
					// return ERR_FILE_MISSING_DEPENDENCIES;
					WARN_PRINT("Failed to load dependent resource " + dep_path + " for scene " + p_src_path);
					continue;
				}
#ifdef TOOLS_ENABLED
				texture->set_import_path(dep_path);
#endif
				texture->set_path(dep, true);
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
	err = ensure_dir(p_dest_path.get_base_dir());
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

Ref<ExportReport> SceneExporter::export_resource(const String &output_dir, Ref<ImportInfo> iinfo) {
	Ref<ExportReport> report = memnew(ExportReport(iinfo));

	Error err;
	Vector<uint64_t> texture_uids;
	String new_path = iinfo->get_export_dest().replace("res://", "res://.assets/");
	if (new_path.get_extension().to_lower() != "glb") {
		new_path = new_path.get_basename() + ".glb";
	}
	String dest_path = output_dir.path_join(new_path.replace("res://", ""));
	report->set_new_source_path(new_path);
	report->set_saved_path(dest_path);
	err = export_file(dest_path, iinfo->get_path());

	report->set_error(err);
	report->set_saved_path(dest_path);
	return report; // We always save to an unoriginal path
}

bool SceneExporter::handles_import(const String &importer, const String &resource_type) const {
	return importer == "scene" || resource_type == "PackedScene";
}