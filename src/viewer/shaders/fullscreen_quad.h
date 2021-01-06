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
#include "viewer/shaders/deferred.h"
#include "viewer/shaders/pbr.h"
#include "viewer/shaders/preamble.h"

namespace viewer {

namespace shaders {

const char* fullscreen_quad_vert = GLSL_VERSION_LINE R"glsl(
out vec2 uv;

void main() {
  uv = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
  gl_Position = vec4(uv * 2.0 - 1.0, 0.0, 1.0);
}
)glsl";

// Same as above, but at the far plane.
const char* fullscreen_quad_far_vert = GLSL_VERSION_LINE R"glsl(
out vec2 uv;

void main() {
  uv = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
  gl_Position = vec4(uv * 2.0 - 1.0, 1.0, 1.0);
}
)glsl";

const char* display_texture_frag = GLSL_LINE R"glsl(
in vec2 uv;

layout(binding = 0) uniform sampler2D tex_debug;

layout(std140, binding = UBO_BINDING_INDEX_DEBUG_DISPLAY_TEXTURE) uniform DebugParams {
  int mode;
  int mip_level;
} debug_params;

out vec3 frag_color;

#define DISPLAY_MODE_RGB                  0
#define DISPLAY_MODE_NORMALS              1
#define DISPLAY_MODE_R                    2
#define DISPLAY_MODE_G                    3
#define DISPLAY_MODE_B                    4
#define DISPLAY_MODE_A                    5
#define DISPLAY_MODE_LINEAR_DEPTH         6
#define DISPLAY_MODE_LINEAR_NEAR_FAR      7
#define DISPLAY_MODE_POSITION_FROM_DEPTH  8

float LinearDepth(float z_depth_buf);
vec3 ReconstructPositionFromDepth(vec2 uv, float z_eye);

void main() {
  const vec4 tex_val = textureLod(tex_debug, uv, debug_params.mip_level);

  if (debug_params.mode == DISPLAY_MODE_RGB) {
    frag_color = tex_val.rgb;
  } else if (debug_params.mode == DISPLAY_MODE_NORMALS) {
    frag_color = tex_val.rgb * vec3(0.5) + vec3(0.5);
  } else if (debug_params.mode == DISPLAY_MODE_R) {
    frag_color = tex_val.rrr;
  } else if (debug_params.mode == DISPLAY_MODE_G) {
    frag_color = tex_val.ggg;
  } else if (debug_params.mode == DISPLAY_MODE_B) {
    frag_color = tex_val.bbb;
  } else if (debug_params.mode == DISPLAY_MODE_A) {
    frag_color = tex_val.aaa;
  } else if (debug_params.mode == DISPLAY_MODE_LINEAR_DEPTH) {
    const float z_linear = LinearDepth(tex_val.r);
    const float z_linear_in_01 = (-z_linear - camera.near) / (camera.far - camera.near);
    frag_color = vec3(z_linear_in_01);
  } else if (debug_params.mode == DISPLAY_MODE_LINEAR_NEAR_FAR) {
    const float linear_01 = (tex_val.r - camera.near) / (camera.far - camera.near);
    frag_color = vec3(linear_01);
  } else if (debug_params.mode == DISPLAY_MODE_POSITION_FROM_DEPTH) {
    const float z = tex_val.r;
    const float z_linear = LinearDepth(tex_val.r);
    frag_color = z == 1.0 ? vec3(1.0) : ReconstructPositionFromDepthUV(uv, z_linear);
  }
}
)glsl";

const char* display_texture_frag_sources[] = {
    version, common_defines, camera_ubo, depth_util, display_texture_frag,
};

const char* debug_shadows_frag = GLSL_LINE R"glsl(
in vec2 uv;

layout(binding = 0) uniform sampler2DArray shadow_maps;

layout(std140, binding = UBO_BINDING_INDEX_DEBUG_SHADOW_MAPS) uniform DebugParams {
  int cascade_number;
} debug_params;

out vec3 frag_color;

void main() {
  frag_color = texture(shadow_maps, vec3(uv, float(debug_params.cascade_number))).rrr;
}
)glsl";

const char* debug_shadows_frag_sources[] = {
    version,
    common_defines,
    debug_shadows_frag,
};

