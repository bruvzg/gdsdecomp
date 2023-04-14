#include "texture_loader_compat.h"
#include "image_enum_compat.h"
#include "resource_loader_compat.h"
#include "webp_compat.h"

#include "utility/gdre_settings.h"

#include "core/io/file_access.h"

enum FormatBits {
	FORMAT_MASK_IMAGE_FORMAT = (1 << 20) - 1,
	FORMAT_BIT_LOSSLESS = 1 << 20, // v2
	FORMAT_BIT_PNG = 1 << 20, // v3
	FORMAT_BIT_LOSSY = 1 << 21, // v2
	FORMAT_BIT_WEBP = 1 << 21, // v3
	FORMAT_BIT_STREAM = 1 << 22,
	FORMAT_BIT_HAS_MIPMAPS = 1 << 23,
	FORMAT_BIT_DETECT_3D = 1 << 24,
	FORMAT_BIT_DETECT_SRGB = 1 << 25,
	FORMAT_BIT_DETECT_NORMAL = 1 << 26,
	FORMAT_BIT_DETECT_ROUGNESS = 1 << 27,
};

void TextureLoaderCompat::_bind_methods() {}

TextureLoaderCompat::TextureVersionType TextureLoaderCompat::recognize(const String &p_path, Error *r_err) {
	Error err;
	if (!r_err) {
		r_err = &err;
	}
	const String res_path = GDRESettings::get_singleton()->get_res_path(p_path);
	Ref<FileAccess> f = FileAccess::open(res_path, FileAccess::READ, r_err);

	ERR_FAIL_COND_V_MSG(*r_err != OK || f.is_null(), FORMAT_NOT_TEXTURE, "Can't open texture file " + p_path);

	uint8_t header[4];
	f->get_buffer(header, 4);
	//Only reading the header
	if (header[0] == 'G' && header[1] == 'D' && header[2] == 'S' && header[3] == 'T') {
		return TextureVersionType::FORMAT_V3_STREAM_TEXTURE2D;
	} else if (header[0] == 'G' && header[1] == 'D' && header[2] == '3' && header[3] == 'T') {
		return TextureVersionType::FORMAT_V3_STREAM_TEXTURE3D;
	} else if (header[0] == 'G' && header[1] == 'D' && header[2] == 'A' && header[3] == 'T') {
		return TextureVersionType::FORMAT_V3_STREAM_TEXTUREARRAY;
	} else if (header[0] == 'G' && header[1] == 'S' && header[2] == 'T' && header[3] == 'L') {
		String ext = p_path.get_extension();
		if (ext == "ctexarray" || ext == "ccube" || ext == "ccubearray") {
			return TextureVersionType::FORMAT_V4_COMPRESSED_TEXTURELAYERED;
		}
		return TextureVersionType::FORMAT_V4_COMPRESSED_TEXTURE3D;
	} else if (header[0] == 'G' && header[1] == 'S' && header[2] == 'T' && header[3] == '2') {
		return TextureVersionType::FORMAT_V4_COMPRESSED_TEXTURE2D;
	} else if ((header[0] == 'R' && header[1] == 'S' && header[2] == 'R' && header[3] == 'C') ||
			(header[0] == 'R' && header[1] == 'S' && header[2] == 'C' && header[3] == 'C')) {
		// check if this is a V2 texture
		ResourceFormatLoaderCompat rlc;
		Ref<ImportInfo> i_info = ImportInfo::load_from_file(res_path);

		if (*r_err == ERR_PRINTER_ON_FIRE) {
			// no import metadata
			*r_err = OK;
		} else if (*r_err) {
			ERR_FAIL_V_MSG(FORMAT_NOT_TEXTURE, "Can't open texture file " + p_path);
		}
		String type = i_info->get_type();
		if (type == "Texture") {
			return FORMAT_V2_TEXTURE;
		} else if (type == "ImageTexture") {
			return FORMAT_V2_IMAGE_TEXTURE;
		} else if (type == "AtlasTexture") {
			return FORMAT_V2_ATLAS_TEXTURE;
		} else if (type == "LargeTexture") {
			return FORMAT_V2_LARGE_TEXTURE;
		} else if (type == "CubeMap") {
			return FORMAT_V2_CUBEMAP;
		}
	}
	*r_err = ERR_FILE_UNRECOGNIZED;
	return FORMAT_NOT_TEXTURE;
}

