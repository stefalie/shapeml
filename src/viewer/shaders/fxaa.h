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

// This is simplified version of FXAA based on:
// https://gist.github.com/kosua20/0c506b81b3812ac900048059d2383126
// http://blog.simonrodriguez.fr/articles/30-07-2016_implementing_fxaa.html
// Unused options have been stripped away, and it has been 'back'-ported to GLSL
// only. Note that in GLSL north/south are flipped compared to the original
// version (likely because screenspace up/down are different in HLSL).

const char* fxaa_frag = GLSL_LINE R"glsl(
in vec2 uv;

// See original source for meaningful values.
layout(std140, binding = UBO_BINDING_INDEX_FXAA_PARAMS) uniform FxaaParams {
  vec2 rcp_frame;
  float quality_subpix;
  float quality_edge_threshold;
  float quality_edge_threshold_min;
  int enabled;
} fxaa_params;

out vec3 frag_color;

layout(binding = 0) uniform sampler2D tex_color_luma;

// Iteration settings, corresponds to FXAA_QUALITY__PRESET == 39, i.e.,
// extreme quality.
// Note: It seems that the original source code never actually runs the 12th
// iteration with any of the predefined presets.
#define NUM_ITERATIONS 12
const float fxaa_quality_p[12] = float[12](1.0, 1.0, 1.0, 1.0, 1.0, 1.5, 2.0, 2.0, 2.0, 2.0, 4.0, 8.0);

