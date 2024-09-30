
#include "core/io/marshalls.h"

namespace gdre{
bool check_header(const Vector<uint8_t> &p_buffer, const char* p_expected_header, int p_expected_len);
}

// Can only pass in string literals
#define _GDRE_CHECK_HEADER(p_buffer, p_expected_header) gdre::check_header(p_buffer, p_expected_header, sizeof(p_expected_header) - 1)