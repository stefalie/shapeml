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

#ifdef _WIN32
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <imgui/imgui.h>
#include <imgui/imgui_stdlib.h>

#include <cstring>
#include <ctime>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include "shapeml/exporter.h"
#include "shapeml/grammar.h"
#include "shapeml/interpreter.h"
#include "shapeml/parser/lexer.h"
#include "shapeml/parser/parser.h"
#include "shapeml/shape.h"
#include "shapeml/util/hex_color.h"
#include "viewer/gl/renderer.h"
#include "viewer/gl/texture_manager.h"
#include "viewer/viewer.h"

const char* ERROR_MSG =
    R"help(ERROR: Incorrect usage, use as follows:
./ShapeMaker
 [--axiom AXIOM_NAME]
 [--seed INT]
 [--export-obj]
 [--export-dont-merge-vertices]
 [--export-dir PATH]
 [--export-suffix STRING]
 [--help]
 [--only-lexer]
 [--only-parser]
 [--no-gui]
 [--parameter PARAMETER_NAME=VALUE]
 [--print-grammar]
 [--print-shape-tree]
 [--screenshot]
 [--window-height (480|600|720|768|960|1080|1200)]
 [--camera R0 R1 R2 R3 T0 T1 T2 D]
 [--show-ground-plane]
 [--ground-plane-elevation FLOAT]
 [--ground-plane-color R G B]

 PATH_TO_GRAMMAR_FILE)help";

const char* HELP_MESSAGE =
    R"help(The easiest way to run ShapeMaker is with only the mandatory arguments:
 ./ShapeMaker PATH_TO_GRAMMAR_FILE

