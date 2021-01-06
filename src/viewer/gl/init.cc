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

#include "viewer/gl/init.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#if defined(__linux__)
#include <dlfcn.h>
#elif defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#else
#error "OpenGL loading is not implemented for given platform, sorry."
#endif

#define GL_API_DEFINE_ENTRY(return_type, name, ...) name##Proc* name;
GL_API_LIST(GL_API_DEFINE_ENTRY)
#undef GL_API_DEFINE_ENTRY

namespace viewer {

namespace gl {

bool Init() {
#if defined(__linux__)
  void* lib_gl = dlopen("libGL.so", RTLD_LAZY);
  if (!lib_gl) {
    fprintf(stderr, "ERROR: Failed to load libGL.so.\n");
    return false;
  }

  typedef void glXExtFunc(void);
  typedef glXExtFunc* glXGetProcAddressProc(const GLubyte* procName);
  glXGetProcAddressProc* glXGetProcAddress =
      reinterpret_cast<glXGetProcAddressProc*>(
          dlsym(lib_gl, "glXGetProcAddress"));
  assert(glXGetProcAddress);

#define GL_API_GETPTR_ENTRY(return_type, name, ...)                      \
  name = (name##Proc*)glXGetProcAddress((const GLubyte*)#name);          \
  if (!name) {                                                           \
    name = (name##Proc*)dlsym(lib_gl, #name);                            \
  }                                                                      \
  if (!name) {                                                           \
    fprintf(stderr, "ERROR: Failed to load %s form libGL.so.\n", #name); \
    return false;                                                        \
  }
  GL_API_LIST(GL_API_GETPTR_ENTRY)
#undef GL_API_GETPTR_ENTRY
#elif defined(_WIN32)
  HMODULE dll = LoadLibraryA("opengl32.dll");
  if (!dll) {
    fprintf(stderr, "ERROR: Failed to load opengl32.dll.\n");
    return false;
  }

  typedef PROC WINAPI wglGetProcAddressProc(LPCSTR lpszProc);
  wglGetProcAddressProc* wglGetProcAddress =
      reinterpret_cast<wglGetProcAddressProc*>(
          GetProcAddress(dll, "wglGetProcAddress"));
  assert(wglGetProcAddress);

  // 'Old' OpenGL <= 1.1 functions must be retrieved with GetProcAddress, all
  // others with wglGetProcAddress. See:
  // https://www.khronos.org/opengl/wiki/Load_OpenGL_Functions#Windows
#define GL_API_GETPTR_ENTRY(return_type, name, ...)                          \
  name = (name##Proc*)wglGetProcAddress(#name);                              \
  if (!name) {                                                               \
    name = (name##Proc*)GetProcAddress(dll, #name);                          \
  }                                                                          \
  if (!name) {                                                               \
    fprintf(stderr, "ERROR: Failed to load %s form opengl32.dll.\n", #name); \
    return false;                                                            \
  }
  GL_API_LIST(GL_API_GETPTR_ENTRY)
#undef GL_API_GETPTR_ENTRY
#endif

  // Sanity check. (Should not be necessary in our use case because if the
  // OpenGL context exists, it should have been created for OpenGL 4.5.)
  GLint version_major, version_minor;
  glGetIntegerv(GL_MAJOR_VERSION, &version_major);
  glGetIntegerv(GL_MINOR_VERSION, &version_minor);
  if (version_major != 4 || version_minor != 5) {
    fprintf(stderr, "ERROR: We wanted OpenGL 4.5, but we got %i.%i instead.\n",
            version_major, version_minor);
    return false;
  }

  return true;
}

bool ExtensionSupported(const GLchar* extension) {
  int num_extensions;
  glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);
  for (int i = 0; i < num_extensions; ++i) {
    if (strcmp(extension, (const char*)glGetStringi(GL_EXTENSIONS, i)) == 0) {
      return true;
    }
  }
  return false;
}

void _CheckError(const char* file, int line) {
  GLenum error;
  // It seems the loop might cause an infinite loop on certain system, but
  // that's ok since it's only for debugging.
  // TODO(stefalie): If the error returned by glGetError doesn't change, break
  // out of the loop.
  while ((error = glGetError()) != GL_NO_ERROR) {
    const char* error_str;
    // clang-format off
    switch (error) {
#define CASE(error)       \
    case error:           \
      error_str = #error; \
      break
    CASE(GL_NO_ERROR);
    CASE(GL_INVALID_ENUM);
    CASE(GL_INVALID_VALUE);
    CASE(GL_INVALID_OPERATION);
    CASE(GL_INVALID_FRAMEBUFFER_OPERATION);
    CASE(GL_OUT_OF_MEMORY);
    CASE(GL_STACK_UNDERFLOW);
    CASE(GL_STACK_OVERFLOW);
#undef CASE
    default:
      error_str = "Unknown GL error number, should never happen.";
      break;
    }
    // clang-format on

    fprintf(stderr, "ERROR: file %s, line %i: %s.\n", file, line, error_str);
  }
}

void DebugMessageCallback(GLenum source, GLenum type, GLuint id,
                          GLenum severity, GLsizei /*length*/,
                          const GLchar* msg, const void* /*data*/) {
  const char* type_str;
  // clang-format off
  switch (type) {
#define CASE(type)    \
  case type:          \
    type_str = #type; \
    break
  CASE(GL_DEBUG_TYPE_ERROR);
  CASE(GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR);
  CASE(GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR);
  CASE(GL_DEBUG_TYPE_PORTABILITY);
  CASE(GL_DEBUG_TYPE_PERFORMANCE);
  CASE(GL_DEBUG_TYPE_OTHER);
  CASE(GL_DEBUG_TYPE_MARKER);
  CASE(GL_DEBUG_TYPE_PUSH_GROUP);
  CASE(GL_DEBUG_TYPE_POP_GROUP);
#undef CASE
  default:
    type_str = "Unknown GL_DEBUG_TYPE, should never happen.";
    break;
  }
  // clang-format on

  const char* severity_str;
  // clang-format off
  switch (severity) {
#define CASE(severity)        \
  case severity:              \
    severity_str = #severity; \
    break
  CASE(GL_DEBUG_SEVERITY_HIGH);
  CASE(GL_DEBUG_SEVERITY_MEDIUM);
  CASE(GL_DEBUG_SEVERITY_LOW);
  CASE(GL_DEBUG_SEVERITY_NOTIFICATION);
#undef CASE
  default:
    severity_str = "Unknown GL_DEBUG_SEVERITY, should never happen.";
    break;
  }
  // clang-format on

  const char* source_str;
  // clang-format off
  switch (source) {
#define CASE(source)      \
  case source:            \
    source_str = #source; \
    break
  CASE(GL_DEBUG_SOURCE_API);
  CASE(GL_DEBUG_SOURCE_WINDOW_SYSTEM);
  CASE(GL_DEBUG_SOURCE_SHADER_COMPILER);
  CASE(GL_DEBUG_SOURCE_THIRD_PARTY);
  CASE(GL_DEBUG_SOURCE_APPLICATION);
  CASE(GL_DEBUG_SOURCE_OTHER);
#undef CASE
  default:
    source_str = "Unknown GL_DEBUG_SEVERITY, should never happen.";
    break;
  }
  // clang-format on

  fprintf(stderr, "ERROR (OpenGL %u): %s of %s raised from %s:\n%s\n", id,
          type_str, severity_str, source_str, msg);
}

}  // namespace gl

}  // namespace viewer
