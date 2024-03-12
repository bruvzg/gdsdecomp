/*************************************************************************/
/*  bytecode_base.cpp                                                    */
/*************************************************************************/

#include "bytecode/bytecode_base.h"
#include "bytecode/bytecode_versions.h"
#include "bytecode/gdscript_tokenizer_compat.h"
#include "compat/file_access_encrypted_v3.h"
#include "compat/variant_writer_compat.h"

#include "core/config/engine.h"
#include "core/io/file_access.h"
#include "core/io/file_access_encrypted.h"
#include "core/io/marshalls.h"

#include <limits.h>

void GDScriptDecomp::_bind_methods() {
	BIND_ENUM_CONSTANT(BYTECODE_TEST_CORRUPT);
	BIND_ENUM_CONSTANT(BYTECODE_TEST_FAIL);
	BIND_ENUM_CONSTANT(BYTECODE_TEST_PASS);
	BIND_ENUM_CONSTANT(BYTECODE_TEST_UNKNOWN);
	ClassDB::bind_method(D_METHOD("decompile_byte_code", "path"), &GDScriptDecomp::decompile_byte_code);
	ClassDB::bind_method(D_METHOD("decompile_byte_code_encrypted", "path", "key"), &GDScriptDecomp::decompile_byte_code_encrypted);
	ClassDB::bind_method(D_METHOD("test_bytecode", "buffer"), &GDScriptDecomp::test_bytecode);

	ClassDB::bind_method(D_METHOD("get_script_text"), &GDScriptDecomp::get_script_text);
	ClassDB::bind_method(D_METHOD("get_error_message"), &GDScriptDecomp::get_error_message);
	ClassDB::bind_method(D_METHOD("get_bytecode_version"), &GDScriptDecomp::get_bytecode_version);
	ClassDB::bind_method(D_METHOD("get_bytecode_rev"), &GDScriptDecomp::get_bytecode_rev);
	ClassDB::bind_method(D_METHOD("get_engine_ver_major"), &GDScriptDecomp::get_engine_ver_major);
	ClassDB::bind_method(D_METHOD("get_variant_ver_major"), &GDScriptDecomp::get_variant_ver_major);
	ClassDB::bind_method(D_METHOD("get_function_count"), &GDScriptDecomp::get_function_count);
	// ClassDB::bind_method(D_METHOD("get_function_arg_count", "func_idx"), &GDScriptDecomp::get_function_arg_count);
	ClassDB::bind_method(D_METHOD("get_function_name", "func_idx"), &GDScriptDecomp::get_function_name);
	ClassDB::bind_method(D_METHOD("get_function_index", "func_name"), &GDScriptDecomp::get_function_index);
	ClassDB::bind_method(D_METHOD("get_token_max"), &GDScriptDecomp::get_token_max);
	// ClassDB::bind_method(D_METHOD("get_global_token", "token_val"), &GDScriptDecomp::get_global_token);
	// ClassDB::bind_method(D_METHOD("get_local_token_val", "global_token"), &GDScriptDecomp::get_local_token_val);

	ClassDB::bind_static_method("GDScriptDecomp", D_METHOD("create_decomp_for_commit", "commit_hash"), &GDScriptDecomp::create_decomp_for_commit);
	ClassDB::bind_static_method("GDScriptDecomp", D_METHOD("read_bytecode_version", "path"), &GDScriptDecomp::read_bytecode_version);
	ClassDB::bind_static_method("GDScriptDecomp", D_METHOD("read_bytecode_version_encrypted", "path", "engine_ver_major", "key"), &GDScriptDecomp::read_bytecode_version_encrypted);
}

void GDScriptDecomp::_ensure_space(String &p_code) {
	if (!p_code.ends_with(" ")) {
		p_code += String(" ");
	}
}

Error GDScriptDecomp::decompile_byte_code_encrypted(const String &p_path, Vector<uint8_t> p_key) {
	Vector<uint8_t> bytecode;
	Error err = get_buffer_encrypted(p_path, get_engine_ver_major(), p_key, bytecode);
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
	Error err = VariantWriterCompat::write_to_string(constants[constId], constString, get_variant_ver_major());
	ERR_FAIL_COND_V(err, "");
	if (constants[constId].get_type() == Variant::Type::STRING) {
		constString = constString.replace("\n", "\\n");
	}
	return constString;
}