Error TextureLoaderCompat::load_image_from_fileV3(Ref<FileAccess> f, int tw, int th, int tw_custom, int th_custom, int flags, int p_size_limit, uint32_t df, Ref<Image> &image) const {
	Image::Format format;
	if (!(df & FORMAT_BIT_STREAM)) {
		// do something??
	}
	if (df & FORMAT_BIT_PNG || df & FORMAT_BIT_WEBP) {
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

		// mipmaps need to be read independently, they will be later combined
		Vector<Ref<Image>> mipmap_images;
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
			if (df & FORMAT_BIT_PNG) {
				img = Image::png_unpacker(pv);
			} else {
				img = WebPCompat::webp_unpack_v2v3(pv);
			}
			ERR_FAIL_COND_V_MSG(img.is_null() || img->is_empty(), ERR_FILE_CORRUPT, "File is corrupt");

			if (i != 0) {
				img->convert(mipmap_images[0]->get_format()); // ensure the same format for all mipmaps
			}

			total_size += img->get_data().size();

			mipmap_images.push_back(img);
		}

		//print_line("mipmap read total: " + itos(mipmap_images.size()));
		format = mipmap_images[0]->get_format();

		if (mipmap_images.size() == 1) {
			image = mipmap_images[0];
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
					memcpy(&wr[ofs], r, len);
					ofs += len;
				}
			}
			image->initialize_data(tw, th, true, format, img_data);
		}
	} else {
		//look for regular format
		uint32_t v3_fmt = df & FORMAT_MASK_IMAGE_FORMAT;
		format = ImageEnumCompat::convert_image_format_enum_v3_to_v4(V3Image::Format(v3_fmt));
		if (format == Image::FORMAT_MAX) {
			// deprecated format
			ERR_FAIL_COND_V_MSG(v3_fmt > 0 && v3_fmt < V3Image::FORMAT_MAX, ERR_UNAVAILABLE,
					"Support for deprecated texture format " + ImageEnumCompat::get_v3_format_name(V3Image::Format(v3_fmt)) + " is unimplemented.");
			ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, "Texture is in an invalid format");
		}

		bool mipmaps = df & FORMAT_BIT_HAS_MIPMAPS;

		if (!mipmaps) {
			int size = Image::get_image_data_size(tw, th, format, false);

			Vector<uint8_t> img_data;
			img_data.resize(size);

			{
				uint8_t *wr = img_data.ptrw();
				f->get_buffer(wr, size);
			}

			image->initialize_data(tw, th, false, format, img_data);
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

			ERR_FAIL_COND_V_MSG(total_size - ofs <= 0, ERR_FILE_CORRUPT,
					"Failed to create image of format " + Image::get_format_name(format) + "from texture");

			f->seek(f->get_position() + ofs);

			Vector<uint8_t> img_data;
			img_data.resize(total_size - ofs);

			{
				uint8_t *wr = img_data.ptrw();
				int bytes = f->get_buffer(wr, total_size - ofs);
				//print_line("requested read: " + itos(total_size - ofs) + " but got: " + itos(bytes));

				int expected = total_size - ofs;
				if (bytes < expected) {
					// this is a compatibility workaround for older format, which saved less mipmaps2. It is still recommended the image is reimported.
					memset(wr + bytes, 0, (expected - bytes));
				}
				ERR_FAIL_COND_V(bytes != expected, ERR_FILE_CORRUPT);
			}
			image->initialize_data(sw, sh, true, format, img_data);
		}
	}
	ERR_FAIL_COND_V_MSG(image.is_null() || image->is_empty(), ERR_FILE_CORRUPT, "Failed to create image of format " + Image::get_format_name(format) + "from texture");
	return OK;
}

