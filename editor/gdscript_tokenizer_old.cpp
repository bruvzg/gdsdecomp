/**************************************************************************/
/*  gdscript_tokenizer.cpp                                                */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/**************************************************************************/

#include "gdscript_tokenizer_old.h"
#include "map.h"
#include "core/io/marshalls.h"

#define FALLTHROUGH [[fallthrough]]
#define UPPERCASE(m_c) (((m_c) >= 'a' && (m_c) <= 'z') ? ((m_c) - ('a' - 'A')) : (m_c))
#define LOWERCASE(m_c) (((m_c) >= 'A' && (m_c) <= 'Z') ? ((m_c) + ('a' - 'A')) : (m_c))
#define IS_DIGIT(m_d) ((m_d) >= '0' && (m_d) <= '9')
#define IS_HEX_DIGIT(m_d) (((m_d) >= '0' && (m_d) <= '9') || ((m_d) >= 'a' && (m_d) <= 'f') || ((m_d) >= 'A' && (m_d) <= 'F'))

const char *GDScriptTokenizerOld::get_func_name(Function p_func) const {
	ERR_FAIL_INDEX_V(p_func, FUNC_MAX, "");

	static const char *_names[FUNC_MAX] = {
		"sin",
		"cos",
		"tan",
		"sinh",
		"cosh",
		"tanh",
		"asin",
		"acos",
		"atan",
		"atan2",
		"sqrt",
		"fmod",
		"fposmod",
		"posmod",
		"floor",
		"ceil",
		"round",
		"abs",
		"sign",
		"pow",
		"log",
		"exp",
		"is_nan",
		"is_inf",
		"is_equal_approx",
		"is_zero_approx",
		"ease",
		"decimals",
		"step_decimals",
		"stepify",
		"lerp",
		"lerp_angle",
		"inverse_lerp",
		"range_lerp",
		"smoothstep",
		"move_toward",
		"dectime",
		"randomize",
		"randi",
		"randf",
		"rand_range",
		"seed",
		"rand_seed",
		"deg2rad",
		"rad2deg",
		"linear2db",
		"db2linear",
		"polar2cartesian",
		"cartesian2polar",
		"wrapi",
		"wrapf",
		"max",
		"min",
		"clamp",
		"nearest_po2",
		"weakref",
		"funcref",
		"convert",
		"typeof",
		"type_exists",
		"char",
		"ord",
		"str",
		"print",
		"printt",
		"prints",
		"printerr",
		"printraw",
		"print_debug",
		"push_error",
		"push_warning",
		"var2str",
		"str2var",
		"var2bytes",
		"bytes2var",
		"range",
		"load",
		"inst2dict",
		"dict2inst",
		"validate_json",
		"parse_json",
		"to_json",
		"hash",
		"Color8",
		"ColorN",
		"print_stack",
		"get_stack",
		"instance_from_id",
		"len",
		"is_instance_valid",
		"deep_equal",
	};

	return _names[p_func];
}

const char *GDScriptTokenizerOld::token_names[TK_MAX] = {
	"Empty",
	"Identifier",
	"Constant",
	"Self",
	"Built-In Type",
	"Built-In Func",
	"In",
	"'=='",
	"'!='",
	"'<'",
	"'<='",
	"'>'",
	"'>='",
	"'and'",
	"'or'",
	"'not'",
	"'+'",
	"'-'",
	"'*'",
	"'/'",
	"'%'",
	"'<<'",
	"'>>'",
	"'='",
	"'+='",
	"'-='",
	"'*='",
	"'/='",
	"'%='",
	"'<<='",
	"'>>='",
	"'&='",
	"'|='",
	"'^='",
	"'&'",
	"'|'",
	"'^'",
	"'~'",
	//"Plus Plus",
	//"Minus Minus",
	"if",
	"elif",
	"else",
	"for",
	"while",
	"break",
	"continue",
	"pass",
	"return",
	"match",
	"func",
	"class",
	"class_name",
	"extends",
	"is",
	"onready",
	"tool",
	"static",
	"export",
	"setget",
	"const",
	"var",
	"as",
	"void",
	"enum",
	"preload",
	"assert",
	"yield",
	"signal",
	"breakpoint",
	"rpc",
	"sync",
	"master",
	"puppet",
	"slave",
	"remotesync",
	"mastersync",
	"puppetsync",
	"'['",
	"']'",
	"'{'",
	"'}'",
	"'('",
	"')'",
	"','",
	"';'",
	"'.'",
	"'?'",
	"':'",
	"'$'",
	"'->'",
	"'\\n'",
	"PI",
	"TAU",
	"_",
	"INF",
	"NAN",
	"Error",
	"EOF",
	"Cursor"
};

struct _bit {
	Variant::Type type;
	const char *text;
};
//built in types

static const _bit _type_list[] = {
	//types
	{ Variant::BOOL, "bool" },
	{ Variant::INT, "int" },
	{ Variant::FLOAT, "float" },
	{ Variant::STRING, "String" },
	{ Variant::VECTOR2, "Vector2" },
	{ Variant::RECT2, "Rect2" },
	{ Variant::TRANSFORM2D, "Transform2D" },
	{ Variant::VECTOR3, "Vector3" },
	{ Variant::AABB, "AABB" },
	{ Variant::PLANE, "Plane" },
	{ Variant::QUATERNION, "Quat" },
	{ Variant::BASIS, "Basis" },
	{ Variant::TRANSFORM3D, "Transform" },
	{ Variant::COLOR, "Color" },
	{ Variant::RID, "RID" },
	{ Variant::OBJECT, "Object" },
	{ Variant::NODE_PATH, "NodePath" },
	{ Variant::DICTIONARY, "Dictionary" },
	{ Variant::ARRAY, "Array" },
	{ Variant::PACKED_BYTE_ARRAY, "PoolByteArray" },
	{ Variant::PACKED_INT64_ARRAY, "PoolIntArray" },
	{ Variant::PACKED_FLOAT64_ARRAY, "PoolRealArray" },
	{ Variant::PACKED_STRING_ARRAY, "PoolStringArray" },
	{ Variant::PACKED_VECTOR2_ARRAY, "PoolVector2Array" },
	{ Variant::PACKED_VECTOR3_ARRAY, "PoolVector3Array" },
	{ Variant::PACKED_COLOR_ARRAY, "PoolColorArray" },
	{ Variant::VARIANT_MAX, nullptr },
};

struct _kws {
	GDScriptTokenizerOld::Token token;
	const char *text;
};

