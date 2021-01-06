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

#include "viewer/shaders/csm.h"
#include "viewer/shaders/forward.h"
#include "viewer/shaders/pbr.h"
#include "viewer/shaders/preamble.h"

namespace viewer {

namespace shaders {

const char* oit_accum_frag = GLSL_LINE R"glsl(
layout(location = 0) out vec4 output_accum;
layout(location = 1) out vec4 output_revealage;  // The texture has only one channel, but we need alpha for blending.

void main() {
  const vec4 frag_color = ForwardShading();

  const float one_sub_d = 1.0 - gl_FragCoord.z;
  const float weight = frag_color.a * max(0.01, 1000.0 * one_sub_d * one_sub_d * one_sub_d);

  output_accum = frag_color * weight;
  output_revealage = vec4(frag_color.a);
}
)glsl";

const char* oit_accum_frag_sources[] = {
    version,
    common_defines,
    camera_ubo,
    pbr_shade,
    shadow_params,
    shadow_map_evaluate,
    mesh_transform_frag_input,
    forward_shading,
    oit_accum_frag,
};

const char* oit_resolve_frag = GLSL_VERSION_LINE R"glsl(
in vec2 uv;

layout(binding = 0) uniform sampler2D tex_accum;
layout(binding = 1) uniform sampler2D tex_revealage;

out vec4 frag_color;

void main() {
  const ivec2 xy = ivec2(gl_FragCoord.xy);

  const vec4 accum = texelFetch(tex_accum, xy, 0);
  const float revealage = texelFetch(tex_revealage, xy, 0).r;

  frag_color = vec4(accum.rgb / clamp(accum.a, 1.0e-4, 5.0e4), revealage);
}
)glsl";

const char* lfb_count_vert = GLSL_LINE R"glsl(
// Vertex attributes
layout(location = ATTRIB_INDEX_POSITIONS) in vec3 position;

// Instance attributes
layout(location = ATTRIB_INDEX_SCALES) in vec3 scale;
layout(location = ATTRIB_INDEX_INSTANCE_IDS) in uint instance_id;

layout(std430, binding = SSBO_BINDING_INDEX_TRANSFORMATIONS) readonly buffer TransformationSSBO {
  mat4 transformations[];
};

void main() {
  const mat4 trafo = transformations[instance_id];
  const vec4 pos_ws = trafo * vec4((position * scale), 1.0);
  const vec4 pos_vs = camera.view_matrix * pos_ws;
  gl_Position = camera.projection_matrix * pos_vs;
}
)glsl";

const char* lfb_count_vert_sources[] = {
    version,
    common_defines,
    camera_ubo,
    lfb_count_vert,
};

const char* lfb_count_frag = GLSL_LINE R"glsl(
layout(early_fragment_tests) in;

layout(std430, binding = SSBO_BINDING_INDEX_LFB_COUNT) coherent buffer LFBCountSSBO {
  uint lfb_counts[];
};

void main() {
  const ivec2 frag_coord = ivec2(gl_FragCoord.xy);
  const int pixel_idx = frag_coord.y * camera.framebuffer_width + frag_coord.x;

  const uint kNumCounterBits = 6;  // TODO(stefalie): Put this in a global LFB preamble.
  atomicAdd(lfb_counts[pixel_idx], 1 << kNumCounterBits);
}
)glsl";

const char* lfb_count_frag_sources[] = {
    version,
    common_defines,
    camera_ubo,
    lfb_count_frag,
};

// This is from:
// Harris - 2007 - Parallel Prefix Sum (Scan) with CUDA
//
// Unfortunately warp/wave-wide intrinsics such as WavePrefixSum is not
// available yet (at least in glsl without going for extensions).
//
// Reading
// https://stackoverflow.com/questions/9689185/conflict-free-offset-macro-used-in-the-parallel-prefix-algorithm-from-gpu-gems-3
// is somewhat disheartening, but what can we do in glsl without warp
// intrinsics.
const char* lfb_prefix_sum_comp = GLSL_LINE R"glsl(
// This is the min allowed max value.
#define LOCAL_SIZE_X 1024

