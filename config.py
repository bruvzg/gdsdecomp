def can_build(env, platform):
    return True

import methods


# A terrible hack to force-enable our dependent modules being included on non-editor builds.
# etcpak has dependencies our decompressor requires,
# astcenc has the decompression functions.
# without these, we can't decompress either astc or etc textures.
#
# astcenc and etcpak's can_build functions returns False if the editor_build flag is False,
# and it can't be overridden by any flags. Also, Since they come before us in the modules list,
# we can't monkey patch that.
#
# During configure, env.module_list isn't set, and it's not possible to add modules to it.
# sort_module_list is called right after after env.module_list is set with all the modules,
# so we can monkey patch that to add the modules we need.
def monkey_patch_sort_module_list():
    old_sort_module_list = methods.sort_module_list

    def sort_module_list(env):
        if not "etcpak" in env.module_list:
            env.module_list["etcpak"] = "modules/etcpak"
        if not "astcenc" in env.module_list:
            env.module_list["astcenc"] = "modules/astcenc"
            # no need to run configure on etcpak
        return old_sort_module_list(env)

    methods.sort_module_list = sort_module_list


def configure(env):
    if not env.editor_build:
        monkey_patch_sort_module_list()


def get_doc_classes():
    return [
        "GDScriptDecomp",
        "GDScriptDecomp_0b806ee",
        "GDScriptDecomp_8c1731b",
        "GDScriptDecomp_31ce3c5",
        "GDScriptDecomp_703004f",
        "GDScriptDecomp_8cab401",
        "GDScriptDecomp_e82dc40",
        "GDScriptDecomp_2185c01",
        "GDScriptDecomp_97f34a1",
        "GDScriptDecomp_be46be7",
        "GDScriptDecomp_65d48d6",
        "GDScriptDecomp_48f1d02",
        "GDScriptDecomp_30c1229",
        "GDScriptDecomp_7d2d144",
        "GDScriptDecomp_64872ca",
        "GDScriptDecomp_6174585",
        "GDScriptDecomp_23441ec",
        "GDScriptDecomp_7124599",
        "GDScriptDecomp_1add52b",
        "GDScriptDecomp_4ee82a2",
        "GDScriptDecomp_513c026",
        "GDScriptDecomp_23381a5",
        "GDScriptDecomp_85585c7",
        "GDScriptDecomp_ed80f45",
        "GDScriptDecomp_8b912d1",
        "GDScriptDecomp_62273e5",
        "GDScriptDecomp_f8a7c46",
        "GDScriptDecomp_c24c739",
        "GDScriptDecomp_5e938f0",
        "GDScriptDecomp_015d36d",
        "GDScriptDecomp_c6120e7",
        "GDScriptDecomp_d28da86",
        "GDScriptDecomp_216a8aa",
        "GDScriptDecomp_91ca725",
        "GDScriptDecomp_054a2ac",
        "GDScriptDecomp_ff1e7cf",
        "GDScriptDecomp_a56d6ff",
        "GDScriptDecomp_3ea6d9f",
        "GDScriptDecomp_8e35d93",
        "GDScriptDecomp_a3f1ee5",
        "GDScriptDecomp_8aab9a0",
        "GDScriptDecomp_d6b31da",
        "GDScriptDecomp_1ca61a3",
        "GDScriptDecomp_1a36141",
        "GDScriptDecomp_514a3fb",
        "GDScriptDecomp_7f7d97f",
        "GDScriptDecomp_620ec47",
        "GDScriptDecomp_c00427a",
        "GDScriptDecomp_a60f242",
        "GDScriptDecomp_6694c11",
        "GDScriptDecomp_506df14",
        "GDScriptDecomp_5565f55",
        "GDScriptDecomp_f3f05dc",
        "GDRECLIMain",
        "GodotREEditorStandalone",
        "ImportExporter",
        "ImportInfo",
        "NewPackDialog",
        "PackDialog",
        "PckDumper",
        "ScriptCompDialog",
        "ScriptDecompDialog",
    ]


def get_doc_path():
    return "doc_classes"