const char* sky_frag = GLSL_LINE R"glsl(
in vec2 uv;
out vec4 frag_color;

void main() {
  const vec2 uv_centered = uv * 2.0 - vec2(1.0);

  const vec3 ray_vs = vec3(camera.inverse_projection_matrix[0][0] * uv_centered.x, camera.inverse_projection_matrix[1][1] * uv_centered.y, -1.0);
  // Compute y-component of:
  // const vec3 ray_ws = mat3(camera.inverse_view_matrix) * ray_vs;
  const float ray_ws_y = dot(vec3(camera.inverse_view_matrix[0].y, camera.inverse_view_matrix[1].y, camera.inverse_view_matrix[2].y), ray_vs);
  // TODO(stefalie): I believe we could compute 'ray_ws_y' already in the vertex shader.

  // Gradients are from: https://www.shadertoy.com/view/4ljBRy
  const vec3 sky_base_color = vec3(0.1, 0.3, 0.6);  // Blue
  // const vec3 sky_base_color = vec3(1.0, 0.75, 0.5);  // Dusk

  const vec3 sky_color = exp2(-(ray_ws_y * 2.0 - 0.25) / sky_base_color);
  frag_color = vec4(sky_color, 1.0);
}
)glsl";

const char* sky_frag_sources[] = {
    version,
    common_defines,
    camera_ubo,
    sky_frag,
};

const char* infinite_plane_frag = GLSL_LINE R"glsl(
in vec2 uv;
out vec4 frag_color;

layout(std140, binding = UBO_BINDING_INDEX_BACKGROUND_AND_SKY) uniform BackgroundAndSkyParams {
  vec4 ground_plane_color;
  float ground_plane_height;
} bg_sky_params;

void main() {
  const vec2 uv_centered = uv * 2.0 - vec2(1.0);

  const vec3 ray_vs = vec3(camera.inverse_projection_matrix[0][0] * uv_centered.x, camera.inverse_projection_matrix[1][1] * uv_centered.y, -1.0);
  // Compute y-component of:
  // const vec3 ray_ws = mat3(camera.inverse_view_matrix) * ray_vs;
  const float ray_ws_y = dot(vec3(camera.inverse_view_matrix[0].y, camera.inverse_view_matrix[1].y, camera.inverse_view_matrix[2].y), ray_vs);
  // TODO(stefalie): I believe we could compute 'ray_ws_y' already in the vertex shader.

  const vec4 plane_ws = vec4(0.0, 1.0, 0.0, -bg_sky_params.ground_plane_height);
  const float cam_ws_y = camera.inverse_view_matrix[3].y;
  
  // Skip if the world space ray does not intersect the world space plane.
  if ((cam_ws_y + plane_ws.w) * ray_ws_y > 0.0) {
    discard;
  }

  // Intersect ground plane with view ray.
  const float lambda = -(cam_ws_y +  plane_ws.w) / ray_ws_y;
  
  // Reproject z-component only.
  const vec3 pos_vs = vec3(lambda * ray_vs.x, lambda * ray_vs.y, -lambda);
  const float depth_ndc = (camera.projection_matrix[2][2] * pos_vs.z + camera.projection_matrix[3][2]) / (camera.projection_matrix[2][3] * pos_vs.z);
  gl_FragDepth = depth_ndc * 0.5 + 0.5;

  // Identical to (normal is always up): mat3(camera.view_matrix) * plane_ws.xyz;
  const vec3 normal_vs = camera.view_matrix[1].xyz;

  const vec3 metallic_roughness_reflectance = vec3(0.0, 1.0, 0.5);
  const vec3 shade = PBRShade(bg_sky_params.ground_plane_color.rgb, normal_vs, pos_vs, metallic_roughness_reflectance);
  const vec3 shadow = ShadowEvaluate(pos_vs, normal_vs);
  frag_color = vec4(shade * shadow, 1.0);
}
)glsl";

const char* infinite_plane_frag_sources[] = {
    version,       common_defines,      camera_ubo,          pbr_shade,
    shadow_params, shadow_map_evaluate, infinite_plane_frag,
};

}  // namespace shaders

}  // namespace viewer
