#pragma once

#include "bytecode/bytecode_base.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"

class BytecodeTester {
public:
	// NOTE: This function is only implemented for testing 2.1 or 3.1 scripts. This returns 0 for everything else.
	static uint64_t test_files(const Vector<String> &p_paths, int ver_major, int ver_minor);

	// NOTE: This function is only implemented for testing 3.1 scripts (2.1 had no encryption scheme). This returns 0 for everything else.
	static uint64_t test_files_encrypted(const Vector<String> &p_paths, const Vector<uint8_t> &p_key, int ver_major, int ver_minor);
};