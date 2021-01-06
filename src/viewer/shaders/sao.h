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

#include "viewer/shaders/deferred.h"
#include "viewer/shaders/preamble.h"

namespace viewer {

namespace shaders {

// This is heavily based on:
// McGuire - 2012 - Scalable Ambient Obscurance

const char* sao_ubo = GLSL_LINE R"glsl(
layout(std140, binding = UBO_BINDING_INDEX_SAO_PARAMS) uniform SAOParams {
  int src_mip_level;

  float radius_ws;
  float k_constrast;
  float intensity;
  float beta_bias;
  int num_samples;
  int num_spiral_turns;

  float max_encode_depth;
  float bilateral_2x2_threshold;
  float bilateral_threshold;
  float bilateral_const_weight;
} sao_params;
)glsl";

const char* sao_depth_pyramid_frag = GLSL_LINE R"glsl(
// Note that we use a sampler2D instead of an image2D because latter one
// requires the format to be specified which conflicts with experimenting with
// both, r16f and r32f.
// layout(binding = 0, r32f/r16f) readonly uniform image2D img_depth_prev_level;
layout(binding = 0) uniform sampler2D tex_depth_prev_level;

out float subsampled_depth;

void main() {
  const ivec2 frag_coord = ivec2(gl_FragCoord.xy);

  // Rotated grid subsampling, the XOR used in the original paper is not
  // necessary (it will just shift the sampling grid).
  // Clamping the sample texture coordinates is not necessary either, they will
  // always be within the next higher mipmap level. The next higher level is always
  // floor(prev_size / 2).
  const ivec2 sample_coord = 2 * frag_coord + ivec2(frag_coord.y & 1, frag_coord.x & 1);
  subsampled_depth = texelFetch(tex_depth_prev_level, sample_coord, sao_params.src_mip_level).r;
}
)glsl";

const char* sao_depth_pyramid_frag_sources[] = {
    version,
    common_defines,
    sao_ubo,
    sao_depth_pyramid_frag,
};

const char* sao_frag = GLSL_LINE R"glsl(
layout(binding = 0) uniform sampler2D tex_linear_depth;

in vec2 uv;

out vec3 frag_ao_depth;

vec3 SamplePositionVS(ivec2 xy, vec2 u_hat, float radius) {
  const int kLogQ = 3;
  const int kMaxMipLevel = 5;
  const int mip_level = clamp(findMSB(int(radius)) - kLogQ, 0, kMaxMipLevel);

  const ivec2 sample_xy = ivec2(radius * u_hat) + xy;

  // Get tex coords for the mip level. Needs manual clamp as it doesn't go
  // through a sampler.
  const ivec2 mip_xy = clamp(sample_xy >> mip_level, ivec2(0), textureSize(tex_linear_depth, mip_level) - ivec2(1));

  const float sample_depth = texelFetch(tex_linear_depth, mip_xy, mip_level).x;
  const vec3 sample_pos_vs = ReconstructPositionFromDepthSS(sample_xy + vec2(0.5), -sample_depth);
  return sample_pos_vs;
}

