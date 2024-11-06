#include "utility/common.h"
#include "bytecode/bytecode_base.h"
#include "core/error/error_list.h"
#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/missing_resource.h"
#include "external/tga/tga.h"

Vector<String> gdre::get_recursive_dir_list(const String dir, const Vector<String> &wildcards, const bool absolute, const String rel, const bool &res) {
	Vector<String> ret;
	Error err;
	Ref<DirAccess> da = DirAccess::open(dir.path_join(rel), &err);
	ERR_FAIL_COND_V_MSG(da.is_null(), ret, "Failed to open directory " + dir);

	if (da.is_null()) {
		return ret;
	}
	String base = absolute ? dir : "";
	da->list_dir_begin();
	String f = da->get_next();
	while (!f.is_empty()) {
		if (f == "." || f == "..") {
			f = da->get_next();
			continue;
		} else if (da->current_is_dir()) {
			ret.append_array(get_recursive_dir_list(dir, wildcards, absolute, rel.path_join(f)));
		} else {
			if (wildcards.size() > 0) {
				for (int i = 0; i < wildcards.size(); i++) {
					if (f.get_file().match(wildcards[i])) {
						ret.append(base.path_join(rel).path_join(f));
						break;
					}
				}
			} else {
				ret.append(base.path_join(rel).path_join(f));
			}
		}
		f = da->get_next();
	}
	da->list_dir_end();
	return ret;
}

Error gdre::ensure_dir(const String &dst_dir) {
	Error err = OK;
	Ref<DirAccess> da = DirAccess::create(DirAccess::ACCESS_FILESYSTEM);
	ERR_FAIL_COND_V(da.is_null(), ERR_FILE_CANT_OPEN);
	// make_dir_recursive requires a mutex lock for every directory in the path, so it behooves us to check if the directory exists first
	if (!da->dir_exists(dst_dir)) {
		err = da->make_dir_recursive(dst_dir);
	}
	return err;
}

bool gdre::check_header(const Vector<uint8_t> &p_buffer, const char *p_expected_header, int p_expected_len) {
	if (p_buffer.size() < p_expected_len) {
		return false;
	}

	for (int i = 0; i < p_expected_len; i++) {
		if (p_buffer[i] != p_expected_header[i]) {
			return false;
		}
	}

	return true;
}

Error gdre::decompress_image(const Ref<Image> &img) {
	Error err;
	if (img->is_compressed()) {
		err = img->decompress();
		if (err == ERR_UNAVAILABLE) {
			return err;
		}
		ERR_FAIL_COND_V_MSG(err != OK || img.is_null(), err, "Failed to decompress image.");
	}
	return OK;
}

class GodotFileInterface : public tga::FileInterface {
	Ref<FileAccess> m_file;

public:
	GodotFileInterface(const String &p_path, FileAccess::ModeFlags p_mode) {
		m_file = FileAccess::open(p_path, p_mode);
	}
	// Returns true if we can read/write bytes from/into the file
	virtual bool ok() const override {
		return m_file.is_valid();
	};

	// Current position in the file
	virtual size_t tell() override {
		return m_file->get_position();
	}

	// Jump to the given position in the file
	virtual void seek(size_t absPos) override {
		m_file->seek(absPos);
	};

	// Returns the next byte in the file or 0 if ok() = false
	virtual uint8_t read8() override {
		return m_file->get_8();
	};

	// Writes one byte in the file (or do nothing if ok() = false)
	virtual void write8(uint8_t value) override {
		m_file->store_8(value);
	};
};

Error gdre::save_image_as_tga(const String &p_path, const Ref<Image> &p_img) {
	Vector<uint8_t> buffer;
	Ref<Image> source_image = p_img->duplicate();
	GDRE_ERR_DECOMPRESS_OR_FAIL(source_image);
	int width = source_image->get_width();
	int height = source_image->get_height();
	bool isRGB = true;
	bool isAlpha = source_image->detect_alpha();
	switch (source_image->get_format()) {
		case Image::FORMAT_L8:
			isRGB = false;
			break;
		case Image::FORMAT_LA8:
			isRGB = true;
			source_image->convert(Image::FORMAT_RGBA8);
			break;
		case Image::FORMAT_RGB8:
			// we still need to convert it to RGBA8 even if it doesn't have alpha due to the encoder requiring 4 bytes per pixel
			source_image->convert(Image::FORMAT_RGBA8);
			break;
		case Image::FORMAT_RGBA8:
			break;
		default:
			source_image->convert(Image::FORMAT_RGBA8);
			isRGB = true;
			break;
	}

	tga::Header header;
	header.idLength = 0;
	header.colormapType = 0;
	header.imageType = isRGB ? tga::ImageType::RleRgb : tga::ImageType::RleGray;
	header.colormapOrigin = 0;
	header.colormapLength = 0;
	header.colormapDepth = 0;

	header.xOrigin = 0;
	header.yOrigin = 0;
	header.width = width;
	header.height = height;
	header.bitsPerPixel = isRGB ? (isAlpha ? 32 : 24) : 8;
	header.imageDescriptor = isAlpha ? 0xf : 0; // top-left origin always
	tga::Image tga_image{};
	Vector<uint8_t> tga_data = source_image->get_data(); //isRGB ? rgba_to_bgra(source_image->get_data()) : source_image->get_data();
	tga_image.pixels = tga_data.ptrw();
	tga_image.bytesPerPixel = isRGB ? 4 : 1;
	tga_image.rowstride = width * tga_image.bytesPerPixel;
	GodotFileInterface file_interface(p_path, FileAccess::WRITE);
	if (!file_interface.ok()) {
		return ERR_FILE_CANT_WRITE;
	}
	tga::Encoder encoder(&file_interface);
	encoder.writeHeader(header);
	encoder.writeImage(header, tga_image);
	encoder.writeFooter();
	return OK;
}

