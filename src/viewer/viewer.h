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

#include <Eigen/Dense>
#include <array>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <string>

#include "viewer/gl/init.h"
#include "viewer/gl/objects.h"
#include "viewer/gl/trackball.h"

struct GLFWwindow;

namespace viewer {

namespace gl {
class Renderer;
class Shader;
}  // namespace gl

class Viewer {
 public:
  // TODO(stefalie): Consider getting rid of num_samples since it is only for
  // the main framebuffer managed by GLFW. The actual target that you want
  // multi-sampling on is another FBO.
  bool Init(const std::string& title, unsigned width, unsigned height,
            unsigned num_samples);
  void Destroy();

  void MainLoop(const std::function<void()>& render_custom_gui);

  void RenderSingleFrame();

  // The camera parameters consist of 8 floats (4 for rotation quaternion, 3 for
  // translation, and 1 for the distance).
  void SetCameraParameters(const std::array<float, 8>& params);
  void GetCameraParameters(std::array<float, 8>* params) const;

  bool SaveScreenshot(const std::string& file_name) const;

  void ResetCamera();
  void ResetCameraPanAndZoomScales();

  void UpdateRenderer(gl::Renderer* renderer);
  void UpdateRendererAsync(gl::Renderer* renderer);
  void UpdateRendererAsyncInSteps(gl::Renderer* renderer);

  // Getters and setters.
  void ToggleRenderSettings(bool show) { show_render_settings_gui_ = show; }

  gl::Renderer* renderer() { return renderer_; }

  void set_auto_reset_camera(bool auto_reset_camera) {
    auto_reset_camera_ = auto_reset_camera;
  }

 private:
  void Render();
  void LFBPrefixSum();
  void LFBPrefixSumRec(int num_elements, int offset);
  void RenderDebug();
  void RenderRenderingSettingsGUI();

  void KeyboardEvent(int key, int scancode, int action, int mods);
  void MouseButtonEvent(int button, int action, int mods);
  void MouseMoveEvent(double xpos, double ypos);
  void MouseScrollEvent(double xoffset, double yoffset);

  // Called frequently (every frame if camera moves)
  void UpdateCameraViewMatrix();
  void UpdateShadowMatrices();
  void UpdateShadowCropMatricesTight();
  void UpdateShadowCropMatricesStable();

  // Called rarely (only when user changes params)
  void UpdateCameraProjection();
  void UpdateCascadeSplits();

  // Compute bounding spheres of each frustum for optimized PCF (shadows maps in
  // The Witness).
  void UpdateCascadeBoundingSpheres();

  GLFWwindow* window_ = nullptr;
  int framebuffer_width_;
  int framebuffer_height_;
  float dpi_scale_;
  int max_num_samples_;

  // Flags that are set whenever camera/light view/projection matrix need to be
  // updated in the beginning of the next frame.
  bool update_camera_matrices_ = true;
  bool update_shadow_matrices_ = true;

  gl::Sampler sampler_bilinear_;
  gl::Sampler sampler_nearest_;
  gl::Sampler sampler_bilinear_mips_;

  bool show_render_settings_gui_ = false;
  struct RenderSettings {
    bool vsync = true;
    bool limit_fps_to_60 = false;
    bool update_renderer = true;
    bool indirect_draws = true;
  } render_settings_;

  struct Camera {
    gl::Trackball trackball;
    bool trackball_active = false;
    Eigen::Matrix4f old_rotation;

    const float pan_scale_default = 0.0035f;
    const float zoom_scale_default = 0.01f;
    float pan_scale = pan_scale_default;
    float zoom_scale = zoom_scale_default;

    // These 3 elements are required to store and reload the camera information.
    Eigen::Matrix4f rotation;
    Eigen::Translation3f pivot_offset;
    float camera_distance;

    struct UBO {
      // The following change frequently.
      Eigen::Matrix4f view;
      Eigen::Matrix4f inverse_view;

      // The following change rarely (only when changed in the UI).:w
      Eigen::Matrix4f projection;
      Eigen::Matrix4f inverse_projection;
      Eigen::Vector4f reconstruct_info;
      float fovy = 45.0f;
      float near = 1.0f;
      float far = 1000.0f;
      // Scale from an imaginary near plane at distance 1 to screen space size
      // in pixels. Used for SAO. It's: height / (2 * tan(fovy * 0.5)).
      float map_ws_to_ss;

