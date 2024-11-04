//_PRELUDE_
// clang-format off

#include "bytecode/bytecode_versions.h"
#include "utility/godotver.h"

void register_decomp_versions() {
//_BYTECODE_CLASSDB_REGISTER_
}

GDScriptDecomp *create_decomp_for_commit(uint64_t p_commit_hash) {
	switch (p_commit_hash) {
//_BYTECODE_CASE_STATEMENTS_
		default:
			return nullptr;
	}
}

Vector<Ref<GDScriptDecomp>> get_decomps_for_bytecode_ver(int bytecode_version, bool include_dev) {
	Vector<Ref<GDScriptDecomp>> decomps;
	for (int i = 0; i < num_decomp_versions; i++) {
		if (decomp_versions[i].bytecode_version == bytecode_version && (include_dev || !decomp_versions[i].is_dev)) {
			decomps.push_back(Ref<GDScriptDecomp>(create_decomp_for_commit(decomp_versions[i].commit)));
		}
	}
	return decomps;
}

Vector<GDScriptDecompVersion> get_decomp_versions(bool include_dev, int ver_major) {
	Vector<GDScriptDecompVersion> versions;
	String ver_major_str = itos(ver_major);
	for (int i = 0; i < num_decomp_versions; i++) {
		if (decomp_versions[i].commit == 0xfffffff || decomp_versions[i].commit == 0x0000000) {
			continue;
		}
		if (!include_dev && decomp_versions[i].is_dev) {
			continue;
		}
		if (ver_major > 0 && decomp_versions[i].min_version[0] != ver_major_str[0]) {
			continue;
		}
		versions.push_back(GDScriptDecompVersion(decomp_versions[i]));
	}
	return versions;
}
