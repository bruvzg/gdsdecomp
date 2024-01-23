/*************************************************************************/
/*  bytecode_base.h                                                      */
/*************************************************************************/
#pragma once

#include "compat/variant_decoder_compat.h"

#include "core/object/class_db.h"
#include "core/object/object.h"
#include "core/object/ref_counted.h"
#include "core/templates/rb_map.h"
#include "core/templates/vmap.h"

class GDScriptDecomp : public RefCounted {
	GDCLASS(GDScriptDecomp, RefCounted);

protected:
	static void _bind_methods();
	static void _ensure_space(String &p_code);

	String script_text;
	String error_message;

	Pair<int, int> get_arg_count_for_builtin(String builtin_func_name);
	bool test_built_in_func_arg_count(const Vector<uint32_t> &tokens, Pair<int, int> arg_count, int curr_pos);

public:
	enum GlobalToken {
		G_TK_EMPTY,
		G_TK_IDENTIFIER,
		G_TK_CONSTANT,
		G_TK_SELF,
		G_TK_BUILT_IN_TYPE,
		G_TK_BUILT_IN_FUNC,
		G_TK_OP_IN,
		G_TK_OP_EQUAL,
		G_TK_OP_NOT_EQUAL,
		G_TK_OP_LESS,
		G_TK_OP_LESS_EQUAL,
		G_TK_OP_GREATER,
		G_TK_OP_GREATER_EQUAL,
		G_TK_OP_AND,
		G_TK_OP_OR,
		G_TK_OP_NOT,
		G_TK_OP_ADD,
		G_TK_OP_SUB,
		G_TK_OP_MUL,
		G_TK_OP_DIV,
		G_TK_OP_MOD,
		G_TK_OP_SHIFT_LEFT,
		G_TK_OP_SHIFT_RIGHT,
		G_TK_OP_ASSIGN,
		G_TK_OP_ASSIGN_ADD,
		G_TK_OP_ASSIGN_SUB,
		G_TK_OP_ASSIGN_MUL,
		G_TK_OP_ASSIGN_DIV,
		G_TK_OP_ASSIGN_MOD,
		G_TK_OP_ASSIGN_SHIFT_LEFT,
		G_TK_OP_ASSIGN_SHIFT_RIGHT,
		G_TK_OP_ASSIGN_BIT_AND,
		G_TK_OP_ASSIGN_BIT_OR,
		G_TK_OP_ASSIGN_BIT_XOR,
		G_TK_OP_BIT_AND,
		G_TK_OP_BIT_OR,
		G_TK_OP_BIT_XOR,
		G_TK_OP_BIT_INVERT,
		G_TK_CF_IF,
		G_TK_CF_ELIF,
		G_TK_CF_ELSE,
		G_TK_CF_FOR,
		G_TK_CF_WHILE,
		G_TK_CF_BREAK,
		G_TK_CF_CONTINUE,
		G_TK_CF_PASS,
		G_TK_CF_RETURN,
		G_TK_CF_MATCH,
		G_TK_PR_FUNCTION,
		G_TK_PR_CLASS,
		G_TK_PR_CLASS_NAME,
		G_TK_PR_EXTENDS,
		G_TK_PR_IS,
		G_TK_PR_ONREADY,
		G_TK_PR_TOOL,
		G_TK_PR_STATIC,
		G_TK_PR_EXPORT,
		G_TK_PR_SETGET,
		G_TK_PR_CONST,
		G_TK_PR_VAR,
		G_TK_PR_AS,
		G_TK_PR_VOID,
		G_TK_PR_ENUM,
		G_TK_PR_PRELOAD,
		G_TK_PR_ASSERT,
		G_TK_PR_YIELD,
		G_TK_PR_SIGNAL,
		G_TK_PR_BREAKPOINT,
		G_TK_PR_REMOTE,
		G_TK_PR_SYNC,
		G_TK_PR_MASTER,
		G_TK_PR_SLAVE,
		G_TK_PR_PUPPET,
		G_TK_PR_REMOTESYNC,
		G_TK_PR_MASTERSYNC,
		G_TK_PR_PUPPETSYNC,
		G_TK_BRACKET_OPEN,
		G_TK_BRACKET_CLOSE,
		G_TK_CURLY_BRACKET_OPEN,
		G_TK_CURLY_BRACKET_CLOSE,
		G_TK_PARENTHESIS_OPEN,
		G_TK_PARENTHESIS_CLOSE,
		G_TK_COMMA,
		G_TK_SEMICOLON,
		G_TK_PERIOD,
		G_TK_QUESTION_MARK,
		G_TK_COLON,
		G_TK_DOLLAR,
		G_TK_FORWARD_ARROW,
		G_TK_NEWLINE,
		G_TK_CONST_PI,
		G_TK_CONST_TAU,
		G_TK_WILDCARD,
		G_TK_CONST_INF,
		G_TK_CONST_NAN,
		G_TK_ERROR,
		G_TK_EOF,
		G_TK_CURSOR,
		G_TK_PR_SLAVESYNC, //renamed to puppet sync in most recent versions
		G_TK_CF_DO, // removed in 3.1
		G_TK_CF_CASE,
		G_TK_CF_SWITCH,
		G_TK_MAX,
	};
	enum BytecodeTestResult {
		BYTECODE_TEST_PASS,
		BYTECODE_TEST_FAIL,
		BYTECODE_TEST_CORRUPT,
		BYTECODE_TEST_UNKNOWN,
	};
	enum {
		TOKEN_BYTE_MASK = 0x80,
		TOKEN_BITS = 8,
		TOKEN_MASK = (1 << TOKEN_BITS) - 1,
		TOKEN_LINE_BITS = 24,
		TOKEN_LINE_MASK = (1 << TOKEN_LINE_BITS) - 1,
	};

protected:
	virtual Vector<GlobalToken> get_added_tokens() const { return {}; }
	virtual Vector<GlobalToken> get_removed_tokens() const { return {}; }
	virtual Vector<String> get_added_functions() const { return {}; }
	virtual Vector<String> get_removed_functions() const { return {}; }
	virtual Vector<String> get_function_arg_count_changed() const { return {}; }

public:
	virtual Error decompile_buffer(Vector<uint8_t> p_buffer);
	virtual BytecodeTestResult _test_bytecode(Vector<uint8_t> p_buffer, int &p_token_max, int &p_func_max);
	BytecodeTestResult test_bytecode(Vector<uint8_t> p_buffer);

