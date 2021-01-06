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

// Heavily inspired by:
// https://github.com/ApoorvaJ/Papaya/blob/master/src/libpapaya/gl_lite.h
//
// This OpenGL loader is much more leightweight than glew & Co. It only loads
// what you actually want and prevents usage of any other OpenGL functions.
// Downside: You have to manually list the funciontality that you want.

#pragma once

#include <cstdint>

#ifdef _WIN32
#define GL_DECL __stdcall
#else
#define GL_DECL
#endif

// The following defines and function signatures are all taken from:
// https://www.khronos.org/registry/OpenGL/api/GL/glcorearb.h

// clang-format off

// OpenGL defines and typedefs:
// GL_VERSION_1_0
typedef unsigned int GLenum;
typedef float GLfloat;
typedef int GLint;
typedef int GLsizei;
typedef unsigned int GLbitfield;
typedef unsigned int GLuint;
typedef unsigned char GLboolean;
typedef unsigned char GLubyte;
#define GL_DEPTH_BUFFER_BIT               0x00000100
#define GL_STENCIL_BUFFER_BIT             0x00000400
#define GL_COLOR_BUFFER_BIT               0x00004000
#define GL_FALSE                          0
#define GL_TRUE                           1
#define GL_POINTS                         0x0000
#define GL_LINES                          0x0001
#define GL_LINE_STRIP                     0x0003
#define GL_TRIANGLES                      0x0004
#define GL_TRIANGLE_STRIP                 0x0005
#define GL_NEVER                          0x0200
#define GL_LESS                           0x0201
#define GL_EQUAL                          0x0202
#define GL_LEQUAL                         0x0203
#define GL_GREATER                        0x0204
#define GL_NOTEQUAL                       0x0205
#define GL_GEQUAL                         0x0206
#define GL_ALWAYS                         0x0207
#define GL_ZERO                           0
#define GL_ONE                            1
#define GL_SRC_COLOR                      0x0300
#define GL_ONE_MINUS_SRC_COLOR            0x0301
#define GL_SRC_ALPHA                      0x0302
#define GL_ONE_MINUS_SRC_ALPHA            0x0303
#define GL_DST_ALPHA                      0x0304
#define GL_ONE_MINUS_DST_ALPHA            0x0305
#define GL_DST_COLOR                      0x0306
#define GL_ONE_MINUS_DST_COLOR            0x0307
#define GL_SRC_ALPHA_SATURATE             0x0308
#define GL_CULL_FACE                      0x0B44
#define GL_DEPTH_TEST                     0x0B71
#define GL_UNPACK_ALIGNMENT               0x0CF5
#define GL_PACK_ALIGNMENT                 0x0D05
#define GL_TEXTURE_2D                     0x0DE1
#define GL_TEXTURE_BORDER_COLOR           0x1004
#define GL_DONT_CARE                      0x1100
#define GL_UNSIGNED_BYTE                  0x1401
#define GL_UNSIGNED_SHORT                 0x1403
#define GL_INT                            0x1404
#define GL_UNSIGNED_INT                   0x1405
#define GL_FLOAT                          0x1406
#define GL_COLOR                          0x1800
#define GL_EXTENSIONS                     0x1F03
#define GL_DEPTH                          0x1801
#define GL_RED                            0x1903
#define GL_RGB                            0x1907
#define GL_RGBA                           0x1908
#define GL_NEAREST                        0x2600
#define GL_LINEAR                         0x2601
#define GL_LINEAR_MIPMAP_LINEAR           0x2703
#define GL_TEXTURE_MAG_FILTER             0x2800
#define GL_TEXTURE_MIN_FILTER             0x2801
#define GL_TEXTURE_WRAP_S                 0x2802
#define GL_TEXTURE_WRAP_T                 0x2803
#define GL_REPEAT                         0x2901
// GL_VERSION_1_1
#define GL_CLAMP_TO_EDGE                  0x812F
// GL_VERSION_1_3
#define GL_MULTISAMPLE                    0x809D
#define GL_CLAMP_TO_BORDER                0x812D
// GL_VERSION_1_4
#define GL_TEXTURE_COMPARE_MODE           0x884C
#define GL_TEXTURE_COMPARE_FUNC           0x884D
// GL_VERSION_1_5
// TODO(stefalie): Are the pointer types correct?
typedef signed long long int GLsizeiptr;
typedef signed long long int GLintptr;
#define GL_READ_ONLY                      0x88B8
#define GL_WRITE_ONLY                     0x88B9
#define GL_READ_WRITE                     0x88BA
// GL_VERSION_2_0
typedef char GLchar;
#define GL_FRAGMENT_SHADER                0x8B30
#define GL_VERTEX_SHADER                  0x8B31
#define GL_COMPILE_STATUS                 0x8B81
#define GL_LINK_STATUS                    0x8B82
#define GL_INFO_LOG_LENGTH                0x8B84
// GL_VERSION_3_0
#define GL_COMPARE_REF_TO_TEXTURE         0x884E
#define GL_MAJOR_VERSION                  0x821B
#define GL_MINOR_VERSION                  0x821C
#define GL_NUM_EXTENSIONS                 0x821D
#define GL_MAX_SAMPLES                    0x8D57
#define GL_MAP_READ_BIT                   0x0001
#define GL_MAP_WRITE_BIT                  0x0002
#define GL_MAP_INVALIDATE_RANGE_BIT       0x0004
#define GL_MAP_INVALIDATE_BUFFER_BIT      0x0008
#define GL_MAP_FLUSH_EXPLICIT_BIT         0x0010
#define GL_MAP_UNSYNCHRONIZED_BIT         0x0020
#define GL_FRAMEBUFFER_UNDEFINED          0x8219
#define GL_DEPTH_STENCIL_ATTACHMENT       0x821A
#define GL_FRAMEBUFFER_COMPLETE           0x8CD5
#define GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT 0x8CD6
#define GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT 0x8CD7
#define GL_FRAMEBUFFER_INCOMPLETE_DRAW_BUFFER 0x8CDB
#define GL_FRAMEBUFFER_INCOMPLETE_READ_BUFFER 0x8CDC
#define GL_TEXTURE_2D_ARRAY               0x8C1A
#define GL_FRAMEBUFFER_UNSUPPORTED        0x8CDD
#define GL_MAX_COLOR_ATTACHMENTS          0x8CDF
#define GL_COLOR_ATTACHMENT0              0x8CE0
#define GL_DEPTH_ATTACHMENT               0x8D00
#define GL_STENCIL_ATTACHMENT             0x8D20
#define GL_FRAMEBUFFER                    0x8D40
#define GL_RENDERBUFFER                   0x8D41
#define GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE 0x8D56
// GL_VERSION_3_1
#define GL_UNIFORM_BUFFER                 0x8A11
// GL_VERSION_3_2
typedef struct __GLsync *GLsync;
typedef uint64_t GLuint64;
#define GL_FRAMEBUFFER_INCOMPLETE_LAYER_TARGETS 0x8DA8
#define GL_GEOMETRY_SHADER                0x8DD9
#define GL_DEPTH_CLAMP                    0x864F
#define GL_SYNC_GPU_COMMANDS_COMPLETE     0x9117
#define GL_ALREADY_SIGNALED               0x911A
#define GL_TIMEOUT_EXPIRED                0x911B
#define GL_CONDITION_SATISFIED            0x911C
#define GL_WAIT_FAILED                    0x911D
// GL_VERSION_4_0
#define GL_DRAW_INDIRECT_BUFFER           0x8F3F
#define GL_TESS_EVALUATION_SHADER         0x8E87
#define GL_TESS_CONTROL_SHADER            0x8E88
// GL_VERSION_4_2
#define GL_BUFFER_UPDATE_BARRIER_BIT      0x00000200
// GL_VERSION_4_3
#define GL_SHADER_IMAGE_ACCESS_BARRIER_BIT 0x00000020
#define GL_COMPUTE_SHADER                 0x91B9
#define GL_SHADER_STORAGE_BUFFER          0x90D2
#define GL_SHADER_STORAGE_BARRIER_BIT     0x00002000

