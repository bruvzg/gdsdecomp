#include "variant_writer_compat.h"
#include "core/input/input_event.h"
#include "core/io/image.h"
#include "core/io/resource_loader.h"
#include "core/os/keyboard.h"
#include "core/string/string_buffer.h"
#include "core/variant/variant_parser.h"
#include "image_parser_v2.h"
#include "resource_loader_compat.h"

namespace ToV4 {

enum Type {

	NIL,
	// atomic types
	BOOL,
	INT,
	REAL,
	STRING,
	// math types
	VECTOR2,
	VECTOR2I, //Unused
	RECT2,
	RECT2I, //Unused
	VECTOR3,
	VECTOR3I, //Unused
	TRANSFORM2D,
	PLANE,
	QUAT, // 10
	_AABB, //sorry naming convention fail :( not like it's used often
	BASIS, //Basis
	TRANSFORM,
	// misc types
	COLOR,
	IMAGE, // 15
	NODE_PATH,
	_RID,
	OBJECT,
	INPUT_EVENT, // v2 struct which isn't mapped
	SIGNAL, //Unused
	DICTIONARY, // 20
	ARRAY,
	// V3 arrays
	POOL_BYTE_ARRAY = 26, // 20
	POOL_INT_ARRAY = 27,
	POOL_INT64_ARRAY = 28, //Unused
	POOL_REAL_ARRAY = 29,
	POOL_REAL64_ARRAY = 30, //Unused
	POOL_STRING_ARRAY = 31,
	POOL_VECTOR2_ARRAY = 32,
	POOL_VECTOR3_ARRAY = 33, // 25
	POOL_COLOR_ARRAY = 34,
	VARIANT_MAX = 35,
	// V2 arrays
	RAW_ARRAY = 26, //Byte array
	INT_ARRAY = 27,
	INT64_ARRAY = 28, //unused
	REAL_ARRAY = 29,
	REAL64_ARRAY = 30, //Unused
	STRING_ARRAY = 31, // 25
	VECTOR2_ARRAY = 32,
	VECTOR3_ARRAY = 33,
	COLOR_ARRAY = 34,
	// v2 name for transform2d
	MATRIX32 = 11,
	// v2 name for matrix3
	MATRIX3 = 15,
};
}
//////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////

static String rtosfix(double p_value) {
	if (p_value == 0.0)
		return "0"; //avoid negative zero (-0) being written, which may annoy git, svn, etc. for changes when they don't exist.
	else
		return rtoss(p_value);
}

