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

// Based on:
// https://learnopengl.com/PBR/Theory
// https://google.github.io/filament/Filament.md.html#materialsystem/specularbrdf
// Real Shading in Unreal Engine 4
const char* pbr_shade = GLSL_LINE R"glsl(

struct DirectionalLight {
  vec3 dir;
  float intensity;
  vec3 color;
};

layout(std140, binding = UBO_BINDING_INDEX_PBR_PARAMS) uniform PBRParams {
  DirectionalLight lights[3];
  vec3 ambient_light_color;
  float ambient_light_intensity;
  int light_dirs_in_eye_space;
} pbr_params;

const float pi = 3.14159265359;

// Unreal PBR shader_sky

// Note that the roughness is assumed to always be > 0 to prevent division by 0.
float DistributionGGX(vec3 n, vec3 h, float perceptual_roughness) {
  const float a = perceptual_roughness * perceptual_roughness;
  const float a2 = a * a;
  const float n_dot_h = max(dot(n, h), 0.0);
  const float n_dot_h2 = n_dot_h * n_dot_h;

  float denom = (n_dot_h2 * (a2 - 1.0) + 1.0);
  denom = pi * denom * denom;
  return (a2 / denom);
}

// This is an approximation with removed square roots (see Eq. 16 in Filament).
// k is a parameter used in Unreal. To see how this maps to Filament's PBR, see
// slide 83 in PBR Diffuse Lighting for GGX+Smith Microsurfaces.
float GeometrySchlickGGX(float n_dot_s, float k) {
  const float denom = n_dot_s * (1.0 - k) + k;
  return (n_dot_s / denom);
}

// Non-correlated version of Smith Geometry term.
float GeometrySmithNonCorrelatedApprox(float n_dot_v, float n_dot_l, float perceptual_roughness) {
  const float r = (perceptual_roughness + 1.0);
  const float k_direct = (r * r) / 8.0;
  const float ggx2 = GeometrySchlickGGX(n_dot_v, k_direct);
  const float ggx1 = GeometrySchlickGGX(n_dot_l, k_direct);
  return (ggx1 * ggx2);
}

vec3 FresnelSchlick(float cos_theta, vec3 f0) {
  return (f0 + (1.0 - f0) * pow(1.0 - cos_theta, 5.0));
}

vec3 LambertShade(vec3 albedo, vec3 normal, vec3 position, vec3 metallic_roughness_reflectance) {
  const bool use_eye_space = pbr_params.light_dirs_in_eye_space == 1;
  vec3 light_dir = (use_eye_space ? mat3(1.0) : mat3(camera.view_matrix)) * pbr_params.lights[0].dir;
  light_dir = normalize(light_dir);
  const float diffuse = max(0.0, dot(light_dir, normal));

  const vec3 ambient = pbr_params.ambient_light_color * pbr_params.ambient_light_intensity;

  const vec3 shaded = (ambient + vec3(diffuse)) * albedo;
  return shaded;
}

vec3 PBRShadeUnreal(vec3 albedo, vec3 normal, vec3 position, vec3 metallic_roughness_reflectance) {
  const bool use_eye_space = pbr_params.light_dirs_in_eye_space == 1;

  const vec3 n = normal;
  const vec3 v = normalize(-position);

  const float metallic = metallic_roughness_reflectance.x;
  const float perceptual_roughness = metallic_roughness_reflectance.y;  // Assumed to be > 0 to prevent division by 0.
  const float reflectance = metallic_roughness_reflectance.z;

  // If dielectrics use reflectance, otherwise albedo.
  // See https://google.github.io/filament/Filament.md.html#materialsystem/parameterization/remapping
  // vec3 f0 = 0.16 * reflectance * reflectance * (1.0 - metallic) + albedo * metallic;
  vec3 f0 = vec3(0.16 * reflectance * reflectance);
  f0 = mix(f0, albedo, metallic);

  // Exitant radiance L_o
  vec3 L_o = vec3(0.0);

  for (int i = 0; i < 3; ++i) {
    // Could potentially be precomputed on the CPU, the view matrix and light_dir are uniform per frame.
    vec3 light_dir = (use_eye_space ? mat3(1.0) : mat3(camera.view_matrix)) * pbr_params.lights[i].dir;

    // Reflectance equation
    const vec3 l = normalize(light_dir);  // TODO(stefalie): pbr_params.lights[i].dir could be stored normalized in the UBO.
    const vec3 h = normalize(v + l);

    // Cook-Torrance BRDF
    const float ndf = DistributionGGX(n, h, perceptual_roughness);
    const float n_dot_v = max(dot(n, v), 0.0);
    const float n_dot_l = max(dot(n, l), 0.0);
    const float g = GeometrySmithNonCorrelatedApprox(n_dot_v, n_dot_l, perceptual_roughness);
    const vec3 f = FresnelSchlick(max(dot(h, v), 0.0), f0);

    const vec3 nominator = ndf * g * f;
    const float denominator = 4.0f * n_dot_v * n_dot_l + 0.001;  // 0.001 to prevent division by 0.
    const vec3 specular = nominator / denominator;

    // Amount reflected k_s corresponds to Fresnel term
    const vec3 k_s = f;
    // Energy conservation (k_d + k_s == 1). Filament doens't do this.
    const vec3 k_d = vec3(1.0) - k_s;

    // Diffuse
    const vec3 diffuse_color = (1.0 - metallic) * albedo;
    const vec3 diffuse = k_d * diffuse_color / pi;

    // Incident radiance
    const vec3 L_i = pbr_params.lights[i].color * pbr_params.lights[i].intensity;

    L_o += (diffuse + specular) * L_i * n_dot_l;
  }

  // Ambient light hack
  const vec3 ambient = albedo * pbr_params.ambient_light_color * pbr_params.ambient_light_intensity;

  return (ambient + L_o);
}