// See the plot
// https://www.wolframalpha.com/input/?i=x%2F%28x%5E2+%2B+0.01%29%2C+x+in+%5B0%2C+1%5D
// and compare with Fig. 2 in the Alchemy AO paper. Since the curve maxes out at
// t = 0.1 with a value of 5, we normalize with 0.2. When we set x = t / r, we
// get f(t) = r * t / (t^2 + 0.01 * r^2)
float SampleSAO(int i, ivec2 xy, vec3 C, vec3 n_C, float radius, float start_angle) {
  // Find tap location, i.e., angle and radius.
  const float alpha = (i + 0.5) * (1.0 / sao_params.num_samples);
  const float kTwoPi = 6.2832;

  // Note that fast math cos/sin approximations are very bad for large values.
  // https://stackoverflow.com/questions/21464664/sinx-only-returns-4-different-values-for-moderately-large-input-on-glsl-fragme
  // The random start angle is a hash based on the pixel position and is large,
  // therefore we need to map it into a reasonable range to prevent banding.
  start_angle = mod(start_angle, kTwoPi);
  const float angle = alpha * (sao_params.num_spiral_turns * kTwoPi) + start_angle;
  const vec2 u_hat = vec2(cos(angle), sin(angle));
  radius *= alpha;

  const vec3 Q = SamplePositionVS(xy, u_hat, radius);
  const vec3 v = Q - C;

  const float v_dot_v = dot(v, v);
  const float v_dot_n = dot(v, n_C);

  // TODO(stefalie): Same note about llvmpipe as below.
  const float radius_squared = sao_params.radius_ws * sao_params.radius_ws;
  const float kEps = 0.01 * radius_squared;

  // Compute recommended ambient visibility term (as in the supplemental code).
  // Note: It's funny that in the paper the Heavyside step function (that
  // appears in an intermediate step in the Alchemy AO paper) magically
  // disappeared. But in the code it's there, either as 'float(v_dot_v < radius_squared)'
  // or as a version with a smoother transition to 0:
  float f = max(radius_squared - v_dot_v, 0.0);
  f = f * f * f;  // This gets normalized by 1 / r^6 in the normalizatin factor.

  // Note that the bias in the paper is modulated by z_C (which is negative btw)
  // but not in the code in the supplemental material. We skip it too.
  return f * max((v_dot_n - sao_params.beta_bias) / (v_dot_v + kEps), 0.0);
}