void main() {
  const vec2 pos_m = uv;
  const vec3 rgb_m = textureLod(tex_color_luma, pos_m, 0.0).rgb;

  if (fxaa_params.enabled == 0) {
    frag_color = rgb_m;
    return;
  }

  // Get 7 luma samples with just 2 gather reads. Just 2 corners of the 3x3
  // neighborhood are missing after this fetch.
  //
  // Move the sampling position exactly into the center of the 4 texels we want
  // to gather from. If we don't do this, Mesa/llvmpipe's textureGather returns
  // mostly garbage.
  const vec2 uv_gather = pos_m + vec2(0.5) / textureSize(tex_color_luma, 0);

  const vec4 luma4a = textureGather(tex_color_luma, uv_gather, 3);
  const vec4 luma4b = textureGatherOffset(tex_color_luma, uv_gather, ivec2(-1, -1), 3);

  // 'n' and 's' are flipped compared to original:
  #define luma_m luma4a.w
  #define luma_e luma4a.z
  #define luma_n luma4a.x
  #define luma_ne luma4a.y
  #define luma_sw luma4b.w
  #define luma_s luma4b.z
  #define luma_w luma4b.x

  // Get max & min sample values along the cross centerd at the pixel
  const float range_max = max(luma_m, max(max(luma_s, luma_n), max(luma_w, luma_e))); 
  const float range_min = min(luma_m, min(min(luma_s, luma_n), min(luma_w, luma_e)));

  const float range_max_scaled = range_max * fxaa_params.quality_edge_threshold;
  const float range = range_max - range_min;
  const float range_max_clamped = max(fxaa_params.quality_edge_threshold_min, range_max_scaled);

  if (range < range_max_clamped) {
    frag_color = rgb_m;
    return;
  }

  // If we don't discard we get the luma values for the remaining two corners.
  const float luma_se = textureLodOffset(tex_color_luma, pos_m, 0.0, ivec2(1, -1)).a;
  const float luma_nw = textureLodOffset(tex_color_luma, pos_m, 0.0, ivec2(-1, 1)).a;

  // Edge differences. Reminds me of 1-D Laplacians (i.e., [1 -2 1] and 6 of
  // them in 3x3 neighborhood, 3 horizontal and 3 vertical).
  const vec3 row_n = vec3(luma_nw, luma_n, luma_ne);
  const vec3 row_m = vec3(luma_w, luma_m, luma_e);
  const vec3 row_s = vec3(luma_sw, luma_s, luma_se);
  const vec3 row_n_plus_s = row_n + row_s;
  const vec3 edge_lap_horz = abs(row_n_plus_s - 2.0 * row_m);
  const float edge_horz = edge_lap_horz.x + 2.0 * edge_lap_horz.y + edge_lap_horz.z;

  const vec3 col_w = vec3(luma_nw, luma_w, luma_sw);
  const vec3 col_m = vec3(luma_n, luma_m, luma_s);
  const vec3 col_e = vec3(luma_ne, luma_e, luma_se);
  const vec3 col_w_plus_e = col_w + col_e;
  const vec3 edge_lap_vert = abs(col_w_plus_e - 2.0 * col_m);
  const float edge_vert = edge_lap_vert.x + 2.0 * edge_lap_vert.y + edge_lap_vert.z;

  // Weighted luma average over 3x3 area without its center. Used later on for
  // sub-pixel shift.
  const float luma_avg= (1.0 / 12.0) * (2.0 * (row_n_plus_s.y + col_w_plus_e.y) + row_n_plus_s.x + row_n_plus_s.z);

  const bool horz_span = edge_horz >= edge_vert;

  // Select the 2n lumas opposite of the edge.
  const float luma_neg = horz_span ? luma_s : luma_w;
  const float luma_pos = horz_span ? luma_n : luma_e;

  // Gradients orthogonal to edge in both directions.
  const float gradient_neg = luma_neg - luma_m;
  const float gradient_pos = luma_pos - luma_m;
  const bool pair_neg = abs(gradient_neg) >= abs(gradient_pos);
  const float gradient_scaled = 0.25 * max(abs(gradient_neg), abs(gradient_pos));

  // 1 texel step lenghth in UV space
  float length_sign = horz_span ? fxaa_params.rcp_frame.y : fxaa_params.rcp_frame.x;

  // Luma on the edge along stronger gradient
  float luma_local_avg = 0.0;

  if (pair_neg) {
    length_sign = -length_sign;
    luma_local_avg = 0.5 * (luma_neg + luma_m);
  } else {
    luma_local_avg = 0.5 * (luma_pos + luma_m);
  }

  // Luma on the edge along stronger gradient
  const vec2 pos_b = pos_m + (horz_span ? vec2(0.0, length_sign * 0.5) : vec2(length_sign * 0.5, 0.0));

  // Compute UV offset steps along the edge.
  const vec2 offset = horz_span ? vec2(fxaa_params.rcp_frame.x, 0.0) : vec2(0.0, fxaa_params.rcp_frame.y);
  vec2 pos_neg = pos_b - offset /* * fxaa_quality_p[0] */;
  vec2 pos_pos = pos_b + offset /* * fxaa_quality_p[0] */;

  // Sample luma at current ends along edge, and compute diff to local average
  float luma_end_neg = textureLod(tex_color_luma, pos_neg, 0.0).a;
  float luma_end_pos = textureLod(tex_color_luma, pos_pos, 0.0).a;
  luma_end_neg -= luma_local_avg;
  luma_end_pos -= luma_local_avg;

  // If the luma deltas at the current extremities are larger than the local
  // gradient, we have reached the side of the edge.
  bool done_neg = abs(luma_end_neg) >= gradient_scaled;
  bool done_pos = abs(luma_end_pos) >= gradient_scaled;
  bool done_both = done_neg && done_pos;

  // Iteratively continue stepping along edge until both ends are reached.
  if (!done_neg) {
    pos_neg -= offset /* * fxaa_quality_p[1] */;
  }
  if (!done_pos) {
    pos_pos += offset /* * fxaa_quality_p[1] */;
  } 

  if (!done_both) {
    for (int i = 2; i < NUM_ITERATIONS; i++) {
      if (!done_neg) {
        luma_end_neg = textureLod(tex_color_luma, pos_neg, 0.0).a;
        luma_end_neg -= luma_local_avg;
      }
      if (!done_pos) {
        luma_end_pos = textureLod(tex_color_luma, pos_pos, 0.0).a;
        luma_end_pos -= luma_local_avg;
      }

      done_neg = abs(luma_end_neg) >= gradient_scaled;
      done_pos = abs(luma_end_pos) >= gradient_scaled;
      done_both = done_neg && done_pos;

      if (!done_neg) {
        pos_neg -= offset * fxaa_quality_p[i];
      }
      if (!done_pos) {
        pos_pos += offset * fxaa_quality_p[i];
      }

      if (done_both) {
        break;
      }
    }
  }

  // Distance to final end points
  const float dst_neg = horz_span ? (pos_m.x - pos_neg.x) : (pos_m.y - pos_neg.y);
  const float dst_pos = horz_span ? (pos_pos.x - pos_m.x) : (pos_pos.y - pos_m.y);
  const bool direction_neg = dst_neg < dst_pos;
  const float dst = min(dst_neg, dst_pos);
  const float span_length = dst_neg + dst_pos;

  const float pixel_offset = -dst / span_length + 0.5;

  // Compare center luma with local average, and then check that luma variations
  // are coherent
  const bool luma_m_lt_local_avg = luma_m < luma_local_avg;
  const bool good_span = ((direction_neg ? luma_end_neg : luma_end_pos) < 0.0) != luma_m_lt_local_avg;
  const float pixel_offset_good = good_span ? pixel_offset : 0.0;

  // Sub-pixel anti-aliasing
  const float subpix_c = clamp(abs(luma_avg - luma_m) / range, 0.0, 1.0);
  // Same function as used in smoothstep
  const float subpix_f = (-2.0 * subpix_c + 3.0) * subpix_c * subpix_c;
  const float sub_pixel_offset = subpix_f * subpix_f * fxaa_params.quality_subpix;

  // Final texture sample
  const float final_offset = max(pixel_offset_good, sub_pixel_offset);

  vec2 pos_m_final = pos_m;
  if (horz_span) {
    pos_m_final.y += final_offset * length_sign;
  } else {
    pos_m_final.x += final_offset * length_sign;
  }

  frag_color = textureLod(tex_color_luma, pos_m_final, 0.0).rgb;
}
)glsl";

const char* fxaa_frag_sources[] = {
    version,
    common_defines,
    fxaa_frag,
};

}  // namespace shaders

}  // namespace viewer