Ref<CompressedTexture2D> TextureLoaderCompat::_load_texture2d(const String &p_path, Ref<Image> &image, bool &size_override, int ver_major, Error *r_err) const {
	int lw, lh, lwc, lhc, lflags;
	Error err;
	Ref<CompressedTexture2D> texture;
	if (ver_major == 2) {
		err = _load_data_tex_v2(p_path, lw, lh, lwc, lhc, lflags, image);
	} else if (ver_major == 3) {
		err = _load_data_stex2d_v3(p_path, lw, lh, lwc, lhc, lflags, image);
	} else if (ver_major == 4) {
		err = _load_data_ctex2d_v4(p_path, lw, lh, lwc, lhc, image);
	} else {
		err = ERR_INVALID_PARAMETER;
	}
	if (r_err)
		*r_err = err;
	// deprecated format
	if (err == ERR_UNAVAILABLE) {
		return texture;
	}
	ERR_FAIL_COND_V_MSG(err != OK, texture, "Failed to load image from texture file " + p_path);
	texture.instantiate();
	texture->set("w", lwc ? lwc : lw);
	texture->set("h", lhc ? lhc : lh);
	texture->set("path_to_file", p_path);
	texture->set("format", image->get_format());
	size_override = lwc || lhc;
	// we no longer care about flags, apparently
	return texture;
}

Error TextureLoaderCompat::_load_data_tex_v2(const String &p_path, int &tw, int &th, int &tw_custom, int &th_custom, int &flags, Ref<Image> &image) const {
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);

	ERR_FAIL_COND_V_MSG(err != OK, err, "Cannot open file '" + p_path + "'.");

	ResourceLoaderCompat *loader = memnew(ResourceLoaderCompat);
	loader->fake_load = true;
	loader->local_path = p_path;
	loader->res_path = p_path;
	loader->convert_v2image_indexed = true;
	loader->hacks_for_deprecated_v2img_formats = false;
	err = loader->open_bin(f);
	ERR_RFLBC_COND_V_MSG_CLEANUP(err != OK, err, "Cannot open resource '" + p_path + "'.", loader);
	err = loader->load();
	// deprecated format
	if (err == ERR_UNAVAILABLE) {
		memdelete(loader);
		return ERR_UNAVAILABLE;
	}
	ERR_RFLBC_COND_V_MSG_CLEANUP(err != OK, err, "Cannot load resource '" + p_path + "'.", loader);
	tw_custom = 0;
	th_custom = 0;
	String name;
	Vector2 size;
	// Load the main resource, which should be the ImageTexture
	List<ResourceProperty> lrp = loader->internal_index_cached_properties[loader->res_path];
	for (List<ResourceProperty>::Element *PE = lrp.front(); PE; PE = PE->next()) {
		ResourceProperty pe = PE->get();
		if (pe.name == "resource/name") {
			name = pe.value;
		} else if (pe.name == "image") {
			image = pe.value;
		} else if (pe.name == "size") {
			size = pe.value;
		} else if (pe.name == "flags") {
			flags = (int)pe.value;
		}
	}
	ERR_RFLBC_COND_V_MSG_CLEANUP(image.is_null(), ERR_FILE_CORRUPT, "Cannot load resource '" + p_path + "'.", loader);
	image->set_name(name);
	tw = image->get_width();
	th = image->get_height();
	if (tw != size.width) {
		tw_custom = size.width;
	}
	if (th != size.height) {
		th_custom = size.height;
	}
	memdelete(loader);
	return OK;
}

Error TextureLoaderCompat::_load_data_stex2d_v3(const String &p_path, int &tw, int &th, int &tw_custom, int &th_custom, int &flags, Ref<Image> &image, int p_size_limit) const {
	Error err;

	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);
	ERR_FAIL_COND_V_MSG(f.is_null(), err, "Can't open image file for loading: " + p_path);
	uint8_t header[4];
	f->get_buffer(header, 4);
	// already checked

	tw = f->get_16();
	tw_custom = f->get_16();
	th = f->get_16();
	th_custom = f->get_16();

	flags = f->get_32(); // texture flags!
	uint32_t df = f->get_32(); // data format
	p_size_limit = 0;
	if (image.is_null()) {
		image.instantiate();
	}
	err = load_image_from_fileV3(f, tw, th, tw_custom, th_custom, flags, p_size_limit, df, image);
	ERR_FAIL_COND_V_MSG(err != OK, err, "Failed to load image from texture file " + p_path);

	/*
	print_line("width: " + itos(tw));
	print_line("height: " + itos(th));
	print_line("flags: " + itos(flags));
	print_line("df: " + itos(df));
	*/

	return OK;
}

