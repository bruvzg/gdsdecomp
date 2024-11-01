#include "texture_exporter.h"
#include "compat/resource_compat_binary.h"
#include "compat/resource_loader_compat.h"
#include "compat/texture_loader_compat.h"
#include "core/error/error_list.h"
#include "core/io/file_access.h"
#include "utility/util_functions.h"
bool TextureExporter::handles_import(const String &importer, const String &resource_type) const {
	return importer == "texture" || importer == "bitmap" || resource_type == "BitMap" || resource_type == "CompressedTexture2D" || (resource_type == "Texture2D") || resource_type == "ImageTexture" || resource_type == "StreamTexture" || resource_type == "Texture";
}

bool get_bit(const Vector<uint8_t> &bitmask, int width, int p_x, int p_y) {
	int ofs = width * p_y + p_x;
	int bbyte = ofs / 8;
	int bbit = ofs % 8;

	return (bitmask[bbyte] & (1 << bbit)) != 0;
}
// Format is the same on V2 - V4
Ref<Image> TextureExporter::load_image_from_bitmap(const String p_path, Error *r_err) {
	Error err;
	Ref<Image> image;
	image.instantiate();
	ResourceFormatLoaderCompatBinary rlcb;
	auto res = ResourceCompatLoader::fake_load(p_path, "", &err);
	ERR_FAIL_COND_V_MSG(err != OK, Ref<Image>(), "Cannot open resource '" + p_path + "'.");

	String name;
	Vector2 size;
	Dictionary data;
	PackedByteArray bitmask;
	int width;
	int height;

	// Load the main resource, which should be the ImageTexture
	name = res->get("resource/name");
	data = res->get("data");
	bitmask = data.get("data", PackedByteArray());
	size = data.get("size", Vector2());
	width = size.width;
	height = size.height;
	image->initialize_data(width, height, false, Image::FORMAT_L8);

	if (!name.is_empty()) {
		image->set_name(name);
	}
	for (int i = 0; i < width; i++) {
		for (int j = 0; j < height; j++) {
			image->set_pixel(i, j, get_bit(bitmask, width, i, j) ? Color(1, 1, 1) : Color(0, 0, 0));
		}
	}
	ERR_FAIL_COND_V_MSG(image.is_null() || image->is_empty(), Ref<Image>(), "Failed to load image from " + p_path);
	return image;
}
Error TextureExporter::_convert_bitmap(const String &p_path, const String &dest_path, bool lossy = true) {
	String dst_dir = dest_path.get_base_dir();
	Error err;
	Ref<Image> img = load_image_from_bitmap(p_path, &err);
	// deprecated format
	if (err == ERR_UNAVAILABLE) {
		// TODO: Not reporting here because we can't get the deprecated format type yet,
		// implement functionality to pass it back
		print_line("Did not convert deprecated Bitmap resource " + p_path);
		return err;
	}
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to load bitmap " + p_path);
	err = gdre::ensure_dir(dst_dir);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to create dirs for " + dest_path);
	String dest_ext = dest_path.get_extension().to_lower();
	if (dest_ext == "jpg" || dest_ext == "jpeg") {
		err = gdre::save_image_as_jpeg(dest_path, img);
	} else if (dest_ext == "webp") {
		err = gdre::save_image_as_webp(dest_path, img, lossy);
	} else if (dest_ext == "png") {
		err = img->save_png(dest_path);
	} else {
		ERR_FAIL_V_MSG(ERR_FILE_BAD_PATH, "Invalid file name: " + dest_path);
	}

	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to save image " + dest_path + " from texture " + p_path);

	print_verbose("Converted " + p_path + " to " + dest_path);
	return OK;
}

Error TextureExporter::_convert_tex(const String &p_path, const String &dest_path, bool lossy, String &image_format) {
	Error err;
	String dst_dir = dest_path.get_base_dir();
	Ref<Texture2D> tex;
	ResourceFormatLoaderCompatTexture2D rlcb;
	tex = rlcb.custom_load(p_path, ResourceInfo::LoadType::NON_GLOBAL_LOAD, &err);
	// deprecated format
	if (err == ERR_UNAVAILABLE) {
		// TODO: Not reporting here because we can't get the deprecated format type yet,
		// implement functionality to pass it back
		print_line("Did not convert deprecated Texture resource " + p_path);
		return err;
	}
	ERR_FAIL_COND_V_MSG(err != OK || tex.is_null(), err, "Failed to load texture " + p_path);

	Ref<Image> img = tex->get_image();

	ERR_FAIL_COND_V_MSG(img.is_null(), ERR_PARSE_ERROR, "Failed to load image for texture " + p_path);
	ERR_FAIL_COND_V_MSG(img->is_empty(), ERR_FILE_EOF, "Image data is empty for texture " + p_path + ", not saving");
	err = gdre::ensure_dir(dst_dir);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to create dirs for " + dest_path);
	image_format = Image::get_format_name(img->get_format());
	if (img->is_compressed()) {
		err = img->decompress();
		if (err == ERR_UNAVAILABLE) {
			WARN_PRINT("Decompression not implemented yet for texture format " + image_format);
			return err;
		}
		ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to decompress " + image_format + " texture " + p_path);
	}
	String dest_ext = dest_path.get_extension().to_lower();
	if (dest_ext == "jpg" || dest_ext == "jpeg") {
		err = gdre::save_image_as_jpeg(dest_path, img);
	} else if (dest_ext == "webp") {
		err = gdre::save_image_as_webp(dest_path, img, lossy);
	} else if (dest_ext == "png") {
		err = img->save_png(dest_path);
	} else if (dest_ext == "tga") {
		err = gdre::save_image_as_tga(dest_path, img);
	} else {
		ERR_FAIL_V_MSG(ERR_FILE_BAD_PATH, "Invalid file name: " + dest_path);
	}

	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to save image " + dest_path + " from texture " + p_path);

	print_verbose("Converted " + p_path + " to " + dest_path);
	return OK;
}

