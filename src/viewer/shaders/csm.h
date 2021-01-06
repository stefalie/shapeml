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

#include "viewer/shaders/preamble.h"

namespace viewer {

namespace shaders {

const char* shadow_params = GLSL_LINE R"glsl(
layout(std140, binding = UBO_BINDING_INDEX_SHADOW_MAP_PARAMS) uniform ShadowParams {
  mat4 light_view_matrix;
  mat4 light_projection[4];  // TODO(stefalie): Cheaper to store as scale + translation (6 vs 16 floats)
  vec4 split_depths;
  vec4 light_dir;

  float shadow_strength;

  // Biases
  float const_depth_bias;
  float slope_scale_depth_bias;
  float normal_offset_bias;
  int normal_offset_uv_only;
  int use_receiver_plane_bias;
  int fade_cascades;

  int enabled;
  int debug_show_cascades;  // TODO(stefalie): Use a specialization constant.
} shadow_params;
)glsl";

const char* shadow_map_vert = GLSL_LINE R"glsl(
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
  const vec4 pos_world = trafo * vec4((position * scale), 1.0);
  const vec4 pos_light = shadow_params.light_view_matrix * pos_world;
  gl_Position = shadow_params.light_projection[0] * pos_light;
}
)glsl";

const char* shadow_map_vert_sources[] = {
    version,
    common_defines,
    shadow_params,
    shadow_map_vert,
};

const char* shadow_map_cascade_vert = GLSL_LINE R"glsl(
// Vertex attributes
layout(location = ATTRIB_INDEX_POSITIONS) in vec3 position;

// Instance attributes
layout(location = ATTRIB_INDEX_SCALES) in vec3 scale;
layout(location = ATTRIB_INDEX_INSTANCE_IDS) in uint instance_id;

layout(std430, binding = SSBO_BINDING_INDEX_TRANSFORMATIONS) readonly buffer TransformationSSBO {
  mat4 transformations[];
};

out VertexData {
  vec4 pos_light;
} Out;

void main() {
  const mat4 trafo = transformations[instance_id];
  const vec4 pos_world = trafo * vec4((position * scale), 1.0);
  Out.pos_light = shadow_params.light_view_matrix * pos_world;
}
)glsl";

const char* shadow_map_cascade_vert_sources[] = {
    version,
    common_defines,
    shadow_params,
    shadow_map_cascade_vert,
};

const char* shadow_map_cascade_geom = GLSL_LINE R"glsl(
layout(invocations = 4) in;
layout(triangles) in;
layout(triangle_strip, max_vertices=12) out;

in VertexData {
  vec4 pos_light;
} In[];

void main() {
  gl_Layer = gl_InvocationID;

  for (int j = 0; j < 3; ++j) {
    gl_Position = shadow_params.light_projection[gl_InvocationID] * In[j].pos_light;
    EmitVertex();
  }
  EndPrimitive();
}

)glsl";

const char* shadow_map_cascade_geom_sources[] = {
    version,
    common_defines,
    shadow_params,
    shadow_map_cascade_geom,
};

const char* shadow_map_frag = GLSL_VERSION_LINE R"glsl(
void main() {}
)glsl";

// Note that the following shadow map sampling is done for directional lights
// only, i.e., orthogonal projections. Everything gets more for perspective
// lights (perspective division required, bias needs to be adapted in a more
// complicated way).
//
// Best shadow map resource:
// - https://mynameismjp.wordpress.com/2013/09/10/shadow-maps/
// - https://github.com/TheRealMJP/Shadows (source for the above)

const char* shadow_map_evaluate = GLSL_LINE R"glsl(
layout(binding = SAMPLER_BINDING_SHADOW_MAP) uniform sampler2DArrayShadow shadow_maps;

vec3 DebugShowCascades(int cascade_idx, vec3 shadow_coord) {
  if (shadow_coord.z < 0.0 || shadow_coord.z > 1.0) {
    return vec3(0);
  }

  // Color everything outside the shadow map(s) in red and within everything
  // gets assigned a color according to the cascade it's in.
  const vec3 cascade_colors[5] = vec3[](vec3(1.0, 1.0, 0.0),   // yellow
                                        vec3(0.0, 1.0, 0.0),   // green
                                        vec3(1.0, 0.0, 1.0),   // magenta
                                        vec3(0.0, 1.0, 1.0),   // cyan
                                        vec3(1.0, 0.0, 0.0));  // red, for outside
  // Range check
  if (any(lessThan(shadow_coord, vec3(0.0))) ||
      any(greaterThan(shadow_coord, vec3(1.0)))) {
    cascade_idx = 4;
  }
  return cascade_colors[cascade_idx];
}

