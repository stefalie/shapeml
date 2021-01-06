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
#include <vector>

#include "viewer/gl/init.h"
#include "viewer/gl/objects.h"

namespace viewer {

namespace gl {

class TextureManager {
 public:
  static TextureManager& Get();

  void set_base_path(const std::string& base_path) { base_path_ = base_path; }

  // Creates or retrieves an existing texture ID for a given ressource uri.
  // Doesn't do any actual image loading or OpenGL API calls, can be called
  // from non-rendering thread.
  // For the empty string "", the ID of a 1x1 white texture is returned.
  // P.s., ID 0 is not used.
  uint32_t GreateOrGetTextureID(const std::string& uri);

  // Loads all textures that have not been loaded before. If the image file for
  // a certain texture doesn't exist, couldn't be opened, or is an incompatible
  // format, the of a default checkerboard pattern texture is used in it's
  // place.
  void LoadMissingTextures();

  Texture GetTexture(uint32_t id) const;

  size_t NumTextures() const { return textures_.size(); }

  Sampler default_sampler() const { return default_sampler_; }

 private:
  TextureManager();
  TextureManager(const TextureManager&) = delete;
  void operator=(const TextureManager&) = delete;
  ~TextureManager();

  std::string base_path_;

  std::unordered_map<std::string, uint32_t> uri_to_id_;
  std::vector<Texture> textures_;

  uint32_t not_found_checkerboard_id_;  // Returned when texture is not found.
  uint32_t white_1x1_id_;               // 1x1 white, opaque texture

  Sampler default_sampler_;
};

}  // namespace gl

}  // namespace viewer