Error TextureExporter::export_file(const String &out_path, const String &res_path) {
	Error err;
	auto recognized = TextureLoaderCompat::recognize(res_path, &err);
	if (recognized == TextureLoaderCompat::FORMAT_NOT_TEXTURE) {
		return _convert_bitmap(res_path, out_path);
	}
	String fmt_name;
	return _convert_tex(res_path, out_path, false, fmt_name);
}

Ref<ExportReport> TextureExporter::export_resource(const String &output_dir, Ref<ImportInfo> iinfo) {
	String path = iinfo->get_path();
	String source = iinfo->get_source_file();
	bool lossy = false;
	int ver_major = iinfo->get_ver_major();
	int ver_minor = iinfo->get_ver_minor();
	Ref<ExportReport> report = memnew(ExportReport(iinfo));

	// Sonic Colors Unlimited specific hack: We don't support atsc and nx-low formats, so we need to set the path to something else
	if (ver_major == 3 && ver_minor == 1) {
		String format_type = path.get_basename().get_extension();
		Vector<String> banned_types = { "atsc", "nx-low", "atsc-low" };
		if (banned_types.has(format_type)) {
			bool found = false;
			Vector<String> dest_files = iinfo->get_dest_files();
			if (dest_files.size() > 0) {
				String new_path = path.get_basename().get_basename() + ".s3tc" + path.get_extension();
				if (FileAccess::exists(new_path)) {
					path = new_path;
					found = true;
				}
				if (!found) {
					for (auto &dest : dest_files) {
						String fmt = dest.get_basename().get_extension();
						if (!banned_types.has(format_type) && FileAccess::exists(new_path)) {
							path = dest;
							found = true;
							break;
						}
					}
				}
			}
			if (!found) {
				report->set_error(ERR_UNAVAILABLE);
				report->set_message("Cannot convert custom SCU texture format");
				report->set_unsupported_format_type(format_type);
			}
		}
	}
	// for Godot 2.x resources, we can easily rewrite the metadata to point to a renamed file with a different extension,
	// but this isn't the case for 3.x and greater, so we have to save in the original (lossy) format.
	String source_ext = source.get_extension().to_lower();
	if (source_ext != "png" || ver_major == 2) {
		if (ver_major > 2) {
			if ((source_ext == "jpg" || source_ext == "jpeg")) {
				lossy = true;
				report->set_loss_type(ImportInfo::STORED_LOSSY);
			} else if (source_ext == "webp") {
				// if the engine is <3.4, it can't handle lossless encoded WEBPs
				if (ver_major < 4 && !(ver_major == 3 && ver_minor >= 4)) {
					lossy = true;
				}
				report->set_loss_type(ImportInfo::STORED_LOSSY);
			} else if (source_ext == "tga") {
				lossy = false;
			} else {
				iinfo->set_export_dest(iinfo->get_export_dest().get_basename() + ".png");
				// If this is version 3-4, we need to rewrite the import metadata to point to the new resource name
				// save it under .assets, which won't be picked up for import by the godot editor
				if (false) {
					// disable this for now
					// iinfo->set_source_file(iinfo->get_export_dest());
				} else {
					if (!iinfo->get_export_dest().replace("res://", "").begins_with(".assets")) {
						String prefix = ".assets";
						if (iinfo->get_export_dest().begins_with("res://")) {
							prefix = "res://.assets";
						}
						iinfo->set_export_dest(prefix.path_join(iinfo->get_export_dest().replace("res://", "")));
					}
				}
			}
		} else { //version 2
			iinfo->set_export_dest(iinfo->get_export_dest().get_basename() + ".png");
		}
	}
	report->set_new_source_path(iinfo->get_export_dest());

	Error err;
	String img_format = "bitmap";
	String dest_path = output_dir.path_join(iinfo->get_export_dest().replace("res://", ""));
	if (iinfo->get_importer() == "bitmap") {
		err = _convert_bitmap(path, dest_path, lossy);
	} else {
		err = _convert_tex(path, dest_path, lossy, img_format);
	}
	report->set_error(err);
	report->set_saved_path(dest_path);
	if (err == ERR_UNAVAILABLE) {
		report->set_unsupported_format_type(img_format);
		report->set_message("Decompression not implemented yet for texture format " + img_format);
		// Already reported in export functions above
	}
	ERR_FAIL_COND_V(err, report);
	// If lossy, also convert it as a png
	if (lossy) {
		String dest = iinfo->get_export_dest().get_basename() + ".png";
		if (!dest.replace("res://", "").begins_with(".assets")) {
			String prefix = ".assets";
			if (dest.begins_with("res://")) {
				prefix = "res://.assets";
			}
			dest = prefix.path_join(dest.replace("res://", ""));
		}
		iinfo->set_export_lossless_copy(dest);
		dest_path = output_dir.path_join(dest.replace("res://", ""));
		err = _convert_tex(path, dest_path, false, img_format);
		ERR_FAIL_COND_V(err != OK, report);
	}

	return report;
}

void TextureExporter::get_handled_types(List<String> *out) const {
	out->push_back("Texture2D");
	out->push_back("ImageTexture");
	out->push_back("StreamTexture");
	out->push_back("CompressedTexture2D");
	out->push_back("BitMap");
}

void TextureExporter::get_handled_importers(List<String> *out) const {
	out->push_back("texture");
	out->push_back("bitmap");
}