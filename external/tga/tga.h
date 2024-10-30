// Aseprite TGA Library
// Copyright (C) 2020-2021  Igara Studio S.A.
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#ifndef TGA_TGA_H_INCLUDED
#define TGA_TGA_H_INCLUDED
#pragma once

#include <stdint.h>

#include <cstdio>
#include <string>
#include <vector>

namespace tga {

  enum ImageType {
    NoImage = 0,
    UncompressedIndexed = 1,
    UncompressedRgb = 2,
    UncompressedGray = 3,
    RleIndexed = 9,
    RleRgb = 10,
    RleGray = 11,
  };

  typedef uint32_t color_t;

  const color_t color_r_shift  = 0;
  const color_t color_g_shift  = 8;
  const color_t color_b_shift  = 16;
  const color_t color_a_shift  = 24;
  const color_t color_r_mask   = 0x000000ff;
  const color_t color_g_mask   = 0x0000ff00;
  const color_t color_b_mask   = 0x00ff0000;
  const color_t color_rgb_mask = 0x00ffffff;
  const color_t color_a_mask   = 0xff000000;

  inline uint8_t getr(color_t c) { return (c >> color_r_shift) & 0xff; }
  inline uint8_t getg(color_t c) { return (c >> color_g_shift) & 0xff; }
  inline uint8_t getb(color_t c) { return (c >> color_b_shift) & 0xff; }
  inline uint8_t geta(color_t c) { return (c >> color_a_shift) & 0xff; }
  inline color_t rgba(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
    return ((r << color_r_shift) |
            (g << color_g_shift) |
            (b << color_b_shift) |
            (a << color_a_shift));
  }

  class Colormap {
  public:
    Colormap() { }
    Colormap(const int n) : m_color(n) { }

    int size() const { return int(m_color.size()); }

    const color_t& operator[](int i) const {
      return m_color[i];
    }

    color_t& operator[](int i) {
      return m_color[i];
    }

    bool operator==(const Colormap& o) const {
      for (int i=0; i<int(m_color.size()); ++i) {
        if (m_color[i] != o[i])
          return false;
      }
      return true;
    }

    bool operator!=(const Colormap& o) const {
      return !operator==(o);
    }

  private:
    std::vector<color_t> m_color;
  };

  struct Image {
    uint8_t* pixels;
    uint32_t bytesPerPixel;
    uint32_t rowstride;
  };

  struct Header {
    uint8_t  idLength;
    uint8_t  colormapType;
    uint8_t  imageType;
    uint16_t colormapOrigin;
    uint16_t colormapLength;
    uint8_t  colormapDepth;
    uint16_t xOrigin;
    uint16_t yOrigin;
    uint16_t width;
    uint16_t height;
    uint8_t  bitsPerPixel;
    uint8_t  imageDescriptor;
    std::string imageId;
    Colormap colormap;

    bool leftToRight() const { return !(imageDescriptor & 0x10); }
    bool topToBottom() const { return (imageDescriptor & 0x20); }

    bool hasColormap() const {
      return (colormapLength > 0);
    }

    bool isRgb() const {
      return (imageType == UncompressedRgb ||
              imageType == RleRgb);
    }

    bool isIndexed() const {
      return (imageType == UncompressedIndexed ||
              imageType == RleIndexed);
    }

    bool isGray() const {
      return (imageType == UncompressedGray ||
              imageType == RleGray);
    }

    bool isUncompressed() const {
      return (imageType == UncompressedIndexed ||
              imageType == UncompressedRgb ||
              imageType == UncompressedGray);
    }

    bool isRle() const {
      return (imageType == RleIndexed ||
              imageType == RleRgb ||
              imageType == RleGray);
    }

    bool validColormapType() const {
      return
        // Indexed with palette
        (isIndexed() && bitsPerPixel == 8 && colormapType == 1) ||
        // Grayscale without palette
        (isGray() && bitsPerPixel == 8 && colormapType == 0) ||
        // Non-indexed without palette
        (bitsPerPixel > 8 && colormapType == 0);
    }

