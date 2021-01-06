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

#include "viewer/gl/renderer.h"

#include <algorithm>
#include <cassert>

#include "viewer/gl/init.h"
#include "viewer/gl/texture_manager.h"

namespace viewer {

namespace gl {

Renderer::~Renderer() {
  if (vao_.id != 0) {
    vao_.Delete();
    vertex_buffer_.Delete();
    index_buffer_.Delete();

    for (size_t i = 0; i < kNumConcurrentFrames; ++i) {
      instance_attrib_buffers_[i].Delete();
      instance_trafo_buffers_[i].Delete();
      instance_material_buffers_[i].Delete();
      draw_cmd_buffers_[i].Delete();

      if (draw_fences_[i].sync) {
        draw_fences_[i].Delete();
      }
    }
  }
}

int32_t Renderer::AddMesh(const std::vector<RenderVertex>& vertex_data,
                          const std::vector<uint32_t>& index_data) {
  assert(meshes_.size() <= kMaxNumMeshes);
  if (meshes_.size() == kMaxNumMeshes) {
    return -1;
  }

  MeshInfo info;
  info.base_vertex_offset = static_cast<GLuint>(vertices_.size());
  info.base_index_offset = static_cast<GLuint>(indices_.size());
  info.vertex_count = static_cast<GLuint>(vertex_data.size());
  info.index_count = static_cast<GLuint>(index_data.size());
  meshes_.emplace_back(info);

  vertices_.insert(vertices_.end(), vertex_data.begin(), vertex_data.end());
  indices_.insert(indices_.end(), index_data.begin(), index_data.end());

  return static_cast<GLuint>(meshes_.size() - 1);
}

static float SRGB2Linear(const float x) {
  if (x <= 0.0f) {
    return 0.0f;
  } else if (x >= 1.0f) {
    return 1.0f;
  } else if (x < 0.04045f) {
    return x / 12.92f;
  } else {
    return std::pow((x + 0.055f) / 1.055f, 2.4f);
  }
}

int32_t Renderer::AddMaterial(const Material& material) {
  assert(material.metallic >= 0.0 && material.metallic <= 1.0f);
  assert(material.reflectance >= 0.0 && material.reflectance <= 1.0f);
  assert(material.roughness >= 0.0 && material.roughness <= 1.0f);
  assert((material.color.array() >= Eigen::Array4f::Zero()).all() &&
         (material.color.array() <= Eigen::Array4f::Ones()).all());
  assert(materials_.size() <= kMaxNumMaterials);
  if (materials_.size() == kMaxNumMaterials) {
    return -1;
  }

  materials_.emplace_back(material);

  // Convert colors from sRGB to linear space for all computations in the
  // shaders.
  Eigen::Vector4f& rgba = materials_.back().color;
  rgba.x() = SRGB2Linear(rgba.x());
  rgba.y() = SRGB2Linear(rgba.y());
  rgba.z() = SRGB2Linear(rgba.z());
  // Alternatively a simple gamma decoding would be sufficient too:
  // auto& rgb = materials_.back().color.head<3>()
  // rgb = rgb.array().pow(2.2f);

  // Set minimum perceptual roughness to prevent division by 0 in D_GGX in the
  // shader. Min value taken from Filament.
  const float min_perpectual_roughness = 0.045f;
  float& roughness = materials_.back().roughness;
  roughness = std::max(min_perpectual_roughness, roughness);

  return static_cast<int32_t>(materials_.size() - 1);
}

Material* Renderer::AccessMaterial(uint32_t id) {
  assert(id < static_cast<uint32_t>(materials_.size()));
  return &materials_[id];
}

int32_t Renderer::AddRenderable(const Renderable& renderable) {
  assert(renderable.mesh_id < meshes_.size());
  assert(renderable.material_id < materials_.size());
  renderables_.emplace_back(renderable);
  return static_cast<int32_t>(renderables_.size() - 1);
}

Renderable* Renderer::AccessRenderable(uint32_t id) {
  assert(id < renderables_.size());
  return &renderables_[id];
}

void Renderer::FinalizeStep1AllocateAndMap() {
  if (vertices_.empty() || indices_.empty()) {
    return;
  }

  // Vertex and index data
  vertex_buffer_.Create("Vertex buffer");
  vertex_buffer_.Alloc(vertices_.size() * sizeof(RenderVertex), nullptr,
                       GL_MAP_WRITE_BIT);
  index_buffer_.Create("Index buffer");
  index_buffer_.Alloc(indices_.size() * sizeof(GLuint), nullptr,
                      GL_MAP_WRITE_BIT);

  gl::BindingDesc vertex_binding;
  vertex_binding.binding_index = 0;
  vertex_binding.buffer = vertex_buffer_;
  vertex_binding.stride = 8 * sizeof(GLfloat);
  gl::AttribDesc position_attrib;
  position_attrib.attrib_index = ATTRIB_INDEX_POSITIONS;
  position_attrib.binding_index = 0;
  position_attrib.size = 3;
  position_attrib.type = GL_FLOAT;
  gl::AttribDesc normal_attrib;
  normal_attrib.attrib_index = ATTRIB_INDEX_NORMALS;
  normal_attrib.binding_index = 0;
  normal_attrib.size = 3;
  normal_attrib.type = GL_FLOAT;
  normal_attrib.offset = 3 * sizeof(GLfloat);
  gl::AttribDesc uv_attrib;
  uv_attrib.attrib_index = ATTRIB_INDEX_UVS;
  uv_attrib.binding_index = 0;
  uv_attrib.size = 2;
  uv_attrib.type = GL_FLOAT;
  uv_attrib.offset = 6 * sizeof(GLfloat);

  // Per instance data
  char tmp_buf[64];
  for (size_t i = 0; i < kNumConcurrentFrames; ++i) {
    snprintf(tmp_buf, sizeof(tmp_buf), "Instance attribute buffer %zi", i);
    instance_attrib_buffers_[i].Create(tmp_buf);
    instance_attrib_buffers_[i].Alloc(
        renderables_.size() * sizeof(DrawInstanceInfo), nullptr,
        GL_MAP_WRITE_BIT);

    snprintf(tmp_buf, sizeof(tmp_buf), "Instance transformation buffer %zi", i);
    instance_trafo_buffers_[i].Create(tmp_buf);
    instance_trafo_buffers_[i].Alloc(
        renderables_.size() * sizeof(Eigen::Matrix4f), nullptr,
        GL_MAP_WRITE_BIT);

    snprintf(tmp_buf, sizeof(tmp_buf), "Material SSBO %zi", i);
    instance_material_buffers_[i].Create(tmp_buf);
    instance_material_buffers_[i].Alloc(
        renderables_.size() * sizeof(MaterialGPU), nullptr, GL_MAP_WRITE_BIT);
  }
  handles_.resize(renderables_.size() + 1);  // + 1 to mark the end
  handles_tmp_.resize(renderables_.size() + 1);

  gl::AttribDesc scale_attrib;
  scale_attrib.attrib_index = ATTRIB_INDEX_SCALES;
  scale_attrib.binding_index = 1;
  scale_attrib.size = 3;
  scale_attrib.type = GL_FLOAT;
  gl::AttribDesc instance_it_attrib;
  instance_it_attrib.attrib_index = ATTRIB_INDEX_INSTANCE_IDS;
  instance_it_attrib.binding_index = 1;
  instance_it_attrib.size = 1;
  instance_it_attrib.type = GL_UNSIGNED_INT;
  instance_it_attrib.format_type = GLFormatType::INTEGER;
  instance_it_attrib.offset = 3 * sizeof(GLfloat);

  // Finish VAO.
  vao_.Create("Main VAO");
  vao_.Alloc({&vertex_binding /* instance binding is set every frame */},
             {&position_attrib, &normal_attrib, &uv_attrib, &scale_attrib,
              &instance_it_attrib},
             {index_buffer_});

  // Draw command buffer
  for (size_t i = 0; i < kNumConcurrentFrames; ++i) {
    snprintf(tmp_buf, sizeof(tmp_buf), "Draw buffer %zi", i);
    draw_cmd_buffers_[i].Create(tmp_buf);
    draw_cmd_buffers_[i].Alloc(renderables_.size() * sizeof(DrawCmd), nullptr,
                               GL_MAP_WRITE_BIT);
  }
  draw_cmd_buffer_cpu_.resize(renderables_.size());
  batch_draw_cmds_.reserve((renderables_.size() + kMaxNumTexturesPerBatch - 1) /
                           kMaxNumTexturesPerBatch);

  // Set pointers to mapped/exposed buffers.
  ptrs_.vertices = reinterpret_cast<RenderVertex*>(
      vertex_buffer_.Map(GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
  ptrs_.indices = reinterpret_cast<GLuint*>(
      index_buffer_.Map(GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));
}

void Renderer::FinalizeStep2CopyData() {
  if (vertices_.empty() || indices_.empty()) {
    return;
  }

  memcpy(ptrs_.vertices, &vertices_[0],
         vertices_.size() * sizeof(RenderVertex));
  memcpy(ptrs_.indices, &indices_[0], indices_.size() * sizeof(GLuint));
}

void Renderer::FinalizeStep3UnmapAndLoadTextures() {
  if (vertices_.empty() || indices_.empty()) {
    return;
  }

  vertex_buffer_.Unmap();
  index_buffer_.Unmap();

  vertices_.clear();
  indices_.clear();
  // Materials are not cleared since we still need them.

  TextureManager::Get().LoadMissingTextures();
}

void Renderer::FinalizeAllSteps() {
  FinalizeStep1AllocateAndMap();
  FinalizeStep2CopyData();
  FinalizeStep3UnmapAndLoadTextures();
}

void Renderer::BeginFrame() {
  assert(vao_.id != 0);
  if (vao_.id != 0) {
    if (draw_fences_[buffer_idx_].sync) {
      // Wait at most 1 second.
      const GLenum wait = draw_fences_[buffer_idx_].ClientWaitSync(1000000000);
      assert(wait == GL_CONDITION_SATISFIED || wait == GL_ALREADY_SIGNALED);
      (void)wait;
      draw_fences_[buffer_idx_].Delete();
    }

    num_batches_ = FillPerFrameBuffers(buffer_idx_);

    // Update buffer bindings
    gl::BindingDesc instance_binding;
    instance_binding.binding_index = 1;
    instance_binding.buffer = instance_attrib_buffers_[buffer_idx_];
    instance_binding.stride = sizeof(DrawInstanceInfo);
    instance_binding.divisor = 1;  // Per instance
    vao_.BindBuffer(&instance_binding);

    instance_trafo_buffers_[buffer_idx_].BindBase(
        GL_SHADER_STORAGE_BUFFER, SSBO_BINDING_INDEX_TRANSFORMATIONS);
    instance_material_buffers_[buffer_idx_].BindBase(
        GL_SHADER_STORAGE_BUFFER, SSBO_BINDING_INDEX_MATERIALS);
    draw_cmd_buffers_[buffer_idx_].Bind(GL_DRAW_INDIRECT_BUFFER);
  }
}

void Renderer::Bind() {
  assert(vao_.id != 0);
  if (vao_.id != 0) {
    vao_.Bind();
  }
}

void Renderer::RenderOpaque(bool use_indirect_draws) {
  RenderBatches(0, num_batches_.num_batches_opaque, use_indirect_draws);
}

void Renderer::RenderTransparent(bool use_indirect_draws) {
  RenderBatches(num_batches_.num_batches_opaque,
                num_batches_.num_batches_transparent, use_indirect_draws);
}

void Renderer::EndFrame() {
  assert(vao_.id != 0);
  if (vao_.id != 0) {
    draw_fences_[buffer_idx_].Create();
    buffer_idx_ = (buffer_idx_ + 1) % kNumConcurrentFrames;
  }
}

Renderer::Handle::SortKey Renderer::SortKey(const Renderable* renderable) {
  const Material& material = materials_[renderable->material_id];

  Handle::SortKey key = {};

  // The renderable is considerer transparent if its alpha is < 1 texture has an
  // alpha channel.
  if (material.color[3] < 1.0f) {
    key.transparent = 1;
  } else {
    Texture tex = TextureManager::Get().GetTexture(material.texture_id);
    if (tex.fmt == GL_RGBA8 || tex.fmt == GL_SRGB8_ALPHA8) {
      key.transparent = 1;
    }
  }

  assert(material.texture_id < kMaxNumTextures);
  key.texture_id = material.texture_id;

  assert(renderable->mesh_id < kMaxNumMeshes);
  key.mesh_id = renderable->mesh_id;

  assert(renderable->material_id < kMaxNumMaterials);
  key.material_id = renderable->material_id;

  // Sort by depth.
  Eigen::Vector4f sample_point = Eigen::Vector4f::Ones();
  sample_point.head<3>() = 0.5f * renderable->scale;
  const Eigen::Vector4f sample_point_world_coord =
      renderable->transformation_iso * sample_point;
  const float camera_z = (camera_.view * sample_point_world_coord).z();

  // Depth key to sort renderables front to back in order to avoid as much
  // overdraw as possible.
  float lin_depth = (-camera_z - camera_.near) / (camera_.far - camera_.near);
  lin_depth = std::max(std::min(lin_depth, 1.0f), 0.0f);
  uint64_t depth_key =
      std::min(uint64_t(lin_depth * kMaxNumDepth), uint64_t(kMaxNumDepth - 1));
  if (key.transparent) {
    // If transparent, we toggle the 'depth_key' bits, so that the order is
    // back to front.
    depth_key = ~depth_key & 0x7FFFF;
  }
  key.depth = depth_key;

  return key;
}

void Renderer::SortHandles(std::vector<Handle>* elements,
                           std::vector<Handle>* tmp, int num_items) {
  assert(num_items <= static_cast<int>(elements->size()));
  assert(elements->size() == tmp->size());

  const uint32_t max_byte_index = sizeof(uint64_t) * 8;
  for (uint32_t byte_index = 0; byte_index < max_byte_index; byte_index += 8) {
    uint32_t offsets[256] = {};

    // Make histogram. Put counts into offset array.
    for (int i = 0; i < num_items; ++i) {
      const uint32_t radix =
          (elements->at(i).sort_key.all >> byte_index) & 0xFF;
      ++offsets[radix];
    }

    // Convert counts to real offsets.
    uint32_t total = 0;
    for (int i = 0; i < 256; ++i) {
      const uint32_t count = offsets[i];
      offsets[i] = total;
      total += count;
    }

    // Put element into the right order.
    for (int i = 0; i < num_items; ++i) {
      const uint32_t radix =
          (elements->at(i).sort_key.all >> byte_index) & 0xFF;
      const uint32_t index = offsets[radix]++;
      tmp->at(index) = elements->at(i);
    }

    elements->swap(*tmp);
  }
}

Renderer::NumBatches Renderer::FillPerFrameBuffers(size_t buffer_idx) {
  GLsizei num_renderables = 0;
  for (size_t i = 0; i < renderables_.size(); ++i) {
    if (!renderables_[i].active) {
      continue;
    }

    // TODO(stefalie): I guess this is where smart people would do frustum
    // culling.

    handles_[num_renderables] = {SortKey(&renderables_[i]), i};
    ++num_renderables;
  }
  SortHandles(&handles_, &handles_tmp_, num_renderables);
  handles_[num_renderables].sort_key.all = 0xFFFFFFFFFFFFFFFF;  // Mark the end.

  const GLbitfield access = (GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT |
                             GL_MAP_UNSYNCHRONIZED_BIT);
  DrawInstanceInfo* instance_attribs = reinterpret_cast<DrawInstanceInfo*>(
      instance_attrib_buffers_[buffer_idx].Map(access));
  Eigen::Matrix4f* instance_trafos = reinterpret_cast<Eigen::Matrix4f*>(
      instance_trafo_buffers_[buffer_idx].Map(access));
  MaterialGPU* instance_materials = reinterpret_cast<MaterialGPU*>(
      instance_material_buffers_[buffer_idx].Map(access));
  DrawCmd* draw_cmds =
      reinterpret_cast<DrawCmd*>(draw_cmd_buffers_[buffer_idx].Map(access));

  // Drawcall batching
  batch_draw_cmds_.resize(0);
  NumBatches ret = {};

  GLsizei num_draw_cmds = 0;
  GLsizei num_instances = 0;

  int curr_tex_id = -1;
  const TextureManager& tex_man = TextureManager::Get();
  GLuint curr_tex_gl_id = 0;  // Just to prevent a maybe-uninitialized warning.
  int curr_mesh_id = -1;

  int tex_counter = 0;
  bool handle_transparent = false;

  for (GLsizei i = 0; i < num_renderables; ++i) {
    const Handle handle = handles_[i];
    const Handle::SortKey key = handle.sort_key;

    // Create new batch if necessary.
    bool new_batch = false;
    if (!handle_transparent && key.transparent) {
      new_batch = true;
      handle_transparent = true;
    }

    if (curr_tex_id != key.texture_id) {
      curr_tex_id = key.texture_id;
      curr_tex_gl_id = tex_man.GetTexture(curr_tex_id).id;

      if (!new_batch) {
        if (tex_counter % kMaxNumTexturesPerBatch == 0) {
          new_batch = true;
        } else {
          batch_draw_cmds_.back().textures[tex_counter] = curr_tex_gl_id;
          ++batch_draw_cmds_.back().texture_count;
          ++tex_counter;
        }
      }
    }

    if (new_batch) {
      BatchDrawCmd batch = {};  // Zero init, important for texture array.
      batch.offset = num_draw_cmds;
      batch.count = 0;
      batch.texture_count = tex_counter = 1;
      batch.textures[0] = curr_tex_gl_id;
      batch_draw_cmds_.emplace_back(batch);
      curr_mesh_id = -1;  // Force creation of a new 'DrawCmd'.

      if (handle_transparent) {
        ++ret.num_batches_transparent;
      } else {
        ++ret.num_batches_opaque;
      }
    }

    if (curr_mesh_id != key.mesh_id) {
      curr_mesh_id = key.mesh_id;

      const MeshInfo& mesh = meshes_[curr_mesh_id];

      DrawCmd& cmd = draw_cmds[num_draw_cmds];
      cmd.count = mesh.index_count;
      cmd.instance_count = 1;
      cmd.first_index = mesh.base_index_offset;
      cmd.base_vertex = mesh.base_vertex_offset;
      cmd.base_instance = num_instances;
      draw_cmd_buffer_cpu_[num_draw_cmds] = cmd;
      ++num_draw_cmds;
      ++batch_draw_cmds_.back().count;
    } else {
      ++draw_cmds[num_draw_cmds - 1].instance_count;
      ++draw_cmd_buffer_cpu_[num_draw_cmds - 1].instance_count;
    }

    // Fill the per frame instance attribute buffers.
    const Renderable& renderable = renderables_[handle.index];

    DrawInstanceInfo& instance = instance_attribs[num_instances];
    instance.scale = renderable.scale;
    instance.draw_instance_index = num_instances;

    instance_trafos[num_instances] = renderable.transformation_iso;

    const Material& material = materials_[key.material_id];
    MaterialGPU& instance_material = instance_materials[num_instances];
    instance_material.color = material.color;
    instance_material.metallic = material.metallic;
    instance_material.roughness = material.roughness;
    instance_material.reflectance = material.reflectance;
    instance_material.texture_unit = tex_counter - 1;

    ++num_instances;
  }

  instance_attrib_buffers_[buffer_idx].Unmap();
  instance_trafo_buffers_[buffer_idx].Unmap();
  instance_material_buffers_[buffer_idx].Unmap();
  draw_cmd_buffers_[buffer_idx].Unmap();

  stats_.num_batches = static_cast<unsigned>(batch_draw_cmds_.size());
  stats_.num_draw_calls = num_draw_cmds;
  stats_.num_instances = num_instances;

  return ret;
}

void Renderer::RenderBatches(int offset, int num_batches,
                             bool use_indirect_draws) {
  assert(vao_.id != 0);
  if (vao_.id != 0) {
    for (int i = 0; i < num_batches; ++i) {
      const BatchDrawCmd& batch = batch_draw_cmds_[i + offset];
      glBindTextures(SAMPLER_LOCATION_START_INDEX_MATERIALS,
                     batch.texture_count, batch.textures);

      if (use_indirect_draws) {
        // HACK. It seems the Intel HD 620 driver gets buggy and starts dropping
        // most (but not all) draws if there are more than ~64, and it flickers.
        // Therefore we split the batch into smaller batches. In case of the
        // shape.shp example, some geometry pieces are missing when batching
        // more than ~16 draws.
        // Note that if you put a glFlush at the end of this function, or if you
        // disalbe shadows (which calls RenderOpaque() and RenderTransparent()
        // again), then you can put the number of draws up to 4096 (performance
        // will be worse due to glFlush though). I have no clue why the heck
        // this happens?!?!
        // TODO(stefalie): Fix when driver gets fixed.
        const uint32_t kMaxDrawCalls = 16;
        for (uint32_t offset = 0; offset < batch.count;
             offset += kMaxDrawCalls) {
          const uint32_t offset_bytes =
              (batch.offset + offset) * sizeof(DrawCmd);
          const void* offset_gl = GL_BUFFER_OFFSET(offset_bytes);
          const uint32_t count = std::min(batch.count - offset, kMaxDrawCalls);
          glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT, offset_gl,
                                      count, sizeof(DrawCmd));
        }
        // Use this instead:
        // const void* offset_gl = GL_BUFFER_OFFSET(batch.offset *
        // sizeof(DrawCmd)); glMultiDrawElementsIndirect(GL_TRIANGLES,
        // GL_UNSIGNED_INT, offset_gl, batch.count, sizeof(DrawCmd));
      } else {
        for (uint32_t i = 0; i < batch.count; ++i) {
          DrawCmd cmd = draw_cmd_buffer_cpu_[batch.offset + i];
          glDrawElementsInstancedBaseVertexBaseInstance(
              GL_TRIANGLES, cmd.count, GL_UNSIGNED_INT,
              GL_BUFFER_OFFSET(cmd.first_index * sizeof(GLuint)),
              cmd.instance_count, cmd.base_vertex, cmd.base_instance);
        }
      }
    }
  }
}

}  // namespace gl

}  // namespace viewer