Error gdre::save_image_as_webp(const String &p_path, const Ref<Image> &p_img, bool lossy) {
	Ref<Image> source_image = p_img->duplicate();
	Error err = OK;
	GDRE_ERR_DECOMPRESS_OR_FAIL(source_image);
	Vector<uint8_t> buffer;
	if (lossy) {
		buffer = Image::webp_lossy_packer(source_image, 1);
	} else {
		buffer = Image::webp_lossless_packer(source_image);
	}
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err, err, vformat("Can't save WEBP at path: '%s'.", p_path));

	file->store_buffer(buffer.ptr(), buffer.size());
	if (file->get_error() != OK && file->get_error() != ERR_FILE_EOF) {
		return ERR_CANT_CREATE;
	}
	return OK;
}

Error gdre::save_image_as_jpeg(const String &p_path, const Ref<Image> &p_img) {
	Vector<uint8_t> buffer;
	Ref<Image> source_image = p_img->duplicate();
	GDRE_ERR_DECOMPRESS_OR_FAIL(source_image);
	return source_image->save_jpg(p_path, 1.0f);
}

void gdre::get_strings_from_variant(const Variant &p_var, Vector<String> &r_strings, const String &engine_version) {
	if (p_var.get_type() == Variant::STRING || p_var.get_type() == Variant::STRING_NAME) {
		r_strings.push_back(p_var);
	} else if (p_var.get_type() == Variant::STRING_NAME) {
		r_strings.push_back(p_var);
	} else if (p_var.get_type() == Variant::PACKED_STRING_ARRAY) {
		Vector<String> p_strings = p_var;
		for (auto &E : p_strings) {
			r_strings.push_back(E);
		}
	} else if (p_var.get_type() == Variant::ARRAY) {
		Array arr = p_var;
		for (int i = 0; i < arr.size(); i++) {
			get_strings_from_variant(arr[i], r_strings, engine_version);
		}
	} else if (p_var.get_type() == Variant::DICTIONARY) {
		Dictionary d = p_var;
		Array keys = d.keys();
		for (int i = 0; i < keys.size(); i++) {
			get_strings_from_variant(keys[i], r_strings, engine_version);
			get_strings_from_variant(d[keys[i]], r_strings, engine_version);
		}
	} else if (p_var.get_type() == Variant::OBJECT) {
		Object *obj = Object::cast_to<Object>(p_var);
		if (obj) {
			List<PropertyInfo> p_list;
			obj->get_property_list(&p_list);
			for (List<PropertyInfo>::Element *E = p_list.front(); E; E = E->next()) {
				auto &p = E->get();
				get_strings_from_variant(obj->get(p.name), r_strings, engine_version);
			}
			Dictionary meta = obj->get_meta(META_MISSING_RESOURCES, Dictionary());
			Array keys = meta.keys();
			for (int i = 0; i < meta.size(); i++) {
				get_strings_from_variant(meta[keys[i]], r_strings, engine_version);
			}
			if (!engine_version.is_empty()) {
				Ref<MissingResource> mr = p_var;
				if (obj->get_class() == "GDScript" || (mr.is_valid() && mr->get_original_class() == "GDScript")) {
					String code = obj->get("script/source");
					if (!code.is_empty()) {
						auto decomp = GDScriptDecomp::create_decomp_for_version(engine_version, true);
						if (!decomp.is_null()) {
							auto buf = decomp->compile_code_string(code);
							if (!buf.is_empty()) {
								decomp->get_script_strings_from_buf(buf, r_strings, false);
							}
						}
					}
				}
			}
		}
	}
}
