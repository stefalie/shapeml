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

const char* debug_render_order_vert = GLSL_LINE R"glsl(   
// Vertex attributes
layout(location = ATTRIB_INDEX_POSITIONS) in vec3 position;

// Instance attributes
layout(location = ATTRIB_INDEX_SCALES) in vec3 scale;
layout(location = ATTRIB_INDEX_INSTANCE_IDS) in uint instance_id;

layout(std430, binding = SSBO_BINDING_INDEX_TRANSFORMATIONS) readonly buffer TransformationSSBO {
  mat4 transformations[];
};

layout(std140, binding = UBO_BINDING_INDEX_DEBUG_RENDER_ORDER) uniform RenderOrderParams {
  int num_instances;
} render_order_params;

out VertexData {
  flat vec4 color;
} Out;

void main() {
  const mat4 trafo = transformations[instance_id];
  const vec4 pos_world = trafo * vec4((position * scale), 1.0);
  const vec4 pos_eye = camera.view_matrix * pos_world;
  gl_Position = camera.projection_matrix * pos_eye;

  Out.color = vec4(vec3(float(instance_id) / float(render_order_params.num_instances - 1)), 1.0);
}
)glsl";

const char* debug_render_order_vert_sources[] = {
    version,
    common_defines,
    camera_ubo,
    debug_render_order_vert,
};

const char* debug_render_order_frag = GLSL_VERSION_LINE R"glsl(
in VertexData {
  flat vec4 color;
} In;

out vec4 frag_color;

void main() {
  frag_color = In.color;
}
)glsl";

}  // namespace shaders

}  // namespace viewer