	virtual String get_function_name(int p_func) const = 0;
	virtual int get_function_count() const = 0;
	virtual Pair<int, int> get_function_arg_count(int p_func) const = 0;
	virtual int get_token_max() const = 0;
	virtual int get_function_index(const String &p_func) const = 0;
	virtual GDScriptDecomp::GlobalToken get_global_token(int p_token) const = 0;
	virtual int get_local_token_val(GDScriptDecomp::GlobalToken p_token) const = 0;
	virtual int get_bytecode_version() const = 0;
	virtual int get_bytecode_rev() const = 0;
	virtual int get_engine_ver_major() const = 0;
	virtual int get_variant_ver_major() const = 0;
	virtual int get_parent() const = 0;

	Error decompile_byte_code_encrypted(const String &p_path, Vector<uint8_t> p_key);
	Error decompile_byte_code(const String &p_path);
	static Ref<GDScriptDecomp> create_decomp_for_commit(uint64_t p_commit_hash);
	virtual String get_engine_version() const = 0;
	virtual String get_max_engine_version() const = 0;

	static int read_bytecode_version(const String &p_path);
	static int read_bytecode_version_encrypted(const String &p_path, int engine_ver_major, Vector<uint8_t> p_key);
	static Error get_buffer_encrypted(const String &p_path, int engine_ver_major, Vector<uint8_t> p_key, Vector<uint8_t> &r_buffer);
	String get_script_text();
	String get_error_message();
	String get_constant_string(Vector<Variant> &constants, uint32_t constId);
	Error get_ids_consts_tokens(const Vector<uint8_t> &p_buffer, int bytecode_version, Vector<StringName> &r_identifiers, Vector<Variant> &r_constants, Vector<uint32_t> &r_tokens, VMap<uint32_t, uint32_t> lines);
};

VARIANT_ENUM_CAST(GDScriptDecomp::BytecodeTestResult)