    bool valid() const {
      switch (imageType) {
        case UncompressedIndexed:
        case RleIndexed:
          return (bitsPerPixel == 8);
        case UncompressedRgb:
        case RleRgb:
          return (bitsPerPixel == 15 ||
                  bitsPerPixel == 16 ||
                  bitsPerPixel == 24 ||
                  bitsPerPixel == 32);
        case UncompressedGray:
        case RleGray:
          return (bitsPerPixel == 8);
      }
      return false;
    }

    // Returns the number of bytes per pixel needed in an image
    // created with this Header information.
    int bytesPerPixel() const {
      if (isRgb())
        return 4;
      else
        return 1;
    }

  };

  namespace details {

  class ImageIterator {
  public:
    ImageIterator();
    ImageIterator(const Header& header, Image& image);

    // Put a pixel value into the image and advance the iterator.
    template<typename T>
    bool putPixel(const T value) {
      *((T*)m_ptr) = value;
      return advance();
    }

    // Get one pixel from the image and advance the iterator.
    template<typename T>
    T getPixel() {
      T value = *((T*)m_ptr);
      advance();
      return value;
    }

  public:
    bool advance();
    void calcPtr();

    Image* m_image;
    int m_x, m_y;
    int m_w, m_h;
    int m_dx, m_dy;
    uint8_t* m_ptr;
  };

  } // namespace details

  class FileInterface {
  public:
    virtual ~FileInterface() { }

    // Returns true if we can read/write bytes from/into the file
    virtual bool ok() const = 0;

    // Current position in the file
    virtual size_t tell() = 0;

    // Jump to the given position in the file
    virtual void seek(size_t absPos) = 0;

    // Returns the next byte in the file or 0 if ok() = false
    virtual uint8_t read8() = 0;

    // Writes one byte in the file (or do nothing if ok() = false)
    virtual void write8(uint8_t value) = 0;
  };

  class StdioFileInterface : public tga::FileInterface {
  public:
    StdioFileInterface(FILE* file);
    bool ok() const override;
    size_t tell() override;
    void seek(size_t absPos) override;
    uint8_t read8() override;
    void write8(uint8_t value) override;

  private:
    FILE* m_file;
    bool m_ok;
  };

  class Delegate {
  public:
    virtual ~Delegate() {}
    // Must return true if we should continue the decoding process.
    virtual bool notifyProgress(double progress) = 0;
  };

  class Decoder {
  public:
    Decoder(FileInterface* file);

    bool hasAlpha() const { return m_hasAlpha; }

    // Reads the header + colormap (if the file has a
    // colormap). Returns true if the header is valid.
    bool readHeader(Header& header);

    // Reads the image.
    bool readImage(const Header& header,
                   Image& image,
                   Delegate* delegate = nullptr);

    // Fixes alpha channel for images with invalid alpha values (this
    // is optional, in case you want to preserve the original data
    // from the file, don't use it).
    void postProcessImage(const Header& header,
                          Image& image);

  private:
    void readColormap(Header& header);

    template<typename T>
    bool readUncompressedData(const int w, uint32_t (Decoder::*readPixel)());

    template<typename T>
    bool readRleData(const int w, uint32_t (Decoder::*readPixel)());

    uint8_t read8();
    uint16_t read16();
    uint32_t read32();

    color_t read32AsRgb();
    color_t read24AsRgb();
    color_t read16AsRgb();
    color_t read8Color() { return (color_t)read8(); }

    FileInterface* m_file;
    bool m_hasAlpha = false;
    details::ImageIterator m_iterator;
  };

  class Encoder {
  public:
    Encoder(FileInterface* file);

    // Writes the header + colormap
    void writeHeader(const Header& header);
    void writeImage(const Header& header,
                    const Image& image,
                    Delegate* delegate = nullptr);
    void writeFooter();

  private:
    template<typename T>
    void writeRleScanline(const int w, const Image& image,
                          void (Encoder::*writePixel)(color_t));

    template<typename T>
    void countRepeatedPixels(const int w, const Image& image,
                             int x0, int& offset, int& count);

    void write8(uint8_t value);
    void write16(uint16_t value);
    void write32(uint32_t value);

    void write8Color(color_t c) { write8(uint8_t(c)); }
    void write16Rgb(color_t c);
    void write24Rgb(color_t c);
    void write32Rgb(color_t c);

    FileInterface* m_file;
    bool m_hasAlpha = false;
    details::ImageIterator m_iterator;
  };

} // namespace tga

#endif
