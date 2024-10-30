// Aseprite TGA Library
// Copyright (C) 2020-2022  Igara Studio S.A.
//
// This file is released under the terms of the MIT license.
// Read LICENSE.txt for more information.

#include "tga.h"

namespace tga {
namespace details {

ImageIterator::ImageIterator()
  : m_image(nullptr)
{
}

ImageIterator::ImageIterator(const Header& header, Image& image)
  : m_image(&image)
  , m_x(header.leftToRight() ? 0: header.width-1)
  , m_y(header.topToBottom() ? 0: header.height-1)
  , m_w(header.width)
  , m_h(header.height)
  , m_dx(header.leftToRight() ? +1: -1)
  , m_dy(header.topToBottom() ? +1: -1)
{
  calcPtr();
}

bool ImageIterator::advance()
{
  m_x += m_dx;
  m_ptr += int(m_dx*m_image->bytesPerPixel);

  if ((m_dx < 0 && m_x < 0) ||
      (m_dx > 0 && m_x == m_w)) {
    m_x = (m_dx > 0 ? 0: m_w-1);
    m_y += m_dy;
    if ((m_dy < 0 && m_y < 0) ||
        (m_dy > 0 && m_y == m_h)) {
      return true;
    }
    calcPtr();
  }
  return false;
}

void ImageIterator::calcPtr()
{
  m_ptr =
    m_image->pixels
    + m_image->rowstride*m_y
    + m_image->bytesPerPixel*m_x;
}

} // namespace details
} // namespace tga
