#include "core/io/file_access_pack.h"

class FileAccessPackCompat : public FileAccess {
	PackedData::PackedFile pf;
	mutable size_t pos;
	mutable bool eof;
	uint64_t off;
	FileAccess *f;
	virtual Error _open(const String &p_path, int p_mode_flags);
	virtual uint64_t _get_modified_time(const String &p_file) { return 0; }
	virtual uint32_t _get_unix_permissions(const String &p_file) { return 0; }
	virtual Error _set_unix_permissions(const String &p_file, uint32_t p_permissions) { return FAILED; }

public:
	virtual void close();
	virtual bool is_open() const;
	virtual void seek(size_t p_position);
	virtual void seek_end(int64_t p_position = 0);
	virtual size_t get_position() const;
	virtual size_t get_len() const;
	virtual bool eof_reached() const;
	virtual uint8_t get_8() const;
	virtual int get_buffer(uint8_t *p_dst, int p_length) const;
	virtual void set_endian_swap(bool p_swap);
	virtual Error get_error() const;
	virtual void flush();
	virtual void store_8(uint8_t p_dest);
	virtual void store_buffer(const uint8_t *p_src, int p_length);
	virtual bool file_exists(const String &p_name);
	FileAccessPackCompat(const String &p_path, const PackedData::PackedFile &p_file, const Vector<uint8_t> &script_encryption_key);
	~FileAccessPackCompat();
};