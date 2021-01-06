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

#include "shapeml/asset_cache.h"

#include <memory>

#include "geometry/obj_loader.h"

namespace shapeml {

AssetCache& AssetCache::Get() {
  static AssetCache instance;
  return instance;
}

geometry::HalfedgeMeshPtr AssetCache::GetMeshRessource(const std::string& uri) {
  auto it = meshes_.find(uri);
  if (it != meshes_.end()) {
    return it->second;
  } else {
    geometry::HalfedgeMeshPtr mesh = std::make_shared<geometry::HalfedgeMesh>();
    if (mesh->FromFile(uri)) {
      // Return empty/null pointer if mesh cannot be loaded.
      return geometry::HalfedgeMeshPtr();
    }

    meshes_.emplace(uri, mesh);
    return mesh;
  }
}

bool AssetCache::HasMeshRessource(const std::string& uri) const {
  return (meshes_.find(uri) != meshes_.end());
}

void AssetCache::InsertMeshRessource(const std::string& uri,
                                     geometry::HalfedgeMeshPtr mesh) {
  assert(meshes_.find(uri) == meshes_.end());
  meshes_.emplace(uri, mesh);
}

AssetCache::AssetCache() {}

}  // namespace shapeml
