// Shape Modeling Language (ShapeML)
// Copyright (C) 2019 Stefan Lienhard
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

#include "shapeml/util/hex_color.h"

#include <algorithm>
#include <cassert>

namespace shapeml {

namespace util {

static inline int Hex2Dec(char hex) {
  if (hex <= '9') {
    return hex - '0';
  } else if (hex <= 'F') {
    return hex - 'A' + 10;
  } else if (hex <= 'f') {
    return hex - 'a' + 10;
  } else {
    return -1;
  }
}

bool ValidateHexColor(const std::string& hex_str, bool allow_alpha) {
  if (!(hex_str.size() == 7 || (allow_alpha && hex_str.size() == 9))) {
    return true;
  }
  if (hex_str[0] != '#') {
    return true;
  }
  for (size_t i = 1; i < hex_str.size(); ++i) {
    if (Hex2Dec(hex_str[i]) < 0) {
      return true;
    }
  }
  return false;
}

Vec4 ParseHexColor(const std::string& hex_str) {
  const Scalar r = (Hex2Dec(hex_str[1]) * 16 + Hex2Dec(hex_str[2])) / 255.0;
  const Scalar g = (Hex2Dec(hex_str[3]) * 16 + Hex2Dec(hex_str[4])) / 255.0;
  const Scalar b = (Hex2Dec(hex_str[5]) * 16 + Hex2Dec(hex_str[6])) / 255.0;
  Scalar a = 1.0;
  if (hex_str.size() == 9) {
    a = (Hex2Dec(hex_str[7]) * 16 + Hex2Dec(hex_str[8])) / 255.0;
  }
  return Vec4(r, g, b, a);
}

static inline char Dec2Hex(int dec) {
  assert(dec >= 0 && dec < 16);
  return "0123456789ABCDEF"[dec & 0xF];
}

void HexColor2String(const Vec3f& color, std::string* hex_str) {
  hex_str->resize(7);
  hex_str->at(0) = '#';
  for (size_t i = 0; i < 3; ++i) {
    const int hex_val = std::min(static_cast<int>(color[i] * 256.0f), 255);
    hex_str->at(1 + 2 * i) = Dec2Hex(hex_val >> 4);
    hex_str->at(1 + 2 * i + 1) = Dec2Hex(hex_val & 0xf);
  }
}

}  // namespace util

}  // namespace shapeml