      // The following never change.
      float aspect_ratio;
      int framebuffer_width;
      int framebuffer_height;
    } ubo_host;
    gl::Buffer ubo;
  } camera_;

  struct BackgroundAndSky {
    gl::Shader* shader_sky = nullptr;
    gl::Shader* shader_plane = nullptr;

    struct Params {
      float clear_color[3] = {1.0f, 1.0f, 1.0f};
      float clear_multiplier = 2.5f;
      bool enable_sky = true;
      bool enable_infinite_ground_plane = false;
    } params;

    struct UBO {
      Eigen::Vector4f ground_plane_color = {1.0f, 1.0f, 1.0f, 1.0f};
      float ground_plane_height = 0.0f;
    } ubo_host;
    gl::Buffer ubo;
  } bg_sky_;

  struct PBR {
    gl::Shader* shader_forward = nullptr;
    gl::Shader* shader_gbuffer = nullptr;
    gl::Shader* shader_deferred = nullptr;
    gl::Shader* shader_reconstruct_linear_depth = nullptr;

    struct RenderOutput {
      gl::Texture tex_color;
      gl::Texture tex_depth;
      gl::FrameBuffer fbo_forward;
      gl::FrameBuffer fbo_deferred;

      struct LinearDepthPyramid {
        gl::Texture tex;
        gl::FrameBuffer fbo;
      } linear_depth_pyramid;

      struct ForwardMSAA {
        gl::RenderBuffer rb_color;
        gl::RenderBuffer rb_depth;
        gl::FrameBuffer fbo;
      } msaa;
    } render_output;

    struct GBuffer {
      // This FBO includes render_output.tex_depth.
      gl::Texture tex_albedo;
      gl::Texture tex_normals;
      // This FBO includes render_output.linear_depth_pyramid.tex.
      gl::Texture tex_material;
      gl::FrameBuffer fbo;
    } gbuffer;

    enum class RenderMode : int {
      DEFERRED,
      FORWARD,
    };
    enum class LinarDepthBits : int {
      LIN_DEPTH_16,
      LIN_DEPTH_32,
    };

    struct Params {
      RenderMode render_mode = RenderMode::DEFERRED;
      // 16 bits are not enough, needs 32 to look ok.
      LinarDepthBits linear_depth_bits = LinarDepthBits::LIN_DEPTH_32;
      int forward_num_samples = 4;
    } params;

    struct DirectionalLight {
      Eigen::Vector3f dir;
      float intensity;
      Eigen::Vector3f color;
      int _pad = 0;
    };

    struct UBO {
      DirectionalLight key_light = {Eigen::Vector3f(1.0f, 1.0f, 1.0f), 6.0f,
                                    Eigen::Vector3f::Ones()};
      DirectionalLight fill_light = {Eigen::Vector3f(-0.75f, 0.5f, 0.9f), 2.4f,
                                     Eigen::Vector3f::Ones()};
      DirectionalLight back_light = {Eigen::Vector3f(0.0f, 0.625f, -0.75f),
                                     1.2f, Eigen::Vector3f::Ones()};
      Eigen::Vector3f ambient_light_color = Eigen::Vector3f::Ones();
      float ambient_light_intensity = 0.03f;
      int light_dirs_in_eye_space = 0;
    } ubo_host;
    gl::Buffer ubo;
  } pbr_;

  // Bundles allocation of MSAA buffers to make it easy to reallocate when
  // number of samples changes.
  static void AllocMSAAFBOs(PBR::RenderOutput::ForwardMSAA* output_msaa,
                            int width, int height, int n_samples);

  struct Transparency {
    enum class Method : int {
      OIT,
      LFB,
      LFB_ADAPTIVE,
      LFB_LL,
      LFB_LL_ADAPTIVE,
    };

    struct Params {
      Method method = Method::LFB_LL;
    } params;

    struct WeightedBlendedOrderIndependentTransparency {
      gl::Shader* shader_accum = nullptr;
      gl::Shader* shader_resolve = nullptr;

      struct FrameBuffer {
        gl::Texture tex_accum;
        gl::Texture tex_revealage;
        // This FBO includes pbr_.render_output.tex_depth.
        gl::FrameBuffer fbo;
      } fb;
    } oit;