static const _kws _keyword_list[] = {
	//ops
	{ GDScriptTokenizerOld::TK_OP_IN, "in" },
	{ GDScriptTokenizerOld::TK_OP_NOT, "not" },
	{ GDScriptTokenizerOld::TK_OP_OR, "or" },
	{ GDScriptTokenizerOld::TK_OP_AND, "and" },
	//func
	{ GDScriptTokenizerOld::TK_PR_FUNCTION, "func" },
	{ GDScriptTokenizerOld::TK_PR_CLASS, "class" },
	{ GDScriptTokenizerOld::TK_PR_CLASS_NAME, "class_name" },
	{ GDScriptTokenizerOld::TK_PR_EXTENDS, "extends" },
	{ GDScriptTokenizerOld::TK_PR_IS, "is" },
	{ GDScriptTokenizerOld::TK_PR_ONREADY, "onready" },
	{ GDScriptTokenizerOld::TK_PR_TOOL, "tool" },
	{ GDScriptTokenizerOld::TK_PR_STATIC, "static" },
	{ GDScriptTokenizerOld::TK_PR_EXPORT, "export" },
	{ GDScriptTokenizerOld::TK_PR_SETGET, "setget" },
	{ GDScriptTokenizerOld::TK_PR_VAR, "var" },
	{ GDScriptTokenizerOld::TK_PR_AS, "as" },
	{ GDScriptTokenizerOld::TK_PR_VOID, "void" },
	{ GDScriptTokenizerOld::TK_PR_PRELOAD, "preload" },
	{ GDScriptTokenizerOld::TK_PR_ASSERT, "assert" },
	{ GDScriptTokenizerOld::TK_PR_YIELD, "yield" },
	{ GDScriptTokenizerOld::TK_PR_SIGNAL, "signal" },
	{ GDScriptTokenizerOld::TK_PR_BREAKPOINT, "breakpoint" },
	{ GDScriptTokenizerOld::TK_PR_REMOTE, "remote" },
	{ GDScriptTokenizerOld::TK_PR_MASTER, "master" },
	{ GDScriptTokenizerOld::TK_PR_SLAVE, "slave" },
	{ GDScriptTokenizerOld::TK_PR_PUPPET, "puppet" },
	{ GDScriptTokenizerOld::TK_PR_SYNC, "sync" },
	{ GDScriptTokenizerOld::TK_PR_REMOTESYNC, "remotesync" },
	{ GDScriptTokenizerOld::TK_PR_MASTERSYNC, "mastersync" },
	{ GDScriptTokenizerOld::TK_PR_PUPPETSYNC, "puppetsync" },
	{ GDScriptTokenizerOld::TK_PR_CONST, "const" },
	{ GDScriptTokenizerOld::TK_PR_ENUM, "enum" },
	//controlflow
	{ GDScriptTokenizerOld::TK_CF_IF, "if" },
	{ GDScriptTokenizerOld::TK_CF_ELIF, "elif" },
	{ GDScriptTokenizerOld::TK_CF_ELSE, "else" },
	{ GDScriptTokenizerOld::TK_CF_FOR, "for" },
	{ GDScriptTokenizerOld::TK_CF_WHILE, "while" },
	{ GDScriptTokenizerOld::TK_CF_BREAK, "break" },
	{ GDScriptTokenizerOld::TK_CF_CONTINUE, "continue" },
	{ GDScriptTokenizerOld::TK_CF_RETURN, "return" },
	{ GDScriptTokenizerOld::TK_CF_MATCH, "match" },
	{ GDScriptTokenizerOld::TK_CF_PASS, "pass" },
	{ GDScriptTokenizerOld::TK_SELF, "self" },
	{ GDScriptTokenizerOld::TK_CONST_PI, "PI" },
	{ GDScriptTokenizerOld::TK_CONST_TAU, "TAU" },
	{ GDScriptTokenizerOld::TK_WILDCARD, "_" },
	{ GDScriptTokenizerOld::TK_CONST_INF, "INF" },
	{ GDScriptTokenizerOld::TK_CONST_NAN, "NAN" },
	{ GDScriptTokenizerOld::TK_ERROR, nullptr }
};

const char *GDScriptTokenizerOld::get_token_name(Token p_token) {
	ERR_FAIL_INDEX_V(p_token, TK_MAX, "<error>");
	return token_names[p_token];
}

bool GDScriptTokenizerOld::is_token_literal(int p_offset, bool variable_safe) const {
	switch (get_token(p_offset)) {
		// Can always be literal:
		case TK_IDENTIFIER:

		case TK_PR_ONREADY:
		case TK_PR_TOOL:
		case TK_PR_STATIC:
		case TK_PR_EXPORT:
		case TK_PR_SETGET:
		case TK_PR_SIGNAL:
		case TK_PR_REMOTE:
		case TK_PR_MASTER:
		case TK_PR_PUPPET:
		case TK_PR_SYNC:
		case TK_PR_REMOTESYNC:
		case TK_PR_MASTERSYNC:
		case TK_PR_PUPPETSYNC:
			return true;

		// Literal for non-variables only:
		case TK_BUILT_IN_TYPE:
		case TK_BUILT_IN_FUNC:

		case TK_OP_IN:
			//case TK_OP_NOT:
			//case TK_OP_OR:
			//case TK_OP_AND:

		case TK_PR_CLASS:
		case TK_PR_CONST:
		case TK_PR_ENUM:
		case TK_PR_PRELOAD:
		case TK_PR_FUNCTION:
		case TK_PR_EXTENDS:
		case TK_PR_ASSERT:
		case TK_PR_YIELD:
		case TK_PR_VAR:

		case TK_CF_IF:
		case TK_CF_ELIF:
		case TK_CF_ELSE:
		case TK_CF_FOR:
		case TK_CF_WHILE:
		case TK_CF_BREAK:
		case TK_CF_CONTINUE:
		case TK_CF_RETURN:
		case TK_CF_MATCH:
		case TK_CF_PASS:
		case TK_SELF:
		case TK_CONST_PI:
		case TK_CONST_TAU:
		case TK_WILDCARD:
		case TK_CONST_INF:
		case TK_CONST_NAN:
		case TK_ERROR:
			return !variable_safe;

		case TK_CONSTANT: {
			switch (get_token_constant(p_offset).get_type()) {
				case Variant::NIL:
				case Variant::BOOL:
					return true;
				default:
					return false;
			}
		}
		default:
			return false;
	}
}

StringName GDScriptTokenizerOld::get_token_literal(int p_offset) const {
	Token token = get_token(p_offset);
	switch (token) {
		case TK_IDENTIFIER:
			return get_token_identifier(p_offset);
		case TK_BUILT_IN_TYPE: {
			Variant::Type type = get_token_type(p_offset);
			int idx = 0;

			while (_type_list[idx].text) {
				if (type == _type_list[idx].type) {
					return _type_list[idx].text;
				}
				idx++;
			}
		} break; // Shouldn't get here, stuff happens
		case TK_BUILT_IN_FUNC:
			return get_func_name(get_token_built_in_func(p_offset));
		case TK_CONSTANT: {
			const Variant value = get_token_constant(p_offset);

			switch (value.get_type()) {
				case Variant::NIL:
					return "null";
				case Variant::BOOL:
					return value ? "true" : "false";
				default: {
				}
			}
		}
		case TK_OP_AND:
		case TK_OP_OR:
			break; // Don't get into default, since they can be non-literal
		default: {
			int idx = 0;

			while (_keyword_list[idx].text) {
				if (token == _keyword_list[idx].token) {
					return _keyword_list[idx].text;
				}
				idx++;
			}
		}
	}
	ERR_FAIL_V_MSG("", "Failed to get token literal.");
}

static bool _is_text_char(CharType c) {
	return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_';
}

static bool _is_number(CharType c) {
	return (c >= '0' && c <= '9');
}