layout(local_size_x = LOCAL_SIZE_X) in;

layout(std430, binding = SSBO_BINDING_INDEX_LFB_COUNT) buffer Data {
  uint data[];
};

layout(std430, binding = SSBO_BINDING_INDEX_LFB_SUMS) buffer Sums {
  uint sums[];
};

layout(std140, binding = UBO_BINDING_INDEX_PREFIX_SUM_PARAMS) uniform PrefixSumParams {
  int num_elements;
  int max_num_fragment;
} prefix_sum_params;

// Note that the code from the article got the precedences between >> and + wrong.
// There are different versions of this in the wild, see also:
// https://github.com/phebuswink/CUDA/blob/master/MP4/MP4.2/scan_largearray_kernel.cu
#define NUM_BANKS 16
#define LOG_NUM_BANKS 4
#define CONFLICT_FREE_OFFSET(n) ((n) >> LOG_NUM_BANKS)
//#define CONFLICT_FREE_OFFSET(n) (((n) >> NUM_BANKS) + ((n) >> (2 * LOG_NUM_BANKS)))
// Version without bank conflict avoiding.
//#define CONFLICT_FREE_OFFSET(n) 0

shared uint data_shared[LOCAL_SIZE_X * 2 + CONFLICT_FREE_OFFSET(LOCAL_SIZE_X * 2 - 1)];

void main() {
  const int n = LOCAL_SIZE_X * 2;
  const int thid = int(gl_LocalInvocationID.x);

  // The following index computation is wrong in Harris' paper.
  const int global_start_idx = n * int(gl_WorkGroupID.x);
  int aj = thid;
  int bj = thid + LOCAL_SIZE_X;
  const int bank_offset_a = CONFLICT_FREE_OFFSET(aj);
  const int bank_offset_b = CONFLICT_FREE_OFFSET(bj);

  data_shared[aj + bank_offset_a] = (global_start_idx + aj < prefix_sum_params.num_elements) ? data[global_start_idx + aj] : 0;
  data_shared[bj + bank_offset_b] = (global_start_idx + bj < prefix_sum_params.num_elements) ? data[global_start_idx + bj] : 0;

  // Version without bank conflict avoiding.
  //data_shared[2 * thid    ] = 0;
  //data_shared[2 * thid + 1] = 0;
  //if (2 * gl_GlobalInvocationID.x + 1 < prefix_sum_params.num_elements) {
  //  data_shared[2 * thid    ] = data[2 * gl_GlobalInvocationID.x    ];
  //  data_shared[2 * thid + 1] = data[2 * gl_GlobalInvocationID.x + 1];
  //} else if (2 * gl_GlobalInvocationID.x < prefix_sum_params.num_elements) {
  //  data_shared[2 * thid    ] = data[2 * gl_GlobalInvocationID.x    ];
  //}

  int offset = 1;

  // Up-sweep
  for (int d = n >> 1; d > 0; d >>= 1) {
    barrier();

    if (thid < d) {
      int ai = offset * (2 * thid + 1) - 1;
      int bi = offset * (2 * thid + 2) - 1;
      ai += CONFLICT_FREE_OFFSET(ai);
      bi += CONFLICT_FREE_OFFSET(bi);

      data_shared[bi] += data_shared[ai];
    }
    offset *= 2;
  }

  // Store and clear last element.
  if (thid == 0) {
    const int idx = n - 1 + CONFLICT_FREE_OFFSET(n - 1);
    sums[gl_WorkGroupID.x] = data_shared[idx];
    data_shared[idx] = 0;
  }

  // Down-sweep
  for (int d = 1; d < n; d *= 2) {
    offset >>= 1;
    barrier();

    if (thid < d) {
      int ai = offset * (2 * thid + 1) - 1;
      int bi = offset * (2 * thid + 2) - 1;
      ai += CONFLICT_FREE_OFFSET(ai);
      bi += CONFLICT_FREE_OFFSET(bi);

      const uint t = data_shared[ai];
      data_shared[ai] = data_shared[bi];
      data_shared[bi] += t;
    }
  }

  barrier();

  if (global_start_idx + bj < prefix_sum_params.num_elements) {
    data[global_start_idx + aj] = data_shared[aj + bank_offset_a];
    data[global_start_idx + bj] = data_shared[bj + bank_offset_b];
  } else if (global_start_idx + aj < prefix_sum_params.num_elements) {
    data[global_start_idx + aj] = data_shared[aj + bank_offset_a];
  }

  // Version without bank conflict avoiding.
  //if (2 * gl_GlobalInvocationID.x + 1 < prefix_sum_params.num_elements) {
  //  data[2 * gl_GlobalInvocationID.x    ] = data_shared[2 * thid    ];
  //  data[2 * gl_GlobalInvocationID.x + 1] = data_shared[2 * thid + 1];
  //} else if (2 * gl_GlobalInvocationID.x < prefix_sum_params.num_elements) {
  //  data[2 * gl_GlobalInvocationID.x    ] = data_shared[2 * thid    ];
  //}
}
)glsl";