    struct LayeredFragmentBuffer {
      gl::Shader* shader_count = nullptr;
      gl::Shader* shader_prefix_sum = nullptr;
      gl::Shader* shader_prefix_sum_adjust_incr = nullptr;
      gl::Shader* shader_fill = nullptr;
      gl::Shader* shader_fill_ll = nullptr;
      gl::Shader* shader_sort_and_blend = nullptr;
      gl::Shader* shader_adaptive_blend = nullptr;
      gl::Shader* shader_sort_and_blend_ll = nullptr;
      gl::Shader* shader_adaptive_blend_ll = nullptr;

      const int elements_per_thread_group = 2048;

      gl::Buffer ssbo_count;
      gl::Buffer ssbo_linked_list_header;
      gl::Buffer ssbo_linked_list_next_ptrs;
      gl::Buffer ssbo_fragments;
      struct FrameBuffer {
        // This FBO has no color attachments but includes
        // pbr_.render_output.tex_depth.
        gl::FrameBuffer fbo;
      } fb;

      struct UBO {
        int num_elements;
        int max_num_fragments;
      } ubo_host;
      gl::Buffer ubo;
    } lfb;
  } transparency_;

  struct Shadows {
    gl::Shader* shader = nullptr;

    struct FrameBuffer {
      gl::Texture tex_light_depth;
      gl::FrameBuffer fbo;
    } fb;

    gl::Sampler sampler;

    enum class MapResolution : int {
      RES_1024,
      RES_1440,
      RES_2048,
      RES_4096,
    };

    struct Params {
      const GLsizei num_cascades = 4;
      MapResolution map_resolution = MapResolution::RES_1440;
      float max_shadow_depth = 350.0f;
      float split_lambda = 0.7f;
      bool stabilize = true;

      // 5 times 4 corners for the 4 frusta. This is stored in camera space.
      static const size_t kNumSplits = 4;
      Eigen::Vector4f frusta_eye[kNumSplits + 1][4];

      // Optimized shadows from The Witness
      struct FrustumBoundingSphere {
        float radius;
        float depth;
      } frusta_spheres[kNumSplits];
      Eigen::Matrix4f ortho_crops[kNumSplits];
    } params;

    struct UBO {
      Eigen::Matrix4f light_view_matrix;
      Eigen::Matrix4f light_projection[Params::kNumSplits];
      Eigen::Vector4f split_depths;
      // Could also be extracted from light_view_matrix, but this is easier.
      Eigen::Vector4f light_dir;

      float shadow_strength = 0.6f;

      // Biases
      float const_depth_bias = 0.7f;
      float slope_scale_depth_bias = 1.7f;
      float normal_offset_bias = 1.7f;
      int normal_offset_uv_only = 0;
      int use_receiver_plane_bias = 0;
      int fade_cascades = 1;

      int enabled = 1;
      int debug_show_cascades = 0;
    } ubo_host;
    gl::Buffer ubo;
  } shadows_;

  struct ScalableAmbientObscurance {
    // TODO(stefalie): Consider moving the shader into the PBR struct where the
    // corresponding FBO is. The pyramid might also help for other techniques.
    gl::Shader* shader_depth_pyramid = nullptr;
    gl::Shader* shader_sao = nullptr;
    gl::Shader* shader_blur_x = nullptr;
    gl::Shader* shader_blur_y = nullptr;

    const int max_mip_level = 5;

    struct FrameBufferSAO {
      gl::Texture tex_sao;
      gl::FrameBuffer fbo;
    } fb_sao;

    struct FrameBufferBlurX {
      gl::Texture tex_blurred_x;
      gl::FrameBuffer fbo;
    } fb_blurred_x;

    enum class LinarDepthBits : int {
      LIN_DEPTH_16,
      LIN_DEPTH_32,
    };

    struct Params {
      bool enabled = true;
      bool debug_sao_only = false;
    } params;

    struct UBO {
      // For pyramid
      int src_mip_level = 0;

      float radius_ws = 1.0f;    // Alchemy AO used 0.5
      float k_constrast = 1.0f;  // Unused (to avoid pow)
      float intensity = 25.0f;
      float beta_bias = 0.012f;  // Alchemy AO uses 1e-4
      int num_samples = 11;
      int num_spiral_turns = 7;

      float max_encode_depth = 300.0f;
      float bilateral_2x2_threshold = 0.02f;
      float bilateral_threshold = 2000.0f;
      float bilateral_const_weight = 0.0f;
    } ubo_host;
    gl::Buffer ubo;
  } sao_;

  struct TonemapGammaLuma {
    gl::Shader* shader = nullptr;

    struct FrameBuffer {
      gl::Texture tex;
      gl::FrameBuffer fbo;
    } fb;

