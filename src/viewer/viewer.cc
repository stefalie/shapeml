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

#include "viewer/viewer.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <thread>
#define STB_IMAGE_WRITE_IMPLEMENTATION
#ifdef _WIN32
#define STBI_MSC_SECURE_CRT
#endif
#include <stb/stb_image_write.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <imgui/imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include "viewer/gl/renderer.h"
#include "viewer/gl/shader.h"
#include "viewer/gl/texture_manager.h"
#include "viewer/shaders/csm.h"
#include "viewer/shaders/debug_render_order.h"
#include "viewer/shaders/deferred.h"
#include "viewer/shaders/forward.h"
#include "viewer/shaders/fullscreen_quad.h"
#include "viewer/shaders/fxaa.h"
#include "viewer/shaders/pbr.h"
#include "viewer/shaders/sao.h"
#include "viewer/shaders/tonemap_gamma_luma.h"
#include "viewer/shaders/transparency.h"

extern const unsigned int font_roboto_regular_compressed_size;
extern const unsigned int font_roboto_regular_compressed_data[];

namespace viewer {

using Eigen::Affine3f;
using Eigen::AlignedBox3f;
using Eigen::Isometry3f;
using Eigen::Matrix3f;
using Eigen::Matrix4f;
using Eigen::Quaternionf;
using Eigen::Translation3f;
using Eigen::Vector2f;
using Eigen::Vector3f;
using Eigen::Vector4f;

// Note that this function, as well as OrthogonalProjection and LookAt below,
// use Eigen addressing mode, i.e., matrix(i, j) accesses the element in row i
// in column j even though Eigen internally stores data in col-major format.
// If we ever switch to glm we have to keep in mind to change how the lements
// are addressed (glm uses col-major indices).
static Matrix4f PerspectiveProjection(float fovy, float aspect, float near,
                                      float far) {
  assert(aspect > 0.0f);
  assert(far > near);
  assert(near > 0.0f);
  Matrix4f projection = Matrix4f::Zero();
  const float rad_f = static_cast<float>(M_PI) * fovy / 180.0f;
  const float tan_half_fovy = tan(rad_f / 2.0f);
  projection(0, 0) = 1.0f / (aspect * tan_half_fovy);  // == 2 * near / width
  projection(1, 1) = 1.0f / tan_half_fovy;             // == 2 * near / height
  projection(2, 2) = -(far + near) / (far - near);
  projection(3, 2) = -1.0f;
  projection(2, 3) = -(2.0f * far * near) / (far - near);
  return projection;
}

// See note in 'PerspectiveProjection'.
static Matrix4f OrthogonalProjection(float left, float right, float bottom,
                                     float top, float near, float far) {
  const float right_min_left = right - left;
  const float top_min_bottom = top - bottom;
  const float far_min_near = far - near;
  assert(right_min_left);
  assert(top_min_bottom);
  assert(far_min_near);
  Matrix4f ortho = Matrix4f::Zero();
  ortho(0, 0) = 2.0f / right_min_left;
  ortho(1, 1) = 2.0f / top_min_bottom;
  ortho(2, 2) = -2.0f / far_min_near;
  ortho(0, 3) = -(right + left) / right_min_left;
  ortho(1, 3) = -(top + bottom) / top_min_bottom;
  ortho(2, 3) = -(far + near) / far_min_near;
  ortho(3, 3) = 1.0f;
  return ortho;
}

// See note in 'PerspectiveProjection'.
static Matrix4f LookAt(const Vector3f& eye, const Vector3f& center,
                       const Vector3f& up) {
  const Vector3f z_cam = (eye - center).normalized();
  const Vector3f x_cam = up.cross(z_cam).normalized();
  const Vector3f y_cam = z_cam.cross(x_cam);

  Matrix4f look_at;
  look_at(0, 0) = x_cam.x();
  look_at(0, 1) = x_cam.y();
  look_at(0, 2) = x_cam.z();
  look_at(1, 0) = y_cam.x();
  look_at(1, 1) = y_cam.y();
  look_at(1, 2) = y_cam.z();
  look_at(2, 0) = z_cam.x();
  look_at(2, 1) = z_cam.y();
  look_at(2, 2) = z_cam.z();
  look_at(0, 3) = -x_cam.dot(eye);
  look_at(1, 3) = -y_cam.dot(eye);
  look_at(2, 3) = -z_cam.dot(eye);
  look_at(3, 3) = 1.0f;
  look_at(3, 0) = look_at(3, 1) = look_at(3, 2) = 0.0f;
  return look_at;
}

// TODO(stefalie): Consider removing as it's not used.
// Another perspective division will be required after calling this.
// See:
// https://stackoverflow.com/questions/25463735/w-coordinate-in-inverse-projection
// See note in 'PerspectiveProjection'.
static Matrix4f InvertPerspectiveProjection(const Matrix4f& proj) {
  // Projection matrix:
  // a 0 0 0
  // 0 b 0 0
  // 0 0 c d
  // 0 0 e 0
  // Inverse projection:
  // 1 / a, 0, 0, 0,
  // 0, 1 / b, 0, 0,
  // 0, 0, 0, 1 / e,
  // 0, 0, 1 / d, -c / (d * e)
  const float a = proj(0, 0);
  const float b = proj(1, 1);
  const float c = proj(2, 2);
  const float e = proj(3, 2);
  const float d = proj(2, 3);
  Matrix4f inverse = Matrix4f::Zero();
  inverse(0, 0) = 1.0f / a;
  inverse(1, 1) = 1.0f / b;
  inverse(3, 2) = 1.0f / d;
  inverse(2, 3) = 1.0f / e;
  inverse(3, 3) = -c / (d * e);
  return inverse;
}

// See note in 'PerspectiveProjection'.
static Matrix4f InvertIsometry(const Matrix4f& isometry) {
  // x' = R * x + t
  // x'- t = R * x
  // R^T * x' - R^T * t = x
  Matrix4f inverse = Matrix4f::Zero();
  const Matrix3f new_rotation = isometry.block(0, 0, 3, 3).transpose();
  inverse.block(0, 0, 3, 3) = new_rotation;
  inverse.block(0, 3, 3, 1) = -new_rotation * isometry.block(0, 3, 3, 1);
  inverse(3, 3) = 1.0f;
  return inverse;
}

// TODO(stefalie): Hard coding all the bindings is becoming a pain, can we think
// of a better option?

// Keep in sync with preamble.h.
enum UboBindingIndex {
  UBO_BINDING_INDEX_CAMERA,
  UBO_BINDING_INDEX_FXAA_PARAMS,
  UBO_BINDING_INDEX_TONEMAP_GAMMA_LUMA_PARAMS,
  UBO_BINDING_INDEX_PBR_PARAMS,
  UBO_BINDING_INDEX_SHADOW_MAP_PARAMS,
  UBO_BINDING_INDEX_BACKGROUND_AND_SKY,
  UBO_BINDING_INDEX_PREFIX_SUM_PARAMS,
  UBO_BINDING_INDEX_SAO_PARAMS,
  // Debug UBOS:
  UBO_BINDING_INDEX_DEBUG_RENDER_ORDER = 16,
  UBO_BINDING_INDEX_DEBUG_DISPLAY_TEXTURE,
  UBO_BINDING_INDEX_DEBUG_SHADOWS,
};

// Keep in sync with preamble.h and renderer.h.
// Needs to be >= gl::Renderer::kMaxNumTexturesPerBatch.
enum SamplerBindingIndex {
  SAMPLER_BINDING_SHADOW_MAP = 12,
};

// Keep in sync with preamble.h and renderer.h.
// Needs to be >= max in gl::Renderer::SSBOBindingIndex.
enum SSBOBindingIndex {
  SSBO_BINDING_INDEX_LFB_COUNT = 2,
  SSBO_BINDING_INDEX_LFB_SUMS,
  SSBO_BINDING_INDEX_LFB_FRAGS,
  SSBO_BINDING_INDEX_LFB_LINKED_LIST_HEADER,
  SSBO_BINDING_INDEX_LFB_LINKED_LIST_NEXT_PTRS,
};

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

bool Viewer::Init(const std::string& title, unsigned width, unsigned height,
                  unsigned num_samples) {
  // Initialize GLFW:
  auto error_callback = [](int /*error*/, const char* description) {
    std::cerr << "ERROR: " << description << '\n';
  };
  glfwSetErrorCallback(error_callback);
  if (!glfwInit()) {
    return true;
  }

  // Window creation:

  // No dealing with resize callbacks. If we allow resizing, we have to make
  // sure to also resize and reallocate all the FBOs and render buffers whose
  // size is the same as the main framebuffer's, and I don't want to do that).
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

  // If one wants to find the max number of multisampling samples, one first has
  // to create a temporary window with an OpenGL context to query the number,
  // and then close it again.
  // And while we're at it, we can also query the DPI scale factors.
  {
    assert(num_samples > 0);

    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    window_ = glfwCreateWindow(1, 1, "dummy window", nullptr, nullptr);
    if (!window_) {
      glfwTerminate();
      return true;
    }
    glfwMakeContextCurrent(window_);
    if (!gl::Init()) {
      glfwDestroyWindow(window_);
      glfwTerminate();
      return true;
    }
    float dpi_x_scale, dpi_y_scale;
    glfwGetWindowContentScale(window_, &dpi_x_scale, &dpi_y_scale);
    dpi_scale_ = dpi_x_scale;

    glGetIntegerv(GL_MAX_SAMPLES, &max_num_samples_);
    glfwDestroyWindow(window_);
    if (num_samples > static_cast<unsigned>(max_num_samples_)) {
      assert(false);
      num_samples = max_num_samples_;
    }
    glfwWindowHint(GLFW_SAMPLES, num_samples);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
  }

  // The color channel bits are all set to 8, that's fine. For the depth bits,
  // however, we want 32 Instead of 24.
  glfwWindowHint(GLFW_DEPTH_BITS, 32);

  // TODO(stefalie): Consider using an SRGB default framebuffer:
  // glfwWindowHint(GLFW_SRGB_CAPABLE, GLFW_TRUE);
  // This will require a 'glEnable(GL_FRAMEBUFFER_SRGB);' after creating the
  // window.
  // If the default framebuffer supports SRGB can be queried this way:
  // GLint enc = -1;
  // glGetNamedFramebufferAttachmentParameteriv(0, GL_FRONT_LEFT,
  // GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING, &enc);
  // Then 'enc == GL_LINEAR' or 'enc == GL_SRGB'. It seems the result is
  // sometimes buggy:
  // https://forums.developer.nvidia.com/t/gl-framebuffer-srgb-functions-incorrectly/34889/8

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#ifndef NDEBUG
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#else
  glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_FALSE);
#endif
  const int dpi_scaled_width = static_cast<int>(width * dpi_scale_);
  const int dpi_scaled_height = static_cast<int>(height * dpi_scale_);
  window_ = glfwCreateWindow(dpi_scaled_width, dpi_scaled_height, title.c_str(),
                             nullptr, nullptr);
  if (!window_) {
    glfwTerminate();
    return true;
  }

  glfwMakeContextCurrent(window_);
  glfwGetFramebufferSize(window_, &framebuffer_width_, &framebuffer_height_);

  glfwSwapInterval(static_cast<int>(render_settings_.vsync));

  if (!gl::Init()) {
    glfwDestroyWindow(window_);
    glfwTerminate();
    return true;
  }

  // Setup ImGui:
  ImGui::CreateContext();
  ImGui::GetStyle().ScaleAllSizes(dpi_scale_);
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  // This here doesn't accomplish what we want, better to scale the font below.
  // io.FontGlobalScale = dpi_scale_;
  io.Fonts->AddFontFromMemoryCompressedTTF(font_roboto_regular_compressed_data,
                                           font_roboto_regular_compressed_size,
                                           15.0f * dpi_scale_);
  ImGui_ImplGlfw_InitForOpenGL(window_, true);
  ImGui_ImplOpenGL3_Init();
  ImGui::StyleColorsDark();
  ImGui::GetStyle().FrameRounding = 4.0f;

  // Callbacks:
  //
  // ImGui already setup most of the callbacks. We overwrite some of these
  // callbacks here and will then explicitly call the ImGui functions when
  // required.
  //
  // We cannot use methods as callbacks, that's why use lambdas as callbacks
  // that call our methods.
  glfwSetWindowUserPointer(window_, this);
  auto key_callback = [](GLFWwindow* window, int key, int scancode, int action,
                         int mods) {
    Viewer* viewer = static_cast<Viewer*>(glfwGetWindowUserPointer(window));
    viewer->KeyboardEvent(key, scancode, action, mods);
  };
  auto mouse_button_callback = [](GLFWwindow* window, int button, int action,
                                  int mods) {
    Viewer* viewer = static_cast<Viewer*>(glfwGetWindowUserPointer(window));
    viewer->MouseButtonEvent(button, action, mods);
  };
  auto cursor_pos_callback = [](GLFWwindow* window, double xpos, double ypos) {
    Viewer* viewer = static_cast<Viewer*>(glfwGetWindowUserPointer(window));
    viewer->MouseMoveEvent(xpos, ypos);
  };
  auto scroll_callback = [](GLFWwindow* window, double xoffset,
                            double yoffset) {
    Viewer* viewer = static_cast<Viewer*>(glfwGetWindowUserPointer(window));
    viewer->MouseScrollEvent(xoffset, yoffset);
  };

  // These will overwrite the callbacks set by ImGui. We wil explicitly call the
  // ImGui callbacks when required.
  glfwSetKeyCallback(window_, key_callback);
  glfwSetMouseButtonCallback(window_, mouse_button_callback);
  glfwSetCursorPosCallback(window_, cursor_pos_callback);
  glfwSetScrollCallback(window_, scroll_callback);

  // Setup OpenGL state:
#ifndef NDEBUG
  // glEnable(GL_DEBUG_OUTPUT);  // Should be default-enabled in debug context.
  glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
  glDebugMessageCallback(gl::DebugMessageCallback, nullptr);
  glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE,
                        GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
#endif

  glEnable(GL_CULL_FACE);
  glClearColor(bg_sky_.params.clear_color[0] * bg_sky_.params.clear_multiplier,
               bg_sky_.params.clear_color[1] * bg_sky_.params.clear_multiplier,
               bg_sky_.params.clear_color[2] * bg_sky_.params.clear_multiplier,
               1.0f);
  if (num_samples > 1) {
    glEnable(GL_MULTISAMPLE);
  }

  update_camera_matrices_ = true;
  update_shadow_matrices_ = true;

