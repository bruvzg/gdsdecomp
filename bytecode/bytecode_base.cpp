/*************************************************************************/
/*  bytecode_base.cpp                                                    */
/*************************************************************************/

#include "bytecode_base.h"

#include "core/config/engine.h"
#include "core/io/file_access.h"
#include "core/io/file_access_encrypted.h"
#include "core/io/image.h"
#include "core/io/marshalls.h"

#include <limits.h>

#define _S(a) ((int32_t)a)
#define ERR_FAIL_ADD_OF(a, b, err) ERR_FAIL_COND_V(_S(b) < 0 || _S(a) < 0 || _S(a) > INT_MAX - _S(b), err)
#define ERR_FAIL_MUL_OF(a, b, err) ERR_FAIL_COND_V(_S(a) < 0 || _S(b) <= 0 || _S(a) > INT_MAX / _S(b), err)

#define ENCODE_MASK 0xFF
#define ENCODE_FLAG_64 1 << 16
#define ENCODE_FLAG_OBJECT_AS_ID 1 << 16

static Error _decode_string(const uint8_t *&buf, int &len, int *r_len, String &r_string) {
	ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);

	int32_t strlen = decode_uint32(buf);
	int32_t pad = 0;

	// Handle padding
	if (strlen % 4) {
		pad = 4 - strlen % 4;
	}

	buf += 4;
	len -= 4;

	// Ensure buffer is big enough
	ERR_FAIL_ADD_OF(strlen, pad, ERR_FILE_EOF);
	ERR_FAIL_COND_V(strlen < 0 || strlen + pad > len, ERR_FILE_EOF);

	String str;
	ERR_FAIL_COND_V(str.parse_utf8((const char *)buf, strlen), ERR_INVALID_DATA);
	r_string = str;

	// Add padding
	strlen += pad;

	// Update buffer pos, left data count, and return size
	buf += strlen;
	len -= strlen;
	if (r_len) {
		(*r_len) += 4 + strlen;
	}

	return OK;
}

void GDScriptDecomp::_bind_methods() {
	ClassDB::bind_method(D_METHOD("decompile_byte_code", "path"), &GDScriptDecomp::decompile_byte_code);
	ClassDB::bind_method(D_METHOD("decompile_byte_code_encrypted", "path", "key"), &GDScriptDecomp::decompile_byte_code_encrypted);

	ClassDB::bind_method(D_METHOD("get_script_text"), &GDScriptDecomp::get_script_text);
	ClassDB::bind_method(D_METHOD("get_error_message"), &GDScriptDecomp::get_error_message);
}

void GDScriptDecomp::_ensure_space(String &p_code) {
	if (!p_code.ends_with(" ")) {
		p_code += String(" ");
	}
}

Error GDScriptDecomp::decompile_byte_code_encrypted(const String &p_path, Vector<uint8_t> p_key) {
	Vector<uint8_t> bytecode;

	FileAccess *fa = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V(!fa, ERR_FILE_CANT_OPEN);

	FileAccessEncrypted *fae = memnew(FileAccessEncrypted);
	ERR_FAIL_COND_V(!fae, ERR_FILE_CORRUPT);

	Error err = fae->open_and_parse(fa, p_key, FileAccessEncrypted::MODE_READ);

	if (err) {
		fa->close();
		memdelete(fa);
		memdelete(fae);

		ERR_FAIL_COND_V(err, ERR_FILE_CORRUPT);
	}

	bytecode.resize(fae->get_length());
	fae->get_buffer(bytecode.ptrw(), bytecode.size());
	fae->close();
	memdelete(fae);

	error_message = RTR("No error");

	return decompile_buffer(bytecode);
}

Error GDScriptDecomp::decompile_byte_code(const String &p_path) {
	Vector<uint8_t> bytecode;

	bytecode = FileAccess::get_file_as_array(p_path);

	error_message = RTR("No error");

	return decompile_buffer(bytecode);
}

String GDScriptDecomp::get_script_text() {
	return script_text;
}

String GDScriptDecomp::get_error_message() {
	return error_message;
}

String GDScriptDecomp::get_constant_string(Vector<Variant> &constants, uint32_t constId) {
	String constString = constants[constId].get_construct_string();
	if (constants[constId].get_type() == Variant::Type::STRING) {
		constString = constString.replace("\n", "\\n");
	}
	return constString;
}

