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

Error GDScriptDecomp::decompile_byte_code_encrypted(const String &p_path, Vector<uint8_t> p_key) {
	Vector<uint8_t> bytecode;

	Ref<FileAccess> fa = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V(fa.is_null(), ERR_FILE_CANT_OPEN);

	// Godot v3 only encrypted the scripts and used a different format with different header fields,
	// So we need to use an older version of FAE to access them
	if (engine_ver_major == 3) {
		Ref<FileAccessEncryptedv3> fae;
		fae.instantiate();
		ERR_FAIL_COND_V(fae.is_null(), ERR_FILE_CORRUPT);

		Error err = fae->open_and_parse(fa, p_key, FileAccessEncryptedv3::MODE_READ);

		if (err) {
			ERR_FAIL_COND_V(err, ERR_FILE_CORRUPT);
		}

		bytecode.resize(fae->get_length());
		fae->get_buffer(bytecode.ptrw(), bytecode.size());
	} else {
		Ref<FileAccessEncrypted> fae;
		fae.instantiate();
		ERR_FAIL_COND_V(fae.is_null(), ERR_FILE_CORRUPT);

		Error err = fae->open_and_parse(fa, p_key, FileAccessEncrypted::MODE_READ);

		if (err) {
			ERR_FAIL_COND_V(err, ERR_FILE_CORRUPT);
		}

		bytecode.resize(fae->get_length());
		fae->get_buffer(bytecode.ptrw(), bytecode.size());
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