static bool _is_hex(CharType c) {
	return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static bool _is_bin(CharType c) {
	return (c == '0' || c == '1');
}

void GDScriptTokenizerText::_make_token(Token p_type) {
	TokenData &tk = tk_rb[tk_rb_pos];

	tk.type = p_type;
	tk.line = line;
	tk.col = column;

	tk_rb_pos = (tk_rb_pos + 1) % RB_SIZE;
}
void GDScriptTokenizerText::_make_identifier(const StringName &p_identifier) {
	TokenData &tk = tk_rb[tk_rb_pos];

	tk.type = TK_IDENTIFIER;
	tk.identifier = p_identifier;
	tk.line = line;
	tk.col = column;

	tk_rb_pos = (tk_rb_pos + 1) % RB_SIZE;
}

void GDScriptTokenizerText::_make_built_in_func(Function p_func) {
	TokenData &tk = tk_rb[tk_rb_pos];

	tk.type = TK_BUILT_IN_FUNC;
	tk.func = p_func;
	tk.line = line;
	tk.col = column;

	tk_rb_pos = (tk_rb_pos + 1) % RB_SIZE;
}
void GDScriptTokenizerText::_make_constant(const Variant &p_constant) {
	TokenData &tk = tk_rb[tk_rb_pos];

	tk.type = TK_CONSTANT;
	tk.constant = p_constant;
	tk.line = line;
	tk.col = column;

	tk_rb_pos = (tk_rb_pos + 1) % RB_SIZE;
}

void GDScriptTokenizerText::_make_type(const Variant::Type &p_type) {
	TokenData &tk = tk_rb[tk_rb_pos];

	tk.type = TK_BUILT_IN_TYPE;
	tk.vtype = p_type;
	tk.line = line;
	tk.col = column;

	tk_rb_pos = (tk_rb_pos + 1) % RB_SIZE;
}

int64_t hex_to_int64(String str, bool p_with_prefix = true) {
	int len = str.length();
	//ERR_FAIL_COND_V_MSG(p_with_prefix ? len < 3 : len == 0, 0, String("Invalid hexadecimal notation length in string ") + (p_with_prefix ? "with" : "without") + " prefix \"" + *this + "\".");

	const char32_t *s = str.ptr();

	int64_t sign = s[0] == '-' ? -1 : 1;

	if (sign < 0) {
		s++;
	}

	if (p_with_prefix) {
		//ERR_FAIL_COND_V_MSG(s[0] != '0' || LOWERCASE(s[1]) != 'x', 0, "Invalid hexadecimal notation prefix in string \"" + *this + "\".");
		s += 2;
	}

	int64_t hex = 0;

	while (*s) {
		CharType c = LOWERCASE(*s);
		int64_t n;
		if (c >= '0' && c <= '9') {
			n = c - '0';
		} else if (c >= 'a' && c <= 'f') {
			n = (c - 'a') + 10;
		} else {
			//ERR_FAIL_V_MSG(0, "Invalid hexadecimal notation character \"" + chr(*s) + "\" in string \"" + *this + "\".");
		}
		bool overflow = ((hex > INT64_MAX / 16) && (sign == 1 || (sign == -1 && hex != (INT64_MAX >> 4) + 1))) || (sign == -1 && hex == (INT64_MAX >> 4) + 1 && c > '0');
		//ERR_FAIL_COND_V_MSG(overflow, sign == 1 ? INT64_MAX : INT64_MIN, "Cannot represent " + *this + " as a 64-bit signed integer, since the value is " + (sign == 1 ? "too large." : "too small."));
		hex *= 16;
		hex += n;
		s++;
	}

	return hex * sign;
}

int64_t bin_to_int64(String str, bool p_with_prefix = true) {
	int len = str.length();
	//ERR_FAIL_COND_V_MSG(p_with_prefix ? len < 3 : len == 0, 0, String("Invalid binary notation length in string ") + (p_with_prefix ? "with" : "without") + " prefix \"" + *this + "\".");

	const char32_t *s = str.ptr();

	int64_t sign = s[0] == '-' ? -1 : 1;

	if (sign < 0) {
		s++;
	}

	if (p_with_prefix) {
		//ERR_FAIL_COND_V_MSG(s[0] != '0' || LOWERCASE(s[1]) != 'b', 0, "Invalid binary notation prefix in string \"" + *this + "\".");
		s += 2;
	}

	int64_t binary = 0;

	while (*s) {
		CharType c = LOWERCASE(*s);
		int64_t n;
		if (c == '0' || c == '1') {
			n = c - '0';
		} else {
			//ERR_FAIL_V_MSG(0, "Invalid binary notation character \"" + chr(*s) + "\" in string \"" + *this + "\".");
		}
		// Check for overflow/underflow, with special case to ensure INT64_MIN does not result in error
		bool overflow = ((binary > INT64_MAX / 2) && (sign == 1 || (sign == -1 && binary != (INT64_MAX >> 1) + 1))) || (sign == -1 && binary == (INT64_MAX >> 1) + 1 && c > '0');
		//ERR_FAIL_COND_V_MSG(overflow, sign == 1 ? INT64_MAX : INT64_MIN, "Cannot represent " + *this + " as a 64-bit signed integer, since the value is " + (sign == 1 ? "too large." : "too small."));
		binary *= 2;
		binary += n;
		s++;
	}

	return binary * sign;
}

int64_t to_int64(String str) {
	if (str.length() == 0) {
		return 0;
	}

	int to = (str.find(".") >= 0) ? str.find(".") : str.length();

	int64_t integer = 0;
	int64_t sign = 1;

	for (int i = 0; i < to; i++) {
		char32_t c = str[i];
		if (c >= '0' && c <= '9') {
			bool overflow = (integer > INT64_MAX / 10) || (integer == INT64_MAX / 10 && ((sign == 1 && c > '7') || (sign == -1 && c > '8')));
			//ERR_FAIL_COND_V_MSG(overflow, sign == 1 ? INT64_MAX : INT64_MIN, "Cannot represent " + *this + " as a 64-bit signed integer, since the value is " + (sign == 1 ? "too large." : "too small."));
			integer *= 10;
			integer += c - '0';

		} else if (integer == 0 && c == '-') {
			sign = -sign;
		}
	}

	return integer * sign;
}

template <class C>
static double built_in_strtod(
		/* A decimal ASCII floating-point number,
		 * optionally preceded by white space. Must
		 * have form "-I.FE-X", where I is the integer
		 * part of the mantissa, F is the fractional
		 * part of the mantissa, and X is the
		 * exponent. Either of the signs may be "+",
		 * "-", or omitted. Either I or F may be
		 * omitted, or both. The decimal point isn't
		 * necessary unless F is present. The "E" may
		 * actually be an "e". E and X may both be
		 * omitted (but not just one). */
		const C *string,
		/* If non-nullptr, store terminating Cacter's
		 * address here. */
		C **endPtr = nullptr) {
	/* Largest possible base 10 exponent. Any
	 * exponent larger than this will already
	 * produce underflow or overflow, so there's
	 * no need to worry about additional digits. */
	static const int maxExponent = 511;
	/* Table giving binary powers of 10. Entry
	 * is 10^2^i. Used to convert decimal
	 * exponents into floating-point numbers. */
	static const double powersOf10[] = {
		10.,
		100.,
		1.0e4,
		1.0e8,
		1.0e16,
		1.0e32,
		1.0e64,
		1.0e128,
		1.0e256
	};

	bool sign, expSign = false;
	double fraction, dblExp;
	const double *d;
	const C *p;
	int c;
	/* Exponent read from "EX" field. */
	int exp = 0;
	/* Exponent that derives from the fractional
	 * part. Under normal circumstances, it is
	 * the negative of the number of digits in F.
	 * However, if I is very long, the last digits
	 * of I get dropped (otherwise a long I with a
	 * large negative exponent could cause an
	 * unnecessary overflow on I alone). In this
	 * case, fracExp is incremented one for each
	 * dropped digit. */
	int fracExp = 0;
	/* Number of digits in mantissa. */
	int mantSize;
	/* Number of mantissa digits BEFORE decimal point. */
	int decPt;
	/* Temporarily holds location of exponent in string. */
	const C *pExp;

	/*
	 * Strip off leading blanks and check for a sign.
	 */

	p = string;
	while (*p == ' ' || *p == '\t' || *p == '\n') {
		p += 1;
	}
	if (*p == '-') {
		sign = true;
		p += 1;
	} else {
		if (*p == '+') {
			p += 1;
		}
		sign = false;
	}

	/*
	 * Count the number of digits in the mantissa (including the decimal
	 * point), and also locate the decimal point.
	 */

	decPt = -1;
	for (mantSize = 0;; mantSize += 1) {
		c = *p;
		if (!IS_DIGIT(c)) {
			if ((c != '.') || (decPt >= 0)) {
				break;
			}
			decPt = mantSize;
		}
		p += 1;
	}

	/*
	 * Now suck up the digits in the mantissa. Use two integers to collect 9
	 * digits each (this is faster than using floating-point). If the mantissa
	 * has more than 18 digits, ignore the extras, since they can't affect the
	 * value anyway.
	 */

	pExp = p;
	p -= mantSize;
	if (decPt < 0) {
		decPt = mantSize;
	} else {
		mantSize -= 1; /* One of the digits was the point. */
	}
	if (mantSize > 18) {
		fracExp = decPt - 18;
		mantSize = 18;
	} else {
		fracExp = decPt - mantSize;
	}
	if (mantSize == 0) {
		fraction = 0.0;
		p = string;
		goto done;
	} else {
		int frac1, frac2;

		frac1 = 0;
		for (; mantSize > 9; mantSize -= 1) {
			c = *p;
			p += 1;
			if (c == '.') {
				c = *p;
				p += 1;
			}
			frac1 = 10 * frac1 + (c - '0');
		}
		frac2 = 0;
		for (; mantSize > 0; mantSize -= 1) {
			c = *p;
			p += 1;
			if (c == '.') {
				c = *p;
				p += 1;
			}
			frac2 = 10 * frac2 + (c - '0');
		}
		fraction = (1.0e9 * frac1) + frac2;
	}

	/*
	 * Skim off the exponent.
	 */

	p = pExp;
	if ((*p == 'E') || (*p == 'e')) {
		p += 1;
		if (*p == '-') {
			expSign = true;
			p += 1;
		} else {
			if (*p == '+') {
				p += 1;
			}
			expSign = false;
		}
		if (!IS_DIGIT(CharType(*p))) {
			p = pExp;
			goto done;
		}
		while (IS_DIGIT(CharType(*p))) {
			exp = exp * 10 + (*p - '0');
			p += 1;
		}
	}
	if (expSign) {
		exp = fracExp - exp;
	} else {
		exp = fracExp + exp;
	}

	/*
	 * Generate a floating-point number that represents the exponent. Do this
	 * by processing the exponent one bit at a time to combine many powers of
	 * 2 of 10. Then combine the exponent with the fraction.
	 */

	if (exp < 0) {
		expSign = true;
		exp = -exp;
	} else {
		expSign = false;
	}

	if (exp > maxExponent) {
		exp = maxExponent;
		WARN_PRINT("Exponent too high");
	}
	dblExp = 1.0;
	for (d = powersOf10; exp != 0; exp >>= 1, ++d) {
		if (exp & 01) {
			dblExp *= *d;
		}
	}
	if (expSign) {
		fraction /= dblExp;
	} else {
		fraction *= dblExp;
	}

done:
	if (endPtr != nullptr) {
		*endPtr = (C *)p;
	}

	if (sign) {
		return -fraction;
	}
	return fraction;
}

double to_double(const String str) {
	return built_in_strtod<char32_t>(str.ptr());
}

void GDScriptTokenizerText::_make_error(const String &p_error) {
	error_flag = true;
	last_error = p_error;

	TokenData &tk = tk_rb[tk_rb_pos];
	tk.type = TK_ERROR;
	tk.constant = p_error;
	tk.line = line;
	tk.col = column;
	tk_rb_pos = (tk_rb_pos + 1) % RB_SIZE;
}

void GDScriptTokenizerText::_make_newline(int p_indentation, int p_tabs) {
	TokenData &tk = tk_rb[tk_rb_pos];
	tk.type = TK_NEWLINE;
	tk.constant = Vector2(p_indentation, p_tabs);
	tk.line = line;
	tk.col = column;
	tk_rb_pos = (tk_rb_pos + 1) % RB_SIZE;
}

void GDScriptTokenizerText::_advance() {
	if (error_flag) {
		//parser broke
		_make_error(last_error);
		return;
	}

	if (code_pos >= len) {
		_make_token(TK_EOF);
		return;
	}
#define GETCHAR(m_ofs) ((m_ofs + code_pos) >= len ? 0 : _code[m_ofs + code_pos])
#define INCPOS(m_amount)      \
	{                         \
		code_pos += m_amount; \
		column += m_amount;   \
	}
	while (true) {
		bool is_node_path = false;
		StringMode string_mode = STRING_DOUBLE_QUOTE;

		switch (GETCHAR(0)) {
			case 0:
				_make_token(TK_EOF);
				break;
			case '\\':
				INCPOS(1);
				if (GETCHAR(0) == '\r') {
					INCPOS(1);
				}

				if (GETCHAR(0) != '\n') {
					_make_error("Expected newline after '\\'.");
					return;
				}

				INCPOS(1);
				line++;

				while (GETCHAR(0) == ' ' || GETCHAR(0) == '\t') {
					INCPOS(1);
				}

				continue;
			case '\t':
			case '\r':
			case ' ':
				INCPOS(1);
				continue;
			case '#': { // line comment skip
#ifdef DEBUG_ENABLED
				String comment;
#endif // DEBUG_ENABLED
				while (GETCHAR(0) != '\n') {
#ifdef DEBUG_ENABLED
					comment += GETCHAR(0);
#endif // DEBUG_ENABLED
					code_pos++;
					if (GETCHAR(0) == 0) { //end of file
						//_make_error("Unterminated Comment");
						_make_token(TK_EOF);
						return;
					}
				}
#ifdef DEBUG_ENABLED
				String comment_content = comment.trim_prefix("#").trim_prefix(" ");
				if (comment_content.begins_with("warning-ignore:")) {
					String code = comment_content.get_slice(":", 1);
					warning_skips.push_back(Pair<int, String>(line, code.strip_edges().to_lower()));
				} else if (comment_content.begins_with("warning-ignore-all:")) {
					String code = comment_content.get_slice(":", 1);
					warning_global_skips.insert(code.strip_edges().to_lower());
				} else if (comment_content.strip_edges() == "warnings-disable") {
					ignore_warnings = true;
				}
#endif // DEBUG_ENABLED
				FALLTHROUGH;
			}
			case '\n': {
				line++;
				INCPOS(1);
				bool used_spaces = false;
				int tabs = 0;
				column = 1;
				int i = 0;
				while (true) {
					if (GETCHAR(i) == ' ') {
						i++;
						used_spaces = true;
					} else if (GETCHAR(i) == '\t') {
						if (used_spaces) {
							_make_error("Spaces used before tabs on a line");
							return;
						}
						i++;
						tabs++;
					} else {
						break; // not indentation anymore
					}
				}

				_make_newline(i, tabs);
				return;
			}
			case '/': {
				switch (GETCHAR(1)) {
					case '=': { // diveq

						_make_token(TK_OP_ASSIGN_DIV);
						INCPOS(1);

					} break;
					default:
						_make_token(TK_OP_DIV);
				}
			} break;
			case '=': {
				if (GETCHAR(1) == '=') {
					_make_token(TK_OP_EQUAL);
					INCPOS(1);

				} else {
					_make_token(TK_OP_ASSIGN);
				}

			} break;
			case '<': {
				if (GETCHAR(1) == '=') {
					_make_token(TK_OP_LESS_EQUAL);
					INCPOS(1);
				} else if (GETCHAR(1) == '<') {
					if (GETCHAR(2) == '=') {
						_make_token(TK_OP_ASSIGN_SHIFT_LEFT);
						INCPOS(1);
					} else {
						_make_token(TK_OP_SHIFT_LEFT);
					}
					INCPOS(1);
				} else {
					_make_token(TK_OP_LESS);
				}

			} break;
			case '>': {
				if (GETCHAR(1) == '=') {
					_make_token(TK_OP_GREATER_EQUAL);
					INCPOS(1);
				} else if (GETCHAR(1) == '>') {
					if (GETCHAR(2) == '=') {
						_make_token(TK_OP_ASSIGN_SHIFT_RIGHT);
						INCPOS(1);

					} else {
						_make_token(TK_OP_SHIFT_RIGHT);
					}
					INCPOS(1);
				} else {
					_make_token(TK_OP_GREATER);
				}

			} break;
			case '!': {
				if (GETCHAR(1) == '=') {
					_make_token(TK_OP_NOT_EQUAL);
					INCPOS(1);
				} else {
					_make_token(TK_OP_NOT);
				}

			} break;
			//case '"' //string - no strings in shader
			//case '\'' //string - no strings in shader
			case '{':
				_make_token(TK_CURLY_BRACKET_OPEN);
				break;
			case '}':
				_make_token(TK_CURLY_BRACKET_CLOSE);
				break;
			case '[':
				_make_token(TK_BRACKET_OPEN);
				break;
			case ']':
				_make_token(TK_BRACKET_CLOSE);
				break;
			case '(':
				_make_token(TK_PARENTHESIS_OPEN);
				break;
			case ')':
				_make_token(TK_PARENTHESIS_CLOSE);
				break;
			case ',':
				_make_token(TK_COMMA);
				break;
			case ';':
				_make_token(TK_SEMICOLON);
				break;
			case '?':
				_make_token(TK_QUESTION_MARK);
				break;
			case ':':
				_make_token(TK_COLON); //for methods maybe but now useless.
				break;
			case '$':
				_make_token(TK_DOLLAR); //for the get_node() shortener
				break;
			case '^': {
				if (GETCHAR(1) == '=') {
					_make_token(TK_OP_ASSIGN_BIT_XOR);
					INCPOS(1);
				} else {
					_make_token(TK_OP_BIT_XOR);
				}

			} break;
			case '~':
				_make_token(TK_OP_BIT_INVERT);
				break;
			case '&': {
				if (GETCHAR(1) == '&') {
					_make_token(TK_OP_AND);
					INCPOS(1);
				} else if (GETCHAR(1) == '=') {
					_make_token(TK_OP_ASSIGN_BIT_AND);
					INCPOS(1);
				} else {
					_make_token(TK_OP_BIT_AND);
				}
			} break;
			case '|': {
				if (GETCHAR(1) == '|') {
					_make_token(TK_OP_OR);
					INCPOS(1);
				} else if (GETCHAR(1) == '=') {
					_make_token(TK_OP_ASSIGN_BIT_OR);
					INCPOS(1);
				} else {
					_make_token(TK_OP_BIT_OR);
				}
			} break;
			case '*': {
				if (GETCHAR(1) == '=') {
					_make_token(TK_OP_ASSIGN_MUL);
					INCPOS(1);
				} else {
					_make_token(TK_OP_MUL);
				}
			} break;
			case '+': {
				if (GETCHAR(1) == '=') {
					_make_token(TK_OP_ASSIGN_ADD);
					INCPOS(1);
					/*
				}  else if (GETCHAR(1)=='+') {
					_make_token(TK_OP_PLUS_PLUS);
					INCPOS(1);
				*/
				} else {
					_make_token(TK_OP_ADD);
				}

			} break;
			case '-': {
				if (GETCHAR(1) == '=') {
					_make_token(TK_OP_ASSIGN_SUB);
					INCPOS(1);
				} else if (GETCHAR(1) == '>') {
					_make_token(TK_FORWARD_ARROW);
					INCPOS(1);
				} else {
					_make_token(TK_OP_SUB);
				}
			} break;
			case '%': {
				if (GETCHAR(1) == '=') {
					_make_token(TK_OP_ASSIGN_MOD);
					INCPOS(1);
				} else {
					_make_token(TK_OP_MOD);
				}
			} break;
			case '@':
				if (CharType(GETCHAR(1)) != '"' && CharType(GETCHAR(1)) != '\'') {
					_make_error("Unexpected '@'");
					return;
				}
				INCPOS(1);
				is_node_path = true;
				FALLTHROUGH;
			case '\'':
			case '"': {
				if (GETCHAR(0) == '\'') {
					string_mode = STRING_SINGLE_QUOTE;
				}

				int i = 1;
				if (string_mode == STRING_DOUBLE_QUOTE && GETCHAR(i) == '"' && GETCHAR(i + 1) == '"') {
					i += 2;
					string_mode = STRING_MULTILINE;
				}

				String str;
				while (true) {
					if (CharType(GETCHAR(i)) == 0) {
						_make_error("Unterminated String");
						return;
					} else if (string_mode == STRING_DOUBLE_QUOTE && CharType(GETCHAR(i)) == '"') {
						break;
					} else if (string_mode == STRING_SINGLE_QUOTE && CharType(GETCHAR(i)) == '\'') {
						break;
					} else if (string_mode == STRING_MULTILINE && CharType(GETCHAR(i)) == '\"' && CharType(GETCHAR(i + 1)) == '\"' && CharType(GETCHAR(i + 2)) == '\"') {
						i += 2;
						break;
					} else if (string_mode != STRING_MULTILINE && CharType(GETCHAR(i)) == '\n') {
						_make_error("Unexpected EOL at String.");
						return;
					} else if (CharType(GETCHAR(i)) == 0xFFFF) {
						//string ends here, next will be TK
						i--;
						break;
					} else if (CharType(GETCHAR(i)) == '\\') {
						//escaped characters...
						i++;
						CharType next = GETCHAR(i);
						if (next == 0) {
							_make_error("Unterminated String");
							return;
						}
						CharType res = 0;

						switch (next) {
							case 'a':
								res = 7;
								break;
							case 'b':
								res = 8;
								break;
							case 't':
								res = 9;
								break;
							case 'n':
								res = 10;
								break;
							case 'v':
								res = 11;
								break;
							case 'f':
								res = 12;
								break;
							case 'r':
								res = 13;
								break;
							case '\'':
								res = '\'';
								break;
							case '\"':
								res = '\"';
								break;
							case '\\':
								res = '\\';
								break;
							case '/':
								res = '/';
								break; //wtf

							case 'u': {
								//hexnumbarh - oct is deprecated
								i += 1;
								for (int j = 0; j < 4; j++) {
									CharType c = GETCHAR(i + j);
									if (c == 0) {
										_make_error("Unterminated String");
										return;
									}

									CharType v = 0;
									if (c >= '0' && c <= '9') {
										v = c - '0';
									} else if (c >= 'a' && c <= 'f') {
										v = c - 'a';
										v += 10;
									} else if (c >= 'A' && c <= 'F') {
										v = c - 'A';
										v += 10;
									} else {
										_make_error("Malformed hex constant in string");
										return;
									}

									res <<= 4;
									res |= v;
								}
								i += 3;

							} break;
							default: {
								_make_error("Invalid escape sequence");
								return;
							} break;
						}

						str += res;

					} else {
						if (CharType(GETCHAR(i)) == '\n') {
							line++;
							column = 1;
						}

						str += CharType(GETCHAR(i));
					}
					i++;
				}
				INCPOS(i);

				if (is_node_path) {
					_make_constant(NodePath(str));
				} else {
					_make_constant(str);
				}

			} break;
			case 0xFFFF: {
				_make_token(TK_CURSOR);
			} break;
			default: {
				if (_is_number(GETCHAR(0)) || (GETCHAR(0) == '.' && _is_number(GETCHAR(1)))) {
					// parse number
					bool period_found = false;
					bool exponent_found = false;
					bool hexa_found = false;
					bool bin_found = false;
					bool sign_found = false;

					String str;
					int i = 0;

					while (true) {
						if (GETCHAR(i) == '.') {
							if (period_found || exponent_found) {
								_make_error("Invalid numeric constant at '.'");
								return;
							} else if (bin_found) {
								_make_error("Invalid binary constant at '.'");
								return;
							} else if (hexa_found) {
								_make_error("Invalid hexadecimal constant at '.'");
								return;
							}
							period_found = true;
						} else if (GETCHAR(i) == 'x') {
							if (hexa_found || bin_found || str.length() != 1 || !((i == 1 && str[0] == '0') || (i == 2 && str[1] == '0' && str[0] == '-'))) {
								_make_error("Invalid numeric constant at 'x'");
								return;
							}
							hexa_found = true;
						} else if (hexa_found && _is_hex(GETCHAR(i))) {
						} else if (!hexa_found && GETCHAR(i) == 'b') {
							if (bin_found || str.length() != 1 || !((i == 1 && str[0] == '0') || (i == 2 && str[1] == '0' && str[0] == '-'))) {
								_make_error("Invalid numeric constant at 'b'");
								return;
							}
							bin_found = true;
						} else if (!hexa_found && GETCHAR(i) == 'e') {
							if (exponent_found || bin_found) {
								_make_error("Invalid numeric constant at 'e'");
								return;
							}
							exponent_found = true;
						} else if (_is_number(GETCHAR(i))) {
							//all ok

						} else if (bin_found && _is_bin(GETCHAR(i))) {
						} else if ((GETCHAR(i) == '-' || GETCHAR(i) == '+') && exponent_found) {
							if (sign_found) {
								_make_error("Invalid numeric constant at '-'");
								return;
							}
							sign_found = true;
						} else if (GETCHAR(i) == '_') {
							i++;
							continue; // Included for readability, shouldn't be a part of the string
						} else {
							break;
						}

						str += CharType(GETCHAR(i));
						i++;
					}

					if (!(_is_number(str[str.length() - 1]) || (hexa_found && _is_hex(str[str.length() - 1])))) {
						_make_error("Invalid numeric constant: " + str);
						return;
					}

					INCPOS(i);
					if (hexa_found) {
						int64_t val = hex_to_int64(str);
						_make_constant(val);
					} else if (bin_found) {
						int64_t val = bin_to_int64(str);
						_make_constant(val);
					} else if (period_found || exponent_found) {
						double val = to_double(str);
						_make_constant(val);
					} else {
						int64_t val = to_int64(str);
						_make_constant(val);
					}

					return;
				}

				if (GETCHAR(0) == '.') {
					//parse period
					_make_token(TK_PERIOD);
					break;
				}

				if (_is_text_char(GETCHAR(0))) {
					// parse identifier
					String str;
					str += CharType(GETCHAR(0));

					int i = 1;
					while (_is_text_char(GETCHAR(i))) {
						str += CharType(GETCHAR(i));
						i++;
					}

					bool identifier = false;

					if (str == "null") {
						_make_constant(Variant());

					} else if (str == "true") {
						_make_constant(true);

					} else if (str == "false") {
						_make_constant(false);
					} else {
						bool found = false;

						{
							int idx = 0;

							while (_type_list[idx].text) {
								if (str == _type_list[idx].text) {
									_make_type(_type_list[idx].type);
									found = true;
									break;
								}
								idx++;
							}
						}

						if (!found) {
							//built in func?

							for (int j = 0; j < FUNC_MAX; j++) {
								if (str == get_func_name(Function(j))) {
									_make_built_in_func(Function(j));
									found = true;
									break;
								}
							}
						}

						if (!found) {
							//keyword

							int idx = 0;
							found = false;

							while (_keyword_list[idx].text) {
								if (str == _keyword_list[idx].text) {
									_make_token(_keyword_list[idx].token);
									found = true;
									break;
								}
								idx++;
							}
						}

						if (!found) {
							identifier = true;
						}
					}

					if (identifier) {
						_make_identifier(str);
					}
					INCPOS(str.length());
					return;
				}

				_make_error("Unknown character");
				return;

			} break;
		}

		INCPOS(1);
		break;
	}
}

void GDScriptTokenizerText::set_code(const String &p_code) {
	code = p_code;
	len = p_code.length();
	if (len) {
		_code = (const CharType *)&code[0];
	} else {
		_code = nullptr;
	}
	code_pos = 0;
	line = 1; //it is stand-ar-ized that lines begin in 1 in code..
	column = 1; //the same holds for columns
	tk_rb_pos = 0;
	error_flag = false;
#ifdef DEBUG_ENABLED
	ignore_warnings = false;
#endif // DEBUG_ENABLED
	last_error = "";
	for (int i = 0; i < MAX_LOOKAHEAD + 1; i++) {
		_advance();
	}
}

GDScriptTokenizerText::Token GDScriptTokenizerText::get_token(int p_offset) const {
	ERR_FAIL_COND_V(p_offset <= -MAX_LOOKAHEAD, TK_ERROR);
	ERR_FAIL_COND_V(p_offset >= MAX_LOOKAHEAD, TK_ERROR);

	int ofs = (RB_SIZE + tk_rb_pos + p_offset - MAX_LOOKAHEAD - 1) % RB_SIZE;
	return tk_rb[ofs].type;
}

int GDScriptTokenizerText::get_token_line(int p_offset) const {
	ERR_FAIL_COND_V(p_offset <= -MAX_LOOKAHEAD, -1);
	ERR_FAIL_COND_V(p_offset >= MAX_LOOKAHEAD, -1);

	int ofs = (RB_SIZE + tk_rb_pos + p_offset - MAX_LOOKAHEAD - 1) % RB_SIZE;
	return tk_rb[ofs].line;
}

int GDScriptTokenizerText::get_token_column(int p_offset) const {
	ERR_FAIL_COND_V(p_offset <= -MAX_LOOKAHEAD, -1);
	ERR_FAIL_COND_V(p_offset >= MAX_LOOKAHEAD, -1);

	int ofs = (RB_SIZE + tk_rb_pos + p_offset - MAX_LOOKAHEAD - 1) % RB_SIZE;
	return tk_rb[ofs].col;
}

const Variant &GDScriptTokenizerText::get_token_constant(int p_offset) const {
	ERR_FAIL_COND_V(p_offset <= -MAX_LOOKAHEAD, tk_rb[0].constant);
	ERR_FAIL_COND_V(p_offset >= MAX_LOOKAHEAD, tk_rb[0].constant);

	int ofs = (RB_SIZE + tk_rb_pos + p_offset - MAX_LOOKAHEAD - 1) % RB_SIZE;
	ERR_FAIL_COND_V(tk_rb[ofs].type != TK_CONSTANT, tk_rb[0].constant);
	return tk_rb[ofs].constant;
}

StringName GDScriptTokenizerText::get_token_identifier(int p_offset) const {
	ERR_FAIL_COND_V(p_offset <= -MAX_LOOKAHEAD, StringName());
	ERR_FAIL_COND_V(p_offset >= MAX_LOOKAHEAD, StringName());

	int ofs = (RB_SIZE + tk_rb_pos + p_offset - MAX_LOOKAHEAD - 1) % RB_SIZE;
	ERR_FAIL_COND_V(tk_rb[ofs].type != TK_IDENTIFIER, StringName());
	return tk_rb[ofs].identifier;
}

Function GDScriptTokenizerText::get_token_built_in_func(int p_offset) const {
	ERR_FAIL_COND_V(p_offset <= -MAX_LOOKAHEAD, FUNC_MAX);
	ERR_FAIL_COND_V(p_offset >= MAX_LOOKAHEAD, FUNC_MAX);

	int ofs = (RB_SIZE + tk_rb_pos + p_offset - MAX_LOOKAHEAD - 1) % RB_SIZE;
	ERR_FAIL_COND_V(tk_rb[ofs].type != TK_BUILT_IN_FUNC, FUNC_MAX);
	return tk_rb[ofs].func;
}

Variant::Type GDScriptTokenizerText::get_token_type(int p_offset) const {
	ERR_FAIL_COND_V(p_offset <= -MAX_LOOKAHEAD, Variant::NIL);
	ERR_FAIL_COND_V(p_offset >= MAX_LOOKAHEAD, Variant::NIL);

	int ofs = (RB_SIZE + tk_rb_pos + p_offset - MAX_LOOKAHEAD - 1) % RB_SIZE;
	ERR_FAIL_COND_V(tk_rb[ofs].type != TK_BUILT_IN_TYPE, Variant::NIL);
	return tk_rb[ofs].vtype;
}

int GDScriptTokenizerText::get_token_line_indent(int p_offset) const {
	ERR_FAIL_COND_V(p_offset <= -MAX_LOOKAHEAD, 0);
	ERR_FAIL_COND_V(p_offset >= MAX_LOOKAHEAD, 0);

	int ofs = (RB_SIZE + tk_rb_pos + p_offset - MAX_LOOKAHEAD - 1) % RB_SIZE;
	ERR_FAIL_COND_V(tk_rb[ofs].type != TK_NEWLINE, 0);
	return tk_rb[ofs].constant.operator Vector2().x;
}

int GDScriptTokenizerText::get_token_line_tab_indent(int p_offset) const {
	ERR_FAIL_COND_V(p_offset <= -MAX_LOOKAHEAD, 0);
	ERR_FAIL_COND_V(p_offset >= MAX_LOOKAHEAD, 0);

	int ofs = (RB_SIZE + tk_rb_pos + p_offset - MAX_LOOKAHEAD - 1) % RB_SIZE;
	ERR_FAIL_COND_V(tk_rb[ofs].type != TK_NEWLINE, 0);
	return tk_rb[ofs].constant.operator Vector2().y;
}

String GDScriptTokenizerText::get_token_error(int p_offset) const {
	ERR_FAIL_COND_V(p_offset <= -MAX_LOOKAHEAD, String());
	ERR_FAIL_COND_V(p_offset >= MAX_LOOKAHEAD, String());

	int ofs = (RB_SIZE + tk_rb_pos + p_offset - MAX_LOOKAHEAD - 1) % RB_SIZE;
	ERR_FAIL_COND_V(tk_rb[ofs].type != TK_ERROR, String());
	return tk_rb[ofs].constant;
}

void GDScriptTokenizerText::advance(int p_amount) {
	ERR_FAIL_COND(p_amount <= 0);
	for (int i = 0; i < p_amount; i++) {
		_advance();
	}
}

//////////////////////////////////////////////////////////////////////////////////////////////////////

#define BYTECODE_VERSION 13

Error GDScriptTokenizerBuffer::set_code_buffer(const Vector<uint8_t> &p_buffer) {
	const uint8_t *buf = p_buffer.ptr();
	int total_len = p_buffer.size();
	ERR_FAIL_COND_V(p_buffer.size() < 24 || p_buffer[0] != 'G' || p_buffer[1] != 'D' || p_buffer[2] != 'S' || p_buffer[3] != 'C', ERR_INVALID_DATA);

	int version = decode_uint32(&buf[4]);
	ERR_FAIL_COND_V_MSG(version > BYTECODE_VERSION, ERR_INVALID_DATA, "Bytecode is too recent! Please use a newer engine version.");

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
		// An object cannot be constant, never decode objects
		Error err = decode_variant(v, b, total_len, &len, false);
		if (err) {
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

		if ((*b) & TOKEN_BYTE_MASK) { //little endian always
			ERR_FAIL_COND_V(total_len < 4, ERR_INVALID_DATA);

			tokens.write[i] = decode_uint32(b) & ~TOKEN_BYTE_MASK;
			b += 4;
		} else {
			tokens.write[i] = *b;
			b += 1;
			total_len--;
		}
	}

	token = 0;

	return OK;
}

Vector<uint8_t> GDScriptTokenizerBuffer::parse_code_string(const String &p_code) {
	Vector<uint8_t> buf;

	Map<StringName, int> identifier_map;
	HashMap<Variant, int, VariantHasher, VariantComparator> constant_map;
	Map<uint32_t, int> line_map;
	Vector<uint32_t> token_array;

	GDScriptTokenizerText tt;
	tt.set_code(p_code);
	int line = -1;

	while (true) {
		if (tt.get_token_line() != line) {
			line = tt.get_token_line();
			line_map[line] = token_array.size();
		}

		uint32_t token = tt.get_token();
		switch (tt.get_token()) {
			case TK_IDENTIFIER: {
				StringName id = tt.get_token_identifier();
				if (!identifier_map.has(id)) {
					int idx = identifier_map.size();
					identifier_map[id] = idx;
				}
				token |= identifier_map[id] << TOKEN_BITS;
			} break;
			case TK_CONSTANT: {
				const Variant &c = tt.get_token_constant();
				if (!constant_map.has(c)) {
					int idx = constant_map.size();
					constant_map[c] = idx;
				}
				token |= constant_map[c] << TOKEN_BITS;
			} break;
			case TK_BUILT_IN_TYPE: {
				token |= tt.get_token_type() << TOKEN_BITS;
			} break;
			case TK_BUILT_IN_FUNC: {
				token |= tt.get_token_built_in_func() << TOKEN_BITS;

			} break;
			case TK_NEWLINE: {
				token |= tt.get_token_line_indent() << TOKEN_BITS;
			} break;
			case TK_ERROR: {
				ERR_FAIL_V(Vector<uint8_t>());
			} break;
			default: {
			}
		};

		token_array.push_back(token);

		if (tt.get_token() == TK_EOF) {
			break;
		}
		tt.advance();
	}

	//reverse maps

	Map<int, StringName> rev_identifier_map;
	for (Map<StringName, int>::Element *E = identifier_map.front(); E; E = E->next()) {
		rev_identifier_map[E->get()] = E->key();
	}

	Map<int, Variant> rev_constant_map;
	const Variant *K = nullptr;

	for (KeyValue<Variant, int> &F : constant_map) {
		rev_constant_map[constant_map[F.key]] = F.key;
	}

	Map<int, uint32_t> rev_line_map;
	for (Map<uint32_t, int>::Element *E = line_map.front(); E; E = E->next()) {
		rev_line_map[E->get()] = E->key();
	}

	//save header
	buf.resize(24);
	buf.write[0] = 'G';
	buf.write[1] = 'D';
	buf.write[2] = 'S';
	buf.write[3] = 'C';
	encode_uint32(BYTECODE_VERSION, &buf.write[4]);
	encode_uint32(identifier_map.size(), &buf.write[8]);
	encode_uint32(constant_map.size(), &buf.write[12]);
	encode_uint32(line_map.size(), &buf.write[16]);
	encode_uint32(token_array.size(), &buf.write[20]);

	//save identifiers

	for (Map<int, StringName>::Element *E = rev_identifier_map.front(); E; E = E->next()) {
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

	for (Map<int, Variant>::Element *E = rev_constant_map.front(); E; E = E->next()) {
		int len;
		// Objects cannot be constant, never encode objects
		Error err = encode_variant(E->get(), nullptr, len, false);
		ERR_FAIL_COND_V_MSG(err != OK, Vector<uint8_t>(), "Error when trying to encode Variant.");
		int pos = buf.size();
		buf.resize(pos + len);
		encode_variant(E->get(), &buf.write[pos], len, false);
	}

	for (Map<int, uint32_t>::Element *E = rev_line_map.front(); E; E = E->next()) {
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

GDScriptTokenizerBuffer::Token GDScriptTokenizerBuffer::get_token(int p_offset) const {
	int offset = token + p_offset;

	if (offset < 0 || offset >= tokens.size()) {
		return TK_EOF;
	}

	return GDScriptTokenizerBuffer::Token(tokens[offset] & TOKEN_MASK);
}

StringName GDScriptTokenizerBuffer::get_token_identifier(int p_offset) const {
	int offset = token + p_offset;

	ERR_FAIL_INDEX_V(offset, tokens.size(), StringName());
	uint32_t identifier = tokens[offset] >> TOKEN_BITS;
	ERR_FAIL_UNSIGNED_INDEX_V(identifier, (uint32_t)identifiers.size(), StringName());

	return identifiers[identifier];
}

Function GDScriptTokenizerBuffer::get_token_built_in_func(int p_offset) const {
	int offset = token + p_offset;
	ERR_FAIL_INDEX_V(offset, tokens.size(), Function::FUNC_MAX);
	return Function(tokens[offset] >> TOKEN_BITS);
}

Variant::Type GDScriptTokenizerBuffer::get_token_type(int p_offset) const {
	int offset = token + p_offset;
	ERR_FAIL_INDEX_V(offset, tokens.size(), Variant::NIL);

	return Variant::Type(tokens[offset] >> TOKEN_BITS);
}

int GDScriptTokenizerBuffer::get_token_line(int p_offset) const {
	int offset = token + p_offset;
	int pos = lines.find_nearest(offset);

	if (pos < 0) {
		return -1;
	}
	if (pos >= lines.size()) {
		pos = lines.size() - 1;
	}

	uint32_t l = lines.getv(pos);
	return l & TOKEN_LINE_MASK;
}
int GDScriptTokenizerBuffer::get_token_column(int p_offset) const {
	int offset = token + p_offset;
	int pos = lines.find_nearest(offset);
	if (pos < 0) {
		return -1;
	}
	if (pos >= lines.size()) {
		pos = lines.size() - 1;
	}

	uint32_t l = lines.getv(pos);
	return l >> TOKEN_LINE_BITS;
}
int GDScriptTokenizerBuffer::get_token_line_indent(int p_offset) const {
	int offset = token + p_offset;
	ERR_FAIL_INDEX_V(offset, tokens.size(), 0);
	return tokens[offset] >> TOKEN_BITS;
}
const Variant &GDScriptTokenizerBuffer::get_token_constant(int p_offset) const {
	int offset = token + p_offset;
	ERR_FAIL_INDEX_V(offset, tokens.size(), nil);
	uint32_t constant = tokens[offset] >> TOKEN_BITS;
	ERR_FAIL_UNSIGNED_INDEX_V(constant, (uint32_t)constants.size(), nil);
	return constants[constant];
}
String GDScriptTokenizerBuffer::get_token_error(int p_offset) const {
	ERR_FAIL_V(String());
}

void GDScriptTokenizerBuffer::advance(int p_amount) {
	ERR_FAIL_INDEX(p_amount + token, tokens.size());
	token += p_amount;
}
GDScriptTokenizerBuffer::GDScriptTokenizerBuffer() {
	token = 0;
}
