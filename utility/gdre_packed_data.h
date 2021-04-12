#ifndef GDRE_PACKED_DATA_H
#define GDRE_PACKED_DATA_H

#include "core/object/reference.h"
#include "packed_file_info.h"

class GDREPackedSource : public PackSource {
public:
	virtual bool try_open_pack(const String &p_path, bool p_replace_files, size_t p_offset);
	virtual FileAccess *get_file(const String &p_path, PackedData::PackedFile *p_file);
};

class FileAccessGDRE : public FileAccess {
public:
	static FileAccess *open(const String &p_path, int p_mode_flags, Error *r_error = nullptr); /// Create a file access (for the current platform) this is the only portable way of accessing files.
	static bool exists(const String &pname);
};


#endif // GDRE_PACKED_DATA
