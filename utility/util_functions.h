#ifndef GDRE_UTIL_FUNCTIONS_H
#define GDRE_UTIL_FUNCTIONS_H

#include "external/toojpeg/toojpeg.h"

#include "core/io/dir_access.h"
#include "core/io/file_access.h"
#include "core/io/resource.h"
namespace gdreutil {
static Vector<String> get_recursive_dir_list(const String dir, const Vector<String> &wildcards = Vector<String>(), const bool absolute = true, const String rel = "", const bool &res = false) {
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

static Ref<FileAccess> _____tmp_file;

static Error save_image_as_webp(const String &p_path, const Ref<Image> &p_img, bool lossy = false) {
	Ref<Image> source_image = p_img->duplicate();
	Vector<uint8_t> buffer;
	if (lossy) {
		buffer = Image::webp_lossy_packer(source_image, 1);
	} else {
		buffer = Image::webp_lossless_packer(source_image);
	}
	Error err;
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err, err, vformat("Can't save WEBP at path: '%s'.", p_path));

	if (file->get_error() != OK && file->get_error() != ERR_FILE_EOF) {
		return ERR_CANT_CREATE;
	}
	return OK;
}

static Error save_image_as_jpeg(const String &p_path, const Ref<Image> &p_img) {
	Vector<uint8_t> buffer;
	Ref<Image> source_image = p_img->duplicate();

	if (source_image->is_compressed()) {
		source_image->decompress();
	}

	ERR_FAIL_COND_V(source_image->is_compressed(), FAILED);

	int width = source_image->get_width();
	int height = source_image->get_height();
	bool isRGB;
	if (source_image->detect_alpha()) {
		WARN_PRINT("Alpha channel detected, will not be saved to jpeg...");
	}
	switch (source_image->get_format()) {
		case Image::FORMAT_L8:
			isRGB = false;
			break;
		case Image::FORMAT_LA8:
			isRGB = false;
			source_image->convert(Image::FORMAT_L8);
			break;
		case Image::FORMAT_RGB8:
			isRGB = true;
			break;
		case Image::FORMAT_RGBA8:
		default:
			source_image->convert(Image::FORMAT_RGB8);
			isRGB = true;
			break;
	}

	const Vector<uint8_t> image_data = source_image->get_data();
	const uint8_t *reader = image_data.ptr();
	// we may be passed a buffer with existing content we're expected to append to
	Error err;
	_____tmp_file = FileAccess::open(p_path, FileAccess::WRITE, &err);
	ERR_FAIL_COND_V_MSG(err, err, vformat("Can't save JPEG at path: '%s'.", p_path));

	bool success = false;
	{ // scope writer lifetime
		success = TooJpeg::writeJpeg([](unsigned char oneByte) {
			_____tmp_file->store_8(oneByte);
		},
				image_data.ptr(), width, height, isRGB, 100, false);
		if (!success) {
			_____tmp_file = Ref<FileAccess>();
		}
		ERR_FAIL_COND_V_MSG(!success, ERR_BUG, "Failed to convert image to JPEG");
	}

	if (_____tmp_file->get_error() != OK && _____tmp_file->get_error() != ERR_FILE_EOF) {
		_____tmp_file = Ref<FileAccess>();
		;
		return ERR_CANT_CREATE;
	}
	_____tmp_file->flush();
	_____tmp_file = Ref<FileAccess>();
	return OK;
}

} //namespace gdreutil

#endif //GDRE_UTIL_FUNCTIONS_H
