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

#include "viewer/gl/objects.h"

#include <algorithm>
#include <cassert>

namespace viewer {

namespace gl {

void PushGroup(const char* label) {
  glPushDebugGroup(GL_DEBUG_SOURCE_APPLICATION, 0, -1, label);
}

void PopGroup() { glPopDebugGroup(); }

void Buffer::Create(const char* label) {
  glCreateBuffers(1, &id);
  if (label) {
    glObjectLabel(GL_BUFFER, id, -1, label);
  }
  num_bytes = 0;
}

void Buffer::Alloc(GLsizeiptr size, const void* data, GLbitfield flags) {
  glNamedBufferStorage(id, size, data, flags);
  num_bytes = size;
}

void Buffer::Free() { glInvalidateBufferData(id); }

void Buffer::Delete() {
  glDeleteBuffers(1, &id);
  id = 0;
}

void Buffer::BindBase(GLenum target, GLuint binding_index) {
  glBindBufferBase(target, binding_index, id);
}

void Buffer::BindRange(GLenum target, GLuint binding_index, GLintptr offset,
                       GLsizeiptr size) {
  glBindBufferRange(target, binding_index, id, offset, size);
}

void Buffer::Bind(GLenum target) { glBindBuffer(target, id); }

void* Buffer::Map(GLbitfield access) {
  return glMapNamedBufferRange(id, 0, num_bytes, access);
}

void* Buffer::MapRange(GLintptr offset, GLsizeiptr length, GLbitfield access) {
  return glMapNamedBufferRange(id, offset, length, access);
}

void Buffer::Unmap() { glUnmapNamedBuffer(id); }

#ifndef NDEBUG
// TODO(stefalie): Currently unused, probably remove this. The problem is that
// one can only validate once buffers are known. But sometimes one wants to
// allocate a VAO without knowing which buffers will be bound to it.
void VertexArrayValidateBindingsAndAttributes(
    const std::vector<const BindingDesc*>& binding_descs,
    const std::vector<const AttribDesc*>& attrib_descs) {
  // Verify the bindings.
  for (const BindingDesc* desc : binding_descs) {
    assert(desc->binding_index < GL_MAX_VERTEX_ATTRIB_BINDINGS);
    assert(glIsBuffer(desc->buffer.id));
    assert(desc->offset >= 0);
    assert(desc->stride >= 0 && desc->stride <= GL_MAX_VERTEX_ATTRIB_STRIDE);
  }

  // Verify the attributes.
  for (const AttribDesc* desc : attrib_descs) {
    assert(desc->attrib_index < GL_MAX_VERTEX_ATTRIBS);
    assert(desc->binding_index < GL_MAX_VERTEX_ATTRIB_BINDINGS);
    // Too lazy to verify if desc->type is a valid type.
    assert(desc->size >= 1 && desc->size <= 4);
    assert(desc->offset <= GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET);
  }

  // Verify that there is a buffer bound to each binding point used by any
  // attribute and that each active binding point is at least used by one
  // attribute.
  for (const AttribDesc* attrib : attrib_descs) {
    bool found = false;
    for (const BindingDesc* binding : binding_descs) {
      if (attrib->binding_index == binding->binding_index) {
        found = true;
        break;
      }
    }
    assert(found);
  }

  for (const BindingDesc* binding : binding_descs) {
    bool found = false;
    for (const AttribDesc* attrib : attrib_descs) {
      if (attrib->binding_index == binding->binding_index) {
        found = true;
        break;
      }
    }
    assert(found);
  }
}
#endif

void VertexArray::Create(const char* label) {
  glCreateVertexArrays(1, &id);
  if (label) {
    glObjectLabel(GL_VERTEX_ARRAY, id, -1, label);
  }
}

void VertexArray::Alloc(const std::vector<const BindingDesc*>& bindings,
                        const std::vector<const AttribDesc*>& attribs,
                        Buffer element_buffer) {
  // Ideally we could do everything with bindless glVertexArray* calls, but
  // unfortunately there seems to be a bug in the driver for Intel HD 620. That
  // bug only occurs when int and float vertex attributes are mixed within the
  // same buffer (it does not seem to matter if we use sepearate bindings or
  // not). The simplest workaround seems to be to just bind the VAO before
  // adding the attachments to it.
  glBindVertexArray(id);

  if (element_buffer.id) {
    glVertexArrayElementBuffer(id, element_buffer.id);
  }

  for (const BindingDesc* binding : bindings) {
    BindBuffer(binding);
  }

  for (const AttribDesc* attrib : attribs) {
    glEnableVertexArrayAttrib(id, attrib->attrib_index);
    switch (attrib->format_type) {
      case GLFormatType::FLOAT:
        glVertexArrayAttribFormat(id, attrib->attrib_index, attrib->size,
                                  attrib->type, attrib->normalized,
                                  attrib->offset);
        break;
      case GLFormatType::INTEGER:
        glVertexArrayAttribIFormat(id, attrib->attrib_index, attrib->size,
                                   attrib->type, attrib->offset);
        break;
      case GLFormatType::DOUBLE:
        glVertexArrayAttribLFormat(id, attrib->attrib_index, attrib->size,
                                   attrib->type, attrib->offset);
        break;
    }
    glVertexArrayAttribBinding(id, attrib->attrib_index, attrib->binding_index);
  }
}

void VertexArray::Delete() {
  glDeleteVertexArrays(1, &id);
  id = 0;
}

void VertexArray::Bind() const { glBindVertexArray(id); }

void VertexArray::BindBuffer(const BindingDesc* binding) {
  glVertexArrayVertexBuffer(id, binding->binding_index, binding->buffer.id,
                            binding->offset, binding->stride);
  glVertexArrayBindingDivisor(id, binding->binding_index, binding->divisor);
}

void Sampler::Create(const char* label) {
  glCreateSamplers(1, &id);
  if (label) {
    glObjectLabel(GL_SAMPLER, id, -1, label);
  }
}

void Sampler::Delete() {
  glDeleteSamplers(1, &id);
  id = 0;
}

void Sampler::BindToUnit(GLuint unit) const { glBindSampler(unit, id); }

void Sampler::SetParameterInt(GLenum parameter_name, GLint value) {
  glSamplerParameteri(id, parameter_name, value);
}

void Sampler::SetParameterFloat(GLenum parameter_name, GLfloat value) {
  glSamplerParameterf(id, parameter_name, value);
}

void Sampler::SetParameterFloats(GLenum parameter_name, const GLfloat* values) {
  glSamplerParameterfv(id, parameter_name, values);
}

void Texture::Create(GLenum target, const char* label) {
  glCreateTextures(target, 1, &id);
  if (label) {
    glObjectLabel(GL_TEXTURE, id, -1, label);
  }
  fmt = w = h = l = 0;
}

void Texture::Alloc(bool mipmap, GLenum internalformat, GLsizei width,
                    GLsizei height) {
  GLsizei levels = 1;
  if (mipmap) {
    // Computes levels = (floor(log_2(max(width, height))) + 1)
    GLsizei max_dim = std::max(width, height);
    while (max_dim >>= 1) {
      ++levels;
    }
  }
  fmt = internalformat;
  w = width;
  h = height;
  glTextureStorage2D(id, levels, fmt, w, h);
}

void Texture::Alloc3D(GLenum internalformat, GLsizei width, GLsizei height,
                      GLsizei layers) {
  fmt = internalformat;
  w = width;
  h = height;
  l = layers;
  glTextureStorage3D(id, 1, fmt, w, h, layers);
}

void Texture::Delete() {
  glDeleteTextures(1, &id);
  id = 0;
}

void Texture::Image(GLenum format, GLenum type, const void* pixels) {
  glTextureSubImage2D(id, 0, 0, 0, w, h, format, type, pixels);
}

void Texture::SubImage(GLint level, GLint xoffset, GLint yoffset, GLsizei width,
                       GLsizei height, GLenum format, GLenum type,
                       const void* pixels) {
  glTextureSubImage2D(id, level, xoffset, yoffset, width, height, format, type,
                      pixels);
}

void Texture::GenerateMipmaps() { glGenerateTextureMipmap(id); }

void Texture::BindToUnit(GLuint unit) const { glBindTextureUnit(unit, id); }

void Texture::BindAsImageToUnit(GLuint unit, GLint level, GLenum access) const {
  glBindImageTexture(unit, id, level, GL_FALSE, 0, access, fmt);
}

void RenderBuffer::Create(const char* label) {
  glCreateRenderbuffers(1, &id);
  if (label) {
    glObjectLabel(GL_RENDERBUFFER, id, -1, label);
  }
  fmt = w = h = samples;
}

void RenderBuffer::Alloc(GLenum internalformat, GLsizei width, GLsizei height,
                         GLsizei num_samples) {
  assert(num_samples > 0);
  fmt = internalformat;
  w = width;
  h = height;
  samples = num_samples;

  if (samples > 1) {
#ifndef NDEBUG
    GLint max_num_samples;
    glGetIntegerv(GL_MAX_SAMPLES, &max_num_samples);
    assert(samples <= max_num_samples);
#endif
    glNamedRenderbufferStorageMultisample(id, samples, internalformat, width,
                                          height);
  } else {
    glNamedRenderbufferStorage(id, internalformat, width, height);
  }
}

void RenderBuffer::Delete() {
  glDeleteRenderbuffers(1, &id);
  id = 0;
}

void FrameBuffer::Create(const char* label) {
  glCreateFramebuffers(1, &id);
  if (label) {
    glObjectLabel(GL_FRAMEBUFFER, id, -1, label);
  }
  attachment_mask = 0;
}

static void CheckFrameBufferStatus(const FrameBuffer* fb) {
  GLenum status = glCheckNamedFramebufferStatus(fb->id, GL_FRAMEBUFFER);
  switch (status) {
    case GL_FRAMEBUFFER_COMPLETE:
      break;
    case GL_FRAMEBUFFER_UNDEFINED:
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
      assert(false);
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
      assert(false);
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER:
      assert(false);
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER:
      assert(false);
      break;
    case GL_FRAMEBUFFER_UNSUPPORTED:
      assert(false);
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
      assert(false);
      break;
    case GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS:
      assert(false);
      break;
    default:
      assert(false);
      break;
  }
}

static void FrameBufferSetDrawBuffers(GLuint fbo, uint16_t attachment_mask) {
  // Collect active color attachments.
  std::vector<GLenum> draw_buffers;
  draw_buffers.reserve(8);
  for (int i = 0; i < 8; ++i) {
    if (attachment_mask & (1 << (i + 2))) {
      draw_buffers.emplace_back(GL_COLOR_ATTACHMENT0 + i);
    }
  }

  // If draw_buffers is empty, the buffers will implicitly all be set to
  // GL_NONE.
  glNamedFramebufferDrawBuffers(fbo, static_cast<GLsizei>(draw_buffers.size()),
                                draw_buffers.data());
}

void FrameBuffer::Alloc(
    const std::vector<RenderBufferAttachDesc>& render_buffers,
    const std::vector<TextureAttachDesc>& textures) {
  for (const RenderBufferAttachDesc desc : render_buffers) {
    AttachRenderBuffer(desc);
  }

  for (const TextureAttachDesc desc : textures) {
    AttachTexture(desc);
  }

  FrameBufferSetDrawBuffers(id, attachment_mask);
  CheckFrameBufferStatus(this);
}

static bool FrameBufferSetAttachmentBit(GLenum attachment,
                                        uint16_t* attachment_mask) {
  bool error = false;

  if (attachment == GL_DEPTH_ATTACHMENT) {
    // Only one out of depth/stencil/depth/stencil is allowed.
    assert((*attachment_mask & 2) == 0);
    *attachment_mask |= 1;
  } else if (attachment == GL_STENCIL_ATTACHMENT) {
    assert((*attachment_mask & 1) == 0);
    *attachment_mask |= 2;
  } else if (attachment == GL_DEPTH_STENCIL_ATTACHMENT) {
    assert(((*attachment_mask & 3) == 0) || ((*attachment_mask & 3) == 3));
    *attachment_mask |= 3;
  } else {
    const uint16_t i = attachment - GL_COLOR_ATTACHMENT0;
    assert(i < 8);
    if (i < 8) {
      *attachment_mask |= (1 << (2 + i));
    } else {
      error = true;
    }
  }

  return error;
}

void FrameBuffer::AttachRenderBuffer(RenderBufferAttachDesc desc) {
  if (!FrameBufferSetAttachmentBit(desc.attachment, &attachment_mask)) {
    glNamedFramebufferRenderbuffer(id, desc.attachment, GL_RENDERBUFFER,
                                   desc.render_buffer->id);
  }
}

void FrameBuffer::AttachTexture(TextureAttachDesc desc) {
  if (!FrameBufferSetAttachmentBit(desc.attachment, &attachment_mask)) {
    glNamedFramebufferTexture(id, desc.attachment, desc.texture->id,
                              desc.level);
  }
}

void FrameBuffer::Delete() {
  glDeleteFramebuffers(1, &id);
  id = 0;
}

void FrameBuffer::Bind() { glBindFramebuffer(GL_FRAMEBUFFER, id); }

void FrameBuffer::Unbind() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }

void FrameBuffer::BlitTo(FrameBuffer dst_fbo, GLint src_x0, GLint src_y0,
                         GLint src_x1, GLint src_y1, GLint dst_x0, GLint dst_y0,
                         GLint dst_x1, GLint dst_y1) {
  assert(attachment_mask == dst_fbo.attachment_mask);

  GLbitfield mask_depth_stencil = 0;
  if (attachment_mask & 1) {
    mask_depth_stencil |= GL_DEPTH_BUFFER_BIT;
  } else if (attachment_mask & 2) {
    mask_depth_stencil |= GL_STENCIL_BUFFER_BIT;
  }

  // Blit depth and/or stencil.
  if (mask_depth_stencil) {
    // Note that the filter parameter does not matter for depth and stencil.
    // According to 18.3.1 in the spec it's implementation dependent.
    glBlitNamedFramebuffer(id, dst_fbo.id, src_x0, src_y0, src_x1, src_y1,
                           dst_x0, dst_y0, dst_x1, dst_y1, mask_depth_stencil,
                           GL_NEAREST);
  }

  // Blit color attachments.
  bool needs_drawbuffer_reset = false;
  for (int i = 0; i < 8; ++i) {
    if (attachment_mask & (1 << (i + 2))) {
      glNamedFramebufferReadBuffer(id, GL_COLOR_ATTACHMENT0 + i);
      glNamedFramebufferDrawBuffer(dst_fbo.id, GL_COLOR_ATTACHMENT0 + i);
      // NOTE: that the filter parameters does not matter for resolving MSAA,
      // but it would be good to pass a flag for linear for when we want to copy
      // parts of FBO around with resizing.
      // NOTE: Sigh, this is buggy again on Intel HD 620 if you blit anything
      // else than attachment 0. The following two lines make it work:
      glBindFramebuffer(GL_DRAW_FRAMEBUFFER, dst_fbo.id);
      glDrawBuffer(GL_COLOR_ATTACHMENT0 + i);
      glBlitNamedFramebuffer(id, dst_fbo.id, src_x0, src_y0, src_x1, src_y1,
                             dst_x0, dst_y0, dst_x1, dst_y1,
                             GL_COLOR_BUFFER_BIT, GL_NEAREST);
      needs_drawbuffer_reset = true;
    }
  }

  // Reset the draw buffers after blitting.
  if (needs_drawbuffer_reset) {
    FrameBufferSetDrawBuffers(dst_fbo.id, attachment_mask);
  }
}

void Fence::Create() { sync = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0); }

void Fence::Delete() {
  glDeleteSync(sync);
  sync = nullptr;
}

GLenum Fence::ClientWaitSync(GLuint64 timeout) {
  GLenum ret = glClientWaitSync(sync, 0, timeout);
  assert(ret != GL_WAIT_FAILED);
  return ret;
}

}  // namespace gl

}  // namespace viewer