// Filement PBR shader_sky
// Reuses the FresnelSchlick procedure from Unreal's version above.

// Same computation as DistributionGGX above.
float DistributionGGXFilament(float n_dot_h, float a) {
    const float a2 = a * a;
    const float f = (n_dot_h * a2 - n_dot_h) * n_dot_h + 1.0;
    return a2 / (pi * f * f);
}

// The V or visibility term is a combination of G and the denominator with the
// factor 4 and two dot products of. Also, compared to Unreal, we don't have
// k_ibl or k_direct anymore but rather just a which is perceptual_roughness^2.
float VisibilitySmithGGXCorrelated(float n_dot_v, float n_dot_l, float a) {
  const float a2 = a * a;
  const float ggx_l = n_dot_v * sqrt((-n_dot_l * a2 + n_dot_l) * n_dot_l + a2);
  const float ggx_v = n_dot_l * sqrt((-n_dot_v * a2 + n_dot_v) * n_dot_v + a2);
  return 0.5 / (ggx_v + ggx_l);
}

float FresnelSchlickImproved(float v_dot_h, float f0, float f90) {
    return f0 + (f90 - f0) * pow(1.0 - v_dot_h, 5.0);
}

float DiffuseDisneyBurley(float n_dot_v, float n_dot_l, float l_dot_h, float a) {
    const float f90 = 0.5 + 2.0 * a * l_dot_h * l_dot_h;
    const float light_scatter = FresnelSchlickImproved(n_dot_l, 1.0, f90);
    const float view_scatter = FresnelSchlickImproved(n_dot_v, 1.0, f90);
    return (light_scatter * view_scatter * (1.0 / pi));
}

vec3 PBRShadeFilament(vec3 albedo, vec3 normal, vec3 position, vec3 metallic_roughness_reflectance) {
  const bool use_eye_space = pbr_params.light_dirs_in_eye_space == 1;

  const vec3 n = normal;
  const vec3 v = normalize(-position);

  const float metallic = metallic_roughness_reflectance.x;
  const float perceptual_roughness = metallic_roughness_reflectance.y;  // Assumed to be > 0 to prevent division by 0.
  const float reflectance = metallic_roughness_reflectance.z;

  // If dielectrics use reflectance, otherwise albedo.
  // See https://google.github.io/filament/Filament.md.html#materialsystem/parameterization/remapping
  // vec3 f0 = 0.16 * reflectance * reflectance * (1.0 - metallic) + albedo * metallic;
  vec3 f0 = vec3(0.16 * reflectance * reflectance);
  f0 = mix(f0, albedo, metallic);

  // Exitant radiance L_o
  vec3 L_o = vec3(0.0);

  for (int i = 0; i < 3; ++i) {
    // Could potentially be precomputed on the CPU, the view matrix and light_dir are uniform per frame.
    vec3 light_dir = (use_eye_space ? mat3(1.0) : mat3(camera.view_matrix)) * pbr_params.lights[i].dir;

    // Reflectance equation
    const vec3 l = normalize(light_dir);  // TODO(stefalie): pbr_params.lights[i].dir could be stored normalized in the UBO.
    const vec3 h = normalize(v + l);

    // Cook-Torrance BRDF
    const float n_dot_v = clamp(abs(dot(n, v)) + 1e-5, 0.0, 1.0);  // Plus 1e-5 to prevent division by 0.
    const float n_dot_l = clamp(dot(n, l), 0.0, 1.0);
    const float n_dot_h = clamp(dot(n, h), 0.0, 1.0);
    const float l_dot_h = clamp(dot(l, h), 0.0, 1.0);

    // Ssee parametrization section of Filament.
    const float a = perceptual_roughness * perceptual_roughness;

    const float d = DistributionGGXFilament(n_dot_h, a);
    const vec3 f = FresnelSchlick(l_dot_h, f0);
    const float vis = VisibilitySmithGGXCorrelated(n_dot_v, n_dot_l, a);

    // Specular BRDF
    // (For IBL Filement describes and energy preservation term that simulates
    // multiscattering, but it needs a precomputation step that happens
    // together with the other IBL pre-integrations, hence it's not an option
    // here for just direct lighting.)
    const vec3 fr = (d * vis) * f;

    // Diffuse BRDF
    const vec3 diffuse_color = (1.0 - metallic) * albedo;

    // Choose between Lambert or Disney/Burley diffuse:
    //const vec3 fd = diffuse_color / pi;
    // Disney diffuse is more expensive and supposedly hard to deal with when
    // using SH.
    const vec3 fd = diffuse_color * DiffuseDisneyBurley(n_dot_v, n_dot_l, l_dot_h, a);

    // Incident radiance
    const vec3 L_i = pbr_params.lights[i].color * pbr_params.lights[i].intensity;

    // Apply lighting
    // TODO(stefalie): Unclear if fd and fr can really just be added together,
    // the Filament documentation remains unfinished in that respect.
    L_o += (fd + fr) * L_i * n_dot_l;
  }

  // Ambient light hack
  const vec3 ambient = albedo * pbr_params.ambient_light_color * pbr_params.ambient_light_intensity;

  return (ambient + L_o);
}

// Choose one:
//#define PBRShade PBRShadeUnreal
#define PBRShade PBRShadeFilament

)glsl";

}  // namespace shaders

}  // namespace viewer