Error VariantWriterCompat::write_compat(const Variant &p_variant, const uint32_t ver_major, StoreStringFunc p_store_string_func, void *p_store_string_ud, EncodeResourceFunc p_encode_res_func, void *p_encode_res_ud) {
	// use the v4 write function instead for v4
	if (ver_major == 4) {
		return VariantWriter::write(p_variant, p_store_string_func, p_store_string_ud, p_encode_res_func, p_encode_res_ud);
	}

	// for v2 and v3...
	switch ((ToV4::Type)p_variant.get_type()) {
		case ToV4::NIL: {
			p_store_string_func(p_store_string_ud, "null");
		} break;
		case ToV4::BOOL: {
			p_store_string_func(p_store_string_ud, p_variant.operator bool() ? "true" : "false");
		} break;
		case ToV4::INT: {
			p_store_string_func(p_store_string_ud, itos(p_variant.operator int()));
		} break;
		case ToV4::REAL: {
			String s = rtosfix(p_variant.operator real_t());
			if (s.find(".") == -1 && s.find("e") == -1)
				s += ".0";
			p_store_string_func(p_store_string_ud, s);
		} break;
		case ToV4::STRING: {
			String str = p_variant;

			str = "\"" + str.c_escape_multiline() + "\"";
			p_store_string_func(p_store_string_ud, str);
		} break;
		case ToV4::VECTOR2: {
			Vector2 v = p_variant;
			p_store_string_func(p_store_string_ud, "Vector2( " + rtosfix(v.x) + ", " + rtosfix(v.y) + " )");
		} break;
		case ToV4::RECT2: {
			Rect2 aabb = p_variant;
			p_store_string_func(p_store_string_ud, "Rect2( " + rtosfix(aabb.position.x) + ", " + rtosfix(aabb.position.y) + ", " + rtosfix(aabb.size.x) + ", " + rtosfix(aabb.size.y) + " )");

		} break;
		case ToV4::VECTOR3: {
			Vector3 v = p_variant;
			p_store_string_func(p_store_string_ud, "Vector3( " + rtosfix(v.x) + ", " + rtosfix(v.y) + ", " + rtosfix(v.z) + " )");
		} break;
		case ToV4::TRANSFORM2D: { // v2 Matrix32
			String s = ver_major == 2 ? "Matrix32( " : "Transform2D( ";
			Transform2D m3 = p_variant;
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 2; j++) {
					if (i != 0 || j != 0)
						s += ", ";
					s += rtosfix(m3.elements[i][j]);
				}
			}

			p_store_string_func(p_store_string_ud, s + " )");

		} break;
		case ToV4::PLANE: {
			Plane p = p_variant;
			p_store_string_func(p_store_string_ud, "Plane( " + rtosfix(p.normal.x) + ", " + rtosfix(p.normal.y) + ", " + rtosfix(p.normal.z) + ", " + rtosfix(p.d) + " )");

		} break;
		case ToV4::_AABB: {
			AABB aabb = p_variant;
			p_store_string_func(p_store_string_ud, "AABB( " + rtosfix(aabb.position.x) + ", " + rtosfix(aabb.position.y) + ", " + rtosfix(aabb.position.z) + ", " + rtosfix(aabb.size.x) + ", " + rtosfix(aabb.size.y) + ", " + rtosfix(aabb.size.z) + " )");

		} break;
		case ToV4::QUAT: {
			Quaternion quat = p_variant;
			p_store_string_func(p_store_string_ud, "Quat( " + rtosfix(quat.x) + ", " + rtosfix(quat.y) + ", " + rtosfix(quat.z) + ", " + rtosfix(quat.w) + " )");

		} break;

		case ToV4::BASIS: { // v2 Matrix3

			String s = ver_major == 2 ? "Matrix3( " : "Basis( ";
			Basis m3 = p_variant;
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 3; j++) {
					if (i != 0 || j != 0)
						s += ", ";
					s += rtosfix(m3.elements[i][j]);
				}
			}

			p_store_string_func(p_store_string_ud, s + " )");

		} break;
		case ToV4::TRANSFORM: {
			String s = "Transform( ";
			Transform3D t = p_variant;
			Basis &m3 = t.basis;
			for (int i = 0; i < 3; i++) {
				for (int j = 0; j < 3; j++) {
					if (i != 0 || j != 0)
						s += ", ";
					s += rtosfix(m3.elements[i][j]);
				}
			}

			s = s + ", " + rtosfix(t.origin.x) + ", " + rtosfix(t.origin.y) + ", " + rtosfix(t.origin.z);

			p_store_string_func(p_store_string_ud, s + " )");
		} break;
		case ToV4::COLOR: {
			Color c = p_variant;
			p_store_string_func(p_store_string_ud, "Color( " + rtosfix(c.r) + ", " + rtosfix(c.g) + ", " + rtosfix(c.b) + ", " + rtosfix(c.a) + " )");

		} break;
		case ToV4::NODE_PATH: {
			String str = p_variant;

			str = "NodePath(\"" + str.c_escape() + "\")";
			p_store_string_func(p_store_string_ud, str);

		} break;

		case ToV4::OBJECT: {
			Object *obj = p_variant;

			if (!obj) {
				p_store_string_func(p_store_string_ud, "null");
				break; // don't save it
			}

			RES res = p_variant;
			if (res.is_valid()) {
				//is resource
				String res_text;
				//Hack for V2 Images
				if (ver_major == 2 && res->is_class("Image")) {
					res_text = ImageParserV2::image_v2_to_string(res);
				} else if (p_encode_res_func) {
					//try external function
					res_text = p_encode_res_func(p_encode_res_ud, res);
				} else if (res->is_class("FakeResource")) {
					//this is really just for debugging
					res_text = "Resource( \"" + ((Ref<FakeResource>)res)->get_real_path() + "\")";
				}

				//try path because it's a file
				if (res_text == String() && res->get_path().is_resource_file()) {
					//external resource
					String path = res->get_path();
					res_text = "Resource( \"" + path + "\")";
				}

				//could come up with some sort of text
				if (res_text != String()) {
					p_store_string_func(p_store_string_ud, res_text);
					break;
				}
			}

			//store as generic object

			p_store_string_func(p_store_string_ud, "Object(" + obj->get_class() + ",");

			List<PropertyInfo> props;
			obj->get_property_list(&props);
			bool first = true;
			for (List<PropertyInfo>::Element *E = props.front(); E; E = E->next()) {
				if (E->get().usage & PROPERTY_USAGE_STORAGE || E->get().usage & PROPERTY_USAGE_SCRIPT_VARIABLE) {
					//must be serialized

					if (first) {
						first = false;
					} else {
						p_store_string_func(p_store_string_ud, ",");
					}

					p_store_string_func(p_store_string_ud, "\"" + E->get().name + "\":");
					write_compat(obj->get(E->get().name), ver_major, p_store_string_func, p_store_string_ud, p_encode_res_func, p_encode_res_ud);
				}
			}

			p_store_string_func(p_store_string_ud, ")\n");

		} break;
		case ToV4::INPUT_EVENT: {
			// binary resource formats don't support storing input events,
			// so we don't bother writing them.
			WARN_PRINT("Attempted to save Input Event!");
		} break;
		case ToV4::DICTIONARY: {
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
				write_compat(E->get(), ver_major, p_store_string_func, p_store_string_ud, p_encode_res_func, p_encode_res_ud);
				p_store_string_func(p_store_string_ud, ": ");
				write_compat(dict[E->get()], ver_major, p_store_string_func, p_store_string_ud, p_encode_res_func, p_encode_res_ud);
				if (E->next())
					p_store_string_func(p_store_string_ud, ",\n");
			}

			p_store_string_func(p_store_string_ud, "\n}");

		} break;
		case ToV4::ARRAY: {
			p_store_string_func(p_store_string_ud, "[ ");
			Array array = p_variant;
			int len = array.size();
			for (int i = 0; i < len; i++) {
				if (i > 0)
					p_store_string_func(p_store_string_ud, ", ");
				write_compat(array[i], ver_major, p_store_string_func, p_store_string_ud, p_encode_res_func, p_encode_res_ud);
			}
			p_store_string_func(p_store_string_ud, " ]");

		} break;

		case ToV4::POOL_BYTE_ARRAY: { // v2 ByteArray

			p_store_string_func(p_store_string_ud, ver_major == 2 ? "ByteArray( " : "PoolByteArray( ");
			String s;
			Vector<uint8_t> data = p_variant;
			int len = data.size();

			const uint8_t *ptr = data.ptr();
			for (int i = 0; i < len; i++) {
				if (i > 0)
					p_store_string_func(p_store_string_ud, ", ");

				p_store_string_func(p_store_string_ud, itos(ptr[i]));
			}

			p_store_string_func(p_store_string_ud, " )");

		} break;
		case ToV4::POOL_INT_ARRAY: { // v2 IntArray

			p_store_string_func(p_store_string_ud, ver_major == 2 ? "IntArray( " : "PoolIntArray( ");
			Vector<int> data = p_variant;
			int len = data.size();

			const int *ptr = data.ptr();

			for (int i = 0; i < len; i++) {
				if (i > 0)
					p_store_string_func(p_store_string_ud, ", ");

				p_store_string_func(p_store_string_ud, itos(ptr[i]));
			}

			p_store_string_func(p_store_string_ud, " )");

		} break;
		case ToV4::POOL_REAL_ARRAY: { // v2 FloatArray

			p_store_string_func(p_store_string_ud, ver_major == 2 ? "FloatArray( " : "PoolRealArray( ");
			Vector<real_t> data = p_variant;
			int len = data.size();
			const real_t *ptr = data.ptr();

			for (int i = 0; i < len; i++) {
				if (i > 0)
					p_store_string_func(p_store_string_ud, ", ");
				p_store_string_func(p_store_string_ud, rtosfix(ptr[i]));
			}

			p_store_string_func(p_store_string_ud, " )");

		} break;
		case ToV4::POOL_STRING_ARRAY: { // v2 StringArray

			p_store_string_func(p_store_string_ud, ver_major == 2 ? "StringArray( " : "PoolStringArray( ");
			Vector<String> data = p_variant;
			int len = data.size();

			const String *ptr = data.ptr();
			String s;
			//write_string("\n");

			for (int i = 0; i < len; i++) {
				if (i > 0)
					p_store_string_func(p_store_string_ud, ", ");
				String str = ptr[i];
				p_store_string_func(p_store_string_ud, "\"" + str.c_escape() + "\"");
			}

			p_store_string_func(p_store_string_ud, " )");

		} break;
		case ToV4::POOL_VECTOR2_ARRAY: { // v2 Vector2Array

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
		case ToV4::POOL_VECTOR3_ARRAY: { // v2 Vector3Array

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
		case ToV4::POOL_COLOR_ARRAY: { // v2 ColorArray

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

Error VariantWriterCompat::write_to_string(const Variant &p_variant, String &r_string, const uint32_t ver_major, EncodeResourceFunc p_encode_res_func, void *p_encode_res_ud) {
	r_string = String();
	return write_compat(p_variant, ver_major, _write_to_str, &r_string, p_encode_res_func, p_encode_res_ud);
}