// TODO(stefalie): Several constants could be precomputed and stored in an SSBO:
// - texel_size_half * shadow_params.slope_scale_depth_bias
// - texel_size_half * shadow_params.normal_offset_bias
// - shadow_params.const_depth_bias * cascade_depth_rcp
// - slope_offset * cascade_depth_rcp
// - If the stabilized version is used, the diagonal scaling components in the
//   ortho matrix have all the same magnitude (because of the bounding sphere),
//   and 'texel_size_half * cascade_depth_rcp' will reduce to -0.5 / tex_width.
// Further, the manual/const depth offset should be thrown away.
vec3 ConstAndSlopeAndNormalOffsets(vec3 normal) {
  // Dot product of direction in world coordinates.
  const vec3 normal_wc = mat3(camera.inverse_view_matrix) * normal;
  const float cos_alpha = clamp(dot(normal_wc, shadow_params.light_dir.xyz), 0.0, 1.0);

  // Biases based on:
  // http://the-witness.net/news/2013/09/shadow-mapping-summary-part-1/
  //const float offset_scale_normal = 1.0 - cos_alpha;                  // Original version from poster
  const float offset_scale_normal = sqrt(1.0 - cos_alpha * cos_alpha);  // sin(alpha)
  const float offset_scale_light = offset_scale_normal / cos_alpha;     // tan(alpha)

  const float slope_offset =  min(2.0, offset_scale_light) * shadow_params.slope_scale_depth_bias;
  const float normal_offset = offset_scale_normal * shadow_params.normal_offset_bias;

  return vec3(shadow_params.const_depth_bias, slope_offset, normal_offset);
}

vec3 ScaleBiases(vec3 const_slope_normal_biases, vec2 cascade_dim) {
  // The biases should be scaled by the texel size (or by half of it). 
  const float texel_size = cascade_dim.x / float(textureSize(shadow_maps, 0).x);

  // Multiplying the biases with texel_size_half transforms them into world space
  // 'sizes'. Manual/const and slope scale biases are mapped into the detph range
  // of the cascade.
  const float cascade_depth_inv = 1.0 / cascade_dim.y;
  return texel_size * vec3(const_slope_normal_biases.x * cascade_depth_inv,        // Multiple of texel size relative to cascade depth
                           const_slope_normal_biases.y * cascade_depth_inv * 0.5,  // Multiple of half texel size relative to cascade depth
                           const_slope_normal_biases.z * 0.5);                     // Multiple of world space half texel size
}

// Assumes that width == height. If we use the version where each cascade is
// contained in a sphere, then width == height == depth.
vec2 CascadeDimensions(int cascade_idx) {
  // The upper left element of an ortho projection is 2 / (right - left). This
  // allows extracting the frustum size.
  const float width = 2.0 / shadow_params.light_projection[cascade_idx][0][0];
  const float depth = 2.0 / abs(shadow_params.light_projection[cascade_idx][2][2]);
  return vec2(width, depth);
}

vec3 ShadowCoordFromEyePos(vec3 pos_eye, int cascade_idx) {
  const vec4 pos_world = camera.inverse_view_matrix * vec4(pos_eye, 1.0);
  const vec4 pos_light = shadow_params.light_view_matrix * pos_world;
  const vec4 ndc_light = shadow_params.light_projection[cascade_idx] * pos_light;
  const vec3 shadow_coord = vec3(0.5) * ndc_light.xyz + vec3(0.5);
  return shadow_coord.xyz;
}

vec3 ShadowCoordFromLightPos(vec4 pos_light, int cascade_idx) {
  const vec4 ndc_light = shadow_params.light_projection[cascade_idx] * pos_light;
  const vec3 shadow_coord = vec3(0.5) * ndc_light.xyz + vec3(0.5);
  return shadow_coord.xyz;
}

