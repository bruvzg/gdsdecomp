/*************************************************************************/
/*  bytecode_base.cpp                                                    */
/*************************************************************************/

#include "bytecode_base.h"
#include "compat/file_access_encrypted_v3.h"
#include "compat/variant_writer_compat.h"

#include "core/config/engine.h"
#include "core/io/file_access.h"
#include "core/io/file_access_encrypted.h"
#include "core/io/image.h"
#include "core/io/marshalls.h"

#include <limits.h>

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

static const Pair<String, Pair<int, int>> builtin_func_arg_elements[] = {
	{ "sin", Pair<int, int>(1, 1) },
	{ "cos", Pair<int, int>(1, 1) },
	{ "tan", Pair<int, int>(1, 1) },
	{ "sinh", Pair<int, int>(1, 1) },
	{ "cosh", Pair<int, int>(1, 1) },
	{ "tanh", Pair<int, int>(1, 1) },
	{ "asin", Pair<int, int>(1, 1) },
	{ "acos", Pair<int, int>(1, 1) },
	{ "atan", Pair<int, int>(1, 1) },
	{ "atan2", Pair<int, int>(2, 2) },
	{ "sqrt", Pair<int, int>(1, 1) },
	{ "fmod", Pair<int, int>(2, 2) },
	{ "fposmod", Pair<int, int>(2, 2) },
	{ "posmod", Pair<int, int>(2, 2) },
	{ "floor", Pair<int, int>(1, 1) },
	{ "ceil", Pair<int, int>(1, 1) },
	{ "round", Pair<int, int>(1, 1) },
	{ "abs", Pair<int, int>(1, 1) },
	{ "sign", Pair<int, int>(1, 1) },
	{ "pow", Pair<int, int>(2, 2) },
	{ "log", Pair<int, int>(1, 1) },
	{ "exp", Pair<int, int>(1, 1) },
	{ "is_nan", Pair<int, int>(1, 1) },
	{ "is_inf", Pair<int, int>(1, 1) },
	{ "is_equal_approx", Pair<int, int>(2, 2) },
	{ "is_zero_approx", Pair<int, int>(1, 1) },
	{ "ease", Pair<int, int>(2, 2) },
	{ "decimals", Pair<int, int>(1, 1) },
	{ "step_decimals", Pair<int, int>(1, 1) },
	{ "stepify", Pair<int, int>(2, 2) },
	{ "lerp", Pair<int, int>(3, 3) },
	{ "lerp_angle", Pair<int, int>(3, 3) },
	{ "inverse_lerp", Pair<int, int>(3, 3) },
	{ "range_lerp", Pair<int, int>(5, 5) },
	{ "smoothstep", Pair<int, int>(3, 3) },
	{ "move_toward", Pair<int, int>(3, 3) },
	{ "dectime", Pair<int, int>(3, 3) },
	{ "randomize", Pair<int, int>(0, 0) },
	{ "randi", Pair<int, int>(0, 0) },
	{ "randf", Pair<int, int>(0, 0) },
	{ "rand_range", Pair<int, int>(2, 2) },
	{ "seed", Pair<int, int>(1, 1) },
	{ "rand_seed", Pair<int, int>(1, 1) },
	{ "deg2rad", Pair<int, int>(1, 1) },
	{ "rad2deg", Pair<int, int>(1, 1) },
	{ "linear2db", Pair<int, int>(1, 1) },
	{ "db2linear", Pair<int, int>(1, 1) },
	{ "polar2cartesian", Pair<int, int>(2, 2) },
	{ "cartesian2polar", Pair<int, int>(2, 2) },
	{ "wrapi", Pair<int, int>(3, 3) },
	{ "wrapf", Pair<int, int>(3, 3) },
	{ "max", Pair<int, int>(2, 2) },
	{ "min", Pair<int, int>(2, 2) },
	{ "clamp", Pair<int, int>(3, 3) },
	{ "nearest_po2", Pair<int, int>(1, 1) },
	{ "weakref", Pair<int, int>(1, 1) },
	{ "funcref", Pair<int, int>(2, 2) },
	{ "convert", Pair<int, int>(2, 2) },
	{ "typeof", Pair<int, int>(1, 1) },
	{ "type_exists", Pair<int, int>(1, 1) },
	{ "char", Pair<int, int>(1, 1) },
	{ "ord", Pair<int, int>(1, 1) },
	{ "str", Pair<int, int>(1, INT_MAX) },
	{ "print", Pair<int, int>(0, INT_MAX) },
	{ "printt", Pair<int, int>(0, INT_MAX) },
	{ "prints", Pair<int, int>(0, INT_MAX) },
	{ "printerr", Pair<int, int>(0, INT_MAX) },
	{ "printraw", Pair<int, int>(0, INT_MAX) },
	{ "print_debug", Pair<int, int>(0, INT_MAX) },
	{ "push_error", Pair<int, int>(1, 1) },
	{ "push_warning", Pair<int, int>(1, 1) },
	{ "var2str", Pair<int, int>(1, 1) },
	{ "str2var", Pair<int, int>(1, 1) },
	{ "var2bytes", Pair<int, int>(1, 1) }, // 3.1.1 onwards added an optional 2nd argument, handled below
	{ "bytes2var", Pair<int, int>(1, 1) }, // 3.1.1 onwards added an optional 2nd argument, handled below
	{ "range", Pair<int, int>(1, 3) },
	{ "load", Pair<int, int>(1, 1) },
	{ "inst2dict", Pair<int, int>(1, 1) },
	{ "dict2inst", Pair<int, int>(1, 1) },
	{ "validate_json", Pair<int, int>(1, 1) },
	{ "parse_json", Pair<int, int>(1, 1) },
	{ "to_json", Pair<int, int>(1, 1) },
	{ "hash", Pair<int, int>(1, 1) },
	{ "Color8", Pair<int, int>(3, 3) },
	{ "ColorN", Pair<int, int>(1, 2) },
	{ "print_stack", Pair<int, int>(0, 0) },
	{ "get_stack", Pair<int, int>(0, 0) },
	{ "instance_from_id", Pair<int, int>(1, 1) },
	{ "len", Pair<int, int>(1, 1) },
	{ "is_instance_valid", Pair<int, int>(1, 1) },
	{ "deep_equal", Pair<int, int>(2, 2) },
	{ "get_inst", Pair<int, int>(1, 1) } // rename of instance_from_id
};
static constexpr int BUILTIN_FUNC_ARG_ELEMENTS_MAX = sizeof(builtin_func_arg_elements) / sizeof(builtin_func_arg_elements[0]);