// Image formats, sorted:
// GL_VERSION_3_0 (except if marked separately)
// R
#define GL_R8                             0x8229
#define GL_R16                            0x822A
#define GL_R8I                            0x8231
#define GL_R16I                           0x8233
#define GL_R32I                           0x8235
#define GL_R8UI                           0x8232
#define GL_R16UI                          0x8234
#define GL_R32UI                          0x8236
#define GL_R16F                           0x822D
#define GL_R32F                           0x822E
// RG
#define GL_RG8                            0x822B
#define GL_RG16                           0x822C
#define GL_RG8I                           0x8237
#define GL_RG8UI                          0x8238
#define GL_RG16I                          0x8239
#define GL_RG16UI                         0x823A
#define GL_RG32I                          0x823B
#define GL_RG32UI                         0x823C
#define GL_RG16F                          0x822F
#define GL_RG32F                          0x8230
// RGB
#define GL_RGB8                           0x8051  // GL_VERSION_1_0
#define GL_RGB8I                          0x8D8F
#define GL_RGB16I                         0x8D89
#define GL_RGB32I                         0x8D83
#define GL_RGB8UI                         0x8D7D
#define GL_RGB16UI                        0x8D77
#define GL_RGB32UI                        0x8D71
#define GL_RGB16F                         0x881B
#define GL_RGB32F                         0x8815
// RGBA
#define GL_RGBA8                          0x8058  // GL_VERSION_1_0
#define GL_RGBA8I                         0x8D8E
#define GL_RGBA16I                        0x8D88
#define GL_RGBA32I                        0x8D82
#define GL_RGBA8UI                        0x8D7C
#define GL_RGBA16UI                       0x8D76
#define GL_RGBA32UI                       0x8D70
#define GL_RGBA16F                        0x881A
#define GL_RGBA32F                        0x8814
// SRGB
// GL_VERSION_2_1
#define GL_SRGB8                          0x8C41
#define GL_SRGB8_ALPHA8                   0x8C43
// Depth
// GL_VERSION_1_4
#define GL_DEPTH_COMPONENT16              0x81A5
#define GL_DEPTH_COMPONENT24              0x81A6
#define GL_DEPTH_COMPONENT32              0x81A7
// GL_VERSION_3_0
#define GL_DEPTH_COMPONENT32F             0x8CAC

