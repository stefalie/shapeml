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

#include <Eigen/Dense>
#include <vector>

#include "viewer/gl/objects.h"

namespace viewer {

namespace gl {

struct RenderVertex {
  Eigen::Vector3f position;
  Eigen::Vector3f normal;
  Eigen::Vector2f uv;
};

struct Material {
  Eigen::Vector4f color;
  float metallic;
  float roughness;
  float reflectance;
  uint32_t texture_id;
};

struct Renderable {
  Eigen::Matrix4f transformation_iso;
  Eigen::Vector3f scale;
  bool active = true;
  uint32_t mesh_id;
  uint32_t material_id;
};

struct AABB {
  Eigen::Vector3f min;
  Eigen::Vector3f max;
};

// Keep in sync with preamble.h.
enum AttribIndex {
  ATTRIB_INDEX_POSITIONS,
  ATTRIB_INDEX_NORMALS,
  ATTRIB_INDEX_UVS,
  ATTRIB_INDEX_SCALES,
  ATTRIB_INDEX_INSTANCE_IDS,
};

// Keep in sync with preamble.h.
enum SSBOBindingIndex {
  SSBO_BINDING_INDEX_TRANSFORMATIONS,
  SSBO_BINDING_INDEX_MATERIALS,
};

// Keep in sync with preamble.h.
enum SamplerLocationIndex {
  SAMPLER_LOCATION_START_INDEX_MATERIALS,
};

class Renderer {
 public:
  ~Renderer();

  int32_t AddMesh(const std::vector<RenderVertex>& vertex_data,
                  const std::vector<uint32_t>& index_data);

  int32_t AddMaterial(const Material& material);

  int32_t AddRenderable(const Renderable& renderable);

  Material* AccessMaterial(uint32_t id);
  Renderable* AccessRenderable(uint32_t id);

  void FinalizeStep1AllocateAndMap();
  void FinalizeStep2CopyData();
  void FinalizeStep3UnmapAndLoadTextures();
  void FinalizeAllSteps();

  void BeginFrame();
  void Bind();
  // Unfortunately RenderDoc doesn't seem to get along with my indirect calls,
  // hence the option to go direct.
  void RenderOpaque(bool use_indirect_draws);
  void RenderTransparent(bool use_indirect_draws);
  void EndFrame();

  struct Stats {
    unsigned num_batches;
    unsigned num_draw_calls;
    unsigned num_instances;
  };
  const Stats& stats() const { return stats_; }

  unsigned NumMeshes() const { return static_cast<unsigned>(meshes_.size()); }

  unsigned NumMaterials() const {
    return static_cast<unsigned>(materials_.size());
  }

  void SetCameraParameters(Eigen::Matrix4f view, float near, float far) {
    camera_ = {view, near, far};
  }

  void set_aabb(const AABB& aabb) { aabb_ = aabb; }
  const AABB& aabb() const { return aabb_; }

  // The number of textures used in this batch. 16 is the min value for
  // GL_MAX_TEXTURE_IMAGE_UNITS. We don't want to use up all units for mesh
  // textures because other techniques might need textures too, for example
  // shadow maps.
  // Keep this in sync with MAX_NUM_TEXTURES_PER_BATCH in forward.h.
  static const unsigned kMaxNumTexturesPerBatch = 12;

 private:
  // For the sort keys
  // | transparency |      texture |        mesh |    material |      depth |
  // |    1 bit: 63 |  13 bits: 50 | 18 bits: 32 | 13 bits: 19 | 19 bits: 0 |
  // Sorting by depth is probably not useful for back-to-front (or vice versa)
  // rendering as it has the lowest sort priority. Still fun to do.
  static const uint32_t kNumBitsTextures = 13;
  static const uint32_t kNumBitsMeshes = 18;
  static const uint32_t kNumBitsMaterials = 13;
  static const uint32_t kNumBitsDepth = 19;
  static const uint32_t kMaxNumTextures = 1 << kNumBitsTextures;
  static const uint32_t kMaxNumMeshes = 1 << kNumBitsMeshes;
  static const uint32_t kMaxNumMaterials = 1 << kNumBitsMaterials;
  static const uint32_t kMaxNumDepth = 1 << kNumBitsDepth;