HashMap<String, Pair<int, int>> _inithashmap() {
	HashMap<String, Pair<int, int>> hm;
	for (int i = 0; i < BUILTIN_FUNC_ARG_ELEMENTS_MAX; i++) {
		hm[builtin_func_arg_elements[i].first] = builtin_func_arg_elements[i].second;
	}
	return hm;
}

static const HashMap<String, Pair<int, int>> builtin_func_arg_map = _inithashmap();

Pair<int, int> GDScriptDecomp::get_arg_count_for_builtin(String builtin_func_name) {
	if (!builtin_func_arg_map.has(builtin_func_name)) {
		return Pair<int, int>(-1, -1);
	}
	switch (bytecode_rev) {
		case 0xf3f05dc: // 4.0 dev
		case 0x506df14:
		case 0xa7aad78: // 3.5
		case 0x5565f55: // 3.2
		case 0x6694c11: // 3.2 dev
		case 0xa60f242:
		case 0xc00427a:
		case 0x620ec47:
		case 0x7f7d97f:
		case 0x514a3fb: // 3.1.1
			if (builtin_func_name == "var2bytes" || builtin_func_name == "bytes2var") {
				return Pair<int, int>(1, 2);
			}
			break;
		default:
			break;
	}
	return builtin_func_arg_map[builtin_func_name];
}

Error GDScriptDecomp::decompile_byte_code_encrypted(const String &p_path, Vector<uint8_t> p_key) {
	Vector<uint8_t> bytecode;
	Error err = get_buffer_encrypted(p_path, engine_ver_major, p_key, bytecode);
	if (err != OK) {
		if (err == ERR_BUG) {
			error_message = RTR("FAE doesn't exist...???");
		} else if (err == ERR_UNAUTHORIZED) {
			error_message = RTR("Encryption Error");
		} else {
			error_message = RTR("File Error");
		}
		ERR_FAIL_V_MSG(err, error_message);
	}
	error_message = RTR("No error");
	return decompile_buffer(bytecode);
}

Error GDScriptDecomp::decompile_byte_code(const String &p_path) {
	Vector<uint8_t> bytecode;

	bytecode = FileAccess::get_file_as_bytes(p_path);

	error_message = RTR("No error");

	return decompile_buffer(bytecode);
}

