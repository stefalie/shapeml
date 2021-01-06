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

namespace viewer {

namespace shaders {

#define STRINGIFY(str) STRINGIFY2(str)
#define STRINGIFY2(str) #str

#define GLSL_LINE "#line " STRINGIFY(__LINE__)

// Macro to insert glsl version and the correct line number.
#define GLSL_VERSION_LINE "#version 450\n" GLSL_LINE

const char* version = "#version 450\n";

const char* common_defines = GLSL_LINE R"glsl(
// Keep in sync with AttribIndex in renderer.h.
#define ATTRIB_INDEX_POSITIONS 0
#define ATTRIB_INDEX_NORMALS 1
#define ATTRIB_INDEX_UVS 2
#define ATTRIB_INDEX_SCALES 3
#define ATTRIB_INDEX_INSTANCE_IDS 4
#define ATTRIB_INDEX_MATERIAL_IDS 5

// Keep in sync with SSBOBindingIndex in renderer.h.
#define SSBO_BINDING_INDEX_TRANSFORMATIONS 0
#define SSBO_BINDING_INDEX_MATERIALS 1
#define SSBO_BINDING_INDEX_LFB_COUNT 2
#define SSBO_BINDING_INDEX_LFB_SUMS 3
#define SSBO_BINDING_INDEX_LFB_FRAGS 4
#define SSBO_BINDING_INDEX_LFB_LINKED_LIST_HEADER 5
#define SSBO_BINDING_INDEX_LFB_LINKED_LIST_NEXT_PTRS 6

// Keep in sync with UboBindingIndex in viewer.cc.
#define UBO_BINDING_INDEX_CAMERA 0
#define UBO_BINDING_INDEX_FXAA_PARAMS 1
#define UBO_BINDING_INDEX_TONEMAP_GAMMA_LUMA_PARAMS 2
#define UBO_BINDING_INDEX_PBR_PARAMS 3
#define UBO_BINDING_INDEX_SHADOW_MAP_PARAMS 4
#define UBO_BINDING_INDEX_BACKGROUND_AND_SKY 5
#define UBO_BINDING_INDEX_PREFIX_SUM_PARAMS 6
#define UBO_BINDING_INDEX_SAO_PARAMS 7
// Debug UBOS:
#define UBO_BINDING_INDEX_DEBUG_RENDER_ORDER 16
#define UBO_BINDING_INDEX_DEBUG_DISPLAY_TEXTURE 17
#define UBO_BINDING_INDEX_DEBUG_SHADOW_MAPS 18

// Keep in sync with SamplerLocationIndex in renderer.h
#define SAMPLER_LOCATION_START_INDEX_MATERIALS 0

// Keep in sync with SamplerBindingIndex in viewer.cc.
#define SAMPLER_BINDING_SHADOW_MAP 12

)glsl";

const char* camera_ubo = GLSL_LINE R"glsl(
layout(std140, binding = UBO_BINDING_INDEX_CAMERA) uniform Camera {
  mat4 view_matrix;
  mat4 inverse_view_matrix;
  mat4 projection_matrix;  // TODO(stefalie): Also add combined view_projection_matrix.
  mat4 inverse_projection_matrix;
  vec4 reconstruct_info;
  float fovy;
  float near;
  float far;
  float map_ws_to_ss;
  float aspect_ratio;
  int framebuffer_width;
  int framebuffer_height;
} camera;
)glsl";

}  // namespace shaders

}  // namespace viewer