bool IsContainedInCascade(vec4 pos_light, int cascade_idx) {
  // Since the border of the shadow map is 1, filtering around a sample right
  // on the edge of the shadow map will return (at least) partially 'unshadowed'
  // which is a problem at the transition between two cascades. To keep all
  // filtering samples inside the cascade, we slightly shrink the cascade.
  // The alternative is (see TheRealMJP's code) to set the border to 0, but then
  // one needs to take special care if the camera far plane reaches further than
  // the shadow cascades (everything beyond will be 'shadowed').
  const float threshold = 0.99;
  return all(lessThanEqual(abs((shadow_params.light_projection[cascade_idx] * pos_light).xyz), vec3(threshold)));
}

// Transform light space depth derivatives that are with respect to screen space
// into derivatives that are with respect to shadow/light coordinates via the
// inverse of the Jacobian. This is done because the derivatives via dFdx/dFdy
// are in screen space.
// See: Isidoro - 2006 - Shadow Mapping - GPU-based Tips and Techniques
// | dz / du | = | dx / du   dy / du | * | dz / dx |
// | dz / dv |   | dx / dv   dy / dv |   | dz / dy |
// 
// | dz / du | = | du / dx   dv / dx |^-1 = | du / dx   du / dy |^-T * | dz / dx | = J^-T * | dz / dx |
// | dz / dv |   | du / dy   dv / dy |      | dv / dx   dv / dy |      | dz / dy |          | dz / dy |
//
// | dz / du | = 1 / det(J) * |  dv / dy   -dv / dx | * | dz / dx |
// | dz / dv |                | -du / dy    du / dx |   | dz / dy |
vec2 ComputeReceiverPlaneDepthBias(vec3 shadow_pos) {
  const vec3 shadow_pos_dx = dFdx(shadow_pos);
  const vec3 shadow_pos_dy = dFdy(shadow_pos);

  const float det_J = 1.0 / (shadow_pos_dx.x * shadow_pos_dy.y - shadow_pos_dy.x * shadow_pos_dx.y);

  // Remember, col-major fill in!
  const mat2 J_inv_T = det_J * mat2( shadow_pos_dy.y, -shadow_pos_dy.x,
                                    -shadow_pos_dx.y,  shadow_pos_dx.x);
  return J_inv_T * vec2(shadow_pos_dx.z, shadow_pos_dy.z);
}

float SampleShadowMap(vec2 base_uv, vec2 st, vec2 shadow_map_size_inv, int cascade_idx, float depth, vec2 receiver_plane_depth_bias) {
  const vec2 uv = base_uv + st * shadow_map_size_inv;

  if (shadow_params.use_receiver_plane_bias > 0.0) {
    depth += dot(st * shadow_map_size_inv, receiver_plane_depth_bias);
  }

  // Depth comparison. In comments is the version without a shadow sampler.
  // The shadow sampler version has the advantage that it seems to clamp the
  // passed-in reference value to [0, 1] which gives correct results for samples
  // beyond the light frustum.
  // const float shadow_map_value = texture(shadow_maps, vec3(uv, cascade_idx)).r;
  // return (depth < shadow_map_value) ? 1.0 : 0.0;
  return texture(shadow_maps, vec4(uv, cascade_idx, depth)).r;
}

