#pragma once

#include "bytecode/bytecode_base.h"
#include "core/string/ustring.h"
#include "core/templates/vector.h"

class BytecodeTester {
public:
	// We are only using this to test 2.1.x and 3.1.x currently.
	static uint64_t test_files(const Vector<String> &p_paths, int ver_major, int ver_minor);
};