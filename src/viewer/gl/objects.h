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

#include <vector>

#include "viewer/gl/init.h"

namespace viewer {

namespace gl {

// This file contains wrappers for the most common OpenGL functionality.

void PushGroup(const char* label);
void PopGroup();

struct Buffer {
  void Create(const char* label);
  void Alloc(GLsizeiptr size, const void* data, GLbitfield flags);
  void Free();
  void Delete();

  void BindBase(GLenum target, GLuint binding_index);
  void BindRange(GLenum target, GLuint binding_index, GLintptr offset,
                 GLsizeiptr size);
  // For targets that cannot be bound to any binding point, e.g.,
  // GL_DRAW_INDIRECT_BUFFER.
  void Bind(GLenum target);

  void* Map(GLbitfield access);
  void* MapRange(GLintptr offset, GLsizeiptr length, GLbitfield access);
  // TODO(stefalie): Add `void Flush(...)`.
  void Unmap();

  GLuint id = 0;
  GLsizeiptr num_bytes = 0;
};

enum class GLFormatType {
  FLOAT,
  INTEGER,
  DOUBLE,
};

struct AttribDesc {
  GLuint attrib_index = -1;
  GLuint binding_index = -1;
  GLint size = 0;
  GLenum type;
  GLFormatType format_type = GLFormatType::FLOAT;
  GLboolean normalized = GL_FALSE;
  GLuint offset = 0;
};

struct BindingDesc {
  GLuint binding_index = -1;
  Buffer buffer = {0};
  GLintptr offset = 0;
  GLsizei stride = -1;
  GLuint divisor = 0;
};

struct VertexArray {
  void Create(const char* label);
  void Alloc(const std::vector<const BindingDesc*>& bindings,
             const std::vector<const AttribDesc*>& attribs,
             Buffer element_buffer);
  void Delete();

  void Bind() const;
  void BindBuffer(const BindingDesc* binding);

  GLuint id = 0;
};

struct Sampler {
  void Create(const char* label);
  void Delete();

  void BindToUnit(GLuint unit) const;

  void SetParameterInt(GLenum parameter_name, GLint value);
  void SetParameterFloat(GLenum parameter_name, GLfloat value);
  void SetParameterFloats(GLenum parameter_name, const GLfloat* values);

  GLuint id = 0;
};

struct Texture {
  void Create(GLenum target, const char* label);
  void Alloc(bool mipmap, GLenum internalformat, GLsizei width, GLsizei height);
  void Alloc3D(GLenum internalformat, GLsizei width, GLsizei height,
               GLsizei layers);
  void Delete();

  void Image(GLenum format, GLenum type, const void* pixels);
  void SubImage(GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                GLsizei height, GLenum format, GLenum type, const void* pixels);
  void GenerateMipmaps();

  void BindToUnit(GLuint unit) const;
  void BindAsImageToUnit(GLuint unit, GLint level, GLenum access) const;

  GLuint id = 0;
  GLenum fmt;
  uint16_t w;
  uint16_t h;
  uint16_t l;
  // TODO(stefalie): Add num_samples (like in the RenderBuffer struct).
};

struct RenderBuffer {
  void Create(const char* label);
  void Alloc(GLenum internalformat, GLsizei width, GLsizei height,
             GLsizei num_samples);
  void Delete();

  GLuint id = 0;
  GLenum fmt;
  uint16_t w;
  uint16_t h;
  uint8_t samples;
};

struct RenderBufferAttachDesc {
  RenderBuffer* render_buffer;
  GLenum attachment;
};

struct TextureAttachDesc {
  Texture* texture;
  GLenum attachment;
  GLint level;
};

struct FrameBuffer {
  void Create(const char* label);
  void Alloc(const std::vector<RenderBufferAttachDesc>& render_buffers,
             const std::vector<TextureAttachDesc>& textures);
  void Delete();

  void AttachRenderBuffer(RenderBufferAttachDesc desc);
  void AttachTexture(TextureAttachDesc desc);

  void Bind();
  void Unbind();

  // TODO(stefalie): Consider allowing blitting of individual attachments.
  // TODO(stefalie): Consider adding width/hegith to FBO, so that blitting all
  // doesn't require all these parameters. It will also allow asserting that
  // all attachments have the same dimensions. Blits all attachments. Requires
  // both FBO to have the same 'layout'.
  void BlitTo(FrameBuffer dst_fbo, GLint src_x0, GLint src_y0, GLint src_x1,
              GLint src_y1, GLint dst_x0, GLint dst_y0, GLint dst_x1,
              GLint dst_y1);

  GLuint id = 0;

  // Bitmask listing all attachments. From LSB up: depth, stencil, and up to 8
  // colors (8 is the minimum allowed number for GL_MAX_COLOR_ATTACHMENTS).
  uint16_t attachment_mask;
};

struct Fence {
  void Create();
  void Delete();
  GLenum ClientWaitSync(GLuint64 timeout);

  GLsync sync = nullptr;
};

}  // namespace gl

}  // namespace viewer