vec3 SampleShadowCascade(vec3 shadow_coord, int cascade_idx, float bias) {
  // Note that we have clamp to border filtering activated (with border value 0),
  // therefore we need not worry about shadow_coord being outside the frustum (not
  // in [0, 1]). This makes below comments irrelevant.

  // The following is based on:
  // - http://the-witness.net/news/2013/09/shadow-mapping-summary-part-1/
  // - https://github.com/TheRealMJP/Shadows
  // - Isidoro - 2006 - Shadow Mapping - GPU-based Tips and Techniques
  //   (for receiver plane depth bias)

  const vec2 shadow_map_size_inv = 1.0 / textureSize(shadow_maps, 0).xy;
  float light_depth = shadow_coord.z - bias;

  // The receiver plane bias doesn't work well (as mentioned in
  // http://the-witness.net/news/2013/09/shadow-mapping-summary-part-1/).
  // TODO(stefalie): Consider romving receiver plane bias.
  vec2 receiver_plane_bias = vec2(0.0);

  // TODO(stefalie): Use specialization constant or ifdef.
  if (shadow_params.use_receiver_plane_bias > 0.0) {
    receiver_plane_bias = ComputeReceiverPlaneDepthBias(shadow_coord);

    // This extra static bias "to make up for incorrect fractional sampling on
    // the shadow map grid" is taken from TheRealMJP's code. Without it, there
    // will be terrible artifacts.
    const float fractional_sampling_error = dot(shadow_map_size_inv, abs(receiver_plane_bias));
    light_depth -= fractional_sampling_error;
  }

  // 1 Sample: 1x1 filter tap (2x2 footprint) sampling, for debugging 
  // const float shadow_single_tap = SampleShadowMap(shadow_coord.xy, vec2(0.0), shadow_map_size_inv, cascade_idx, light_depth, vec2(0.0));

  // Base UVs and fractionals
  // UVs where 1 unit == 1 texel
  vec2 uv = shadow_coord.xy * textureSize(shadow_maps, 0).x;
  uv -= vec2(0.5);

  vec2 base_uv = floor(uv);
  const vec2 st = uv - base_uv;
  base_uv += vec2(0.5);  // Add the offset to the center of the texel
  base_uv *= shadow_map_size_inv;

  const float s = st.x;
  const float t = st.y;

  // 9 Samples: 5x5 filter tap (6x6 footprint) sampling, same as in The Witness
  const float uw0 = (4.0 - 3.0 * s);
  const float uw1 = 7.0;
  const float uw2 = (1.0 + 3.0 * s);

  const float u0 = (3.0 - 2.0 * s) / uw0 - 2.0;
  const float u1 = (3.0 + s) / uw1;
  const float u2 = s / uw2 + 2.0;

  const float vw0 = (4.0 - 3.0 * t);
  const float vw1 = 7.0;
  const float vw2 = (1.0 + 3.0 * t);

  const float v0 = (3.0 - 2.0 * t) / vw0 - 2.0;
  const float v1 = (3.0 + t) / vw1;
  const float v2 = t / vw2 + 2.0;

  float sum = 0.0;

  sum += uw0 * vw0 * SampleShadowMap(base_uv, vec2(u0, v0), shadow_map_size_inv, cascade_idx, light_depth, receiver_plane_bias);
  sum += uw1 * vw0 * SampleShadowMap(base_uv, vec2(u1, v0), shadow_map_size_inv, cascade_idx, light_depth, receiver_plane_bias);
  sum += uw2 * vw0 * SampleShadowMap(base_uv, vec2(u2, v0), shadow_map_size_inv, cascade_idx, light_depth, receiver_plane_bias);

  sum += uw0 * vw1 * SampleShadowMap(base_uv, vec2(u0, v1), shadow_map_size_inv, cascade_idx, light_depth, receiver_plane_bias);
  sum += uw1 * vw1 * SampleShadowMap(base_uv, vec2(u1, v1), shadow_map_size_inv, cascade_idx, light_depth, receiver_plane_bias);
  sum += uw2 * vw1 * SampleShadowMap(base_uv, vec2(u2, v1), shadow_map_size_inv, cascade_idx, light_depth, receiver_plane_bias);

  sum += uw0 * vw2 * SampleShadowMap(base_uv, vec2(u0, v2), shadow_map_size_inv, cascade_idx, light_depth, receiver_plane_bias);
  sum += uw1 * vw2 * SampleShadowMap(base_uv, vec2(u1, v2), shadow_map_size_inv, cascade_idx, light_depth, receiver_plane_bias);
  sum += uw2 * vw2 * SampleShadowMap(base_uv, vec2(u2, v2), shadow_map_size_inv, cascade_idx, light_depth, receiver_plane_bias);

  const float shadow = sum * 1.0 / 144.0;

  return vec3(1.0 - shadow_params.shadow_strength * (1.0 - shadow));
}

