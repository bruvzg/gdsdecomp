#pragma once

#include "core/io/file_access_pack.h"

class GDREPackedSource : public PackSource {
public:
	virtual bool try_open_pack(const String &p_path, bool p_replace_files, uint64_t p_offset);
	virtual Ref<FileAccess> get_file(const String &p_path, PackedData::PackedFile *p_file);
};