    enum class TonemapMode : int {
      LINEAR,
      REINHARD_RGB,
      REINHARD,
      REINHARD_EXTENDED,
      EXPOSURE,
      ACES_FILMIC,
      UNCHARTED2_FILMIC,
    };

    struct UBO {
      // For all
      float exposure = 1.0f;
      TonemapMode tonemap_mode = TonemapMode::UNCHARTED2_FILMIC;

      // For Reinhard Extended
      float L_white = 4.0f;

      float gamma = 2.0f;
    } ubo_host;
    gl::Buffer ubo;
  } tonemap_gamma_luma_;

  struct Fxaa {
    gl::Shader* shader = nullptr;

    struct UBO {
      // Reciprocal frame, i.e., { 1 / screen_width, 1 / screen_height }
      Eigen::Vector2f rcp_frame;

      // FXAA_QUALITY__SUBPIX
      // This can effect sharpness.
      //   1.00 - upper limit (softer)
      //   0.75 - default amount of filtering
      //   0.50 - lower limit (sharper, less sub-pixel aliasing removal)
      //   0.25 - almost off
      //   0.00 - completely off
      float quality_subpix = 0.75f;

      // FXAA_EDGE_THRESHOLD
      // The minimum amount of local contrast required to apply algorithm.
      //   0.333 - too little (faster)
      //   0.250 - low quality
      //   0.166 - default
      //   0.125 - high quality
      //   0.063 - overkill (slower)
      float quality_edge_threshold = 0.125f;

      // FXAA_EDGE_THRESHOLD_MIN
      // Trims the algorithm from processing darks.
      //   0.0833 - upper limit (default, the start of visible unfiltered edges)
      //   0.0625 - high quality (faster)
      //   0.0312 - visible limit (slower)
      float quality_edge_threshold_min = 0.0312f;

      int enabled = 1;
    } ubo_host;
    gl::Buffer ubo;
  } fxaa_;

  // Debugging visualizations
  struct Debug {
    enum class RenderMode : int {
      NONE,
      RENDER_ORDER,
      DISPLAY_TEXTURE,
      SHADOW_MAPS,
    };

    struct Params {
      RenderMode render_mode = RenderMode::NONE;
    } params;

    struct RenderOrder {
      gl::Shader* shader = nullptr;
      struct UBO {
        int num_instances;
      } ubo_host;
      gl::Buffer ubo;
    } render_order;

    struct DisplayFB {
      gl::Shader* shader = nullptr;

      enum class Tex : int {
        GBUFFER_ALBEDO,
        GBUFFER_NORMALS,
        GBUFFER_MATERIAL,
        OUTPUT_COLOR,
        OUTPUT_DEPTH,
        OUTPUT_LINEAR_DEPTH_PYRAMID,
        TONEMAP_GAMMA_LUMA,
        OIT_ACCUMULATION,
        OIT_REVEALAGE,
        SAO,
      };

      enum class Mode : int {
        RGB,
        NORMALS,
        R,
        G,
        B,
        A,
        LINEAR_DEPTH,
        LINEAR_NEAR_FAR,
        POSITION_FROM_DEPTH,
      };

      struct Params {
        Tex tex = Tex::GBUFFER_ALBEDO;
      } params;
      struct UBO {
        int mode = static_cast<int>(Mode::RGB);
        int mip_level = 0;
      } ubo_host;
      gl::Buffer ubo;
    } display_fb;

    struct Shadows {
      gl::Shader* shader = nullptr;
      struct UBO {
        int cascade_number = 0;
      } ubo_host;
      gl::Buffer ubo;
    } shadows;
  } debug_;

  // Renderer and renderer update
  gl::Renderer* renderer_ = nullptr;

  bool auto_reset_camera_ = true;

  gl::VertexArray fullscreen_quad_vao_;

  struct AsyncRendererUpdate {
    enum class State : char {
      IDLE,
      WAIT_4_ALLOC,
      ALLOCATED,
      READY_2_SWAP,
      WAIT_4_UNMAP_SYNC,
      ABORT,  // To prevent a threads from waiting after the main loop ends.
      UPDATE_ALL_IN_ONE_GO,
    } state;
    std::mutex mutex;
    std::condition_variable cond_var;

    gl::Renderer* new_renderer;
  } update_;

  struct ImGuiConsts {
    const float margin = 12.0f;
    const float window_width = 300.0f;
  } imgui_;
};

}  // namespace viewer
