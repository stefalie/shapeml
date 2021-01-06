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

#include "shapeml/geometry/obj_loader.h"

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>

namespace shapeml {

namespace geometry {

OBJMesh* OBJLoader::Load(const std::string& file_name, bool load_materials) {
  load_materials_ = load_materials;
  mesh_ = new OBJMesh();

#ifdef _WIN32
  size_t slash = file_name.find_last_of("\\/");
#else
  size_t slash = file_name.rfind("/");
#endif
  if (slash != std::string::npos) {
    base_path_ = file_name.substr(0, slash + 1);
  }
  mesh_->name = file_name.substr(slash + 1);

  current_material_index_ = 0;

  if (!ParseFile(file_name, &OBJLoader::ParseObjLine)) {
    delete mesh_;
    return nullptr;
  }
  return mesh_;
}

bool OBJLoader::ParseFile(const std::string& file_name, LineFunc func_ptr) {
  std::ifstream input(file_name);
  if (!input.good()) {
    return false;
  }

  std::string s;
  int line = 1;
  while (!input.eof()) {
    // Skip comments.
    while (input.peek() == '#') {
      getline(input, s);
      ++line;
    }

    if (input.eof()) {
      break;
    }

    getline(input, s);
    (this->*func_ptr)(s, line);
    ++line;
  }
  input.close();

  return true;
}

void OBJLoader::ParseObjLine(const std::string& line, int line_no) {
  std::istringstream input(line);
  std::string s;

  if (!(input >> s)) {
    return;
  }

  if (s == "v") {
    // Parse vertex.
    double x, y, z;
    input >> x >> y >> z;
    mesh_->vertices.push_back(x);
    mesh_->vertices.push_back(y);
    mesh_->vertices.push_back(z);
  } else if (s == "f") {
    if (ParseFace(line)) {
      std::cerr << "WARNING in OBJ loader: A face with less than 3 vertices is "
                   "ignored on line ";
      std::cerr << line_no << " of file '" << base_path_ + mesh_->name
                << "'.\n";
    }
  } else if (s == "vt") {
    // Parse texture Coordinates.
    double u, v;
    input >> u >> v;
    mesh_->tex_coords.push_back(u);
    mesh_->tex_coords.push_back(v);
  } else if (s == "mtllib") {
    // Parse material file.
    input >> s;

    if (load_materials_) {
      const std::string path = base_path_ + s;
      current_mtl_path_ = s;
      if (!ParseFile(path, &OBJLoader::ParseMtlLine)) {
        std::cerr << "WARNING in OBJ loader: Can't read material file '" << path
                  << "'";
        std::cerr << " for mesh file '" << base_path_ + mesh_->name << "'.\n";
      }
      current_mtl_path_.clear();
    }
  } else if (s == "usemtl") {
    // Change of material.
    input >> s;
    for (int i = 0; i < static_cast<int>(mesh_->materials.size()); ++i) {
      if (mesh_->materials[i].name == s) {
        current_material_index_ = i;
        break;
      }
    }
  } else if (s == "vn") {
    // Parse vertex normal.
    double x, y, z;
    input >> x >> y >> z;
    mesh_->normals.push_back(x);
    mesh_->normals.push_back(y);
    mesh_->normals.push_back(z);
  } else {
    // Ignore remaining valid cases that we're not interested in.
    if (s != "s" && s != "o" && s != "g") {
      // Print error message for everything else.
      std::cerr << "WARNING in OBJ loader: Unknown value '" << s
                << "'found on line ";
      std::cerr << line_no << " of file '" << base_path_ + mesh_->name
                << "'.\n";
    }
  }
}

bool OBJLoader::ParseFace(const std::string& line) {
  std::istringstream input(line);
  std::string s;
  input >> s;

  FaceVertexArray face;

  int vertices = 0;
  int vi;
  while (input >> vi) {
    int vti = -1, vni = -1;
    FaceVertex fv;

    if (input.peek() == '/') {
      input.get();

      if (input.peek() != '/') {
        input >> vti;
      }

      if (input.peek() == '/') {
        input.get();
        input >> vni;
      }
    }

    // Subtract one since Obj uses one-based indexing.
    fv.vertex_index = vi - 1;
    if (vni > 0) {
      fv.normal_index = vni - 1;
    }
    if (vti > 0) {
      fv.tex_coord_index = vti - 1;
    }

    face.push_back(fv);
    ++vertices;

    while (input.peek() == ' ' || input.peek() == '\t') {
      input.get();
    }
  }

  if (vertices < 3) {
    return true;
  }

  // We need at least one material. If none is specified we create a default
  // material.
  if (mesh_->materials.empty()) {
    mesh_->face_sets.push_back(FaceSet());
    mesh_->materials.push_back(OBJMaterial());
  }
  assert(current_material_index_ < static_cast<int>(mesh_->face_sets.size()));

  // Decide what polygon type should be used.
  FaceSet& face_set = mesh_->face_sets[current_material_index_];
  face_set.push_back(face);

  return false;
}

void OBJLoader::ParseMtlLine(const std::string& line, int line_no) {
  std::istringstream input(line);
  std::string s;
  if (!(input >> s)) {
    return;
  }

  if (s == "newmtl") {
    // Parse material definition.
    current_material_index_ = static_cast<int>(mesh_->materials.size());
    OBJMaterial mat;
    input >> s;
    mat.name = s;
    mesh_->materials.push_back(mat);
    mesh_->face_sets.push_back(FaceSet());
  } else if (s == "Kd") {
    // Parse diffuse color.
    double r, g, b;
    input >> r >> g >> b;
    mesh_->materials[current_material_index_].diffuse[0] = r;
    mesh_->materials[current_material_index_].diffuse[1] = g;
    mesh_->materials[current_material_index_].diffuse[2] = b;
  } else if (s == "Ka") {
    // Parse ambient color.
    double r, g, b;
    input >> r >> g >> b;
    mesh_->materials[current_material_index_].ambient[0] = r;
    mesh_->materials[current_material_index_].ambient[1] = g;
    mesh_->materials[current_material_index_].ambient[2] = b;
  } else if (s == "Ks") {
    // Parse specular color.
    double r, g, b;
    input >> r >> g >> b;
    mesh_->materials[current_material_index_].specular[0] = r;
    mesh_->materials[current_material_index_].specular[1] = g;
    mesh_->materials[current_material_index_].specular[2] = b;
  } else if (s == "Ns") {
    // Parse shininess.
    double ns;
    input >> ns;
    mesh_->materials[current_material_index_].shininess = ns;
  } else if (s == "d" || s == "Tr") {
    // Parse transparency.
    double tr;
    input >> tr;
    mesh_->materials[current_material_index_].opacity = 1.0 - tr;
  } else if (s == "map_Kd") {
    input >> s;
    mesh_->materials[current_material_index_].diffuse_map = s;
  } else if (s == "map_Ka") {
    input >> s;
    mesh_->materials[current_material_index_].ambient_map = s;
  } else if (s == "map_Ks") {
    input >> s;
    mesh_->materials[current_material_index_].specular_map = s;
  } else if (s == "map_bump" || s == "bump") {
    input >> s;
    mesh_->materials[current_material_index_].bump_map = s;
  } else if (s == "map_refl") {
    input >> s;
    mesh_->materials[current_material_index_].reflection_map = s;
  } else {
    // Ignore remaining valid cases that we're not interested in.
    if (s != "illum" && s != "Ni" && s != "Tf") {
      // Print error message for everything else.
      std::cerr << "WARNING in OBJ loader: Unknown value '" << s
                << "' found on line ";
      std::cerr << line_no << " of material file '" << current_mtl_path_;
      std::cerr << "' for mesh file '" << base_path_ + mesh_->name << "'.\n";
    }
  }
}

}  // namespace geometry

}  // namespace shapeml