// Defines for debugging:
// GL_VERSION_1_0
#define GL_NO_ERROR                       0
#define GL_INVALID_ENUM                   0x0500
#define GL_INVALID_VALUE                  0x0501
#define GL_INVALID_OPERATION              0x0502
#define GL_OUT_OF_MEMORY                  0x0505
#define GL_STACK_OVERFLOW                 0x0503
#define GL_STACK_UNDERFLOW                0x0504
// GL_VERSION_3_0
#define GL_INVALID_FRAMEBUFFER_OPERATION  0x0506
#define GL_DRAW_FRAMEBUFFER               0x8CA9  // HACK: for fixing blitting on Intel
// GL_VERSION_4_3
typedef void (GL_DECL *GLDEBUGPROC)(GLenum source, GLenum type, GLuint id, GLenum severity, GLsizei length, const GLchar *message, const void *userParam);
#define GL_DEBUG_OUTPUT_SYNCHRONOUS       0x8242
#define GL_DEBUG_SOURCE_API               0x8246
#define GL_DEBUG_SOURCE_WINDOW_SYSTEM     0x8247
#define GL_DEBUG_SOURCE_SHADER_COMPILER   0x8248
#define GL_DEBUG_SOURCE_THIRD_PARTY       0x8249
#define GL_DEBUG_SOURCE_APPLICATION       0x824A
#define GL_DEBUG_SOURCE_OTHER             0x824B
#define GL_DEBUG_TYPE_ERROR               0x824C
#define GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR 0x824D
#define GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR  0x824E
#define GL_DEBUG_TYPE_PORTABILITY         0x824F
#define GL_DEBUG_TYPE_PERFORMANCE         0x8250
#define GL_DEBUG_TYPE_OTHER               0x8251
#define GL_DEBUG_SEVERITY_HIGH            0x9146
#define GL_DEBUG_SEVERITY_MEDIUM          0x9147
#define GL_DEBUG_SEVERITY_LOW             0x9148
#define GL_DEBUG_SEVERITY_NOTIFICATION    0x826B
#define GL_DEBUG_TYPE_MARKER              0x8268
#define GL_DEBUG_TYPE_PUSH_GROUP          0x8269
#define GL_DEBUG_TYPE_POP_GROUP           0x826A
#define GL_DEBUG_OUTPUT                   0x92E0

// Defines for object labeling:
/* GL_VERSION_1_0 */
#define GL_TEXTURE                        0x1702
/* GL_VERSION_1_1 */
#define GL_VERTEX_ARRAY                   0x8074
/* GL_VERSION_3_0 */
// #define GL_FRAMEBUFFER                    0x8D40
// #define GL_RENDERBUFFER                   0x8D41
/* GL_VERSION_4_3 */
#define GL_BUFFER                         0x82E0
#define GL_SHADER                         0x82E1
#define GL_PROGRAM                        0x82E2
#define GL_QUERY                          0x82E3
#define GL_PROGRAM_PIPELINE               0x82E4
#define GL_SAMPLER                        0x82E6
#define GL_MAX_LABEL_LENGTH               0x82E8

