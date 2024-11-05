/**************************************************************************/
/*  image_decompress_etcpak.cpp                                            */
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/

#include "image_decompress_etcpak.h"

#include "core/os/os.h"
#include "core/string/print_string.h"
#include "external/etcpak-decompress/BlockData.hpp"

void image_decompress_etc(Image *p_image) {
	uint64_t start_time = OS::get_singleton()->get_ticks_msec();

	int width = p_image->get_width();
	int height = p_image->get_height();

	// Compressed images' dimensions should be padded to the upper multiple of 4.
	// If they aren't, they need to be realigned (the actual data is correctly padded though).
	if (width % 4 != 0 || height % 4 != 0) {
		int new_width = width + (4 - (width % 4));
		int new_height = height + (4 - (height % 4));

		print_verbose(vformat("Compressed image's dimensions are not multiples of 4 (%dx%d), aligning to (%dx%d)", width, height, new_width, new_height));

		width = new_width;
		height = new_height;
	}

	Image::Format source_format = p_image->get_format();
	// the decoders always output RGBA8; we need to convert after the fact if it's RGB8
	constexpr const Image::Format start_format = Image::FORMAT_RGBA8;
	Image::Format target_format = Image::FORMAT_MAX;

	EtcFormat bcdec_format = EtcFormat::Etc1;

	if (!etcpack_decompressor_supports_format(source_format)) {
		ERR_FAIL_MSG("etcpak-decompress: Can't decompress unsupported format: " + Image::get_format_name(source_format) + ".");
	}
	switch (source_format) {
		case Image::FORMAT_ETC: {
			bcdec_format = EtcFormat::Etc1;
			target_format = Image::FORMAT_RGB8;
		} break;
		case Image::FORMAT_ETC2_RGB8: {
			bcdec_format = EtcFormat::Etc2_RGB;
			target_format = Image::FORMAT_RGB8;
		} break;
		case Image::FORMAT_ETC2_RGBA8: {
			bcdec_format = EtcFormat::Etc2_RGBA;
			target_format = Image::FORMAT_RGBA8;
		} break;
		case Image::FORMAT_ETC2_R11: {
			bcdec_format = EtcFormat::Etc2_R11;
			target_format = Image::FORMAT_RGBA8;
		} break;
		case Image::FORMAT_ETC2_RG11: {
			bcdec_format = EtcFormat::Etc2_RG11;
			target_format = Image::FORMAT_RGBA8;
		} break;
			// case Image::FORMAT_ETC2_R11S: {
			// 	bcdec_format = EtcFormat::Etc2_R11;
			// 	target_format = Image::FORMAT_RGBA8;
			// } break;
			// case Image::FORMAT_ETC2_RG11S: {
			// 	bcdec_format = EtcFormat::Etc2_RG11;
			// 	target_format = Image::FORMAT_RGBA8;
			// } break;
			// case Image::FORMAT_ETC2_RGB8A1: {
			// 	bcdec_format = EtcFormat::Etc2_RGBA8;
			// 	target_format = Image::FORMAT_RGBA8;
			// } break;
			// case Image::FORMAT_ETC2_RA_AS_RG: {
			// 	bcdec_format = EtcFormat::Etc2_RA_AS_RG;
			// 	target_format = Image::FORMAT_RGBA8;
			// } break;

		default:
			ERR_FAIL_MSG("etcpak-decompress: Can't decompress unsupported format: " + Image::get_format_name(source_format) + ".");
			break;
	}

	int mm_count = p_image->get_mipmap_count();
	int64_t target_size = Image::get_image_data_size(width, height, start_format, p_image->has_mipmaps());

	// Decompressed data.
	Vector<uint8_t> data;
	data.resize(target_size);
	uint8_t *wb = data.ptrw();

	// Source data.
	const uint8_t *rb = p_image->get_data().ptr();

	// Decompress mipmaps.
	for (int i = 0; i <= mm_count; i++) {
		int mipmap_w = 0, mipmap_h = 0;
		int64_t src_ofs = Image::get_image_mipmap_offset_and_dimensions(width, height, source_format, i, mipmap_w, mipmap_h);
		int64_t dst_ofs = Image::get_image_mipmap_offset(width, height, start_format, i);
		etcpak_decompress::decompress_image(bcdec_format, rb + src_ofs, wb + dst_ofs, mipmap_w, mipmap_h, target_size);
		// if (total_dest_written != target_size) {
		// 	ERR_FAIL_MSG("etcpak-decompress: Decompression failed.");
		// }
	}

	p_image->set_data(width, height, p_image->has_mipmaps(), start_format, data);

	// convert to RGB8 if needed.
	if (target_format == Image::FORMAT_RGB8) {
		p_image->convert(Image::FORMAT_RGBA8);
	}

	print_verbose(vformat("etcpak: Decompression of a %dx%d %s image with %d mipmaps took %d ms.",
			p_image->get_width(), p_image->get_height(), Image::get_format_name(source_format), p_image->get_mipmap_count(), OS::get_singleton()->get_ticks_msec() - start_time));
}

bool etcpack_decompressor_supports_format(Image::Format p_format) {
	switch (p_format) {
		case Image::FORMAT_ETC:
		case Image::FORMAT_ETC2_RGB8:
		case Image::FORMAT_ETC2_RGBA8:
		case Image::FORMAT_ETC2_R11:
		case Image::FORMAT_ETC2_RG11:
			return true;
		default:
			return false;
	}
}