  // Light-weight handles to renderables for sorting by material, state, depth,
  // etc.
  struct Handle {
    union SortKey {
      struct {
        uint64_t material_id : 13;  // This probably doesn't gain us anything.
        uint64_t depth : 19;
        uint64_t mesh_id : 18;
        uint64_t texture_id : 13;
        uint64_t transparent : 1;
      };
      uint64_t all;
    } sort_key;
    uint64_t index;
  };
  Handle::SortKey SortKey(const Renderable* renderable);
  static void SortHandles(std::vector<Handle>* elements,
                          std::vector<Handle>* tmp, int num_items);

  struct NumBatches {
    GLsizei num_batches_opaque = 0;
    GLsizei num_batches_transparent = 0;
  };
  NumBatches FillPerFrameBuffers(size_t buffer_idx);
  NumBatches num_batches_;

  void RenderBatches(int offset, int num_batches, bool use_indirect_draws);

  struct CameraParams {
    Eigen::Matrix4f view;
    float near;
    float far;
  } camera_;
  AABB aabb_;  // Used to reset camera to reaonsable position/direction.

  struct MeshInfo {
    GLuint base_vertex_offset;
    GLuint base_index_offset;
    GLuint vertex_count;
    GLuint index_count;
  };
  std::vector<MeshInfo> meshes_;

  std::vector<Renderable> renderables_;
  std::vector<Handle> handles_;
  std::vector<Handle> handles_tmp_;  // For swapping

  struct DrawCmd {
    GLuint count;
    GLuint instance_count;
    GLuint first_index;  // Offset into index array
    GLuint base_vertex;  // Added to each index
    GLuint base_instance;
  };

  struct BatchDrawCmd {
    uint32_t offset;         // Index into the array of DrawCmds
    uint32_t count;          // Number of DrawCmds to draw at once
    uint32_t texture_count;  // Not necessary, but still good to have.
    GLuint textures[kMaxNumTexturesPerBatch];
  };

  // Note that matrices are not here becasue they are stored separately in an
  // SSBOs since putting them into 4 attributes is a pain.
  struct DrawInstanceInfo {
    Eigen::Vector3f scale;
    GLuint draw_instance_index;
  };

  std::vector<RenderVertex> vertices_;
  std::vector<GLuint> indices_;
  std::vector<Material> materials_;

  struct MaterialGPU {
    Eigen::Vector4f color;
    float metallic;
    float roughness;
    float reflectance;
    uint32_t texture_unit;  // Texture unit and not ID!
  };

  gl::VertexArray vao_;
  gl::Buffer vertex_buffer_;
  gl::Buffer index_buffer_;

  static const size_t kNumConcurrentFrames = 3;
  size_t buffer_idx_ = 0;

  gl::Buffer instance_attrib_buffers_[kNumConcurrentFrames];
  gl::Buffer instance_trafo_buffers_[kNumConcurrentFrames];
  gl::Buffer instance_material_buffers_[kNumConcurrentFrames];
  gl::Buffer draw_cmd_buffers_[kNumConcurrentFrames];
  gl::Fence draw_fences_[kNumConcurrentFrames];

  // CPU copy of the above for non-indirect rendering.
  std::vector<DrawCmd> draw_cmd_buffer_cpu_;

  std::vector<BatchDrawCmd> batch_draw_cmds_;

  struct AsyncUpdatePtrs {
    RenderVertex* vertices;
    GLuint* indices;
  } ptrs_;

  Stats stats_;
};

}  // namespace gl

}  // namespace viewer
