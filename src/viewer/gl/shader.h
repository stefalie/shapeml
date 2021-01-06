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

#include "viewer/gl/init.h"

namespace viewer {

namespace gl {

class Shader {
 public:
  explicit Shader(const char* label);
  ~Shader();

  bool BuildShaderFromFile(GLenum shader_type, const std::string& file_name,
                           const char* label);
  bool BuildShaderFromSource(GLenum shader_type, size_t num_sources,
                             const char* const sources[], const char* label);

  bool BuildProgram();

  void ProgramInfo(std::string* info) const;

  void Bind() const;

  GLuint program() const { return program_; }

 private:
  bool LoadFile(const std::string& file_name, std::string* source);

  GLuint vertex_shader_;
  GLuint tess_control_shader_;
  GLuint tess_evaluation_shader_;
  GLuint geometry_shader_;
  GLuint fragment_shader_;
  GLuint compute_shader_;

  GLuint program_;
};

}  // namespace gl

}  // namespace viewer
