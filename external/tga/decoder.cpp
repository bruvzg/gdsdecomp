// Aseprite TGA Library
// Copyright (C) 2020-2022  Igara Studio S.A.
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#include "tga.h"

#include <cassert>

namespace tga {

static inline uint8_t scale_5bits_to_8bits(uint8_t v) {
  assert(v >= 0 && v < 32);
  return (v << 3) | (v >> 2);
}

Decoder::Decoder(FileInterface* file)
  : m_file(file)
{
}

bool Decoder::readHeader(Header& header)
{
  header.idLength = read8();
  header.colormapType = read8();
  header.imageType = read8();
  header.colormapOrigin = read16();
  header.colormapLength  = read16();
  header.colormapDepth = read8();
  header.xOrigin = read16();
  header.yOrigin = read16();
  header.width = read16();
  header.height = read16();
  header.bitsPerPixel = read8();
  header.imageDescriptor = read8();

  // Invalid image size
  if (header.width == 0 ||
      header.height == 0)
    return false;

  // Skip ID string (idLength bytes)
  if (header.idLength > 0) {
    uint8_t i = header.idLength;
    while (i--) {
      uint8_t chr = m_file->read8();
      header.imageId.push_back(chr);
    }
  }

#if 0
  // In the best case the "alphaBits" should be valid, but there are
  // invalid TGA files out there which don't indicate the
  // "alphaBits" correctly, so they could be 0 and use the alpha
  // channel anyway on each pixel.
  int alphaBits = (header.imageDescriptor & 15);
  m_hasAlpha =
    (header.bitsPerPixel == 32 && alphaBits == 8) ||
    (header.bitsPerPixel == 16 && alphaBits == 1);
#else
  // So to detect if a 32bpp or 16bpp TGA image has alpha, we'll use
  // the "alpha histogram" in postProcessImage() to check if there are
  // different alpha values. If there is only one alpha value (all 0
  // or all 255), we create an opaque image anyway. The only exception
  // to this rule is when all pixels are black and transparent
  // (RGBA=0), that is the only case when an image is fully
  // transparent.
  //
  // Note: This same heuristic is used in apps like macOS Preview:
  // https://twitter.com/davidcapello/status/1242803110868893697
  m_hasAlpha =
    (header.bitsPerPixel == 32) ||
    (header.bitsPerPixel == 16);
#endif

  // Read colormap
  if (header.colormapType == 1)
    readColormap(header);

  return (header.validColormapType() &&
          header.valid());
}

void Decoder::readColormap(Header& header)
{
  header.colormap = Colormap(header.colormapLength);

  for (int i=0; i<header.colormapLength; ++i) {
    switch (header.colormapDepth) {

      case 15:
      case 16: {
        const uint16_t c = read16();
        header.colormap[i] =
          rgba(scale_5bits_to_8bits((c >> 10) & 0x1F),
               scale_5bits_to_8bits((c >> 5) & 0x1F),
               scale_5bits_to_8bits(c & 0x1F));
        break;
      }

      case 24:
      case 32: {
        const uint8_t b = read8();
        const uint8_t g = read8();
        const uint8_t r = read8();
        uint8_t a;
        if (header.colormapDepth == 32)
          a = read8();
        else
          a = 255;
        header.colormap[i] = rgba(r, g, b, a);
        break;
      }
    }
  }
}

bool Decoder::readImage(const Header& header,
                        Image& image,
                        Delegate* delegate)
{
  // Bit 4 means right-to-left, else left-to-right
  // Bit 5 means top-to-bottom, else bottom-to-top
  m_iterator = details::ImageIterator(header, image);

  for (int y=0; y<header.height; ++y) {
    switch (header.imageType) {

      case UncompressedIndexed:
        assert(header.bitsPerPixel == 8);
        if (readUncompressedData<uint8_t>(header.width, &Decoder::read8Color))
          return true;
        break;

      case UncompressedRgb:
        switch (header.bitsPerPixel) {
          case 15:
          case 16:
            if (readUncompressedData<uint32_t>(header.width, &Decoder::read16AsRgb))
              return true;
            break;
          case 24:
            if (readUncompressedData<uint32_t>(header.width, &Decoder::read24AsRgb))
              return true;
            break;
          case 32:
            if (readUncompressedData<uint32_t>(header.width, &Decoder::read32AsRgb))
              return true;
            break;
          default:
            assert(false);
            break;
        }
        break;

      case UncompressedGray:
        assert(header.bitsPerPixel == 8);
        if (readUncompressedData<uint8_t>(header.width, &Decoder::read8Color))
          return true;
        break;

      case RleIndexed:
        assert(header.bitsPerPixel == 8);
        if (readRleData<uint8_t>(header.width, &Decoder::read8Color))
          return true;
        break;

      case RleRgb:
        switch (header.bitsPerPixel) {
          case 15:
          case 16:
            if (readRleData<uint32_t>(header.width, &Decoder::read16AsRgb))
              return true;
            break;
          case 24:
            if (readRleData<uint32_t>(header.width, &Decoder::read24AsRgb))
              return true;
            break;
          case 32:
            if (readRleData<uint32_t>(header.width, &Decoder::read32AsRgb))
              return true;
            break;
          default:
            assert(false);
            break;
        }
        break;

      case RleGray:
        assert(header.bitsPerPixel == 8);
        if (readRleData<uint8_t>(header.width, &Decoder::read8Color))
          return true;
        break;
    }

    if (delegate &&
        !delegate->notifyProgress(float(y) / float(header.height))) {
      break;
    }
  }

  return true;
}

void Decoder::postProcessImage(const Header& header,
                               Image& image)
{
  // The post-processing is only for RGB images with possible invalid
  // alpha information.
  if (!header.isRgb() || !m_hasAlpha)
    return;

  bool transparentImage = true;
  bool blackImage = true;

  for (int y=0; y<header.height; ++y) {
    auto p = (uint32_t*)(image.pixels + y*image.rowstride);
    for (int x=0; x<header.width; ++x, ++p) {
      color_t c = *p;
      if (transparentImage &&
          geta(c) != 0) {
        transparentImage = false;
      }
      if (blackImage &&
          (getr(c) != 0 ||
           getg(c) != 0 ||
           getb(c) != 0)) {
        blackImage = false;
      }
    }
  }

  // If the image is fully transparent (all pixels with alpha=0) and
  // there are pixels with RGB != 0 (!blackImage), we have to make the
  // image completely opaque (alpha=255).
  if (transparentImage && !blackImage) {
    for (int y=0; y<header.height; ++y) {
      auto p = (uint32_t*)(image.pixels + y*image.rowstride);
      for (int x=0; x<header.width; ++x, ++p) {
        color_t c = *p;
        *p = rgba(getr(c),
                  getg(c),
                  getb(c), 255);
      }
    }
  }
}

template<typename T>
bool Decoder::readUncompressedData(const int w, color_t (Decoder::*readPixel)())
{
  for (int x=0; x<w && m_file->ok(); ++x) {
    if (m_iterator.putPixel<T>(static_cast<T>((this->*readPixel)())))
      return true;
  }
  return false;
}

// In the best case (TGA 2.0 spec) this should read just one
// scanline, but in old TGA versions (1.0) it was possible to save
// several scanlines with the same RLE data.
//
// Returns true when are are done.
template<typename T>
bool Decoder::readRleData(const int w, color_t (Decoder::*readPixel)())
{
  for (int x=0; x<w && m_file->ok(); ) {
    int c = read8();
    if (c & 0x80) {
      c = (c & 0x7f) + 1;
      x += c;
      const T pixel = static_cast<T>((this->*readPixel)());
      while (c-- > 0)
        if (m_iterator.putPixel<T>(pixel))
          return true;
    }
    else {
      ++c;
      x += c;
      while (c-- > 0) {
        if (m_iterator.putPixel<T>(static_cast<T>((this->*readPixel)())))
          return true;
      }
    }
  }
  return false;
}

uint8_t Decoder::read8()
{
  return m_file->read8();
}

// Reads a WORD (16 bits) using in little-endian byte ordering.
uint16_t Decoder::read16()
{
  uint8_t b1 = m_file->read8();
  uint8_t b2 = m_file->read8();

  if (m_file->ok()) {
    return ((b2 << 8) | b1); // Little endian
  }
  else
    return 0;
}

// Reads a DWORD (32 bits) using in little-endian byte ordering.
uint32_t Decoder::read32()
{
  const uint8_t b1 = m_file->read8();
  const uint8_t b2 = m_file->read8();
  const uint8_t b3 = m_file->read8();
  const uint8_t b4 = m_file->read8();

  if (m_file->ok()) {
    // Little endian
    return ((b4 << 24) | (b3 << 16) | (b2 << 8) | b1);
  }
  else
    return 0;
}

color_t Decoder::read32AsRgb()
{
  const uint8_t b = read8();
  const uint8_t g = read8();
  const uint8_t r = read8();
  uint8_t a = read8();
  if (!m_hasAlpha)
    a = 255;
  return rgba(r, g, b, a);
}

color_t Decoder::read24AsRgb()
{
  const uint8_t b = read8();
  const uint8_t g = read8();
  const uint8_t r = read8();
  return rgba(r, g, b, 255);
}

color_t Decoder::read16AsRgb()
{
  const uint16_t v = read16();
  uint8_t a = 255;
  if (m_hasAlpha) {
    if ((v & 0x8000) == 0)    // Transparent bit
      a = 0;
  }
  return rgba(scale_5bits_to_8bits((v >> 10) & 0x1F),
              scale_5bits_to_8bits((v >> 5) & 0x1F),
              scale_5bits_to_8bits(v & 0x1F),
              a);
}

} // namespace tga
