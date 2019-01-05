/*************************************************************************/
/*  gdscript_decomp.cpp                                                  */
/*************************************************************************/

#include "gdscript_decomp.h"

#include "core/engine.h"

#include "core/os/file_access.h"
#include "core/io/file_access_encrypted.h"

void GDScriptDeComp::_bind_methods() {

	ClassDB::bind_method(D_METHOD("load_byte_code", "path"), &GDScriptDeComp::load_byte_code);
	ClassDB::bind_method(D_METHOD("load_byte_code_encrypted", "path", "key"), &GDScriptDeComp::load_byte_code_encrypted);
}

String GDScriptDeComp::load_byte_code_encrypted(const String &p_path, Vector<uint8_t> p_key) {

	Vector<uint8_t> bytecode;

	FileAccess *fa = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V(!fa, String());

	FileAccessEncrypted *fae = memnew(FileAccessEncrypted);
	ERR_FAIL_COND_V(!fae, String());

	Error err = fae->open_and_parse(fa, p_key, FileAccessEncrypted::MODE_READ);

	if (err) {
		fa->close();
		memdelete(fa);
		memdelete(fae);

		ERR_FAIL_COND_V(err, String());
	}

	bytecode.resize(fae->get_len());
	fae->get_buffer(bytecode.ptrw(), bytecode.size());
	fae->close();
	memdelete(fae);

	return _parse(bytecode);
}

String GDScriptDeComp::load_byte_code(const String &p_path) {

	Vector<uint8_t> bytecode;

	bytecode = FileAccess::get_file_as_array(p_path);

	return _parse(bytecode);
}

