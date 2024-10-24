/*************************************************************************/
/*  register_types.cpp                                                   */
/*************************************************************************/

#include "register_types.h"
#include "core/object/class_db.h"
#include "modules/regex/regex.h"
#ifdef TOOLS_ENABLED
#include "editor/editor_node.h"
#endif

#include "bytecode/bytecode_versions.h"
#include "compat/oggstr_loader_compat.h"
#include "compat/resource_compat_binary.h"
#include "compat/resource_compat_text.h"
#include "compat/resource_loader_compat.h"
#include "compat/texture_loader_compat.h"
#include "editor/gdre_editor.h"
#include "utility/gdre_settings.h"
#include "utility/glob.h"
#include "utility/godotver.h"
#include "utility/import_exporter.h"
#include "utility/packed_file_info.h"
#include "utility/pck_dumper.h"
#ifdef TOOLS_ENABLED
void gdsdecomp_init_callback() {
	EditorNode *editor = EditorNode::get_singleton();
	editor->add_child(memnew(GodotREEditor(editor)));
};
#endif
static GDRESettings *gdre_singleton = nullptr;
// TODO: move this to its own thing
static Ref<ResourceFormatLoaderCompatText> text_loader = nullptr;
static Ref<ResourceFormatLoaderCompatBinary> binary_loader = nullptr;
static Ref<ResourceFormatLoaderCompatTexture2D> texture_loader = nullptr;
static Ref<ResourceFormatLoaderCompatTexture3D> texture3d_loader = nullptr;
static Ref<ResourceFormatLoaderCompatTextureLayered> texture_layered_loader = nullptr;

void init_ver_regex() {
	SemVer::strict_regex = RegEx::create_from_string(GodotVer::strict_regex_str);
	GodotVer::non_strict_regex = RegEx::create_from_string(GodotVer::non_strict_regex_str);
	Glob::magic_check = RegEx::create_from_string(Glob::magic_pattern);
	Glob::escapere = RegEx::create_from_string(Glob::escape_pattern);
}

void free_ver_regex() {
	SemVer::strict_regex = Ref<RegEx>();
	GodotVer::non_strict_regex = Ref<RegEx>();
	Glob::magic_check = Ref<RegEx>();
	Glob::escapere = Ref<RegEx>();
}

void init_loaders() {
	text_loader = memnew(ResourceFormatLoaderCompatText);
	binary_loader = memnew(ResourceFormatLoaderCompatBinary);
	texture_loader = memnew(ResourceFormatLoaderCompatTexture2D);
	texture3d_loader = memnew(ResourceFormatLoaderCompatTexture3D);
	texture_layered_loader = memnew(ResourceFormatLoaderCompatTextureLayered);
	ResourceCompatLoader::add_resource_format_loader(text_loader, true);
	ResourceCompatLoader::add_resource_format_loader(binary_loader, true);
	ResourceCompatLoader::add_resource_format_loader(texture_loader, true);
	ResourceCompatLoader::add_resource_format_loader(texture3d_loader, true);
	ResourceCompatLoader::add_resource_format_loader(texture_layered_loader, true);
}

void deinit_loaders() {
	if (text_loader.is_valid()) {
		ResourceCompatLoader::remove_resource_format_loader(text_loader);
	}
	if (binary_loader.is_valid()) {
		ResourceCompatLoader::remove_resource_format_loader(binary_loader);
	}
	if (texture_loader.is_valid()) {
		ResourceCompatLoader::remove_resource_format_loader(texture_loader);
	}
	if (texture3d_loader.is_valid()) {
		ResourceCompatLoader::remove_resource_format_loader(texture3d_loader);
	}
	if (texture_layered_loader.is_valid()) {
		ResourceCompatLoader::remove_resource_format_loader(texture_layered_loader);
	}
	text_loader = nullptr;
	binary_loader = nullptr;
	texture_loader = nullptr;
	texture3d_loader = nullptr;
	texture_layered_loader = nullptr;
}

void initialize_gdsdecomp_module(ModuleInitializationLevel p_level) {
	ResourceLoader::set_create_missing_resources_if_class_unavailable(true);
	if (p_level != MODULE_INITIALIZATION_LEVEL_SCENE) {
		return;
	}
	ClassDB::register_class<SemVer>();
	ClassDB::register_class<GodotVer>();
	ClassDB::register_class<Glob>();
	init_ver_regex();

	ClassDB::register_abstract_class<GDScriptDecomp>();
	register_decomp_versions();

	ClassDB::register_class<GodotREEditorStandalone>();
	ClassDB::register_class<PckDumper>();
	ClassDB::register_abstract_class<ImportInfo>();
	ClassDB::register_class<ImportInfoModern>();
	ClassDB::register_class<ImportInfov2>();
	ClassDB::register_class<ImportInfoDummy>();
	ClassDB::register_class<ImportInfoRemap>();
	ClassDB::register_class<ImportExporter>();
	ClassDB::register_class<ImportExporterReport>();
	ClassDB::register_class<OggStreamLoaderCompat>();
	ClassDB::register_class<TextureLoaderCompat>();
	ClassDB::register_class<GDRESettings>();

	ClassDB::register_class<PackedFileInfo>();

	ClassDB::register_class<PackDialog>();
	ClassDB::register_class<NewPackDialog>();
	ClassDB::register_class<ScriptCompDialog>();
	ClassDB::register_class<ScriptDecompDialog>();
	gdre_singleton = memnew(GDRESettings);
	Engine::get_singleton()->add_singleton(Engine::Singleton("GDRESettings", GDRESettings::get_singleton()));

#ifdef TOOLS_ENABLED
	EditorNode::add_init_callback(&gdsdecomp_init_callback);
#endif
	init_loaders();
}

void uninitialize_gdsdecomp_module(ModuleInitializationLevel p_level) {
	deinit_loaders();
	if (gdre_singleton) {
		memdelete(gdre_singleton);
		gdre_singleton = nullptr;
	}
	free_ver_regex();
}