For advanced usage and more control, the following list describes all possible
arguments for ShapeMaker:

 --axiom AXIOM_NAME (optional)
    The name of the start rule or axiom. Default value is 'Axiom'.

 --seed INT (optional)
    The seed for the random number generator. Defalut value is time(0).

 --export-obj (optional)
    Exports a OBJ mesh file of the derived model. Ignored in GUI mode (--no-gui
    not set) since the GUI provides an export button.

 --export-dont-merge-vertices (optional)
    By default, vertices in the exported mesh are merged together if they
    conincide with other vertices. This flag disables vertex merging. The
    default merging is done by discretizing the points into a 3D grid whose cell
    side length is 1/(2^16). Ignored in GUI mode.

 --export-dir PATH (optional)
    The path to the directory to which the exporter will write the mesh file. If
    this argument is not set, the default export directory is the current
    working directory.

 --export-suffix STRING (optional)
    A suffix that is appended to name upon export. Useful for enumerating the
    outputs when batch scripting (prevents the exporter from overwriting already
    existing files).

 --help (optional)
    Prints this help message.

 --only-lexer (optional)
    Runs only the lexer and prints the list of tokens to standard output.

 --only-parser (optional)
    Runs only the lexer and parser.

 --no-gui (optional)
    The interactive GUI will not be opened.

 --parameter PARAMETER_NAME=VALUE (optional)
    Allows changing the default value of a parameter by defining it as a name/
    value pair. The -parameter argument can be used several times to pass
    multiple name/value pairs. Note that the easiest way to change parameters is
    through the interactive GUI. If ShapeMaker is run with --no-gui,
    parameters can only be changed with -parameter.

 --print-grammar (optional)
    Prints the grammar source to standard output. Ignored in GUI mode.

 --print-shape-tree (optional)
    Prints the shape tree with its properties to standard output. Ignored in GUI
    mode.

 --screenshot (optional)
    Saves a screenshot in png format. Ignored in GUI mode since the GUI provides
    a screenshot button. (It does, however, open a GUI window but immediately
    closes it again after the image has been grabbed from the framebuffer.)

 --window-height (480|600|720|768|960|1080|1200) (optional)
    The height of the window. The aspect ratio will be 4:3. Default window size
    is 800x600;

 --camera R0 R1 R2 R3 T0 T1 T2 D (optional)
    Sets the startup camera parameters. Useful for batch scripting. r0-r3 is a
    quaternion storing the orientation of the trackball. t0-t2 is the offset
    translation of the model to the trackball pivot. d is the distance of the
    camera from the trackball pivot. These values can be exported from the GUI.

 --show-ground-plane (optional)
    Shows a ground plane.

 --ground-plane-elevation FLOAT (optional)
    The height of the ground plane (if it's activated).

 --ground-plane-color R G B (optional)
    The color of the ground plane (if it's activated).

 PATH_TO_GRAMMAR_FILE (mandatory)
    The path to the ShapeML file containing the grammar to be derived. Only one
    grammar file can be passed to ShapeMaker.)help";

void FilterColorParameters();
shapeml::Shape* Derive();
void DerivationThread();
viewer::gl::Renderer* CreateRenderer(const shapeml::Shape* shape);
void RenderGui();
bool ParseParameter(const std::string& key_val, shapeml::ValueDict* params);
bool ParseInt(const std::string& val, int* int_value);
bool ParseFloat(const std::string& val, float* float_val);

struct Context {
  // TODO(stefalie): Consider the octree max size to be larger.
  const shapeml::Scalar octree_extent = 1000.0f;
  const int max_derivation_steps = 1000;

  int seed;
  std::string file_name;
  std::string axiom_name;
  shapeml::Grammar grammar;
  shapeml::ValueDict parameters;
  bool has_grammar = false;
  shapeml::Shape* root = nullptr;
} context;

viewer::Viewer shapeml_viewer;

struct GuiSettings {
  std::string grammar_file;
  bool auto_derive = false;
  bool auto_reset_camera = true;
  std::string export_name;
  bool export_merge_vertices;
  bool show_rendering_settings = false;
  // ID of renderable and material of ground plane. Since they're added first,
  // they'll both be 0.
  const int32_t ground_plane_id = 0;
  bool show_ground_plane = false;
  float ground_plane_elevation = 0.0f;
  Eigen::Vector4f ground_plane_color = {1.0f, 1.0f, 1.0f, 1.0f};
  int new_seed;
  std::string new_axiom_name;
  shapeml::ValueDict new_parameters;
  std::unordered_map<std::string, shapeml::Vec3f> color_param_map;
  bool parameters_changed = false;
} gui_settings;

struct DerivationThreadData {
  std::mutex derivation_mutex;
  std::mutex model_mutex;
  std::condition_variable cond_var;
  bool is_deriving = false;
  bool finished = false;
} derivation_thread_data;

int main(int argc, char* argv[]) {
  // Argument parsing
  bool flag_err = false;
  context.seed = static_cast<int>(time(nullptr));
  bool flag_export_obj = false;
  bool flag_export_dont_merge_vertices = false;
  std::string export_dir;
  std::string export_suffix;
  bool flag_help = false;
  bool flag_only_lexer = false;
  bool flag_only_parser = false;
  bool flag_no_gui = false;
  shapeml::ValueDict parameters_argv;
  bool flag_print_grammar = false;
  bool flag_print_shape_tree = false;
  bool flag_screenshot = false;
  int window_width = 800;
  int window_height = 600;
  bool flag_has_camera_params = false;
  std::array<float, 8> camera_params;

  int i = 1;
  while (i < argc) {
    if (std::string(argv[i]) == "--axiom") {
      ++i;
      if (i < argc && context.axiom_name.empty()) {
        context.axiom_name = argv[i];
      } else {
        flag_err = true;
      }
    } else if (std::string(argv[i]) == "--seed") {
      ++i;
      if (i < argc) {
        if (ParseInt(argv[i], &context.seed)) {
          flag_err = true;
        }
      } else {
        flag_err = true;
      }
    } else if (std::string(argv[i]) == "--export-obj") {
      flag_export_obj = true;
    } else if (std::string(argv[i]) == "--export-dont-merge-vertices") {
      flag_export_dont_merge_vertices = true;
    } else if (std::string(argv[i]) == "--export-dir") {
      ++i;
      if (i < argc && export_dir.empty()) {
        export_dir = argv[i];
      } else {
        flag_err = true;
      }
    } else if (std::string(argv[i]) == "--export-suffix") {
      ++i;
      if (i < argc && export_suffix.empty()) {
        export_suffix = argv[i];
      } else {
        flag_err = true;
      }
    } else if (std::string(argv[i]) == "--help") {
      flag_help = true;
    } else if (std::string(argv[i]) == "--only-lexer") {
      flag_only_lexer = true;
    } else if (std::string(argv[i]) == "--only-parser") {
      flag_only_parser = true;
    } else if (std::string(argv[i]) == "--no-gui") {
      flag_no_gui = true;
    } else if (std::string(argv[i]) == "--parameter") {
      ++i;
      if (i < argc) {
        if (ParseParameter(argv[i], &parameters_argv)) {
          flag_err = true;
        }
      } else {
        flag_err = true;
      }
    } else if (std::string(argv[i]) == "--screenshot") {
      flag_screenshot = true;
    } else if (std::string(argv[i]) == "--print-grammar") {
      flag_print_grammar = true;
    } else if (std::string(argv[i]) == "--print-shape-tree") {
      flag_print_shape_tree = true;
    } else if (std::string(argv[i]) == "--window-height") {
      ++i;
      if (i < argc) {
        if (std::string(argv[i]) == "480") {
          window_height = 480;
        } else if (std::string(argv[i]) == "600") {
          window_height = 600;
        } else if (std::string(argv[i]) == "720") {
          window_height = 720;
        } else if (std::string(argv[i]) == "768") {
          window_height = 768;
        } else if (std::string(argv[i]) == "960") {
          window_height = 960;
        } else if (std::string(argv[i]) == "1080") {
          window_height = 1080;
        } else if (std::string(argv[i]) == "1200") {
          window_height = 1200;
        } else {
          flag_err = true;
        }
        window_width = window_height / 3 * 4;
      } else {
        flag_err = true;
      }
    } else if (std::string(argv[i]) == "--camera") {
      for (int j = 0; j < 8; ++j) {
        ++i;
        if (i < argc) {
          if (ParseFloat(argv[i], &camera_params[j])) {
            flag_err = true;
            break;
          }
        } else {
          flag_err = true;
          break;
        }
      }
      flag_has_camera_params = true;
    } else if (std::string(argv[i]) == "--show-ground-plane") {
      gui_settings.show_ground_plane = true;
    } else if (std::string(argv[i]) == "--ground-plane-elevation") {
      ++i;
      if (i < argc) {
        if (ParseFloat(argv[i], &gui_settings.ground_plane_elevation)) {
          flag_err = true;
        }
      } else {
        flag_err = true;
      }
    } else if (std::string(argv[i]) == "--ground-plane-color") {
      for (int j = 0; j < 3; ++j) {
        ++i;
        if (i < argc) {
          if (ParseFloat(argv[i], &gui_settings.ground_plane_color[j])) {
            flag_err = true;
            break;
          }
        } else {
          flag_err = true;
          break;
        }
      }
    } else {
      if (context.file_name.empty()) {
        context.file_name = argv[i];
      } else {
        flag_err = true;
      }
    }
    ++i;
  }

  if (flag_help) {
    std::cout << HELP_MESSAGE << '\n';
    return 0;
  }

  if (context.file_name.empty()) {
    flag_err = true;
  }
  if (context.axiom_name.empty()) {
    context.axiom_name = "Axiom";
  }
  if (flag_err) {
    std::cerr << ERROR_MSG << '\n';
    return 64;
  }

  if (flag_only_parser) {
    flag_no_gui = true;
  }

  // Only lexing.
  if (flag_only_lexer) {
    shapeml::parser::Lexer lexer;
    if (lexer.Init(context.file_name)) {
      std::cerr << "ERROR: Initialization of lexer failed.\n";
      return 1;
    }

    shapeml::parser::TokenVec tokens;
    if (lexer.Tokenize(&tokens)) {
      std::cerr << "ERROR: Lexing failed.\n";
      return 1;
    }

    for (size_t i = 0; i < tokens.size(); ++i) {
      std::cout << tokens[i] << '\n';
    }

    return 0;
  }

  // Parsing
  shapeml::parser::Parser parser;
  if (parser.Parse(context.file_name, &context.grammar)) {
    std::cerr << "ERROR: Parsing failed.\n";
    return 1;
  }

  if (flag_print_grammar && flag_no_gui) {
    std::cout << context.grammar << '\n';
  }

  if (flag_only_parser) {
    return 0;
  }

  // Passed command line parameters are checked for validity and the remaining
  // ones are filled in with the default values from the grammar.
  context.parameters = context.grammar.parameters();
  for (const auto& key_val : parameters_argv) {
    auto it = context.parameters.find(key_val.first);
    if (it != context.parameters.end()) {
      it->second = key_val.second;
    } else {
      std::cerr << "WARNING: The parameter '" << key_val.first << "' does not ";
      std::cerr << "exist in the grammar and will be ignored.\n";
    }
  }

  // Fix the export directory if necessary.
  if (!export_dir.empty()) {
#ifdef _WIN32
    if (export_dir.back() != '\\' && export_dir.back() != '/') {
      export_dir += '\\';
    }
#else
    if (export_dir.back() != '/') {
      export_dir += '/';
    }
#endif
  }

  // Cut of the base from the export file name.
  std::string export_name = export_dir + context.grammar.file_name();

  // Change extension of the export file name (if possible).
  if (export_name.substr(export_name.length() - 4) == ".shp") {
    export_name = export_name.substr(0, export_name.length() - 4);
  }
  export_name += export_suffix;

  if (flag_no_gui) {
    context.root = Derive();
    if (!context.root) {
      return 1;
    }

    if (flag_print_shape_tree) {
      context.root->PrintToStreamRecursively(&std::cout);
    }

    if (flag_export_obj) {
      shapeml::Exporter(context.root, shapeml::ExportType::OBJ, export_name,
                        context.grammar.base_path(),
                        !flag_export_dont_merge_vertices);
    }

    if (flag_screenshot) {
      if (shapeml_viewer.Init("ShapeMaker", window_width, window_height, 1)) {
        return 1;
      }
      viewer::gl::Renderer* renderer = CreateRenderer(context.root);
      renderer->FinalizeAllSteps();
      if (flag_has_camera_params) {
        shapeml_viewer.SetCameraParameters(camera_params);
        shapeml_viewer.set_auto_reset_camera(false);
      }
      shapeml_viewer.UpdateRenderer(renderer);

      // Render 2 frames before grabbing a screenshot from the back buffer.
      // Alternatively we could render only one and grab the frot buffer by
      // calling glReadBuffer(GL_FRONT) before glReadPixels(...).
      shapeml_viewer.RenderSingleFrame();
      shapeml_viewer.RenderSingleFrame();
      shapeml_viewer.SaveScreenshot(export_name);
      shapeml_viewer.Destroy();
    }

    delete context.root;
  } else {
    // Open the interactive GUI application.
    assert(!flag_no_gui);

    gui_settings.grammar_file = context.grammar.file_name();
    gui_settings.export_name = export_name;
    gui_settings.export_merge_vertices = !flag_export_dont_merge_vertices;
    gui_settings.new_seed = context.seed;
    gui_settings.new_axiom_name = context.axiom_name;
    gui_settings.new_parameters = context.parameters;
    FilterColorParameters();

    if (shapeml_viewer.Init("ShapeMaker", window_width, window_height, 1)) {
      return 1;
    }
    if (flag_has_camera_params) {
      shapeml_viewer.SetCameraParameters(camera_params);
      shapeml_viewer.set_auto_reset_camera(false);
      gui_settings.auto_reset_camera = false;
    }

    derivation_thread_data.is_deriving = true;  // Start 1st derivation.
    std::thread derivation_thread(DerivationThread);
    shapeml_viewer.MainLoop(RenderGui);
    {
      std::lock_guard<std::mutex> lock(derivation_thread_data.derivation_mutex);
      derivation_thread_data.finished = true;
      derivation_thread_data.cond_var.notify_one();
    }
    derivation_thread.join();

    shapeml_viewer.Destroy();

    if (context.root) {
      delete context.root;
    }
  }
  return 0;
}

// Filter out the color parameters defined as hex strings.
void FilterColorParameters() {
  gui_settings.color_param_map.clear();
  for (auto& key_val : gui_settings.new_parameters) {
    const std::string& value = *key_val.second.s;
    if (key_val.second.type == shapeml::ValueType::STRING &&
        !shapeml::util::ValidateHexColor(value, false)) {
      const shapeml::Vec4 color = shapeml::util::ParseHexColor(value);
      const shapeml::Vec3f color_f(static_cast<float>(color.x()),
                                   static_cast<float>(color.y()),
                                   static_cast<float>(color.z()));
      gui_settings.color_param_map.emplace(key_val.first, color_f);
    }
  }
}

shapeml::Shape* Derive() {
  shapeml::Interpreter& interpreter = shapeml::Interpreter::Get();
  shapeml::geometry::Octree* octree = new shapeml::geometry::Octree(
      shapeml::Vec3::Zero(), context.octree_extent);
  shapeml::Shape* shape =
      interpreter.Init(&context.grammar, context.axiom_name, context.seed,
                       &context.parameters, octree);

  if (shape) {
    if (!interpreter.Derive(shape, context.max_derivation_steps)) {
      delete shape;
      shape = nullptr;
    }
  }
  delete octree;
  return shape;
}

// For continuous derivation is separate thread.
void DerivationThread() {
  while (true) {
    std::unique_lock<std::mutex> lock(derivation_thread_data.derivation_mutex);
    derivation_thread_data.cond_var.wait(lock, [] {
      return (derivation_thread_data.is_deriving ||
              derivation_thread_data.finished);
    });
    if (derivation_thread_data.finished) {
      break;
    }
    lock.unlock();  // We don't need the mutex during derivation.

    // Run the derivation.
    shapeml::Shape* shape = Derive();

    // Create OpenGL buffers.
    if (shape) {
      viewer::gl::Renderer* renderer = CreateRenderer(shape);

      shapeml_viewer.UpdateRendererAsync(renderer);
      // TODO(stefalie): We probably want to remove this alternative (multistep)
      // procedure to update the renderer. Since only the render thread can use
      // the OpenGL context, creation and mapping of the buffers needs be done
      // there. Next the filling of the buffers can happen in the derivation
      // thread here, and finally control is handled again to the rendering
      // thread to unmap the buffers. Copying the data is in most cases not a
      // bottleneck and the one step procedure is a lot easier to understand.
      // shapeml_viewer.UpdateRendererAsyncInSteps(renderer);

      // Exchange the resulting 3D shapes/model.
      std::lock_guard<std::mutex> lck_model(derivation_thread_data.model_mutex);
      if (context.root) {
        delete context.root;
      }
      context.root = shape;
    } else {
      std::cerr << "ERROR: Derivation failed. Keeping previous model ";
      std::cerr << "(if any).\n";
    }

    lock.lock();  // Get the lock again to signal that the derivation is done.
    derivation_thread_data.is_deriving = false;
  }
}

struct MatCmp {
  bool operator()(const viewer::gl::Material& lhs,
                  const viewer::gl::Material& rhs) const {
    return (lhs.color[0] == rhs.color[0] && lhs.color[1] == rhs.color[1] &&
            lhs.color[2] == rhs.color[2] && lhs.color[3] == rhs.color[3] &&
            lhs.metallic == rhs.metallic && lhs.roughness == rhs.roughness &&
            lhs.reflectance == rhs.reflectance &&
            lhs.texture_id == rhs.texture_id);
  }
};

// https://stackoverflow.com/questions/2590677/how-do-i-combine-hash-values-in-c0x
struct MatHash {
  size_t operator()(const viewer::gl::Material& material) const {
    std::hash<float> hasher_float;
    std::hash<uint32_t> hasher_uint;
    size_t h = hasher_float(material.color[0]);
    h ^= hasher_float(material.color[1]) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= hasher_float(material.color[2]) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= hasher_float(material.color[3]) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= hasher_float(material.metallic) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= hasher_float(material.roughness) + 0x9e3779b9 + (h << 6) + (h >> 2);
    h ^= hasher_uint(material.texture_id) + 0x9e3779b9 + (h << 6) + (h >> 2);
    return h;
  }
};

struct RendererCreationContext {
  viewer::gl::Renderer* renderer;
  std::unordered_map<const shapeml::geometry::HalfedgeMesh*, int32_t> meshes;
  std::unordered_map<viewer::gl::Material, int32_t, MatHash, MatCmp> materials;
};

void CreateRenderableFromShape(const shapeml::Shape* shape,
                               RendererCreationContext* context) {
  if (!shape->terminal() || !shape->visible() || !shape->mesh()) {
    return;
  }
  assert(!shape->mesh()->Empty());

  // Find or add mesh.
  int32_t mesh_id;
  auto it_mesh = context->meshes.find(shape->mesh().get());
  if (it_mesh == context->meshes.end()) {
    std::vector<viewer::gl::RenderVertex> vertex_data;
    std::vector<uint32_t> index_data;

    using ShapeMLRenderVertex = shapeml::geometry::RenderVertex;
    shape->mesh()->FillRenderBuffer(
        reinterpret_cast<std::vector<ShapeMLRenderVertex>*>(&vertex_data),
        &index_data);
    mesh_id = context->renderer->AddMesh(vertex_data, index_data);
    context->meshes.emplace(shape->mesh().get(), mesh_id);
    if (mesh_id < 0) {
      std::cerr << "ERROR: Can't add any more meshes to the renderer, .\n";
      std::cerr << "skipping renderable.\n";
    }
  } else {
    mesh_id = it_mesh->second;
  }

  if (mesh_id < 0) {
    return;
  }

  // Find or add material.
  const shapeml::Material& shapeml_mat = shape->material();
  viewer::gl::Material material = {};
  material.color = shapeml_mat.color.cast<float>();
  material.metallic = static_cast<float>(shapeml_mat.metallic);
  material.roughness = static_cast<float>(shapeml_mat.roughness);
  material.reflectance = static_cast<float>(shapeml_mat.reflectance);

  viewer::gl::TextureManager& tex_man = viewer::gl::TextureManager::Get();
  material.texture_id = tex_man.GreateOrGetTextureID(shapeml_mat.texture);

  int32_t material_id;
  auto it_material = context->materials.find(material);
  if (it_material == context->materials.end()) {
    material_id = context->renderer->AddMaterial(material);
    context->materials.emplace(material, material_id);
    if (material_id < 0) {
      std::cerr << "ERROR: Can't add any more materials to the renderer, ";
      std::cerr << "skipping renderable.\n";
    }
  } else {
    material_id = it_material->second;
  }

  if (material_id < 0) {
    return;
  }

  // Retrieve transformation as isometry plus a scaling factor.
  shapeml::Isometry3 trans;
  shapeml::Vec3 scale_double;
  shape->mesh()->GetScaledUnitTrafo(shape->Size(), &trans, &scale_double);
  const shapeml::Mat4f trafo =
      (shape->WorldTrafo() * trans).matrix().cast<float>();

  // Finally create the renderable.
  viewer::gl::Renderable renderable;
  renderable.transformation_iso = trafo;
  renderable.scale = scale_double.cast<float>();
  renderable.mesh_id = mesh_id;
  renderable.material_id = material_id;

  context->renderer->AddRenderable(renderable);
}

int32_t CreateGroundPlaneRenderable(viewer::gl::Renderer* renderer) {
  const float scale = 1000.0f;
  const std::vector<viewer::gl::RenderVertex> vertices = {
      {{-1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 0.0f}},
      {{1.0f, 0.0f, 1.0f}, {0.0f, 1.0f, 1.0f}, {1.0f, 0.0f}},
      {{1.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 1.0f}, {1.0f, 1.0f}},
      {{-1.0f, 0.0f, -1.0f}, {0.0f, 1.0f, 1.0f}, {0.0f, 1.0f}}};
  const std::vector<uint32_t> indices = {0, 1, 2, 0, 2, 3};

  const int32_t mesh_id = renderer->AddMesh(vertices, indices);

  viewer::gl::Material material;
  material.color = gui_settings.ground_plane_color;
  material.metallic = 0.0f;
  material.roughness = 1.0f;
  material.reflectance = 0.5f;
  viewer::gl::TextureManager& tex_man = viewer::gl::TextureManager::Get();
  material.texture_id = tex_man.GreateOrGetTextureID("");
  const int32_t material_id = renderer->AddMaterial(material);
  assert(material_id == gui_settings.ground_plane_id);

  viewer::gl::Renderable renderable;
  renderable.transformation_iso = Eigen::Matrix4f::Identity();
  renderable.transformation_iso(1, 3) = gui_settings.ground_plane_elevation;
  renderable.scale = {scale, 0.0f, scale};
  renderable.active = gui_settings.show_ground_plane;
  renderable.mesh_id = mesh_id;
  renderable.material_id = material_id;

  return renderer->AddRenderable(renderable);
}

viewer::gl::Renderer* CreateRenderer(const shapeml::Shape* shape) {
  viewer::gl::Renderer* renderer = new viewer::gl::Renderer;
  RendererCreationContext renderer_context;
  renderer_context.renderer = renderer;
  // TODO(stefalie): This is a bit delicate. This will make things crash if
  // TextureManager::Get() has not been called earlier on from the main/
  // rendering thread.
  viewer::gl::TextureManager::Get().set_base_path(context.grammar.base_path());

  const int32_t ground_plane_id = CreateGroundPlaneRenderable(renderer);
  assert(ground_plane_id == gui_settings.ground_plane_id);
  (void)ground_plane_id;

  // Create a renderable for each shape tree leave.
  shapeml::LeafConstVisitor leaf_visitor;
  shape->AcceptVisitor(&leaf_visitor);
  for (const shapeml::Shape* s : leaf_visitor.leaves()) {
    CreateRenderableFromShape(s, &renderer_context);
  }

  // Bounding box for the renderer
  shapeml::AABBVisitor aabb_visitor;
  shape->AcceptVisitor(&aabb_visitor);
  const shapeml::geometry::AABB shapeml_aabb = aabb_visitor.aabb();
  viewer::gl::AABB renderer_aabb;
  shapeml::Vec3f tmp =
      (shapeml_aabb.center - shapeml_aabb.extent).cast<float>();
  renderer_aabb.min = {tmp.x(), tmp.y(), tmp.z()};
  tmp = (shapeml_aabb.center + shapeml_aabb.extent).cast<float>();
  renderer_aabb.max = {tmp.x(), tmp.y(), tmp.z()};
  renderer->set_aabb(renderer_aabb);

  // Note that we don't add the ground plane to the AABB, this entails that the
  // part of the ground plane outside the AABB can't cast shadows. It can
  // receive shadow though. This is only valid for the non-stabilized shadow
  // mapping mode.
  return renderer;
}

void RenderGui() {
  // Get a local copy of 'is_deriving' so that we don't always have to aquire a
  // lock.
  bool is_deriving_local;
  {
    std::lock_guard<std::mutex> lock(derivation_thread_data.derivation_mutex);
    is_deriving_local = derivation_thread_data.is_deriving;
  }

  // There are several GUI fields that access the 'context.root' or the final 3D
  // model (e.g., the export button). We just get a global lock on these things
  // here so that we don't have to worry each time it's used.
  std::lock_guard<std::mutex> lock_model(derivation_thread_data.model_mutex);
  bool start_derivation = false;

  ImGui::Begin("ShapeML Settings");

  if (ImGui::CollapsingHeader("General", ImGuiTreeNodeFlags_DefaultOpen)) {
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    ImGui::InputText(
        "Grammar file", const_cast<char*>(gui_settings.grammar_file.c_str()),
        gui_settings.grammar_file.length() + 1, ImGuiInputTextFlags_ReadOnly);
    ImGui::PopStyleVar();

    if (ImGui::InputText("Axiom", &gui_settings.new_axiom_name)) {
      gui_settings.parameters_changed = true;
    }

    if (ImGui::InputInt("Seed", &gui_settings.new_seed)) {
      gui_settings.parameters_changed = true;
    }

    if (is_deriving_local) {
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }
    if (ImGui::Button("Reload grammar") && !is_deriving_local) {
      shapeml::parser::Parser parser;
      shapeml::Grammar new_grammar;
      if (parser.Parse(context.file_name, &new_grammar)) {
        std::cerr << "ERROR: Parsing failed. Keeping old version of grammar.\n";
        delete context.root;
        context.root = nullptr;
      } else {
        std::swap(context.grammar, new_grammar);
        gui_settings.new_parameters = context.grammar.parameters();
        gui_settings.parameters_changed = true;
        FilterColorParameters();
        start_derivation = true;
      }
    }
    if (is_deriving_local) {
      ImGui::PopStyleVar();
    }
    ImGui::SameLine();
    if (ImGui::Button("Parameters > stdout")) {
      bool first = true;
      for (const std::string& param : context.grammar.parameter_ordering()) {
        auto it = gui_settings.new_parameters.find(param);
        assert(it != gui_settings.new_parameters.end());

        if (first) {
          first = false;
        } else {
          std::cout << ' ';
        }
        std::cout << "--parameter " << it->first << '=' << it->second;
      }
      std::cout << '\n';
    }

    if (ImGui::Checkbox("Auto reset camera", &gui_settings.auto_reset_camera)) {
      shapeml_viewer.set_auto_reset_camera(gui_settings.auto_reset_camera);
    }

    if (ImGui::Button("Grammar > stdout")) {
      std::cout << context.grammar << '\n';
    }
    ImGui::SameLine();
    if (!context.root) {
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }
    if (ImGui::Button("Shape tree > stdout")) {
      if (context.root) {
        context.root->PrintToStreamRecursively(&std::cout);
      }
    }
    if (!context.root) {
      ImGui::PopStyleVar();
    }

    ImGui::Separator();

    if (gui_settings.auto_derive || is_deriving_local) {
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }
    if (ImGui::Button("Derive") && !gui_settings.auto_derive &&
        !is_deriving_local) {
      start_derivation = true;
    }
    if (gui_settings.auto_derive || is_deriving_local) {
      ImGui::PopStyleVar();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Auto derive", &gui_settings.auto_derive);

    if (is_deriving_local) {
      ImGui::PushStyleColor(
          ImGuiCol_Text,
          ImVec4(46.0f / 255.0f, 208.0f / 255.0f, 25.0f / 255.0f, 1.0f));
      const char* dots[] = {"", ".", "..", "..."};
      ImGui::Text("Derivation thread is busy %s",
                  dots[static_cast<int>(ImGui::GetTime() / 0.3f) & 3]);
      ImGui::PopStyleColor();
    } else {
      ImGui::Text("Derivation thread is idle");
    }

    ImGui::Separator();

    ImGui::InputText("Export path", &gui_settings.export_name);

    if (!context.root) {
      ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);
    }
    if (ImGui::Button("Save screenshot")) {
      if (context.root) {
        if (shapeml_viewer.SaveScreenshot(gui_settings.export_name)) {
          std::cerr << "ERROR: Failed to save screenshot '";
          std::cerr << gui_settings.export_name << ".png'.\n";
        }
      }
    }

    if (ImGui::Button("Export OBJ")) {
      if (context.root) {
        shapeml::Exporter(context.root, shapeml::ExportType::OBJ,
                          gui_settings.export_name, context.grammar.base_path(),
                          !gui_settings.export_merge_vertices);
      }
    }
    if (!context.root) {
      ImGui::PopStyleVar();
    }
    ImGui::SameLine();
    ImGui::Checkbox("Merge vertices", &gui_settings.export_merge_vertices);

    ImGui::Separator();

    if (ImGui::Button("Render settings > stdout")) {
      if (gui_settings.show_ground_plane) {
        std::cout << "--show-ground-plane ";
      }
      std::cout << "--ground-plane-elevation ";
      std::cout << gui_settings.ground_plane_elevation << ' ';
      std::cout << "--ground-plane-color ";
      std::cout << gui_settings.ground_plane_color[0] << ' ';
      std::cout << gui_settings.ground_plane_color[1] << ' ';
      std::cout << gui_settings.ground_plane_color[2] << ' ';
      // TODO(stefalie): Print all other state too so that a specic state of the
      // viewer can be reloaded later on with all settings (including render
      // settings).
      std::cout << '\n';
    }

    if (ImGui::Checkbox("Show rendering settings",
                        &gui_settings.show_rendering_settings)) {
      shapeml_viewer.ToggleRenderSettings(gui_settings.show_rendering_settings);
    }

    // TODO(stefalie): Consider removing the ground plane completely as the sky/
    // background shader can render an infinite ground plane (which is better).
    {  // Ground plane
      viewer::gl::Renderer* renderer = shapeml_viewer.renderer();
      if (!renderer) {
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha,
                            ImGui::GetStyle().Alpha * 0.5f);
      }
      // TODO(stefalie): Use the following once externally exposed in ImGui:
      // ImGui::PushItemFlag(ImGuiItemFlags_Disabled, true);
      // ...
      // ImGui::PopItemFlag();
      ImGui::PushID(static_cast<const void*>("Geometric ground plane"));
      bool tmp_show_ground_plane = gui_settings.show_ground_plane;
      if (ImGui::Checkbox("Show ground plane", &tmp_show_ground_plane)) {
        if (renderer) {
          gui_settings.show_ground_plane = tmp_show_ground_plane;

          viewer::gl::Renderable* renderable =
              renderer->AccessRenderable(gui_settings.ground_plane_id);
          renderable->active = gui_settings.show_ground_plane;
        }
      }
      float tmp_ground_plane_elevation = gui_settings.ground_plane_elevation;
      if (ImGui::DragFloat("Ground plane elevation",
                           &tmp_ground_plane_elevation)) {
        if (renderer) {
          gui_settings.ground_plane_elevation = tmp_ground_plane_elevation;

          viewer::gl::Renderable* renderable =
              renderer->AccessRenderable(gui_settings.ground_plane_id);
          renderable->transformation_iso(1, 3) =
              gui_settings.ground_plane_elevation;
        }
      }
      Eigen::Vector4f tmp_ground_plane_color = gui_settings.ground_plane_color;
      if (ImGui::ColorEdit3("Ground plane color", tmp_ground_plane_color.data(),
                            ImGuiColorEditFlags_DisplayHex)) {
        if (renderer) {
          gui_settings.ground_plane_color = tmp_ground_plane_color;

          viewer::gl::Material* material =
              renderer->AccessMaterial(gui_settings.ground_plane_id);
          material->color = gui_settings.ground_plane_color;
        }
      }
      ImGui::PopID();
      if (!renderer) {
        ImGui::PopStyleVar();
      }
    }
  }

  if (ImGui::CollapsingHeader("Grammar Parameters",
                              ImGuiTreeNodeFlags_DefaultOpen)) {
    for (const std::string& param : context.grammar.parameter_ordering()) {
      auto it = gui_settings.new_parameters.find(param);
      assert(it != gui_settings.new_parameters.end());
      const char* name = it->first.c_str();
      shapeml::Value& value = it->second;

      switch (value.type) {
        case shapeml::ValueType::BOOL:
          if (ImGui::Checkbox(name, &value.b)) {
            gui_settings.parameters_changed = true;
          }
          break;
        case shapeml::ValueType::FLOAT:
          if (ImGui::InputDouble(name, &value.f)) {
            gui_settings.parameters_changed = true;
          }
          break;
        case shapeml::ValueType::INT:
          if (ImGui::InputInt(name, &value.i)) {
            gui_settings.parameters_changed = true;
          }
          break;
        case shapeml::ValueType::SHAPE_OP_STRING:
          assert(false);
          break;
        case shapeml::ValueType::STRING:
          auto it = gui_settings.color_param_map.find(name);
          if (it != gui_settings.color_param_map.end()) {
            if (ImGui::ColorEdit3(name, it->second.data())) {
              shapeml::util::HexColor2String(it->second, value.s);
              gui_settings.parameters_changed = true;
            }
          } else {
            if (ImGui::InputText(name, value.s)) {
              gui_settings.parameters_changed = true;
            }
          }
          break;
      }
    }
  }

  ImGui::End();

  if (start_derivation ||
      (gui_settings.auto_derive && gui_settings.parameters_changed &&
       !is_deriving_local)) {
    if (gui_settings.parameters_changed) {
      context.parameters = gui_settings.new_parameters;
      context.seed = gui_settings.new_seed;
      context.axiom_name = gui_settings.new_axiom_name;
      gui_settings.parameters_changed = false;
    }

    std::lock_guard<std::mutex> lock(derivation_thread_data.derivation_mutex);
    derivation_thread_data.is_deriving = true;
    derivation_thread_data.cond_var.notify_one();
  }
}