Error TextureLoaderCompat::_load_data_ctex2d_v4(const String &p_path, int &tw, int &th, int &tw_custom, int &th_custom, Ref<Image> &image, int p_size_limit) const {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	uint8_t header[4];
	// already checked header
	f->get_buffer(header, 4);

	uint32_t version = f->get_32();

	if (version > CompressedTexture2D::FORMAT_VERSION) {
		ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, "Compressed texture file is too new.");
	}
	tw_custom = f->get_32();
	th_custom = f->get_32();
	uint32_t df = f->get_32(); //data format

	//skip reserved
	int mipmap_limit = int(f->get_32());
	//reserved
	f->get_32();
	f->get_32();
	f->get_32();

	if (!(df & FORMAT_BIT_STREAM)) {
		p_size_limit = 0;
	}
	image = CompressedTexture2D::load_image_from_file(f, p_size_limit);

	if (image.is_null() || image->is_empty()) {
		return ERR_CANT_OPEN;
	}
	if (!tw_custom) {
		tw = image->get_width();
	}
	if (!th_custom) {
		th = image->get_height();
	}
	return OK;
}

Error TextureLoaderCompat::_load_layered_texture_v3(const String &p_path, Vector<Ref<Image>> &r_data, Image::Format &r_format, int &r_width, int &r_height, int &r_depth, bool &r_mipmaps) const {
	Error err;
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ, &err);
	ERR_FAIL_COND_V_MSG(f.is_null(), err, "Cannot open file '" + p_path + "'.");

	uint8_t header[5] = { 0, 0, 0, 0, 0 };
	f->get_buffer(header, 4);
	// already checked

	int tw = f->get_32();
	int th = f->get_32();
	int td = f->get_32();
	int flags = f->get_32(); //texture flags!

	Image::Format format = ImageEnumCompat::convert_image_format_enum_v3_to_v4(V3Image::Format(f->get_32()));
	ERR_FAIL_COND_V_MSG(format == Image::FORMAT_MAX, ERR_FILE_CORRUPT, "Textured layer is in an invalid or deprecated format");

	uint32_t compression = f->get_32(); // 0 - lossless (PNG), 1 - vram, 2 - uncompressed

	for (int layer = 0; layer < td; layer++) {
		Ref<Image> image;
		image.instantiate();

		if (compression == 0) { // COMPRESSION_LOSSLESS
			// look for a PNG file inside

			int mipmaps = f->get_32();
			Vector<Ref<Image>> mipmap_images;

			for (int i = 0; i < mipmaps; i++) {
				uint32_t size = f->get_32();

				Vector<uint8_t> pv;
				pv.resize(size);
				{
					f->get_buffer(pv.ptrw(), size);
				}
				Ref<Image> img = Image::png_unpacker(pv);

				if (img.is_null() || img->is_empty() || format != img->get_format()) {
					ERR_FAIL_V(ERR_FILE_CORRUPT);
				}
				mipmap_images.push_back(img);
			}

			if (mipmap_images.size() == 1) {
				image = mipmap_images[0];

			} else {
				int total_size = Image::get_image_data_size(tw, th, format, true);
				Vector<uint8_t> img_data;
				img_data.resize(total_size);

				{
					int ofs = 0;
					for (int i = 0; i < mipmap_images.size(); i++) {
						Vector<uint8_t> id = mipmap_images[i]->get_data();
						int len = id.size();
						memcpy(&img_data.ptrw()[ofs], id.ptr(), len);
						ofs += len;
					}
				}

				image->initialize_data(tw, th, true, format, img_data);
				if (image->is_empty()) {
					ERR_FAIL_V(ERR_FILE_CORRUPT);
				}
			}

		} else {
			// look for regular format
			bool mipmaps = (flags & 1); // Texture::FLAG_MIPMAPS
			int total_size = Image::get_image_data_size(tw, th, format, mipmaps);

			Vector<uint8_t> img_data;
			img_data.resize(total_size);

			{
				int bytes = f->get_buffer(img_data.ptrw(), total_size);
				if (bytes != total_size) {
					ERR_FAIL_V(ERR_FILE_CORRUPT);
				}
			}
			image->initialize_data(tw, th, mipmaps, format, img_data);
		}
		r_data.push_back(image);
	}

	return OK;
}

