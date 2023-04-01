#include "bytecode_tester.h"
#include "bytecode_versions.h"
#include "core/io/file_access.h"
// We are only using this to test 2.1.x and 3.1.x currently.

uint64_t test_files_2_1(const Vector<String> &p_paths) {
	uint64_t rev = 0;
	bool ed80f45_failed = false;
	bool ed80f45_passed = false;
	bool _85585c7_failed = false;
	bool _85585c7_passed = false;
	bool _7124599_failed = false;
	bool _7124599_passed = false;

	GDScriptDecomp_ed80f45 *decomp_ed80f45 = memnew(GDScriptDecomp_ed80f45);
	GDScriptDecomp_85585c7 *decomp_85585c7 = memnew(GDScriptDecomp_85585c7);
	GDScriptDecomp_7124599 *decomp_7124599 = memnew(GDScriptDecomp_7124599);
	for (String path : p_paths) {
		Vector<uint8_t> data = FileAccess::get_file_as_bytes(path);
		if (data.size() == 0) {
			continue;
		}
		if (!ed80f45_failed && !ed80f45_passed) {
			auto test_result = decomp_ed80f45->test_bytecode(data);
			if (test_result == GDScriptDecomp::BYTECODE_TEST_RESULT::BYTECODE_TEST_FAIL) {
				ed80f45_failed = true;
			} else if (test_result == GDScriptDecomp::BYTECODE_TEST_RESULT::BYTECODE_TEST_PASS) {
				// we don't need to test further, this revision is the highest possible for bytecode version 10
				ed80f45_passed = true;
				rev = 0xed80f45;
				break;
			}
		}
		if (!_85585c7_failed && !_85585c7_passed) {
			auto test_result = decomp_85585c7->test_bytecode(data);
			if (test_result == GDScriptDecomp::BYTECODE_TEST_RESULT::BYTECODE_TEST_FAIL) {
				_85585c7_failed = true;
			} else if (test_result == GDScriptDecomp::BYTECODE_TEST_RESULT::BYTECODE_TEST_PASS) {
				_85585c7_passed = true;
				if (ed80f45_failed) {
					// no need to go further
					rev = 0x85585c7;
					break;
				}
			}
		}
		if (!_7124599_failed && !_7124599_passed) {
			auto test_result = decomp_7124599->test_bytecode(data);
			if (test_result == GDScriptDecomp::BYTECODE_TEST_RESULT::BYTECODE_TEST_FAIL) {
				_7124599_failed = true;
			} else if (test_result == GDScriptDecomp::BYTECODE_TEST_RESULT::BYTECODE_TEST_PASS) {
				// doesn't currently have a pass condition, but we check it anyway if we come up with something later
				_7124599_passed = true;
			}
		}
		if (ed80f45_failed && _85585c7_failed && _7124599_failed) {
			// Welp
			break;
		}
	}
	if (rev == 0) {
		// ed80f45 didn't pass, but this passed, so it's probably this version.
		if (_85585c7_passed) {
			rev = 0x85585c7;
		} else if (ed80f45_failed) {
			// 7124599 doesn't currently have a pass condition; if the others have failed and this has not, then it's probably this version.
			if (!_7124599_failed) {
				if (_85585c7_failed) {
					rev = 0x7124599;
				} else {
					rev = 0x85585c7;
				}
			}
		} else if ((!ed80f45_failed && !_85585c7_failed && !_7124599_failed)) {
			// It likely won't matter what we decompile as, just return the highest revision.
			rev = 0xed80f45;
		}
	}
	memdelete(decomp_ed80f45);
	memdelete(decomp_85585c7);
	memdelete(decomp_7124599);
	return rev;
}

// don't use this, just parse the binary for the version string
uint64_t test_files_3_1(const Vector<String> &p_paths) {
	// only need to test 514a3fb and 1a36141
	uint64_t rev = 0;
	bool _514a3fb_failed = false;
	bool _1a36141_failed = false;

	GDScriptDecomp_514a3fb *decomp_514a3fb = memnew(GDScriptDecomp_514a3fb);
	GDScriptDecomp_1a36141 *decomp_1a36141 = memnew(GDScriptDecomp_1a36141);

	// Neither of these tests have pass cases, only fail cases.
	for (String path : p_paths) {
		Vector<uint8_t> data = FileAccess::get_file_as_bytes(path);
		if (data.size() == 0) {
			continue;
		}
		if (!_514a3fb_failed) {
			auto test_result = decomp_514a3fb->test_bytecode(data);
			if (test_result == GDScriptDecomp::BYTECODE_TEST_RESULT::BYTECODE_TEST_FAIL) {
				_514a3fb_failed = true;
			}
		}
		if (!_1a36141_failed) {
			auto test_result = decomp_1a36141->test_bytecode(data);
			if (test_result == GDScriptDecomp::BYTECODE_TEST_RESULT::BYTECODE_TEST_FAIL) {
				_1a36141_failed = true;
			}
		}
		if (_514a3fb_failed && _1a36141_failed) {
			// Welp
			break;
		}
	}

	if (rev == 0) {
		if (!_514a3fb_failed && !_1a36141_failed) {
			// won't matter, just use highest revision
			rev = 0x514a3fb;
		}
		if (_514a3fb_failed && !_1a36141_failed) {
			rev = 0x1a36141;
		}
	}

	memdelete(decomp_514a3fb);
	memdelete(decomp_1a36141);
	return rev;
}

uint64_t BytecodeTester::test_files(const Vector<String> &p_paths, int ver_major, int ver_minor) {
	uint64_t rev = 0;
	if (ver_major == 2 && ver_minor == 1) {
		return test_files_2_1(p_paths);
	} else if (ver_major == 3 && ver_minor == 1) {
		return test_files_3_1(p_paths);
	} else {
		return 0;
	}
	return rev;
}
