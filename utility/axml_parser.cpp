#include "axml_parser.h"

#include <core/io/marshalls.h>

Error AXMLParser::parse_manifest(Vector<uint8_t> &p_manifest) {
	// Leaving the unused types commented because looking these constants up
	// again later would be annoying
	// const int CHUNK_AXML_FILE = 0x00080003;
	// const int CHUNK_RESOURCEIDS = 0x00080180;
	const int CHUNK_STRINGS = 0x001C0001;
	// const int CHUNK_XML_END_NAMESPACE = 0x00100101;
	const int CHUNK_XML_END_TAG = 0x00100103;
	// const int CHUNK_XML_START_NAMESPACE = 0x00100100;
	const int CHUNK_XML_START_TAG = 0x00100102;
	// const int CHUNK_XML_TEXT = 0x00100104;
	const int UTF8_FLAG = 0x00000100;

	Vector<String> string_table;

	uint32_t ofs = 8;

	uint32_t string_count = 0;
	uint32_t string_flags = 0;
	uint32_t string_data_offset = 0;

	uint32_t string_table_begins = 0;
	uint32_t string_table_ends = 0;
	Vector<uint8_t> stable_extra;

	while (ofs < (uint32_t)p_manifest.size()) {
		uint32_t chunk = decode_uint32(&p_manifest[ofs]);
		uint32_t size = decode_uint32(&p_manifest[ofs + 4]);

		switch (chunk) {
			case CHUNK_STRINGS: {
				int iofs = ofs + 8;

				string_count = decode_uint32(&p_manifest[iofs]);
				string_flags = decode_uint32(&p_manifest[iofs + 8]);
				string_data_offset = decode_uint32(&p_manifest[iofs + 12]);

				uint32_t st_offset = iofs + 20;
				string_table.resize(string_count);
				uint32_t string_end = 0;

				string_table_begins = st_offset;

				for (uint32_t i = 0; i < string_count; i++) {
					uint32_t string_at = decode_uint32(&p_manifest[st_offset + i * 4]);
					string_at += st_offset + string_count * 4;

					ERR_FAIL_COND_V_MSG(string_flags & UTF8_FLAG, ERR_FILE_CORRUPT, "Unimplemented, can't read UTF-8 string table.");

					if (string_flags & UTF8_FLAG) {
					} else {
						uint32_t len = decode_uint16(&p_manifest[string_at]);
						Vector<char32_t> ucstring;
						ucstring.resize(len + 1);
						for (uint32_t j = 0; j < len; j++) {
							uint16_t c = decode_uint16(&p_manifest[string_at + 2 + 2 * j]);
							ucstring.write[j] = c;
						}
						string_end = MAX(string_at + 2 + 2 * len, string_end);
						ucstring.write[len] = 0;
						string_table.write[i] = ucstring.ptr();
					}
				}

				for (uint32_t i = string_end; i < (ofs + size); i++) {
					stable_extra.push_back(p_manifest[i]);
				}

				string_table_ends = ofs + size;

			} break;
			case CHUNK_XML_START_TAG: {
				int iofs = ofs + 8;
				uint32_t name = decode_uint32(&p_manifest[iofs + 12]);

				String tname = string_table[name];
				uint32_t attrcount = decode_uint32(&p_manifest[iofs + 20]);
				iofs += 28;
				String previous_attrname = "";
				String previous_value = "";

				for (uint32_t i = 0; i < attrcount; i++) {
					bool get_version_num = false;
					uint32_t attr_nspace = decode_uint32(&p_manifest[iofs]);
					uint32_t attr_name = decode_uint32(&p_manifest[iofs + 4]);
					uint32_t attr_value = decode_uint32(&p_manifest[iofs + 8]);
					uint32_t attr_resid = decode_uint32(&p_manifest[iofs + 16]);

					const String value = (attr_value != 0xFFFFFFFF) ? string_table[attr_value] : "Res #" + itos(attr_resid);
					String attrname = string_table[attr_name];
					const String nspace = (attr_nspace != 0xFFFFFFFF) ? string_table[attr_nspace] : "";
					//replace project information
					if (tname == "manifest" && attrname == "package") {
						package_name = string_table[attr_value];
					}

					if (tname == "manifest" && attrname == "versionCode") {
						version_code = decode_uint32(&p_manifest[iofs + 16]);
					}

					if (tname == "manifest" && attrname == "versionName") {
						if (attr_value == 0xFFFFFFFF) {
							WARN_PRINT("Version name in a resource, should be plain text");
						} else {
							version_name = string_table[attr_value];
						}
					}

					if (tname == "activity" && attrname == "screenOrientation") {
						screen_orientation = decode_uint32(&p_manifest.write[iofs + 16]);
					}

					if (tname == "supports-screens") {
						if (attrname == "smallScreens") {
							screen_support_small = decode_uint32(&p_manifest.write[iofs + 16]);

						} else if (attrname == "normalScreens") {
							screen_support_normal = decode_uint32(&p_manifest.write[iofs + 16]);

						} else if (attrname == "largeScreens") {
							screen_support_large = decode_uint32(&p_manifest.write[iofs + 16]);

						} else if (attrname == "xlargeScreens") {
							screen_support_xlarge = decode_uint32(&p_manifest.write[iofs + 16]);
						}
					}

					//meta-data tags attrs are "name" = "<name>" "value" = "<value>"
					if (tname == "meta-data" && previous_attrname == "name" && previous_value == "org.godotengine.editor.version") {
						godot_editor_version = string_table[attr_value];
					}

					if (tname == "meta-data" && previous_attrname == "name" && previous_value == "org.godotengine.library.version") {
						godot_library_version = string_table[attr_value];
					}
					previous_attrname = attrname;
					previous_value = value;
					iofs += 20;
				}

			} break;
			case CHUNK_XML_END_TAG: {
				int iofs = ofs + 8;
				uint32_t name = decode_uint32(&p_manifest[iofs + 12]);
				String tname = string_table[name];
				// don't care about these
			} break;
		}

		ofs += size;
	}

	return OK;
}