bool ParseParameter(const std::string& key_val, shapeml::ValueDict* params) {
  size_t assign_pos = key_val.find("=");
  if (assign_pos == std::string::npos) {
    return true;
  }

  const std::string key = key_val.substr(0, assign_pos);
  const std::string val = key_val.substr(assign_pos + 1);
  if (key.empty() || val.empty()) {
    return true;
  }

  // Check key for validity.
  if (!(isalpha(key[0]) || key[0] == '_')) {
    return true;
  }
  for (size_t i = 1; i < key.length(); ++i) {
    if (!(isalpha(key[i]) || isdigit(key[i]) || key[i] == '_')) {
      return true;
    }
  }

  // Check value for validity, and parse it.
  if (val == "true") {
    params->emplace(key, shapeml::Value(true));
    return false;
  } else if (val == "false") {
    params->emplace(key, shapeml::Value(false));
    return false;
  }

  // Now we check for int or float. If any contradictions occur, we simply
  // return a string.

  if (val[0] != '-' && !isdigit(val[0])) {
    // Return a string if it's definitely not a number.
    params->emplace(key, shapeml::Value(val));
    return false;
  }

  size_t i = 0;
  const bool is_negative = val[0] == '-';
  if (is_negative) {
    ++i;
  }

  int sum = 0;
  double sum_d = 0.0;
  while (i < val.size() && isdigit(val[i])) {
    // Note that we don't check for int overflow here.
    sum = 10 * sum + (val[i] - '0');
    sum_d = 10.0 * sum_d + static_cast<double>(val[i] - '0');
    ++i;
  }

  if (i == val.size()) {
    // It's an integer.
    params->emplace(key, shapeml::Value(is_negative ? -sum : sum));
    return false;
  }

  if (val[i] != '.') {
    params->emplace(key, shapeml::Value(val));
    return false;
  }

  // There was a decimal point, it might be a float
  ++i;
  if (i == val.size()) {
    params->emplace(key, shapeml::Value(val));
    return false;
  }

  double exp = 1.0;
  while (i < val.size() && isdigit(val[i])) {
    exp *= 10.0;
    sum_d = sum_d + static_cast<double>(val[i] - '0') / exp;
    ++i;
  }

  if (i == val.size()) {
    // It's a float.
    params->emplace(key, shapeml::Value(is_negative ? -sum_d : sum_d));
    return false;
  }

  params->emplace(key, shapeml::Value(val));
  return false;
}

bool ParseInt(const std::string& val, int* int_value) {
  *int_value = 0;
  for (size_t j = 0; j < val.size(); ++j) {
    if (val[j] < '0' || val[j] > '9') {
      return true;
    } else {
      *int_value = *int_value * 10 + (val[j] - '0');
    }
  }
  return false;
}

bool ParseFloat(const std::string& val, float* float_val) {
  size_t i = 0;
  const bool is_negative = val[0] == '-';
  if (is_negative) {
    ++i;
  }

  float sum = 0.0f;
  while (i < val.size() && isdigit(val[i])) {
    sum = 10.0f * sum + static_cast<float>(val[i] - '0');
    ++i;
  }

  if (i == val.size()) {
    *float_val = is_negative ? -sum : sum;
    return false;
  }

  if (val[i] != '.') {
    return true;
  }

  ++i;
  if (i == val.size()) {
    return true;
  }

  float exp = 1.0f;
  while (i < val.size() && isdigit(val[i])) {
    exp *= 10.0f;
    sum = sum + static_cast<float>(val[i] - '0') / exp;
    ++i;
  }

  if (i == val.size()) {
    *float_val = is_negative ? -sum : sum;
    return false;
  }

  return true;
}