const char* lfb_prefix_sum_comp_sources[] = {
    version,
    common_defines,
    lfb_prefix_sum_comp,
};

const char* lfb_prefix_sum_adjust_incr_comp = GLSL_LINE R"glsl(
// This is the min allowed max value.
#define LOCAL_SIZE_X 1024

layout(local_size_x = LOCAL_SIZE_X) in;

layout(std430, binding = SSBO_BINDING_INDEX_LFB_COUNT) buffer Data {
  uint data[];
};

layout(std430, binding = SSBO_BINDING_INDEX_LFB_SUMS) readonly buffer Sums {
  uint sums[];
};

layout(std140, binding = UBO_BINDING_INDEX_PREFIX_SUM_PARAMS) uniform PrefixSumParams {
  int num_elements;
  int max_num_fragment;
} prefix_sum_params;

void main() {
  if (2 * gl_GlobalInvocationID.x + 1 < prefix_sum_params.num_elements) {
    data[2 * gl_GlobalInvocationID.x + 1] += sums[gl_WorkGroupID.x];
    data[2 * gl_GlobalInvocationID.x    ] += sums[gl_WorkGroupID.x];
  } else if (2 * gl_GlobalInvocationID.x < prefix_sum_params.num_elements) {
    data[2 * gl_GlobalInvocationID.x    ] += sums[gl_WorkGroupID.x];
  }
}
)glsl";

const char* lfb_prefix_sum_adjust_incr_comp_sources[] = {
    version,
    common_defines,
    lfb_prefix_sum_adjust_incr_comp,
};

const char* lfb_linked_list_define = GLSL_LINE R"glsl(
#define LFB_LINKED_LIST
)glsl";

const char* lfb_fill_frag = GLSL_LINE R"glsl(
layout(early_fragment_tests) in;

#ifndef LFB_LINKED_LIST
layout(std430, binding = SSBO_BINDING_INDEX_LFB_COUNT) coherent buffer LFBOffsetSSBO {
  uint lfb_offsets[];
};
#else
layout(std430, binding = SSBO_BINDING_INDEX_LFB_LINKED_LIST_HEADER) coherent buffer LFBLinkedListHeaderSSBO {
  uint lfb_ll_counter;
  uint lfb_ll_head_ptrs[];
};

layout(std430, binding = SSBO_BINDING_INDEX_LFB_LINKED_LIST_NEXT_PTRS) writeonly buffer LFBLinkedListNextPtrsSSBO {
  uint lfb_ll_next_ptrs[];
};
#endif

