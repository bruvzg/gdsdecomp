// Aseprite TGA Library
// Copyright (C) 2020  Igara Studio S.A.
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#include "tga.h"

#include <algorithm>
#include <cassert>

namespace tga {

template<typename T>
inline color_t get_pixel(Image& image, int x, int y) {
  return *(T*)(image.pixels + y*image.rowstride + x*image.bytesPerPixel);
}

Encoder::Encoder(FileInterface* file)
  : m_file(file)
{
}

void Encoder::writeHeader(const Header& header)
{
  write8(header.idLength);
  write8(header.colormapType);
  write8(header.imageType);
  write16(header.colormapOrigin);
  write16(header.colormapLength);
  write8(header.colormapDepth);
  write16(header.xOrigin);
  write16(header.yOrigin);
  write16(header.width);
  write16(header.height);
  write8(header.bitsPerPixel);
  write8(header.imageDescriptor);

  m_hasAlpha = (header.bitsPerPixel == 16 ||
                header.bitsPerPixel == 32);

  assert(header.colormapLength == header.colormap.size());

  // Write colormap
  if (header.colormapType == 1) {
    for (int i=0; i<header.colormap.size(); ++i) {
      color_t c = header.colormap[i];
      switch (header.colormapDepth) {
        case 15:
        case 16: write16Rgb(c); break;
        case 24: write24Rgb(c); break;
        case 32: write32Rgb(c); break;
      }
    }
  }
  else {
    assert(header.colormapLength == 0);
  }
}

void Encoder::writeFooter()
{
  const char* tga2_footer = "\0\0\0\0\0\0\0\0TRUEVISION-XFILE.\0";
  for (int i=0; i<26; ++i)
    write8(tga2_footer[i]);
}

void Encoder::writeImage(const Header& header,
                         const Image& image,
                         Delegate* delegate)
{
  const int w = header.width;
  const int h = header.height;

  m_iterator = details::ImageIterator(header, const_cast<Image&>(image));

  switch (header.imageType) {

    case tga::UncompressedIndexed:
    case tga::UncompressedGray:
      for (int y=0; y<h; ++y) {
        for (int x=0; x<w; ++x)
          write8(m_iterator.getPixel<uint8_t>());

        if (delegate && !delegate->notifyProgress(float(y) / float(h)))
          return;
      }
      break;

    case tga::RleIndexed:
    case tga::RleGray:
      for (int y=0; y<h; ++y) {
        writeRleScanline<uint8_t>(w, image, &Encoder::write8Color);

        if (delegate && !delegate->notifyProgress(float(y) / float(h)))
          return;
      }
      break;

    case tga::UncompressedRgb: {
      switch (header.bitsPerPixel) {
        case 15:
        case 16:
          for (int y=0; y<h; ++y) {
            for (int x=0; x<w; ++x)
              write16Rgb(m_iterator.getPixel<uint32_t>());

            if (delegate && !delegate->notifyProgress(float(y) / float(h)))
              return;
          }
          break;
        case 24:
          for (int y=0; y<h; ++y) {
            for (int x=0; x<w; ++x)
              write24Rgb(m_iterator.getPixel<uint32_t>());

            if (delegate && !delegate->notifyProgress(float(y) / float(h)))
              return;
          }
          break;
        case 32:
          for (int y=0; y<h; ++y) {
            for (int x=0; x<w; ++x)
              write32Rgb(m_iterator.getPixel<uint32_t>());

            if (delegate && !delegate->notifyProgress(float(y) / float(h)))
              return;
          }
          break;
        }
      }
      break;

    case tga::RleRgb:
      for (int y=0; y<h; ++y) {
        switch (header.bitsPerPixel) {
          case 15:
          case 16: writeRleScanline<uint32_t>(w, image, &Encoder::write16Rgb); break;
          case 24: writeRleScanline<uint32_t>(w, image, &Encoder::write24Rgb); break;
          case 32: writeRleScanline<uint32_t>(w, image, &Encoder::write32Rgb); break;
          default:
            assert(false);
            return;
        }
        if (delegate && !delegate->notifyProgress(float(y) / float(h)))
          return;
      }
      break;

  }
}

template<typename T>
void Encoder::writeRleScanline(const int w,
                               const Image& image,
                               void (Encoder::*writePixel)(color_t))
{
  int x = 0;
  while (x < w) {
    int count, offset;
    countRepeatedPixels<T>(w, image, x, offset, count);

    // Save a sequence of pixels with different colors
    while (offset > 0) {
      const int n = std::min(offset, 128);

      assert(n >= 1 && n <= 128);
      write8(static_cast<uint8_t>(n - 1));
      for (int i=0; i<n; ++i) {
        const color_t c = m_iterator.getPixel<T>();
        (this->*writePixel)(c);
      }
      offset -= n;
      x += n;
    }

    // Save a sequence of pixels with the same color
    while (count*image.bytesPerPixel > 1+image.bytesPerPixel) {
      const int n = std::min(count, 128);
      const color_t c = m_iterator.getPixel<T>();

      for (int i=1; i<n; ++i) {
#ifndef NDEBUG
        const color_t c2 =
#endif
          m_iterator.getPixel<T>();
        assert(c == c2);
      }

      assert(n >= 1 && n <= 128);
      write8(0x80 | static_cast<uint8_t>(n - 1));
      (this->*writePixel)(c);
      count -= n;
      x += n;
    }
  }
  assert(x == w);
}

template<typename T>
void Encoder::countRepeatedPixels(const int w,
                                  const Image& image, int x0,
                                  int& offset, int& count)
{
  auto it = m_iterator;

  for (int x=x0; x<w; ) {
    const color_t p = it.getPixel<T>();

    int u = x+1;
    auto next_it = it;

    for (; u<w; ++u) {
      const color_t q = it.getPixel<T>();
      if (p != q)
        break;
    }

    if ((u - x)*image.bytesPerPixel > 1+image.bytesPerPixel) {
      offset = x - x0;
      count = u - x;
      return;
    }

    ++x;
    it = next_it;
  }

  offset = w - x0;
  count = 0;
}

void Encoder::write8(uint8_t value)
{
  m_file->write8(value);
}

void Encoder::write16(uint16_t value)
{
  // Little endian
  m_file->write8(value & 0x00FF);
  m_file->write8((value & 0xFF00) >> 8);
}

void Encoder::write32(uint32_t value)
{
  // Little endian
  m_file->write8(value & 0xFF);
  m_file->write8((value >> 8) & 0xFF);
  m_file->write8((value >> 16) & 0xFF);
  m_file->write8((value >> 24) & 0xFF);
}

void Encoder::write16Rgb(color_t c)
{
  const uint8_t r = getr(c);
  const uint8_t g = getg(c);
  const uint8_t b = getb(c);
  const uint8_t a = geta(c);
  const uint16_t v =
    ((r>>3) << 10) |
    ((g>>3) << 5) |
    ((b>>3)) |
    (m_hasAlpha && a >= 128 ? 0x8000: 0); // TODO configurable threshold
  write16(v);
}

void Encoder::write24Rgb(color_t c)
{
  write8(getb(c));
  write8(getg(c));
  write8(getr(c));
}

void Encoder::write32Rgb(color_t c)
{
  write8(getb(c));
  write8(getg(c));
  write8(getr(c));
  write8(geta(c));
}

} // namespace tga
