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

#pragma once

#include <string>

#include "shapeml/geometry/vector_types.h"

namespace shapeml {

struct Material {
  bool operator==(const Material& other) const {
    return (name == other.name && color == other.color &&
            metallic == other.metallic && roughness == other.roughness &&
            reflectance == other.reflectance && texture == other.texture);
  }

  bool operator!=(const Material& other) const { return !operator==(other); }

  std::string name;
  Vec4 color = Vec4(0.6, 0.6, 0.6, 1.0);
  Scalar metallic = 0.0;
  Scalar roughness = 1.0;
  Scalar reflectance = 0.5;  // Leads to default F0 = 0.04.
  std::string texture;
};

}  // namespace shapeml
