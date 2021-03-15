#include "variant_writer_compat.h"
#include "core/input/input_event.h"
#include "core/io/resource_loader.h"
#include "core/os/keyboard.h"
#include "core/string/string_buffer.h"
#include "core/io/image.h"
#include "core/variant/variant_parser.h"
namespace V2toV4{

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
	RECT2I,   //Unused 
	VECTOR3, 
	VECTOR3I,  //Unused
    MATRIX32,	//Transform2D
    PLANE,
    QUAT, // 10
    _AABB, //sorry naming convention fail :( not like it's used often
    MATRIX3,  //Basis
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

    // arrays
    RAW_ARRAY,//Byte array
    INT_ARRAY, 
	INT64_ARRAY, //unused
    REAL_ARRAY,
	REAL64_ARRAY, //Unused
    STRING_ARRAY, // 25
    VECTOR2_ARRAY,
    VECTOR3_ARRAY,
    COLOR_ARRAY,

    VARIANT_MAX

};

}
namespace V3toV4{

enum Type {

	NIL,

	// atomic types
	BOOL,
	INT,
	REAL,
	STRING,

	// math types

	VECTOR2, // 5
	VECTOR2I, //Unused
	RECT2,
	RECT2I,   //Unused 
	VECTOR3, 
	VECTOR3I,  //10, Unused
	TRANSFORM2D,
	PLANE,
	QUAT, // 10
	_AABB,
	BASIS,
	TRANSFORM,

	// misc types
	COLOR,
	STRING_NAME, // Unused
	NODE_PATH, // 15
	_RID,
	OBJECT,
	CALLABLE, //Unused
	SIGNAL,   //Unused
	DICTIONARY,
	ARRAY,

	// arrays Pool -> Packed
	POOL_BYTE_ARRAY, // 20
	POOL_INT_ARRAY,
	POOL_INT64_ARRAY, //Unused
	POOL_REAL_ARRAY,
	POOL_REAL64_ARRAY, //Unused
	POOL_STRING_ARRAY,
	POOL_VECTOR2_ARRAY,
	POOL_VECTOR3_ARRAY, // 25
	POOL_COLOR_ARRAY,

