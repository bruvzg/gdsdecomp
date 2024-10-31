#pragma once

#include "bytecode/bytecode_base.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"

class BytecodeTester {
public:
	// NOTE: This function is only implemented for testing 2.1 or 3.1 scripts. This returns 0 for everything else.
	static uint64_t test_files(const Vector<String> &p_paths, int ver_major_hint = -1, int ver_minor_hint = -1);
	static Vector<Ref<GDScriptDecomp>> filter_decomps(const Vector<Ref<GDScriptDecomp>> &decomps, int ver_major_hint, int ver_minor_hint);
	static Vector<Ref<GDScriptDecomp>> get_possible_decomps(Vector<String> bytecode_files, bool include_dev = false);
};