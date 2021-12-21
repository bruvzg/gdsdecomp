
#include "image_parser_v2.h"
#include "core/io/image.h"

namespace V2Image {
enum Type {
	IMAGE_ENCODING_EMPTY = 0,
	IMAGE_ENCODING_RAW = 1,
	IMAGE_ENCODING_LOSSLESS = 2,
	IMAGE_ENCODING_LOSSY = 3,

	IMAGE_FORMAT_GRAYSCALE = 0,
	IMAGE_FORMAT_INTENSITY = 1,
	IMAGE_FORMAT_GRAYSCALE_ALPHA = 2,
	IMAGE_FORMAT_RGB = 3,
	IMAGE_FORMAT_RGBA = 4,
	IMAGE_FORMAT_INDEXED = 5,
	IMAGE_FORMAT_INDEXED_ALPHA = 6,
	IMAGE_FORMAT_BC1 = 7,
	IMAGE_FORMAT_BC2 = 8,
	IMAGE_FORMAT_BC3 = 9,
	IMAGE_FORMAT_BC4 = 10,
	IMAGE_FORMAT_BC5 = 11,
	IMAGE_FORMAT_PVRTC2 = 12,
	IMAGE_FORMAT_PVRTC2_ALPHA = 13,
	IMAGE_FORMAT_PVRTC4 = 14,
	IMAGE_FORMAT_PVRTC4_ALPHA = 15,
	IMAGE_FORMAT_ETC = 16,
	IMAGE_FORMAT_ATC = 17,
	IMAGE_FORMAT_ATC_ALPHA_EXPLICIT = 18,
	IMAGE_FORMAT_ATC_ALPHA_INTERPOLATED = 19,
	IMAGE_FORMAT_CUSTOM = 30
};
}

void _advance_padding(FileAccess *f, uint32_t p_len) {
	uint32_t extra = 4 - (p_len % 4);
	if (extra < 4) {
		for (uint32_t i = 0; i < extra; i++) {
			f->get_8(); //pad to 32
		}
	}
}

String ImageParserV2::image_v2_to_string(const Variant &r_v) {
	Ref<Image> img = r_v;

	if (img.is_null() || img->is_empty()) {
		return String("Image()");
	}

	String imgstr = "Image( ";
	imgstr += itos(img->get_width());
	imgstr += ", " + itos(img->get_height());
	String subimgstr = ", " + itos(img->get_mipmap_count()) + ", ";

	switch (img->get_format()) {
		case Image::FORMAT_L8:
			subimgstr += "GRAYSCALE";
			break;
		case Image::FORMAT_LA8:
			subimgstr += "GRAYSCALE_ALPHA";
			break;
		case Image::FORMAT_RGB8:
			subimgstr += "RGB";
			break;
		case Image::FORMAT_RGBA8:
			subimgstr += "RGBA";
			break;
		case Image::FORMAT_DXT1:
			subimgstr += "BC1";
			break;
		case Image::FORMAT_DXT3:
			subimgstr += "BC2";
			break;
		case Image::FORMAT_DXT5:
			subimgstr += "BC3";
			break;
		case Image::FORMAT_RGTC_R:
			subimgstr += "BC4";
			break;
		case Image::FORMAT_RGTC_RG:
			subimgstr += "BC5";
			break;
		case Image::FORMAT_PVRTC1_2:
			subimgstr += "PVRTC2";
			break;
		case Image::FORMAT_PVRTC1_2A:
			subimgstr += "PVRTC2_ALPHA";
			break;
		case Image::FORMAT_PVRTC1_4:
			subimgstr += "PVRTC4";
			break;
		case Image::FORMAT_PVRTC1_4A:
			subimgstr += "PVRTC4_ALPHA";
			break;
		case Image::FORMAT_ETC:
			subimgstr += "ETC";
			break;
		// Hacks for no-longer supported image formats
		// If hacks_for_dropped_fmt is true in parse_image_v2():
		// The mipmap counts for these will not be correct in the img data
		// the v4 formats in "get_image_required_mipmaps" map to the required mipmaps for the deprecated formats
		case Image::FORMAT_ETC2_R11:
			//reset substr
			subimgstr = ", " + itos(Image::get_image_required_mipmaps(img->get_width(), img->get_height(), Image::FORMAT_L8)) + ", ";
			subimgstr += "INTENSITY";
			break;
		case Image::FORMAT_ETC2_R11S:
			subimgstr = ", " + itos(Image::get_image_required_mipmaps(img->get_width(), img->get_height(), Image::FORMAT_L8)) + ", ";
			subimgstr += "INDEXED";
			break;
		case Image::FORMAT_ETC2_RG11:
			subimgstr = ", " + itos(Image::get_image_required_mipmaps(img->get_width(), img->get_height(), Image::FORMAT_L8)) + ", ";
			subimgstr += "INDEXED_ALPHA";
			break;
		case Image::FORMAT_ETC2_RG11S:
			subimgstr = ", " + itos(Image::get_image_required_mipmaps(img->get_width(), img->get_height(), Image::FORMAT_PVRTC1_4A)) + ", ";
			subimgstr += "ATC";
			break;
		case Image::FORMAT_ETC2_RGB8:
			subimgstr = ", " + itos(Image::get_image_required_mipmaps(img->get_width(), img->get_height(), Image::FORMAT_BPTC_RGBA)) + ", ";
			subimgstr += "ATC_ALPHA_EXPLICIT";
			break;
		case Image::FORMAT_ETC2_RGB8A1:
			subimgstr = ", " + itos(Image::get_image_required_mipmaps(img->get_width(), img->get_height(), Image::FORMAT_BPTC_RGBA)) + ", ";
			subimgstr += "ATC_ALPHA_INTERPOLATED";
			break;
		case Image::FORMAT_ETC2_RA_AS_RG:
			subimgstr = ", " + itos(1) + ", ";
			subimgstr += "CUSTOM";
			break;
		default: {
		}
	}
	imgstr += subimgstr;

	String s;

	Vector<uint8_t> data = img->get_data();
	int len = data.size();
	for (int i = 0; i < len; i++) {
		if (i > 0)
			s += ", ";
		s += itos(data[i]);
	}

	imgstr += ", " + s + " )";
	return imgstr;
}