// Defines for shader introspection:
// GL_VERSION_2_0
#define GL_MAX_VERTEX_ATTRIBS             0x8869
#define GL_VALIDATE_STATUS                0x8B83
#define GL_ATTACHED_SHADERS               0x8B85
// GL_VERSION_4_3
#define GL_UNIFORM                        0x92E1
#define GL_UNIFORM_BLOCK                  0x92E2
#define GL_PROGRAM_INPUT                  0x92E3
#define GL_PROGRAM_OUTPUT                 0x92E4
#define GL_BUFFER_VARIABLE                0x92E5
#define GL_SHADER_STORAGE_BLOCK           0x92E6
#define GL_ACTIVE_RESOURCES               0x92F5
#define GL_MAX_NAME_LENGTH                0x92F6
#define GL_MAX_NUM_ACTIVE_VARIABLES       0x92F7
#define GL_TYPE                           0x92FA
#define GL_ARRAY_SIZE                     0x92FB
#define GL_OFFSET                         0x92FC
#define GL_BLOCK_INDEX                    0x92FD
#define GL_BUFFER_BINDING                 0x9302
#define GL_BUFFER_DATA_SIZE               0x9303
#define GL_NUM_ACTIVE_VARIABLES           0x9304
#define GL_ACTIVE_VARIABLES               0x9305
#define GL_LOCATION                       0x930E
#define GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET 0x82D9
#define GL_MAX_VERTEX_ATTRIB_BINDINGS     0x82DA
// GL_VERSION_4_4
#define GL_MAX_VERTEX_ATTRIB_STRIDE       0x82E5

// Defines for extensions:
// TODO(stefalie): These anisotropic filtering extensions should become part of
// the core in 4.6, move them to the list above once your driver supports them.
#define GL_TEXTURE_MAX_ANISOTROPY_EXT     0x84FE
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF

// Extra defines and typedefs for ImGui:
// GL_VERSION_1_0
typedef void GLvoid;
// typedef unsigned int GLenum;
// typedef float GLfloat;
// typedef int GLint;
// typedef int GLsizei;
// typedef unsigned int GLuint;
// typedef unsigned char GLboolean;
// #define GL_FALSE                          0
// #define GL_TRUE                           1
// #define GL_TRIANGLES                      0x0004
// #define GL_SRC_ALPHA                      0x0302
// #define GL_ONE_MINUS_SRC_ALPHA            0x0303
#define GL_FRONT_AND_BACK                 0x0408
#define GL_POLYGON_MODE                   0x0B40
#define GL_CULL_FACE                      0x0B44
// #define GL_DEPTH_TEST                     0x0B71
#define GL_VIEWPORT                       0x0BA2
#define GL_BLEND                          0x0BE2
#define GL_SCISSOR_BOX                    0x0C10
#define GL_SCISSOR_TEST                   0x0C11
#define GL_UNPACK_ROW_LENGTH              0x0CF2
// #define GL_TEXTURE_2D                     0x0DE1
// #define GL_UNSIGNED_BYTE                  0x1401
// #define GL_UNSIGNED_SHORT                 0x1403
// #define GL_INT                            0x1404
// #define GL_UNSIGNED_INT                   0x1405
// #define GL_FLOAT                          0x1406
// #define GL_RGB                            0x1907
// #define GL_RGBA                           0x1908
#define GL_FILL                           0x1B02
// #define GL_LINEAR                         0x2601
// #define GL_TEXTURE_MAG_FILTER             0x2800
// #define GL_TEXTURE_MIN_FILTER             0x2801
// GL_VERSION_1_1
#define GL_TEXTURE_BINDING_2D             0x8069
// GL_VERSION_1_3
#define GL_TEXTURE0                       0x84C0
#define GL_ACTIVE_TEXTURE                 0x84E0
// GL_VERSION_1_4
#define GL_BLEND_DST_RGB                  0x80C8
#define GL_BLEND_SRC_RGB                  0x80C9
#define GL_BLEND_DST_ALPHA                0x80CA
#define GL_BLEND_SRC_ALPHA                0x80CB
#define GL_FUNC_ADD                       0x8006
#define GL_STREAM_DRAW                    0x88E0
// GL_VERSION_1_5
// typedef signed long long int GLsizeiptr;
// typedef signed long long int GLintptr;
#define GL_ARRAY_BUFFER                   0x8892
#define GL_ELEMENT_ARRAY_BUFFER           0x8893
#define GL_ARRAY_BUFFER_BINDING           0x8894
// GL_VERSION_2_0
// typedef char GLchar;
#define GL_BLEND_EQUATION_RGB             0x8009
#define GL_BLEND_EQUATION_ALPHA           0x883D
// #define GL_FRAGMENT_SHADER                0x8B30
// #define GL_VERTEX_SHADER                  0x8B31
#define GL_CURRENT_PROGRAM                0x8B8D
#define GL_VERTEX_ARRAY_BINDING           0x85B5
// GL_VERSION_3_3
#define GL_SAMPLER_BINDING                0x8919

