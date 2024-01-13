#include "packed_file_info.h"

void PackedFileInfo::_bind_methods() {
	ClassDB::bind_method(D_METHOD("get_pack"), &PackedFileInfo::get_pack);
	ClassDB::bind_method(D_METHOD("get_path"), &PackedFileInfo::get_path);
	ClassDB::bind_method(D_METHOD("get_raw_path"), &PackedFileInfo::get_raw_path);
	ClassDB::bind_method(D_METHOD("get_offset"), &PackedFileInfo::get_offset);
	ClassDB::bind_method(D_METHOD("get_size"), &PackedFileInfo::get_size);
	ClassDB::bind_method(D_METHOD("get_md5"), &PackedFileInfo::get_md5);
	ClassDB::bind_method(D_METHOD("has_md5"), &PackedFileInfo::has_md5);
	ClassDB::bind_method(D_METHOD("is_malformed"), &PackedFileInfo::is_malformed);
	ClassDB::bind_method(D_METHOD("is_encrypted"), &PackedFileInfo::is_encrypted);
	ClassDB::bind_method(D_METHOD("is_checksum_validated"), &PackedFileInfo::is_checksum_validated);
}