layout(std430, binding = SSBO_BINDING_INDEX_LFB_FRAGS) writeonly buffer LFBFragmentsSSBO {
  vec3 lfb_fragments[];
};

layout(std140, binding = UBO_BINDING_INDEX_PREFIX_SUM_PARAMS) uniform PrefixSumParams {
  int num_elements;
  int max_num_fragment;  // TODO(stefalie): Naming of the block is weird for the LL case as prefix sum is not used.
} prefix_sum_params;

void main() {
  // Shading
  const vec4 frag_color = ForwardShading();

  // Store fragment in LFB.
  const ivec2 frag_coord = ivec2(gl_FragCoord.xy);
  const int pixel_idx = frag_coord.y * camera.framebuffer_width + frag_coord.x;
#ifndef LFB_LINKED_LIST
  const uint counter = atomicAdd(lfb_offsets[pixel_idx], 1);
  const uint kNumCounterBits = 6;
  const int offset = int((counter >> kNumCounterBits) +
                         (((1u << kNumCounterBits) - 1u) & counter));
#else
  const uint offset = 1 + atomicAdd(lfb_ll_counter, 1);  // 1-based indices!
  if (offset < prefix_sum_params.max_num_fragment) {
    const uint old_head_ptr = atomicExchange(lfb_ll_head_ptrs[pixel_idx], offset);
    lfb_ll_next_ptrs[offset] = old_head_ptr;
  }
#endif

  if (offset < prefix_sum_params.max_num_fragment) {
    // Use pre-multiplied alpha. Should probably compress better and makes
    // composition easier (see next shader).
    const vec4 col_premul_alpha = vec4(frag_color.rgb * frag_color.a, frag_color.a);
    const float packed_rg = uintBitsToFloat(packHalf2x16(col_premul_alpha.rg));
    const float packed_ba = uintBitsToFloat(packHalf2x16(col_premul_alpha.ba));
    lfb_fragments[offset] = vec3(packed_rg, packed_ba, gl_FragCoord.z);
  }
}
)glsl";

const char* lfb_fill_frag_sources[] = {
    version,
    common_defines,
    camera_ubo,
    pbr_shade,
    shadow_params,
    shadow_map_evaluate,
    mesh_transform_frag_input,
    forward_shading,
    lfb_fill_frag,
};

const char* lfb_fill_linked_list_frag_sources[] = {
    version,
    common_defines,
    camera_ubo,
    pbr_shade,
    shadow_params,
    shadow_map_evaluate,
    mesh_transform_frag_input,
    forward_shading,
    lfb_linked_list_define,
    lfb_fill_frag,
};

const char* lfb_sort_and_blend_frag = GLSL_LINE R"glsl(
#ifndef LFB_LINKED_LIST
layout(std430, binding = SSBO_BINDING_INDEX_LFB_COUNT) readonly buffer LFBOffsetSSBO {
  uint lfb_offsets[];
};
#else
layout(std430, binding = SSBO_BINDING_INDEX_LFB_LINKED_LIST_HEADER) readonly buffer LFBLinkedListHeaderSSBO {
  uint lfb_ll_counter;
  uint lfb_ll_head_ptrs[];
};

layout(std430, binding = SSBO_BINDING_INDEX_LFB_LINKED_LIST_NEXT_PTRS) readonly buffer LFBLinkedListNextPtrsSSBO {
  uint lfb_ll_next_ptrs[];
};
#endif

layout(std430, binding = SSBO_BINDING_INDEX_LFB_FRAGS) readonly buffer LFBFragmentsSSBO {
  vec3 lfb_fragments[];
};

#ifndef LFB_LINKED_LIST
layout(std140, binding = UBO_BINDING_INDEX_PREFIX_SUM_PARAMS) uniform PrefixSumParams {
  int num_elements;
  int max_num_fragments;
} prefix_sum_params;
#endif

