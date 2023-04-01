#include "gdre_packed_source.h"
#include "core/io/file_access_encrypted.h"
#include "gdre_settings.h"

uint64_t get_offset_unix(const String &p_path) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);

	if (f.is_null()) {
		return 0;
	}

	// Read and check ELF magic number.
	{
		uint32_t magic = f->get_32();
		if (magic != 0x464c457f) { // 0x7F + "ELF"
			return 0;
		}
	}

	// Read program architecture bits from class field.
	int bits = f->get_8() * 32;

	// Get info about the section header table.
	int64_t section_table_pos;
	int64_t section_header_size;
	if (bits == 32) {
		section_header_size = 40;
		f->seek(0x20);
		section_table_pos = f->get_32();
		f->seek(0x30);
	} else { // 64
		section_header_size = 64;
		f->seek(0x28);
		section_table_pos = f->get_64();
		f->seek(0x3c);
	}
	int num_sections = f->get_16();
	int string_section_idx = f->get_16();

	// Load the strings table.
	uint8_t *strings;
	{
		// Jump to the strings section header.
		f->seek(section_table_pos + string_section_idx * section_header_size);

		// Read strings data size and offset.
		int64_t string_data_pos;
		int64_t string_data_size;
		if (bits == 32) {
			f->seek(f->get_position() + 0x10);
			string_data_pos = f->get_32();
			string_data_size = f->get_32();
		} else { // 64
			f->seek(f->get_position() + 0x18);
			string_data_pos = f->get_64();
			string_data_size = f->get_64();
		}

		// Read strings data.
		f->seek(string_data_pos);
		strings = (uint8_t *)memalloc(string_data_size);
		if (!strings) {
			return 0;
		}
		f->get_buffer(strings, string_data_size);
	}

	// Search for the "pck" section.
	int64_t off = 0;
	for (int i = 0; i < num_sections; ++i) {
		int64_t section_header_pos = section_table_pos + i * section_header_size;
		f->seek(section_header_pos);

		uint32_t name_offset = f->get_32();
		if (strcmp((char *)strings + name_offset, "pck") == 0) {
			if (bits == 32) {
				f->seek(section_header_pos + 0x10);
				off = f->get_32();
			} else { // 64
				f->seek(section_header_pos + 0x18);
				off = f->get_64();
			}
			break;
		}
	}
	memfree(strings);
	return off;
}

uint64_t get_offset_windows(const String &p_path) {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	if (f.is_null()) {
		return 0;
	}
	// Process header.
	{
		f->seek(0x3c);
		uint32_t pe_pos = f->get_32();

		f->seek(pe_pos);
		uint32_t magic = f->get_32();
		if (magic != 0x00004550) {
			return 0;
		}
	}

	int num_sections;
	{
		int64_t header_pos = f->get_position();

		f->seek(header_pos + 2);
		num_sections = f->get_16();
		f->seek(header_pos + 16);
		uint16_t opt_header_size = f->get_16();

		// Skip rest of header + optional header to go to the section headers.
		f->seek(f->get_position() + 2 + opt_header_size);
	}
	int64_t section_table_pos = f->get_position();

	// Search for the "pck" section.
	int64_t off = 0;
	for (int i = 0; i < num_sections; ++i) {
		int64_t section_header_pos = section_table_pos + i * 40;
		f->seek(section_header_pos);

		uint8_t section_name[9];
		f->get_buffer(section_name, 8);
		section_name[8] = '\0';

		if (strcmp((char *)section_name, "pck") == 0) {
			f->seek(section_header_pos + 20);
			off = f->get_32();
			break;
		}
	}
	return off;
}

bool seek_offset_from_exe(Ref<FileAccess> f, const String &p_path, uint64_t p_offset) {
	uint64_t off = 0;
	bool pck_header_found = false;
	uint32_t magic = 0;
	// Loading with offset feature not supported for self contained exe files.
	if (p_offset != 0) {
		ERR_FAIL_V_MSG(false, "Loading self-contained executable with offset not supported.");
	}

	int64_t pck_off = p_path.get_extension() == "exe" ? get_offset_windows(p_path) : get_offset_unix(p_path);
	if (pck_off != 0) {
		// Search for the header, in case PCK start and section have different alignment.
		for (int i = 0; i < 8; i++) {
			f->seek(pck_off);
			magic = f->get_32();
			if (magic == PACK_HEADER_MAGIC) {
#ifdef DEBUG_ENABLED
				print_verbose("PCK header found in executable pck section, loading from offset 0x" + String::num_int64(pck_off - 4, 16));
#endif
				return true;
			}
			pck_off++;
		}
	}

	// Search for the header at the end of file - self contained executable.
	if (!pck_header_found) {
		// Loading with offset feature not supported for self contained exe files.
		if (p_offset != 0) {
			ERR_FAIL_V_MSG(false, "Loading self-contained executable with offset not supported.");
		}

		f->seek_end();
		f->seek(f->get_position() - 4);
		magic = f->get_32();

		if (magic == PACK_HEADER_MAGIC) {
			f->seek(f->get_position() - 12);
			uint64_t ds = f->get_64();
			f->seek(f->get_position() - ds - 8);
			magic = f->get_32();
			if (magic == PACK_HEADER_MAGIC) {
#ifdef DEBUG_ENABLED
				print_verbose("PCK header found at the end of executable, loading from offset 0x" + String::num_int64(f->get_position() - 4, 16));
#endif
				return true;
			}
		}
	}
	return false;
}

