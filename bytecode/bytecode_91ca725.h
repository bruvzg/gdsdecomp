// This file is automatically generated by `bytecode_generator.py`
// Do not edit this file directly, as it will be overwritten.
// Instead, edit `bytecode_generator.py` and run it to generate this file.

// clang-format off
#pragma once

#include "bytecode_base.h"

class GDScriptDecomp_91ca725 : public GDScriptDecomp {
	GDCLASS(GDScriptDecomp_91ca725, GDScriptDecomp);
protected:
	static void _bind_methods(){};
	static constexpr int bytecode_version = 12;
	static constexpr int bytecode_rev = 0x91ca725;
	static constexpr int engine_ver_major = 3;
	static constexpr int variant_ver_major = 3;
	static constexpr const char *bytecode_rev_str = "91ca725";
	static constexpr const char *engine_version = "3.0-dev14";
	static constexpr const char *max_engine_version = "";
	static constexpr int parent = 0x216a8aa;

	virtual Vector<GlobalToken> get_added_tokens() const override { return {GlobalToken::G_TK_CONST_TAU}; }
public:
	virtual String get_function_name(int p_func) const override;
	virtual int get_function_count() const override;
	virtual Pair<int, int> get_function_arg_count(int p_func) const override;
	virtual int get_token_max() const override;
	virtual int get_function_index(const String &p_func) const override;
	virtual GDScriptDecomp::GlobalToken get_global_token(int p_token) const override;
	virtual int get_local_token_val(GDScriptDecomp::GlobalToken p_token) const override;
	virtual int get_bytecode_version() const override { return bytecode_version; }
	virtual int get_bytecode_rev() const override { return bytecode_rev; }
	virtual int get_engine_ver_major() const override { return engine_ver_major; }
	virtual int get_variant_ver_major() const override { return variant_ver_major; }
	virtual int get_parent() const override { return parent; }
	virtual String get_engine_version() const override { return engine_version; }
	virtual String get_max_engine_version() const override { return max_engine_version; }
	GDScriptDecomp_91ca725() {}
};