String GDScriptDecomp::get_type_name_v2(int p_type) {
	switch (p_type) {
		case V2Type::Type::NIL: {
			return "Nil";
		} break;

		// atomic types
		case V2Type::Type::BOOL: {
			return "bool";
		} break;
		case V2Type::Type::INT: {
			return "int";

		} break;
		case V2Type::Type::REAL: {
			return "float";

		} break;
		case V2Type::Type::STRING: {
			return "String";
		} break;

		// math types
		case V2Type::Type::VECTOR2: {
			return "Vector2";
		} break;
		case V2Type::Type::RECT2: {
			return "Rect2";
		} break;
		case V2Type::Type::MATRIX32: {
			return "Matrix32";
		} break;
		case V2Type::Type::VECTOR3: {
			return "Vector3";
		} break;
		case V2Type::Type::PLANE: {
			return "Plane";

		} break;
		/*
			case V2Type::Type::QUAT: {
			} break;*/
		case V2Type::Type::_AABB: {
			return "AABB";
		} break;
		case V2Type::Type::QUAT: {
			return "Quat";

		} break;
		case V2Type::Type::MATRIX3: {
			return "Matrix3";

		} break;
		case V2Type::Type::TRANSFORM: {
			return "Transform";

		} break;

		// misc types
		case V2Type::Type::COLOR: {
			return "Color";

		} break;
		case V2Type::Type::IMAGE: {
			return "Image";

		} break;
		case V2Type::Type::_RID: {
			return "RID";
		} break;
		case V2Type::Type::OBJECT: {
			return "Object";
		} break;
		case V2Type::Type::NODE_PATH: {
			return "NodePath";

		} break;
		case V2Type::Type::INPUT_EVENT: {
			return "InputEvent";

		} break;
		case V2Type::Type::DICTIONARY: {
			return "Dictionary";

		} break;
		case V2Type::Type::ARRAY: {
			return "Array";

		} break;

		// arrays
		case V2Type::Type::RAW_ARRAY: {
			return "RawArray";

		} break;
		case V2Type::Type::INT_ARRAY: {
			return "IntArray";

		} break;
		case V2Type::Type::REAL_ARRAY: {
			return "RealArray";

		} break;
		case V2Type::Type::STRING_ARRAY: {
			return "StringArray";
		} break;
		case V2Type::Type::VECTOR2_ARRAY: {
			return "Vector2Array";

		} break;
		case V2Type::Type::VECTOR3_ARRAY: {
			return "Vector3Array";

		} break;
		case V2Type::Type::COLOR_ARRAY: {
			return "ColorArray";

		} break;
		default: {
		}
	}

	return "";
}

String GDScriptDecomp::get_type_name_v3(int p_type) {
	switch (p_type) {
		case V3Type::Type::NIL: {
			return "Nil";
		} break;

		// atomic types
		case V3Type::Type::BOOL: {
			return "bool";
		} break;
		case V3Type::Type::INT: {
			return "int";
		} break;
		case V3Type::Type::REAL: {
			return "float";
		} break;
		case V3Type::Type::STRING: {
			return "String";
		} break;

		// math types
		case V3Type::Type::VECTOR2: {
			return "Vector2";
		} break;
		case V3Type::Type::RECT2: {
			return "Rect2";
		} break;
		case V3Type::Type::TRANSFORM2D: {
			return "Transform2D";
		} break;
		case V3Type::Type::VECTOR3: {
			return "Vector3";
		} break;
		case V3Type::Type::PLANE: {
			return "Plane";
		} break;
		/*
			case V3Type::Type::QUAT: {
			} break;*/
		case V3Type::Type::AABB: {
			return "AABB";
		} break;
		case V3Type::Type::QUAT: {
			return "Quat";

		} break;
		case V3Type::Type::BASIS: {
			return "Basis";

		} break;
		case V3Type::Type::TRANSFORM: {
			return "Transform";
		} break;

		// misc types
		case V3Type::Type::COLOR: {
			return "Color";
		} break;
		case V3Type::Type::_RID: {
			return "RID";
		} break;
		case V3Type::Type::OBJECT: {
			return "Object";
		} break;
		case V3Type::Type::NODE_PATH: {
			return "NodePath";

		} break;
		case V3Type::Type::DICTIONARY: {
			return "Dictionary";
		} break;
		case V3Type::Type::ARRAY: {
			return "Array";
		} break;

		// arrays
		case V3Type::Type::POOL_BYTE_ARRAY: {
			return "PoolByteArray";
		} break;
		case V3Type::Type::POOL_INT_ARRAY: {
			return "PoolIntArray";
		} break;
		case V3Type::Type::POOL_REAL_ARRAY: {
			return "PoolRealArray";
		} break;
		case V3Type::Type::POOL_STRING_ARRAY: {
			return "PoolStringArray";
		} break;
		case V3Type::Type::POOL_VECTOR2_ARRAY: {
			return "PoolVector2Array";
		} break;
		case V3Type::Type::POOL_VECTOR3_ARRAY: {
			return "PoolVector3Array";
		} break;
		case V3Type::Type::POOL_COLOR_ARRAY: {
			return "PoolColorArray";
		} break;
		default: {
		}
	}

	return "";
}

