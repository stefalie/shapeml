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

namespace shapeml {

class Shape;

enum class ExportType { OBJ };

// TODO(stefalie): Add a booelan parameter 'triangulize' that allows toggling
// the output between triangles and polygons of aribtrary size.

class Exporter {
 public:
  Exporter(const Shape* shape, ExportType type, const std::string& file_name,
           const std::string& src_dir, bool merge_vertices);

 private:
  void ExportOBJ(const Shape* shape, const std::string& file_name,
                 const std::string& src_dir, bool merge_vertices);
};

}  // namespace shapeml