Error TextureLoaderCompat::_load_data_ctexlayered_v4(const String &p_path, Vector<Ref<Image>> &r_data, Image::Format &r_format, int &r_width, int &r_height, int &r_depth, int &r_type, bool &r_mipmaps) const {
	Ref<FileAccess> f = FileAccess::open(p_path, FileAccess::READ);
	ERR_FAIL_COND_V_MSG(f.is_null(), ERR_CANT_OPEN, vformat("Unable to open file: %s.", p_path));

	uint8_t header[4];
	f->get_buffer(header, 4);
	ERR_FAIL_COND_V(header[0] != 'G' || header[1] != 'S' || header[2] != 'T' || header[3] != 'L', ERR_FILE_UNRECOGNIZED);

	// stored as stream textures (used for lossless and lossy compression)
	uint32_t version = f->get_32();

	if (version > 1) {
		ERR_FAIL_V_MSG(ERR_FILE_CORRUPT, "Stream texture file is too new.");
	}

	r_depth = f->get_32(); //depth or layer count (CompressedTextureLayered)
	r_type = f->get_32(); //type
	f->get_32(); //data format
	f->get_32(); // mipmap limit, pretty sure it's ignored?
	int mipmaps = f->get_32();
	f->get_32(); // ignored
	f->get_32(); // ignored

	r_mipmaps = mipmaps != 0;

	r_data.clear();

	String ext = p_path.get_extension();
	bool is_layered = false;
	if (ext == "stexarray" || ext == "scube" || ext == "scubearray" ||
			ext == "ctexarray" || ext == "ccube" || ext == "ccubearray") {
		is_layered = true;
	}

	int limit = is_layered ? r_depth : r_depth + mipmaps;
	for (int i = 0; i < limit; i++) {
		Ref<Image> image = CompressedTexture2D::load_image_from_file(f, 0);
		ERR_FAIL_COND_V(image.is_null() || image->is_empty(), ERR_CANT_OPEN);
		if (i == 0) {
			r_format = image->get_format();
			r_width = image->get_width();
			r_height = image->get_height();
		}
		r_data.push_back(image);
	}

	return OK;
}

Ref<CompressedTexture3D> TextureLoaderCompat::_load_texture3d(const String p_path, Vector<Ref<Image>> &r_data, Error *r_err, int ver_major) const {
	int lw, lh, ld, ltype;
	bool mipmaps;
	Image::Format fmt;
	Error err;
	Ref<CompressedTexture3D> texture;
	if (ver_major == 2) {
		err = ERR_UNAVAILABLE;
	} else if (ver_major == 3) {
		err = _load_layered_texture_v3(p_path, r_data, fmt, lw, lh, ld, mipmaps);
	} else if (ver_major == 4) {
		err = _load_data_ctexlayered_v4(p_path, r_data, fmt, lw, lh, ld, ltype, mipmaps);
	} else {
		err = ERR_INVALID_PARAMETER;
	}
	if (r_err)
		*r_err = err;
	ERR_FAIL_COND_V_MSG(err == ERR_UNAVAILABLE, texture, "V2 Texture3d conversion unimplemented");
	ERR_FAIL_COND_V_MSG(err != OK, texture, "Failed to load image from texture file " + p_path);
	texture.instantiate();
	texture->set("w", lw);
	texture->set("h", lh);
	texture->set("d", ld);
	texture->set("mipmaps", mipmaps);
	texture->set("format", fmt);
	texture->set("path_to_file", p_path);
	return texture;
}

Ref<CompressedTextureLayered> TextureLoaderCompat::_load_texture_layered(const String p_path, Vector<Ref<Image>> &r_data, int &type, Error *r_err, int ver_major) const {
	int lw, lh, ld;

	bool mipmaps;
	Image::Format fmt;
	Error err;
	if (ver_major == 2) {
		err = ERR_UNAVAILABLE;
	} else if (ver_major == 3) {
		err = _load_layered_texture_v3(p_path, r_data, fmt, lw, lh, ld, mipmaps);
		type = 0;
	} else if (ver_major == 4) {
		err = _load_data_ctexlayered_v4(p_path, r_data, fmt, lw, lh, ld, type, mipmaps);
	} else {
		err = ERR_INVALID_PARAMETER;
	}
	if (r_err)
		*r_err = err;
	ERR_FAIL_COND_V_MSG(err == ERR_UNAVAILABLE, Ref<Resource>(), "V2 TextureLayered conversion unimplemented");
	ERR_FAIL_COND_V_MSG(err != OK, Ref<Resource>(), "Failed to load image from texture file " + p_path);
	Ref<Resource> res;
	if (type == RS::TEXTURE_LAYERED_2D_ARRAY) {
		res = Ref<CompressedTexture2DArray>();
	} else if (type == RS::TEXTURE_LAYERED_CUBEMAP) {
		res = Ref<CompressedCubemap>();
	} else if (type == RS::TEXTURE_LAYERED_CUBEMAP_ARRAY) {
		res = Ref<CompressedCubemapArray>();
	}

	Ref<CompressedTextureLayered> texture = res;

	texture->set("w", lw);
	texture->set("h", lh);
	texture->set("mipmaps", mipmaps);
	texture->set("format", fmt);
	texture->set("layers", r_data.size());
	texture->set("path_to_file", p_path);
	return texture;
}

