#ifndef TEXTURE_LOADER_COMPAT_H
#define TEXTURE_LOADER_COMPAT_H

#include "core/io/file_access.h"
#include "core/io/image.h"
#include "core/io/resource.h"
#include "core/io/resource_loader.h"
#include "core/object/ref_counted.h"
#include "core/templates/vector.h"
#include "scene/resources/texture.h"

class TextureLoaderCompat : public RefCounted {
public:
	enum TextureVersionType {
		FORMAT_NOT_TEXTURE = -1,
		FORMAT_V2_TEXTURE = 0, //tex
		FORMAT_V2_IMAGE_TEXTURE, //tex
		FORMAT_V2_ATLAS_TEXTURE, //atex
		FORMAT_V2_LARGE_TEXTURE, //ltex
		FORMAT_V2_CUBEMAP, //cbm
		FORMAT_V3_STREAM_TEXTURE2D, //stex
		FORMAT_V3_STREAM_TEXTURE3D, //tex3d
		FORMAT_V3_STREAM_TEXTUREARRAY, //texarr
		FORMAT_V4_COMPRESSED_TEXTURE2D, //ctex
		FORMAT_V4_COMPRESSED_TEXTURE3D, //ctex3d
		FORMAT_V4_COMPRESSED_TEXTURELAYERED //ctexarray, ccube, ccubearray
	};

private:
	GDCLASS(TextureLoaderCompat, RefCounted);

	Error _load_data_ctexlayered_v4(const String &p_path, Vector<Ref<Image>> &r_data, Image::Format &r_format, int &r_width, int &r_height, int &r_depth, int &r_type, bool &r_mipmaps) const;
	Error _load_layered_texture_v3(const String &p_path, Vector<Ref<Image>> &r_data, Image::Format &r_format, int &r_width, int &r_height, int &r_depth, bool &r_mipmaps) const;

	Error load_image_from_fileV3(Ref<FileAccess> f, int tw, int th, int tw_custom, int th_custom, int flags, int p_size_limit, uint32_t df, Ref<Image> &image) const;

	Error _load_data_ctex2d_v4(const String &p_path, int &tw, int &th, int &tw_custom, int &th_custom, Ref<Image> &image, int p_size_limit = 0) const;
	Error _load_data_stex2d_v3(const String &p_path, int &tw, int &th, int &tw_custom, int &th_custom, int &flags, Ref<Image> &image, int p_size_limit = 0) const;
	Error _load_data_tex_v2(const String &p_path, int &tw, int &th, int &tw_custom, int &th_custom, int &flags, Ref<Image> &image) const;

	Ref<CompressedTextureLayered> _load_texture_layered(const String p_path, Vector<Ref<Image>> &r_data, int &type, Error *r_err, int ver_major) const;
	Ref<CompressedTexture3D> _load_texture3d(const String p_path, Vector<Ref<Image>> &r_data, Error *r_err, int ver_major) const;
	Ref<CompressedTexture2D> _load_texture2d(const String &p_path, Ref<Image> &image, bool &size_override, int ver_major, Error *r_err) const;

protected:
	static void _bind_methods();

public:
	static TextureVersionType recognize(const String &p_path, Error *r_err);

	Ref<CompressedTextureLayered> load_texture_layered(const String p_path, Error *r_err);
	Ref<CompressedTexture3D> load_texture3d(const String p_path, Error *r_err);
	Ref<CompressedTexture2D> load_texture2d(const String p_path, Error *r_err);

	Vector<Ref<Image>> load_images_from_layered_tex(const String p_path, Error *r_err);
	Ref<Image> load_image_from_tex(const String p_path, Error *r_err);
	Ref<Image> load_image_from_bitmap(const String p_path, Error *r_err);
};

#endif // TEXTURE_LOADER_COMPAT_H
