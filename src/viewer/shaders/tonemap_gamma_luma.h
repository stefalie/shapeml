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

const char* tonemap_gamma_luma_frag = GLSL_LINE R"glsl(
out vec4 frag_color;

layout(binding = 0) uniform sampler2D tex_hdr_color;

layout(std140, binding = UBO_BINDING_INDEX_TONEMAP_GAMMA_LUMA_PARAMS) uniform TonemapGammaLumaParams {
  float exposure;
  int tonemap_mode;
  float L_white;
  float gamma;
} tonemap_gamma_luma_params;

#define TONEMAP_MODE_LINEAR            0
#define TONEMAP_MODE_REINHARD_RGB      1
#define TONEMAP_MODE_REINHARD          2
#define TONEMAP_MODE_REINHARD_EXTENDED 3
#define TONEMAP_MODE_EXPOSURE          4
#define TONEMAP_MODE_ACES_FILMIC       5
#define TONEMAP_MODE_UNCHARTED2_FILMIC 6

const vec3 tonemap_luminance_weights = vec3(0.212671, 0.71516, 0.07216);

vec3 TonemapReinhardRGB(vec3 hdr_color) {
  // From: https://learnopengl.com/Advanced-Lighting/HDR
  return (hdr_color / (hdr_color + vec3(1.0)));
}

vec3 TonemapReinhard(vec3 hdr_color) {
  // From: Reinhard et al., 2002, Photographic Tone Reproduction for Digital Images
  const float L = dot(hdr_color, tonemap_luminance_weights);
  const float Ld = L / (1.0 + L);
  const vec3 mapped_color = Ld * hdr_color / L;
  return mapped_color;
}

vec3 TonemapReinhardExtended(vec3 hdr_color) {
  // From: Reinhard et al., 2002, Photographic Tone Reproduction for Digital Images
  const float L_white = tonemap_gamma_luma_params.L_white;

  const float L = dot(hdr_color, tonemap_luminance_weights);
  const float Ld = (L * (1.0 + L / (L_white * L_white))) / (1.0 + L);
  const vec3 mapped_color = Ld * hdr_color / L;
  return clamp(mapped_color, vec3(0.0), vec3(1.0));
}

vec3 TonemapExposure(vec3 hdr_color) {
  // From: https://learnopengl.com/Advanced-Lighting/HDR
  return (vec3(1.0) - exp(-hdr_color));
}

vec3 TonemapACESFilmic(vec3 x) {
  // From: https://knarkowicz.wordpress.com/2016/01/06/aces-filmic-tone-mapping-curve/
  const float a = 2.51;
  const float b = 0.03;
  const float c = 2.43;
  const float d = 0.59;
  const float e = 0.14;
  return (x * (a * x + b)) / (x * (c * x + d) + e);
}

vec3 TonemapUncharted2Filmic(vec3 x) {
  // From: http://duikerresearch.com/2015/09/filmic-tonemapping-for-real-time-rendering/
  const float A = 0.15;
  const float B = 0.50;
  const float C = 0.10;
  const float D = 0.20;
  const float E = 0.02;
  const float F = 0.30;
  const float W = 11.2;

  // The curve:
  // f(x) = ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
  // Let's hope the optimizer simplifies this.
  vec3 curr = ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;

  const float exposure_bias = 2.0;
  curr = exposure_bias * curr;

  // white_scale = 1.0 / f(W)
  const float white_scale = 1.37906424665;

  // Hmm, no clamping even though the this might leave the [0, 1] range.
  return (curr * white_scale);
}

void main() {
  const ivec2 xy = ivec2(gl_FragCoord.xy);
  vec3 hdr_color = texelFetch(tex_hdr_color, xy, 0).rgb;
  
  const float exposure = tonemap_gamma_luma_params.exposure;
  hdr_color = hdr_color * exposure;

  vec3 tone_mapped;
  if (tonemap_gamma_luma_params.tonemap_mode == TONEMAP_MODE_LINEAR) {
    tone_mapped = hdr_color;
  } else if (tonemap_gamma_luma_params.tonemap_mode == TONEMAP_MODE_REINHARD_RGB) {
    tone_mapped = TonemapReinhardRGB(hdr_color);
  } else if (tonemap_gamma_luma_params.tonemap_mode == TONEMAP_MODE_REINHARD) {
    tone_mapped = TonemapReinhard(hdr_color);
  } else if (tonemap_gamma_luma_params.tonemap_mode == TONEMAP_MODE_REINHARD_EXTENDED) {
    tone_mapped = TonemapReinhardExtended(hdr_color);
  } else if (tonemap_gamma_luma_params.tonemap_mode == TONEMAP_MODE_EXPOSURE) {
    tone_mapped = TonemapExposure(hdr_color);
  } else if (tonemap_gamma_luma_params.tonemap_mode == TONEMAP_MODE_ACES_FILMIC) {
    tone_mapped = TonemapACESFilmic(hdr_color);
  } else /* if (tonemap_gamma_luma_params.tonemap_mode == TONEMAP_MODE_UNCHARTED2_FILMIC) */ {
    tone_mapped = TonemapUncharted2Filmic(hdr_color);
  }

  // Gamma encoding
  const float gamma = tonemap_gamma_luma_params.gamma;
  const vec3 gamma_encoded = pow(tone_mapped, vec3(1.0 / gamma));
  // Alternatively one could use a gamma of 2.0 and use a cheaper sqrt to do
  // gamma encoding as suggested in the original FXAA source code:
  // https://gist.github.com/kosua20/0c506b81b3812ac900048059d2383126
  //const vec3 gamma_encoded = sqrt(tone_mapped);  // Gamma 2.0

  // TOOO(stefalie): Or if we were smart, we would just use an SRGB target FBO.
  // This is usually done with 'glEnable(GL_FRAMEBUFFER_SRGB)' but only works if
  // the underlying framebuffer has an SRGB format. When creating FBOs in OpenGL
  // that's easy, but if it's for the default framebuffer it needs to be done
  // when creating the window with GLFW (via GLFW_SRGB_CAPABLE).

  // Luma computation suggested in the original FXAA source code.
  const vec3 luma_fxaa_weights = vec3(0.299, 0.587, 0.114);
  const float luma = dot(gamma_encoded, luma_fxaa_weights);

  frag_color = vec4(gamma_encoded, luma);
}
)glsl";

const char* tonemap_gamma_luma_frag_sources[] = {
    version,
    common_defines,
    tonemap_gamma_luma_frag,
};

}  // namespace shaders

}  // namespace viewer