Error GDScriptDecomp::decode_variant_3(Variant &r_variant, const uint8_t *p_buffer, int p_len, int *r_len, bool p_allow_objects) {
	const uint8_t *buf = p_buffer;
	int len = p_len;

	ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);

	uint32_t type = decode_uint32(buf);

	ERR_FAIL_COND_V((type & ENCODE_MASK) >= 27, ERR_INVALID_DATA);

	buf += 4;
	len -= 4;
	if (r_len) {
		*r_len = 4;
	}

	switch (type & ENCODE_MASK) {
		case 0: { // NIL
			r_variant = Variant();
		} break;
		case 1: { // BOOL
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			bool val = decode_uint32(buf);
			r_variant = val;
			if (r_len) {
				(*r_len) += 4;
			}
		} break;
		case 2: { // INT
			if (type & ENCODE_FLAG_64) {
				ERR_FAIL_COND_V(len < 8, ERR_INVALID_DATA);
				int64_t val = decode_uint64(buf);
				r_variant = val;
				if (r_len) {
					(*r_len) += 8;
				}

			} else {
				ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
				int32_t val = decode_uint32(buf);
				r_variant = val;
				if (r_len) {
					(*r_len) += 4;
				}
			}

		} break;
		case 3: { // REAL
			if (type & ENCODE_FLAG_64) {
				ERR_FAIL_COND_V(len < 8, ERR_INVALID_DATA);
				double val = decode_double(buf);
				r_variant = val;
				if (r_len) {
					(*r_len) += 8;
				}
			} else {
				ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
				float val = decode_float(buf);
				r_variant = val;
				if (r_len) {
					(*r_len) += 4;
				}
			}

		} break;
		case 4: { // STRING
			String str;
			Error err = _decode_string(buf, len, r_len, str);
			if (err) {
				return err;
			}
			r_variant = str;

		} break;

		// math types
		case 5: { // VECTOR2
			ERR_FAIL_COND_V(len < 4 * 2, ERR_INVALID_DATA);
			Vector2 val;
			val.x = decode_float(&buf[0]);
			val.y = decode_float(&buf[4]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 2;
			}

		} break;
		case 6: { // RECT2
			ERR_FAIL_COND_V(len < 4 * 4, ERR_INVALID_DATA);
			Rect2 val;
			val.position.x = decode_float(&buf[0]);
			val.position.y = decode_float(&buf[4]);
			val.size.x = decode_float(&buf[8]);
			val.size.y = decode_float(&buf[12]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 4;
			}

		} break;
		case 7: { // VECTOR3
			ERR_FAIL_COND_V(len < 4 * 3, ERR_INVALID_DATA);
			Vector3 val;
			val.x = decode_float(&buf[0]);
			val.y = decode_float(&buf[4]);
			val.z = decode_float(&buf[8]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 3;
			}

		} break;
		case 8: { // TRANSFORM2D
			ERR_FAIL_COND_V(len < 4 * 6, ERR_INVALID_DATA);
			Transform2D val;
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 2; j++) {
					val.elements[i][j] = decode_float(&buf[(i * 2 + j) * 4]);
				}
			}

			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 6;
			}

		} break;
		case 9: { // PLANE
			ERR_FAIL_COND_V(len < 4 * 4, ERR_INVALID_DATA);
			Plane val;
			val.normal.x = decode_float(&buf[0]);
			val.normal.y = decode_float(&buf[4]);
			val.normal.z = decode_float(&buf[8]);
			val.d = decode_float(&buf[12]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 4;
			}

		} break;
		case 10: { // QUAT
			ERR_FAIL_COND_V(len < 4 * 4, ERR_INVALID_DATA);
			Quaternion val;
			val.x = decode_float(&buf[0]);
			val.y = decode_float(&buf[4]);
			val.z = decode_float(&buf[8]);
			val.w = decode_float(&buf[12]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 4;
			}

		} break;
		case 11: { // AABB
			ERR_FAIL_COND_V(len < 4 * 6, ERR_INVALID_DATA);
			AABB val;
			val.position.x = decode_float(&buf[0]);
			val.position.y = decode_float(&buf[4]);
			val.position.z = decode_float(&buf[8]);
			val.size.x = decode_float(&buf[12]);
			val.size.y = decode_float(&buf[16]);
			val.size.z = decode_float(&buf[20]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 6;
			}

		} break;
		case 12: { // BASIS
			ERR_FAIL_COND_V(len < 4 * 9, ERR_INVALID_DATA);
			Basis val;
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 3; j++) {
					val.elements[i][j] = decode_float(&buf[(i * 3 + j) * 4]);
				}
			}

			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 9;
			}

		} break;
		case 13: { // TRANSFORM
			ERR_FAIL_COND_V(len < 4 * 12, ERR_INVALID_DATA);
			Transform3D val;
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 3; j++) {
					val.basis.elements[i][j] = decode_float(&buf[(i * 3 + j) * 4]);
				}
			}
			val.origin[0] = decode_float(&buf[36]);
			val.origin[1] = decode_float(&buf[40]);
			val.origin[2] = decode_float(&buf[44]);

			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 12;
			}

		} break;

		// misc types
		case 14: { // COLOR
			ERR_FAIL_COND_V(len < 4 * 4, ERR_INVALID_DATA);
			Color val;
			val.r = decode_float(&buf[0]);
			val.g = decode_float(&buf[4]);
			val.b = decode_float(&buf[8]);
			val.a = decode_float(&buf[12]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 4;
			}

		} break;
		case 15: { // NODE_PATH
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t strlen = decode_uint32(buf);

			if (strlen & 0x80000000) {
				//new format
				ERR_FAIL_COND_V(len < 12, ERR_INVALID_DATA);
				Vector<StringName> names;
				Vector<StringName> subnames;

				uint32_t namecount = strlen &= 0x7FFFFFFF;
				uint32_t subnamecount = decode_uint32(buf + 4);
				uint32_t flags = decode_uint32(buf + 8);

				len -= 12;
				buf += 12;

				if (flags & 2) { // Obsolete format with property separate from subpath
					subnamecount++;
				}

				uint32_t total = namecount + subnamecount;

				if (r_len) {
					(*r_len) += 12;
				}

				for (uint32_t i = 0; i < total; i++) {
					String str;
					Error err = _decode_string(buf, len, r_len, str);
					if (err) {
						return err;
					}

					if (i < namecount) {
						names.push_back(str);
					} else {
						subnames.push_back(str);
					}
				}

				r_variant = NodePath(names, subnames, flags & 1);

			} else {
				//old format, just a string

				ERR_FAIL_V(ERR_INVALID_DATA);
			}

		} break;
		case 16: { // RID
			r_variant = RID();
		} break;
		case 17: { // OBJECT
			if (type & ENCODE_FLAG_OBJECT_AS_ID) {
				//this _is_ allowed
				ERR_FAIL_COND_V(len < 8, ERR_INVALID_DATA);
				ObjectID val = ObjectID(decode_uint64(buf));
				if (r_len) {
					(*r_len) += 8;
				}

				if (val.is_null()) {
					r_variant = (Object *)nullptr;
				} else {
					Ref<EncodedObjectAsID> obj_as_id;
					obj_as_id.instantiate();
					obj_as_id->set_object_id(val);

					r_variant = obj_as_id;
				}

			} else {
				ERR_FAIL_COND_V(!p_allow_objects, ERR_UNAUTHORIZED);

				String str;
				Error err = _decode_string(buf, len, r_len, str);
				if (err) {
					return err;
				}

				if (str == String()) {
					r_variant = (Object *)nullptr;
				} else {
					Object *obj = ClassDB::instantiate(str);

					ERR_FAIL_COND_V(!obj, ERR_UNAVAILABLE);
					ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);

					int32_t count = decode_uint32(buf);
					buf += 4;
					len -= 4;
					if (r_len) {
						(*r_len) += 4;
					}

					for (int i = 0; i < count; i++) {
						str = String();
						err = _decode_string(buf, len, r_len, str);
						if (err) {
							return err;
						}

						Variant value;
						int used;
						err = decode_variant_3(value, buf, len, &used, p_allow_objects);
						if (err) {
							return err;
						}

						buf += used;
						len -= used;
						if (r_len) {
							(*r_len) += used;
						}

						obj->set(str, value);
					}

					if (Object::cast_to<RefCounted>(obj)) {
						REF ref = REF(Object::cast_to<RefCounted>(obj));
						r_variant = ref;
					} else {
						r_variant = obj;
					}
				}
			}

		} break;
		case 18: { // DICTIONARY
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			//  bool shared = count&0x80000000;
			count &= 0x7FFFFFFF;

			buf += 4;
			len -= 4;

			if (r_len) {
				(*r_len) += 4;
			}

			Dictionary d;

			for (int i = 0; i < count; i++) {
				Variant key, value;

				int used;
				Error err = decode_variant_3(key, buf, len, &used, p_allow_objects);
				ERR_FAIL_COND_V_MSG(err != OK, err, "Error when trying to decode Variant.");

				buf += used;
				len -= used;
				if (r_len) {
					(*r_len) += used;
				}

				err = decode_variant_3(value, buf, len, &used, p_allow_objects);
				ERR_FAIL_COND_V_MSG(err != OK, err, "Error when trying to decode Variant.");

				buf += used;
				len -= used;
				if (r_len) {
					(*r_len) += used;
				}

				d[key] = value;
			}

			r_variant = d;

		} break;
		case 19: { // ARRAY
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			//  bool shared = count&0x80000000;
			count &= 0x7FFFFFFF;

			buf += 4;
			len -= 4;

			if (r_len) {
				(*r_len) += 4;
			}

			Array varr;

			for (int i = 0; i < count; i++) {
				int used = 0;
				Variant v;
				Error err = decode_variant_3(v, buf, len, &used, p_allow_objects);
				ERR_FAIL_COND_V_MSG(err != OK, err, "Error when trying to decode Variant.");
				buf += used;
				len -= used;
				varr.push_back(v);
				if (r_len) {
					(*r_len) += used;
				}
			}

			r_variant = varr;

		} break;

		// arrays
		case 20: { // PACKED_BYTE_ARRAY
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;
			ERR_FAIL_COND_V(count < 0 || count > len, ERR_INVALID_DATA);

			Vector<uint8_t> data;

			if (count) {
				data.resize(count);
				uint8_t *w = data.ptrw();
				for (int32_t i = 0; i < count; i++) {
					w[i] = buf[i];
				}
			}

			r_variant = data;

			if (r_len) {
				if (count % 4) {
					(*r_len) += 4 - count % 4;
				}
				(*r_len) += 4 + count;
			}

		} break;
		case 21: { // PACKED_INT32_ARRAY
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;
			ERR_FAIL_MUL_OF(count, 4, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 4 > len, ERR_INVALID_DATA);

			Vector<int32_t> data;

			if (count) {
				//const int*rbuf=(const int*)buf;
				data.resize(count);
				int32_t *w = data.ptrw();
				for (int32_t i = 0; i < count; i++) {
					w[i] = decode_uint32(&buf[i * 4]);
				}
			}
			r_variant = Variant(data);
			if (r_len) {
				(*r_len) += 4 + count * sizeof(int32_t);
			}

		} break;
		case 22: { // PACKED_FLOAT32_ARRAY
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;
			ERR_FAIL_MUL_OF(count, 4, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 4 > len, ERR_INVALID_DATA);

			Vector<float> data;

			if (count) {
				//const float*rbuf=(const float*)buf;
				data.resize(count);
				float *w = data.ptrw();
				for (int32_t i = 0; i < count; i++) {
					w[i] = decode_float(&buf[i * 4]);
				}
			}
			r_variant = data;

			if (r_len) {
				(*r_len) += 4 + count * sizeof(float);
			}

		} break;
		case 23: { // PACKED_STRING_ARRAY
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);

			Vector<String> strings;
			buf += 4;
			len -= 4;

			if (r_len) {
				(*r_len) += 4;
			}
			//printf("string count: %i\n",count);

			for (int32_t i = 0; i < count; i++) {
				String str;
				Error err = _decode_string(buf, len, r_len, str);
				if (err) {
					return err;
				}

				strings.push_back(str);
			}

			r_variant = strings;

		} break;
		case 24: { // PACKED_VECTOR2_ARRAY
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;

			ERR_FAIL_MUL_OF(count, 4 * 2, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 4 * 2 > len, ERR_INVALID_DATA);
			Vector<Vector2> varray;

			if (r_len) {
				(*r_len) += 4;
			}

			if (count) {
				varray.resize(count);
				Vector2 *w = varray.ptrw();

				for (int32_t i = 0; i < count; i++) {
					w[i].x = decode_float(buf + i * 4 * 2 + 4 * 0);
					w[i].y = decode_float(buf + i * 4 * 2 + 4 * 1);
				}

				int adv = 4 * 2 * count;

				if (r_len) {
					(*r_len) += adv;
				}
			}

			r_variant = varray;

		} break;
		case 25: { // PACKED_VECTOR3_ARRAY
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;

			ERR_FAIL_MUL_OF(count, 4 * 3, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 4 * 3 > len, ERR_INVALID_DATA);

			Vector<Vector3> varray;

			if (r_len) {
				(*r_len) += 4;
			}

			if (count) {
				varray.resize(count);
				Vector3 *w = varray.ptrw();

				for (int32_t i = 0; i < count; i++) {
					w[i].x = decode_float(buf + i * 4 * 3 + 4 * 0);
					w[i].y = decode_float(buf + i * 4 * 3 + 4 * 1);
					w[i].z = decode_float(buf + i * 4 * 3 + 4 * 2);
				}

				int adv = 4 * 3 * count;

				if (r_len) {
					(*r_len) += adv;
				}
			}

			r_variant = varray;

		} break;
		case 26: { // PACKED_COLOR_ARRAY
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;

			ERR_FAIL_MUL_OF(count, 4 * 4, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 4 * 4 > len, ERR_INVALID_DATA);

			Vector<Color> carray;

			if (r_len) {
				(*r_len) += 4;
			}

			if (count) {
				carray.resize(count);
				Color *w = carray.ptrw();

				for (int32_t i = 0; i < count; i++) {
					w[i].r = decode_float(buf + i * 4 * 4 + 4 * 0);
					w[i].g = decode_float(buf + i * 4 * 4 + 4 * 1);
					w[i].b = decode_float(buf + i * 4 * 4 + 4 * 2);
					w[i].a = decode_float(buf + i * 4 * 4 + 4 * 3);
				}

				int adv = 4 * 4 * count;

				if (r_len) {
					(*r_len) += adv;
				}
			}

			r_variant = carray;

		} break;
		default: {
			ERR_FAIL_V(ERR_BUG);
		}
	}

	return OK;
}

