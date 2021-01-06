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

const char* gbuffer_frag = GLSL_LINE R"glsl(
layout(location = 0) out vec4 output_albedo;
layout(location = 1) out vec3 output_normals;
layout(location = 2) out float output_linear_depth;
layout(location = 3) out vec3 output_material;

void main() {
  const vec3 albedo = (In.color * texture(textures[In.texture_unit], In.uv)).rgb;
  const vec3 normal = normalize(In.normal_eye);

  output_albedo = vec4(albedo, 1.0);  // The alpha channel is used to mark non-empty pixels.
  output_normals = normal;
  output_linear_depth = -In.pos_eye.z;
  output_material = In.material;
}
)glsl";

const char* gbuffer_frag_sources[] = {
    version,      common_defines, camera_ubo, mesh_transform_frag_input,
    gbuffer_frag,
};

const char* deferred_resolve_frag = GLSL_LINE R"glsl(
in vec2 uv;

layout(binding = 0) uniform sampler2D tex_depth;
layout(binding = 1) uniform sampler2D tex_albedo;
layout(binding = 2) uniform sampler2D tex_normals;
layout(binding = 3) uniform sampler2D tex_material;

out vec3 frag_color;

void main() {
  const ivec2 xy = ivec2(gl_FragCoord.xy);

  const float frag_depth = texelFetch(tex_depth, xy, 0).r;
  const float z_eye = LinearDepth(frag_depth);
  const vec4 albedo_nonempty = texelFetch(tex_albedo, xy, 0);
  const vec3 position = ReconstructPositionFromDepthSS(vec2(xy) + vec2(0.5), z_eye);
  // const vec3 position = ReconstructPositionFromDepthUV(uv, z_eye);

  // Compute derivatives
  const vec3 dx = dFdx(position);
  const vec3 dy = dFdy(position);

  if (albedo_nonempty.a == 0.0) {
    discard;
  }

  const vec3 albedo = albedo_nonempty.rgb;
  const vec3 normal = texelFetch(tex_normals, xy, 0).rgb;
  const vec3 material = texelFetch(tex_material, xy, 0).rgb;

  const vec3 shade = PBRShade(albedo, normal, position, material);
  const vec3 face_normal = normalize(cross(dx, dy));  
  const vec3 shadow = ShadowEvaluate(position, face_normal);

  frag_color = shade * shadow;
}
)glsl";

const char* depth_util = GLSL_LINE R"glsl(
float LinearDepth(float z_depth_buf) {
  const float n = camera.near;
  const float f = camera.far;
  const float f_mul_n = camera.far * camera.near;  // TODO(stefalie): Could/should potentially be precomputed.
  const float f_sub_n = camera.far - camera.near;  // "

  // One can do this like this:
  // const float z_ndc = 2.0 * z_depth_buf - 1.0;
  // const float z_linear = 2.0 * f * n / (z_ndc * (f - n) - (f + n));
  // Or directly with less operations:
  const float z_linear = f_mul_n / (f_sub_n * z_depth_buf - f);
  return z_linear;
}

// Note: This assumes P_02 == P_12 == 0
// TODO(stefalie): This could be made way more efficient (1 mul + 1 add per
// comp). See the 'Scalable Ambient Obscurance' 2012 paper.
vec3 ReconstructPositionFromDepthUV(vec2 uv, float z_eye) {
  // x = (2.0 * uv - 1.0) / proj[0][0] * -z_eye
  // y = (2.0 * uv - 1.0) / proj[1][1] * -z_eye
  const vec2 inv_denom = vec2(camera.inverse_projection_matrix[0][0],
                              camera.inverse_projection_matrix[1][1]);
  const vec3 position = vec3((vec2(1.0) - 2.0 * uv) * inv_denom * z_eye, z_eye);
  return position;
}

// Position reconstruction with minimal number of operations as presented in
// McGuire - 2012 - Scalable Ambient Obscurance.
vec3 ReconstructPositionFromDepthSS(vec2 xy_ss, float z_eye) {
  // x = (((x_ss + 0.5) / width * 2 - 1) + P_02) / P_00 * -z_eye
  // x = ((1 - P_02) / P_00 - 2 / (width * P_00) * (x_ss + 0.5)) * z_eye
  // (This could potentially be simplified even more assuming P_02 == 0 and
  // knowing that P_00 == 2 * near / width.)
  // Y is analogous.
  const vec3 position = vec3((xy_ss * camera.reconstruct_info.xy + camera.reconstruct_info.zw) * z_eye, z_eye);
  return position;
}


)glsl";

const char* deferred_resolve_frag_sources[] = {
    version,   common_defines, camera_ubo,          depth_util,
    pbr_shade, shadow_params,  shadow_map_evaluate, deferred_resolve_frag,
};

const char* reconstruct_linear_depth_frag = GLSL_LINE R"glsl(
in vec2 uv;

layout(binding = 0) uniform sampler2D tex_depth;

out float linear_depth;

void main() {
  const ivec2 xy = ivec2(gl_FragCoord.xy);

  const float frag_depth = texelFetch(tex_depth, xy, 0).r;
  const float z_eye = LinearDepth(frag_depth);

  linear_depth = -z_eye;
}
)glsl";

const char* reconstruct_linear_depth_frag_sources[] = {
    version,
    common_defines,
    camera_ubo,
    depth_util,
    reconstruct_linear_depth_frag,
};

}  // namespace shaders

}  // namespace viewer