void main() {
  const ivec2 xy = ivec2(gl_FragCoord.xy);

  // Gather 5 linear depth samples with 2 gather instructions. The middle we
  // need anyway, left/right/up/down help reconstructing improved normals. See
  // https://atyuwen.github.io/posts/normal-reconstruction/ (we use the 5 tap
  // version).
  //
  // Move the sampling position exactly into the center of the 4 texels we want
  // to gather from. If we don't do this, Mesa/llvmpipe's textureGather returns
  // mostly garbage.
  const vec2 uv_gather = uv + vec2(0.5) / textureSize(tex_linear_depth, 0);

  const vec4 depth_vs_m_e_n = textureGather(tex_linear_depth, uv_gather, 0);
  const vec4 depth_vs_s_w = textureGatherOffset(tex_linear_depth, uv_gather, ivec2(-1, -1), 0);
  #define depth_m depth_vs_m_e_n.w
  #define depth_e depth_vs_m_e_n.z
  #define depth_n depth_vs_m_e_n.x
  #define depth_s depth_vs_s_w.z
  #define depth_w depth_vs_s_w.x

  float sign_x = 1.0;
  float sign_y = 1.0;
  float depth_other_x = depth_e;
  float depth_other_y = depth_n;
  if (abs(depth_m - depth_w) < abs(depth_e - depth_m)) {
    sign_x = -1.0;
    depth_other_x = depth_w;
  }
  if (abs(depth_m - depth_s) < abs(depth_n - depth_m)) {
    sign_y = -1.0;
    depth_other_y = depth_s;
  }

  const vec2 xy_offset = xy + vec2(0.5);
  const vec3 pos_vs         = ReconstructPositionFromDepthSS(xy_offset                    , -depth_m      );
  const vec3 pos_vs_other_x = ReconstructPositionFromDepthSS(xy_offset + vec2(sign_x, 0.0), -depth_other_x);
  const vec3 pos_vs_other_y = ReconstructPositionFromDepthSS(xy_offset + vec2(0.0, sign_y), -depth_other_y);
  const vec3 dx = sign_x * (pos_vs_other_x - pos_vs);
  const vec3 dy = sign_y * (pos_vs_other_y - pos_vs);
  const vec3 normal_vs = normalize(cross(dx, dy));

  // cross(dFdx, dFdy) version to reconstruct camera space normals from
  // positions. This causes messed up normals at discontinuities that are rather
  // hard to blur. With the approach from the paper where blurring takes 2-pixel
  // steps, it's even worse.

  // TODO(stefalie): This is very similar to what happens in deferred.h. We
  // should consider using tex_linear_depth in the differed renderer (instead of
  // the main depth buffer). And this TODO should be there, not here.
  //const float depth_vs = texelFetch(tex_linear_depth, xy, 0).x;
  //const vec3 pos_vs = ReconstructPositionFromDepthSS(xy + vec2(0.5), -depth_vs);
  //const vec3 dx = dFdx(pos_vs);
  //const vec3 dy = dFdy(pos_vs);
  //const vec3 normal_vs = normalize(cross(dx, dy));

  // TODO(stefalie): Alternatively consider feeding in the normal buffer from
  // the deferred renderer (but what about forward mode).

  // Note: No need here to discard if we're on a background pixel, the depth
  // test plus fullscreen quad at far take care of this.

  // Pixel hash function supposedly from the Alchemy Ambient Obscurance paper.
  // Note that that paper doesn't actually present the hash, it's presented for
  // for the first time in the SAO paper (afaik). Also note that the hash
  // function in the supplemental material/code (which we use here) is different
  // from what's in the paper (probably because ^ has such a weird/low
  // precedence because). Here we add parens to make very explicit what happens.
  const float rand_rot_angle = ((3 * xy.x) ^ (xy.y + xy.x * xy.y)) * 10;
  // Something easier also works well btw:
  //const float rand_rot_angle = (xy.x ^ xy.y) + xy.x * xy.y;

  // Max sampling radius in screen space. It's the projected radius of a sphere
  // around the world space position. Scale the radius projected on an imaginary
  // near plane at 1 to screen space so that the size is in pixels.
  const float disk_radius = -camera.map_ws_to_ss * sao_params.radius_ws / pos_vs.z;

  float sum = 0.0;
  for (int i = 0; i < sao_params.num_samples; ++i) {
    sum += SampleSAO(i, xy, pos_vs, normal_vs, disk_radius, rand_rot_angle);
  }

  // TODO(stefalie): Ideally the following two consts should be at global scope,
  // and this works with Intel and NVidia drivers, but Mesa/llvmpipe doesn't
  // like it.
  const float radius_squared = sao_params.radius_ws * sao_params.radius_ws;
  // Should this contain a 2 * pi factor? Probably.
  const float falloff_normalization_factor = sao_params.intensity * sao_params.radius_ws * 0.2 /
      (radius_squared * radius_squared * radius_squared * sao_params.num_samples);
  float A = max(0.0, 1.0 - sum * falloff_normalization_factor);

  // Not done in the supplemental material since they use k = 1 and can save the pow.
  //A = pow(A, sao_params.k_constrast);

  // Bilateral filter on 2x2 quad, exactly as in the paper.
  if (abs(dFdx(pos_vs.z)) < sao_params.bilateral_2x2_threshold) {
    A -= dFdx(A) * ((xy.x & 1) - 0.5);
  }
  if (abs(dFdy(pos_vs.z)) < sao_params.bilateral_2x2_threshold) {
    A -= dFdy(A) * ((xy.y & 1) - 0.5);
  }

  // Pack pos_vs.z int the outputs GB channels.
  // Just for packing, no need to match.
  const float pack_far_plane_z_inv = 1.0 / -sao_params.max_encode_depth;
  const float z_clamp = clamp(pos_vs.z * pack_far_plane_z_inv, 0.0, 1.0);

  // Round to nearest 1 / 256.
  const float tmp = floor(z_clamp * 256.0);
  vec2 z_packed;
  z_packed.x = tmp * (1.0 / 256.0);    // Int part
  z_packed.y = z_clamp * 256.0 - tmp;  // Fract part

  frag_ao_depth = vec3(A, z_packed);
}
)glsl";

const char* sao_frag_sources[] = {
    version, common_defines, camera_ubo, depth_util, sao_ubo, sao_frag,
};

// TODO(stefalie): The blur filter could probably be sped up by using linear
// sampler and by smartly computing weights/uv offsets.

const char* sao_blur_x_axis_frag = GLSL_LINE R"glsl(
#define AXIS ivec2(1, 0)
)glsl";

const char* sao_blur_y_axis_frag = GLSL_LINE R"glsl(
#define AXIS ivec2(0, 1)
#define BLEND_WITH_OUTPUT
)glsl";