// OpenGL procedures:
#define GL_API_LIST(ENTRY) \
    /* GL_VERSION_1_0 */ \
    ENTRY(void,           glDrawBuffer,                   GLenum buf) /* HACK: for fixing blitting on Intel */ \
    ENTRY(void,           glClear,                        GLbitfield mask) \
    ENTRY(void,           glClearColor,                   GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) \
    ENTRY(void,           glDepthMask,                    GLboolean flag) \
    ENTRY(void,           glDisable,                      GLenum cap) \
    ENTRY(void,           glEnable,                       GLenum cap) \
    ENTRY(void,           glBlendFunc,                    GLenum sfactor, GLenum dfactor) \
    ENTRY(void,           glDepthFunc,                    GLenum func) \
    ENTRY(void,           glPixelStorei,                  GLenum pname, GLint param) \
    ENTRY(void,           glReadPixels,                   GLint x, GLint y, GLsizei width, GLsizei height, GLenum format, GLenum type, void *pixels) \
    ENTRY(void,           glGetFloatv,                    GLenum pname, GLfloat *data) \
    ENTRY(void,           glGetIntegerv,                  GLenum pname, GLint *data) \
    ENTRY(void,           glViewport,                     GLint x, GLint y, GLsizei width, GLsizei height) \
    /* GL_VERSION_1_1 */ \
    ENTRY(void,           glDrawArrays,                   GLenum mode, GLint first, GLsizei count) \
    ENTRY(void,           glDeleteTextures,               GLsizei n, const GLuint *textures) \
    /* GL_VERSION_1_4 */ \
    ENTRY(void,           glDeleteBuffers,                GLsizei n, const GLuint *buffers) \
    /* GL_VERSION_1_5 */ \
    ENTRY(void,           glBindBuffer,                   GLenum target, GLuint buffer) \
    ENTRY(GLboolean,      glIsBuffer,                     GLuint buffer) \
    /* GL_VERSION_2_0 */ \
    ENTRY(void,           glAttachShader,                 GLuint program, GLuint shader) \
    ENTRY(void,           glCompileShader,                GLuint shader) \
    ENTRY(GLuint,         glCreateProgram,                void) \
    ENTRY(GLuint,         glCreateShader,                 GLenum type) \
    ENTRY(void,           glDeleteProgram,                GLuint program) \
    ENTRY(void,           glDeleteShader,                 GLuint shader) \
    ENTRY(void,           glDetachShader,                 GLuint program, GLuint shader) \
    ENTRY(void,           glGetActiveAttrib,              GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name) \
    ENTRY(void,           glGetActiveUniform,             GLuint program, GLuint index, GLsizei bufSize, GLsizei *length, GLint *size, GLenum *type, GLchar *name) \
    ENTRY(void,           glGetProgramiv,                 GLuint program, GLenum pname, GLint *params) \
    ENTRY(void,           glGetProgramInfoLog,            GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog) \
    ENTRY(void,           glGetShaderiv,                  GLuint shader, GLenum pname, GLint *params) \
    ENTRY(void,           glGetShaderInfoLog,             GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog) \
    ENTRY(GLboolean,      glIsProgram,                    GLuint program) \
    ENTRY(void,           glLinkProgram,                  GLuint program) \
    ENTRY(void,           glShaderSource,                 GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length) \
    ENTRY(void,           glUseProgram,                   GLuint program) \
    /* GL_VERSION_3_0 */ \
    ENTRY(void,           glBindBufferRange,              GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size) \
    ENTRY(void,           glBindBufferBase,               GLenum target, GLuint index, GLuint buffer) \
    ENTRY(const GLubyte*, glGetStringi,                   GLenum name, GLuint index) \
    ENTRY(void,           glDeleteRenderbuffers,          GLsizei n, const GLuint *renderbuffers) \
    ENTRY(void,           glBindFramebuffer,              GLenum target, GLuint framebuffer) \
    ENTRY(void,           glDeleteFramebuffers,           GLsizei n, const GLuint *framebuffers) \
    ENTRY(void,           glBindVertexArray,              GLuint array) \
    ENTRY(void,           glDeleteVertexArrays,           GLsizei n, const GLuint *arrays) \
    /* GL_VERSION_3_2 */ \
    ENTRY(GLsync,         glFenceSync,                    GLenum condition, GLbitfield flags) \
    ENTRY(void,           glDeleteSync,                   GLsync sync) \
    ENTRY(GLenum,         glClientWaitSync,               GLsync sync, GLbitfield flags, GLuint64 timeout) \
    /* GL_VERSION_3_3 */ \
    ENTRY(void,           glDeleteSamplers,               GLsizei count, const GLuint *samplers) \
    ENTRY(void,           glBindSampler,                  GLuint unit, GLuint sampler) \
    ENTRY(void,           glSamplerParameteri,            GLuint sampler, GLenum pname, GLint param) \
    ENTRY(void,           glSamplerParameterf,            GLuint sampler, GLenum pname, GLfloat param) \
    ENTRY(void,           glSamplerParameterfv,           GLuint sampler, GLenum pname, const GLfloat *param) \
    ENTRY(void,           glBlendFunci,                   GLuint buf, GLenum src, GLenum dst) \
    /* GL_VERSION_4_1 */ \
    ENTRY(void,           glProgramUniform1i,             GLuint program, GLint location, GLint v0); \
    /* GL_VERSION_4_2 */ \
    ENTRY(void,           glBindImageTexture,             GLuint unit, GLuint texture, GLint level, GLboolean layered, GLint layer, GLenum access, GLenum format) \
    ENTRY(void,           glDrawElementsInstancedBaseVertexBaseInstance, GLenum mode, GLsizei count, GLenum type, const void *indices, GLsizei instancecount, GLint basevertex, GLuint baseinstance) \
    ENTRY(void,           glMemoryBarrier,                GLbitfield barriers) \
    /* GL_VERSION_4_3 */ \
    ENTRY(void,           glDispatchCompute,              GLuint num_groups_x, GLuint num_groups_y, GLuint num_groups_z) \
    ENTRY(void,           glInvalidateBufferData,         GLuint buffer) \
    ENTRY(void,           glMultiDrawElementsIndirect,    GLenum mode, GLenum type, const void *indirect, GLsizei drawcount, GLsizei stride) \
    /* GL_VERSION_4_4 */ \
    ENTRY(void,           glBindTextures,                 GLuint first, GLsizei count, const GLuint *textures) \
    /* GL_VERSION_4_5 */ \
    ENTRY(void,           glCreateBuffers,                GLsizei n, GLuint *buffers) \
    ENTRY(void,           glNamedBufferStorage,           GLuint buffer, GLsizeiptr size, const void *data, GLbitfield flags) \
    ENTRY(void,           glClearNamedBufferData,         GLuint buffer, GLenum internalformat, GLenum format, GLenum type, const void *data) \
    ENTRY(void*,          glMapNamedBufferRange,          GLuint buffer, GLintptr offset, GLsizeiptr length, GLbitfield access) \
    ENTRY(GLboolean,      glUnmapNamedBuffer,             GLuint buffer) \
    ENTRY(void,           glCreateFramebuffers,           GLsizei n, GLuint *framebuffers) \
    ENTRY(void,           glNamedFramebufferRenderbuffer, GLuint framebuffer, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) \
    ENTRY(void,           glNamedFramebufferTexture,      GLuint framebuffer, GLenum attachment, GLuint texture, GLint level) \
    ENTRY(void,           glNamedFramebufferDrawBuffer,   GLuint framebuffer, GLenum buf) \
    ENTRY(void,           glNamedFramebufferDrawBuffers,  GLuint framebuffer, GLsizei n, const GLenum *bufs) \
    ENTRY(void,           glNamedFramebufferReadBuffer,   GLuint framebuffer, GLenum src) \
    ENTRY(void,           glClearNamedFramebufferfv,      GLuint framebuffer, GLenum buffer, GLint drawbuffer, const GLfloat* value) \
    ENTRY(void,           glBlitNamedFramebuffer,         GLuint readFramebuffer, GLuint drawFramebuffer, GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) \
    ENTRY(GLenum,         glCheckNamedFramebufferStatus,  GLuint framebuffer, GLenum target) \
    ENTRY(void,           glCreateRenderbuffers,          GLsizei n, GLuint *renderbuffers) \
    ENTRY(void,           glNamedRenderbufferStorage,     GLuint renderbuffer, GLenum internalformat, GLsizei width, GLsizei height) \
    ENTRY(void,           glNamedRenderbufferStorageMultisample, GLuint renderbuffer, GLsizei samples, GLenum internalformat, GLsizei width, GLsizei height) \
    ENTRY(void,           glCreateTextures,               GLenum target, GLsizei n, GLuint *textures) \
    ENTRY(void,           glTextureStorage2D,             GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) \
    ENTRY(void,           glTextureStorage3D,             GLuint texture, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth) \
    ENTRY(void,           glTextureSubImage2D,            GLuint texture, GLint level, GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, GLenum format, GLenum type, const void *pixels) \
    ENTRY(void,           glGenerateTextureMipmap,        GLuint texture) \
    ENTRY(void,           glBindTextureUnit,              GLuint unit, GLuint texture) \
    ENTRY(void,           glCreateVertexArrays,           GLsizei n, GLuint *arrays) \
    ENTRY(void,           glEnableVertexArrayAttrib,      GLuint vaobj, GLuint index) \
    ENTRY(void,           glVertexArrayElementBuffer,     GLuint vaobj, GLuint buffer) \
    ENTRY(void,           glVertexArrayVertexBuffer,      GLuint vaobj, GLuint bindingindex, GLuint buffer, GLintptr offset, GLsizei stride) \
    ENTRY(void,           glVertexArrayAttribBinding,     GLuint vaobj, GLuint attribindex, GLuint bindingindex) \
    ENTRY(void,           glVertexArrayAttribFormat,      GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLboolean normalized, GLuint relativeoffset) \
    ENTRY(void,           glVertexArrayAttribIFormat,     GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset) \
    ENTRY(void,           glVertexArrayAttribLFormat,     GLuint vaobj, GLuint attribindex, GLint size, GLenum type, GLuint relativeoffset) \
    ENTRY(void,           glVertexArrayBindingDivisor,    GLuint vaobj, GLuint bindingindex, GLuint divisor) \
    ENTRY(void,           glCreateSamplers,               GLsizei n, GLuint *samplers) \
    \
    /* OpenGL procedures for debugging: */ \
    /* GL_VERSION_1_0 */ \
    ENTRY(GLenum,         glGetError,                     void) \
    /* GL_VERSION_4_3 */ \
    ENTRY(void,           glDebugMessageControl,          GLenum source, GLenum type, GLenum severity, GLsizei count, const GLuint *ids, GLboolean enabled) \
    ENTRY(void,           glDebugMessageCallback,         GLDEBUGPROC callback, const void *userParam) \
    ENTRY(void,           glPushDebugGroup,               GLenum source, GLuint id, GLsizei length, const GLchar *message) \
    ENTRY(void,           glPopDebugGroup,                void) \
    \
    /* OpenGL procedures for object labeling: */ \
    /* GL_VERSION_4_3 */ \
    ENTRY(void,           glObjectLabel,                  GLenum identifier, GLuint name, GLsizei length, const GLchar *label) \
    ENTRY(void,           glObjectPtrLabel,               const void *ptr, GLsizei length, const GLchar *label) \
    ENTRY(void,           glGetObjectLabel,               GLenum identifier, GLuint name, GLsizei bufSize, GLsizei *length, GLchar *label) \
    \
    /* OpenGL procedures for introspection: */ \
    /* GL_VERSION_2_0 */ \
    ENTRY(void,           glValidateProgram,              GLuint program) \
    /* GL_VERSION_4_3 */ \
    ENTRY(void,           glGetProgramInterfaceiv,        GLuint program, GLenum programInterface, GLenum pname, GLint *params) \
    ENTRY(GLuint,         glGetProgramResourceIndex,      GLuint program, GLenum programInterface, const GLchar *name) \
    ENTRY(void,           glGetProgramResourceName,       GLuint program, GLenum programInterface, GLuint index, GLsizei bufSize, GLsizei *length, GLchar *name) \
    ENTRY(void,           glGetProgramResourceiv,         GLuint program, GLenum programInterface, GLuint index, GLsizei propCount, const GLenum *props, GLsizei bufSize, GLsizei *length, GLint *params) \
    \
    /* Extra OpenGL procedures for ImGui: */ \
    /* GL_VERSION_1_0 */ \
    ENTRY(void,           glPolygonMode,                  GLenum face, GLenum mode) \
    ENTRY(void,           glScissor,                      GLint x, GLint y, GLsizei width, GLsizei height) \
    ENTRY(void,           glTexImage2D,                   GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void *pixels) \
    ENTRY(void,           glTexParameteri,                GLenum target, GLenum pname, GLint param) \
    /* ENTRY(void,           glDisable,                      GLenum cap) */ \
    /* ENTRY(void,           glEnable,                       GLenum cap) */ \
    /* ENTRY(void,           glBlendFunc,                    GLenum sfactor, GLenum dfactor) */ \
    /* ENTRY(void,           glPixelStorei,                  GLenum pname, GLint param) */ \
    /* ENTRY(void,           glGetIntegerv,                  GLenum pname, GLint *data) */ \
    ENTRY(GLboolean,      glIsEnabled,                    GLenum cap) \
    /* ENTRY(void,           glViewport,                     GLint x, GLint y, GLsizei width, GLsizei height) */ \
    /* GL_VERSION_1_1 */ \
    ENTRY(void,           glBindTexture,                  GLenum target, GLuint texture) \
    /* ENTRY(void,           glDeleteTextures,               GLsizei n, const GLuint *textures) */ \
    ENTRY(void,           glGenTextures,                  GLsizei n, GLuint *textures) \
    /* GL_VERSION_1_3 */ \
    ENTRY(void,           glActiveTexture,                GLenum texture) \
    /* GL_VERSION_1_4 */ \
    ENTRY(void,           glBlendFuncSeparate,            GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha) \
    ENTRY(void,           glBlendEquation,                GLenum mode) \
    /* ENTRY(void,           glDeleteBuffers,                GLsizei n, const GLuint *buffers) */ \
    ENTRY(void,           glGenBuffers,                   GLsizei n, GLuint *buffers) \
    /* GL_VERSOIN_1_5 */ \
    /* ENTRY(void,           glBindBuffer,                   GLenum target, GLuint buffer) */ \
    ENTRY(void,           glBufferData,                   GLenum target, GLsizeiptr size, const void *data, GLenum usage) \
    /* GL_VERSION_2_0 */ \
    ENTRY(void,           glBlendEquationSeparate,        GLenum modeRGB, GLenum modeAlpha) \
    /* ENTRY(void,           glAttachShader,                 GLuint program, GLuint shader) */ \
    /* ENTRY(void,           glCompileShader,                GLuint shader) */ \
    /* ENTRY(GLuint,         glCreateProgram,                void) */ \
    /* ENTRY(GLuint,         glCreateShader,                 GLenum type) */ \
    /* ENTRY(void,           glDeleteProgram,                GLuint program) */ \
    /* ENTRY(void,           glDeleteShader,                 GLuint shader) */ \
    /* ENTRY(void,           glDetachShader,                 GLuint program, GLuint shader) */ \
    ENTRY(void,           glEnableVertexAttribArray,      GLuint index) \
    ENTRY(GLint,          glGetAttribLocation,            GLuint program, const GLchar *name) \
    ENTRY(GLint,          glGetUniformLocation,           GLuint program, const GLchar *name) \
    /* ENTRY(void,           glLinkProgram,                  GLuint program) */ \
    /* ENTRY(void,           glShaderSource,                 GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length) */ \
    /* ENTRY(void,           glUseProgram,                   GLuint program) */ \
    ENTRY(void,           glUniform1i,                    GLint location, GLint v0) \
    ENTRY(void,           glUniformMatrix4fv,             GLint location, GLsizei count, GLboolean transpose, const GLfloat *value) \
    ENTRY(void,           glVertexAttribPointer,          GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer) \
    /* GL_VERSION_3_0 */ \
    /* ENTRY(void,           glBindVertexArray,              GLuint array) */ \
    /* ENTRY(void,           glDeleteVertexArrays,           GLsizei n, const GLuint *arrays) */ \
    ENTRY(void,           glGenVertexArrays,              GLsizei n, GLuint *arrays) \
    /* GL_VERSION_3_2 */ \
    ENTRY(void,           glDrawElementsBaseVertex,       GLenum mode, GLsizei count, GLenum type, const void *indices, GLint basevertex) \
    /* GL_VERSION_3_3 */ \
    /* ENTRY(void,           glBindSampler,                  GLuint unit, GLuint sampler) */

// clang-format on

#define GL_API_DECL_ENTRY(return_type, name, ...)      \
  typedef return_type GL_DECL name##Proc(__VA_ARGS__); \
  extern name##Proc *name;
GL_API_LIST(GL_API_DECL_ENTRY)
#undef GL_API_DECL_ENTRY

namespace viewer {

namespace gl {

bool Init();

bool ExtensionSupported(const GLchar *extension);

void _CheckError(const char *file, int line);

#define GL_BUFFER_OFFSET(bytes) \
  reinterpret_cast<GLubyte *>(static_cast<ptrdiff_t>(bytes))

#ifndef NDEBUG
#define CheckError() _CheckError(__FILE__, __LINE__)
#else
#define CheckError() ((void)0)
#endif

void DebugMessageCallback(GLenum source, GLenum type, GLuint id,
                          GLenum severity, GLsizei length, const GLchar *msg,
                          const void *data);

}  // namespace gl

}  // namespace viewer