out vec4 frag_color;

// TODO(stefalie): This should be a shader keyword or specialization constant.
#define MAX_NUM_FRAGS 8
vec3 local_frags[MAX_NUM_FRAGS];

void InsertionSortFrags(int num_frags);

void main() {
  const ivec2 frag_coord = ivec2(gl_FragCoord.xy);
  const int pixel_idx = frag_coord.y * camera.framebuffer_width + frag_coord.x;

#ifndef LFB_LINKED_LIST
  // Start and end indices into the LFB array
  const uint counter = lfb_offsets[pixel_idx];
  const uint kNumCounterBits = 6;
  const int idx_start = int(counter >> kNumCounterBits);
  int count = int(((1u << kNumCounterBits) - 1u) & counter);

  if (count == 0) {
    discard;
  }
  count = min(min(count, MAX_NUM_FRAGS), prefix_sum_params.max_num_fragments - idx_start);

  for (int i = 0; i < count; ++i) {
    local_frags[i] = lfb_fragments[idx_start + i];
  }
#else
  uint node = lfb_ll_head_ptrs[pixel_idx];
  int count = 0;
  while (node != 0 && count < MAX_NUM_FRAGS) {
    local_frags[count] = lfb_fragments[node];
    node = lfb_ll_next_ptrs[node];
    ++count;
  }
#endif

  InsertionSortFrags(count);

  // There are two ways to do do the blending while iterating through the
  // fragments front-to-back.
  // 1. option with the front-to-back equations:
  // https://community.khronos.org/t/front-to-back-blending/65155
  //vec4 color_acc_1 = vec4(0.0, 0.0, 0.0, 1.0);
  //for (int i = 0; i < count; ++i) {
  //  const vec2 packed_rgba = local_frags[i].rg;
  //  const vec2 unpacked_rg = unpackHalf2x16(floatBitsToUint(packed_rgba.x));
  //  const vec2 unpacked_ba = unpackHalf2x16(floatBitsToUint(packed_rgba.y));
  //  const vec4 frag = vec4(unpacked_rg, unpacked_ba);
  //
  //  // Forward blending
  //  color_acc_1.rgb = color_acc_1.a * frag.rgb + color_acc_1.rgb;  // Alpha is pre-multiplied in frag.rgb
  //  color_acc_1.a = (1.0 - frag.a) * color_acc_1.a;
  //}
  //
  //// Mix with the background. We must cheat a bit as we use
  //// glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA) (which is required
  //// for the 2nd way of blending this).
  //const vec4 final_blend_color_1 = vec4(color_acc_1.rgb, 1.0 - color_acc_1.a);

  // 2. option with pre-multiplied alpha (where blending is associative):
  // https://tomforsyth1000.github.io/blog.wiki.html#%5B%5BPremultiplied%20alpha%5D%5D
  vec4 color_acc_2 = vec4(0.0);
  for (int i = 0; i < count; ++i) {
    const vec2 packed_rgba = local_frags[i].rg;
    const vec2 unpacked_rg = unpackHalf2x16(floatBitsToUint(packed_rgba.x));
    const vec2 unpacked_ba = unpackHalf2x16(floatBitsToUint(packed_rgba.y));
    const vec4 frag = vec4(unpacked_rg, unpacked_ba);

    // Forward blending
    color_acc_2 = color_acc_2 + (1.0 - color_acc_2.a) * frag;
  }
  // Blending with the background is exactly the same procedure, and with
  // glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA) set, the following just works.
  const vec4 final_blend_color_2 = color_acc_2;

  frag_color = final_blend_color_2;
}

void InsertionSortFrags(int num_frags) {
  for (int i = 0; i < (num_frags - 1); ++i) {
    int swap = i;

    for (int j = (i + 1); j < num_frags; ++j) {
      // Swap if closer to the viewer.
      if (local_frags[j].z < local_frags[swap].z) {
        swap = j;
      }
    }

    const vec3 tmp_frag = local_frags[swap];
    local_frags[swap] = local_frags[i];
    local_frags[i] = tmp_frag;
  }
}
)glsl";