Error GDScriptDecomp::get_buffer_encrypted(const String &p_path, int engine_ver_major, Vector<uint8_t> p_key, Vector<uint8_t> &bytecode) {
	Ref<FileAccess> fa = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V(fa.is_null(), ERR_FILE_CANT_OPEN);

	// Godot v3 only encrypted the scripts and used a different format with different header fields,
	// So we need to use an older version of FAE to access them
	if (engine_ver_major == 3) {
		Ref<FileAccessEncryptedv3> fae;
		fae.instantiate();
		ERR_FAIL_COND_V(fae.is_null(), ERR_BUG);

		Error err = fae->open_and_parse(fa, p_key, FileAccessEncryptedv3::MODE_READ);
		ERR_FAIL_COND_V(err != OK, ERR_UNAUTHORIZED);

		bytecode.resize(fae->get_length());
		fae->get_buffer(bytecode.ptrw(), bytecode.size());
	} else {
		Ref<FileAccessEncrypted> fae;
		fae.instantiate();
		ERR_FAIL_COND_V(fae.is_null(), ERR_BUG);

		Error err = fae->open_and_parse(fa, p_key, FileAccessEncrypted::MODE_READ);
		ERR_FAIL_COND_V(err != OK, ERR_UNAUTHORIZED);

		bytecode.resize(fae->get_length());
		fae->get_buffer(bytecode.ptrw(), bytecode.size());
	}
	return OK;
}

String GDScriptDecomp::get_script_text() {
	return script_text;
}

String GDScriptDecomp::get_error_message() {
	return error_message;
}

String GDScriptDecomp::get_constant_string(Vector<Variant> &constants, uint32_t constId) {
	String constString;
	Error err = VariantWriterCompat::write_to_string(constants[constId], constString, variant_ver_major);
	ERR_FAIL_COND_V(err, "");
	if (constants[constId].get_type() == Variant::Type::STRING) {
		constString = constString.replace("\n", "\\n");
	}
	return constString;
}

Error GDScriptDecomp::get_ids_consts_tokens(const Vector<uint8_t> &p_buffer, int bytecode_version, Vector<StringName> &identifiers, Vector<Variant> &constants, Vector<uint32_t> &tokens) {
	//Load bytecode
	VMap<uint32_t, uint32_t> lines;

	const uint8_t *buf = p_buffer.ptr();
	int total_len = p_buffer.size();
	ERR_FAIL_COND_V(p_buffer.size() < 24 || p_buffer[0] != 'G' || p_buffer[1] != 'D' || p_buffer[2] != 'S' || p_buffer[3] != 'C', ERR_INVALID_DATA);

	int version = decode_uint32(&buf[4]);
	if (version != bytecode_version) {
		ERR_FAIL_COND_V(version > bytecode_version, ERR_INVALID_DATA);
	}
	int identifier_count = decode_uint32(&buf[8]);
	int constant_count = decode_uint32(&buf[12]);
	int line_count = decode_uint32(&buf[16]);
	int token_count = decode_uint32(&buf[20]);

	const uint8_t *b = &buf[24];
	total_len -= 24;

	identifiers.resize(identifier_count);
	for (int i = 0; i < identifier_count; i++) {
		int len = decode_uint32(b);
		ERR_FAIL_COND_V(len > total_len, ERR_INVALID_DATA);
		b += 4;
		Vector<uint8_t> cs;
		cs.resize(len);
		for (int j = 0; j < len; j++) {
			cs.write[j] = b[j] ^ 0xb6;
		}

		cs.write[cs.size() - 1] = 0;
		String s;
		s.parse_utf8((const char *)cs.ptr());
		b += len;
		total_len -= len + 4;
		identifiers.write[i] = s;
	}

	constants.resize(constant_count);
	for (int i = 0; i < constant_count; i++) {
		Variant v;
		int len;
		Error err = VariantDecoderCompat::decode_variant_compat(variant_ver_major, v, b, total_len, &len);
		if (err) {
			error_message = RTR("Invalid constant");
			return err;
		}
		b += len;
		total_len -= len;
		constants.write[i] = v;
	}

	ERR_FAIL_COND_V(line_count * 8 > total_len, ERR_INVALID_DATA);

	for (int i = 0; i < line_count; i++) {
		uint32_t token = decode_uint32(b);
		b += 4;
		uint32_t linecol = decode_uint32(b);
		b += 4;

		lines.insert(token, linecol);
		total_len -= 8;
	}
	tokens.resize(token_count);
	for (int i = 0; i < token_count; i++) {
		ERR_FAIL_COND_V(total_len < 1, ERR_INVALID_DATA);

		if ((*b) & 0x80) { //BYTECODE_MASK, little endian always
			ERR_FAIL_COND_V(total_len < 4, ERR_INVALID_DATA);

			tokens.write[i] = decode_uint32(b) & ~0x80;
			b += 4;
		} else {
			tokens.write[i] = *b;
			b += 1;
			total_len--;
		}
	}
	return OK;
}