const char* sao_blur_frag = GLSL_LINE R"glsl(
layout(binding = 0) uniform sampler2D tex_sao;

// A scale of 2 (as proposed in the paper) just dosen't work when there a
// slivers that are only 1 pixel thick. The 2x2 dFdx/dFdy filters in the
// previous pass might not blur all these sliver (depending on alignment of
// the 2x2 quads), and skipping pixels here will increase the artifacts.
const int scale = 1;

// 5 taps of a Gaussian distribution with sigma = 3 for an overall of 9 taps
// (symmetry). The weights are renormalized to 1, same as the code that comes
// with  the paper.
//const int radius = 5;
//const float gaussian[radius + 1] = float[](
//    0.1532,
//    0.1449,
//    0.1226,
//    0.0929,
//    0.0630
//);
// 6 taps of a Gaussian distribution with sigma = 3 for an overall of 11 taps
// (symmetry). The weights are renormalized to 1.
const int radius = 5;
const float gaussian[radius + 1] = float[](
    0.1423,
    0.1346,
    0.1139,
    0.0863,
    0.0585,
    0.0355
);
// 7 taps of a Gaussian distribution with sigma = 3 for an overall of 13 taps
// (symmetry). The weights are renormalized to 1.
//const int radius = 7;
//const float gaussian[radius + 1] = float[](
//    0.1370,
//    0.1296,
//    0.1097,
//    0.0831,
//    0.0563,
//    0.0342,
//    0.0185
//);

out vec3 frag_color;

float Decode(vec2 z_packed) {
  return (z_packed.r * (256.0 / 257.0) + z_packed.g * (1.0 / 257.0));
}

vec2 DecodePixel(ivec2 xy) {
  const vec3 tmp = texelFetch(tex_sao, xy, 0).rgb;
  return vec2(tmp.r, Decode(tmp.gb));
}

// TODO(stefalie): Consider exposing a few more of the blurring parameters in the
// GUI and maybe also allow adding 1 tap more on each side of the Gaussian.
void main() {
  const ivec2 xy = ivec2(gl_FragCoord.xy);

  const vec3 tmp = texelFetch(tex_sao, xy, 0).rgb;
  const float depth_center = Decode(tmp.gb);
  float sum = tmp.r;

  // No need here to discard if we're on a background pixel, the depth test plus
  // fullscreen quad at far take care of this.

  float w_total = gaussian[0];
  sum *= w_total;

  for (int r = -radius; r < 0; ++r) {
    const vec2 tap = DecodePixel(xy + r * AXIS * scale);

    // Radial blur
    // TODO(stefalie): Shall we keep this + constant? It's also done in the
    // supplemental code that comes with the paper (with a value of 0.3). I find
    // it weird as makes all weights larger than the center weight. It looks
    // okay though.
    float w = gaussian[abs(r)] + sao_params.bilateral_const_weight;

    // Depth difference/bilateral weight
    // The weight becomes 0 when depth difference is > 0.0005.
    w *= max(0.0, 1.0 - sao_params.bilateral_threshold * abs(tap.y - depth_center));

	sum += tap.x * w;
	w_total += w;
  }

  // Identical to above loop but for the positive side.
  for (int r = 1; r <= radius; ++r) {
    const vec2 tap = DecodePixel(xy + r * AXIS * scale);
    float w = gaussian[abs(r)];
    w *= max(0.0, 1.0 - sao_params.bilateral_threshold * abs(tap.y - depth_center));
    sum += tap.x * w;
    w_total += w;
  }

  const float eps = 0.0001;
  const float result = sum / (w_total + eps);

#ifdef BLEND_WITH_OUTPUT
  frag_color = vec3(result);
#else
  frag_color = vec3(result, tmp.gb);
#endif
}
)glsl";

const char* sao_blur_x_frag_sources[] = {
    version, common_defines, sao_ubo, sao_blur_x_axis_frag, sao_blur_frag,
};

const char* sao_blur_y_frag_sources[] = {
    version, common_defines, sao_ubo, sao_blur_y_axis_frag, sao_blur_frag,
};

}  // namespace shaders

}  // namespace viewer