const char* lfb_sort_and_blend_frag_sources[] = {
    version,
    common_defines,
    camera_ubo,
    lfb_sort_and_blend_frag,
};

const char* lfb_sort_and_blend_linked_list_frag_sources[] = {
    version,
    common_defines,
    camera_ubo,
    lfb_linked_list_define,
    lfb_sort_and_blend_frag,
};

const char* lfb_adaptive_blend_frag = GLSL_LINE R"glsl(
#ifndef LFB_LINKED_LIST
layout(std430, binding = SSBO_BINDING_INDEX_LFB_COUNT) readonly buffer LFBOffsetSSBO {
  uint lfb_offsets[];
};
#else
layout(std430, binding = SSBO_BINDING_INDEX_LFB_LINKED_LIST_HEADER) readonly buffer LFBLinkedListHeaderSSBO {
  uint lfb_ll_counter;
  uint lfb_ll_head_ptrs[];
};

layout(std430, binding = SSBO_BINDING_INDEX_LFB_LINKED_LIST_NEXT_PTRS) readonly buffer LFBLinkedListNextPtrsSSBO {
  uint lfb_ll_next_ptrs[];
};
#endif

layout(std430, binding = SSBO_BINDING_INDEX_LFB_FRAGS) readonly buffer LFBFragmentsSSBO {
  vec3 lfb_fragments[];
};

#ifndef LFB_LINKED_LIST
layout(std140, binding = UBO_BINDING_INDEX_PREFIX_SUM_PARAMS) uniform PrefixSumParams {
  int num_elements;
  int max_num_fragments;
} prefix_sum_params;
#endif

out vec4 frag_color;

// TODO(stefalie): This should be a shader keyword or specialization constant.
#define MAX_NUM_FRAGS 16
vec4 local_frags[MAX_NUM_FRAGS];
float local_depths[MAX_NUM_FRAGS];

// TODO(stefalie): This should be a shader keyword or specialization constant.
#define MAX_VISIBILITY_NODES 8

// Each node is vec3(depth, 1 - alpha, \\prod_k (1 - alpha_k)).
// There is space for 1 extra element used temporarily before node removal.
vec3 visibility_list[MAX_VISIBILITY_NODES + 1];

