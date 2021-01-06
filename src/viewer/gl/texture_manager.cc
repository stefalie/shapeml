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

#include "viewer/gl/texture_manager.h"
#define STB_IMAGE_IMPLEMENTATION
#include <stb/stb_image.h>

#include <algorithm>
#include <iostream>

namespace viewer {

namespace gl {

TextureManager& TextureManager::Get() {
  static TextureManager instance;
  return instance;
}

uint32_t TextureManager::GreateOrGetTextureID(const std::string& uri) {
  const std::string path = base_path_ + uri;

  auto it = uri_to_id_.find(uri);
  if (it != uri_to_id_.end()) {
    return it->second;
  } else {
    Texture tex = {};  // Stub texture
    textures_.emplace_back(tex);
    const uint32_t id = static_cast<uint32_t>(textures_.size());
    uri_to_id_.emplace(uri, id);
    return id;
  }
}

void TextureManager::LoadMissingTextures() {
  for (auto& it : uri_to_id_) {
    Texture& tex = textures_[it.second - 1];

    // If the OpenGL handle is 0, the image has not been loaded yet.
    if (tex.id == 0) {
      const std::string& uri = it.first;
      const std::string path = base_path_ + uri;
      int width, height, num_channels;

      unsigned char* image =
          stbi_load(path.c_str(), &width, &height, &num_channels, 0);
      if (!image) {
        std::cerr << "WARNING: Couldn't load texture '" << path << "' using ";
        std::cerr << "checkerboard texture instead.\n";

        // Redirect the uri to the not found checkerboard. The unused/incomplete
        // texture is left in the array.
        it.second = not_found_checkerboard_id_;
      }

      if (num_channels != 3 && num_channels != 4) {
        std::cerr << "WARNING: Texture '" << path << "' has " << num_channels;
        std::cerr << " color channels, we can only handle 3 or 4. Using ";
        std::cerr << "checkerboard texture instead.\n";
        stbi_image_free(image);

        // Redirect the uri to the not found checkerboard. The unused/incomplete
        // texture is left in the array.
        it.second = not_found_checkerboard_id_;
      }

      const std::string label = std::string("Tex: ") + uri;
      tex.Create(GL_TEXTURE_2D, label.c_str());
      // Load texture data as sRGB.
      tex.Alloc(true, num_channels == 3 ? GL_SRGB8 : GL_SRGB8_ALPHA8, width,
                height);
      tex.Image(num_channels == 3 ? GL_RGB : GL_RGBA, GL_UNSIGNED_BYTE, image);
      tex.GenerateMipmaps();
      stbi_image_free(image);

      // TODO(stefalie): Check that texture allocation succeeded, if print some
      // error and use checkerboard instead. Failure should only happend in
      // border cases (e.g., GPU out of memory). We don't handle that right now.
    }
  }
}

Texture TextureManager::GetTexture(uint32_t id) const {
  assert(id > 0 && id <= textures_.size());
  return textures_[id - 1];
}

TextureManager::TextureManager() {
  stbi_set_flip_vertically_on_load(1);
  glPixelStorei(GL_UNPACK_ALIGNMENT, 1);

  // Create 8x8 checkerboard texture.
  Texture not_found_checkerboard;
  not_found_checkerboard.Create(GL_TEXTURE_2D, "Tex: checkerboard");
  not_found_checkerboard.Alloc(true, GL_RGB8, 512, 512);
  const unsigned dim = 512;
  GLubyte checkerboard[dim][dim][3];
  for (unsigned i = 0; i < dim; ++i) {
    for (unsigned j = 0; j < dim; ++j) {
      GLubyte c = (((i & 0x40) == 0) ^ ((j & 0x40) == 0)) * 255;
      checkerboard[i][j][0] = c;
      checkerboard[i][j][1] = c;
      checkerboard[i][j][2] = c;
    }
  }
  not_found_checkerboard.Image(GL_RGB, GL_UNSIGNED_BYTE, checkerboard);
  not_found_checkerboard.GenerateMipmaps();
  textures_.emplace_back(not_found_checkerboard);
  not_found_checkerboard_id_ = static_cast<uint32_t>(textures_.size());

  // Create 1x1 white texture.
  Texture white_1x1;
  white_1x1.Create(GL_TEXTURE_2D, "Tex: white1x1");
  white_1x1.Alloc(true, GL_RGB8, 1, 1);
  GLfloat white[3] = {1.0f, 1.0f, 1.0f};
  white_1x1.Image(GL_RGB, GL_FLOAT, white);
  // TODO(stefalie): Is this needed for a 1x1 texture?
  white_1x1.GenerateMipmaps();
  textures_.emplace_back(white_1x1);
  white_1x1_id_ = static_cast<uint32_t>(textures_.size());
  uri_to_id_.emplace("", white_1x1_id_);

  // Default sampler
  default_sampler_.Create("Sampler: default");
  // TODO(stefalie): This should probably only be repeat if the texture is
  // actually repeated.
  default_sampler_.SetParameterInt(GL_TEXTURE_WRAP_S, GL_REPEAT);
  default_sampler_.SetParameterInt(GL_TEXTURE_WRAP_T, GL_REPEAT);
  default_sampler_.SetParameterInt(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  default_sampler_.SetParameterInt(GL_TEXTURE_MIN_FILTER,
                                   GL_LINEAR_MIPMAP_LINEAR);

  // Anisotropic filtering, if available
  if (gl::ExtensionSupported("ARB_texture_filter_anisotropic")) {
    GLfloat max_anisotropy;
    glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &max_anisotropy);
    default_sampler_.SetParameterFloat(GL_TEXTURE_MAX_ANISOTROPY_EXT,
                                       std::min(16.0f, max_anisotropy));
  }
}

TextureManager::~TextureManager() {
  default_sampler_.Delete();
  for (auto it : textures_) {
    it.Delete();
  }
}

}  // namespace gl

}  // namespace viewer