Ref<CompressedTextureLayered> TextureLoaderCompat::load_texture_layered(const String p_path, Error *r_err) {
	Error err;
	const String res_path = GDRESettings::get_singleton()->get_res_path(p_path);
	TextureLoaderCompat::TextureVersionType t = recognize(res_path, &err);

	if (t == FORMAT_NOT_TEXTURE) {
		if (r_err) {
			*r_err = err;
		}
		ERR_FAIL_COND_V_MSG(err == ERR_FILE_UNRECOGNIZED, Ref<Image>(), "File " + p_path + " is not a texture.");
		ERR_FAIL_COND_V(err != OK, Ref<Image>());
	}

	int ver_major = 0;
	switch (t) {
		case FORMAT_V2_LARGE_TEXTURE:
		case FORMAT_V2_CUBEMAP:
			ver_major = 2;
			break;
		case FORMAT_V3_STREAM_TEXTUREARRAY:
			ver_major = 3;
			break;
		case FORMAT_V4_COMPRESSED_TEXTURELAYERED:
			ver_major = 4;
			break;
		default:
			if (r_err) {
				*r_err = ERR_INVALID_PARAMETER;
			}
			ERR_FAIL_V_MSG(Ref<Image>(), "Not a layered texture: " + res_path);
			break;
	}
	Ref<CompressedTexture3D> texture;
	Vector<Ref<Image>> r_data;
	int type;
	texture = _load_texture_layered(res_path, r_data, type, &err, ver_major);
	if (r_err) {
		*r_err = err;
	}
	ERR_FAIL_COND_V_MSG(err != OK, texture, "Couldn't load texture");
	// put the texture into the rendering server
	RID texture_rid = RS::get_singleton()->texture_2d_layered_create(r_data, RS::TextureLayeredType(type));
	texture->set("texture", texture_rid);
	String local_path = GDRESettings::get_singleton()->localize_path(p_path);
	RS::get_singleton()->texture_set_path(texture_rid, local_path);
	texture->set_path(local_path, false);
	return texture;
}

Ref<CompressedTexture3D> TextureLoaderCompat::load_texture3d(const String p_path, Error *r_err) {
	Error err;
	const String res_path = GDRESettings::get_singleton()->get_res_path(p_path);
	TextureLoaderCompat::TextureVersionType t = recognize(res_path, &err);
	if (t == FORMAT_NOT_TEXTURE) {
		if (r_err) {
			*r_err = err;
		}
		ERR_FAIL_COND_V_MSG(err == ERR_FILE_UNRECOGNIZED, Ref<Image>(), "File " + p_path + " is not a texture.");
		ERR_FAIL_COND_V(err != OK, Ref<Image>());
	}

	int ver_major = 0;
	switch (t) {
		// TODO: what the hell does v2 use for 3d textures?
		case FORMAT_V2_TEXTURE:
			ver_major = 2;
			break;
		case FORMAT_V3_STREAM_TEXTURE3D:
			ver_major = 3;
			break;
		case FORMAT_V4_COMPRESSED_TEXTURE3D:
			ver_major = 4;
			break;
		default:
			if (r_err) {
				*r_err = ERR_INVALID_PARAMETER;
			}
			ERR_FAIL_V_MSG(Ref<Image>(), "Not a 3d image texture: " + p_path);
			break;
	}
	Ref<CompressedTexture3D> texture;
	Vector<Ref<Image>> r_data;

	texture = _load_texture3d(p_path, r_data, &err, ver_major);
	if (r_err) {
		*r_err = err;
	}
	ERR_FAIL_COND_V_MSG(err != OK, texture, "Couldn't load texture");
	// put the texture into the rendering server
	RID texture_rid = RS::get_singleton()->texture_3d_create(texture->get_format(),
			texture->get_width(),
			texture->get_height(),
			texture->get_depth(),
			texture->has_mipmaps(),
			r_data);
	texture->set("texture", texture_rid);
	String local_path = GDRESettings::get_singleton()->localize_path(p_path);
	RS::get_singleton()->texture_set_path(texture_rid, local_path);
	texture->set_path(local_path, false);
	return texture;
}