void main() {
  const ivec2 frag_coord = ivec2(gl_FragCoord.xy);
  const int pixel_idx = frag_coord.y * camera.framebuffer_width + frag_coord.x;

#ifndef LFB_LINKED_LIST
  // Start and end indices into the LFB array
  const uint counter = lfb_offsets[pixel_idx];
  const uint kNumCounterBits = 6;
  const int idx_start = int(counter >> kNumCounterBits);
  int count = int(((1u << kNumCounterBits) - 1u) & counter);

  if (count == 0) {
    discard;
  }
  count = min(min(count, MAX_NUM_FRAGS), prefix_sum_params.max_num_fragments - idx_start);

  for (int i = 0; i < count; ++i) {
    const vec3 packed_rgbaz = lfb_fragments[idx_start + i];
    const vec2 unpacked_rg = unpackHalf2x16(floatBitsToUint(packed_rgbaz.x));
    const vec2 unpacked_ba = unpackHalf2x16(floatBitsToUint(packed_rgbaz.y));
    local_frags[i] = vec4(unpacked_rg, unpacked_ba);
    local_depths[i] = packed_rgbaz.z;
  }
#else
  uint node = lfb_ll_head_ptrs[pixel_idx];
  int count = 0;
  while (node != 0 && count < MAX_NUM_FRAGS) {
    const vec3 packed_rgbaz = lfb_fragments[node];
    const vec2 unpacked_rg = unpackHalf2x16(floatBitsToUint(packed_rgbaz.x));
    const vec2 unpacked_ba = unpackHalf2x16(floatBitsToUint(packed_rgbaz.y));
    local_frags[count] = vec4(unpacked_rg, unpacked_ba);
    local_depths[count] = packed_rgbaz.z;

    node = lfb_ll_next_ptrs[node];
    ++count;
  }
  if (count == 0) {
    discard;
  }
#endif

  // 1st traversal for visibility compression

  // First keep filling the list for as long as we can.
  const int num_used_vis_nodes = min(MAX_VISIBILITY_NODES, count);
  for (int i = 0; i < count; ++i) {
    // Insert new node at the right position.
    const float new_depth = local_depths[i];
    const float new_transmittance = 1.0 - local_frags[i].a;

    int new_idx = min(i, MAX_VISIBILITY_NODES);
    while ((new_idx > 0) && (new_depth < visibility_list[new_idx - 1].x)) {
      visibility_list[new_idx] = visibility_list[new_idx - 1];
      visibility_list[new_idx].z *= new_transmittance;
      --new_idx;
    }

    const float total_transmittance = (new_idx > 0) ? visibility_list[new_idx - 1].z * new_transmittance : new_transmittance;
    const vec3 new_node = vec3(new_depth, new_transmittance, total_transmittance);
    visibility_list[new_idx] = new_node;

    // The list is full and we have to remove a node.
    if (i >= MAX_VISIBILITY_NODES) {
      // Find index of node that whose removal will cause the smallest error of
      // the integration of the transmittance function.
      float min_area = 100.0;  // Max possible area should be 1.0, so this is large enough.
      int min_idx = 0;         // Will be increased to at least 1.

      // Include the temporary end node in the search (i.e., <=).
      for (int j = 1; j <= MAX_VISIBILITY_NODES; ++j) {
        const float area = (visibility_list[j - 1].z - visibility_list[j].z) * (visibility_list[j].x - visibility_list[j - 1].x);
        if (area <= min_area) {
          min_area = area;
          min_idx = j;
        }
      }

      // Underestimate transmittance by combining the transmittance of the
      // 'min_idx' node with the previous node.
      visibility_list[min_idx - 1].y *= visibility_list[min_idx].y;
      visibility_list[min_idx - 1].z *= visibility_list[min_idx].y;

      // Remove node by shifting everything forward.
      for (int j = min_idx; j < MAX_VISIBILITY_NODES; ++j) {
        visibility_list[j] = visibility_list[j + 1];
        visibility_list[j].z = visibility_list[j - 1].z * visibility_list[j].y;
      }
    }
  }

  // 2nd traversal for blending. Accumulate transmittance per fragment.
  vec4 color_acc = vec4(0.0);
  for (int i = 0; i < count; ++i) {
    const vec3 frag = local_frags[i].rgb;
    const float d = local_depths[i];

    float transmittance = 1.0;
    for (int j = 0; (d > visibility_list[j].x) && (j < num_used_vis_nodes); ++j) {
      transmittance = visibility_list[j].z;
    }
    color_acc.rgb += frag * transmittance;
  }
  color_acc.a = visibility_list[num_used_vis_nodes - 1].z;

  // Since we use glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA), we have to flip
  // the alpha (i.e., the final total transmittance). The choice for this blend
  // function comes from the default LFB blending algorithm above.
  frag_color = vec4(color_acc.rgb, 1.0 - color_acc.a);
}
)glsl";

const char* lfb_adaptive_blend_frag_sources[] = {
    version,
    common_defines,
    camera_ubo,
    lfb_adaptive_blend_frag,
};

const char* lfb_adaptive_blend_linked_list_frag_sources[] = {
    version,
    common_defines,
    camera_ubo,
    lfb_linked_list_define,
    lfb_adaptive_blend_frag,
};

}  // namespace shaders

}  // namespace viewer