  {  // Samplers
    // Bilinear sampler
    sampler_bilinear_.Create("Sampler: bilinear");
    sampler_bilinear_.SetParameterInt(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    sampler_bilinear_.SetParameterInt(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    sampler_bilinear_.SetParameterInt(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    sampler_bilinear_.SetParameterInt(GL_TEXTURE_MIN_FILTER, GL_LINEAR);

    // Nearest sampler for image to image operations
    sampler_nearest_.Create("Sampler: nearest");
    sampler_nearest_.SetParameterInt(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    sampler_nearest_.SetParameterInt(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    sampler_nearest_.SetParameterInt(GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    sampler_nearest_.SetParameterInt(GL_TEXTURE_MIN_FILTER, GL_NEAREST);

    // Bilinear sampler with mip mapping
    sampler_bilinear_mips_.Create("Sampler: bilinear mips");
    sampler_bilinear_mips_.SetParameterInt(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    sampler_bilinear_mips_.SetParameterInt(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    sampler_bilinear_mips_.SetParameterInt(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    sampler_bilinear_mips_.SetParameterInt(GL_TEXTURE_MIN_FILTER,
                                           GL_LINEAR_MIPMAP_LINEAR);

    // Set sampler parameters for all used texture units.
    const gl::Sampler sampler = gl::TextureManager::Get().default_sampler();
    for (unsigned i = 0; i < gl::Renderer::kMaxNumTexturesPerBatch; ++i) {
      sampler.BindToUnit(gl::SAMPLER_LOCATION_START_INDEX_MATERIALS + i);
    }
  }

  {  // Camera
    // UBO
    camera_.ubo_host.framebuffer_width = framebuffer_width_;
    camera_.ubo_host.framebuffer_height = framebuffer_height_;
    camera_.ubo_host.aspect_ratio =
        static_cast<float>(framebuffer_width_) / framebuffer_height_;

    // Most fields in the UBO are still undefined at this point, for example the
    // view or projection matrix amongst others. These will get set either in
    // the call to UpdateCameraProjection() just below or in the first call to
    // UpdateCameraViewMatrix().
    camera_.ubo.Create("UBO: Camera");
    camera_.ubo.Alloc(sizeof(camera_.ubo_host), &camera_.ubo_host,
                      GL_MAP_WRITE_BIT);
    camera_.ubo.BindBase(GL_UNIFORM_BUFFER, UBO_BINDING_INDEX_CAMERA);

    UpdateCameraProjection();

    ResetCamera();
  }

  {  // BG & sky
    // Shader
    bg_sky_.shader_sky = new gl::Shader("Program: sky");
    bg_sky_.shader_sky->BuildShaderFromSource(GL_VERTEX_SHADER, 1,
                                              &shaders::fullscreen_quad_vert,
                                              "Shader vert: sky");
    bg_sky_.shader_sky->BuildShaderFromSource(
        GL_FRAGMENT_SHADER, ARRAY_SIZE(shaders::sky_frag_sources),
        shaders::sky_frag_sources, "Shader frag: sky");
    bg_sky_.shader_sky->BuildProgram();

    bg_sky_.shader_plane = new gl::Shader("Program: infinite plane");
    bg_sky_.shader_plane->BuildShaderFromSource(GL_VERTEX_SHADER, 1,
                                                &shaders::fullscreen_quad_vert,
                                                "Shader vert: infinite plane");
    bg_sky_.shader_plane->BuildShaderFromSource(
        GL_FRAGMENT_SHADER, ARRAY_SIZE(shaders::infinite_plane_frag_sources),
        shaders::infinite_plane_frag_sources, "Shader frag: infinite plane");
    bg_sky_.shader_plane->BuildProgram();

    // UBO
    bg_sky_.ubo.Create("UBO: Background/Sky");
    bg_sky_.ubo.Alloc(sizeof(BackgroundAndSky::UBO), &bg_sky_.ubo_host,
                      GL_MAP_WRITE_BIT);
    bg_sky_.ubo.BindBase(GL_UNIFORM_BUFFER,
                         UBO_BINDING_INDEX_BACKGROUND_AND_SKY);
  }

  {  // PBR
    // Forward shader
    pbr_.shader_forward = new gl::Shader("Program: forward");
    pbr_.shader_forward->BuildShaderFromSource(
        GL_VERTEX_SHADER, ARRAY_SIZE(shaders::mesh_transform_vert_sources),
        shaders::mesh_transform_vert_sources, "Shader vert: forward");
    pbr_.shader_forward->BuildShaderFromSource(
        GL_FRAGMENT_SHADER, ARRAY_SIZE(shaders::forward_frag_sources),
        shaders::forward_frag_sources, "Shader frag: forward");
    pbr_.shader_forward->BuildProgram();
    // Bind the samplers to their respective texture units in the shader.
    // Unfortunately that can not be done directly in the shader via
    // 'binding = X' since we declare an array of sampler and not just a single
    // one.
    for (unsigned i = 0; i < gl::Renderer::kMaxNumTexturesPerBatch; ++i) {
      glProgramUniform1i(pbr_.shader_forward->program(),
                         gl::SAMPLER_LOCATION_START_INDEX_MATERIALS + i, i);
    }

    // Deferred shader
    pbr_.shader_gbuffer = new gl::Shader("Program: gbuffer");
    pbr_.shader_gbuffer->BuildShaderFromSource(
        GL_VERTEX_SHADER, ARRAY_SIZE(shaders::mesh_transform_vert_sources),
        shaders::mesh_transform_vert_sources, "Shader vert: gbuffer");
    pbr_.shader_gbuffer->BuildShaderFromSource(
        GL_FRAGMENT_SHADER, ARRAY_SIZE(shaders::gbuffer_frag_sources),
        shaders::gbuffer_frag_sources, "Shader frag: gbuffer");
    pbr_.shader_gbuffer->BuildProgram();
    // See comment above.
    for (unsigned i = 0; i < gl::Renderer::kMaxNumTexturesPerBatch; ++i) {
      glProgramUniform1i(pbr_.shader_gbuffer->program(),
                         gl::SAMPLER_LOCATION_START_INDEX_MATERIALS + i, i);
    }

    // Deferred resolve shader
    pbr_.shader_deferred = new gl::Shader("Program: deferred");
    pbr_.shader_deferred->BuildShaderFromSource(GL_VERTEX_SHADER, 1,
                                                &shaders::fullscreen_quad_vert,
                                                "Shader vert: deferred");
    pbr_.shader_deferred->BuildShaderFromSource(
        GL_FRAGMENT_SHADER, ARRAY_SIZE(shaders::deferred_resolve_frag_sources),
        shaders::deferred_resolve_frag_sources, "Shader frag: deferred");
    pbr_.shader_deferred->BuildProgram();

    pbr_.shader_reconstruct_linear_depth =
        new gl::Shader("Program: reconstruct linear depth");
    pbr_.shader_reconstruct_linear_depth->BuildShaderFromSource(
        GL_VERTEX_SHADER, 1, &shaders::fullscreen_quad_vert,
        "Shader vert: reconstruct linear depth");
    pbr_.shader_reconstruct_linear_depth->BuildShaderFromSource(
        GL_FRAGMENT_SHADER,
        ARRAY_SIZE(shaders::reconstruct_linear_depth_frag_sources),
        shaders::reconstruct_linear_depth_frag_sources,
        "Shader frag: reconstruct linear depth");
    pbr_.shader_reconstruct_linear_depth->BuildProgram();

    // Render output FBO
    pbr_.render_output.tex_color.Create(GL_TEXTURE_2D,
                                        "Render output tex: color");
    pbr_.render_output.tex_color.Alloc(false, GL_RGB16F, framebuffer_width_,
                                       framebuffer_height_);

    pbr_.render_output.tex_depth.Create(GL_TEXTURE_2D,
                                        "Render output tex: depth");
    pbr_.render_output.tex_depth.Alloc(false, GL_DEPTH_COMPONENT32,
                                       framebuffer_width_, framebuffer_height_);

    pbr_.render_output.fbo_forward.Create("Render output forward FBO");
    pbr_.render_output.fbo_forward.Alloc(
        {}, {{&pbr_.render_output.tex_depth, GL_DEPTH_ATTACHMENT},
             {&pbr_.render_output.tex_color, GL_COLOR_ATTACHMENT0}});

    pbr_.render_output.fbo_deferred.Create("Render output deferred FBO");
    pbr_.render_output.fbo_deferred.Alloc(
        {}, {{&pbr_.render_output.tex_color, GL_COLOR_ATTACHMENT0}});

    // Depth pyramid
    // TODO(stefalie): Maybe it would make more sense to to have a separate
    // depth pre-pass and to output linear depth from it instead of converting
    // it from the depth buffer. Then again, the rendered objects are mostly
    // sorted and a pre-pass might not make much sense.
    pbr_.render_output.linear_depth_pyramid.tex.Create(
        GL_TEXTURE_2D, "Linear depth pyramid tex");
    GLenum lin_depth_format = GL_R16F;
    if (pbr_.params.linear_depth_bits == PBR::LinarDepthBits::LIN_DEPTH_32) {
      lin_depth_format = GL_R32F;
    }
    pbr_.render_output.linear_depth_pyramid.tex.Alloc(
        true, lin_depth_format, framebuffer_width_, framebuffer_height_);

    pbr_.render_output.linear_depth_pyramid.fbo.Create(
        "Linear depth pyramid FBO");
    pbr_.render_output.linear_depth_pyramid.fbo.Alloc(
        {},
        {{&pbr_.render_output.linear_depth_pyramid.tex, GL_COLOR_ATTACHMENT0}});

    // MSAA FBOs
    const int n_samples = pbr_.params.forward_num_samples;
    if (n_samples > 1) {
      AllocMSAAFBOs(&pbr_.render_output.msaa, framebuffer_width_,
                    framebuffer_height_, n_samples);
    }

    // GBuffer FBO
    pbr_.gbuffer.tex_albedo.Create(GL_TEXTURE_2D, "GBuffer tex: albedo");
    pbr_.gbuffer.tex_albedo.Alloc(false, GL_RGBA8, framebuffer_width_,
                                  framebuffer_height_);

    pbr_.gbuffer.tex_normals.Create(GL_TEXTURE_2D, "GBuffer tex: normals");
    pbr_.gbuffer.tex_normals.Alloc(false, GL_RGB16F, framebuffer_width_,
                                   framebuffer_height_);

    pbr_.gbuffer.tex_material.Create(GL_TEXTURE_2D,
                                     "GBuffer tex: metal/rough/reflect");
    pbr_.gbuffer.tex_material.Alloc(false, GL_RGB8, framebuffer_width_,
                                    framebuffer_height_);

    pbr_.gbuffer.fbo.Create("GBuffer FBO");
    pbr_.gbuffer.fbo.Alloc(
        {}, {{&pbr_.render_output.tex_depth, GL_DEPTH_ATTACHMENT},
             {&pbr_.gbuffer.tex_albedo, GL_COLOR_ATTACHMENT0},
             {&pbr_.gbuffer.tex_normals, GL_COLOR_ATTACHMENT0 + 1},
             {&pbr_.render_output.linear_depth_pyramid.tex,
              GL_COLOR_ATTACHMENT0 + 2},
             {&pbr_.gbuffer.tex_material, GL_COLOR_ATTACHMENT0 + 3}});

    // UBO
    pbr_.ubo.Create("UBO: PBR params");
    pbr_.ubo.Alloc(sizeof(PBR::UBO), &pbr_.ubo_host, GL_MAP_WRITE_BIT);
    pbr_.ubo.BindBase(GL_UNIFORM_BUFFER, UBO_BINDING_INDEX_PBR_PARAMS);
  }

  {  // OIT
    Transparency::WeightedBlendedOrderIndependentTransparency& oit =
        transparency_.oit;

    // Accumulation FBO
    oit.fb.tex_accum.Create(GL_TEXTURE_2D, "OIT accumulation tex");
    oit.fb.tex_accum.Alloc(false, GL_RGBA16F, framebuffer_width_,
                           framebuffer_height_);
    // TODO(stefalie): Is 16 bit too much for revealage?
    oit.fb.tex_revealage.Create(GL_TEXTURE_2D, "OIT revealage tex");
    oit.fb.tex_revealage.Alloc(false, GL_R16F, framebuffer_width_,
                               framebuffer_height_);

    oit.fb.fbo.Create("OIT accumulation FBO");
    // Note that the depth buffer is shared with the PBR FBO.
    oit.fb.fbo.Alloc({}, {{&pbr_.render_output.tex_depth, GL_DEPTH_ATTACHMENT},
                          {&oit.fb.tex_accum, GL_COLOR_ATTACHMENT0},
                          {&oit.fb.tex_revealage, GL_COLOR_ATTACHMENT0 + 1}});

    // Shader
    // TODO(stefalie): HACK. The intel driver will throw the infamous
    // "API_ID_RECOMPILE_FRAGMENT_SHADER performance warning has been generated.
    // Fragment shader recompiled due to state change." warning unless we
    // compile the following shader with the exact blend state and FBO enabled
    // that will be used later on with this shader.
    glEnable(GL_BLEND);
    glBlendFunci(0, GL_ONE, GL_ONE);
    glBlendFunci(1, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
    oit.fb.fbo.Bind();

    oit.shader_accum = new gl::Shader("Program: OIT accumulation");
    oit.shader_accum->BuildShaderFromSource(
        GL_VERTEX_SHADER, ARRAY_SIZE(shaders::mesh_transform_vert_sources),
        shaders::mesh_transform_vert_sources, "Shader vert: OIT accumulation");
    oit.shader_accum->BuildShaderFromSource(
        GL_FRAGMENT_SHADER, ARRAY_SIZE(shaders::oit_accum_frag_sources),
        shaders::oit_accum_frag_sources, "Shader frag: OIT accumulation");
    oit.shader_accum->BuildProgram();

    // Undo above's hack.
    glDisable(GL_BLEND);

    oit.shader_resolve = new gl::Shader("Program: OIT resolve");
    oit.shader_resolve->BuildShaderFromSource(GL_VERTEX_SHADER, 1,
                                              &shaders::fullscreen_quad_vert,
                                              "Shader vert: OIT resolve");
    oit.shader_resolve->BuildShaderFromSource(GL_FRAGMENT_SHADER, 1,
                                              &shaders::oit_resolve_frag,
                                              "Shader frag: OIT resolve");
    oit.shader_resolve->BuildProgram();
  }

  {  // LFB
    Transparency::LayeredFragmentBuffer& lfb = transparency_.lfb;
    lfb.ubo_host.num_elements = framebuffer_width_ * framebuffer_height_;
    // Arbitrary max number of transparent fragments on the screen. Here we take
    // thrice the total number of pixels. If there are more transparent
    // fragments in the current frame, they won't be displayed and you'll see
    // artifacts.
    // TODO(stefalie): Allow the user to change this.
    lfb.ubo_host.max_num_fragments = 3 * lfb.ubo_host.num_elements;

    // Shader
    lfb.shader_count = new gl::Shader("Program: LFB count");
    lfb.shader_count->BuildShaderFromSource(
        GL_VERTEX_SHADER, ARRAY_SIZE(shaders::lfb_count_vert_sources),
        shaders::lfb_count_vert_sources, "Shader vert: LFB count");
    lfb.shader_count->BuildShaderFromSource(
        GL_FRAGMENT_SHADER, ARRAY_SIZE(shaders::lfb_count_frag_sources),
        shaders::lfb_count_frag_sources, "Shader frag: LFB count");
    lfb.shader_count->BuildProgram();

    lfb.shader_prefix_sum = new gl::Shader("Program: LFB prefix sum");
    lfb.shader_prefix_sum->BuildShaderFromSource(
        GL_COMPUTE_SHADER, ARRAY_SIZE(shaders::lfb_prefix_sum_comp_sources),
        shaders::lfb_prefix_sum_comp_sources, "Shader comp: LFB prefix sum");
    lfb.shader_prefix_sum->BuildProgram();

    lfb.shader_prefix_sum_adjust_incr =
        new gl::Shader("Program: LFB prefix sum");
    lfb.shader_prefix_sum_adjust_incr->BuildShaderFromSource(
        GL_COMPUTE_SHADER,
        ARRAY_SIZE(shaders::lfb_prefix_sum_adjust_incr_comp_sources),
        shaders::lfb_prefix_sum_adjust_incr_comp_sources,
        "Shader comp: LFB prefix sum adjust increment");
    lfb.shader_prefix_sum_adjust_incr->BuildProgram();

    lfb.shader_fill = new gl::Shader("Program: LFB fill");
    // TODO(stefalie): Separate shaders and program. We're compiling/using the
    // same shader the 3rd time here (forward/gbuffer use it too). Allow
    // attaching already compiled shaders to a program. Same goes for all the
    // uses of the fullscreen quad vertex shader.
    lfb.shader_fill->BuildShaderFromSource(
        GL_VERTEX_SHADER, ARRAY_SIZE(shaders::mesh_transform_vert_sources),
        shaders::mesh_transform_vert_sources, "Shader vert: LFB fill");
    lfb.shader_fill->BuildShaderFromSource(
        GL_FRAGMENT_SHADER, ARRAY_SIZE(shaders::lfb_fill_frag_sources),
        shaders::lfb_fill_frag_sources, "Shader frag: LFB fill");
    lfb.shader_fill->BuildProgram();

    lfb.shader_fill_ll = new gl::Shader("Program: LFB fill LL");
    lfb.shader_fill_ll->BuildShaderFromSource(
        GL_VERTEX_SHADER, ARRAY_SIZE(shaders::mesh_transform_vert_sources),
        shaders::mesh_transform_vert_sources, "Shader vert: LFB fill LL");
    lfb.shader_fill_ll->BuildShaderFromSource(
        GL_FRAGMENT_SHADER,
        ARRAY_SIZE(shaders::lfb_fill_linked_list_frag_sources),
        shaders::lfb_fill_linked_list_frag_sources, "Shader frag: LFB fill LL");
    lfb.shader_fill_ll->BuildProgram();

    lfb.shader_sort_and_blend = new gl::Shader("Program: LFB sort & blend");
    lfb.shader_sort_and_blend->BuildShaderFromSource(
        GL_VERTEX_SHADER, 1, &shaders::fullscreen_quad_vert,
        "Shader vert: LFB sort & blend");
    lfb.shader_sort_and_blend->BuildShaderFromSource(
        GL_FRAGMENT_SHADER,
        ARRAY_SIZE(shaders::lfb_sort_and_blend_frag_sources),
        shaders::lfb_sort_and_blend_frag_sources,
        "Shader frag: LFB sort & blend");
    lfb.shader_sort_and_blend->BuildProgram();

    lfb.shader_adaptive_blend = new gl::Shader("Program: LFB adaptive blend");
    lfb.shader_adaptive_blend->BuildShaderFromSource(
        GL_VERTEX_SHADER, 1, &shaders::fullscreen_quad_vert,
        "Shader vert: LFB adaptive blend");
    lfb.shader_adaptive_blend->BuildShaderFromSource(
        GL_FRAGMENT_SHADER,
        ARRAY_SIZE(shaders::lfb_adaptive_blend_frag_sources),
        shaders::lfb_adaptive_blend_frag_sources,
        "Shader frag: LFB adaptive blend");
    lfb.shader_adaptive_blend->BuildProgram();

    lfb.shader_sort_and_blend_ll =
        new gl::Shader("Program: LFB sort & blend LL");
    lfb.shader_sort_and_blend_ll->BuildShaderFromSource(
        GL_VERTEX_SHADER, 1, &shaders::fullscreen_quad_vert,
        "Shader vert: LFB sort & blend LL");
    lfb.shader_sort_and_blend_ll->BuildShaderFromSource(
        GL_FRAGMENT_SHADER,
        ARRAY_SIZE(shaders::lfb_sort_and_blend_linked_list_frag_sources),
        shaders::lfb_sort_and_blend_linked_list_frag_sources,
        "Shader frag: LFB sort & blend LL");
    lfb.shader_sort_and_blend_ll->BuildProgram();

    lfb.shader_adaptive_blend_ll =
        new gl::Shader("Program: LFB adaptive blend LL");
    lfb.shader_adaptive_blend_ll->BuildShaderFromSource(
        GL_VERTEX_SHADER, 1, &shaders::fullscreen_quad_vert,
        "Shader vert: LFB adaptive blend LL");
    lfb.shader_adaptive_blend_ll->BuildShaderFromSource(
        GL_FRAGMENT_SHADER,
        ARRAY_SIZE(shaders::lfb_adaptive_blend_linked_list_frag_sources),
        shaders::lfb_adaptive_blend_linked_list_frag_sources,
        "Shader frag: LFB adaptive blend LL");
    lfb.shader_adaptive_blend_ll->BuildProgram();

    // Counting SSBO (a range of it will be bound upon use)
    int num_elements_in_level = lfb.ubo_host.num_elements;
    int total_size = 0;
    while (num_elements_in_level > 1) {
      // We need some extra alignment if we don't want to get "does not meet
      // minimum alignment requirements for shader storage buffers" errors.
      // 256 == 4 * 64 is the maximally allowed
      // SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT.
      total_size += (num_elements_in_level + 63) & ~63;
      const int num_groups =
          (num_elements_in_level + (lfb.elements_per_thread_group - 1)) /
          lfb.elements_per_thread_group;
      num_elements_in_level = num_groups;
    }
    total_size += 1;  // last level
    lfb.ssbo_count.Create("LFB count SSBO");
    lfb.ssbo_count.Alloc(total_size * sizeof(uint32_t), nullptr, 0);

    // TODO(stefalie): We should allocate memory either for the LL version or
    // the prefix sum version, not both.
    lfb.ssbo_linked_list_header.Create("LFB LL head ptrs SSBO");
    lfb.ssbo_linked_list_header.Alloc(
        lfb.ubo_host.num_elements * sizeof(uint32_t), nullptr, 0);
    lfb.ssbo_linked_list_header.BindBase(
        GL_SHADER_STORAGE_BUFFER, SSBO_BINDING_INDEX_LFB_LINKED_LIST_HEADER);
    lfb.ssbo_linked_list_next_ptrs.Create("LFB LL next ptrs SSBO");
    lfb.ssbo_linked_list_next_ptrs.Alloc(
        (lfb.ubo_host.max_num_fragments + 1) * sizeof(uint32_t), nullptr, 0);
    lfb.ssbo_linked_list_next_ptrs.BindBase(
        GL_SHADER_STORAGE_BUFFER, SSBO_BINDING_INDEX_LFB_LINKED_LIST_NEXT_PTRS);

    // Fragments SSBO
    lfb.ssbo_fragments.Create("LFB fragments SSBO");
    const size_t num_floats_for_packed_rgbad = 3;
    // +1 is for the linked list version that starts at index 1. This wastes the
    // first array element but allows 0-cleraing the head/next_ptrs.
    lfb.ssbo_fragments.Alloc((lfb.ubo_host.max_num_fragments + 1) *
                                 num_floats_for_packed_rgbad * sizeof(float),
                             nullptr, 0);
    lfb.ssbo_fragments.BindBase(GL_SHADER_STORAGE_BUFFER,
                                SSBO_BINDING_INDEX_LFB_FRAGS);

    // FBO
    lfb.fb.fbo.Create("LFB count FBO");
    // Note that the depth buffer is shared with the PBR FBO.
    lfb.fb.fbo.Alloc({},
                     {{&pbr_.render_output.tex_depth, GL_DEPTH_ATTACHMENT}});

    // UBO
    lfb.ubo.Create("UBO: Prefix sum params");
    lfb.ubo.Alloc(sizeof(Transparency::LayeredFragmentBuffer::UBO),
                  &lfb.ubo_host, GL_MAP_WRITE_BIT);
    lfb.ubo.BindBase(GL_UNIFORM_BUFFER, UBO_BINDING_INDEX_PREFIX_SUM_PARAMS);
  }

  {  // Cascade shadow maps
    // Shader
    shadows_.shader = new gl::Shader("Program: shadow maps");
    shadows_.shader->BuildShaderFromSource(
        GL_VERTEX_SHADER, ARRAY_SIZE(shaders::shadow_map_cascade_vert_sources),
        shaders::shadow_map_cascade_vert_sources, "Shader vert: shadow maps");
    shadows_.shader->BuildShaderFromSource(
        GL_GEOMETRY_SHADER,
        ARRAY_SIZE(shaders::shadow_map_cascade_geom_sources),
        shaders::shadow_map_cascade_geom_sources, "Shader geom: shadow maps");
    shadows_.shader->BuildShaderFromSource(GL_FRAGMENT_SHADER, 1,
                                           &shaders::shadow_map_frag,
                                           "Shader frag: shadow maps");
    shadows_.shader->BuildProgram();

    // FBO
    const int sizes[] = {1024, 2048, 4096};
    const int size = sizes[static_cast<int>(shadows_.params.map_resolution)];
    shadows_.fb.tex_light_depth.Create(GL_TEXTURE_2D_ARRAY,
                                       "Shadow tex: light depth");
    shadows_.fb.tex_light_depth.Alloc3D(GL_DEPTH_COMPONENT32, size, size,
                                        Shadows::Params::kNumSplits);
    shadows_.fb.fbo.Create("Shadow FBO");
    // Not having a color attachment will implicitly set the draw buffers to
    // GL_NONE.
    shadows_.fb.fbo.Alloc(
        {}, {{&shadows_.fb.tex_light_depth, GL_DEPTH_ATTACHMENT}});

    // Sampler
    shadows_.sampler.Create("Sampler: Shadows");
    shadows_.sampler.SetParameterInt(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    shadows_.sampler.SetParameterInt(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    // Set depth at border to max so that uv samples outside [0, 1] are never
    // considered as 'in shadow'.
    const GLfloat max_depth_border[4] = {1.0f};
    shadows_.sampler.SetParameterFloats(GL_TEXTURE_BORDER_COLOR,
                                        max_depth_border);
    shadows_.sampler.SetParameterInt(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    shadows_.sampler.SetParameterInt(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    shadows_.sampler.SetParameterInt(GL_TEXTURE_COMPARE_MODE,
                                     GL_COMPARE_REF_TO_TEXTURE);
    shadows_.sampler.SetParameterInt(GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);

    // UBO
    shadows_.ubo.Create("UBO: Shadows");
    shadows_.ubo.Alloc(sizeof(shadows_.ubo_host), nullptr, GL_MAP_WRITE_BIT);
    shadows_.ubo.BindBase(GL_UNIFORM_BUFFER,
                          UBO_BINDING_INDEX_SHADOW_MAP_PARAMS);

    UpdateCascadeSplits();
  }

  {  // SAO (Scalable Ambient Obscurance
    // Shader depth pyramid
    sao_.shader_depth_pyramid = new gl::Shader("Program: SAO depth pyramid");
    sao_.shader_depth_pyramid->BuildShaderFromSource(
        GL_VERTEX_SHADER, 1, &shaders::fullscreen_quad_vert,
        "Shader vert: SAO depth pyramid");
    sao_.shader_depth_pyramid->BuildShaderFromSource(
        GL_FRAGMENT_SHADER, ARRAY_SIZE(shaders::sao_depth_pyramid_frag_sources),
        shaders::sao_depth_pyramid_frag_sources,
        "Shader frag: SAO depth pyramid");
    sao_.shader_depth_pyramid->BuildProgram();

    // Shader SAO
    sao_.shader_sao = new gl::Shader("Program: SAO");
    sao_.shader_sao->BuildShaderFromSource(GL_VERTEX_SHADER, 1,
                                           &shaders::fullscreen_quad_far_vert,
                                           "Shader vert: SAO");
    sao_.shader_sao->BuildShaderFromSource(
        GL_FRAGMENT_SHADER, ARRAY_SIZE(shaders::sao_frag_sources),
        shaders::sao_frag_sources, "Shader frag: SAO");
    sao_.shader_sao->BuildProgram();

    // Shader SAO blur X
    sao_.shader_blur_x = new gl::Shader("Program: SAO blur X");
    sao_.shader_blur_x->BuildShaderFromSource(
        GL_VERTEX_SHADER, 1, &shaders::fullscreen_quad_far_vert,
        "Shader vert: SAO blur X");
    sao_.shader_blur_x->BuildShaderFromSource(
        GL_FRAGMENT_SHADER, ARRAY_SIZE(shaders::sao_blur_x_frag_sources),
        shaders::sao_blur_x_frag_sources, "Shader frag: SAO blur X");
    sao_.shader_blur_x->BuildProgram();

    // Shader SAO blur Y
    sao_.shader_blur_y = new gl::Shader("Program: SAO blur Y");
    sao_.shader_blur_y->BuildShaderFromSource(
        GL_VERTEX_SHADER, 1, &shaders::fullscreen_quad_far_vert,
        "Shader vert: SAO blur Y");
    sao_.shader_blur_y->BuildShaderFromSource(
        GL_FRAGMENT_SHADER, ARRAY_SIZE(shaders::sao_blur_y_frag_sources),
        shaders::sao_blur_y_frag_sources, "Shader frag: SAO blur Y");
    sao_.shader_blur_y->BuildProgram();

    // FBO SAO
    sao_.fb_sao.tex_sao.Create(GL_TEXTURE_2D, "SAO tex");
    sao_.fb_sao.tex_sao.Alloc(false, GL_RGB8, framebuffer_width_,
                              framebuffer_height_);
    sao_.fb_sao.fbo.Create("SAO FBO");
    sao_.fb_sao.fbo.Alloc({},
                          {{&pbr_.render_output.tex_depth, GL_DEPTH_ATTACHMENT},
                           {&sao_.fb_sao.tex_sao, GL_COLOR_ATTACHMENT0}});

    // FBO blur X
    sao_.fb_blurred_x.tex_blurred_x.Create(GL_TEXTURE_2D, "SAO blurred X tex");
    sao_.fb_blurred_x.tex_blurred_x.Alloc(false, GL_RGB8, framebuffer_width_,
                                          framebuffer_height_);
    sao_.fb_blurred_x.fbo.Create("SAO blurred X FBO");
    sao_.fb_blurred_x.fbo.Alloc(
        {}, {{&pbr_.render_output.tex_depth, GL_DEPTH_ATTACHMENT},
             {&sao_.fb_blurred_x.tex_blurred_x, GL_COLOR_ATTACHMENT0}});

    // There is no FBO blur Y, the last pass writes directly to the render
    // output.

    // UBO
    sao_.ubo.Create("UBO: SAO");
    sao_.ubo.Alloc(sizeof(ScalableAmbientObscurance::UBO), &sao_.ubo_host,
                   GL_MAP_WRITE_BIT);
    sao_.ubo.BindBase(GL_UNIFORM_BUFFER, UBO_BINDING_INDEX_SAO_PARAMS);
  }

  {  // Tonemap/Gamma/Luma
    // Shader
    tonemap_gamma_luma_.shader = new gl::Shader("Program: Tonemap/Gamma/Luma");
    tonemap_gamma_luma_.shader->BuildShaderFromSource(
        GL_VERTEX_SHADER, 1, &shaders::fullscreen_quad_vert,
        "Shader vert: Tonemap/Gamma/Luma");
    tonemap_gamma_luma_.shader->BuildShaderFromSource(
        GL_FRAGMENT_SHADER,
        ARRAY_SIZE(shaders::tonemap_gamma_luma_frag_sources),
        shaders::tonemap_gamma_luma_frag_sources,
        "Shader frag: Tonemap/Gamma/Luma");
    tonemap_gamma_luma_.shader->BuildProgram();

    // FBO
    tonemap_gamma_luma_.fb.tex.Create(GL_TEXTURE_2D, "Tonemap/Gamma/Luma tex");
    tonemap_gamma_luma_.fb.tex.Alloc(false, GL_RGBA8, framebuffer_width_,
                                     framebuffer_height_);

    tonemap_gamma_luma_.fb.fbo.Create("Tonemap/Gamma/Luma FBO");
    tonemap_gamma_luma_.fb.fbo.Alloc(
        {}, {{&tonemap_gamma_luma_.fb.tex, GL_COLOR_ATTACHMENT0}});

    // UBO
    tonemap_gamma_luma_.ubo.Create("UBO: Tonemap/Gamma/Luma params");
    tonemap_gamma_luma_.ubo.Alloc(sizeof(TonemapGammaLuma::UBO),
                                  &tonemap_gamma_luma_.ubo_host,
                                  GL_MAP_WRITE_BIT);
    tonemap_gamma_luma_.ubo.BindBase(
        GL_UNIFORM_BUFFER, UBO_BINDING_INDEX_TONEMAP_GAMMA_LUMA_PARAMS);
  }

  {  // FXAA
    // Shader
    fxaa_.shader = new gl::Shader("Program: FXAA");
    fxaa_.shader->BuildShaderFromSource(GL_VERTEX_SHADER, 1,
                                        &shaders::fullscreen_quad_vert,
                                        "Shader vert: FXAA");
    fxaa_.shader->BuildShaderFromSource(
        GL_FRAGMENT_SHADER, ARRAY_SIZE(shaders::fxaa_frag_sources),
        shaders::fxaa_frag_sources, "Shader frag: FXAA");
    fxaa_.shader->BuildProgram();

    // UBO
    fxaa_.ubo.Create("UBO: FXAA params");
    fxaa_.ubo_host.rcp_frame =
        Vector2f(1.0f / framebuffer_width_, 1.0f / framebuffer_height_);
    fxaa_.ubo.Alloc(sizeof(Fxaa::UBO), &fxaa_.ubo_host, GL_MAP_WRITE_BIT);
    fxaa_.ubo.BindBase(GL_UNIFORM_BUFFER, UBO_BINDING_INDEX_FXAA_PARAMS);
  }

  {  // Debug render order
    // Shader
    debug_.render_order.shader = new gl::Shader("Program: debug render order");
    debug_.render_order.shader->BuildShaderFromSource(
        GL_VERTEX_SHADER, ARRAY_SIZE(shaders::debug_render_order_vert_sources),
        shaders::debug_render_order_vert_sources,
        "Shader vert: debug render order");
    debug_.render_order.shader->BuildShaderFromSource(
        GL_FRAGMENT_SHADER, 1, &shaders::debug_render_order_frag,
        "Shader frag: debug render order");
    debug_.render_order.shader->BuildProgram();

    // UBO
    debug_.render_order.ubo.Create("UBO: debug render order");
    // Number of instances is unknown, will be set just before render call.
    debug_.render_order.ubo.Alloc(sizeof(Debug::RenderOrder::UBO), nullptr,
                                  GL_MAP_WRITE_BIT);
    debug_.render_order.ubo.BindBase(GL_UNIFORM_BUFFER,
                                     UBO_BINDING_INDEX_DEBUG_RENDER_ORDER);
  }

  {  // Debug display framebuffer
    // Shader
    debug_.display_fb.shader = new gl::Shader("Program: display texture");
    debug_.display_fb.shader->BuildShaderFromSource(
        GL_VERTEX_SHADER, 1, &shaders::fullscreen_quad_vert,
        "Shader vert: display texture");
    debug_.display_fb.shader->BuildShaderFromSource(
        GL_FRAGMENT_SHADER, ARRAY_SIZE(shaders::display_texture_frag_sources),
        shaders::display_texture_frag_sources, "Shader frag: display texture");
    debug_.display_fb.shader->BuildProgram();

    // UBO
    debug_.display_fb.ubo.Create("UBO: debug display texture");
    debug_.display_fb.ubo.Alloc(sizeof(Debug::DisplayFB::UBO),
                                &debug_.display_fb.ubo_host, GL_MAP_WRITE_BIT);
    debug_.display_fb.ubo.BindBase(GL_UNIFORM_BUFFER,
                                   UBO_BINDING_INDEX_DEBUG_DISPLAY_TEXTURE);
  }

  {  // Shadows debug
    // Shader
    debug_.shadows.shader = new gl::Shader("Program: Debug shadows");
    debug_.shadows.shader->BuildShaderFromSource(GL_VERTEX_SHADER, 1,
                                                 &shaders::fullscreen_quad_vert,
                                                 "Shader vert: Debug shadows");
    debug_.shadows.shader->BuildShaderFromSource(
        GL_FRAGMENT_SHADER, ARRAY_SIZE(shaders::debug_shadows_frag_sources),
        shaders::debug_shadows_frag_sources, "Shader frag: Debug shadows");
    debug_.shadows.shader->BuildProgram();

    // UBO
    debug_.shadows.ubo.Create("UBO: Debug shadows");
    debug_.shadows.ubo.Alloc(sizeof(Debug::Shadows::UBO),
                             &debug_.shadows.ubo_host, GL_MAP_WRITE_BIT);
    debug_.shadows.ubo.BindBase(GL_UNIFORM_BUFFER,
                                UBO_BINDING_INDEX_DEBUG_SHADOWS);
  }

  // An empty but complete dummy VAO, required when drawing fullscreen quads.
  fullscreen_quad_vao_.Create("Fullscreen quad VAO");

  return false;
}  // namespace viewer

void Viewer::Destroy() {
  // TODO(stefalie): Maybe time to start using unique_ptr or something else
  // that's smart enough to clean up after itself.
  if (renderer_) {
    delete renderer_;
  }
  if (update_.new_renderer) {
    delete update_.new_renderer;
  }

  {  // BG & Sky
    if (bg_sky_.shader_sky) {
      delete bg_sky_.shader_sky;
      bg_sky_.shader_sky = nullptr;
    }
    if (bg_sky_.shader_plane) {
      delete bg_sky_.shader_plane;
      bg_sky_.shader_plane = nullptr;
    }
  }
  {  // PBR
    if (pbr_.shader_forward) {
      delete pbr_.shader_forward;
      pbr_.shader_forward = nullptr;
    }
    if (pbr_.shader_gbuffer) {
      delete pbr_.shader_gbuffer;
      pbr_.shader_gbuffer = nullptr;
    }
    if (pbr_.shader_deferred) {
      delete pbr_.shader_deferred;
      pbr_.shader_deferred = nullptr;
    }
    pbr_.render_output.tex_color.Delete();
    pbr_.render_output.tex_depth.Delete();
    pbr_.render_output.fbo_forward.Delete();
    pbr_.render_output.fbo_deferred.Delete();
    pbr_.render_output.linear_depth_pyramid.tex.Delete();
    pbr_.render_output.linear_depth_pyramid.fbo.Delete();

    if (pbr_.params.forward_num_samples > 1) {
      pbr_.render_output.msaa.rb_color.Delete();
      pbr_.render_output.msaa.rb_depth.Delete();
      pbr_.render_output.msaa.fbo.Delete();
    }
    pbr_.gbuffer.tex_albedo.Delete();
    pbr_.gbuffer.tex_normals.Delete();
    pbr_.gbuffer.tex_material.Delete();
    pbr_.gbuffer.fbo.Delete();
    pbr_.ubo.Delete();
  }
  {
    // Transparency
    // OIT
    if (transparency_.oit.shader_accum) {
      delete transparency_.oit.shader_accum;
      transparency_.oit.shader_accum = nullptr;
    }
    if (transparency_.oit.shader_resolve) {
      delete transparency_.oit.shader_resolve;
      transparency_.oit.shader_resolve = nullptr;
    }
    transparency_.oit.fb.tex_accum.Delete();
    transparency_.oit.fb.tex_revealage.Delete();
    transparency_.oit.fb.fbo.Delete();

    // LFB
    if (transparency_.lfb.shader_count) {
      delete transparency_.lfb.shader_count;
      transparency_.lfb.shader_count = nullptr;
    }
    if (transparency_.lfb.shader_prefix_sum) {
      delete transparency_.lfb.shader_prefix_sum;
      transparency_.lfb.shader_prefix_sum = nullptr;
    }
    if (transparency_.lfb.shader_fill) {
      delete transparency_.lfb.shader_fill;
      transparency_.lfb.shader_fill = nullptr;
    }
    if (transparency_.lfb.shader_sort_and_blend) {
      delete transparency_.lfb.shader_sort_and_blend;
      transparency_.lfb.shader_sort_and_blend = nullptr;
    }
    if (transparency_.lfb.shader_adaptive_blend) {
      delete transparency_.lfb.shader_adaptive_blend;
      transparency_.lfb.shader_adaptive_blend = nullptr;
    }
    transparency_.lfb.ssbo_count.Delete();
    transparency_.lfb.ssbo_fragments.Delete();
  }
  {  // Shadows
    if (shadows_.shader) {
      delete shadows_.shader;
      shadows_.shader = nullptr;
    }
    shadows_.fb.tex_light_depth.Delete();
    shadows_.fb.fbo.Delete();
    shadows_.ubo.Delete();
  }
  {  // Scalable Ambient Obscurance
    if (sao_.shader_depth_pyramid) {
      delete sao_.shader_depth_pyramid;
      sao_.shader_depth_pyramid = nullptr;
    }
    if (sao_.shader_sao) {
      delete sao_.shader_sao;
      sao_.shader_sao = nullptr;
    }
    if (sao_.shader_blur_x) {
      delete sao_.shader_blur_x;
      sao_.shader_blur_x = nullptr;
    }
    if (sao_.shader_blur_y) {
      delete sao_.shader_blur_y;
      sao_.shader_blur_y = nullptr;
    }
    sao_.fb_sao.tex_sao.Delete();
    sao_.fb_sao.fbo.Delete();
    sao_.fb_blurred_x.tex_blurred_x.Delete();
    sao_.fb_blurred_x.fbo.Delete();
    sao_.ubo.Delete();
  }
  {  // Tonemap/gamma/luma
    if (tonemap_gamma_luma_.shader) {
      delete tonemap_gamma_luma_.shader;
      tonemap_gamma_luma_.shader = nullptr;
    }
    tonemap_gamma_luma_.fb.tex.Delete();
    tonemap_gamma_luma_.fb.fbo.Delete();
    tonemap_gamma_luma_.ubo.Delete();
  }
  {  // FXAA
    if (fxaa_.shader) {
      delete fxaa_.shader;
      fxaa_.shader = nullptr;
    }
    fxaa_.ubo.Delete();
  }

  {  // Debug render order
    if (debug_.render_order.shader) {
      delete debug_.render_order.shader;
      debug_.render_order.shader = nullptr;
    }
  }
  {  // Debug display framebuffer
    if (debug_.display_fb.shader) {
      delete debug_.display_fb.shader;
      debug_.display_fb.shader = nullptr;
    }
  }
  {  // Debug shadows
    if (debug_.shadows.shader) {
      delete debug_.shadows.shader;
      debug_.shadows.shader = nullptr;
    }
  }

  if (window_) {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window_);
    glfwTerminate();
    window_ = nullptr;
  }
}

void Viewer::MainLoop(const std::function<void()>& render_custom_gui) {
  if (!window_) {
    return;
  }

  using std::chrono::high_resolution_clock;
  high_resolution_clock::time_point prev_time = high_resolution_clock::now();

  while (!glfwWindowShouldClose(window_)) {
    // Process events and update camera and shadows
    glfwPollEvents();
    if (update_camera_matrices_) {
      UpdateCameraViewMatrix();
      update_shadow_matrices_ = true;
      update_camera_matrices_ = false;
    }
    if (update_shadow_matrices_ && shadows_.ubo_host.enabled == 1) {
      UpdateShadowMatrices();
      update_shadow_matrices_ = false;
    }

    gl::PushGroup("Main");
    Render();
    gl::PopGroup();

    {  // GUI
      ImGui_ImplOpenGL3_NewFrame();
      ImGui_ImplGlfw_NewFrame();
      ImGui::NewFrame();

      const float margin = imgui_.margin * dpi_scale_;
      const float window_width = imgui_.window_width * dpi_scale_;
      const float window_height = framebuffer_height_ - 2.0f * margin;
      ImGui::SetNextWindowPos(ImVec2(margin, margin), ImGuiCond_Once);
      ImGui::SetNextWindowSize(ImVec2(window_width, window_height),
                               ImGuiCond_FirstUseEver);
      render_custom_gui();
      if (show_render_settings_gui_) {
        RenderRenderingSettingsGUI();
      }

      // ImGui Demo (needs uncommenting imgui_demo.cpp in CMakeLists.txt).
      // static bool show_imgui_demo_window = true;
      // ImGui::ShowDemoWindow(&show_imgui_demo_window);

      ImGui::Render();

      gl::PushGroup("ImGui");
      ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
      gl::PopGroup();
    }

    {  // Async update
      std::lock_guard<std::mutex> lock(update_.mutex);
      if (update_.state == AsyncRendererUpdate::State::UPDATE_ALL_IN_ONE_GO) {
        update_.new_renderer->FinalizeAllSteps();
        // We could also make a tmp pointer to the new renderer, finish the
        // critical section, and then call UpdateRenderer. But it's easier to
        // read as is and UpdateRenderer is not a big overhead.
        UpdateRenderer(update_.new_renderer);
        update_.new_renderer = nullptr;
        update_.state = AsyncRendererUpdate::State::IDLE;
        update_.cond_var.notify_one();
      } else if (update_.state == AsyncRendererUpdate::State::WAIT_4_ALLOC) {
        update_.new_renderer->FinalizeStep1AllocateAndMap();
        update_.state = AsyncRendererUpdate::State::ALLOCATED;
        update_.cond_var.notify_one();
      } else if (update_.state == AsyncRendererUpdate::State::READY_2_SWAP) {
        update_.new_renderer->FinalizeStep3UnmapAndLoadTextures();
        // See comment in the first if clause.
        UpdateRenderer(update_.new_renderer);
        update_.new_renderer = nullptr;
        update_.state = AsyncRendererUpdate::State::IDLE;
        update_.cond_var.notify_one();
      } else if (update_.state ==
                 AsyncRendererUpdate::State::WAIT_4_UNMAP_SYNC) {
        assert(false);
        update_.state = AsyncRendererUpdate::State::IDLE;
        update_.cond_var.notify_one();
      }
    }

    glfwSwapBuffers(window_);

    // Throttle the frame rate to max. 60 fps.
    if (render_settings_.limit_fps_to_60) {
      const auto delta_t = high_resolution_clock::now() - prev_time;
      const auto target_frame_time = std::chrono::milliseconds(1000 / 60);
      if (target_frame_time > delta_t) {
        // TODO(stefalie): Don't do this. this_thread::sleep_for is not precise.
        // Only keep the VSync option. See:
        // https://stackoverflow.com/questions/58436872/stdthis-threadsleep-for-sleeps-for-too-long
        std::this_thread::sleep_for(target_frame_time - delta_t);
      }
    }
    prev_time = high_resolution_clock::now();
  }

  // Prevent deadlock in case another thread is waiting for a buffer
  // allocation for a new renderer.
  std::unique_lock<std::mutex> lock(update_.mutex);
  update_.state = AsyncRendererUpdate::State::ABORT;
  update_.cond_var.notify_one();
}

void Viewer::RenderSingleFrame() {
  if (!window_) {
    return;
  }

  Render();
  glfwSwapBuffers(window_);
}

// TODO(stefalie): Use angle-axis representation instead of quaternion?
void Viewer::SetCameraParameters(const std::array<float, 8>& params) {
  // Note that the w-component goes in first.
  Quaternionf q(params[3], params[0], params[1], params[2]);
  camera_.rotation = Matrix4f::Identity();
  camera_.rotation.block(0, 0, 3, 3) = q.matrix();
  camera_.pivot_offset = Translation3f(-params[4], -params[5], -params[6]);
  camera_.camera_distance = params[7];
  update_camera_matrices_ = true;
}

void Viewer::GetCameraParameters(std::array<float, 8>* params) const {
  const Matrix3f rot = camera_.rotation.block(0, 0, 3, 3);
  Quaternionf q(rot);
  params->at(0) = q.x();
  params->at(1) = q.y();
  params->at(2) = q.z();
  params->at(3) = q.w();
  params->at(4) = -camera_.pivot_offset.x();
  params->at(5) = -camera_.pivot_offset.y();
  params->at(6) = -camera_.pivot_offset.z();
  params->at(7) = camera_.camera_distance;
}

bool Viewer::SaveScreenshot(const std::string& file_name) const {
  if (file_name.empty()) {
    return true;
  }

  // Only works if width of the main framebuffer is a multiple of 8.
  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  const int buf_size = framebuffer_width_ * framebuffer_height_ * 3;
  GLubyte* buffer = new GLubyte[buf_size];
  glReadPixels(0, 0, framebuffer_width_, framebuffer_height_, GL_RGB,
               GL_UNSIGNED_BYTE, buffer);
  stbi_flip_vertically_on_write(true);  // Could also be done in initialization.
  int ret =
      stbi_write_png((file_name + ".png").c_str(), framebuffer_width_,
                     framebuffer_height_, 3, buffer, framebuffer_width_ * 3);
  delete[] buffer;
  return !ret;
}

void Viewer::ResetCamera() {
  camera_.rotation = Matrix4f::Identity();
  camera_.pivot_offset = Translation3f::Identity();

  if (renderer_) {
    const Vector3f min = renderer_->aabb().min;
    const Vector3f max = renderer_->aabb().max;
    const Vector3f box_size = max - min;
    const Vector3f center = (max + min) * 0.5f;
    const float hlf_diag = box_size.norm() * 0.5f;
    const float tan_half_fovy =
        tan(camera_.ubo_host.fovy * 0.5f * static_cast<float>(M_PI) / 180.0f);
    const float dist_box = hlf_diag / tan_half_fovy;
    camera_.pan_scale = camera_.pan_scale_default * hlf_diag;
    camera_.zoom_scale = camera_.zoom_scale_default * hlf_diag;
    camera_.camera_distance = dist_box + box_size.z() * 0.5f;
    camera_.pivot_offset = Translation3f(-center);
  } else {
    camera_.pan_scale = camera_.pan_scale_default;
    camera_.zoom_scale = camera_.zoom_scale_default;
    camera_.camera_distance = 0.0f;
    camera_.pivot_offset = Translation3f::Identity();
  }

  update_camera_matrices_ = true;
}

void Viewer::ResetCameraPanAndZoomScales() {
  if (renderer_) {
    const Vector3f min = renderer_->aabb().min;
    const Vector3f max = renderer_->aabb().max;
    const Vector3f box_size = max - min;
    const float hlf_diag = box_size.norm() * 0.5f;
    camera_.pan_scale = camera_.pan_scale_default * hlf_diag;
    camera_.zoom_scale = camera_.zoom_scale_default * hlf_diag;
  } else {
    camera_.pan_scale = camera_.pan_scale_default;
    camera_.zoom_scale = camera_.zoom_scale_default;
  }
}

void Viewer::UpdateRenderer(gl::Renderer* renderer) {
  if (renderer_) {
    delete renderer_;
  }
  renderer_ = renderer;
  if (auto_reset_camera_) {
    ResetCamera();
  } else {
    ResetCameraPanAndZoomScales();
  }
  update_shadow_matrices_ = true;
  render_settings_.update_renderer = true;
}

void Viewer::UpdateRendererAsync(gl::Renderer* renderer) {
  // Request OpenGL buffer allocation, data copy, and swap of the renderer.
  std::unique_lock<std::mutex> lock(update_.mutex);
  update_.cond_var.wait(lock, [&] {
    return (update_.state == AsyncRendererUpdate::State::IDLE ||
            update_.state == AsyncRendererUpdate::State::ABORT);
  });
  if (update_.state != AsyncRendererUpdate::State::ABORT) {
    assert(update_.state == AsyncRendererUpdate::State::IDLE);
    update_.state = AsyncRendererUpdate::State::UPDATE_ALL_IN_ONE_GO;
    update_.new_renderer = renderer;
  }
}

void Viewer::UpdateRendererAsyncInSteps(gl::Renderer* renderer) {
  // Request OpenGL buffer allocations.
  std::unique_lock<std::mutex> lock(update_.mutex);
  if (update_.state != AsyncRendererUpdate::State::ABORT) {
    assert(update_.state == AsyncRendererUpdate::State::IDLE);
    update_.state = AsyncRendererUpdate::State::WAIT_4_ALLOC;
    update_.new_renderer = renderer;

    // Wait for the buffers to be mapped.
    update_.cond_var.wait(lock, [&] {
      return (update_.state == AsyncRendererUpdate::State::ALLOCATED ||
              update_.state == AsyncRendererUpdate::State::ABORT);
    });

    if (update_.state != AsyncRendererUpdate::State::ABORT) {
      lock.unlock();

      update_.new_renderer->FinalizeStep2CopyData();

      // Request unmapping of buffers and the renderer to be swapped.
      lock.lock();
      update_.state = AsyncRendererUpdate::State::READY_2_SWAP;
    }
  }
}

template <class T>
static void UpdateUBO(const T& ubo_host, gl::Buffer ubo) {
  T* ubo_device = reinterpret_cast<T*>(
      ubo.Map(GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT));
  *ubo_device = ubo_host;
  ubo.Unmap();
}

void Viewer::Render() {
  gl::PushGroup("Rendering & Shading");
  // NOTE: At this point the blending must be disabled (if not, something is
  // messed up).

  gl::PushGroup("Per Frame Setup");
  if (renderer_ && render_settings_.update_renderer) {
    renderer_->SetCameraParameters(camera_.ubo_host.view, camera_.ubo_host.near,
                                   camera_.ubo_host.far);
    renderer_->BeginFrame();
  }
  gl::PopGroup();

  // Create shadow map cascades
  if (renderer_) {
    gl::PushGroup("Shadow Map Cascades");

    // Clear shadow maps
    shadows_.fb.fbo.Bind();
    const GLsizei resolution = shadows_.fb.tex_light_depth.w;
    assert(shadows_.fb.tex_light_depth.w == shadows_.fb.tex_light_depth.h);
    glViewport(0, 0, resolution, resolution);
    glClear(GL_DEPTH_BUFFER_BIT);

    if (shadows_.ubo_host.enabled == 1) {
      // Back faces are still culled.

      glEnable(GL_DEPTH_TEST);

      // Depth clamping. This allows to make the light render frustum smaller
      // so that it doesn't contain all the casters. Casters behind the near
      // plane are simply move to the near plane (-1.0 in NCD).
      glEnable(GL_DEPTH_CLAMP);

      shadows_.shader->Bind();
      // TODO(stefalie): Cull objects against convex hull of camera frustum
      // and light origin.
      renderer_->Bind();
      renderer_->RenderOpaque(render_settings_.indirect_draws);
      // TODO(stefalie): Properly handle transparency. The easy solution is to
      // completely exclude/include transparent objects as shadow casters.
      // Everything in-between, e.g., casting shadow only if alpha > 0.5 or
      // casting only partial shadow (via stochastic shadow maps for example),
      // makes it a lot harder. For now, transparent objects simply cast full
      // shadows.
      renderer_->RenderTransparent(render_settings_.indirect_draws);

      glDisable(GL_DEPTH_CLAMP);
      glDisable(GL_DEPTH_TEST);
    }

    glViewport(0, 0, framebuffer_width_, framebuffer_height_);
    gl::PopGroup();
  }

  const bool do_msaa = (pbr_.params.forward_num_samples > 1) &&
                       (pbr_.params.render_mode == PBR::RenderMode::FORWARD);
  gl::FrameBuffer forward_fbo =
      (do_msaa ? pbr_.render_output.msaa.fbo : pbr_.render_output.fbo_forward);
  forward_fbo.Bind();

  if (bg_sky_.params.enable_sky) {
    // TODO(stefalie): Use glClearNamedFramebufferfv(...) instead, then we can
    // further limit the number of OpenGL calls we need, and compact init.h.
    glClear(GL_DEPTH_BUFFER_BIT);
  } else {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  }

  // TODO(stefalie): Do a potential depth prepass here in forward mode, and then
  // set glDepthFunc(GL_EQUAL). Generally it probably is better to just
  // approximately sort objects from back to front. The renderer is already
  // trying to do this partially (within an instanced draw, the mesh ID has
  // higher sorting priority than depth).

  // TODO(stefalie): If sky/ground plane evaluation should ever become expensive
  // (maybe through a complex sky light model), you can move this below the main
  // geometry rendering pipeline step, move the fullscreen quad to the far
  // plane, and activate the early depth test.
  if (bg_sky_.params.enable_sky) {
    gl::PushGroup("Render Sky");
    bg_sky_.shader_sky->Bind();
    fullscreen_quad_vao_.Bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);  // Full screen triangle
    gl::PopGroup();
  }

  // Will be disabled again just before post-processing.
  glEnable(GL_DEPTH_TEST);

  if (bg_sky_.params.enable_infinite_ground_plane) {
    gl::PushGroup("Render Infinite Ground Plane");

    // This requires GL_DEPTH_TEST to be on.
    // Without this, precision at infinity is completely messed up. GL_ALWAYS
    // should work too (assuming nothing else has been drawn first).
    glDepthFunc(GL_LEQUAL);
    bg_sky_.shader_plane->Bind();
    fullscreen_quad_vao_.Bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);  // Full screen triangle
    glDepthFunc(GL_LESS);              // Reset depth func to default.
    gl::PopGroup();
  }

  if (renderer_) {
    shadows_.fb.tex_light_depth.BindToUnit(SAMPLER_BINDING_SHADOW_MAP);
    shadows_.sampler.BindToUnit(SAMPLER_BINDING_SHADOW_MAP);

    // TODO(stefalie): Cull objects against camera frustum.

    if (pbr_.params.render_mode == PBR::RenderMode::FORWARD) {
      gl::PushGroup("Render Forward");

      // Depth and color have been cleared already, only linear depth needs
      // clearing.
      // TODO(stefalie): I don't think we need to clear the linear depth. It's
      // only used for SAO which uses the standard depth buffer and the depth
      // test to ignore sky/background pixels.
      const float lin_depth_clear[4] = {camera_.ubo_host.far};
      glClearNamedFramebufferfv(forward_fbo.id, GL_COLOR, 1, lin_depth_clear);

      pbr_.shader_forward->Bind();

      renderer_->Bind();
      renderer_->RenderOpaque(render_settings_.indirect_draws);

      if (do_msaa) {  // Blit
        gl::PushGroup("MSAA Resolve Blit");
        const int w = framebuffer_width_;
        const int h = framebuffer_height_;
        pbr_.render_output.msaa.fbo.BlitTo(pbr_.render_output.fbo_forward, 0, 0,
                                           w, h, 0, 0, w, h);
        gl::PopGroup();
      }

      {
        gl::PushGroup("Linear Depth Reconstruct");
        pbr_.render_output.linear_depth_pyramid.fbo.Bind();
        pbr_.shader_reconstruct_linear_depth->Bind();

        // The sampler params don't matter, we only use texelFetch.
        pbr_.render_output.tex_depth.BindToUnit(0);

        fullscreen_quad_vao_.Bind();
        glDrawArrays(GL_TRIANGLES, 0, 3);  // Full screen triangle

        gl::PopGroup();
      }

      gl::PopGroup();
    } else /* if (pbr_.params.render_mode == PBR::RenderMode::DEFERRED) */ {
      gl::PushGroup("Render Deferred");

      gl::PushGroup("GBuffer");
      pbr_.gbuffer.fbo.Bind();
      const float depth_clear[4] = {1.0f};
      const float lin_depth_clear[4] = {camera_.ubo_host.far};
      const float zero_clear[4] = {};
      glClearNamedFramebufferfv(pbr_.gbuffer.fbo.id, GL_DEPTH, 0, depth_clear);
      glClearNamedFramebufferfv(pbr_.gbuffer.fbo.id, GL_COLOR, 0, zero_clear);
      glClearNamedFramebufferfv(pbr_.gbuffer.fbo.id, GL_COLOR, 1, zero_clear);
      glClearNamedFramebufferfv(pbr_.gbuffer.fbo.id, GL_COLOR, 2,
                                lin_depth_clear);
      glClearNamedFramebufferfv(pbr_.gbuffer.fbo.id, GL_COLOR, 3, zero_clear);
      pbr_.shader_gbuffer->Bind();

      renderer_->Bind();
      renderer_->RenderOpaque(render_settings_.indirect_draws);

      gl::PopGroup();

      gl::PushGroup("Resolve");
      // This FBO has no depth attachment, depth test will always pass. The
      // output depth buffer has already been filled in the GBuffer pass.
      pbr_.render_output.fbo_deferred.Bind();
      pbr_.shader_deferred->Bind();
      const GLuint resolve_textures[] = {
          pbr_.render_output.tex_depth.id,
          pbr_.gbuffer.tex_albedo.id,
          pbr_.gbuffer.tex_normals.id,
          pbr_.gbuffer.tex_material.id,
      };
      glBindTextures(0, ARRAY_SIZE(resolve_textures), resolve_textures);
      fullscreen_quad_vao_.Bind();
      glDrawArrays(GL_TRIANGLES, 0, 3);  // Full screen triangle
      gl::PopGroup();

      gl::PopGroup();
    }

    {  // Transparent objects
      gl::PushGroup("Transparency Forward");

      if (transparency_.params.method == Transparency::Method::OIT) {
        // Implements:
        // McGuire, Bavoil - 2013 - Weighted Blended Order-Independent
        // Transparency

        gl::PushGroup("OIT");
        glEnable(GL_BLEND);

        gl::PushGroup("OIT Accumulation");

        transparency_.oit.fb.fbo.Bind();
        const float zero_clear[4] = {};
        const float one_clear[4] = {1.0f, 1.0f, 1.0f, 1.0f};
        glClearNamedFramebufferfv(transparency_.oit.fb.fbo.id, GL_COLOR, 0,
                                  zero_clear);
        glClearNamedFramebufferfv(transparency_.oit.fb.fbo.id, GL_COLOR, 1,
                                  one_clear);

        glBlendFunci(0, GL_ONE, GL_ONE);
        glBlendFunci(1, GL_ZERO, GL_ONE_MINUS_SRC_ALPHA);
        glDepthMask(GL_FALSE);
        transparency_.oit.shader_accum->Bind();

        renderer_->Bind();
        renderer_->RenderTransparent(render_settings_.indirect_draws);

        glDepthMask(GL_TRUE);
        gl::PopGroup();

        gl::PushGroup("OIT Resolve");

        pbr_.render_output.fbo_forward.Bind();
        transparency_.oit.shader_resolve->Bind();
        glBlendFunc(GL_ONE_MINUS_SRC_ALPHA, GL_SRC_ALPHA);
        glDisable(GL_DEPTH_TEST);
        const GLuint resolve_textures[] = {
            transparency_.oit.fb.tex_accum.id,
            transparency_.oit.fb.tex_revealage.id,
        };
        glBindTextures(0, ARRAY_SIZE(resolve_textures), resolve_textures);

        fullscreen_quad_vao_.Bind();
        glDrawArrays(GL_TRIANGLES, 0, 3);  // Full screen triangle

        glEnable(GL_DEPTH_TEST);
        gl::PopGroup();

        glDisable(GL_BLEND);
        gl::PopGroup();
      } else {  // Transparency::Method::LFB or
                // Transparency::Method::LFB_ADAPTIVE
        // Implements:
        // OpenGL Insights - Chapter 20 - Efficient Layered Fragment Buffer
        // Techniques
        // We use the linearized version of the LFB. It's claimed that the
        // linked list version is faster, but I feel with a prefix sum that
        // makes use of shared memory the linearized version should be ok too.
        Transparency::LayeredFragmentBuffer& lfb = transparency_.lfb;

        gl::PushGroup("LFB");
        glDepthMask(GL_FALSE);

        const bool use_ll =
            transparency_.params.method == Transparency::Method::LFB_LL ||
            transparency_.params.method ==
                Transparency::Method::LFB_LL_ADAPTIVE;
        if (!use_ll) {
          gl::PushGroup("LFB Count");
          // Ideally we would only clear the range for the first level of the
          // buffer. Shouldn't make much of a performance difference though as
          // each subsequent level is 2048 times smaller.
          // TODO(stefalie): We use a float to clear here, everything else seems
          // make Mesa/llvmpipe unhappy.
          const GLfloat zero_clear[1] = {};
          glClearNamedBufferData(lfb.ssbo_count.id, GL_R32F, GL_RED, GL_FLOAT,
                                 zero_clear);

          // Bind FBO with just the main depth buffer but no color attachments
          // for fragments per pixel counting.
          lfb.fb.fbo.Bind();

          lfb.ssbo_count.BindRange(
              GL_SHADER_STORAGE_BUFFER, SSBO_BINDING_INDEX_LFB_COUNT, 0,
              lfb.ubo_host.num_elements * sizeof(uint32_t));

          lfb.shader_count->Bind();
          renderer_->Bind();
          renderer_->RenderTransparent(render_settings_.indirect_draws);
          glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
          gl::PopGroup();

          gl::PushGroup("LFB Prefix Sum");
          LFBPrefixSum();
          gl::PopGroup();

          gl::PushGroup("LFB Fill");
          lfb.shader_fill->Bind();
          renderer_->RenderTransparent(render_settings_.indirect_draws);
          glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
          gl::PopGroup();
        } else {
          gl::PushGroup("LFB LL Fill");

          // Clear global counter, head & next ptrs.
          // TODO(stefalie): We use a float to clear here, everything else seems
          // make Mesa/llvmpipe unhappy.
          const GLfloat zero_clear[1] = {};
          glClearNamedBufferData(lfb.ssbo_linked_list_header.id, GL_R32F,
                                 GL_RED, GL_FLOAT, zero_clear);
          glClearNamedBufferData(lfb.ssbo_linked_list_next_ptrs.id, GL_R32F,
                                 GL_RED, GL_FLOAT, zero_clear);

          // Bind FBO with just the main depth buffer but no color attachments
          // for fragments per pixel counting.
          lfb.fb.fbo.Bind();

          lfb.shader_fill_ll->Bind();
          renderer_->Bind();
          renderer_->RenderTransparent(render_settings_.indirect_draws);
          glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
          gl::PopGroup();
        }

        gl::PushGroup("LFB Blend");
        pbr_.render_output.fbo_forward.Bind();
        switch (transparency_.params.method) {
          case Transparency::Method::LFB:
            lfb.shader_sort_and_blend->Bind();
            break;
          case Transparency::Method::LFB_ADAPTIVE:
            lfb.shader_adaptive_blend->Bind();
            break;
          case Transparency::Method::LFB_LL:
            lfb.shader_sort_and_blend_ll->Bind();
            break;
          case Transparency::Method::LFB_LL_ADAPTIVE:
            lfb.shader_adaptive_blend_ll->Bind();
            break;
          default:
            assert(false);
            break;
        }
        glDisable(GL_DEPTH_TEST);
        glEnable(GL_BLEND);
        glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);  // Pre-multiplied alpha
        fullscreen_quad_vao_.Bind();
        glDrawArrays(GL_TRIANGLES, 0, 3);  // Full screen triangle
        glDisable(GL_BLEND);
        glEnable(GL_DEPTH_TEST);
        gl::PopGroup();

        glDepthMask(GL_TRUE);
        gl::PopGroup();
      }

      gl::PopGroup();
    }
  }

  // At this point the depth test is still enabled.

  if (sao_.params.enabled) {
    gl::PushGroup("SAO");

    gl::PushGroup("Depth pyramid");

    pbr_.render_output.linear_depth_pyramid.fbo.Bind();
    sao_.shader_depth_pyramid->Bind();
    pbr_.render_output.linear_depth_pyramid.tex.BindToUnit(0);
    fullscreen_quad_vao_.Bind();

    int mip_width = framebuffer_width_;
    int mip_height = framebuffer_height_;
    for (int i = 1; i < sao_.max_mip_level; ++i) {
      // Update FBO attachment.
      pbr_.render_output.linear_depth_pyramid.fbo.AttachTexture(
          {&pbr_.render_output.linear_depth_pyramid.tex, GL_COLOR_ATTACHMENT0,
           i});

      // Prepare viewport
      mip_width >>= 1;
      mip_height >>= 1;
      glViewport(0, 0, mip_width, mip_height);

      // Update level in UBO.
      // TODO(stefalie): Update only the range that changed in the UBO (probably
      // doesn't matter though for these few bytes).
      sao_.ubo_host.src_mip_level = i - 1;
      UpdateUBO(sao_.ubo_host, sao_.ubo);

      // Downsample with rotated grid.
      glDrawArrays(GL_TRIANGLES, 0, 3);  // Full screen triangle
    }

    // Reset the FBO to level 0 for the next frame.
    pbr_.render_output.linear_depth_pyramid.fbo.AttachTexture(
        {&pbr_.render_output.linear_depth_pyramid.tex, GL_COLOR_ATTACHMENT0,
         0});

    glViewport(0, 0, framebuffer_width_, framebuffer_height_);

    gl::PopGroup();

    glDepthFunc(GL_GREATER);
    glDepthMask(GL_FALSE);

    gl::PushGroup("SAO main");
    // No need to clear the SAO FBO.
    sao_.fb_sao.fbo.Bind();
    // The fullscreen quad at far VAO and the linear depth pyramid tex are
    // already bound.
    sao_.shader_sao->Bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);  // Full screen triangle
    gl::PopGroup();

    gl::PushGroup("Blur");

    // X blur
    sao_.fb_blurred_x.fbo.Bind();
    sao_.fb_sao.tex_sao.BindToUnit(0);
    sao_.shader_blur_x->Bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);  // Full screen triangle

    // Y blur
    pbr_.render_output.fbo_forward.Bind();
    sao_.fb_blurred_x.tex_blurred_x.BindToUnit(0);
    sao_.shader_blur_y->Bind();

    if (!sao_.params.debug_sao_only) {
      glEnable(GL_BLEND);
      glBlendFunc(GL_ZERO, GL_SRC_COLOR);  // Multiply SAO with background.
    }
    glDrawArrays(GL_TRIANGLES, 0, 3);  // Full screen triangle
    if (!sao_.params.debug_sao_only) {
      glDisable(GL_BLEND);
    }

    gl::PopGroup();

    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);  // Reset depth func to default.

    gl::PopGroup();
  }

  gl::PopGroup();  // Rendering and Shading

  {  // Post-processing
    gl::PushGroup("Post-processing");

    // Disable depth test for post processing
    glDisable(GL_DEPTH_TEST);

    // Most post-processing are simple image to image operations, a nearest
    // sampler is sufficient.
    sampler_nearest_.BindToUnit(0);

    gl::PushGroup("Tonemap/Gamma/Luma");
    tonemap_gamma_luma_.fb.fbo.Bind();
    tonemap_gamma_luma_.shader->Bind();
    pbr_.render_output.tex_color.BindToUnit(0);
    fullscreen_quad_vao_.Bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);  // Full screen triangle
    tonemap_gamma_luma_.fb.fbo.Unbind();
    gl::PopGroup();

    // For FXAA we use a non-mip-mapped bilinear sampler.
    // This is also kept for the debug rendering, see below.
    sampler_bilinear_.BindToUnit(0);

    gl::PushGroup("FXAA");
    fxaa_.shader->Bind();
    tonemap_gamma_luma_.fb.tex.BindToUnit(0);
    fullscreen_quad_vao_.Bind();
    glDrawArrays(GL_TRIANGLES, 0, 3);  // Full screen triangle
    gl::PopGroup();

    gl::PopGroup();
  }

  // Debug rendering
  if (debug_.params.render_mode != Debug::RenderMode::NONE) {
    RenderDebug();
  }

  if (renderer_ && render_settings_.update_renderer) {
    renderer_->EndFrame();
  }

  // Reactivate the mip-mapped, anisotropic sampler for texture unit 0.
  gl::TextureManager::Get().default_sampler().BindToUnit(0);
}

// Based on:
// https://github.com/phebuswink/CUDA/blob/master/MP4/MP4.2/scan_largearray_kernel.cu#L199
void Viewer::LFBPrefixSum() {
  // UBO doesn't need updating because number of pixel doesn't change.
  transparency_.lfb.shader_prefix_sum->Bind();

  assert(transparency_.lfb.ubo_host.num_elements > 1);
  LFBPrefixSumRec(transparency_.lfb.ubo_host.num_elements, 0);
}

void Viewer::LFBPrefixSumRec(int num_elements, int offset) {
  Transparency::LayeredFragmentBuffer& lfb = transparency_.lfb;

  lfb.ubo_host.num_elements = num_elements;
  UpdateUBO(lfb.ubo_host, lfb.ubo);

  const int num_groups = (num_elements + (lfb.elements_per_thread_group - 1)) /
                         lfb.elements_per_thread_group;

  assert(sizeof(uint32_t) == 4);
  const int num_elements_aligned = (num_elements + 63) & ~63;

  lfb.ssbo_count.BindRange(GL_SHADER_STORAGE_BUFFER,
                           SSBO_BINDING_INDEX_LFB_COUNT, offset * 4,
                           num_elements * 4);
  offset += num_elements_aligned;
  lfb.ssbo_count.BindRange(GL_SHADER_STORAGE_BUFFER,
                           SSBO_BINDING_INDEX_LFB_SUMS, offset * 4,
                           num_groups * 4);

  glDispatchCompute(num_groups, 1, 1);
  glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

  // If there is more than one group, we recurse.
  if (num_groups > 1) {
    LFBPrefixSumRec(num_groups, offset);

    lfb.ubo_host.num_elements = num_elements;
    UpdateUBO(lfb.ubo_host, lfb.ubo);

    lfb.ssbo_count.BindRange(GL_SHADER_STORAGE_BUFFER,
                             SSBO_BINDING_INDEX_LFB_SUMS, offset * 4,
                             num_groups * 4);
    offset -= num_elements_aligned;
    lfb.ssbo_count.BindRange(GL_SHADER_STORAGE_BUFFER,
                             SSBO_BINDING_INDEX_LFB_COUNT, offset * 4,
                             num_elements * 4);

    glDispatchCompute(num_groups, 1, 1);
    glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
  } else {  // Otherwise we switch the shader and let the stack unwind.
    lfb.shader_prefix_sum_adjust_incr->Bind();
  }
}

void Viewer::RenderDebug() {
  switch (debug_.params.render_mode) {
    case Debug::RenderMode::NONE:
      break;
    case Debug::RenderMode::RENDER_ORDER: {
      gl::PushGroup("Render Debug Order");
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      debug_.render_order.ubo_host.num_instances =
          renderer_->stats().num_instances;
      UpdateUBO(debug_.render_order.ubo_host, debug_.render_order.ubo);

      debug_.render_order.shader->Bind();
      renderer_->Bind();
      renderer_->RenderOpaque(render_settings_.indirect_draws);
      renderer_->RenderTransparent(render_settings_.indirect_draws);
      gl::PopGroup();
      break;
    }
    case Debug::RenderMode::DISPLAY_TEXTURE: {
      gl::PushGroup("Render Debug Framebuffer");
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      switch (debug_.display_fb.params.tex) {
        case Debug::DisplayFB::Tex::GBUFFER_ALBEDO:
          pbr_.gbuffer.tex_albedo.BindToUnit(0);
          break;
        case Debug::DisplayFB::Tex::GBUFFER_NORMALS:
          pbr_.gbuffer.tex_normals.BindToUnit(0);
          break;
        case Debug::DisplayFB::Tex::GBUFFER_MATERIAL:
          pbr_.gbuffer.tex_material.BindToUnit(0);
          break;
        case Debug::DisplayFB::Tex::OUTPUT_COLOR:
          pbr_.render_output.tex_color.BindToUnit(0);
          break;
        case Debug::DisplayFB::Tex::OUTPUT_DEPTH:
          pbr_.render_output.tex_depth.BindToUnit(0);
          break;
        case Debug::DisplayFB::Tex::OUTPUT_LINEAR_DEPTH_PYRAMID:
          pbr_.render_output.linear_depth_pyramid.tex.BindToUnit(0);
          break;
        case Debug::DisplayFB::Tex::TONEMAP_GAMMA_LUMA:
          tonemap_gamma_luma_.fb.tex.BindToUnit(0);
          break;
        case Debug::DisplayFB::Tex::OIT_ACCUMULATION:
          transparency_.oit.fb.tex_accum.BindToUnit(0);
          break;
        case Debug::DisplayFB::Tex::OIT_REVEALAGE:
          transparency_.oit.fb.tex_revealage.BindToUnit(0);
          break;
        case Debug::DisplayFB::Tex::SAO:
          sao_.fb_sao.tex_sao.BindToUnit(0);
          break;
      }

      // Don't worry, this will be reset before the frame ends.
      sampler_bilinear_mips_.BindToUnit(0);

      debug_.display_fb.shader->Bind();
      fullscreen_quad_vao_.Bind();
      glDrawArrays(GL_TRIANGLES, 0, 3);  // Full screen triangle
      gl::PopGroup();
      break;
    }
    case Debug::RenderMode::SHADOW_MAPS: {
      gl::PushGroup("Render Debug Shadows");
      glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

      shadows_.fb.tex_light_depth.BindToUnit(0);
      debug_.shadows.shader->Bind();
      fullscreen_quad_vao_.Bind();
      glDrawArrays(GL_TRIANGLES, 0, 3);  // Full screen triangle

      gl::PopGroup();
      break;
    }
  }
}

void Viewer::RenderRenderingSettingsGUI() {
  const float margin = imgui_.margin * dpi_scale_;
  const float window_width = imgui_.window_width * dpi_scale_;
  const float horizontal_pos = framebuffer_width_ - window_width - margin;
  const float window_height = framebuffer_height_ - 2.0f * margin;
  ImGui::SetNextWindowPos(ImVec2(horizontal_pos, margin),
                          ImGuiCond_FirstUseEver);
  ImGui::SetNextWindowSize(ImVec2(window_width, window_height),
                           ImGuiCond_FirstUseEver);
  ImGui::Begin("Rendering Settings");

  // Generic
  if (ImGui::CollapsingHeader("Generic")) {
    if (ImGui::Checkbox("VSync", &render_settings_.vsync)) {
      glfwSwapInterval(static_cast<int>(render_settings_.vsync));
    }
    ImGui::Checkbox("Limit FPS to 60", &render_settings_.limit_fps_to_60);
    ImGui::Checkbox("Update renderer every frame",
                    &render_settings_.update_renderer);
    ImGui::Checkbox("Indirect draws", &render_settings_.indirect_draws);
  }

  // Camera
  if (ImGui::CollapsingHeader("Camera")) {
    bool dirty = false;

    dirty |= ImGui::SliderFloat("Fovy", &camera_.ubo_host.fovy, 22.5f, 90.0f);
    dirty |= ImGui::InputFloat("Near plane", &camera_.ubo_host.near);
    dirty |= ImGui::InputFloat("Far plane", &camera_.ubo_host.far);

    if (dirty) {
      const float min_near = 0.000001f;
      if (camera_.ubo_host.near < min_near) {
        camera_.ubo_host.near = min_near;
      }
      const float min_far = camera_.ubo_host.near + 0.1f;
      if (camera_.ubo_host.far < min_far) {
        camera_.ubo_host.far = min_far;
      }

      UpdateCameraProjection();
      UpdateCascadeSplits();
      update_shadow_matrices_ = true;
    }

    // Do the trackball separately.
    if (ImGui::TreeNode("Trackball")) {
      dirty = false;

      std::array<float, 8> camera_params;
      GetCameraParameters(&camera_params);

      dirty |= ImGui::SliderFloat4("Rotation quaternion", &camera_params[0],
                                   -1.0f, 1.0f);
      dirty |= ImGui::DragFloat3("Pivot", &camera_params[4]);
      dirty |= ImGui::DragFloat("Distance", &camera_params[7]);

      Quaternionf q(&camera_params[0]);
      q.normalize();
      camera_params[0] = q.x();
      camera_params[1] = q.y();
      camera_params[2] = q.z();
      camera_params[3] = q.w();
      camera_params[7] = std::max(camera_params[7], 0.0f);

      if (dirty) {
        SetCameraParameters(camera_params);
      }

      ImGui::TreePop();
    }

    if (ImGui::Button("Print camera t stdout")) {
      std::array<float, 8> params;
      GetCameraParameters(&params);
      for (int i = 0; i < 8; ++i) {
        if (i > 0) {
          std::cout << ' ';
        }
        std::cout << params[i];
      }
      std::cout << '\n';
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset camera")) {
      ResetCamera();
    }
  }

  // BG & Sky
  if (ImGui::CollapsingHeader("BG & Sky")) {
    bool dirty = false;

    dirty |= ImGui::ColorEdit3("Clear color", bg_sky_.params.clear_color,
                               ImGuiColorEditFlags_DisplayHex);
    dirty |= ImGui::SliderFloat("Clear multiplier",
                                &bg_sky_.params.clear_multiplier, 1.0f, 5.0f);
    ImGui::Checkbox("Procedural sky", &bg_sky_.params.enable_sky);
    ImGui::Checkbox("Infinite ground plane",
                    &bg_sky_.params.enable_infinite_ground_plane);
    dirty |= ImGui::DragFloat("Ground plane elevation",
                              &bg_sky_.ubo_host.ground_plane_height);
    dirty |= ImGui::ColorEdit3("Ground plane color",
                               bg_sky_.ubo_host.ground_plane_color.data(),
                               ImGuiColorEditFlags_DisplayHex);
    if (dirty) {
      glClearColor(
          bg_sky_.params.clear_color[0] * bg_sky_.params.clear_multiplier,
          bg_sky_.params.clear_color[1] * bg_sky_.params.clear_multiplier,
          bg_sky_.params.clear_color[2] * bg_sky_.params.clear_multiplier,
          1.0f);
      UpdateUBO(bg_sky_.ubo_host, bg_sky_.ubo);
    }
  }

  // PBR
  if (ImGui::CollapsingHeader("PBR")) {
    bool dirty = false;

    const char* render_modes[] = {
        "Deferred",
        "Forward",
    };
    ImGui::Combo("Render mode",
                 reinterpret_cast<int*>(&pbr_.params.render_mode), render_modes,
                 IM_ARRAYSIZE(render_modes));

    // TODO(stefalie): How to handle reallocations of buffers? This is
    // currently all mixed up in UI code. We can potentially throw this
    // out as 16 bits is insufficient anyway.
    const char* lin_depth_modes[] = {"16 bits", "32 bits"};
    if (ImGui::Combo("Linear depth buffer bits",
                     reinterpret_cast<int*>(&pbr_.params.linear_depth_bits),
                     lin_depth_modes, IM_ARRAYSIZE(lin_depth_modes))) {
      // When the number of depth bits change, we delete the old texture
      // and reallocate a new one right away. The dirty flag is not set,
      // we want to handle this explicitly and not just reallocate the
      // texture every time a PBR parameter changes.
      pbr_.render_output.linear_depth_pyramid.tex.Delete();
      pbr_.render_output.linear_depth_pyramid.tex.Create(
          GL_TEXTURE_2D, "Linear depth pyramid tex");

      GLenum lin_depth_format = GL_R16F;
      if (pbr_.params.linear_depth_bits == PBR::LinarDepthBits::LIN_DEPTH_32) {
        lin_depth_format = GL_R32F;
      }
      pbr_.render_output.linear_depth_pyramid.tex.Alloc(
          true, lin_depth_format, framebuffer_width_, framebuffer_height_);

      // Overwrite the FBO attachment (recreation of FBO is not be
      // necessary).
      pbr_.render_output.linear_depth_pyramid.fbo.AttachTexture(
          {&pbr_.render_output.linear_depth_pyramid.tex,
           GL_COLOR_ATTACHMENT0 + 0});
      pbr_.gbuffer.fbo.AttachTexture(
          {&pbr_.render_output.linear_depth_pyramid.tex,
           GL_COLOR_ATTACHMENT0 + 2});
    }

    const int n_samples_old = pbr_.params.forward_num_samples;
    if (ImGui::SliderInt("Forward MSAA", &pbr_.params.forward_num_samples, 1,
                         max_num_samples_)) {
      // Cleanup
      if (n_samples_old > 1) {
        pbr_.render_output.msaa.rb_color.Delete();
        pbr_.render_output.msaa.rb_depth.Delete();
        pbr_.render_output.msaa.fbo.Delete();
      }

      // Realloc if necessary
      const int n_samples = pbr_.params.forward_num_samples;
      if (n_samples > 1) {
        AllocMSAAFBOs(&pbr_.render_output.msaa, framebuffer_width_,
                      framebuffer_height_, n_samples);
      }
    }

    dirty |= ImGui::Checkbox(
        "Light dirs in eye space",
        reinterpret_cast<bool*>(&pbr_.ubo_host.light_dirs_in_eye_space));

    auto light_gui = [](PBR::DirectionalLight* light, const char* label) {
      bool d = false;
      if (ImGui::TreeNode(label)) {
        d |= ImGui::SliderFloat3("Direction", light->dir.data(), -1.0f, 1.0f);
        d |= ImGui::SliderFloat("Intensity", &light->intensity, 0.0f, 20.0f);
        d |= ImGui::ColorEdit3("Color", light->color.data(),
                               ImGuiColorEditFlags_DisplayHex);
        ImGui::TreePop();
      }
      return d;
    };
    dirty |= light_gui(&pbr_.ubo_host.key_light, "Key light");
    dirty |= light_gui(&pbr_.ubo_host.fill_light, "Fill light");
    dirty |= light_gui(&pbr_.ubo_host.back_light, "Back light");

    if (ImGui::TreeNode("Ambient light")) {
      dirty |=
          ImGui::ColorEdit3("Color", pbr_.ubo_host.ambient_light_color.data(),
                            ImGuiColorEditFlags_DisplayHex);
      dirty |= ImGui::SliderFloat(
          "Intensity", &pbr_.ubo_host.ambient_light_intensity, 0.0f, 2.0f);
      ImGui::TreePop();
    }

    if (dirty) {
      UpdateUBO(pbr_.ubo_host, pbr_.ubo);

      // TODO(stefalie): Update the light matrix for the shadows only if
      // the key light direction changed. The other changes are irrelevant
      // for the shadows.
      update_shadow_matrices_ = true;
    }
  }

  // Transparency
  if (ImGui::CollapsingHeader("Transparency")) {
    const char* methods[] = {
        "Weighted blended OIT", "Layered Fragment Buffer (LFB)", "Adaptive LFB",
        "LFB with linked list", "Adaptive LFB with linked list",
    };
    ImGui::Combo("Method", reinterpret_cast<int*>(&transparency_.params.method),
                 methods, IM_ARRAYSIZE(methods));
  }

  // Shadows
  if (ImGui::CollapsingHeader("Shadows")) {
    bool dirty = false;

    dirty |= ImGui::Checkbox(
        "Enable", reinterpret_cast<bool*>(&shadows_.ubo_host.enabled));

    const char* shadow_map_resolutions[] = {
        "1024",
        "1440",
        "2048",
        "4096",
    };

    if (ImGui::Combo("Map resolution",
                     reinterpret_cast<int*>(&shadows_.params.map_resolution),
                     shadow_map_resolutions,
                     IM_ARRAYSIZE(shadow_map_resolutions))) {
      // When the shadow map resolution changes, we delete the old shadow
      // map and reallocate a new one right away. The dirty flag is not
      // set, we want to handle this explicitly and not just reallocate
      // the texture every time a shadow parameter changes.

      // Delete old texture and create a new one.
      const int sizes[] = {1024, 1440, 2048, 4096};
      const int size = sizes[static_cast<int>(shadows_.params.map_resolution)];
      shadows_.fb.tex_light_depth.Delete();
      shadows_.fb.tex_light_depth.Create(GL_TEXTURE_2D_ARRAY,
                                         "Shadow tex: light depth");
      shadows_.fb.tex_light_depth.Alloc3D(GL_DEPTH_COMPONENT32, size, size,
                                          Shadows::Params::kNumSplits);
      // Overwrite the FBO attachment (recreation of FBO is not be
      // necessary).
      shadows_.fb.fbo.AttachTexture(
          {&shadows_.fb.tex_light_depth, GL_DEPTH_ATTACHMENT});
    }

    dirty |= ImGui::InputFloat("Max. shadow depth",
                               &shadows_.params.max_shadow_depth);
    dirty |= ImGui::SliderFloat("Split lambda", &shadows_.params.split_lambda,
                                0.0f, 1.0f);
    dirty |= ImGui::SliderFloat("Shadow strength",
                                &shadows_.ubo_host.shadow_strength, 0.0f, 1.0f);
    dirty |= ImGui::Checkbox("Stabilize", &shadows_.params.stabilize);

    if (ImGui::TreeNode("Bias")) {
      dirty |= ImGui::SliderFloat("Const depth bias",
                                  &shadows_.ubo_host.const_depth_bias, 0.0f,
                                  40.0f, "%.5f");
      dirty |= ImGui::SliderFloat("Slope scale depth bias",
                                  &shadows_.ubo_host.slope_scale_depth_bias,
                                  0.0f, 20.0f, "%.5f");
      dirty |=
          ImGui::SliderFloat("Normal offset bias",
                             &shadows_.ubo_host.normal_offset_bias, 0.0f, 20.f);
      dirty |= ImGui::Checkbox(
          "Normal offset UV only",
          reinterpret_cast<bool*>(&shadows_.ubo_host.normal_offset_uv_only));
      dirty |= ImGui::Checkbox(
          "Receiver plane bias",
          reinterpret_cast<bool*>(&shadows_.ubo_host.use_receiver_plane_bias));
      dirty |= ImGui::Checkbox(
          "Fade cascades",
          reinterpret_cast<bool*>(&shadows_.ubo_host.fade_cascades));
      ImGui::TreePop();
    }

    dirty |= ImGui::Checkbox(
        "Show cascades",
        reinterpret_cast<bool*>(&shadows_.ubo_host.debug_show_cascades));

    if (dirty) {
      // This will also update the shadows UBO.
      UpdateCascadeSplits();
      update_shadow_matrices_ = true;
    }
  }

  // Scalable Ambient Obscurance
  if (ImGui::CollapsingHeader("SAO (Scalable Ambient Obscurance)")) {
    bool dirty = false;

    ImGui::Checkbox("Enable SAO", &sao_.params.enabled);
    ImGui::Checkbox("Show only SAO", &sao_.params.debug_sao_only);

    dirty |= ImGui::SliderFloat("Radius", &sao_.ubo_host.radius_ws, 0.1f, 4.0f);
    dirty |=
        ImGui::SliderFloat("Intensity", &sao_.ubo_host.intensity, 0.0f, 100.0f);
    dirty |= ImGui::SliderFloat("Bias", &sao_.ubo_host.beta_bias, 1.0e-4f, 1.0f,
                                "%.6f");
    dirty |= ImGui::SliderInt("#samples", &sao_.ubo_host.num_samples, 1, 30);
    dirty |= ImGui::SliderInt("#turns", &sao_.ubo_host.num_spiral_turns, 1, 20);
    dirty |= ImGui::SliderFloat(
        "Max encode depth", &sao_.ubo_host.max_encode_depth, 100.0f, 1000.0f);
    dirty |=
        ImGui::SliderFloat("Bilateral 2x2 threshold",
                           &sao_.ubo_host.bilateral_2x2_threshold, 0.0f, 1.0f);
    dirty |=
        ImGui::SliderFloat("Bilateral threshold",
                           &sao_.ubo_host.bilateral_threshold, 0.0f, 1.0e4f);
    dirty |=
        ImGui::SliderFloat("Bilateral const weight",
                           &sao_.ubo_host.bilateral_const_weight, 0.0f, 1.0f);

    if (dirty) {
      UpdateUBO(sao_.ubo_host, sao_.ubo);
    }
  }

  // Tonemap/Gamma/Luma
  if (ImGui::CollapsingHeader("Tonemap/Gamma/Luma")) {
    TonemapGammaLuma& tgl = tonemap_gamma_luma_;
    bool dirty = false;

    dirty |=
        ImGui::SliderFloat("Exposure", &tgl.ubo_host.exposure, 0.0f, 100.0f);

    const char* tonemap_modes[] = {
        "Linear/none",        "Reinhard (on RGB)", "Reinhard",
        "Reinhard extended",  "Exposure",          "ACES filmic",
        "Uncharted 2 filmic",
    };

    dirty |= ImGui::Combo("Tonemap mode",
                          reinterpret_cast<int*>(&tgl.ubo_host.tonemap_mode),
                          tonemap_modes, IM_ARRAYSIZE(tonemap_modes));

    if (ImGui::TreeNode("Reinhard extended")) {
      dirty |= ImGui::SliderFloat("Lwhite", &tgl.ubo_host.L_white, 0.0f, 10.0f);
      ImGui::TreePop();
    }

    dirty |= ImGui::SliderFloat("Gamma", &tgl.ubo_host.gamma, 1.0f, 3.0f);

    if (dirty) {
      UpdateUBO(tgl.ubo_host, tgl.ubo);
    }
  }

  // FXAA
  if (ImGui::CollapsingHeader("FXAA")) {
    bool dirty = false;

    dirty |= ImGui::Checkbox("Enable FXAA",
                             reinterpret_cast<bool*>(&fxaa_.ubo_host.enabled));
    dirty |= ImGui::SliderFloat("Quality Sub-Pixel",
                                &fxaa_.ubo_host.quality_subpix, 1.0f, 0.0f);
    dirty |=
        ImGui::SliderFloat("Quality Edge Threshold",
                           &fxaa_.ubo_host.quality_edge_threshold, 0.5f, 0.05f);
    dirty |= ImGui::SliderFloat("Quality Edge Threshold Min",
                                &fxaa_.ubo_host.quality_edge_threshold_min,
                                0.1f, 0.01f, "%.4f");

    if (dirty) {
      UpdateUBO(fxaa_.ubo_host, fxaa_.ubo);
    }
  }

  // Debug visualizations
  if (ImGui::CollapsingHeader("Debug")) {
    const char* debug_modes[] = {
        "None",
        "Render order",
        "Display framebuffer",
        "Shadow maps",
    };
    int* debug_mode = reinterpret_cast<int*>(&debug_.params.render_mode);
    ImGui::Combo("Debug mode", debug_mode, debug_modes,
                 IM_ARRAYSIZE(debug_modes));

    // Settings for individual debug render modes

    // Render Order doesn't have any Gui and it's buffer needs an update
    // every frame just before the render call.

    if (ImGui::TreeNode("Display framebuffer")) {
      bool dirty = false;

      const char* fb_textures[] = {
          "GBuffer Albedo/Luminance",
          "GBuffer Normal",
          "GBuffer Metallic/Roughness/Reflectance",
          "Render Output Color",
          "Render Output Depth",
          "Linear Depth Pyramid",
          "Tonemap/Gamma/Luma",
          "OIT Accumulation",
          "OIT Revealage",
          "SAO",
      };
      int* fb = reinterpret_cast<int*>(&debug_.display_fb.params.tex);
      ImGui::Combo("Framebuffer", fb, fb_textures, IM_ARRAYSIZE(fb_textures));

      const char* display_modes[] = {
          "RGB",
          "Normals",
          "R",
          "G",
          "B",
          "A",
          "Linear Depth",
          "Linear [Near, Far]",
          "Position form Depth",
      };
      int* display_mode =
          reinterpret_cast<int*>(&debug_.display_fb.ubo_host.mode);
      dirty |= ImGui::Combo("Mode", display_mode, display_modes,
                            IM_ARRAYSIZE(display_modes));

      // TODO(stefalie): This only makes sense for textures that have
      // mips, i.e., only linear depth.
      dirty |= ImGui::SliderInt("Mip", &debug_.display_fb.ubo_host.mip_level, 0,
                                sao_.max_mip_level - 1);

      ImGui::TreePop();

      if (dirty) {
        UpdateUBO(debug_.display_fb.ubo_host, debug_.display_fb.ubo);
      }
    }

    // TODO(stefallie): Make this more generic, i.e., just a general
    // slider for layers and one for mipmaps. The tricky part: how to
    // check if there actually are several layers or mip levels.
    if (ImGui::TreeNode("Shadow Maps")) {
      bool dirty = false;

      dirty |= ImGui::SliderInt("Cascade",
                                &debug_.shadows.ubo_host.cascade_number, 0, 3);
      ImGui::TreePop();

      if (dirty) {
        UpdateUBO(debug_.shadows.ubo_host, debug_.shadows.ubo);
      }
    }
  }

  ImGui::Separator();
  ImGui::Text("%.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
              ImGui::GetIO().Framerate);

  if (renderer_) {
    ImGui::Separator();
    ImGui::Text("Num meshes: %u", renderer_->NumMeshes());
    ImGui::Text("Num materials: %u", renderer_->NumMaterials());
    ImGui::Text("Num textures: %lu", gl::TextureManager::Get().NumTextures());
    ImGui::Text("Num draw batches: %u", renderer_->stats().num_batches);
    ImGui::Text("Num draw calls: %u", renderer_->stats().num_draw_calls);
    ImGui::Text("Num draw instances: %u", renderer_->stats().num_instances);
  }

  ImGui::End();
}

void Viewer::KeyboardEvent(int key, int scancode, int action, int mods) {
  ImGui_ImplGlfw_KeyCallback(window_, key, scancode, action, mods);
  if (ImGui::GetIO().WantCaptureKeyboard) {
    return;
  }

  if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
    glfwSetWindowShouldClose(window_, GLFW_TRUE);
    return;
  }
}

void Viewer::MouseButtonEvent(int button, int action, int mods) {
  ImGui_ImplGlfw_MouseButtonCallback(window_, button, action, mods);
  if (ImGui::GetIO().WantCaptureMouse) {
    return;
  }

  if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS) {
    camera_.old_rotation = camera_.rotation;
    double glfw_x, glfw_y;
    glfwGetCursorPos(window_, &glfw_x, &glfw_y);
    const float x = 1.0f / framebuffer_height_ *
                    (2.0f * static_cast<float>(glfw_x) - framebuffer_width_);
    const float y =
        1.0f - 2.0f * static_cast<float>(glfw_y) / framebuffer_height_;

    camera_.trackball.BeginDrag(x, y);
    camera_.trackball_active = true;
  } else if (button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_RELEASE) {
    camera_.trackball_active = false;
  }
}

void Viewer::MouseMoveEvent(double xpos, double ypos) {
  const bool ctrl = (glfwGetKey(window_, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS ||
                     glfwGetKey(window_, GLFW_KEY_RIGHT_CONTROL) == GLFW_PRESS);
  const bool shift = (glfwGetKey(window_, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
                      glfwGetKey(window_, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS);
  const bool button_left =
      glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
  const bool button_middle =
      glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS;
  const bool button_right =
      glfwGetMouseButton(window_, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS;

  const float xpos_f = static_cast<float>(xpos);
  const float ypos_f = static_cast<float>(ypos);
  static float old_x = xpos_f;
  static float old_y = ypos_f;
  const float dx = xpos_f - old_x;
  const float dy = ypos_f - old_y;
  old_x = xpos_f;
  old_y = ypos_f;

  // Maya style controls.
  //
  // Rotate
  if (button_left && !ctrl && !shift) {
    if (camera_.trackball_active) {
      const float x_f =
          1.0f / framebuffer_height_ * (2.0f * xpos_f - framebuffer_width_);
      const float y_f = 1.0f - 2.0f * ypos_f / framebuffer_height_;

      camera_.trackball.Drag(x_f, y_f);
      camera_.rotation =
          camera_.trackball.incremental_rotation() * camera_.old_rotation;
      update_camera_matrices_ = true;
    }
  }

  // Pan
  if (button_middle || (button_left && ctrl && !shift)) {
    camera_.pivot_offset =
        (camera_.pivot_offset *
         Translation3f(
             camera_.rotation.block(0, 0, 3, 3).transpose() *
             Vector3f(camera_.pan_scale * dx, -camera_.pan_scale * dy, 0.0f)));
    update_camera_matrices_ = true;
  }

  // Zoom
  if (button_right || (button_left && !ctrl && shift)) {
    camera_.camera_distance -= camera_.zoom_scale * dy;
    update_camera_matrices_ = true;
  }
}

void Viewer::MouseScrollEvent(double xoffset, double yoffset) {
  ImGui_ImplGlfw_ScrollCallback(window_, xoffset, yoffset);
  if (ImGui::GetIO().WantCaptureMouse) {
    return;
  }

  const float scroll_zoom_scale = 10.0f;
  camera_.camera_distance +=
      -camera_.zoom_scale * scroll_zoom_scale * static_cast<float>(yoffset);
  update_camera_matrices_ = true;
}

void Viewer::UpdateCameraViewMatrix() {
  camera_.ubo_host.view = (Translation3f(0.0f, 0.0f, -camera_.camera_distance) *
                           Isometry3f(camera_.rotation) * camera_.pivot_offset)
                              .matrix();
  camera_.ubo_host.inverse_view = InvertIsometry(camera_.ubo_host.view);

  // Update the view matrix and its inverse. The projection matrix stays
  // the same and is not mapped.
  auto* ubo_device = reinterpret_cast<Camera::UBO*>(camera_.ubo.MapRange(
      0, 2 * sizeof(Matrix4f), GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT));
  ubo_device->view = camera_.ubo_host.view;
  ubo_device->inverse_view = camera_.ubo_host.inverse_view;
  camera_.ubo.Unmap();
}

void Viewer::UpdateShadowMatrices() {
  // Compute light view matrix.
  Vector3f light_dir = pbr_.ubo_host.key_light.dir;
  if (pbr_.ubo_host.light_dirs_in_eye_space) {
    light_dir = camera_.ubo_host.inverse_view.block(0, 0, 3, 3) * light_dir;
  }

  // Build light view with no translation.
  light_dir.normalize();
  const float l_dot_y = light_dir.dot(Vector3f::UnitY());
  const Vector3f up = (l_dot_y < 0.9f) ? Vector3f::UnitY() : Vector3f::UnitX();
  const Matrix4f light_view = LookAt(Vector3f::Zero(), -light_dir, up);
  shadows_.ubo_host.light_view_matrix = light_view;
  shadows_.ubo_host.light_dir.head<3>() = light_dir;

  if (shadows_.params.stabilize) {
    UpdateShadowCropMatricesStable();
  } else {
    UpdateShadowCropMatricesTight();
  }

  UpdateUBO(shadows_.ubo_host, shadows_.ubo);
}

void Viewer::UpdateShadowCropMatricesTight() {
  // Transform all corners of all split frusta into light coordinates.
  const Matrix4f eye_to_light =
      shadows_.ubo_host.light_view_matrix * camera_.ubo_host.inverse_view;
  Eigen::Vector3f corners_light[Shadows::Params::kNumSplits + 1][4];
  for (size_t i = 0; i < (Shadows::Params::kNumSplits + 1); ++i) {
    for (size_t j = 0; j < 4; ++j) {
      corners_light[i][j] =
          (eye_to_light * shadows_.params.frusta_eye[i][j]).head<3>();
    }
  }

  // Find AABB for each frustum in light coordinates. (Alternatively one
  // could compute a AABB on each of the 5 split planes and then merge
  // each pair of adjecents AABBs.)
  AlignedBox3f aabb_cascade[Shadows::Params::kNumSplits];
  for (size_t i = 0; i < Shadows::Params::kNumSplits; ++i) {
    for (size_t j = 0; j < 4; ++j) {
      aabb_cascade[i].extend(corners_light[i][j]);
    }
    for (size_t j = 0; j < 4; ++j) {
      aabb_cascade[i].extend(corners_light[i + 1][j]);
    }
    assert(!aabb_cascade[i].isEmpty());
  }

  // Generate crop matrices (i.e., orthogonal projections).
  for (size_t i = 0; i < Shadows::Params::kNumSplits; ++i) {
    // Note the negation and switch (of min/max) of the z values. This is
    // because the camera looks down the negative z-axis, and near/far
    // must be provided along that view direction.
    const AlignedBox3f aabb = aabb_cascade[i];
    shadows_.ubo_host.light_projection[i] =
        OrthogonalProjection(aabb.min().x(), aabb.max().x(), aabb.min().y(),
                             aabb.max().y(), -aabb.max().z(), -aabb.min().z());
  }
}

// The idea is from:
// http://the-witness.net/news/2010/03/graphics-tech-shadow-maps-part-1/,
// which again is from Michal Valient's "Stable Rendering of Cascaded
// Shadow Maps" article in ShaderX 6. The compact storing done by Jon Blow
// in his follow up article is not implemented.
void Viewer::UpdateShadowCropMatricesStable() {
  const Matrix4f light_view = shadows_.ubo_host.light_view_matrix;
  const Matrix4f eye_to_light = light_view * camera_.ubo_host.inverse_view;
  for (size_t i = 0; i < Shadows::Params::kNumSplits; ++i) {
    // Transform the sphere center of the split frustum into light
    // coordinates.
    const Eigen::Vector4f center_eye(
        0.0f, 0.0f, -shadows_.params.frusta_spheres[i].depth, 1.0f);
    const Eigen::Vector3f center_light = (eye_to_light * center_eye).head<3>();

    // The light projection places the precomputed crop matrices around
    // the sphere center (in light space).
    const Affine3f translate = Affine3f(Translation3f(-center_light));
    Matrix4f& ligh_proj = shadows_.ubo_host.light_projection[i];
    ligh_proj = (shadows_.params.ortho_crops[i] * translate).matrix();
  }

  // Snap position to integer multiple of shadow map texel size to avoid
  // flickering/shimmering under camera motion.
  // Adapted from: https://github.com/TheRealMJP/Shadows
  for (size_t i = 0; i < Shadows::Params::kNumSplits; ++i) {
    Matrix4f& ligh_proj = shadows_.ubo_host.light_projection[i];

    // Project the world space origin (any other point should work too)
    // into light/shadow space for rounding. (ligh_proj * light_view *
    // vec4(0, 0, 0, 1)).uv Simpler version: extract only uv translation
    Eigen::Vector2f origin_light = (ligh_proj * light_view).block(0, 3, 2, 1);

    // Get fractional offset (in texels) in shadow map space.
    const float shadow_map_size = shadows_.fb.tex_light_depth.w;
    origin_light *= shadow_map_size * 0.5f;
    const Eigen::Vector2f rounded_origin = origin_light.array().round();
    Eigen::Vector2f offset = rounded_origin - origin_light;

    // Convert offset back into [-1, 1] range and apply it.
    offset *= 2.0f / shadow_map_size;
    ligh_proj(0, 3) += offset.x();
    ligh_proj(1, 3) += offset.y();
  }
}

void Viewer::UpdateCameraProjection() {
  camera_.ubo_host.projection = PerspectiveProjection(
      camera_.ubo_host.fovy, camera_.ubo_host.aspect_ratio,
      camera_.ubo_host.near, camera_.ubo_host.far);
  camera_.ubo_host.inverse_projection =
      InvertPerspectiveProjection(camera_.ubo_host.projection);

  // Update:
  // - projection matrix
  // - inverse projection matrix
  // - reconstruction info
  // - fovy
  // - near
  // - far
  // - map_ws_to_ss
  // Only these updated parts of the UBO are mapped.
  const GLintptr offset = 2 * sizeof(Matrix4f);
  const GLsizeiptr size =
      2 * sizeof(Matrix4f) + 1 * sizeof(Vector4f) + 4 * sizeof(float);
  uint8_t* ptr = reinterpret_cast<uint8_t*>(camera_.ubo.MapRange(
      offset, size, GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_RANGE_BIT));
  Camera::UBO* ubo_device = reinterpret_cast<Camera::UBO*>(ptr - offset);
  ubo_device->projection = camera_.ubo_host.projection;
  ubo_device->inverse_projection = camera_.ubo_host.inverse_projection;

  const Matrix4f& p = ubo_device->projection;
  // Note the differences to 'McGuire - 2012 - Scalable Ambient
  // Obscurance'. P_12 has a positive sign in the paper, I think that's
  // wrong but it doesn't matter as the usual projection matrix is zero
  // there. Further is there 2nd (wrong) minus in the 2nd component in the
  // paper (but not in the supplemental material and also not here).
  camera_.ubo_host.reconstruct_info = {
      (-2.0f / (camera_.ubo_host.framebuffer_width * p(0, 0))),  // == -1 / near
      (-2.0f / (camera_.ubo_host.framebuffer_height * p(1, 1))),  // == -1 / far
      (1.0f - p(0, 2)) / p(0, 0),  // normall == 1 / P_00
      (1.0f - p(1, 2)) / p(1, 1),  // normall == 1 / P_11
  };
  ubo_device->reconstruct_info = camera_.ubo_host.reconstruct_info;

  ubo_device->fovy = camera_.ubo_host.fovy;
  ubo_device->near = camera_.ubo_host.near;
  ubo_device->far = camera_.ubo_host.far;

  const float tan_half_fovy =
      tan(ubo_device->fovy * 0.5f * static_cast<float>(M_PI) / 180.0f);
  camera_.ubo_host.map_ws_to_ss =
      camera_.ubo_host.framebuffer_height / (2.0f * tan_half_fovy);
  ubo_device->map_ws_to_ss = camera_.ubo_host.map_ws_to_ss;

  camera_.ubo.Unmap();
}

void Viewer::UpdateCascadeSplits() {
  const float near = camera_.ubo_host.near;
  const float far = shadows_.params.max_shadow_depth;
  const float lambda = shadows_.params.split_lambda;

  // lambda * n * (f / n)^(i / N) + (1 - lambda) * (n + i / N * (f - n))
  assert(Shadows::Params::kNumSplits == 4);
  const Vector4f i_over_N = Vector4f(1.0f, 2.0f, 3.0f, 4.0f) / 4.0f;
  const Vector4f log_splits = near * Eigen::pow(far / near, i_over_N.array());
  const Vector4f lin_splits = near * Vector4f::Ones() + i_over_N * (far - near);
  const Vector4f splits = lambda * log_splits + (1.0f - lambda) * lin_splits;
  shadows_.ubo_host.split_depths = splits;

  // Compute corners of split frusta in camera space.
  const float tan_half_fovy =
      tan(camera_.ubo_host.fovy * 0.5f * static_cast<float>(M_PI) / 180.0f);

  float depth = camera_.ubo_host.near;
  for (size_t i = 0; i < (Shadows::Params::kNumSplits + 1); ++i) {
    const float y_extent = tan_half_fovy * depth;
    const float x_extent = y_extent * camera_.ubo_host.aspect_ratio;
    shadows_.params.frusta_eye[i][0] = {-x_extent, -y_extent, -depth, 1.0f};
    shadows_.params.frusta_eye[i][1] = {x_extent, -y_extent, -depth, 1.0f};
    shadows_.params.frusta_eye[i][2] = {x_extent, y_extent, -depth, 1.0f};
    shadows_.params.frusta_eye[i][3] = {-x_extent, y_extent, -depth, 1.0f};

    // Set distance for next split if there will be one more.
    if (i < Shadows::Params::kNumSplits) {
      depth = splits[i];
    }
  }

  UpdateCascadeBoundingSpheres();

  // TODO(stefalie): This currently updates more of the buffer than
  // necessary.
  UpdateUBO(shadows_.ubo_host, shadows_.ubo);
}

// Implements:
// https://lxjk.github.io/2017/04/15/Calculate-Minimal-Bounding-Sphere-of-Frustum.html
// but assumes that the FOV is along the y-axis and not the x-axis.
void Viewer::UpdateCascadeBoundingSpheres() {
  const float near = camera_.ubo_host.near;

  auto bounding_sphere = [](const float aspect, const float fovy,
                            const float near, const float far) {
    Shadows::Params::FrustumBoundingSphere ret;
    const float tan_half_fovy =
        tan(fovy * 0.5f * static_cast<float>(M_PI) / 180.0f);

    const float k = sqrt(1.0f + aspect * aspect) * tan_half_fovy;
    const float k_squared = k * k;
    const float far_sub_near = far - near;
    const float far_add_near = far + near;

    if (k_squared >= far_sub_near / far_add_near) {
      ret.depth = far;
      ret.radius = far * k;
    } else {
      ret.depth = 0.5f * far_add_near * (1.0f + k_squared);
      ret.radius =
          0.5f * sqrt(far_sub_near * far_sub_near +
                      2.0f * (far * far + near * near) * k_squared +
                      far_add_near * far_add_near * k_squared * k_squared);
    }
    return ret;
  };
  float near_i = near;
  for (size_t i = 0; i < Shadows::Params::kNumSplits; ++i) {
    const float far_i = shadows_.ubo_host.split_depths[i];
    shadows_.params.frusta_spheres[i] = bounding_sphere(
        camera_.ubo_host.aspect_ratio, camera_.ubo_host.fovy, near_i, far_i);

    const float radius = shadows_.params.frusta_spheres[i].radius;
    shadows_.params.ortho_crops[i] =
        Vector4f(1.0f / radius, 1.0f / radius, -1.0f / radius, 1.0f)
            .asDiagonal();
    // This is identical to:
    // OrthogonalProjection(-radius, radius, -radius, radius, -radius,
    // radius);

    near_i = far_i;
  }
}

void Viewer::AllocMSAAFBOs(PBR::RenderOutput::ForwardMSAA* output_msaa,
                           int width, int height, int n_samples) {
  output_msaa->rb_color.Create("Render output MSAA rb: color");
  output_msaa->rb_color.Alloc(GL_RGB16F, width, height, n_samples);

  output_msaa->rb_depth.Create("Render output MSAA rb: depth");
  output_msaa->rb_depth.Alloc(GL_DEPTH_COMPONENT32, width, height, n_samples);

  output_msaa->fbo.Create("Render output MSAA FBO");
  output_msaa->fbo.Alloc({{&output_msaa->rb_depth, GL_DEPTH_ATTACHMENT},
                          {&output_msaa->rb_color, GL_COLOR_ATTACHMENT0}},
                         {});
}

}  // namespace viewer
