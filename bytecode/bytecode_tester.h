#pragma once

#include "bytecode/bytecode_base.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"

class BytecodeTester {
	static int get_bytecode_version(const Vector<String> &bytecode_files);
	static uint64_t generic_test(const Vector<String> &p_paths, int ver_major_hint, int ver_minor_hint, bool include_dev = false, bool print_verbosely = false);
	static uint64_t test_files_2_1(const Vector<String> &p_paths);
	static uint64_t test_files_3_1(const Vector<String> &p_paths, const Vector<uint8_t> &p_key = Vector<uint8_t>());

public:
	static uint64_t test_files(const Vector<String> &p_paths, int ver_major_hint = -1, int ver_minor_hint = -1, bool print_verbosely = false);
	static Vector<Ref<GDScriptDecomp>> filter_decomps(const Vector<Ref<GDScriptDecomp>> &decomps, int ver_major_hint, int ver_minor_hint);
	static Vector<Ref<GDScriptDecomp>> get_possible_decomps(Vector<String> bytecode_files, bool include_dev = false, bool print_verbosely = false);
};