Error GDScriptDecomp::get_ids_consts_tokens(const Vector<uint8_t> &p_buffer, int bytecode_version, Vector<StringName> &identifiers, Vector<Variant> &constants, Vector<uint32_t> &tokens, VMap<uint32_t, uint32_t> lines) {
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
		Error err = VariantDecoderCompat::decode_variant_compat(get_variant_ver_major(), v, b, total_len, &len);
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

Ref<GDScriptDecomp> GDScriptDecomp::create_decomp_for_commit(uint64_t p_commit_hash) {
	return Ref<GDScriptDecomp>(::create_decomp_for_commit(p_commit_hash));
}

int GDScriptDecomp::read_bytecode_version(const String &p_path) {
	Vector<uint8_t> p_buffer;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(f.is_null(), -1, "Cannot open file!");
	p_buffer = f->get_buffer(24);
	ERR_FAIL_COND_V_MSG(p_buffer.size() < 24 || p_buffer[0] != 'G' || p_buffer[1] != 'D' || p_buffer[2] != 'S' || p_buffer[3] != 'C', -1, "Invalid gdscript!");
	const uint8_t *buf = p_buffer.ptr();
	int version = decode_uint32(&buf[4]);
	return version;
}

int GDScriptDecomp::read_bytecode_version_encrypted(const String &p_path, int engine_ver_major, Vector<uint8_t> p_key) {
	Vector<uint8_t> p_buffer;
	Error err = get_buffer_encrypted(p_path, engine_ver_major, p_key, p_buffer);
	ERR_FAIL_COND_V_MSG(err != OK, -1, "Cannot open file!");
	ERR_FAIL_COND_V_MSG(p_buffer.size() < 24 || p_buffer[0] != 'G' || p_buffer[1] != 'D' || p_buffer[2] != 'S' || p_buffer[3] != 'C', -1, "Invalid gdscript!");
	const uint8_t *buf = p_buffer.ptr();
	int version = decode_uint32(&buf[4]);
	return version;
}

#if DEBUG_ENABLED
// template T where T is derived from Vector
template <typename T>
static int continuity_tester(const T &p_vector, const T &p_other, String name, int pos = 0) {
	if (p_vector.is_empty() && p_other.is_empty()) {
		return -1;
	}
	if (p_vector.is_empty() && !p_other.is_empty()) {
		WARN_PRINT(name + " first is empty");
		return -1;
	}
	if (!p_vector.is_empty() && p_other.is_empty()) {
		WARN_PRINT(name + " second is empty");
		return -1;
	}
	if (pos == 0) {
		if (p_vector.size() != p_other.size()) {
			WARN_PRINT(name + " size mismatch: " + itos(p_vector.size()) + " != " + itos(p_other.size()));
		}
	}
	if (pos >= p_vector.size() || pos >= p_other.size()) {
		WARN_PRINT(name + " pos out of range");
		return MIN(p_vector.size(), p_other.size());
	}
	for (int i = pos; i < p_vector.size(); i++) {
		if (i >= p_other.size()) {
			WARN_PRINT(name + " discontinuity at index " + itos(i));
			return i;
		}
		if (p_vector[i] != p_other[i]) {
			WARN_PRINT(name + " bytecode discontinuity at index " + itos(i));
			return i;
		}
	}
	return -1;
}
#endif

Error GDScriptDecomp::decompile_buffer(Vector<uint8_t> p_buffer) {
	//Cleanup
	script_text = String();

	//Load bytecode
	Vector<StringName> identifiers;
	Vector<Variant> constants;
	VMap<uint32_t, uint32_t> lines;
	Vector<uint32_t> tokens;
	int bytecode_version = get_bytecode_version();
	int variant_ver_major = get_variant_ver_major();
	int FUNC_MAX = get_function_count();

	const uint8_t *buf = p_buffer.ptr();
	ERR_FAIL_COND_V(p_buffer.size() < 24 || p_buffer[0] != 'G' || p_buffer[1] != 'D' || p_buffer[2] != 'S' || p_buffer[3] != 'C', ERR_INVALID_DATA);

	int version = decode_uint32(&buf[4]);
	ERR_FAIL_COND_V(version != get_bytecode_version(), ERR_INVALID_DATA);
	Error err = get_ids_consts_tokens(p_buffer, bytecode_version, identifiers, constants, tokens, lines);
	ERR_FAIL_COND_V(err != OK, err);

	//Decompile script
	String line;
	int indent = 0;

	GlobalToken prev_token = G_TK_NEWLINE;
	for (int i = 0; i < tokens.size(); i++) {
		GlobalToken curr_token = get_global_token(tokens[i] & TOKEN_MASK);
		switch (curr_token) {
			case G_TK_EMPTY: {
				//skip
			} break;
			case G_TK_IDENTIFIER: {
				uint32_t identifier = tokens[i] >> TOKEN_BITS;
				ERR_FAIL_COND_V(identifier >= (uint32_t)identifiers.size(), ERR_INVALID_DATA);
				line += String(identifiers[identifier]);
			} break;
			case G_TK_CONSTANT: {
				uint32_t constant = tokens[i] >> TOKEN_BITS;
				ERR_FAIL_COND_V(constant >= (uint32_t)constants.size(), ERR_INVALID_DATA);
				line += get_constant_string(constants, constant);
			} break;
			case G_TK_SELF: {
				line += "self";
			} break;
			case G_TK_BUILT_IN_TYPE: {
				line += VariantDecoderCompat::get_variant_type_name(tokens[i] >> TOKEN_BITS, variant_ver_major);
			} break;
			case G_TK_BUILT_IN_FUNC: {
				ERR_FAIL_COND_V(tokens[i] >> TOKEN_BITS >= FUNC_MAX, ERR_INVALID_DATA);
				line += get_function_name(tokens[i] >> TOKEN_BITS);
			} break;
			case G_TK_OP_IN: {
				_ensure_space(line);
				line += "in ";
			} break;
			case G_TK_OP_EQUAL: {
				_ensure_space(line);
				line += "== ";
			} break;
			case G_TK_OP_NOT_EQUAL: {
				_ensure_space(line);
				line += "!= ";
			} break;
			case G_TK_OP_LESS: {
				_ensure_space(line);
				line += "< ";
			} break;
			case G_TK_OP_LESS_EQUAL: {
				_ensure_space(line);
				line += "<= ";
			} break;
			case G_TK_OP_GREATER: {
				_ensure_space(line);
				line += "> ";
			} break;
			case G_TK_OP_GREATER_EQUAL: {
				_ensure_space(line);
				line += ">= ";
			} break;
			case G_TK_OP_AND: {
				_ensure_space(line);
				line += "and ";
			} break;
			case G_TK_OP_OR: {
				_ensure_space(line);
				line += "or ";
			} break;
			case G_TK_OP_NOT: {
				_ensure_space(line);
				line += "not ";
			} break;
			case G_TK_OP_ADD: {
				_ensure_space(line);
				line += "+ ";
			} break;
			case G_TK_OP_SUB: {
				if (prev_token != G_TK_NEWLINE)
					_ensure_space(line);
				line += "- ";
				//TODO: do not add space after unary "-"
			} break;
			case G_TK_OP_MUL: {
				_ensure_space(line);
				line += "* ";
			} break;
			case G_TK_OP_DIV: {
				_ensure_space(line);
				line += "/ ";
			} break;
			case G_TK_OP_MOD: {
				_ensure_space(line);
				line += "% ";
			} break;
			case G_TK_OP_SHIFT_LEFT: {
				_ensure_space(line);
				line += "<< ";
			} break;
			case G_TK_OP_SHIFT_RIGHT: {
				_ensure_space(line);
				line += ">> ";
			} break;
			case G_TK_OP_ASSIGN: {
				_ensure_space(line);
				line += "= ";
			} break;
			case G_TK_OP_ASSIGN_ADD: {
				_ensure_space(line);
				line += "+= ";
			} break;
			case G_TK_OP_ASSIGN_SUB: {
				_ensure_space(line);
				line += "-= ";
			} break;
			case G_TK_OP_ASSIGN_MUL: {
				_ensure_space(line);
				line += "*= ";
			} break;
			case G_TK_OP_ASSIGN_DIV: {
				_ensure_space(line);
				line += "/= ";
			} break;
			case G_TK_OP_ASSIGN_MOD: {
				_ensure_space(line);
				line += "%= ";
			} break;
			case G_TK_OP_ASSIGN_SHIFT_LEFT: {
				_ensure_space(line);
				line += "<<= ";
			} break;
			case G_TK_OP_ASSIGN_SHIFT_RIGHT: {
				_ensure_space(line);
				line += ">>= ";
			} break;
			case G_TK_OP_ASSIGN_BIT_AND: {
				_ensure_space(line);
				line += "&= ";
			} break;
			case G_TK_OP_ASSIGN_BIT_OR: {
				_ensure_space(line);
				line += "|= ";
			} break;
			case G_TK_OP_ASSIGN_BIT_XOR: {
				_ensure_space(line);
				line += "^= ";
			} break;
			case G_TK_OP_BIT_AND: {
				_ensure_space(line);
				line += "& ";
			} break;
			case G_TK_OP_BIT_OR: {
				_ensure_space(line);
				line += "| ";
			} break;
			case G_TK_OP_BIT_XOR: {
				_ensure_space(line);
				line += "^ ";
			} break;
			case G_TK_OP_BIT_INVERT: {
				_ensure_space(line);
				line += "~ ";
			} break;
			//case G_TK_OP_PLUS_PLUS: {
			//	line += "++";
			//} break;
			//case G_TK_OP_MINUS_MINUS: {
			//	line += "--";
			//} break;
			case G_TK_CF_IF: {
				if (prev_token != G_TK_NEWLINE)
					_ensure_space(line);
				line += "if ";
			} break;
			case G_TK_CF_ELIF: {
				line += "elif ";
			} break;
			case G_TK_CF_ELSE: {
				if (prev_token != G_TK_NEWLINE)
					_ensure_space(line);
				line += "else ";
			} break;
			case G_TK_CF_FOR: {
				line += "for ";
			} break;
			case G_TK_CF_WHILE: {
				line += "while ";
			} break;
			case G_TK_CF_BREAK: {
				line += "break";
			} break;
			case G_TK_CF_CONTINUE: {
				line += "continue";
			} break;
			case G_TK_CF_PASS: {
				line += "pass";
			} break;
			case G_TK_CF_RETURN: {
				line += "return ";
			} break;
			case G_TK_CF_MATCH: {
				line += "match ";
			} break;
			case G_TK_PR_FUNCTION: {
				line += "func ";
			} break;
			case G_TK_PR_CLASS: {
				line += "class ";
			} break;
			case G_TK_PR_CLASS_NAME: {
				line += "class_name ";
			} break;
			case G_TK_PR_EXTENDS: {
				if (prev_token != G_TK_NEWLINE)
					_ensure_space(line);
				line += "extends ";
			} break;
			case G_TK_PR_IS: {
				_ensure_space(line);
				line += "is ";
			} break;
			case G_TK_PR_ONREADY: {
				line += "onready ";
			} break;
			case G_TK_PR_TOOL: {
				line += "tool ";
			} break;
			case G_TK_PR_STATIC: {
				line += "static ";
			} break;
			case G_TK_PR_EXPORT: {
				line += "export ";
			} break;
			case G_TK_PR_SETGET: {
				line += " setget ";
			} break;
			case G_TK_PR_CONST: {
				line += "const ";
			} break;
			case G_TK_PR_VAR: {
				if (line != String() && prev_token != G_TK_PR_ONREADY)
					line += " ";
				line += "var ";
			} break;
			case G_TK_PR_AS: {
				_ensure_space(line);
				line += "as ";
			} break;
			case G_TK_PR_VOID: {
				line += "void ";
			} break;
			case G_TK_PR_ENUM: {
				line += "enum ";
			} break;
			case G_TK_PR_PRELOAD: {
				line += "preload";
			} break;
			case G_TK_PR_ASSERT: {
				line += "assert ";
			} break;
			case G_TK_PR_YIELD: {
				line += "yield ";
			} break;
			case G_TK_PR_SIGNAL: {
				line += "signal ";
			} break;
			case G_TK_PR_BREAKPOINT: {
				line += "breakpoint ";
			} break;
			case G_TK_PR_REMOTE: {
				line += "remote ";
			} break;
			case G_TK_PR_SYNC: {
				line += "sync ";
			} break;
			case G_TK_PR_MASTER: {
				line += "master ";
			} break;
			case G_TK_PR_SLAVE: {
				line += "slave ";
			} break;
			case G_TK_PR_PUPPET: {
				line += "puppet ";
			} break;
			case G_TK_PR_REMOTESYNC: {
				line += "remotesync ";
			} break;
			case G_TK_PR_MASTERSYNC: {
				line += "mastersync ";
			} break;
			case G_TK_PR_PUPPETSYNC: {
				line += "puppetsync ";
			} break;
			case G_TK_BRACKET_OPEN: {
				line += "[";
			} break;
			case G_TK_BRACKET_CLOSE: {
				line += "]";
			} break;
			case G_TK_CURLY_BRACKET_OPEN: {
				line += "{";
			} break;
			case G_TK_CURLY_BRACKET_CLOSE: {
				line += "}";
			} break;
			case G_TK_PARENTHESIS_OPEN: {
				line += "(";
			} break;
			case G_TK_PARENTHESIS_CLOSE: {
				line += ")";
			} break;
			case G_TK_COMMA: {
				line += ", ";
			} break;
			case G_TK_SEMICOLON: {
				line += ";";
			} break;
			case G_TK_PERIOD: {
				line += ".";
			} break;
			case G_TK_QUESTION_MARK: {
				line += "?";
			} break;
			case G_TK_COLON: {
				line += ":";
			} break;
			case G_TK_DOLLAR: {
				line += "$";
			} break;
			case G_TK_FORWARD_ARROW: {
				line += "->";
			} break;
			case G_TK_NEWLINE: {
				for (int j = 0; j < indent; j++) {
					script_text += "\t";
				}
				script_text += line + "\n";
				line = String();
				indent = tokens[i] >> TOKEN_BITS;
			} break;
			case G_TK_CONST_PI: {
				line += "PI";
			} break;
			case G_TK_CONST_TAU: {
				line += "TAU";
			} break;
			case G_TK_WILDCARD: {
				line += "_";
			} break;
			case G_TK_CONST_INF: {
				line += "INF";
			} break;
			case G_TK_CONST_NAN: {
				line += "NAN";
			} break;
			case G_TK_PR_SLAVESYNC: {
				line += "slavesync ";
			} break;
			case G_TK_CF_DO: {
				line += "do ";
			} break;
			case G_TK_CF_CASE: {
				line += "case ";
			} break;
			case G_TK_CF_SWITCH: {
				line += "switch ";
			} break;
			case G_TK_ERROR: {
				//skip - invalid
			} break;
			case G_TK_EOF: {
				//skip - invalid
			} break;
			case G_TK_CURSOR: {
				//skip - invalid
			} break;
			case G_TK_MAX: {
				ERR_FAIL_COND_V_MSG(curr_token == G_TK_MAX, ERR_INVALID_DATA, "Invalid token");
			} break;
			default: {
				ERR_FAIL_V_MSG(ERR_INVALID_DATA, "Invalid token");
			}
		}
		prev_token = curr_token;
	}

	if (!line.is_empty()) {
		for (int j = 0; j < indent; j++) {
			script_text += "\t";
		}
		script_text += line + "\n";
	}

	if (script_text == String()) {
		error_message = RTR("Invalid token");
		return ERR_INVALID_DATA;
	}

	// Testing recompile of the bytecode
	// TODO: move this elsewhere
#if DEBUG_ENABLED
	Error new_err;
	Vector<uint8_t> recompiled_bytecode = compile_code_string(script_text);
	// compare the recompiled bytecode to the original bytecode
	int discontinuity = continuity_tester<Vector<uint8_t>>(recompiled_bytecode, p_buffer, "Bytecode");
	if (discontinuity == -1) {
		return OK;
	}

	Ref<GDScriptDecomp> decomp = create_decomp_for_commit(get_bytecode_rev());

	Vector<StringName> newidentifiers;
	Vector<Variant> newconstants;
	VMap<uint32_t, uint32_t> newlines;
	Vector<uint32_t> newtokens;
	new_err = get_ids_consts_tokens(recompiled_bytecode, bytecode_version, newidentifiers, newconstants, newtokens, newlines);
	if (new_err != OK) {
		WARN_PRINT("Recompiled bytecode failed to decompile: " + error_message);
		return OK;
	}
	discontinuity = continuity_tester<Vector<StringName>>(identifiers, newidentifiers, "Identifiers");
	if (discontinuity != -1 && discontinuity < identifiers.size() && newidentifiers.size()) {
		WARN_PRINT("Different StringNames: " + identifiers[discontinuity] + " != " + newidentifiers[discontinuity]);
	}
	discontinuity = continuity_tester<Vector<Variant>>(constants, newconstants, "Constants");
	if (discontinuity != -1 && discontinuity < constants.size() && newconstants.size()) {
		WARN_PRINT("Different Constants: " + constants[discontinuity].operator String() + " != " + newconstants[discontinuity].operator String());
	}
	auto old_tokens_size = tokens.size();
	auto new_tokens_size = newtokens.size();
	discontinuity = continuity_tester<Vector<uint32_t>>(tokens, newtokens, "Tokens");

	if (is_print_verbose_enabled() && discontinuity != -1 && discontinuity < new_tokens_size && discontinuity < old_tokens_size) {
		// go through and print the ALL the tokens, "oldtoken (val)  ==  newtoken  (val)"
		print_verbose("START TOKEN PRINT");
		for (int i = 0; i < old_tokens_size; i++) {
			auto old_token = tokens[i];
			auto new_token = newtokens[i];
			String old_token_name = GDScriptTokenizerTextCompat::get_token_name(get_global_token(old_token & TOKEN_MASK));
			String new_token_name = GDScriptTokenizerTextCompat::get_token_name(get_global_token(new_token & TOKEN_MASK));
			if (old_token_name != new_token_name) {
				print_verbose(String("Different Tokens: ") + old_token_name + String(" != ") + new_token_name);
			} else {
				int old_token_val = old_token >> TOKEN_BITS;
				int new_token_val = new_token >> TOKEN_BITS;
				if (old_token_val != new_token_val) {
					print_verbose(String("Different Token Val for ") + old_token_name + ":" + itos(old_token_val) + String(" != ") + itos(new_token_val));
				} else {
					print_verbose(String("Same Token Val for ") + old_token_name + ":" + itos(old_token_val) + String(" == ") + itos(new_token_val));
				}
			}
		}
		print_verbose("END TOKEN PRINT");
	}

	if (discontinuity != -1 && discontinuity < tokens.size() && discontinuity < newtokens.size()) {
		while (discontinuity < tokens.size() && discontinuity < newtokens.size() && discontinuity != -1) {
			auto old_token = tokens[discontinuity];
			auto new_token = newtokens[discontinuity];
			String old_token_name = GDScriptTokenizerTextCompat::get_token_name(get_global_token(old_token & TOKEN_MASK));
			String new_token_name = GDScriptTokenizerTextCompat::get_token_name(get_global_token(new_token & TOKEN_MASK));
			if (old_token_name != new_token_name) {
				WARN_PRINT(String("Different Tokens: ") + old_token_name + String(" != ") + new_token_name);
			} else {
				int old_token_val = old_token >> TOKEN_BITS;
				int new_token_val = new_token >> TOKEN_BITS;
				if (old_token_val != new_token_val) {
					WARN_PRINT(String("Different Token Val for ") + old_token_name + ":" + itos(old_token_val) + String(" != ") + itos(new_token_val));
				}
			}

			discontinuity = continuity_tester<Vector<uint32_t>>(tokens, newtokens, "Tokens", discontinuity + 1);
		}
	}
	auto lines_Size = lines.size();
	auto newlines_Size = newlines.size();
	discontinuity = continuity_tester<VMap<uint32_t, uint32_t>>(lines, newlines, "Lines");
	if (discontinuity != -1 && discontinuity < lines_Size && discontinuity < newlines_Size) {
		WARN_PRINT("Different Lines: " + itos(lines[discontinuity]) + " != " + itos(newlines[discontinuity]));
	}

	new_err = decomp->decompile_buffer(recompiled_bytecode);
	if (new_err != OK) {
		WARN_PRINT("Recompiled bytecode failed to decompile: " + error_message);
	} else if (script_text != decomp->script_text) {
		WARN_PRINT("Recompiled bytecode decompiled differently: " + decomp->script_text + " != " + script_text);
	} else {
		WARN_PRINT("Recompiled bytecode decompiled the same");
		return OK;
	}

#endif
	return OK;
}

GDScriptDecomp::BytecodeTestResult GDScriptDecomp::test_bytecode(Vector<uint8_t> p_buffer) {
	int p_token_max = 0;
	int p_func_max = 0;
	return _test_bytecode(p_buffer, p_token_max, p_func_max);
}

GDScriptDecomp::BytecodeTestResult GDScriptDecomp::_test_bytecode(Vector<uint8_t> p_buffer, int &r_tok_max, int &r_func_max) {
#define SIZE_CHECK(x)                                  \
	if (i + x >= tokens.size()) {                      \
		return BytecodeTestResult::BYTECODE_TEST_FAIL; \
	}

	//Load bytecode
	Vector<StringName> identifiers;
	Vector<Variant> constants;
	VMap<uint32_t, uint32_t> lines;
	Vector<uint32_t> tokens;
	int bytecode_version = get_bytecode_version();
	int FUNC_MAX = get_function_count();

	const uint8_t *buf = p_buffer.ptr();
	ERR_FAIL_COND_V(p_buffer.size() < 24 || p_buffer[0] != 'G' || p_buffer[1] != 'D' || p_buffer[2] != 'S' || p_buffer[3] != 'C', BYTECODE_TEST_CORRUPT);

	int version = decode_uint32(&buf[4]);
	ERR_FAIL_COND_V(version != bytecode_version, BYTECODE_TEST_CORRUPT);
	Error err = get_ids_consts_tokens(p_buffer, bytecode_version, identifiers, constants, tokens, lines);
	ERR_FAIL_COND_V(err != OK, BYTECODE_TEST_CORRUPT);

	for (int i = 0; i < tokens.size(); i++) {
		r_tok_max = MAX(r_tok_max, tokens[i] & TOKEN_MASK);
		GlobalToken curr_token = get_global_token(tokens[i] & TOKEN_MASK);
		Pair<int, int> arg_count;
		bool test_func = false;
		int func_id = -1;
		// All of these assumptions should apply for all bytecodes that we have support for
		switch (curr_token) {
			// Functions go like:
			// `func <literally_fucking_anything_resembling_an_identifier_including_keywords_and_built-in_funcs>(<arguments>)`
			case G_TK_PR_FUNCTION: {
				SIZE_CHECK(2);
				// ignore the next_token because it can be anything
				// get the one after
				GlobalToken nextnext_token = get_global_token(tokens[i + 2] & TOKEN_MASK);
				if (nextnext_token != G_TK_PARENTHESIS_OPEN) {
					return BytecodeTestResult::BYTECODE_TEST_FAIL;
				}
			} break;
			case G_TK_CF_PASS: {
				// next token has to be EOF, semicolon, or newline
				SIZE_CHECK(1);

				GlobalToken next_token = get_global_token(tokens[i + 1] & TOKEN_MASK);
				if (next_token != G_TK_NEWLINE && next_token != G_TK_SEMICOLON && next_token != G_TK_EOF) {
					return BytecodeTestResult::BYTECODE_TEST_FAIL;
				}
			} break;
			case G_TK_PR_STATIC: {
				SIZE_CHECK(1);
				// STATIC requires TK_PR_FUNCTION as the next token
				GlobalToken next_token = get_global_token(tokens[i + 1] & TOKEN_MASK);
				if (next_token != G_TK_PR_FUNCTION) {
					return BytecodeTestResult::BYTECODE_TEST_FAIL;
				}
			} break;
			case G_TK_PR_ENUM: { // not added until 2.1.3, but valid for all versions after
				SIZE_CHECK(1);
				// ENUM requires TK_IDENTIFIER or TK_CURLY_BRACKET_OPEN as the next token
				GlobalToken next_token = get_global_token(tokens[i + 1] & TOKEN_MASK);
				if (next_token != G_TK_IDENTIFIER && next_token != G_TK_CURLY_BRACKET_OPEN) {
					return BytecodeTestResult::BYTECODE_TEST_FAIL;
				}
			} break;
			case G_TK_BUILT_IN_FUNC: {
				func_id = tokens[i] >> TOKEN_BITS;
				r_func_max = MAX(r_func_max, func_id);
				if (func_id >= FUNC_MAX) {
					return BytecodeTestResult::BYTECODE_TEST_FAIL;
				}
				arg_count = get_function_arg_count(func_id);
				test_func = true;
			} break;
			case G_TK_PR_PRELOAD: // Preload is like a function with 1 argument
				arg_count = { 1, 1 };
				test_func = true;
				break;
			case G_TK_ERROR: // none of these should have made it into the bytecode
			case G_TK_CURSOR:
			case G_TK_MAX:
				return BytecodeTestResult::BYTECODE_TEST_FAIL;
			case G_TK_EOF: {
				if (tokens.size() != i + 1) {
					return BytecodeTestResult::BYTECODE_TEST_FAIL;
				}
			} break;
			default:
				break;
		}
		if (test_func) { // we're at the function identifier, so check the argument count
			SIZE_CHECK(2);
			if (i + 2 >= tokens.size()) { // should at least have two more tokens for `()` after the function identifier
				return BytecodeTestResult::BYTECODE_TEST_FAIL;
			}
			if (get_global_token(tokens[i + 1] & TOKEN_MASK) != G_TK_PARENTHESIS_OPEN) {
				return BytecodeTestResult::BYTECODE_TEST_FAIL;
			}
			if (!test_built_in_func_arg_count(tokens, arg_count, i + 2)) {
				return BytecodeTestResult::BYTECODE_TEST_FAIL;
			};
		}
	}

	return BYTECODE_TEST_UNKNOWN;
#undef SIZE_CHECK
}

// we should be just past the open parenthesis
bool GDScriptDecomp::test_built_in_func_arg_count(const Vector<uint32_t> &tokens, Pair<int, int> arg_count, int curr_pos) {
	int pos = curr_pos;
	int comma_count = 0;
	int min_args = arg_count.first;
	int max_args = arg_count.second;
	GlobalToken t = get_global_token(tokens[pos] & TOKEN_MASK);
	int bracket_open = 0;

	if (min_args == 0 && max_args == 0) {
		for (; pos < tokens.size(); pos++, t = get_global_token(tokens[pos] & TOKEN_MASK)) {
			if (t == G_TK_PARENTHESIS_CLOSE) {
				break;
			} else if (t != G_TK_NEWLINE) {
				return false;
			}
		}
		// if we didn't find a close parenthesis, then we have an error
		if (pos == tokens.size()) {
			return false;
		}
		return true;
	}
	// count the commas
	// at least in 3.x and below, the only time commas are allowed in function args are other expressions
	// this is not the case for GDScript 2.0 (4.x), due to lambdas, but that doesn't have a compiled version yet
	for (; pos < tokens.size(); pos++, t = get_global_token(tokens[pos] & TOKEN_MASK)) {
		switch (t) {
			case G_TK_BRACKET_OPEN:
			case G_TK_CURLY_BRACKET_OPEN:
			case G_TK_PARENTHESIS_OPEN:
				bracket_open++;
				break;
			case G_TK_BRACKET_CLOSE:
			case G_TK_CURLY_BRACKET_CLOSE:
			case G_TK_PARENTHESIS_CLOSE:
				bracket_open--;
				break;
			case G_TK_COMMA:
				if (bracket_open == 0) {
					comma_count++;
				}
				break;
			default:
				break;
		}
		if (bracket_open == -1) {
			break;
		}
	}
	// trailing commas are not allowed after the last argument
	if (pos == tokens.size() || t != G_TK_PARENTHESIS_CLOSE || comma_count < min_args - 1 || comma_count > max_args - 1) {
		return false;
	}
	return true;
}

Vector<uint8_t> GDScriptDecomp::compile_code_string(const String &p_code) {
	Vector<uint8_t> buf;

	RBMap<StringName, int> identifier_map;
	HashMap<Variant, int, VariantHasher, VariantComparator> constant_map;
	RBMap<uint32_t, int> line_map;
	Vector<uint32_t> token_array;

	// compat: from 3.0 - 3.1.1, the tokenizer defaulted to storing full objects
	// e61a074, Mar 28, 2019
	Ref<GodotVer> NO_FULL_OBJ_VER = GodotVer::create(3, 2, 0, "dev1");
	Ref<GodotVer> godot_ver = GodotVer::parse(get_engine_version());
	bool encode_full_objects = godot_ver->lt(NO_FULL_OBJ_VER);
	GDScriptTokenizerTextCompat tt(this);
	tt.set_code(p_code);
	int line = -1;

	while (true) {
		if (tt.get_token_line() != line) {
			line = tt.get_token_line();
			line_map[line] = token_array.size();
		}
		const GlobalToken g_token = tt.get_token();
		uint32_t local_token = get_local_token_val(g_token);
		switch (g_token) {
			case G_TK_IDENTIFIER: {
				StringName id = tt.get_token_identifier();
				if (!identifier_map.has(id)) {
					int idx = identifier_map.size();
					identifier_map[id] = idx;
				}
				local_token |= identifier_map[id] << TOKEN_BITS;
			} break;
			case G_TK_CONSTANT: {
				const Variant &c = tt.get_token_constant();
				if (!constant_map.has(c)) {
					int idx = constant_map.size();
					constant_map[c] = idx;
				}
				local_token |= constant_map[c] << TOKEN_BITS;
			} break;
			case G_TK_BUILT_IN_TYPE: {
				Variant::Type type = tt.get_token_type();
				int local_type = VariantDecoderCompat::convert_variant_type_to_old(type, get_variant_ver_major());
				local_token |= local_type << TOKEN_BITS;
			} break;
			case G_TK_BUILT_IN_FUNC: {
				// built-in function already has correct value
				local_token |= tt.get_token_built_in_func() << TOKEN_BITS;
			} break;
			case G_TK_NEWLINE: {
				local_token |= tt.get_token_line_indent() << TOKEN_BITS;
			} break;
			case G_TK_ERROR: {
				ERR_FAIL_V(Vector<uint8_t>());
			} break;
			default: {
			}
		};

		token_array.push_back(local_token);

		if (tt.get_token() == G_TK_EOF) {
			break;
		}
		tt.advance();
	}

	//reverse maps

	RBMap<int, StringName> rev_identifier_map;
	for (RBMap<StringName, int>::Element *E = identifier_map.front(); E; E = E->next()) {
		rev_identifier_map[E->get()] = E->key();
	}

	RBMap<int, Variant> rev_constant_map;
	for (auto K : constant_map) {
		rev_constant_map[K.value] = K.key;
	}

	RBMap<int, uint32_t> rev_line_map;
	for (RBMap<uint32_t, int>::Element *E = line_map.front(); E; E = E->next()) {
		rev_line_map[E->get()] = E->key();
	}

	//save header
	buf.resize(24);
	buf.write[0] = 'G';
	buf.write[1] = 'D';
	buf.write[2] = 'S';
	buf.write[3] = 'C';
	encode_uint32(get_bytecode_version(), &buf.write[4]);
	encode_uint32(identifier_map.size(), &buf.write[8]);
	encode_uint32(constant_map.size(), &buf.write[12]);
	encode_uint32(line_map.size(), &buf.write[16]);
	encode_uint32(token_array.size(), &buf.write[20]);

	//save identifiers

	for (RBMap<int, StringName>::Element *E = rev_identifier_map.front(); E; E = E->next()) {
		CharString cs = String(E->get()).utf8();
		int len = cs.length() + 1;
		int extra = 4 - (len % 4);
		if (extra == 4) {
			extra = 0;
		}

		uint8_t ibuf[4];
		encode_uint32(len + extra, ibuf);
		for (int i = 0; i < 4; i++) {
			buf.push_back(ibuf[i]);
		}
		for (int i = 0; i < len; i++) {
			buf.push_back(cs[i] ^ 0xb6);
		}
		for (int i = 0; i < extra; i++) {
			buf.push_back(0 ^ 0xb6);
		}
	}

	for (RBMap<int, Variant>::Element *E = rev_constant_map.front(); E; E = E->next()) {
		int len;
		Error err = VariantDecoderCompat::encode_variant_compat(get_variant_ver_major(), E->get(), nullptr, len, encode_full_objects);
		ERR_FAIL_COND_V_MSG(err != OK, Vector<uint8_t>(), "Error when trying to encode Variant.");
		int pos = buf.size();
		buf.resize(pos + len);
		VariantDecoderCompat::encode_variant_compat(get_variant_ver_major(), E->get(), &buf.write[pos], len, encode_full_objects);
	}

	for (RBMap<int, uint32_t>::Element *E = rev_line_map.front(); E; E = E->next()) {
		uint8_t ibuf[8];
		encode_uint32(E->key(), &ibuf[0]);
		encode_uint32(E->get(), &ibuf[4]);
		for (int i = 0; i < 8; i++) {
			buf.push_back(ibuf[i]);
		}
	}

	for (int i = 0; i < token_array.size(); i++) {
		uint32_t token = token_array[i];

		if (token & ~TOKEN_MASK) {
			uint8_t buf4[4];
			encode_uint32(token_array[i] | TOKEN_BYTE_MASK, &buf4[0]);
			for (int j = 0; j < 4; j++) {
				buf.push_back(buf4[j]);
			}
		} else {
			buf.push_back(token);
		}
	}

	return buf;
}