Ref<CompressedTexture2D> TextureLoaderCompat::load_texture2d(const String p_path, Error *r_err) {
	Error err;
	const String res_path = GDRESettings::get_singleton()->get_res_path(p_path);
	TextureLoaderCompat::TextureVersionType t = recognize(res_path, &err);
	if (t == FORMAT_NOT_TEXTURE) {
		if (r_err) {
			*r_err = err;
		}
		ERR_FAIL_COND_V_MSG(err == ERR_FILE_UNRECOGNIZED, Ref<Image>(), "File " + p_path + " is not a texture.");
		ERR_FAIL_COND_V(err != OK, Ref<Image>());
	}

	int ver_major = 0;
	switch (t) {
		case FORMAT_V2_IMAGE_TEXTURE:
			ver_major = 2;
			break;
		case FORMAT_V3_STREAM_TEXTURE2D:
			ver_major = 3;
			break;
		case FORMAT_V4_COMPRESSED_TEXTURE2D:
			ver_major = 4;
			break;
		default:
			if (r_err) {
				*r_err = ERR_INVALID_PARAMETER;
			}
			ERR_FAIL_V_MSG(Ref<Image>(), "Not a 2d image texture: " + res_path);
			break;
	}
	Ref<CompressedTexture2D> texture;
	Ref<Image> image;
	bool size_override = false;
	texture = _load_texture2d(res_path, image, size_override, ver_major, &err);
	if (r_err) {
		*r_err = err;
	}
	if (err == ERR_UNAVAILABLE) {
		return texture;
	}
	ERR_FAIL_COND_V_MSG(err != OK, texture, "Couldn't load texture " + res_path);
	// put the texture into the rendering server
	RID texture_rid = RS::get_singleton()->texture_2d_create(image);
	texture->set("texture", texture_rid);
	if (size_override) {
		RS::get_singleton()->texture_set_size_override(texture_rid, texture->get_width(), texture->get_height());
	}
	String local_path = GDRESettings::get_singleton()->localize_path(p_path);
	RS::get_singleton()->texture_set_path(texture_rid, local_path);
	texture->set_path(local_path, false);
	return texture;
}

// TODO: What to do with this?
Vector<Ref<Image>> TextureLoaderCompat::load_images_from_layered_tex(const String p_path, Error *r_err) {
	Error err;
	Vector<Ref<Image>> data;
	const String res_path = GDRESettings::get_singleton()->get_res_path(p_path);

	TextureLoaderCompat::TextureVersionType t = recognize(res_path, &err);
	if (t == FORMAT_NOT_TEXTURE) {
		if (r_err) {
			*r_err = err;
		}
		ERR_FAIL_COND_V_MSG(err == ERR_FILE_UNRECOGNIZED, data, "File " + res_path + " is not a texture.");
		ERR_FAIL_COND_V(err != OK, data);
	}
	int type;
	Ref<Resource> res;
	switch (t) {
		// TODO: what the hell does v2 use for 3d textures?
		case FORMAT_V2_TEXTURE:
			_load_texture3d(res_path, data, &err, 2);
			break;
		case FORMAT_V3_STREAM_TEXTURE3D:
			_load_texture3d(res_path, data, &err, 3);
			break;
		case FORMAT_V4_COMPRESSED_TEXTURE3D:
			_load_texture3d(res_path, data, &err, 4);
			break;
		case FORMAT_V2_LARGE_TEXTURE:
		case FORMAT_V2_CUBEMAP:
			_load_texture_layered(res_path, data, type, &err, 2);
			break;
		case FORMAT_V3_STREAM_TEXTUREARRAY:
			_load_texture_layered(res_path, data, type, &err, 3);
			break;
		case FORMAT_V4_COMPRESSED_TEXTURELAYERED:
			_load_texture_layered(res_path, data, type, &err, 4);
			break;
		default:
			if (r_err) {
				*r_err = ERR_INVALID_PARAMETER;
			}
			ERR_FAIL_V_MSG(data, "Not a 3d image texture: " + res_path);
			break;
	}

	if (r_err) {
		*r_err = err;
	}
	ERR_FAIL_COND_V_MSG(err != OK, data, "Texture " + res_path + " could not be loaded");
	return data;
}

