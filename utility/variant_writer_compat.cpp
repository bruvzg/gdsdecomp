#include "variant_writer_compat.h"
#include "image_parser_v2.h"
#include "input_event_parser_v2.h"

static String rtosfix(double p_value) {
	if (p_value == 0.0)
		return "0"; // avoid negative zero (-0) being written, which may annoy git, svn, etc. for changes when they don't exist.
	else
		return rtoss(p_value);
}

// Don't think this is necessary at the moment? The only objects that get stored inline in text resources are Position3D objects and those are unchanged from Godot 2.x
Error VariantParserCompat::fake_parse_object(VariantParser::Token &token, Variant &r_value, VariantParser::Stream *p_stream, int &line, String &r_err_str, VariantParser::ResourceParser *p_res_parser) {
	return parse_value(token, r_value, p_stream, line, r_err_str, p_res_parser);
}
Error VariantParserCompat::parse_tag(VariantParser::Stream *p_stream, int &line, String &r_err_str, Tag &r_tag, VariantParser::ResourceParser *p_res_parser, bool p_simple_tag) {
	return VariantParser::parse_tag(p_stream, line, r_err_str, r_tag, p_res_parser, p_simple_tag);
}

Error VariantParserCompat::parse_tag_assign_eof(VariantParser::Stream *p_stream, int &line, String &r_err_str, Tag &r_tag, String &r_assign, Variant &r_value, VariantParser::ResourceParser *p_res_parser, bool p_simple_tag) {
	// assign..
	r_assign = "";
	String what;

	while (true) {
		char32_t c;
		if (p_stream->saved) {
			c = p_stream->saved;
			p_stream->saved = 0;
		} else {
			c = p_stream->get_char();
		}

		if (p_stream->is_eof()) {
			return ERR_FILE_EOF;
		}

		if (c == ';') { // comment
			while (true) {
				char32_t ch = p_stream->get_char();
				if (p_stream->is_eof()) {
					return ERR_FILE_EOF;
				}
				if (ch == '\n') {
					break;
				}
			}
			continue;
		}

		if (c == '[' && what.length() == 0) {
			// it's a tag!
			p_stream->saved = '['; // go back one
			Error err = parse_tag(p_stream, line, r_err_str, r_tag, p_res_parser, p_simple_tag);
			return err;
		}

		if (c > 32) {
			if (c == '"') { // quoted
				p_stream->saved = '"';
				Token tk;
				Error err = get_token(p_stream, tk, line, r_err_str);
				if (err) {
					return err;
				}
				if (tk.type != TK_STRING) {
					r_err_str = "Error reading quoted string";
					return ERR_INVALID_DATA;
				}

				what = tk.value;

			} else if (c != '=') {
				what += String::chr(c);
			} else {
				r_assign = what;
				Token token;
				get_token(p_stream, token, line, r_err_str);
				Error err;
				// VariantParserCompat hacks for compatibility
				if (token.type == TK_IDENTIFIER) {
					String id = token.value;
					// Old V2 Image
					if (id == "Image") {
						err = ImageParserV2::parse_image_construct_v2(p_stream, r_value, true, line, r_err_str);
					} else if (id == "InputEvent") {
						err = InputEventParserV2::parse_input_event_construct_v2(p_stream, r_value, line, r_err_str);
					} else if (id == "Object") {
						err = fake_parse_object(token, r_value, p_stream, line, r_err_str, p_res_parser);
					} else {
						err = parse_value(token, r_value, p_stream, line, r_err_str, p_res_parser);
					}
				}
				// end hacks
				err = parse_value(token, r_value, p_stream, line, r_err_str, p_res_parser);
				return err;
			}
		} else if (c == '\n') {
			line++;
		}
	}
	return OK;
}

