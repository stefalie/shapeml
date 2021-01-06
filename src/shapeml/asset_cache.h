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
#include <unordered_map>

#include "shapeml/geometry/halfedge_mesh.h"

namespace shapeml {

class AssetCache {
 public:
  static AssetCache& Get();

  geometry::HalfedgeMeshPtr GetMeshRessource(const std::string& uri);

  bool HasMeshRessource(const std::string& uri) const;

  void InsertMeshRessource(const std::string& uri,
                           geometry::HalfedgeMeshPtr mesh);

 private:
  AssetCache();
  AssetCache(const AssetCache&) = delete;
  void operator=(const AssetCache&) = delete;

  std::unordered_map<std::string, geometry::HalfedgeMeshPtr> meshes_;
};

}  // namespace shapeml