Ref<Image> TextureLoaderCompat::load_image_from_tex(const String p_path, Error *r_err) {
	Error err;
	const String res_path = GDRESettings::get_singleton()->get_res_path(p_path);
	TextureLoaderCompat::TextureVersionType t = recognize(res_path, &err);
	if (t == FORMAT_NOT_TEXTURE) {
		if (r_err) {
			*r_err = err;
		}
		ERR_FAIL_COND_V_MSG(err == ERR_FILE_UNRECOGNIZED, Ref<Image>(), "File " + p_path + " is not a texture.");
		ERR_FAIL_COND_V(err != OK, Ref<Image>());
	}

	int ver_major = 0;
	switch (t) {
		case FORMAT_V2_IMAGE_TEXTURE:
			ver_major = 2;
			break;
		case FORMAT_V3_STREAM_TEXTURE2D:
			ver_major = 3;
			break;
		case FORMAT_V4_COMPRESSED_TEXTURE2D:
			ver_major = 4;
			break;
		default:
			if (r_err) {
				*r_err = ERR_INVALID_PARAMETER;
			}
			ERR_FAIL_V_MSG(Ref<Image>(), "Not a 2d image texture: " + res_path);
			break;
	}
	Ref<Image> image;
	image.instantiate();
	bool size_override;

	_load_texture2d(res_path, image, size_override, ver_major, &err);
	if (r_err) {
		*r_err = err;
	}
	// deprecated format
	if (*r_err == ERR_UNAVAILABLE) {
		return Ref<Image>();
	}

	ERR_FAIL_COND_V_MSG(err || image.is_null(), Ref<Image>(), "Failed to load image from " + p_path);
	if (image->is_empty()) {
		WARN_PRINT("Image data is empty: " + p_path);
	}
	return image;
}

bool get_bit(const Vector<uint8_t> &bitmask, int width, int p_x, int p_y) {
	int ofs = width * p_y + p_x;
	int bbyte = ofs / 8;
	int bbit = ofs % 8;

	return (bitmask[bbyte] & (1 << bbit)) != 0;
}

// Format is the same on V2 - V4
Ref<Image> TextureLoaderCompat::load_image_from_bitmap(const String p_path, Error *r_err) {
	Error err;
	const String res_path = GDRESettings::get_singleton()->get_res_path(p_path);
	Ref<FileAccess> f = FileAccess::open(res_path, FileAccess::READ, &err);
	ERR_FAIL_COND_V_MSG(err != OK, Ref<Image>(), "Cannot open file '" + p_path + "'.");

	Ref<Image> image;
	image.instantiate();
	ResourceLoaderCompat *loader = memnew(ResourceLoaderCompat);
	loader->fake_load = true;
	loader->local_path = p_path;
	loader->res_path = p_path;
	loader->convert_v2image_indexed = true;
	loader->hacks_for_deprecated_v2img_formats = false;
	err = loader->open_bin(f);
	ERR_RFLBC_COND_V_MSG_CLEANUP(err != OK, Ref<Image>(), "Cannot open resource '" + p_path + "'.", loader);

	err = loader->load();
	ERR_RFLBC_COND_V_MSG_CLEANUP(err != OK, Ref<Image>(), "Cannot load resource '" + p_path + "'.", loader);

	String name;
	Vector2 size;
	Dictionary data;
	PackedByteArray bitmask;
	int width;
	int height;

	// Load the main resource, which should be the ImageTexture
	List<ResourceProperty> lrp = loader->internal_index_cached_properties[loader->res_path];
	for (List<ResourceProperty>::Element *PE = lrp.front(); PE; PE = PE->next()) {
		ResourceProperty pe = PE->get();
		if (pe.name == "resource/name") {
			name = pe.value;
		} else if (pe.name == "data") {
			data = pe.value;
		}
	}
	ERR_RFLBC_COND_V_MSG_CLEANUP(!data.has("data") || !data.has("size"), Ref<Image>(), "Cannot load bitmap '" + p_path + "'.", loader);
	memdelete(loader);

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