Error VariantWriterCompat::write_compat(const Variant &p_variant, const uint32_t ver_major, StoreStringFunc p_store_string_func, void *p_store_string_ud, EncodeResourceFunc p_encode_res_func, void *p_encode_res_ud, bool is_pcfg) {
	// use the v4 write function instead for v4
	if (ver_major == 4) {
		return VariantWriter::write(p_variant, p_store_string_func, p_store_string_ud, p_encode_res_func, p_encode_res_ud);
	}

	// for v2 and v3...
	switch ((Variant::Type)p_variant.get_type()) {
		case Variant::Type::NIL: {
			p_store_string_func(p_store_string_ud, "null");
		} break;
		case Variant::BOOL: {
			p_store_string_func(p_store_string_ud, p_variant.operator bool() ? "true" : "false");
		} break;
		case Variant::INT: {
			p_store_string_func(p_store_string_ud, itos(p_variant.operator int()));
		} break;
		case Variant::FLOAT: { // "REAL" in v2 and v3
			String s = rtosfix(p_variant.operator real_t());
			if (s.find(".") == -1 && s.find("e") == -1)
				s += ".0";
			p_store_string_func(p_store_string_ud, s);
		} break;
		case Variant::STRING: {
			String str = p_variant;

			str = "\"" + str.c_escape_multiline() + "\"";
			p_store_string_func(p_store_string_ud, str);
		} break;
		case Variant::VECTOR2: {
			Vector2 v = p_variant;
			p_store_string_func(p_store_string_ud, "Vector2( " + rtosfix(v.x) + ", " + rtosfix(v.y) + " )");
		} break;
		case Variant::RECT2: {
			Rect2 aabb = p_variant;
			p_store_string_func(p_store_string_ud, "Rect2( " + rtosfix(aabb.position.x) + ", " + rtosfix(aabb.position.y) + ", " + rtosfix(aabb.size.x) + ", " + rtosfix(aabb.size.y) + " )");

		} break;
		case Variant::VECTOR3: {
			Vector3 v = p_variant;
			p_store_string_func(p_store_string_ud, "Vector3( " + rtosfix(v.x) + ", " + rtosfix(v.y) + ", " + rtosfix(v.z) + " )");
		} break;
		case Variant::TRANSFORM2D: { // v2 Matrix32
			String s = ver_major == 2 ? "Matrix32( " : "Transform2D( ";
			Transform2D m3 = p_variant;
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 2; j++) {
					if (i != 0 || j != 0)
						s += ", ";
					s += rtosfix(m3.columns[i][j]);
				}
			}

			p_store_string_func(p_store_string_ud, s + " )");

		} break;
		case Variant::PLANE: {
			Plane p = p_variant;
			p_store_string_func(p_store_string_ud, "Plane( " + rtosfix(p.normal.x) + ", " + rtosfix(p.normal.y) + ", " + rtosfix(p.normal.z) + ", " + rtosfix(p.d) + " )");

		} break;
		case Variant::AABB: { // _AABB in v2
			AABB aabb = p_variant;
			p_store_string_func(p_store_string_ud, "AABB( " + rtosfix(aabb.position.x) + ", " + rtosfix(aabb.position.y) + ", " + rtosfix(aabb.position.z) + ", " + rtosfix(aabb.size.x) + ", " + rtosfix(aabb.size.y) + ", " + rtosfix(aabb.size.z) + " )");

		} break;
		case Variant::QUATERNION: { // QUAT in v2 and v3
			Quaternion quat = p_variant;
			p_store_string_func(p_store_string_ud, "Quat( " + rtosfix(quat.x) + ", " + rtosfix(quat.y) + ", " + rtosfix(quat.z) + ", " + rtosfix(quat.w) + " )");

		} break;

		case Variant::BASIS: { // v2 Matrix3

			String s = ver_major == 2 ? "Matrix3( " : "Basis( ";
			Basis m3 = p_variant;
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 3; j++) {
					if (i != 0 || j != 0)
						s += ", ";
					s += rtosfix(m3.rows[i][j]);
				}
			}

			p_store_string_func(p_store_string_ud, s + " )");

		} break;
		case Variant::TRANSFORM3D: { // TRANSFORM in v2 and v3
			String s = "Transform( ";
			Transform3D t = p_variant;
			Basis &m3 = t.basis;
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 3; j++) {
					if (i != 0 || j != 0)
						s += ", ";
					s += rtosfix(m3.rows[i][j]);
				}
			}

			s = s + ", " + rtosfix(t.origin.x) + ", " + rtosfix(t.origin.y) + ", " + rtosfix(t.origin.z);

			p_store_string_func(p_store_string_ud, s + " )");
		} break;
		case Variant::COLOR: {
			Color c = p_variant;
			if (ver_major == 2 && is_pcfg) {
				p_store_string_func(p_store_string_ud, "#" + c.to_html());
			} else {
				p_store_string_func(p_store_string_ud, "Color( " + rtosfix(c.r) + ", " + rtosfix(c.g) + ", " + rtosfix(c.b) + ", " + rtosfix(c.a) + " )");
			}

		} break;
		case Variant::NODE_PATH: {
			String str = p_variant;

			str = "NodePath(\"" + str.c_escape() + "\")";
			p_store_string_func(p_store_string_ud, str);

		} break;

		case Variant::OBJECT: {
			Object *obj = p_variant;

			if (!obj) {
				p_store_string_func(p_store_string_ud, "null");
				break; // don't save it
			}

			Ref<Resource> res = p_variant;
			if (res.is_valid()) {
				// is resource
				String res_text;
				// Hack for V2 Images
				if (ver_major == 2 && res->is_class("Image")) {
					res_text = ImageParserV2::image_v2_to_string(res, is_pcfg);
				} else if (p_encode_res_func) {
					// try external function
					res_text = p_encode_res_func(p_encode_res_ud, res);
				}
				// try path because it's a file
				if (res_text == String() && res->get_path().is_resource_file()) {
					// external resource
					String path = res->get_path();
					res_text = "Resource( \"" + path + "\")";
				}

				// could come up with some sort of text
				if (res_text != String()) {
					p_store_string_func(p_store_string_ud, res_text);
					break;
				}
			}

			// store as generic object

			p_store_string_func(p_store_string_ud, "Object(" + obj->get_class() + ",");

			List<PropertyInfo> props;
			obj->get_property_list(&props);
			bool first = true;
			for (List<PropertyInfo>::Element *E = props.front(); E; E = E->next()) {
				if (E->get().usage & PROPERTY_USAGE_STORAGE || E->get().usage & PROPERTY_USAGE_SCRIPT_VARIABLE) {
					// must be serialized

					if (first) {
						first = false;
					} else {
						p_store_string_func(p_store_string_ud, ",");
					}

					p_store_string_func(p_store_string_ud, "\"" + E->get().name + "\":");
					write_compat(obj->get(E->get().name), ver_major, p_store_string_func, p_store_string_ud, p_encode_res_func, p_encode_res_ud, is_pcfg);
				}
			}

			p_store_string_func(p_store_string_ud, ")\n");

		} break;
		//case Variant::INPUT_EVENT: { // only in V2, does not exist in v3 and v4
		// binary resource formats don't support storing input events,
		// so we don't bother writing them.
		//	WARN_PRINT("Attempted to save Input Event!");
		//} break;
		case Variant::DICTIONARY: {
			Dictionary dict = p_variant;

			List<Variant> keys;
			dict.get_key_list(&keys);
			keys.sort();

			p_store_string_func(p_store_string_ud, "{\n");
			for (List<Variant>::Element *E = keys.front(); E; E = E->next()) {
				/*
				if (!_check_type(dict[E->get()]))
					continue;
				*/
				write_compat(E->get(), ver_major, p_store_string_func, p_store_string_ud, p_encode_res_func, p_encode_res_ud, is_pcfg);
				p_store_string_func(p_store_string_ud, ": ");
				write_compat(dict[E->get()], ver_major, p_store_string_func, p_store_string_ud, p_encode_res_func, p_encode_res_ud, is_pcfg);
				if (E->next())
					p_store_string_func(p_store_string_ud, ",\n");
			}

			p_store_string_func(p_store_string_ud, "\n}");

		} break;
		case Variant::ARRAY: {
			p_store_string_func(p_store_string_ud, "[ ");
			Array array = p_variant;
			int len = array.size();
			for (int i = 0; i < len; i++) {
				if (i > 0)
					p_store_string_func(p_store_string_ud, ", ");
				write_compat(array[i], ver_major, p_store_string_func, p_store_string_ud, p_encode_res_func, p_encode_res_ud, is_pcfg);
			}
			p_store_string_func(p_store_string_ud, " ]");

		} break;

		case Variant::PACKED_BYTE_ARRAY: { // v2 ByteArray, v3 POOL_BYTE_ARRAY
			if (ver_major == 2 && is_pcfg) {
				p_store_string_func(p_store_string_ud, "[ ");
			} else {
				p_store_string_func(p_store_string_ud, ver_major == 2 ? "ByteArray( " : "PoolByteArray( ");
			}
			String s;
			Vector<uint8_t> data = p_variant;
			int len = data.size();

			const uint8_t *ptr = data.ptr();
			for (int i = 0; i < len; i++) {
				if (i > 0)
					p_store_string_func(p_store_string_ud, ", ");

				p_store_string_func(p_store_string_ud, itos(ptr[i]));
			}
			if (ver_major == 2 && is_pcfg) {
				p_store_string_func(p_store_string_ud, " ]");
			} else {
				p_store_string_func(p_store_string_ud, " )");
			}
		} break;
		case Variant::PACKED_INT32_ARRAY: { // v2 IntArray, v3 POOL_INT_ARRAY
			if (ver_major == 2 && is_pcfg) {
				p_store_string_func(p_store_string_ud, "[ ");
			} else {
				p_store_string_func(p_store_string_ud, ver_major == 2 ? "IntArray( " : "PoolIntArray( ");
			}
			Vector<int> data = p_variant;
			int len = data.size();

			const int *ptr = data.ptr();

			for (int i = 0; i < len; i++) {
				if (i > 0)
					p_store_string_func(p_store_string_ud, ", ");

				p_store_string_func(p_store_string_ud, itos(ptr[i]));
			}

			if (ver_major == 2 && is_pcfg) {
				p_store_string_func(p_store_string_ud, " ]");
			} else {
				p_store_string_func(p_store_string_ud, " )");
			}

		} break;
		//case Variant::PACKED_INT64_ARRAY: { // v2 and v3 did not have 64-bit ints
		case Variant::PACKED_FLOAT32_ARRAY: { // v2 FloatArray, v3 POOL_REAL_ARRAY
			if (ver_major == 2 && is_pcfg) {
				p_store_string_func(p_store_string_ud, "[ ");
			} else {
				p_store_string_func(p_store_string_ud, ver_major == 2 ? "FloatArray( " : "PoolRealArray( ");
			}
			Vector<real_t> data = p_variant;
			int len = data.size();
			const real_t *ptr = data.ptr();

			for (int i = 0; i < len; i++) {
				if (i > 0)
					p_store_string_func(p_store_string_ud, ", ");
				p_store_string_func(p_store_string_ud, rtosfix(ptr[i]));
			}

			if (ver_major == 2 && is_pcfg) {
				p_store_string_func(p_store_string_ud, " ]");
			} else {
				p_store_string_func(p_store_string_ud, " )");
			}

		} break;
		//case Variant::PACKED_FLOAT64_ARRAY: { // v2 and v3 did not have 64-bit floats
		case Variant::PACKED_STRING_ARRAY: { // v2 StringArray, v3 POOL_STRING_ARRAY
			if (ver_major == 2 && is_pcfg) {
				p_store_string_func(p_store_string_ud, "[ ");
			} else {
				p_store_string_func(p_store_string_ud, ver_major == 2 ? "StringArray( " : "PoolStringArray( ");
			}
			Vector<String> data = p_variant;
			int len = data.size();

			const String *ptr = data.ptr();
			String s;
			// write_string("\n");

			for (int i = 0; i < len; i++) {
				if (i > 0)
					p_store_string_func(p_store_string_ud, ", ");
				String str = ptr[i];
				p_store_string_func(p_store_string_ud, "\"" + str.c_escape() + "\"");
			}

			if (ver_major == 2 && is_pcfg) {
				p_store_string_func(p_store_string_ud, " ]");
			} else {
				p_store_string_func(p_store_string_ud, " )");
			}

		} break;
		// no pcfg variants for the rest of the arrays
		case Variant::PACKED_VECTOR2_ARRAY: { // v2 Vector2Array, v3 POOL_VECTOR2_ARRAY

			p_store_string_func(p_store_string_ud, ver_major == 2 ? "Vector2Array( " : "PoolVector2Array( ");
			Vector<Vector2> data = p_variant;
			int len = data.size();
			const Vector2 *ptr = data.ptr();

			for (int i = 0; i < len; i++) {
				if (i > 0)
					p_store_string_func(p_store_string_ud, ", ");
				p_store_string_func(p_store_string_ud, rtosfix(ptr[i].x) + ", " + rtosfix(ptr[i].y));
			}

			p_store_string_func(p_store_string_ud, " )");

		} break;
		case Variant::PACKED_VECTOR3_ARRAY: { // v2 Vector3Array, v3 POOL_VECTOR3_ARRAY

			p_store_string_func(p_store_string_ud, ver_major == 2 ? "Vector3Array( " : "PoolVector3Array( ");
			Vector<Vector3> data = p_variant;
			int len = data.size();
			const Vector3 *ptr = data.ptr();

			for (int i = 0; i < len; i++) {
				if (i > 0)
					p_store_string_func(p_store_string_ud, ", ");
				p_store_string_func(p_store_string_ud, rtosfix(ptr[i].x) + ", " + rtosfix(ptr[i].y) + ", " + rtosfix(ptr[i].z));
			}

			p_store_string_func(p_store_string_ud, " )");

		} break;
		case Variant::PACKED_COLOR_ARRAY: { // v2 ColorArray, v3 POOL_COLOR_ARRAY

			p_store_string_func(p_store_string_ud, ver_major == 2 ? "ColorArray( " : "PoolColorArray( ");

			Vector<Color> data = p_variant;
			int len = data.size();

			const Color *ptr = data.ptr();

			for (int i = 0; i < len; i++) {
				if (i > 0)
					p_store_string_func(p_store_string_ud, ", ");

				p_store_string_func(p_store_string_ud, rtosfix(ptr[i].r) + ", " + rtosfix(ptr[i].g) + ", " + rtosfix(ptr[i].b) + ", " + rtosfix(ptr[i].a));
			}
			p_store_string_func(p_store_string_ud, " )");

		} break;
		default: {
		}
	}
	return OK;
}

static Error _write_to_str(void *ud, const String &p_string) {
	String *str = (String *)ud;
	(*str) += p_string;
	return OK;
}
// project.cfg variants are written differently than resource variants in Godot 2.x
Error VariantWriterCompat::write_to_string_pcfg(const Variant &p_variant, String &r_string, const uint32_t ver_major, EncodeResourceFunc p_encode_res_func, void *p_encode_res_ud) {
	r_string = String();
	return write_compat(p_variant, ver_major, _write_to_str, &r_string, p_encode_res_func, p_encode_res_ud, true);
}
Error VariantWriterCompat::write_to_string(const Variant &p_variant, String &r_string, const uint32_t ver_major, EncodeResourceFunc p_encode_res_func, void *p_encode_res_ud) {
	r_string = String();
	return write_compat(p_variant, ver_major, _write_to_str, &r_string, p_encode_res_func, p_encode_res_ud, false);
}
