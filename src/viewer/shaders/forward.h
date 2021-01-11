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
#include "viewer/shaders/pbr.h"
#include "viewer/shaders/preamble.h"

namespace viewer {

namespace shaders {

const char* mesh_transform_vert = GLSL_LINE R"glsl(   
// Vertex attributes
layout(location = ATTRIB_INDEX_POSITIONS) in vec3 position;
layout(location = ATTRIB_INDEX_NORMALS) in vec3 normal;
layout(location = ATTRIB_INDEX_UVS) in vec2 uv;

// Instance attributes
layout(location = ATTRIB_INDEX_SCALES) in vec3 scale;
layout(location = ATTRIB_INDEX_INSTANCE_IDS) in uint instance_id;

layout(std430, binding = SSBO_BINDING_INDEX_TRANSFORMATIONS) readonly buffer TransformationSSBO {
  mat4 transformations[];
};

// Material SSBO
struct Material {
  vec4 color;
  float metallic;
  float roughness;
  float reflectance;
  uint texture_unit;
};
layout(std430, binding = SSBO_BINDING_INDEX_MATERIALS) readonly buffer MaterialSSBO {
  Material materials[];
};

out VertexData {
  vec3 pos_eye;
  vec3 normal_eye;
  vec2 uv;
  flat vec4 color;
  flat uint texture_unit;
  flat vec3 material;  // metallic/roughness/reflectance
} Out;

void main() {
  const mat4 trafo = transformations[instance_id];
  const vec4 pos_world = trafo * vec4((position * scale), 1.0);
  const vec4 pos_eye = camera.view_matrix * pos_world;
  gl_Position = camera.projection_matrix * pos_eye;

  // Handle normal matrix separately for rotation and scaling for the case that
  // the scale is degenerate.
  // normal_matrix = ((R * S) ^ -1) ^ T
  // = (S^-1 * T^-1) ^ T
  // = R^-T * S^-T
  // = R * S^-1
  const bvec3 degenerate = equal(scale, vec3(0.0));
  const vec3 scale_inv = vec3(
    degenerate.x ? 1.0 : 1.0 / scale.x,
    degenerate.y ? 1.0 : 1.0 / scale.y,
    degenerate.z ? 1.0 : 1.0 / scale.z
  );
  vec3 transformed_normal = mat3(trafo) * (normal * scale_inv);
  // Could normalize here already.
  transformed_normal = mat3(camera.view_matrix) * transformed_normal;
  transformed_normal = normalize(transformed_normal);

  Out.pos_eye = pos_eye.xyz;
  Out.normal_eye = transformed_normal;
  Out.uv = uv;
  Out.color = materials[instance_id].color;
  Out.texture_unit = materials[instance_id].texture_unit;
  Out.material = vec3(materials[instance_id].metallic,
                      materials[instance_id].roughness,
                      materials[instance_id].reflectance);
}
)glsl";

const char* mesh_transform_vert_sources[] = {
    version,
    common_defines,
    camera_ubo,
    mesh_transform_vert,
};

const char* mesh_transform_frag_input = GLSL_LINE R"glsl(
in VertexData {
  vec3 pos_eye;  // The deferred renderer will only use the z value from here, but forward mode uses all.
  vec3 normal_eye;
  vec2 uv;
  flat vec4 color;
  flat uint texture_unit;
  flat vec3 material;  // metallic/roughness/reflectance
} In;

// Keep in sync with kMaxNumTexturesPerBatch in renderer.h.
#define MAX_NUM_TEXTURES_PER_BATCH 12
layout(location = SAMPLER_LOCATION_START_INDEX_MATERIALS) uniform sampler2D textures[MAX_NUM_TEXTURES_PER_BATCH];
)glsl";

const char* forward_shading = GLSL_LINE R"glsl(
vec4 ForwardShading() {
  const vec4 color = In.color * texture(textures[In.texture_unit], In.uv);
  const vec3 normal = normalize(In.normal_eye);

  const vec3 shade = PBRShade(color.rgb, normal, In.pos_eye, In.material);

  // Compute face normal and shadow.
  const vec3 dx = dFdx(In.pos_eye);
  const vec3 dy = dFdy(In.pos_eye);
  const vec3 face_normal = normalize(cross(dx, dy));  
  const vec3 shadow = ShadowEvaluate(In.pos_eye, face_normal);

  const vec4 frag_color = vec4(shade * shadow, color.a);
  return frag_color;
}
)glsl";

const char* forward_frag = GLSL_LINE R"glsl(
layout(early_fragment_tests) in;

layout(location = 0) out vec4 frag_color;

void main() {
  frag_color = ForwardShading();
}
)glsl";

const char* forward_frag_sources[] = {
    version,
    common_defines,
    camera_ubo,
    pbr_shade,
    shadow_params,
    shadow_map_evaluate,
    mesh_transform_frag_input,
    forward_shading,
    forward_frag,
};

}  // namespace shaders

}  // namespace viewer
