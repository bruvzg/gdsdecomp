#include "utility/common.h"
#include "core/io/dir_access.h"

Error gdre::ensure_dir(const String &dst_dir) {
	Error err;
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	ERR_FAIL_COND_V(da.is_null(), ERR_FILE_CANT_OPEN);
	err = da->make_dir_recursive(dst_dir);
	return err;
}

bool gdre::check_header(const Vector<uint8_t> &p_buffer, const char *p_expected_header, int p_expected_len) {
	if (p_buffer.size() < p_expected_len) {
		return false;
	}

	for (int i = 0; i < p_expected_len; i++) {
		if (p_buffer[i] != p_expected_header[i]) {
			return false;
		}
	}

	return true;
}