// Returns a shade of gray (or a shade of a color if cascade debugging is
// enabled).
// The face normal has to be passed in because it requires derivatives. The
// the deferred rendered might discard the fragment before reaching shadow
// evaluation and therefore the derivatives need to be computed before that.
// Alternatively we could pass in the shading normal, but for shadows I think
// we want the actual face normal.
vec3 ShadowEvaluate(vec3 pos_eye, vec3 face_normal) {
  if (shadow_params.enabled == 0) {
    return vec3(1.0);
  }

  const float depth = -pos_eye.z;
  const vec4 pos_world = camera.inverse_view_matrix * vec4(pos_eye, 1.0);
  const vec4 pos_light = shadow_params.light_view_matrix * pos_world;

  // Choose cascade as the smallest one that still contains the position or simply
  // based on the view space depth value. Former is better and inspired by:
  // https://github.com/TheRealMJP/Shadows/blob/9d13c33d72d38210074d5e22213208f65caa1307/Shadows/Mesh.hlsl#L740
  int cascade_idx;
  const bool SELECT_FROM_PROJECTION = true;
  if (SELECT_FROM_PROJECTION) {
    if (IsContainedInCascade(pos_light, 0)) {
      cascade_idx = 0;
    } else if (IsContainedInCascade(pos_light, 1)) {
      cascade_idx = 1;
    } else if (IsContainedInCascade(pos_light, 2)) {
      cascade_idx = 2;
    } else if (IsContainedInCascade(pos_light, 3)) {
      cascade_idx = 3;
    } else {
      return vec3(1.0);
    }
    // Note that the early exit here can be problematic for receiver plane bias
    // because it uses screen-space derivatives of 'shadow_coord'. Then again,
    // the deferred renderer also contains a return even earlier on.
    // TODO(stefalie): Find a solution for this. Maybe just remove receiver
    // plane bias because it doesn't work well. Alternatively take the
    // derivatives before the return in the deffered renderer. That will
    // intertwine shadowing and deferred shading.
  } else {
    if (depth < shadow_params.split_depths[0]) {
      cascade_idx = 0;
    } else if (depth < shadow_params.split_depths[1]) {
      cascade_idx = 1;
    } else if (depth < shadow_params.split_depths[2]) {
      cascade_idx = 2;
    } else if (depth < shadow_params.split_depths[3]) {
      cascade_idx = 3;
    } else {
      return vec3(1.0);
    }
    // See note in the 'true' branch above.
  }

  // Compute lambda to fade between cascades.
  const float next_split = shadow_params.split_depths[cascade_idx];
  const float split_size = cascade_idx == 0 ? (next_split - camera.near) : (next_split - shadow_params.split_depths[cascade_idx - 1]);
  const float fade_lambda = (next_split - depth) / split_size;

  const vec2 cascade_dim_curr = CascadeDimensions(cascade_idx);
  vec2 cascade_dim_next = cascade_idx == 3 ? cascade_dim_curr : CascadeDimensions(cascade_idx + 1);
  // HACK: Somehow the transitions between the cascades work better with this, but
  // I don't really know why. Probably because precision suffers more and more the
  // further we are away from the camera.
  cascade_dim_next *= 1.6;
  const vec2 cascade_dim = (shadow_params.fade_cascades > 0) ? mix(cascade_dim_next, cascade_dim_curr, fade_lambda) : cascade_dim_curr;

  const vec3 biases = ScaleBiases(ConstAndSlopeAndNormalOffsets(face_normal), cascade_dim);
  const float final_bias = biases[0] + biases[1];  // Used below for depth comparison.

  // Normal offset shadows, see: Holbert, 2011, Saying Goodbye to Shadow Acne
  const vec3 normal_offset = biases[2] * face_normal;
  vec3 shadow_coord = ShadowCoordFromEyePos(pos_eye + normal_offset, cascade_idx);
  if (shadow_params.normal_offset_uv_only > 0) {
    // Unclear if this technic is of much use.
    const vec3 shadow_coord_no_offset = ShadowCoordFromLightPos(pos_light, cascade_idx);
    shadow_coord.z = shadow_coord_no_offset.z;
  }

  vec3 visibility = SampleShadowCascade(shadow_coord, cascade_idx, final_bias);

  // TODO(stefalie): Move this into its own debug shader_sky.
  if (shadow_params.debug_show_cascades > 0) {
    visibility *= DebugShowCascades(cascade_idx, shadow_coord);
  }

  return visibility;
}
)glsl";

}  // namespace shaders

}  // namespace viewer
