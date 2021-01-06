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
#include <vector>

namespace shapeml {

namespace geometry {

struct OBJMaterial {
  // Variables for the colors with default value in case a color should not
  // The default values for the colors and the shininess are also the OpenGL
  // defaults.
  OBJMaterial() : shininess(0.0), opacity(1.0) {
    ambient[0] = ambient[1] = ambient[2] = 0.2;
    diffuse[0] = diffuse[1] = diffuse[2] = 0.8;
    specular[0] = specular[1] = specular[2] = 0.0;
  }

  std::string name;

  double ambient[3];
  double diffuse[3];
  double specular[3];
  double shininess;
  double opacity;

  std::string diffuse_map;
  std::string ambient_map;
  std::string specular_map;
  std::string bump_map;
  std::string reflection_map;
};

struct FaceVertex {
  int vertex_index;
  int normal_index = -1;
  int tex_coord_index = -1;
};

typedef std::vector<FaceVertex> FaceVertexArray;
typedef std::vector<FaceVertexArray> FaceSet;

struct OBJMesh {
  // Vector of vectors full of faces. All faces in an inner vector use the same
  // material. The index into the outer vector shall be the material index.
  std::vector<FaceSet> face_sets;
  std::vector<OBJMaterial> materials;

  std::vector<double> vertices;
  std::vector<double> normals;
  std::vector<double> tex_coords;

  std::string name;
};

class OBJLoader {
 public:
  OBJMesh* Load(const std::string& file_name, bool load_mtl);

 private:
  typedef void (OBJLoader::*LineFunc)(const std::string&, int);

  bool ParseFile(const std::string& file_name, LineFunc func_ptr);
  void ParseObjLine(const std::string& line, int line_no);
  void ParseMtlLine(const std::string& line, int line_no);
  bool ParseFace(const std::string& line);

  bool load_materials_;
  int current_material_index_;
  std::string base_path_;
  std::string current_mtl_path_;
  OBJMesh* mesh_;
};

}  // namespace geometry

}  // namespace shapeml