Error ImageParserV2::write_image_v2_to_bin(FileAccess *f, const Variant &r_v, const PropertyHint p_hint) {
	Ref<Image> val = r_v;
	if (val.is_null() || val->is_empty()) {
		f->store_32(V2Image::IMAGE_ENCODING_EMPTY);
		return OK;
	}

	int encoding = V2Image::IMAGE_ENCODING_RAW;
	float quality = 0.7;

	if (val->get_format() <= Image::FORMAT_RGB565) {
		//can only compress uncompressed stuff
		if (p_hint == PROPERTY_HINT_IMAGE_COMPRESS_LOSSLESS && Image::png_packer) {
			encoding = V2Image::IMAGE_ENCODING_LOSSLESS;
		}
	}

	f->store_32(encoding); //raw encoding

	if (encoding == V2Image::IMAGE_ENCODING_RAW) {
		f->store_32(val->get_width());
		f->store_32(val->get_height());
		int mipmaps = val->get_mipmap_count();

		V2Image::Type fmt;
		switch (val->get_format()) {
			//convert old image format types to new ones
			case Image::FORMAT_L8: {
				fmt = V2Image::IMAGE_FORMAT_GRAYSCALE;
			} break;
			case Image::FORMAT_LA8: {
				fmt = V2Image::IMAGE_FORMAT_GRAYSCALE_ALPHA;
			} break;
			case Image::FORMAT_RGB8: {
				fmt = V2Image::IMAGE_FORMAT_RGB;
			} break;
			case Image::FORMAT_RGBA8: {
				fmt = V2Image::IMAGE_FORMAT_RGBA;
			} break;
			case Image::FORMAT_DXT1: {
				fmt = V2Image::IMAGE_FORMAT_BC1;
			} break;
			case Image::FORMAT_DXT3: {
				fmt = V2Image::IMAGE_FORMAT_BC2;
			} break;
			case Image::FORMAT_DXT5: {
				fmt = V2Image::IMAGE_FORMAT_BC3;
			} break;
			case Image::FORMAT_RGTC_R: {
				fmt = V2Image::IMAGE_FORMAT_BC4;
			} break;
			case Image::FORMAT_RGTC_RG: {
				fmt = V2Image::IMAGE_FORMAT_BC5;
			} break;
			case Image::FORMAT_PVRTC1_2: {
				fmt = V2Image::IMAGE_FORMAT_PVRTC2;
			} break;
			case Image::FORMAT_PVRTC1_2A: {
				fmt = V2Image::IMAGE_FORMAT_PVRTC2_ALPHA;
			} break;
			case Image::FORMAT_PVRTC1_4: {
				fmt = V2Image::IMAGE_FORMAT_PVRTC4;
			} break;
			case Image::FORMAT_PVRTC1_4A: {
				fmt = V2Image::IMAGE_FORMAT_PVRTC4_ALPHA;
			} break;
			case Image::FORMAT_ETC: {
				fmt = V2Image::IMAGE_FORMAT_ETC;
			} break;
			// Hacks for no-longer supported image formats
			// If hacks_for_dropped_fmt is true in parse_image_v2()
			// the v4 formats in "get_image_required_mipmaps" map to the required mipmaps for the deprecated formats
			case Image::FORMAT_ETC2_R11: {
				mipmaps = Image::get_image_required_mipmaps(val->get_width(), val->get_height(), Image::FORMAT_L8);
				fmt = V2Image::IMAGE_FORMAT_INTENSITY;
			} break;
			case Image::FORMAT_ETC2_R11S: {
				mipmaps = Image::get_image_required_mipmaps(val->get_width(), val->get_height(), Image::FORMAT_L8);
				fmt = V2Image::IMAGE_FORMAT_INDEXED;
			} break;
			case Image::FORMAT_ETC2_RG11: {
				mipmaps = Image::get_image_required_mipmaps(val->get_width(), val->get_height(), Image::FORMAT_L8);
				fmt = V2Image::IMAGE_FORMAT_INDEXED_ALPHA;
			} break;
			case Image::FORMAT_ETC2_RG11S: {
				mipmaps = Image::get_image_required_mipmaps(val->get_width(), val->get_height(), Image::FORMAT_PVRTC1_4A);
				fmt = V2Image::IMAGE_FORMAT_ATC;
			} break;
			case Image::FORMAT_ETC2_RGB8: {
				mipmaps = Image::get_image_required_mipmaps(val->get_width(), val->get_height(), Image::FORMAT_BPTC_RGBA);
				fmt = V2Image::IMAGE_FORMAT_ATC_ALPHA_EXPLICIT;
			} break;
			case Image::FORMAT_ETC2_RGB8A1: {
				mipmaps = Image::get_image_required_mipmaps(val->get_width(), val->get_height(), Image::FORMAT_BPTC_RGBA);
				fmt = V2Image::IMAGE_FORMAT_ATC_ALPHA_INTERPOLATED;
			} break;
			case Image::FORMAT_ETC2_RA_AS_RG: {
				mipmaps = 0;
				fmt = V2Image::IMAGE_FORMAT_CUSTOM;
			} break;

			default: {
				ERR_FAIL_V(ERR_FILE_CORRUPT);
			}
		}
		f->store_32(mipmaps);
		f->store_32(fmt);
		int dlen = val->get_data().size();
		f->store_32(dlen);
		f->store_buffer(val->get_data().ptr(), dlen);
		_advance_padding(f, dlen);
	} else {
		Vector<uint8_t> data;
		if (encoding == V2Image::IMAGE_ENCODING_LOSSY) {
			data = Image::webp_lossy_packer(val, quality);
		} else if (encoding == V2Image::IMAGE_ENCODING_LOSSLESS) {
			data = Image::png_packer(val);
		}

		int ds = data.size();
		f->store_32(ds);
		if (ds > 0) {
			f->store_buffer(data.ptr(), ds);
			_advance_padding(f, ds);
		}
	}
	return OK;
}

