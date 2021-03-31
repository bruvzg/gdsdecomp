#include "file_access_pack_compat.h"
#include "core/os/file_access.h"
#include "core/io/file_access_encrypted.h"
//We have to reimplement this whole goddamn class because it references a global for the key.

Error FileAccessPackCompat::_open(const String &p_path, int p_mode_flags) {
	ERR_FAIL_V(ERR_UNAVAILABLE);
	return ERR_UNAVAILABLE;
}

void FileAccessPackCompat::close() {
	f->close();
}

bool FileAccessPackCompat::is_open() const {
	return f->is_open();
}

void FileAccessPackCompat::seek(size_t p_position) {
	if (p_position > pf.size) {
		eof = true;
	} else {
		eof = false;
	}

	f->seek(off + p_position);
	pos = p_position;
}

void FileAccessPackCompat::seek_end(int64_t p_position) {
	seek(pf.size + p_position);
}

size_t FileAccessPackCompat::get_position() const {
	return pos;
}

size_t FileAccessPackCompat::get_len() const {
	return pf.size;
}

bool FileAccessPackCompat::eof_reached() const {
	return eof;
}

uint8_t FileAccessPackCompat::get_8() const {
	if (pos >= pf.size) {
		eof = true;
		return 0;
	}

	pos++;
	return f->get_8();
}

int FileAccessPackCompat::get_buffer(uint8_t *p_dst, int p_length) const {
	ERR_FAIL_COND_V(!p_dst && p_length > 0, -1);
	ERR_FAIL_COND_V(p_length < 0, -1);

	if (eof) {
		return 0;
	}

	uint64_t to_read = p_length;
	if (to_read + pos > pf.size) {
		eof = true;
		to_read = int64_t(pf.size) - int64_t(pos);
	}

	pos += p_length;

	if (to_read <= 0) {
		return 0;
	}
	f->get_buffer(p_dst, to_read);

	return to_read;
}

void FileAccessPackCompat::set_endian_swap(bool p_swap) {
	FileAccess::set_endian_swap(p_swap);
	f->set_endian_swap(p_swap);
}

Error FileAccessPackCompat::get_error() const {
	if (eof) {
		return ERR_FILE_EOF;
	}
	return OK;
}

void FileAccessPackCompat::flush() {
	ERR_FAIL();
}

void FileAccessPackCompat::store_8(uint8_t p_dest) {
	ERR_FAIL();
}

void FileAccessPackCompat::store_buffer(const uint8_t *p_src, int p_length) {
	ERR_FAIL();
}

bool FileAccessPackCompat::file_exists(const String &p_name) {
	return false;
}

FileAccessPackCompat::FileAccessPackCompat(const String &p_path, const PackedData::PackedFile &p_file, const Vector<uint8_t> &script_encryption_key) :
		pf(p_file),
		f(FileAccess::open(pf.pack, FileAccess::READ)) {
	ERR_FAIL_COND_MSG(!f, "Can't open pack-referenced file '" + String(pf.pack) + "'.");

	f->seek(pf.offset);
	off = pf.offset;

	if (pf.encrypted) {
		FileAccessEncrypted *fae = memnew(FileAccessEncrypted);
		if (!fae) {
			ERR_FAIL_MSG("Can't open encrypted pack-referenced file '" + String(pf.pack) + "'.");
		}

		Vector<uint8_t> key;
		key.resize(32);
		for (int i = 0; i < key.size(); i++) {
			key.write[i] = script_encryption_key[i];
		}

		Error err = fae->open_and_parse(f, key, FileAccessEncrypted::MODE_READ, false);
		if (err) {
			memdelete(fae);
			ERR_FAIL_MSG("Can't open encrypted pack-referenced file '" + String(pf.pack) + "'.");
		}
		f = fae;
		off = 0;
	}
	pos = 0;
	eof = false;
}

FileAccessPackCompat::~FileAccessPackCompat() {
	if (f) {
		f->close();
		memdelete(f);
	}
}
