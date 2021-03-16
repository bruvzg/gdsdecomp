#include "old_stream_texture.h"
#include "core/os/file_access.h"
enum FormatBits {
    FORMAT_MASK_IMAGE_FORMAT = (1 << 20) - 1,
    FORMAT_BIT_LOSSLESS = 1 << 20,
    FORMAT_BIT_LOSSY = 1 << 21,
    FORMAT_BIT_STREAM = 1 << 22,
    FORMAT_BIT_HAS_MIPMAPS = 1 << 23,
    FORMAT_BIT_DETECT_3D = 1 << 24,
    FORMAT_BIT_DETECT_SRGB = 1 << 25,
    FORMAT_BIT_DETECT_NORMAL = 1 << 26,
    FORMAT_BIT_DETECT_ROUGNESS = 1 << 27,
};

Ref<Image> StreamTextureV3::get_image(){
    return image;
}

void StreamTextureV3::_bind_methods(){
    ClassDB::bind_method("load", &StreamTextureV3::load);
    ClassDB::bind_method("get_image", &StreamTextureV3::get_image);
}


Error StreamTextureV3::_load_data(const String &p_path) {


    if (p_path.find("cloud_sky") != -1){
        printf("CLOUD SKY!!!");
    }
	FileAccess *f = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V(!f, ERR_CANT_OPEN);

	uint8_t header[4];
	f->get_buffer(header, 4);
	if (header[0] != 'G' || header[1] != 'D' || header[2] != 'S' || header[3] != 'T') {
		memdelete(f);
		ERR_FAIL_COND_V(header[0] != 'G' || header[1] != 'D' || header[2] != 'S' || header[3] != 'T', ERR_FILE_CORRUPT);
	}
    
	uint16_t tw = f->get_16();
	uint16_t tw_custom = f->get_16();
	uint16_t th = f->get_16();
	uint16_t th_custom = f->get_16();

	uint32_t flags = f->get_32(); //texture flags!
	uint32_t df = f->get_32(); //data format
    int p_size_limit = 0;
	/*
	print_line("width: " + itos(tw));
	print_line("height: " + itos(th));
	print_line("flags: " + itos(flags));
	print_line("df: " + itos(df));
	*/

	if (!(df & FORMAT_BIT_STREAM)) {
    // do something
	}
	if (df & FORMAT_BIT_LOSSLESS || df & FORMAT_BIT_LOSSY) {
		//look for a PNG or WEBP file inside

		int sw = tw;
		int sh = th;

		uint32_t mipmaps = f->get_32();
		uint32_t size = f->get_32();

		//print_line("mipmaps: " + itos(mipmaps));

		while (mipmaps > 1 && p_size_limit > 0 && (sw > p_size_limit || sh > p_size_limit)) {

			f->seek(f->get_position() + size);
			mipmaps = f->get_32();
			size = f->get_32();

			sw = MAX(sw >> 1, 1);
			sh = MAX(sh >> 1, 1);
			mipmaps--;
		}

		//mipmaps need to be read independently, they will be later combined
		Vector<Ref<Image> > mipmap_images;
		int total_size = 0;

		for (uint32_t i = 0; i < mipmaps; i++) {

			if (i) {
				size = f->get_32();
			}
			Vector<uint8_t> pv;
			pv.resize(size);
			{
				uint8_t *wr = pv.ptrw();
				f->get_buffer(wr, size);
			}

			Ref<Image> img;
			if (df & FORMAT_BIT_LOSSLESS) {
				img = Image::lossless_unpacker(pv);
			} else {
				img = Image::lossy_unpacker(pv);
			}

			if (img.is_null() || img->is_empty()) {
				memdelete(f);
				ERR_FAIL_COND_V(img.is_null() || img->is_empty(), ERR_FILE_CORRUPT);
			}

			total_size += img->get_data().size();

			mipmap_images.push_back(img);
		}

		//print_line("mipmap read total: " + itos(mipmap_images.size()));

		memdelete(f); //no longer needed

		if (mipmap_images.size() == 1) {
			image = mipmap_images[0];
			return OK;
		} else {
			Vector<uint8_t> img_data;
			img_data.resize(total_size);

			{
				uint8_t *wr = img_data.ptrw();

				int ofs = 0;
				for (int i = 0; i < mipmap_images.size(); i++) {
					Vector<uint8_t> id = mipmap_images[i]->get_data();
					int len = id.size();
					const uint8_t *r = id.ptr();
					copymem(&wr[ofs], r, len);
					ofs += len;
				}
			}
			image.instance();
			image->create(tw, th, true, mipmap_images[0]->get_format(), img_data);
			return OK;
		}

	} else {
		//look for regular format
		Image::Format format = (Image::Format)(df & FORMAT_MASK_IMAGE_FORMAT);
		bool mipmaps = df & FORMAT_BIT_HAS_MIPMAPS;

		if (!mipmaps) {
			int size = Image::get_image_data_size(tw, th, format, false);

			Vector<uint8_t> img_data;
			img_data.resize(size);

			{
				uint8_t *wr = img_data.ptrw();
				f->get_buffer(wr, size);
			}

			memdelete(f);
			image.instance();
			image->create(tw, th, false, format, img_data);
			return OK;
		} else {

			int sw = tw;
			int sh = th;

			int mipmaps2 = Image::get_image_required_mipmaps(tw, th, format);
			int total_size = Image::get_image_data_size(tw, th, format, true);
			int idx = 0;

			while (mipmaps2 > 1 && p_size_limit > 0 && (sw > p_size_limit || sh > p_size_limit)) {

				sw = MAX(sw >> 1, 1);
				sh = MAX(sh >> 1, 1);
				mipmaps2--;
				idx++;
			}

			int ofs = Image::get_image_mipmap_offset(tw, th, format, idx);

			if (total_size - ofs <= 0) {
				memdelete(f);
				ERR_FAIL_V(ERR_FILE_CORRUPT);
			}

			f->seek(f->get_position() + ofs);

			Vector<uint8_t> img_data;
			img_data.resize(total_size - ofs);

			{
				uint8_t *wr = img_data.ptrw();
				int bytes = f->get_buffer(wr, total_size - ofs);
				//print_line("requested read: " + itos(total_size - ofs) + " but got: " + itos(bytes));

				memdelete(f);

				int expected = total_size - ofs;
				if (bytes < expected) {
					//this is a compatibility workaround for older format, which saved less mipmaps2. It is still recommended the image is reimported.
					zeromem(wr + bytes, (expected - bytes));
				} else if (bytes != expected) {
					ERR_FAIL_V(ERR_FILE_CORRUPT);
				}
			}

			image.instance();
			image->create(sw, sh, true, format, img_data);

			return OK;
		}
	}

	return ERR_BUG; //unreachable
}

Error StreamTextureV3::load(const String &p_path){
	return _load_data(p_path);
}