Error GDScriptDecomp::decode_variant_2(Variant &r_variant, const uint8_t *p_buffer, int p_len, int *r_len, bool p_allow_objects) {
	const uint8_t *buf = p_buffer;
	int len = p_len;

	ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);

	uint32_t type = decode_uint32(buf);

	ERR_FAIL_COND_V((type & ENCODE_MASK) >= 29, ERR_INVALID_DATA);

	buf += 4;
	len -= 4;
	if (r_len) {
		*r_len = 4;
	}

	switch (type & ENCODE_MASK) {
		case 0: { // NIL
			r_variant = Variant();
		} break;
		case 1: { // BOOL
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			bool val = decode_uint32(buf);
			r_variant = val;
			if (r_len) {
				(*r_len) += 4;
			}
		} break;
		case 2: { // INT
			if (type & ENCODE_FLAG_64) {
				ERR_FAIL_COND_V(len < 8, ERR_INVALID_DATA);
				int64_t val = decode_uint64(buf);
				r_variant = val;
				if (r_len) {
					(*r_len) += 8;
				}

			} else {
				ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
				int32_t val = decode_uint32(buf);
				r_variant = val;
				if (r_len) {
					(*r_len) += 4;
				}
			}

		} break;
		case 3: { // REAL
			if (type & ENCODE_FLAG_64) {
				ERR_FAIL_COND_V(len < 8, ERR_INVALID_DATA);
				double val = decode_double(buf);
				r_variant = val;
				if (r_len) {
					(*r_len) += 8;
				}
			} else {
				ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
				float val = decode_float(buf);
				r_variant = val;
				if (r_len) {
					(*r_len) += 4;
				}
			}

		} break;
		case 4: { // STRING
			String str;
			Error err = _decode_string(buf, len, r_len, str);
			if (err) {
				return err;
			}
			r_variant = str;

		} break;

		// math types
		case 5: { // VECTOR2
			ERR_FAIL_COND_V(len < 4 * 2, ERR_INVALID_DATA);
			Vector2 val;
			val.x = decode_float(&buf[0]);
			val.y = decode_float(&buf[4]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 2;
			}

		} break;
		case 6: { // RECT2
			ERR_FAIL_COND_V(len < 4 * 4, ERR_INVALID_DATA);
			Rect2 val;
			val.position.x = decode_float(&buf[0]);
			val.position.y = decode_float(&buf[4]);
			val.size.x = decode_float(&buf[8]);
			val.size.y = decode_float(&buf[12]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 4;
			}

		} break;
		case 7: { // VECTOR3
			ERR_FAIL_COND_V(len < 4 * 3, ERR_INVALID_DATA);
			Vector3 val;
			val.x = decode_float(&buf[0]);
			val.y = decode_float(&buf[4]);
			val.z = decode_float(&buf[8]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 3;
			}

		} break;
		case 8: { // TRANSFORM2D
			ERR_FAIL_COND_V(len < 4 * 6, ERR_INVALID_DATA);
			Transform2D val;
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 2; j++) {
					val.elements[i][j] = decode_float(&buf[(i * 2 + j) * 4]);
				}
			}

			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 6;
			}

		} break;
		case 9: { // PLANE
			ERR_FAIL_COND_V(len < 4 * 4, ERR_INVALID_DATA);
			Plane val;
			val.normal.x = decode_float(&buf[0]);
			val.normal.y = decode_float(&buf[4]);
			val.normal.z = decode_float(&buf[8]);
			val.d = decode_float(&buf[12]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 4;
			}

		} break;
		case 10: { // QUAT
			ERR_FAIL_COND_V(len < 4 * 4, ERR_INVALID_DATA);
			Quaternion val;
			val.x = decode_float(&buf[0]);
			val.y = decode_float(&buf[4]);
			val.z = decode_float(&buf[8]);
			val.w = decode_float(&buf[12]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 4;
			}

		} break;
		case 11: { // AABB
			ERR_FAIL_COND_V(len < 4 * 6, ERR_INVALID_DATA);
			AABB val;
			val.position.x = decode_float(&buf[0]);
			val.position.y = decode_float(&buf[4]);
			val.position.z = decode_float(&buf[8]);
			val.size.x = decode_float(&buf[12]);
			val.size.y = decode_float(&buf[16]);
			val.size.z = decode_float(&buf[20]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 6;
			}

		} break;
		case 12: { // BASIS
			ERR_FAIL_COND_V(len < 4 * 9, ERR_INVALID_DATA);
			Basis val;
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 3; j++) {
					val.elements[i][j] = decode_float(&buf[(i * 3 + j) * 4]);
				}
			}

			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 9;
			}

		} break;
		case 13: { // TRANSFORM
			ERR_FAIL_COND_V(len < 4 * 12, ERR_INVALID_DATA);
			Transform3D val;
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 3; j++) {
					val.basis.elements[i][j] = decode_float(&buf[(i * 3 + j) * 4]);
				}
			}
			val.origin[0] = decode_float(&buf[36]);
			val.origin[1] = decode_float(&buf[40]);
			val.origin[2] = decode_float(&buf[44]);

			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 12;
			}

		} break;

		// misc types
		case 14: { // COLOR
			ERR_FAIL_COND_V(len < 4 * 4, ERR_INVALID_DATA);
			Color val;
			val.r = decode_float(&buf[0]);
			val.g = decode_float(&buf[4]);
			val.b = decode_float(&buf[8]);
			val.a = decode_float(&buf[12]);
			r_variant = val;

			if (r_len) {
				(*r_len) += 4 * 4;
			}

		} break;

		case 15: { // IMAGE

			ERR_FAIL_COND_V(len < 5 * 4, ERR_INVALID_DATA);
			Image::Format fmt = (Image::Format)decode_uint32(&buf[0]);
			ERR_FAIL_INDEX_V(fmt, Image::FORMAT_MAX, ERR_INVALID_DATA);
			int32_t datalen = decode_uint32(&buf[16]);

			// Skip decoding, should not be part of script source.

			if (r_len) {
				if (datalen % 4) {
					(*r_len) += 4 - datalen % 4;
				}

				(*r_len) += 4 * 5 + datalen;
			}

		} break;

		case 16: { // NODE_PATH
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t strlen = decode_uint32(buf);

			if (strlen & 0x80000000) {
				//new format
				ERR_FAIL_COND_V(len < 12, ERR_INVALID_DATA);
				Vector<StringName> names;
				Vector<StringName> subnames;

				uint32_t namecount = strlen &= 0x7FFFFFFF;
				uint32_t subnamecount = decode_uint32(buf + 4);
				uint32_t flags = decode_uint32(buf + 8);

				len -= 12;
				buf += 12;

				if (flags & 2) { // Obsolete format with property separate from subpath
					subnamecount++;
				}

				uint32_t total = namecount + subnamecount;

				if (r_len) {
					(*r_len) += 12;
				}

				for (uint32_t i = 0; i < total; i++) {
					String str;
					Error err = _decode_string(buf, len, r_len, str);
					if (err) {
						return err;
					}

					if (i < namecount) {
						names.push_back(str);
					} else {
						subnames.push_back(str);
					}
				}

				r_variant = NodePath(names, subnames, flags & 1);

			} else {
				//old format, just a string

				ERR_FAIL_V(ERR_INVALID_DATA);
			}

		} break;
		case 17: { // RID
			r_variant = RID();
		} break;
		case 18: { // OBJECT
			if (type & ENCODE_FLAG_OBJECT_AS_ID) {
				//this _is_ allowed
				ERR_FAIL_COND_V(len < 8, ERR_INVALID_DATA);
				ObjectID val = ObjectID(decode_uint64(buf));
				if (r_len) {
					(*r_len) += 8;
				}

				if (val.is_null()) {
					r_variant = (Object *)nullptr;
				} else {
					Ref<EncodedObjectAsID> obj_as_id;
					obj_as_id.instantiate();
					obj_as_id->set_object_id(val);

					r_variant = obj_as_id;
				}

			} else {
				ERR_FAIL_COND_V(!p_allow_objects, ERR_UNAUTHORIZED);

				String str;
				Error err = _decode_string(buf, len, r_len, str);
				if (err) {
					return err;
				}

				if (str == String()) {
					r_variant = (Object *)nullptr;
				} else {
					Object *obj = ClassDB::instantiate(str);

					ERR_FAIL_COND_V(!obj, ERR_UNAVAILABLE);
					ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);

					int32_t count = decode_uint32(buf);
					buf += 4;
					len -= 4;
					if (r_len) {
						(*r_len) += 4;
					}

					for (int i = 0; i < count; i++) {
						str = String();
						err = _decode_string(buf, len, r_len, str);
						if (err) {
							return err;
						}

						Variant value;
						int used;
						err = decode_variant_2(value, buf, len, &used, p_allow_objects);
						if (err) {
							return err;
						}

						buf += used;
						len -= used;
						if (r_len) {
							(*r_len) += used;
						}

						obj->set(str, value);
					}

					if (Object::cast_to<RefCounted>(obj)) {
						REF ref = REF(Object::cast_to<RefCounted>(obj));
						r_variant = ref;
					} else {
						r_variant = obj;
					}
				}
			}

		} break;
		case 19: { // INPUT_EVENT

			ERR_FAIL_COND_V(len < 8, ERR_INVALID_DATA);

			uint32_t ie_type = decode_uint32(&buf[0]);

			if (r_len) {
				(*r_len) += 12;
			}

			switch (ie_type) {
				case 1: { // KEY
					ERR_FAIL_COND_V(len < 20, ERR_INVALID_DATA);
					if (r_len) {
						(*r_len) += 8;
					}
				} break;
				case 3: { // MOUSE_BUTTON
					ERR_FAIL_COND_V(len < 16, ERR_INVALID_DATA);
					if (r_len) {
						(*r_len) += 4;
					}
				} break;
				case 5: { // JOYSTICK_BUTTON
					ERR_FAIL_COND_V(len < 16, ERR_INVALID_DATA);
					if (r_len) {
						(*r_len) += 4;
					}
				} break;
				case 6: { // SCREEN_TOUCH
					ERR_FAIL_COND_V(len < 16, ERR_INVALID_DATA);
					if (r_len) {
						(*r_len) += 4;
					}
				} break;
				case 4: { // JOYSTICK_MOTION
					ERR_FAIL_COND_V(len < 20, ERR_INVALID_DATA);
					if (r_len) {
						(*r_len) += 8;
					}
				} break;
			}
			// Skip decoding, should not be part of script source.
		} break;
		case 20: { // DICTIONARY
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			//  bool shared = count&0x80000000;
			count &= 0x7FFFFFFF;

			buf += 4;
			len -= 4;

			if (r_len) {
				(*r_len) += 4;
			}

			Dictionary d;

			for (int i = 0; i < count; i++) {
				Variant key, value;

				int used;
				Error err = decode_variant_2(key, buf, len, &used, p_allow_objects);
				ERR_FAIL_COND_V_MSG(err != OK, err, "Error when trying to decode Variant.");

				buf += used;
				len -= used;
				if (r_len) {
					(*r_len) += used;
				}

				err = decode_variant_2(value, buf, len, &used, p_allow_objects);
				ERR_FAIL_COND_V_MSG(err != OK, err, "Error when trying to decode Variant.");

				buf += used;
				len -= used;
				if (r_len) {
					(*r_len) += used;
				}

				d[key] = value;
			}

			r_variant = d;

		} break;
		case 21: { // ARRAY
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			//  bool shared = count&0x80000000;
			count &= 0x7FFFFFFF;

			buf += 4;
			len -= 4;

			if (r_len) {
				(*r_len) += 4;
			}

			Array varr;

			for (int i = 0; i < count; i++) {
				int used = 0;
				Variant v;
				Error err = decode_variant_2(v, buf, len, &used, p_allow_objects);
				ERR_FAIL_COND_V_MSG(err != OK, err, "Error when trying to decode Variant.");
				buf += used;
				len -= used;
				varr.push_back(v);
				if (r_len) {
					(*r_len) += used;
				}
			}

			r_variant = varr;

		} break;

		// arrays
		case 22: { // PACKED_BYTE_ARRAY
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;
			ERR_FAIL_COND_V(count < 0 || count > len, ERR_INVALID_DATA);

			Vector<uint8_t> data;

			if (count) {
				data.resize(count);
				uint8_t *w = data.ptrw();
				for (int32_t i = 0; i < count; i++) {
					w[i] = buf[i];
				}
			}

			r_variant = data;

			if (r_len) {
				if (count % 4) {
					(*r_len) += 4 - count % 4;
				}
				(*r_len) += 4 + count;
			}

		} break;
		case 23: { // PACKED_INT32_ARRAY
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;
			ERR_FAIL_MUL_OF(count, 4, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 4 > len, ERR_INVALID_DATA);

			Vector<int32_t> data;

			if (count) {
				//const int*rbuf=(const int*)buf;
				data.resize(count);
				int32_t *w = data.ptrw();
				for (int32_t i = 0; i < count; i++) {
					w[i] = decode_uint32(&buf[i * 4]);
				}
			}
			r_variant = Variant(data);
			if (r_len) {
				(*r_len) += 4 + count * sizeof(int32_t);
			}

		} break;
		case 24: { // PACKED_FLOAT32_ARRAY
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;
			ERR_FAIL_MUL_OF(count, 4, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 4 > len, ERR_INVALID_DATA);

			Vector<float> data;

			if (count) {
				//const float*rbuf=(const float*)buf;
				data.resize(count);
				float *w = data.ptrw();
				for (int32_t i = 0; i < count; i++) {
					w[i] = decode_float(&buf[i * 4]);
				}
			}
			r_variant = data;

			if (r_len) {
				(*r_len) += 4 + count * sizeof(float);
			}

		} break;
		case 25: { // PACKED_STRING_ARRAY
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);

			Vector<String> strings;
			buf += 4;
			len -= 4;

			if (r_len) {
				(*r_len) += 4;
			}
			//printf("string count: %i\n",count);

			for (int32_t i = 0; i < count; i++) {
				String str;
				Error err = _decode_string(buf, len, r_len, str);
				if (err) {
					return err;
				}

				strings.push_back(str);
			}

			r_variant = strings;

		} break;
		case 26: { // PACKED_VECTOR2_ARRAY
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;

			ERR_FAIL_MUL_OF(count, 4 * 2, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 4 * 2 > len, ERR_INVALID_DATA);
			Vector<Vector2> varray;

			if (r_len) {
				(*r_len) += 4;
			}

			if (count) {
				varray.resize(count);
				Vector2 *w = varray.ptrw();

				for (int32_t i = 0; i < count; i++) {
					w[i].x = decode_float(buf + i * 4 * 2 + 4 * 0);
					w[i].y = decode_float(buf + i * 4 * 2 + 4 * 1);
				}

				int adv = 4 * 2 * count;

				if (r_len) {
					(*r_len) += adv;
				}
			}

			r_variant = varray;

		} break;
		case 27: { // PACKED_VECTOR3_ARRAY
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;

			ERR_FAIL_MUL_OF(count, 4 * 3, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 4 * 3 > len, ERR_INVALID_DATA);

			Vector<Vector3> varray;

			if (r_len) {
				(*r_len) += 4;
			}

			if (count) {
				varray.resize(count);
				Vector3 *w = varray.ptrw();

				for (int32_t i = 0; i < count; i++) {
					w[i].x = decode_float(buf + i * 4 * 3 + 4 * 0);
					w[i].y = decode_float(buf + i * 4 * 3 + 4 * 1);
					w[i].z = decode_float(buf + i * 4 * 3 + 4 * 2);
				}

				int adv = 4 * 3 * count;

				if (r_len) {
					(*r_len) += adv;
				}
			}

			r_variant = varray;

		} break;
		case 28: { // PACKED_COLOR_ARRAY
			ERR_FAIL_COND_V(len < 4, ERR_INVALID_DATA);
			int32_t count = decode_uint32(buf);
			buf += 4;
			len -= 4;

			ERR_FAIL_MUL_OF(count, 4 * 4, ERR_INVALID_DATA);
			ERR_FAIL_COND_V(count < 0 || count * 4 * 4 > len, ERR_INVALID_DATA);

			Vector<Color> carray;

			if (r_len) {
				(*r_len) += 4;
			}

			if (count) {
				carray.resize(count);
				Color *w = carray.ptrw();

				for (int32_t i = 0; i < count; i++) {
					w[i].r = decode_float(buf + i * 4 * 4 + 4 * 0);
					w[i].g = decode_float(buf + i * 4 * 4 + 4 * 1);
					w[i].b = decode_float(buf + i * 4 * 4 + 4 * 2);
					w[i].a = decode_float(buf + i * 4 * 4 + 4 * 3);
				}

				int adv = 4 * 4 * count;

				if (r_len) {
					(*r_len) += adv;
				}
			}

			r_variant = carray;

		} break;
		default: {
			ERR_FAIL_V(ERR_BUG);
		}
	}

	return OK;
}