Error ImageParserV2::parse_image_v2(FileAccess *f, Variant &r_v, bool hacks_for_dropped_fmt, bool convert_indexed) {
	uint32_t encoding = f->get_32();
	Ref<Image> img;
	img.instantiate();

	if (encoding == V2Image::IMAGE_ENCODING_EMPTY) {
		r_v = img;
		return OK;
	} else if (encoding == V2Image::IMAGE_ENCODING_RAW) {
		uint32_t width = f->get_32();
		uint32_t height = f->get_32();
		uint32_t mipmaps = f->get_32();
		uint32_t format = f->get_32();
		Image::Format fmt = Image::FORMAT_MAX;
		switch (format) {
			//convert old image format types to new ones
			case V2Image::IMAGE_FORMAT_GRAYSCALE: {
				fmt = Image::FORMAT_L8;
			} break;
			case V2Image::IMAGE_FORMAT_GRAYSCALE_ALPHA: {
				fmt = Image::FORMAT_LA8;
			} break;
			case V2Image::IMAGE_FORMAT_RGB: {
				fmt = Image::FORMAT_RGB8;
			} break;
			case V2Image::IMAGE_FORMAT_RGBA: {
				fmt = Image::FORMAT_RGBA8;
			} break;
			case V2Image::IMAGE_FORMAT_BC1: {
				fmt = Image::FORMAT_DXT1;
			} break;
			case V2Image::IMAGE_FORMAT_BC2: {
				fmt = Image::FORMAT_DXT3;
			} break;
			case V2Image::IMAGE_FORMAT_BC3: {
				fmt = Image::FORMAT_DXT5;
			} break;
			case V2Image::IMAGE_FORMAT_BC4: {
				fmt = Image::FORMAT_RGTC_R;
			} break;
			case V2Image::IMAGE_FORMAT_BC5: {
				fmt = Image::FORMAT_RGTC_RG;
			} break;
			case V2Image::IMAGE_FORMAT_PVRTC2: {
				fmt = Image::FORMAT_PVRTC1_2;
			} break;
			case V2Image::IMAGE_FORMAT_PVRTC2_ALPHA: {
				fmt = Image::FORMAT_PVRTC1_2A;
			} break;
			case V2Image::IMAGE_FORMAT_PVRTC4: {
				fmt = Image::FORMAT_PVRTC1_4;
			} break;
			case V2Image::IMAGE_FORMAT_PVRTC4_ALPHA: {
				fmt = Image::FORMAT_PVRTC1_4A;
			} break;
			case V2Image::IMAGE_FORMAT_ETC: {
				fmt = Image::FORMAT_ETC;
			} break;

			// If this is a deprecated or unsupported format...
			default: {
				// Hacks for no-longer supported image formats
				// We change the format to something that V2 didn't have support for as a placeholder
				// This is just in the case of converting a bin resource to a text resource
				// It gets handled in the above converters
				if (hacks_for_dropped_fmt) {
					switch (format) {
						case V2Image::IMAGE_FORMAT_INTENSITY: {
							fmt = Image::FORMAT_ETC2_R11;
						} break;
						case V2Image::IMAGE_FORMAT_INDEXED: {
							fmt = Image::FORMAT_ETC2_R11S;
						} break;
						case V2Image::IMAGE_FORMAT_INDEXED_ALPHA: {
							fmt = Image::FORMAT_ETC2_RG11;
						} break;
						case V2Image::IMAGE_FORMAT_ATC: {
							fmt = Image::FORMAT_ETC2_RG11S;
						} break;
						case V2Image::IMAGE_FORMAT_ATC_ALPHA_EXPLICIT: {
							fmt = Image::FORMAT_ETC2_RGB8;
						} break;
						case V2Image::IMAGE_FORMAT_ATC_ALPHA_INTERPOLATED: {
							fmt = Image::FORMAT_ETC2_RGB8A1;
						} break;
						case V2Image::IMAGE_FORMAT_CUSTOM: {
							fmt = Image::FORMAT_ETC2_RA_AS_RG;
						} break;
							// We can't convert YUV format
					}
				} else {
					// We'll error out after we've skipped over the data
					fmt = Image::FORMAT_MAX;
				}
			}
		}

		uint32_t datalen = f->get_32();

		Vector<uint8_t> imgdata;
		imgdata.resize(datalen);
		uint8_t *w = imgdata.ptrw();
		f->get_buffer(w, datalen);
		_advance_padding(f, datalen);
		// This is for if we're saving the image as a png.
		if (convert_indexed && (format == 5 || format == 6 || format == 1)) {
			Vector<uint8_t> new_imgdata;

			if (format == V2Image::IMAGE_FORMAT_INTENSITY) {
				fmt = Image::FORMAT_RGBA8;
				new_imgdata.resize(datalen * 4);
				for (uint32_t i = 0; i < datalen; i++) {
					new_imgdata.write[i * 4] = 255;
					new_imgdata.write[i * 4 + 1] = 255;
					new_imgdata.write[i * 4 + 2] = 255;
					new_imgdata.write[i * 4 + 3] = imgdata[i];
				}
			} else {
				int p_width;
				if (format == V2Image::IMAGE_FORMAT_INDEXED) {
					fmt = Image::FORMAT_RGB8;
					p_width = 3;
				} else { //V2Image::IMAGE_FORMAT_INDEXED_ALPHA
					fmt = Image::FORMAT_RGBA8;
					p_width = 4;
				}

				Vector<Vector<uint8_t>> palette;

				//palette data starts at end of pixel data, is equal to 256 * p_width
				for (int dataidx = width * height; dataidx < imgdata.size(); dataidx += p_width) {
					palette.push_back(imgdata.slice(dataidx, dataidx + p_width));
				}
				//pixel data is index into palette
				for (uint32_t i = 0; i < width * height; i++) {
					new_imgdata.append_array(palette[imgdata[i]]);
				}
			}
			imgdata = new_imgdata;
		}
		//We wait until we've skipped all the data to do this
		ERR_FAIL_COND_V_MSG(fmt == Image::FORMAT_MAX, ERR_FILE_CORRUPT, "Can't convert deprecated image formats to new image formats!");
		img->create(width, height, mipmaps > 0, fmt, imgdata);
	} else {
		//compressed
		Vector<uint8_t> data;
		data.resize(f->get_32());
		uint8_t *w = data.ptrw();
		f->get_buffer(w, data.size());

		if (encoding == V2Image::IMAGE_ENCODING_LOSSY && Image::webp_unpacker) {
			img = img->webp_unpacker(data);
		} else if (encoding == V2Image::IMAGE_ENCODING_LOSSLESS && Image::png_unpacker) {
			img = img->png_unpacker(data);
		}
		_advance_padding(f, data.size());
	}
	r_v = img;
	return OK;
}