String GDScriptDeComp::_parse(Vector<uint8_t> p_bytecode) {

	GDScriptTokenizerBuffer *tokenizer = memnew(GDScriptTokenizerBuffer);
	tokenizer->set_code_buffer(p_bytecode);

	String ret;
	String line;

	int indent = 0;
	while (true) {
		GDScriptTokenizer::Token token = tokenizer->get_token();
		switch (token) {
			case GDScriptTokenizer::TK_EMPTY: {
				//skip
			} break;
			case GDScriptTokenizer::TK_IDENTIFIER: {
				line += String(tokenizer->get_token_identifier()) + String("");
			} break;
			case GDScriptTokenizer::TK_CONSTANT: {
				line += tokenizer->get_token_constant().get_construct_string() + String("");
			} break;
			case GDScriptTokenizer::TK_SELF: {
				line += "self ";
			} break;
			case GDScriptTokenizer::TK_BUILT_IN_TYPE: {
				line += Variant::get_type_name(tokenizer->get_token_type()) + String("");
			} break;
			case GDScriptTokenizer::TK_BUILT_IN_FUNC: {
				line += GDScriptFunctions::get_func_name(tokenizer->get_token_built_in_func()) + String("");
			} break;
			case GDScriptTokenizer::TK_OP_IN: {
				line += " in ";
			} break;
			case GDScriptTokenizer::TK_OP_EQUAL: {
				line += "==";
			} break;
			case GDScriptTokenizer::TK_OP_NOT_EQUAL: {
				line += "!=";
			} break;
			case GDScriptTokenizer::TK_OP_LESS: {
				line += "<";
			} break;
			case GDScriptTokenizer::TK_OP_LESS_EQUAL: {
				line += "<=";
			} break;
			case GDScriptTokenizer::TK_OP_GREATER: {
				line += ">";
			} break;
			case GDScriptTokenizer::TK_OP_GREATER_EQUAL: {
				line += "<=";
			} break;
			case GDScriptTokenizer::TK_OP_AND: {
				line += " and ";
			} break;
			case GDScriptTokenizer::TK_OP_OR: {
				line += " or ";
			} break;
			case GDScriptTokenizer::TK_OP_NOT: {
				line += " not ";
			} break;
			case GDScriptTokenizer::TK_OP_ADD: {
				line += "+";
			} break;
			case GDScriptTokenizer::TK_OP_SUB: {
				line += "-";
			} break;
			case GDScriptTokenizer::TK_OP_MUL: {
				line += "*";
			} break;
			case GDScriptTokenizer::TK_OP_DIV: {
				line += "/";
			} break;
			case GDScriptTokenizer::TK_OP_MOD: {
				line += "%";
			} break;
			case GDScriptTokenizer::TK_OP_SHIFT_LEFT: {
				line += "<<";
			} break;
			case GDScriptTokenizer::TK_OP_SHIFT_RIGHT: {
				line += ">>";
			} break;
			case GDScriptTokenizer::TK_OP_ASSIGN: {
				line += "=";
			} break;
			case GDScriptTokenizer::TK_OP_ASSIGN_ADD: {
				line += "+= ";
			} break;
			case GDScriptTokenizer::TK_OP_ASSIGN_SUB: {
				line += "-=";
			} break;
			case GDScriptTokenizer::TK_OP_ASSIGN_MUL: {
				line += "*=";
			} break;
			case GDScriptTokenizer::TK_OP_ASSIGN_DIV: {
				line += "/=";
			} break;
			case GDScriptTokenizer::TK_OP_ASSIGN_MOD: {
				line += "%=";
			} break;
			case GDScriptTokenizer::TK_OP_ASSIGN_SHIFT_LEFT: {
				line += "<<=";
			} break;
			case GDScriptTokenizer::TK_OP_ASSIGN_SHIFT_RIGHT: {
				line += ">>=";
			} break;
			case GDScriptTokenizer::TK_OP_ASSIGN_BIT_AND: {
				line += "&=";
			} break;
			case GDScriptTokenizer::TK_OP_ASSIGN_BIT_OR: {
				line += "|=";
			} break;
			case GDScriptTokenizer::TK_OP_ASSIGN_BIT_XOR: {
				line += "^=";
			} break;
			case GDScriptTokenizer::TK_OP_BIT_AND: {
				line += "&";
			} break;
			case GDScriptTokenizer::TK_OP_BIT_OR: {
				line += "|";
			} break;
			case GDScriptTokenizer::TK_OP_BIT_XOR: {
				line += "^";
			} break;
			case GDScriptTokenizer::TK_OP_BIT_INVERT: {
				line += "!";
			} break;
			//case GDScriptTokenizer::TK_OP_PLUS_PLUS: {
			//	line += "++";
			//} break;
			//case GDScriptTokenizer::TK_OP_MINUS_MINUS: {
			//	line += "--";
			//} break;
			case GDScriptTokenizer::TK_CF_IF: {
				line += "if ";
			} break;
			case GDScriptTokenizer::TK_CF_ELIF: {
				line += "elif ";
			} break;
			case GDScriptTokenizer::TK_CF_ELSE: {
				line += "else ";
			} break;
			case GDScriptTokenizer::TK_CF_FOR: {
				line += "for ";
			} break;
			case GDScriptTokenizer::TK_CF_DO: {
				line += "do ";
			} break;
			case GDScriptTokenizer::TK_CF_WHILE: {
				line += "while ";
			} break;
			case GDScriptTokenizer::TK_CF_SWITCH: {
				line += "swith ";
			} break;
			case GDScriptTokenizer::TK_CF_CASE: {
				line += "case ";
			} break;
			case GDScriptTokenizer::TK_CF_BREAK: {
				line += "break";
			} break;
			case GDScriptTokenizer::TK_CF_CONTINUE: {
				line += "continue";
			} break;
			case GDScriptTokenizer::TK_CF_PASS: {
				line += "pass";
			} break;
			case GDScriptTokenizer::TK_CF_RETURN: {
				line += "return ";
			} break;
			case GDScriptTokenizer::TK_CF_MATCH: {
				line += "match ";
			} break;
			case GDScriptTokenizer::TK_PR_FUNCTION: {
				line += "func ";
			} break;
			case GDScriptTokenizer::TK_PR_CLASS: {
				line += "class ";
			} break;
			case GDScriptTokenizer::TK_PR_CLASS_NAME: {
				line += "class_name ";
			} break;
			case GDScriptTokenizer::TK_PR_EXTENDS: {
				line += "extends ";
			} break;
			case GDScriptTokenizer::TK_PR_IS: {
				line += "is ";
			} break;
			case GDScriptTokenizer::TK_PR_ONREADY: {
				line += "onready ";
			} break;
			case GDScriptTokenizer::TK_PR_TOOL: {
				line += "tool ";
			} break;
			case GDScriptTokenizer::TK_PR_STATIC: {
				line += "static ";
			} break;
			case GDScriptTokenizer::TK_PR_EXPORT: {
				line += "export ";
			} break;
			case GDScriptTokenizer::TK_PR_SETGET: {
				line += "setget ";
			} break;
			case GDScriptTokenizer::TK_PR_CONST: {
				line += "const ";
			} break;
			case GDScriptTokenizer::TK_PR_VAR: {
				line += "var ";
			} break;
			case GDScriptTokenizer::TK_PR_AS: {
				line += "as ";
			} break;
			case GDScriptTokenizer::TK_PR_VOID: {
				line += "void ";
			} break;
			case GDScriptTokenizer::TK_PR_ENUM: {
				line += "enum ";
			} break;
			case GDScriptTokenizer::TK_PR_PRELOAD: {
				line += "preload";
			} break;
			case GDScriptTokenizer::TK_PR_ASSERT: {
				line += "assert ";
			} break;
			case GDScriptTokenizer::TK_PR_YIELD: {
				line += "yield ";
			} break;
			case GDScriptTokenizer::TK_PR_SIGNAL: {
				line += "signal ";
			} break;
			case GDScriptTokenizer::TK_PR_BREAKPOINT: {
				line += "breakpoint ";
			} break;
			case GDScriptTokenizer::TK_PR_REMOTE: {
				line += "remote ";
			} break;
			case GDScriptTokenizer::TK_PR_SYNC: {
				line += "sync ";
			} break;
			case GDScriptTokenizer::TK_PR_MASTER: {
				line += "master ";
			} break;
			case GDScriptTokenizer::TK_PR_SLAVE: {
				line += "slave ";
			} break;
			case GDScriptTokenizer::TK_PR_PUPPET: {
				line += "puppet ";
			} break;
			case GDScriptTokenizer::TK_PR_REMOTESYNC: {
				line += "remotesync ";
			} break;
			case GDScriptTokenizer::TK_PR_MASTERSYNC: {
				line += "mastersync ";
			} break;
			case GDScriptTokenizer::TK_PR_PUPPETSYNC: {
				line += "puppetsync";
			} break;
			case GDScriptTokenizer::TK_BRACKET_OPEN: {
				line += "[";
			} break;
			case GDScriptTokenizer::TK_BRACKET_CLOSE: {
				line += "]";
			} break;
			case GDScriptTokenizer::TK_CURLY_BRACKET_OPEN: {
				line += "{";
			} break;
			case GDScriptTokenizer::TK_CURLY_BRACKET_CLOSE: {
				line += "}";
			} break;
			case GDScriptTokenizer::TK_PARENTHESIS_OPEN: {
				line += "(";
			} break;
			case GDScriptTokenizer::TK_PARENTHESIS_CLOSE: {
				line += ")";
			} break;
			case GDScriptTokenizer::TK_COMMA: {
				line += ",";
			} break;
			case GDScriptTokenizer::TK_SEMICOLON: {
				line += ";";
			} break;
			case GDScriptTokenizer::TK_PERIOD: {
				line += ".";
			} break;
			case GDScriptTokenizer::TK_QUESTION_MARK: {
				line += "?";
			} break;
			case GDScriptTokenizer::TK_COLON: {
				line += ":";
			} break;
			case GDScriptTokenizer::TK_DOLLAR: {
				line += "$";
			} break;
			case GDScriptTokenizer::TK_FORWARD_ARROW: {
				line += "->";
			} break;
			case GDScriptTokenizer::TK_NEWLINE: {
				for (int i = 0; i < indent; i++) {
					ret += "    ";
				}
				ret += line + "\n";
				line = String();
				indent = tokenizer->get_token_line_indent();
			} break;
			case GDScriptTokenizer::TK_CONST_PI: {
				line += "PI";
			} break;
			case GDScriptTokenizer::TK_CONST_TAU: {
				line += "TAU";
			} break;
			case GDScriptTokenizer::TK_WILDCARD: {
				line += "_";
			} break;
			case GDScriptTokenizer::TK_CONST_INF: {
				line += "INF";
			} break;
			case GDScriptTokenizer::TK_CONST_NAN: {
				line += "NAN";
			} break;
			case GDScriptTokenizer::TK_ERROR: {
				memdelete(tokenizer);
				return ret;
			} break;
			case GDScriptTokenizer::TK_EOF: {
				memdelete(tokenizer);
				return ret;
			} break;
			default: {
				//invalid token
			} break;
		}
		tokenizer->advance();
	}
	memdelete(tokenizer);
	return ret;
}