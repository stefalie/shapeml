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

#include "viewer/gl/shader.h"

#include <algorithm>
#include <cassert>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>

namespace viewer {

namespace gl {

Shader::Shader(const char* label)
    : vertex_shader_(0),
      tess_control_shader_(0),
      tess_evaluation_shader_(0),
      geometry_shader_(0),
      fragment_shader_(0),
      compute_shader_(0) {
  program_ = glCreateProgram();
  assert(program_);
  if (label) {
    glObjectLabel(GL_PROGRAM, program_, -1, label);
  }
}

Shader::~Shader() {
  if (vertex_shader_) {
    glDeleteShader(vertex_shader_);
  }
  if (tess_control_shader_) {
    glDeleteShader(tess_control_shader_);
  }
  if (tess_evaluation_shader_) {
    glDeleteShader(tess_evaluation_shader_);
  }
  if (geometry_shader_) {
    glDeleteShader(geometry_shader_);
  }
  if (fragment_shader_) {
    glDeleteShader(fragment_shader_);
  }
  if (compute_shader_) {
    glDeleteShader(compute_shader_);
  }

  glDeleteProgram(program_);
}

bool Shader::BuildShaderFromFile(GLenum shader_type,
                                 const std::string& file_name,
                                 const char* label) {
  std::string source;
  if (LoadFile(file_name, &source)) {
    std::cerr << "ERROR: Failed to load shader_sky file: " << file_name
              << ".\n";
    return true;
  }
  const char* c_str = source.c_str();
  if (BuildShaderFromSource(shader_type, 1, &c_str, label)) {
    std::cerr << "ERROR: Failed to compile shader_sky file: " << file_name
              << ".\n";
    return true;
  }
  return false;
}

static const char* ShaderTypeToString(GLenum shader_type) {
  const char* str = nullptr;
  switch (shader_type) {
#define CASE(shader_type) \
  case shader_type:       \
    str = #shader_type;   \
    break
    CASE(GL_VERTEX_SHADER);
    CASE(GL_TESS_CONTROL_SHADER);
    CASE(GL_TESS_EVALUATION_SHADER);
    CASE(GL_GEOMETRY_SHADER);
    CASE(GL_FRAGMENT_SHADER);
    CASE(GL_COMPUTE_SHADER);
#undef CASE
  }
  return str;
}

static std::string LabelString(GLenum identifier, GLuint name) {
  GLint max_label_length = 0;
  glGetIntegerv(GL_MAX_LABEL_LENGTH, &max_label_length);
  GLchar* label = new GLchar[max_label_length];
  GLsizei label_len = 0;
  glGetObjectLabel(identifier, name, max_label_length, &label_len, label);

  std::string ret = *label ? std::string(label) : std::to_string(name);
  delete[] label;
  return ret;
}

bool Shader::BuildShaderFromSource(GLenum shader_type, size_t num_sources,
                                   const char* const sources[],
                                   const char* label) {
  if (shader_type != GL_VERTEX_SHADER &&
      shader_type != GL_TESS_CONTROL_SHADER &&
      shader_type != GL_TESS_EVALUATION_SHADER &&
      shader_type != GL_GEOMETRY_SHADER && shader_type != GL_FRAGMENT_SHADER &&
      shader_type != GL_COMPUTE_SHADER) {
    return true;
  }

  for (size_t i = 0; i < num_sources; i++) {
    if (!sources[i] || strlen(sources[i]) == 0) {
      std::cerr << "ERROR in program '" << LabelString(GL_PROGRAM, program_);
      std::cerr << "': " << ShaderTypeToString(shader_type);
      std::cerr << " shader_sky source " << i << " is empty.\n";
      return true;
    }
  }

  GLuint handle = glCreateShader(shader_type);
  if (handle == 0) {
    std::cerr << "ERROR in program '" << LabelString(GL_PROGRAM, program_);
    std::cerr << "': " << ShaderTypeToString(shader_type);
    std::cerr << " shader_sky could not be created.\n";
    return true;
  }
  if (label) {
    glObjectLabel(GL_SHADER, handle, -1, label);
  }

  glShaderSource(handle, static_cast<GLsizei>(num_sources), sources, 0);
  glCompileShader(handle);

  GLint compile_success;
  glGetShaderiv(handle, GL_COMPILE_STATUS, &compile_success);
  if (compile_success == GL_FALSE) {
    GLint log_length;
    glGetShaderiv(handle, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length == 0) {
      std::cerr << "ERROR in program '" << LabelString(GL_PROGRAM, program_);
      std::cerr << "': " << ShaderTypeToString(shader_type) << " '";
      std::cerr << LabelString(GL_SHADER, handle) << "'";
      std::cerr << " compilation failed, no info log.\n";
    } else {
      GLchar* log = new GLchar[log_length];
      glGetShaderInfoLog(handle, log_length, nullptr, log);
      std::cerr << "ERROR in program '" << LabelString(GL_PROGRAM, program_);
      std::cerr << "': " << ShaderTypeToString(shader_type) << " '";
      std::cerr << LabelString(GL_SHADER, handle) << "'";
      std::cerr << " compilation failed:\n" << log << '\n';
      delete[] log;
    }
    return true;
  }

  switch (shader_type) {
    case GL_VERTEX_SHADER:
      vertex_shader_ = handle;
      break;
    case GL_TESS_CONTROL_SHADER:
      tess_control_shader_ = handle;
      break;
    case GL_TESS_EVALUATION_SHADER:
      tess_evaluation_shader_ = handle;
      break;
    case GL_GEOMETRY_SHADER:
      geometry_shader_ = handle;
      break;
    case GL_FRAGMENT_SHADER:
      fragment_shader_ = handle;
      break;
    case GL_COMPUTE_SHADER:
      compute_shader_ = handle;
      break;
    default:
      break;
  }

  return false;
}

bool Shader::BuildProgram() {
  if (compute_shader_ &&
      (vertex_shader_ || tess_control_shader_ || tess_evaluation_shader_ ||
       geometry_shader_ || fragment_shader_)) {
    std::cerr << "ERROR in program '" << LabelString(GL_PROGRAM, program_);
    std::cerr << "': A compute shader_sky cannot be combined with any other ";
    std::cerr << "shaders.\n";
    return true;
  }
  if (!compute_shader_ && (!vertex_shader_ || !fragment_shader_)) {
    std::cerr << "ERROR in program '" << LabelString(GL_PROGRAM, program_);
    std::cerr << "': At least vertex and fragment shaders are required ";
    std::cerr << "(unless it's a compute shader).\n";
    return true;
  }

  if (vertex_shader_) {
    glAttachShader(program_, vertex_shader_);
  }
  if (tess_control_shader_) {
    glAttachShader(program_, tess_control_shader_);
  }
  if (tess_evaluation_shader_) {
    glAttachShader(program_, tess_evaluation_shader_);
  }
  if (geometry_shader_) {
    glAttachShader(program_, geometry_shader_);
  }
  if (fragment_shader_) {
    glAttachShader(program_, fragment_shader_);
  }
  if (compute_shader_) {
    glAttachShader(program_, compute_shader_);
  }
  glLinkProgram(program_);

  GLint link_success;
  glGetProgramiv(program_, GL_LINK_STATUS, &link_success);
  if (link_success == GL_FALSE) {
    GLint log_length;
    glGetProgramiv(program_, GL_INFO_LOG_LENGTH, &log_length);
    if (log_length == 0) {
      std::cerr << "ERROR in program '" << LabelString(GL_PROGRAM, program_);
      std::cerr << "': Can't link, no info log.\n";
    } else {
      GLchar* log = new GLchar[log_length];
      glGetProgramInfoLog(program_, log_length, nullptr, log);
      std::cerr << "ERROR in program '" << LabelString(GL_PROGRAM, program_);
      std::cerr << "': Can't link:\n" << log << '\n';
      delete[] log;
    }
    return true;
  }

  const bool detach_shaders = false;
  if (detach_shaders) {
    if (vertex_shader_) {
      glDetachShader(program_, vertex_shader_);
    }
    if (tess_control_shader_) {
      glDetachShader(program_, tess_control_shader_);
    }
    if (tess_evaluation_shader_) {
      glDetachShader(program_, tess_evaluation_shader_);
    }
    if (geometry_shader_) {
      glDetachShader(program_, geometry_shader_);
    }
    if (fragment_shader_) {
      glDetachShader(program_, fragment_shader_);
    }
    if (compute_shader_) {
      glDetachShader(program_, compute_shader_);
    }
  }

  assert(glIsProgram(program_));
  return false;
}

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof(arr[0]))

// Used to name the types in program info.
#define GL_FLOAT 0x1406
#define GL_FLOAT_VEC2 0x8B50
#define GL_FLOAT_VEC3 0x8B51
#define GL_FLOAT_VEC4 0x8B52
#define GL_DOUBLE 0x140A
#define GL_DOUBLE_VEC2 0x8FFC
#define GL_DOUBLE_VEC3 0x8FFD
#define GL_DOUBLE_VEC4 0x8FFE
#define GL_INT 0x1404
#define GL_INT_VEC2 0x8B53
#define GL_INT_VEC3 0x8B54
#define GL_INT_VEC4 0x8B55
#define GL_UNSIGNED_INT 0x1405
#define GL_UNSIGNED_INT_VEC2 0x8DC6
#define GL_UNSIGNED_INT_VEC3 0x8DC7
#define GL_UNSIGNED_INT_VEC4 0x8DC8
#define GL_BOOL 0x8B56
#define GL_BOOL_VEC2 0x8B57
#define GL_BOOL_VEC3 0x8B58
#define GL_BOOL_VEC4 0x8B59
#define GL_FLOAT_MAT2 0x8B5A
#define GL_FLOAT_MAT3 0x8B5B
#define GL_FLOAT_MAT4 0x8B5C
#define GL_FLOAT_MAT2x3 0x8B65
#define GL_FLOAT_MAT2x4 0x8B66
#define GL_FLOAT_MAT3x2 0x8B67
#define GL_FLOAT_MAT3x4 0x8B68
#define GL_FLOAT_MAT4x2 0x8B69
#define GL_FLOAT_MAT4x3 0x8B6A
#define GL_DOUBLE_MAT2 0x8F46
#define GL_DOUBLE_MAT3 0x8F47
#define GL_DOUBLE_MAT4 0x8F48
#define GL_DOUBLE_MAT2x3 0x8F49
#define GL_DOUBLE_MAT2x4 0x8F4A
#define GL_DOUBLE_MAT3x2 0x8F4B
#define GL_DOUBLE_MAT3x4 0x8F4C
#define GL_DOUBLE_MAT4x2 0x8F4D
#define GL_DOUBLE_MAT4x3 0x8F4E
#define GL_SAMPLER_1D 0x8B5D
#define GL_SAMPLER_2D 0x8B5E
#define GL_SAMPLER_3D 0x8B5F
#define GL_SAMPLER_CUBE 0x8B60
#define GL_SAMPLER_1D_SHADOW 0x8B61
#define GL_SAMPLER_2D_SHADOW 0x8B62
#define GL_SAMPLER_1D_ARRAY 0x8DC0
#define GL_SAMPLER_2D_ARRAY 0x8DC1
#define GL_SAMPLER_1D_ARRAY_SHADOW 0x8DC3
#define GL_SAMPLER_2D_ARRAY_SHADOW 0x8DC4
#define GL_SAMPLER_2D_MULTISAMPLE 0x9108
#define GL_SAMPLER_2D_MULTISAMPLE_ARRAY 0x910B
#define GL_SAMPLER_CUBE_SHADOW 0x8DC5
#define GL_SAMPLER_BUFFER 0x8DC2
#define GL_SAMPLER_2D_RECT 0x8B63
#define GL_SAMPLER_2D_RECT_SHADOW 0x8B64
#define GL_INT_SAMPLER_1D 0x8DC9
#define GL_INT_SAMPLER_2D 0x8DCA
#define GL_INT_SAMPLER_3D 0x8DCB
#define GL_INT_SAMPLER_CUBE 0x8DCC
#define GL_INT_SAMPLER_1D_ARRAY 0x8DCE
#define GL_INT_SAMPLER_2D_ARRAY 0x8DCF
#define GL_INT_SAMPLER_2D_MULTISAMPLE 0x9109
#define GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY 0x910C
#define GL_INT_SAMPLER_BUFFER 0x8DD0
#define GL_INT_SAMPLER_2D_RECT 0x8DCD
#define GL_UNSIGNED_INT_SAMPLER_1D 0x8DD1
#define GL_UNSIGNED_INT_SAMPLER_2D 0x8DD2
#define GL_UNSIGNED_INT_SAMPLER_3D 0x8DD3
#define GL_UNSIGNED_INT_SAMPLER_CUBE 0x8DD4
#define GL_UNSIGNED_INT_SAMPLER_1D_ARRAY 0x8DD6
#define GL_UNSIGNED_INT_SAMPLER_2D_ARRAY 0x8DD7
#define GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE 0x910A
#define GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY 0x910D
#define GL_UNSIGNED_INT_SAMPLER_BUFFER 0x8DD8
#define GL_UNSIGNED_INT_SAMPLER_2D_RECT 0x8DD5

static const char* ShaderInternalTypeToString(GLenum type) {
  switch (type) {
    case GL_FLOAT:
      return "float";
    case GL_FLOAT_VEC2:
      return "vec2";
    case GL_FLOAT_VEC3:
      return "vec3";
    case GL_FLOAT_VEC4:
      return "vec4";
    case GL_DOUBLE:
      return "double";
    case GL_DOUBLE_VEC2:
      return "dvec2";
    case GL_DOUBLE_VEC3:
      return "dvec3";
    case GL_DOUBLE_VEC4:
      return "dvec4";
    case GL_INT:
      return "int";
    case GL_INT_VEC2:
      return "ivec2";
    case GL_INT_VEC3:
      return "ivec3";
    case GL_INT_VEC4:
      return "ivec4";
    case GL_UNSIGNED_INT:
      return "unsigned int";
    case GL_UNSIGNED_INT_VEC2:
      return "uvec2";
    case GL_UNSIGNED_INT_VEC3:
      return "uvec3";
    case GL_UNSIGNED_INT_VEC4:
      return "uvec4";
    case GL_BOOL:
      return "bool";
    case GL_BOOL_VEC2:
      return "bvec2";
    case GL_BOOL_VEC3:
      return "bvec3";
    case GL_BOOL_VEC4:
      return "bvec4";
    case GL_FLOAT_MAT2:
      return "mat2";
    case GL_FLOAT_MAT3:
      return "mat3";
    case GL_FLOAT_MAT4:
      return "mat4";
    case GL_FLOAT_MAT2x3:
      return "mat2x3";
    case GL_FLOAT_MAT2x4:
      return "mat2x4";
    case GL_FLOAT_MAT3x2:
      return "mat3x2";
    case GL_FLOAT_MAT3x4:
      return "mat3x4";
    case GL_FLOAT_MAT4x2:
      return "mat4x2";
    case GL_FLOAT_MAT4x3:
      return "mat4x3";
    case GL_DOUBLE_MAT2:
      return "dmat2";
    case GL_DOUBLE_MAT3:
      return "dmat3";
    case GL_DOUBLE_MAT4:
      return "dmat4";
    case GL_DOUBLE_MAT2x3:
      return "dmat2x3";
    case GL_DOUBLE_MAT2x4:
      return "dmat2x4";
    case GL_DOUBLE_MAT3x2:
      return "dmat3x2";
    case GL_DOUBLE_MAT3x4:
      return "dmat3x4";
    case GL_DOUBLE_MAT4x2:
      return "dmat4x2";
    case GL_DOUBLE_MAT4x3:
      return "dmat4x3";
    case GL_SAMPLER_1D:
      return "sampler1D";
    case GL_SAMPLER_2D:
      return "sampler2D";
    case GL_SAMPLER_3D:
      return "sampler3D";
    case GL_SAMPLER_CUBE:
      return "samplerCube";
    case GL_SAMPLER_1D_SHADOW:
      return "sampler1DShadow";
    case GL_SAMPLER_2D_SHADOW:
      return "sampler2DShadow";
    case GL_SAMPLER_1D_ARRAY:
      return "sampler1DArray";
    case GL_SAMPLER_2D_ARRAY:
      return "sampler2DArray";
    case GL_SAMPLER_1D_ARRAY_SHADOW:
      return "sampler1DArrayShadow";
    case GL_SAMPLER_2D_ARRAY_SHADOW:
      return "sampler2DArrayShadow";
    case GL_SAMPLER_2D_MULTISAMPLE:
      return "sampler2DMS";
    case GL_SAMPLER_2D_MULTISAMPLE_ARRAY:
      return "sampler2DMSArray";
    case GL_SAMPLER_CUBE_SHADOW:
      return "samplerCubeShadow";
    case GL_SAMPLER_BUFFER:
      return "samplerBuffer";
    case GL_SAMPLER_2D_RECT:
      return "sampler2DRect";
    case GL_SAMPLER_2D_RECT_SHADOW:
      return "sampler2DRectShadow";
    case GL_INT_SAMPLER_1D:
      return "isampler1D";
    case GL_INT_SAMPLER_2D:
      return "isampler2D";
    case GL_INT_SAMPLER_3D:
      return "isampler3D";
    case GL_INT_SAMPLER_CUBE:
      return "isamplerCube";
    case GL_INT_SAMPLER_1D_ARRAY:
      return "isampler1DArray";
    case GL_INT_SAMPLER_2D_ARRAY:
      return "isampler2DArray";
    case GL_INT_SAMPLER_2D_MULTISAMPLE:
      return "isampler2DMS";
    case GL_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
      return "isampler2DMSArray";
    case GL_INT_SAMPLER_BUFFER:
      return "isamplerBuffer";
    case GL_INT_SAMPLER_2D_RECT:
      return "isampler2DRect";
    case GL_UNSIGNED_INT_SAMPLER_1D:
      return "usampler1D";
    case GL_UNSIGNED_INT_SAMPLER_2D:
      return "usampler2D";
    case GL_UNSIGNED_INT_SAMPLER_3D:
      return "usampler3D";
    case GL_UNSIGNED_INT_SAMPLER_CUBE:
      return "usamplerCube";
    case GL_UNSIGNED_INT_SAMPLER_1D_ARRAY:
      return "usampler1DArray";
    case GL_UNSIGNED_INT_SAMPLER_2D_ARRAY:
      return "usampler2DArray";
    case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE:
      return "usampler2DMS";
    case GL_UNSIGNED_INT_SAMPLER_2D_MULTISAMPLE_ARRAY:
      return "usampler2DMSArray";
    case GL_UNSIGNED_INT_SAMPLER_BUFFER:
      return "usamplerBuffer";
    case GL_UNSIGNED_INT_SAMPLER_2D_RECT:
      return "usampler2DRect";
    default:
      return "unknown type";
  }
}

void Shader::ProgramInfo(std::string* info) const {
  std::ostringstream oss;

  std::string label = LabelString(GL_PROGRAM, program_);

  if (!glIsProgram(program_)) {
    oss << "ERROR: '" << label << "' is not a shader_sky program.\n";
    *info = oss.str();
    return;
  }

  GLint valid;
  glValidateProgram(program_);
  glGetProgramiv(program_, GL_VALIDATE_STATUS, &valid);
  if (!valid) {
    oss << "ERROR: Shader program '" << label << "' is not valid.\n";
    *info = oss.str();
    return;
  }

  GLint num_shaders;
  glGetProgramiv(program_, GL_ATTACHED_SHADERS, &num_shaders);
  oss << "Number of attached Shaders: " << num_shaders << '\n';

  {  // Get information about all non-block uniforms.
    oss << "Uniforms:\n";

    GLint num_uniforms;
    GLint max_name_length;
    glGetProgramInterfaceiv(program_, GL_UNIFORM, GL_ACTIVE_RESOURCES,
                            &num_uniforms);
    glGetProgramInterfaceiv(program_, GL_UNIFORM, GL_MAX_NAME_LENGTH,
                            &max_name_length);
    const GLenum props[] = {
        GL_BLOCK_INDEX,
        GL_LOCATION,
        GL_TYPE,
        GL_ARRAY_SIZE,
    };

    const GLint num_props = ARRAY_SIZE(props);
    GLchar* name = new GLchar[max_name_length];
    GLint* values = new GLint[num_props];
    for (int i = 0; i < num_uniforms; ++i) {
      glGetProgramResourceiv(program_, GL_UNIFORM, i, num_props, props,
                             num_props, nullptr, values);
      if (values[0] != -1) {  // Skip uniforms that are in a uniform block.
        continue;
      }

      glGetProgramResourceName(program_, GL_UNIFORM, i, max_name_length,
                               nullptr, name);
      oss << "  Uniform " << i << ": " << name;
      oss << " (location: " << values[1];
      oss << ", type: " << ShaderInternalTypeToString(values[2]);
      oss << ", array size: " << values[3] << ")\n";
    }
    delete[] values;
    delete[] name;
  }

  {  // Get information about all uniforms in blocks.
    oss << "Uniform blocks:\n";

    GLint num_uniform_blocks;
    GLint max_num_active_resources;
    GLint max_block_name_length;
    GLint max_uniform_name_length;
    glGetProgramInterfaceiv(program_, GL_UNIFORM_BLOCK, GL_ACTIVE_RESOURCES,
                            &num_uniform_blocks);
    glGetProgramInterfaceiv(program_, GL_UNIFORM_BLOCK,
                            GL_MAX_NUM_ACTIVE_VARIABLES,
                            &max_num_active_resources);
    glGetProgramInterfaceiv(program_, GL_UNIFORM_BLOCK, GL_MAX_NAME_LENGTH,
                            &max_block_name_length);
    glGetProgramInterfaceiv(program_, GL_UNIFORM, GL_MAX_NAME_LENGTH,
                            &max_uniform_name_length);
    const GLint max_name_length =
        std::max(max_block_name_length, max_uniform_name_length);

    const GLenum block_props[] = {
        GL_NUM_ACTIVE_VARIABLES,
        GL_BUFFER_BINDING,
        GL_BUFFER_DATA_SIZE,
    };
    const GLenum uniform_indices_prop[] = {GL_ACTIVE_VARIABLES};
    const GLenum uniform_props[] = {
        GL_TYPE,
        GL_ARRAY_SIZE,
        GL_OFFSET,
    };

    const GLint num_block_props = ARRAY_SIZE(block_props);
    const GLint num_uniform_props = ARRAY_SIZE(uniform_props);
    const GLint num_values = std::max(num_block_props, num_uniform_props);
    GLint* block_uniforms = new GLint[max_num_active_resources];
    GLchar* name = new GLchar[max_name_length];
    GLint* values = new GLint[num_values];

    for (int i = 0; i < num_uniform_blocks; ++i) {
      glGetProgramResourceiv(program_, GL_UNIFORM_BLOCK, i, num_block_props,
                             block_props, num_values, nullptr, values);
      glGetProgramResourceName(program_, GL_UNIFORM_BLOCK, i, max_name_length,
                               nullptr, name);

      oss << "  Uniform block " << i << ": " << name;
      oss << " (binding: " << values[1];
      oss << ", size: " << values[2] << ")\n";

      const GLint num_active_uniforms = values[0];
      glGetProgramResourceiv(program_, GL_UNIFORM_BLOCK, i, 1,
                             uniform_indices_prop, num_active_uniforms, nullptr,
                             block_uniforms);

      for (int j = 0; j < num_active_uniforms; ++j) {
        glGetProgramResourceName(program_, GL_UNIFORM, block_uniforms[j],
                                 max_name_length, nullptr, name);
        glGetProgramResourceiv(program_, GL_UNIFORM, block_uniforms[j],
                               num_uniform_props, uniform_props, num_values,
                               nullptr, values);

        oss << "    Uniform " << j << ": " << name;
        oss << " (type: " << ShaderInternalTypeToString(values[0]);
        oss << ", array size: " << values[1];
        oss << ", offset: " << values[2] << ")\n";
      }
    }

    delete[] values;
    delete[] name;
    delete[] block_uniforms;
  }

  {  // Get information about all input attributes.
    oss << "Input Attributes:\n";

    GLint num_attribs;
    GLint max_name_length;
    glGetProgramInterfaceiv(program_, GL_PROGRAM_INPUT, GL_ACTIVE_RESOURCES,
                            &num_attribs);
    glGetProgramInterfaceiv(program_, GL_PROGRAM_INPUT, GL_MAX_NAME_LENGTH,
                            &max_name_length);
    const GLenum props[] = {
        GL_LOCATION,
        GL_TYPE,
        GL_ARRAY_SIZE,
    };

    const GLint num_props = ARRAY_SIZE(props);
    GLchar* name = new GLchar[max_name_length];
    GLint* values = new GLint[num_props];
    for (int i = 0; i < num_attribs; ++i) {
      glGetProgramResourceiv(program_, GL_PROGRAM_INPUT, i, num_props, props,
                             num_props, nullptr, values);

      glGetProgramResourceName(program_, GL_PROGRAM_INPUT, i, max_name_length,
                               nullptr, name);
      oss << "  Attribute " << i << ": " << name;
      oss << " (location: " << values[0];
      oss << ", type: " << ShaderInternalTypeToString(values[1]);
      oss << ", array size: " << values[2] << ")\n";
    }
    delete[] values;
    delete[] name;
  }

  {  // Get information about all SSBOs.
    oss << "SSBOs:\n";

    GLint num_ssbos;
    GLint max_num_active_resources;
    GLint max_ssbo_name_length;
    GLint max_variable_name_length;
    glGetProgramInterfaceiv(program_, GL_SHADER_STORAGE_BLOCK,
                            GL_ACTIVE_RESOURCES, &num_ssbos);
    glGetProgramInterfaceiv(program_, GL_SHADER_STORAGE_BLOCK,
                            GL_MAX_NUM_ACTIVE_VARIABLES,
                            &max_num_active_resources);
    glGetProgramInterfaceiv(program_, GL_SHADER_STORAGE_BLOCK,
                            GL_MAX_NAME_LENGTH, &max_ssbo_name_length);
    glGetProgramInterfaceiv(program_, GL_BUFFER_VARIABLE, GL_MAX_NAME_LENGTH,
                            &max_variable_name_length);
    const GLint max_name_length =
        std::max(max_ssbo_name_length, max_variable_name_length);

    const GLenum ssbo_props[] = {
        GL_NUM_ACTIVE_VARIABLES,
        GL_BUFFER_BINDING,
        GL_BUFFER_DATA_SIZE,
    };

    const GLint num_ssbo_props = ARRAY_SIZE(ssbo_props);
    const GLint num_values = num_ssbo_props;
    GLint* ssbo_variables = new GLint[max_num_active_resources];
    GLchar* name = new GLchar[max_name_length];
    GLint* values = new GLint[num_ssbo_props];

    for (int i = 0; i < num_ssbos; ++i) {
      glGetProgramResourceiv(program_, GL_SHADER_STORAGE_BLOCK, i,
                             num_ssbo_props, ssbo_props, num_values, nullptr,
                             values);
      glGetProgramResourceName(program_, GL_SHADER_STORAGE_BLOCK, i,
                               max_name_length, nullptr, name);

      oss << "  SSBO " << i << ": " << name;
      oss << " (binding: " << values[1];
      oss << ", size: " << values[2] << ")\n";
    }

    delete[] values;
    delete[] name;
    delete[] ssbo_variables;
  }

  {  // Get information about all output/frag data.
    oss << "Input Attributes:\n";

    GLint num_attribs;
    GLint max_name_length;
    glGetProgramInterfaceiv(program_, GL_PROGRAM_OUTPUT, GL_ACTIVE_RESOURCES,
                            &num_attribs);
    glGetProgramInterfaceiv(program_, GL_PROGRAM_OUTPUT, GL_MAX_NAME_LENGTH,
                            &max_name_length);
    const GLenum props[] = {
        GL_LOCATION,
        GL_TYPE,
        GL_ARRAY_SIZE,
    };

    const GLint num_props = ARRAY_SIZE(props);
    GLchar* name = new GLchar[max_name_length];
    GLint* values = new GLint[num_props];
    for (int i = 0; i < num_attribs; ++i) {
      glGetProgramResourceiv(program_, GL_PROGRAM_OUTPUT, i, num_props, props,
                             num_props, nullptr, values);

      glGetProgramResourceName(program_, GL_PROGRAM_OUTPUT, i, max_name_length,
                               nullptr, name);
      oss << "  Output frag data " << i << ": " << name;
      oss << " (location: " << values[0];
      oss << ", type: " << ShaderInternalTypeToString(values[1]);
      oss << ", size: " << values[2] << ")\n";
    }
    delete[] values;
    delete[] name;
  }

  *info = oss.str();
}

void Shader::Bind() const { glUseProgram(program_); }

bool Shader::LoadFile(const std::string& file_name, std::string* source) {
  std::ifstream input(file_name);
  *source = std::string(std::istreambuf_iterator<char>(input),
                        std::istreambuf_iterator<char>());
  return false;
}

}  // namespace gl

}  // namespace viewer