	VARIANT_MAX

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

Error VariantWriterCompat::writeV2(const Variant &p_variant, StoreStringFunc p_store_string_func, void *p_store_string_ud, EncodeResourceFunc p_encode_res_func, void *p_encode_res_ud) {

	switch (p_variant.get_type()) {

		case V2toV4::Type::NIL: {
			p_store_string_func(p_store_string_ud, "null");
		} break;
		case V2toV4::Type::BOOL: {

			p_store_string_func(p_store_string_ud, p_variant.operator bool() ? "true" : "false");
		} break;
		case V2toV4::Type::INT: {

			p_store_string_func(p_store_string_ud, itos(p_variant.operator int()));
		} break;
		case V2toV4::Type::REAL: {

			String s = rtosfix(p_variant.operator real_t());
			if (s.find(".") == -1 && s.find("e") == -1)
				s += ".0";
			p_store_string_func(p_store_string_ud, s);
		} break;
		case V2toV4::Type::STRING: {

			String str = p_variant;

			str = "\"" + str.c_escape_multiline() + "\"";
			p_store_string_func(p_store_string_ud, str);
		} break;
		case V2toV4::Type::VECTOR2: {

			Vector2 v = p_variant;
			p_store_string_func(p_store_string_ud, "Vector2( " + rtosfix(v.x) + ", " + rtosfix(v.y) + " )");
		} break;
		case V2toV4::Type::RECT2: {

			Rect2 aabb = p_variant;
			p_store_string_func(p_store_string_ud, "Rect2( " + rtosfix(aabb.position.x) + ", " + rtosfix(aabb.position.y) + ", " + rtosfix(aabb.size.x) + ", " + rtosfix(aabb.size.y) + " )");

		} break;
		case V2toV4::Type::VECTOR3: {

			Vector3 v = p_variant;
			p_store_string_func(p_store_string_ud, "Vector3( " + rtosfix(v.x) + ", " + rtosfix(v.y) + ", " + rtosfix(v.z) + " )");
		} break;
		case V2toV4::Type::PLANE: {

			Plane p = p_variant;
			p_store_string_func(p_store_string_ud, "Plane( " + rtosfix(p.normal.x) + ", " + rtosfix(p.normal.y) + ", " + rtosfix(p.normal.z) + ", " + rtosfix(p.d) + " )");

		} break;
		case V2toV4::Type::_AABB: {

			AABB aabb = p_variant;
			p_store_string_func(p_store_string_ud, "AABB( " + rtosfix(aabb.position.x) + ", " + rtosfix(aabb.position.y) + ", " + rtosfix(aabb.position.z) + ", " + rtosfix(aabb.size.x) + ", " + rtosfix(aabb.size.y) + ", " + rtosfix(aabb.size.z) + " )");

		} break;
		case V2toV4::Type::QUAT: {

			Quat quat = p_variant;
			p_store_string_func(p_store_string_ud, "Quat( " + rtosfix(quat.x) + ", " + rtosfix(quat.y) + ", " + rtosfix(quat.z) + ", " + rtosfix(quat.w) + " )");

		} break;
		case V2toV4::Type::MATRIX32: {

			String s = "Matrix32( ";
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
		case V2toV4::Type::MATRIX3: {
			String s = "Matrix3( ";
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
		case V2toV4::Type::TRANSFORM: {

			String s = "Transform( ";
			Transform t = p_variant;
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

		// misc types
		case V2toV4::Type::COLOR: {

			Color c = p_variant;
			p_store_string_func(p_store_string_ud, "Color( " + rtosfix(c.r) + ", " + rtosfix(c.g) + ", " + rtosfix(c.b) + ", " + rtosfix(c.a) + " )");

		} break;
		case V2toV4::Type::IMAGE: {

			// Image img = p_variant;

			// if (img.empty()) {
			// 	p_store_string_func(p_store_string_ud, "Image()");
			// 	break;
			// }

			// String imgstr = "Image( ";
			// imgstr += itos(img.get_width());
			// imgstr += ", " + itos(img.get_height());
			// imgstr += ", " + itos(img.get_mipmaps());
			// imgstr += ", ";

			// switch (img.get_format()) {

			// 	case Image::FORMAT_GRAYSCALE: imgstr += "GRAYSCALE"; break;
			// 	case Image::FORMAT_INTENSITY: imgstr += "INTENSITY"; break;
			// 	case Image::FORMAT_GRAYSCALE_ALPHA: imgstr += "GRAYSCALE_ALPHA"; break;
			// 	case Image::FORMAT_RGB: imgstr += "RGB"; break;
			// 	case Image::FORMAT_RGBA: imgstr += "RGBA"; break;
			// 	case Image::FORMAT_INDEXED: imgstr += "INDEXED"; break;
			// 	case Image::FORMAT_INDEXED_ALPHA: imgstr += "INDEXED_ALPHA"; break;
			// 	case Image::FORMAT_BC1: imgstr += "BC1"; break;
			// 	case Image::FORMAT_BC2: imgstr += "BC2"; break;
			// 	case Image::FORMAT_BC3: imgstr += "BC3"; break;
			// 	case Image::FORMAT_BC4: imgstr += "BC4"; break;
			// 	case Image::FORMAT_BC5: imgstr += "BC5"; break;
			// 	case Image::FORMAT_PVRTC2: imgstr += "PVRTC2"; break;
			// 	case Image::FORMAT_PVRTC2_ALPHA: imgstr += "PVRTC2_ALPHA"; break;
			// 	case Image::FORMAT_PVRTC4: imgstr += "PVRTC4"; break;
			// 	case Image::FORMAT_PVRTC4_ALPHA: imgstr += "PVRTC4_ALPHA"; break;
			// 	case Image::FORMAT_ETC: imgstr += "ETC"; break;
			// 	case Image::FORMAT_ATC: imgstr += "ATC"; break;
			// 	case Image::FORMAT_ATC_ALPHA_EXPLICIT: imgstr += "ATC_ALPHA_EXPLICIT"; break;
			// 	case Image::FORMAT_ATC_ALPHA_INTERPOLATED: imgstr += "ATC_ALPHA_INTERPOLATED"; break;
			// 	case Image::FORMAT_CUSTOM: imgstr += "CUSTOM"; break;
			// 	default: {
			// 	}
			// }

			// String s;

			// Vector<uint8_t> data = img.get_data();
			// int len = data.size();

			// const uint8_t *ptr = data.ptr();
			// for (int i = 0; i < len; i++) {

			// 	if (i > 0)
			// 		s += ", ";
			// 	s += itos(ptr[i]);
			// }

			// imgstr += ", ";
			// p_store_string_func(p_store_string_ud, imgstr);
			// p_store_string_func(p_store_string_ud, s);
			// p_store_string_func(p_store_string_ud, " )");
		} break;
		case V2toV4::Type::NODE_PATH: {

			String str = p_variant;

			str = "NodePath(\"" + str.c_escape() + "\")";
			p_store_string_func(p_store_string_ud, str);

		} break;

		case V2toV4::Type::OBJECT: {

			RES res = p_variant;
			if (res.is_null()) {
				p_store_string_func(p_store_string_ud, "null");
				break; // don't save it
			}

			String res_text;

			if (p_encode_res_func) {

				res_text = p_encode_res_func(p_encode_res_ud, res);
			}

			if (res_text == String() && res->get_path().is_resource_file()) {

				//external resource
				String path = res->get_path();
				res_text = "Resource( \"" + path + "\")";
			}

			if (res_text == String())
				res_text = "null";

			p_store_string_func(p_store_string_ud, res_text);

		} break;
		case V2toV4::Type::INPUT_EVENT: {

			// String str = "InputEvent(";

			// InputEventV2 ev = p_variant;
			// switch (ev.type) {
			// 	case InputEventV2::KEY: {

			// 		str += "KEY," + itos(ev.key.scancode);
			// 		String mod;
			// 		if (ev.key.mod.alt)
			// 			mod += "A";
			// 		if (ev.key.mod.shift)
			// 			mod += "S";
			// 		if (ev.key.mod.control)
			// 			mod += "C";
			// 		if (ev.key.mod.meta)
			// 			mod += "M";

			// 		if (mod != String())
			// 			str += "," + mod;
			// 	} break;
			// 	case InputEventV2::MOUSE_BUTTON: {

			// 		str += "MBUTTON," + itos(ev.mouse_button.button_index);
			// 	} break;
			// 	case InputEventV2::JOYSTICK_BUTTON: {
			// 		str += "JBUTTON," + itos(ev.joy_button.button_index);

			// 	} break;
			// 	case InputEventV2::JOYSTICK_MOTION: {
			// 		str += "JAXIS," + itos(ev.joy_motion.axis) + "," + itos(ev.joy_motion.axis_value);
			// 	} break;
			// 	case InputEventV2::NONE: {
			// 		str += "NONE";
			// 	} break;
			// 	default: {
			// 	}
			// }

			// str += ")";

			// p_store_string_func(p_store_string_ud, str); //will be added later

		} break;
		case V2toV4::Type::DICTIONARY: {

			Dictionary dict = p_variant;

			List<Variant> keys;
			dict.get_key_list(&keys);
			keys.sort();

			p_store_string_func(p_store_string_ud, "{\n");
			for (List<Variant>::Element *E = keys.front(); E; E = E->next()) {

				//if (!_check_type(dict[E->get()]))
				//	continue;
				writeV2(E->get(), p_store_string_func, p_store_string_ud, p_encode_res_func, p_encode_res_ud);
				p_store_string_func(p_store_string_ud, ": ");
				writeV2(dict[E->get()], p_store_string_func, p_store_string_ud, p_encode_res_func, p_encode_res_ud);
				if (E->next())
					p_store_string_func(p_store_string_ud, ",\n");
			}

			p_store_string_func(p_store_string_ud, "\n}");

		} break;
		case V2toV4::Type::ARRAY: {

			p_store_string_func(p_store_string_ud, "[ ");
			Array array = p_variant;
			int len = array.size();
			for (int i = 0; i < len; i++) {

				if (i > 0)
					p_store_string_func(p_store_string_ud, ", ");
				writeV2(array[i], p_store_string_func, p_store_string_ud, p_encode_res_func, p_encode_res_ud);
			}
			p_store_string_func(p_store_string_ud, " ]");

		} break;

		case V2toV4::Type::RAW_ARRAY: {

			p_store_string_func(p_store_string_ud, "ByteArray( ");
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
		case V2toV4::Type::INT_ARRAY: {

			p_store_string_func(p_store_string_ud, "IntArray( ");
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
		case V2toV4::Type::REAL_ARRAY: {

			p_store_string_func(p_store_string_ud, "FloatArray( ");
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
		case V2toV4::Type::STRING_ARRAY: {

			p_store_string_func(p_store_string_ud, "StringArray( ");
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
		case V2toV4::Type::VECTOR2_ARRAY: {

			p_store_string_func(p_store_string_ud, "Vector2Array( ");
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
		case V2toV4::Type::VECTOR3_ARRAY: {

			p_store_string_func(p_store_string_ud, "Vector3Array( ");
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
		case V2toV4::Type::COLOR_ARRAY: {

			p_store_string_func(p_store_string_ud, "ColorArray( ");

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


Error VariantWriterCompat::writeV3(const Variant &p_variant, StoreStringFunc p_store_string_func, void *p_store_string_ud, EncodeResourceFunc p_encode_res_func, void *p_encode_res_ud) {

	switch (p_variant.get_type()) {

		case V3toV4::Type::NIL: {
			p_store_string_func(p_store_string_ud, "null");
		} break;
		case V3toV4::Type::BOOL: {

			p_store_string_func(p_store_string_ud, p_variant.operator bool() ? "true" : "false");
		} break;
		case V3toV4::Type::INT: {

			p_store_string_func(p_store_string_ud, itos(p_variant.operator int()));
		} break;
		case V3toV4::Type::REAL: {

			String s = rtosfix(p_variant.operator real_t());
			if (s.find(".") == -1 && s.find("e") == -1)
				s += ".0";
			p_store_string_func(p_store_string_ud, s);
		} break;
		case V3toV4::Type::STRING: {

			String str = p_variant;

			str = "\"" + str.c_escape_multiline() + "\"";
			p_store_string_func(p_store_string_ud, str);
		} break;
		case V3toV4::Type::VECTOR2: {

			Vector2 v = p_variant;
			p_store_string_func(p_store_string_ud, "Vector2( " + rtosfix(v.x) + ", " + rtosfix(v.y) + " )");
		} break;
		case V3toV4::Type::RECT2: {

			Rect2 aabb = p_variant;
			p_store_string_func(p_store_string_ud, "Rect2( " + rtosfix(aabb.position.x) + ", " + rtosfix(aabb.position.y) + ", " + rtosfix(aabb.size.x) + ", " + rtosfix(aabb.size.y) + " )");

		} break;
		case V3toV4::Type::VECTOR3: {

			Vector3 v = p_variant;
			p_store_string_func(p_store_string_ud, "Vector3( " + rtosfix(v.x) + ", " + rtosfix(v.y) + ", " + rtosfix(v.z) + " )");
		} break;
		case V3toV4::Type::PLANE: {

			Plane p = p_variant;
			p_store_string_func(p_store_string_ud, "Plane( " + rtosfix(p.normal.x) + ", " + rtosfix(p.normal.y) + ", " + rtosfix(p.normal.z) + ", " + rtosfix(p.d) + " )");

		} break;
		case V3toV4::Type::_AABB: {

			AABB aabb = p_variant;
			p_store_string_func(p_store_string_ud, "AABB( " + rtosfix(aabb.position.x) + ", " + rtosfix(aabb.position.y) + ", " + rtosfix(aabb.position.z) + ", " + rtosfix(aabb.size.x) + ", " + rtosfix(aabb.size.y) + ", " + rtosfix(aabb.size.z) + " )");

		} break;
		case V3toV4::Type::QUAT: {

			Quat quat = p_variant;
			p_store_string_func(p_store_string_ud, "Quat( " + rtosfix(quat.x) + ", " + rtosfix(quat.y) + ", " + rtosfix(quat.z) + ", " + rtosfix(quat.w) + " )");

		} break;
		case V3toV4::Type::TRANSFORM2D: {

			String s = "Transform2D( ";
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
		case V3toV4::Type::BASIS: {

			String s = "Basis( ";
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
		case V3toV4::Type::TRANSFORM: {

			String s = "Transform( ";
			Transform t = p_variant;
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

		// misc types
		case V3toV4::Type::COLOR: {

			Color c = p_variant;
			p_store_string_func(p_store_string_ud, "Color( " + rtosfix(c.r) + ", " + rtosfix(c.g) + ", " + rtosfix(c.b) + ", " + rtosfix(c.a) + " )");

		} break;
		case V3toV4::Type::NODE_PATH: {

			String str = p_variant;

			str = "NodePath(\"" + str.c_escape() + "\")";
			p_store_string_func(p_store_string_ud, str);

		} break;

		case V3toV4::Type::OBJECT: {

			Object *obj = p_variant;

			if (!obj) {
				p_store_string_func(p_store_string_ud, "null");
				break; // don't save it
			}

			RES res = p_variant;
			if (res.is_valid()) {
				//is resource
				String res_text;

				//try external function
				if (p_encode_res_func) {

					res_text = p_encode_res_func(p_encode_res_ud, res);
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
					writeV3(obj->get(E->get().name), p_store_string_func, p_store_string_ud, p_encode_res_func, p_encode_res_ud);
				}
			}

			p_store_string_func(p_store_string_ud, ")\n");

		} break;

		case V3toV4::Type::DICTIONARY: {

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
				writeV3(E->get(), p_store_string_func, p_store_string_ud, p_encode_res_func, p_encode_res_ud);
				p_store_string_func(p_store_string_ud, ": ");
				writeV3(dict[E->get()], p_store_string_func, p_store_string_ud, p_encode_res_func, p_encode_res_ud);
				if (E->next())
					p_store_string_func(p_store_string_ud, ",\n");
			}

			p_store_string_func(p_store_string_ud, "\n}");

		} break;
		case V3toV4::Type::ARRAY: {

			p_store_string_func(p_store_string_ud, "[ ");
			Array array = p_variant;
			int len = array.size();
			for (int i = 0; i < len; i++) {

				if (i > 0)
					p_store_string_func(p_store_string_ud, ", ");
				writeV3(array[i], p_store_string_func, p_store_string_ud, p_encode_res_func, p_encode_res_ud);
			}
			p_store_string_func(p_store_string_ud, " ]");

		} break;

		case V3toV4::Type::POOL_BYTE_ARRAY: {

			p_store_string_func(p_store_string_ud, "PoolByteArray( ");
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
		case V3toV4::Type::POOL_INT_ARRAY: {

			p_store_string_func(p_store_string_ud, "PoolIntArray( ");
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
		case V3toV4::Type::POOL_REAL_ARRAY: {

			p_store_string_func(p_store_string_ud, "PoolRealArray( ");
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
		case V3toV4::Type::POOL_STRING_ARRAY: {

			p_store_string_func(p_store_string_ud, "PoolStringArray( ");
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
		case V3toV4::Type::POOL_VECTOR2_ARRAY: {

			p_store_string_func(p_store_string_ud, "PoolVector2Array( ");
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
		case V3toV4::Type::POOL_VECTOR3_ARRAY: {

			p_store_string_func(p_store_string_ud, "PoolVector3Array( ");
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
		case V3toV4::Type::POOL_COLOR_ARRAY: {

			p_store_string_func(p_store_string_ud, "PoolColorArray( ");

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
		default: {}
	}

	return OK;
}



static Error _write_to_str(void *ud, const String &p_string) {

	String *str = (String *)ud;
	(*str) += p_string;
	return OK;
}

Error VariantWriterCompat::write_to_string(const Variant &p_variant, String &r_string, const uint32_t ver_major, EncodeResourceFunc p_encode_res_func, void *p_encode_res_ud) {

	if (ver_major == 2){
		r_string = String();
		return writeV2(p_variant, _write_to_str, &r_string, p_encode_res_func, p_encode_res_ud);
	} else if (ver_major == 3){
		r_string = String();
		return writeV3(p_variant, _write_to_str, &r_string, p_encode_res_func, p_encode_res_ud);
	} else {
		return VariantWriter::write_to_string(p_variant, r_string, p_encode_res_func, p_encode_res_ud);
	}
}