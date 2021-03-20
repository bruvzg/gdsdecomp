
#include "core/io/image.h"
#include "V2ImageParser.h"

namespace V2Image{
    enum Type{
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

void _advance_padding(FileAccess * f, uint32_t p_len) {
	uint32_t extra = 4 - (p_len % 4);
	if (extra < 4) {
		for (uint32_t i = 0; i < extra; i++) {
			f->get_8(); //pad to 32
		}
	}
}

String V2ImageParser::ImageV2_to_string(const Variant &r_v){
    
        Ref<Image> img = r_v;

        if (img.is_null() || img->is_empty()) {
            return String("Image()");
        }

        String imgstr = "Image( ";
        imgstr += itos(img->get_width());
        imgstr += ", " + itos(img->get_height());
        String subimgstr;
        subimgstr += ", " + itos(img->get_mipmap_count());
        subimgstr += ", ";

        switch (img->get_format()) {

            case Image::FORMAT_L8: subimgstr += "GRAYSCALE"; break;
            case Image::FORMAT_LA8: subimgstr += "GRAYSCALE_ALPHA"; break;
            case Image::FORMAT_RGB8: subimgstr += "RGB"; break;
            case Image::FORMAT_RGBA8: subimgstr += "RGBA"; break;
            case Image::FORMAT_DXT1: subimgstr += "BC1"; break;
            case Image::FORMAT_DXT3: subimgstr += "BC2"; break;
            case Image::FORMAT_DXT5: subimgstr += "BC3"; break;
            case Image::FORMAT_RGTC_R: subimgstr += "BC4"; break;
            case Image::FORMAT_RGTC_RG: subimgstr += "BC5"; break;
            case Image::FORMAT_PVRTC1_2: subimgstr += "PVRTC2"; break;
            case Image::FORMAT_PVRTC1_2A: subimgstr += "PVRTC2_ALPHA"; break;
            case Image::FORMAT_PVRTC1_4: subimgstr += "PVRTC4"; break;
            case Image::FORMAT_PVRTC1_4A: subimgstr += "PVRTC4_ALPHA"; break;
            case Image::FORMAT_ETC: subimgstr += "ETC"; break;
            //Hacks for no-longer supported iamge formats
            //the mipmap counts for these formats match the deprecated formats
            case Image::FORMAT_ETC2_R11:
                //reset substr
                subimgstr = ", " + itos(Image::get_image_required_mipmaps(img->get_width(), img->get_height(), Image::FORMAT_L8)) + ", ";
                subimgstr += "INTENSITY"; 
                break;
            case Image::FORMAT_ETC2_R11S: 
                subimgstr = ", " + itos(Image::get_image_required_mipmaps(img->get_width(), img->get_height(), Image::FORMAT_L8)) + ", ";
                subimgstr += "INDEXED"; break;
            case Image::FORMAT_ETC2_RG11: 
                subimgstr = ", " + itos(Image::get_image_required_mipmaps(img->get_width(), img->get_height(), Image::FORMAT_L8)) + ", ";
                subimgstr += "INDEXED_ALPHA"; break;
            case Image::FORMAT_ETC2_RG11S: 
                subimgstr = ", " + itos(Image::get_image_required_mipmaps(img->get_width(), img->get_height(), Image::FORMAT_PVRTC1_4A)) + ", ";
                subimgstr += "ATC"; break;
            case Image::FORMAT_ETC2_RGB8: 
                subimgstr = ", " + itos(Image::get_image_required_mipmaps(img->get_width(), img->get_height(), Image::FORMAT_BPTC_RGBA)) + ", ";
                subimgstr += "ATC_ALPHA_EXPLICIT"; break;
            case Image::FORMAT_ETC2_RGB8A1: 
                subimgstr = ", " + itos(Image::get_image_required_mipmaps(img->get_width(), img->get_height(), Image::FORMAT_BPTC_RGBA)) + ", ";
                subimgstr += "ATC_ALPHA_INTERPOLATED"; break;
            case Image::FORMAT_ETC2_RA_AS_RG: 
                subimgstr = ", " + itos(1) + ", ";
                subimgstr += "CUSTOM"; break;
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

Error V2ImageParser::parse_image_v2(FileAccess * f, Variant &r_v, bool hacks_for_dropped_fmt, bool convert_indexed){
	uint32_t encoding = f->get_32();
    Ref<Image> img;
    img.instance();
    
	if (encoding == V2Image::Type::IMAGE_ENCODING_EMPTY) {
        r_v = img;
		return OK;
	} else if (encoding == V2Image::Type::IMAGE_ENCODING_RAW) {
		uint32_t width = f->get_32();
		uint32_t height = f->get_32();
		uint32_t mipmaps = f->get_32();
		uint32_t format = f->get_32();
		Image::Format fmt;
		switch (format) {
            //convert old image format types to new ones
			case V2Image::Type::IMAGE_FORMAT_GRAYSCALE: {
				fmt = Image::FORMAT_L8;
			} break;
			case V2Image::Type::IMAGE_FORMAT_GRAYSCALE_ALPHA: {
				fmt = Image::FORMAT_LA8;
			} break;
			case V2Image::Type::IMAGE_FORMAT_RGB: {
				fmt = Image::FORMAT_RGB8;
			} break;
			case V2Image::Type::IMAGE_FORMAT_RGBA: {
				fmt = Image::FORMAT_RGBA8;
			} break;
			case V2Image::Type::IMAGE_FORMAT_BC1: {
				fmt = Image::FORMAT_DXT1;
			} break;
			case V2Image::Type::IMAGE_FORMAT_BC2: {
				fmt = Image::FORMAT_DXT3;
			} break;
			case V2Image::Type::IMAGE_FORMAT_BC3: {
				fmt = Image::FORMAT_DXT5;
			} break;
			case V2Image::Type::IMAGE_FORMAT_BC4: {
				fmt = Image::FORMAT_RGTC_R;
			} break;
			case V2Image::Type::IMAGE_FORMAT_BC5: {
				fmt = Image::FORMAT_RGTC_RG;
			} break;
			case V2Image::Type::IMAGE_FORMAT_PVRTC2: {
				fmt = Image::FORMAT_PVRTC1_2;
			} break;
			case V2Image::Type::IMAGE_FORMAT_PVRTC2_ALPHA: {
				fmt = Image::FORMAT_PVRTC1_2A;
			} break;
			case V2Image::Type::IMAGE_FORMAT_PVRTC4: {
				fmt = Image::FORMAT_PVRTC1_4;
			} break;
			case V2Image::Type::IMAGE_FORMAT_PVRTC4_ALPHA: {
				fmt = Image::FORMAT_PVRTC1_4A;
			} break;
			case V2Image::Type::IMAGE_FORMAT_ETC: {
				fmt = Image::FORMAT_ETC;
			} break;
            if (hacks_for_dropped_fmt){
                //Hacks for no-longer supported image formats
                //These formats do not match up, so when saving, we have to set mipmaps manually
                case V2Image::Type::IMAGE_FORMAT_INTENSITY: {
                    fmt = Image::FORMAT_ETC2_R11;
                } break;
                case V2Image::Type::IMAGE_FORMAT_INDEXED: {
                    fmt = Image::FORMAT_ETC2_R11S;
                } break;
                case V2Image::Type::IMAGE_FORMAT_INDEXED_ALPHA: {
                    fmt = Image::FORMAT_ETC2_RG11;
                } break;
                case V2Image::Type::IMAGE_FORMAT_ATC: {
                    fmt = Image::FORMAT_ETC2_RG11S;
                } break;
                case V2Image::Type::IMAGE_FORMAT_ATC_ALPHA_EXPLICIT: {
                    fmt = Image::FORMAT_ETC2_RGB8;
                } break;
                case V2Image::Type::IMAGE_FORMAT_ATC_ALPHA_INTERPOLATED: {
                    fmt = Image::FORMAT_ETC2_RGB8A1;
                } break;
                case V2Image::Type::IMAGE_FORMAT_CUSTOM: {
                    fmt = Image::FORMAT_ETC2_RA_AS_RG;
                } break;
            }
			default: {
				ERR_FAIL_V(ERR_FILE_CORRUPT);
			}
		}

		uint32_t datalen = f->get_32();
        
        //bool has_mipmaps = Image::get_image_required_mipmaps(width,height,fmt);
        //get_image_required_mipmaps
		Vector<uint8_t> imgdata;
		imgdata.resize(datalen);
		uint8_t * w = imgdata.ptrw();
		f->get_buffer(w, datalen);
		_advance_padding(f, datalen);

		if (convert_indexed && (format == 5 || format == 6)) {
			int p_width;
			if (format == V2Image::Type::IMAGE_FORMAT_INDEXED) {
				fmt = Image::FORMAT_RGB8;
				p_width = 3;
			} else if (format == V2Image::Type::IMAGE_FORMAT_INDEXED_ALPHA) {
				fmt = Image::FORMAT_RGBA8;
				p_width = 4;
			}

			Vector<uint8_t> new_imgdata;

			Vector<Vector<uint8_t> > palette;

			//palette data starts at end of pixel data, is equal to 256 * p_width
			for (int dataidx = width * height; dataidx < imgdata.size(); dataidx += p_width) {
				palette.push_back(imgdata.subarray(dataidx, dataidx + p_width - 1));
			}
			//pixel data is index into palette
			for (int i = 0; i < width * height; i++) {
				new_imgdata.append_array(palette[imgdata[i]]);
			}
			img->create(width, height, mipmaps > 0, fmt, new_imgdata);
		} else {
			img->create(width, height, mipmaps > 0, fmt, imgdata);
		}
	} else {
		//compressed
		Vector<uint8_t> data;
		data.resize(f->get_32());
		uint8_t * w = data.ptrw();
		f->get_buffer(w, data.size());

		if (encoding == V2Image::Type::IMAGE_ENCODING_LOSSY && Image::lossy_unpacker) {

			img = img->lossy_unpacker(data);
		} else if (encoding == V2Image::Type::IMAGE_ENCODING_LOSSLESS && Image::lossless_unpacker) {

			img = img->lossless_unpacker(data);
		}
		_advance_padding(f, data.size());
	}
    r_v = img;
	return OK;
}