bool GDREPackedSource::try_open_pack(const String &p_path, bool p_replace_files, uint64_t p_offset) {
	if (p_path.get_extension().to_lower() == "apk" || p_path.get_extension().to_lower() == "zip") {
		return false;
	}
	String pck_path = p_path.replace("_GDRE_a_really_dumb_hack", "");
	Ref<FileAccess> f = FileAccess::open(pck_path, FileAccess::READ);
	if (f.is_null()) {
		return false;
	}

	f->seek(p_offset);

	bool is_exe = false;
	uint32_t magic = f->get_32();

	if (magic != PACK_HEADER_MAGIC) {
		if (!seek_offset_from_exe(f, p_path, p_offset)) {
			return false;
		}
		is_exe = true;
	}

	uint32_t version = f->get_32();
	uint32_t ver_major = f->get_32();
	uint32_t ver_minor = f->get_32();
	uint32_t ver_rev = f->get_32(); // patch number, did not start getting set to anything other than 0 until 3.2

	if (version > PACK_FORMAT_VERSION) {
		ERR_FAIL_V_MSG(false, "Pack version unsupported: " + itos(version) + ".");
	}

	uint32_t pack_flags = 0;
	uint64_t file_base = 0;

	if (version == 2) {
		pack_flags = f->get_32();
		file_base = f->get_64();
	}

	bool enc_directory = (pack_flags & PACK_DIR_ENCRYPTED);

	for (int i = 0; i < 16; i++) {
		//reserved
		f->get_32();
	}

	uint32_t file_count = f->get_32();

	if (enc_directory) {
		Ref<FileAccessEncrypted> fae = memnew(FileAccessEncrypted);
		if (fae.is_null()) {
			GDRESettings::get_singleton()->_set_error_encryption(true);
			ERR_FAIL_V_MSG(false, "Can't open encrypted pack directory.");
		}

		Vector<uint8_t> key;
		key.resize(32);
		for (int i = 0; i < key.size(); i++) {
			key.write[i] = script_encryption_key[i];
		}

		Error err = fae->open_and_parse(f, key, FileAccessEncrypted::MODE_READ, false);
		if (err) {
			GDRESettings::get_singleton()->_set_error_encryption(true);
			ERR_FAIL_V_MSG(false, "Can't open encrypted pack directory.");
		}
		f = fae;
	}
	String ver_string;

	// they only started writing the actual patch number in 3.2
	if (ver_major < 3 || (ver_major == 3 && ver_minor < 2)) {
		ver_string = itos(ver_major) + "." + itos(ver_minor) + ".x";
	} else {
		ver_string = itos(ver_major) + "." + itos(ver_minor) + "." + itos(ver_rev);
	}

	// Everything worked, now set the data
	Ref<GDRESettings::PackInfo> pckinfo;
	pckinfo.instantiate();
	pckinfo->init(
			pck_path, ver_major, ver_minor, ver_rev, version, pack_flags, file_base, file_count,
			ver_string, is_exe ? GDRESettings::PackInfo::EXE : GDRESettings::PackInfo::PCK);
	GDRESettings::get_singleton()->add_pack_info(pckinfo);

	for (uint32_t i = 0; i < file_count; i++) {
		uint32_t sl = f->get_32();
		CharString cs;
		cs.resize(sl + 1);
		f->get_buffer((uint8_t *)cs.ptr(), sl);
		cs[sl] = 0;

		String path;
		path.parse_utf8(cs.ptr());

		ERR_FAIL_COND_V_MSG(path.get_file().find("gdre_") != -1, false, "Don't try to extract the GDRE pack files, just download the source from github.");

		uint64_t ofs = file_base + f->get_64();
		uint64_t size = f->get_64();
		uint8_t md5[16];
		uint32_t flags = 0;
		f->get_buffer(md5, 16);
		if (version == 2) {
			flags = f->get_32();
		}
		// add the file info to settings
		PackedData::PackedFile pf;
		pf.offset = ofs + p_offset;
		memcpy(pf.md5, md5, 16);
		pf.size = size;
		pf.encrypted = flags & PACK_FILE_ENCRYPTED;
		pf.pack = p_path;
		pf.src = this;
		Ref<PackedFileInfo> pf_info;
		pf_info.instantiate();
		pf_info->init(path, &pf);
		GDRESettings::get_singleton()->add_pack_file(pf_info);
		// use the corrected path, not the raw path
		path = pf_info->get_path();
		PackedData::get_singleton()->add_path(pck_path, path, ofs + p_offset, size, md5, this, p_replace_files, (flags & PACK_FILE_ENCRYPTED));
	}

	return true;
}
Ref<FileAccess> GDREPackedSource::get_file(const String &p_path, PackedData::PackedFile *p_file) {
	return memnew(FileAccessPack(p_path, *p_file));
}