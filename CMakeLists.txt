# Shape Modeling Language (ShapeML)
# Copyright (C) 2019 Stefan Lienhard
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program.  If not, see <https://www.gnu.org/licenses/>.

cmake_minimum_required(VERSION 3.0)
project(ShapeML CXX)


# Flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU")
  set(CMAKE_CXX_FLAGS "-Wall -Werror -Wextra -std=c++14 -Wno-missing-field-initializers -Wno-class-memaccess")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-copy -Wno-uninitialized")  # Needed for Eigen vs GCC 9
  set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
  set(CMAKE_CXX_FLAGS_DEBUG "-g -Wno-unused-variable -Wno-unused-parameter -Wno-unused-function")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
  set(CMAKE_CXX_FLAGS "-Wall -Wextra -std=c++14 -Wno-missing-field-initializers -Wno-range-loop-construct")
  # Clang 10 completely ignores "-Wno-deprecated-copy". Solution for now is to remove "-Werror".
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-deprecated-copy")  # Needed for Eigen vs clang 10
  set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")
  set(CMAKE_CXX_FLAGS_DEBUG "-g")
elseif(CMAKE_CXX_COMPILER_ID MATCHES "MSVC")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /D_USE_MATH_DEFINES /D_ENABLE_EXTENDED_ALIGNED_STORAGE")
  set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} /MT")
  set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "${CMAKE_CXX_FLAGS_RELWITHDEBINFO} /MT")
  set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /MTd")
else()
  message(FATAL_ERROR "No compatible compiler found, you need MSVC, GCC, or Clang")
endif()

# Set 'Release' as default build type.
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE "Release")
endif()

# Dependencies
message(STATUS "Configuring GLFW")
add_subdirectory(external/glfw EXCLUDE_FROM_ALL)

message(STATUS "Configuring surface_mesh")
add_subdirectory(external/surface_mesh EXCLUDE_FROM_ALL)
target_include_directories(surface_mesh PRIVATE external/eigen)  
target_include_directories(surface_mesh PRIVATE src)  # Access to vector_types.h


# ShapeML library
add_library(shapeml STATIC "")

target_sources(shapeml PRIVATE
  src/shapeml/asset_cache.cc
  src/shapeml/asset_cache.h
  src/shapeml/evaluator.h
  src/shapeml/exporter.cc
  src/shapeml/exporter.h
  src/shapeml/expressions.cc
  src/shapeml/expressions.h
  src/shapeml/expressions_functions.cc
  src/shapeml/geometry/halfedge_mesh.cc
  src/shapeml/geometry/halfedge_mesh.h
  src/shapeml/geometry/obj_loader.cc
  src/shapeml/geometry/obj_loader.h
  src/shapeml/geometry/octree.cc
  src/shapeml/geometry/octree.h
  src/shapeml/geometry/primitives.cc
  src/shapeml/geometry/primitives.h
  src/shapeml/geometry/ray_plane_bv.cc
  src/shapeml/geometry/ray_plane_bv.h
  src/shapeml/geometry/skeleton/roof.cc
  src/shapeml/geometry/skeleton/roof.h
  src/shapeml/geometry/skeleton/skeleton.cc
  src/shapeml/geometry/skeleton/skeleton.h
  src/shapeml/geometry/triangulator.cc
  src/shapeml/geometry/triangulator.h
  src/shapeml/geometry/vector_types.h
  src/shapeml/grammar.cc
  src/shapeml/grammar.h
  src/shapeml/material.h
  src/shapeml/interpreter.cc
  src/shapeml/interpreter.h
  src/shapeml/interpreter_shape_attributes.cc
  src/shapeml/interpreter_shape_operations.cc
  src/shapeml/parser/lexer.cc
  src/shapeml/parser/lexer.h
  src/shapeml/parser/parser.cc
  src/shapeml/parser/parser.h
  src/shapeml/parser/token.cc
  src/shapeml/parser/token.h
  src/shapeml/shape.cc
  src/shapeml/shape.h
  src/shapeml/util/hex_color.cc
  src/shapeml/util/hex_color.h
  src/shapeml/util/noise.cc
  src/shapeml/util/noise.h
  src/shapeml/value.cc
  src/shapeml/value.h)

target_include_directories(shapeml PUBLIC src)
target_include_directories(shapeml SYSTEM PUBLIC external/eigen)
target_include_directories(shapeml SYSTEM PUBLIC external/surface_mesh/src)
target_link_libraries(shapeml PRIVATE surface_mesh)


# Viewer library
add_library(viewer STATIC "")

target_sources(viewer PRIVATE
  external/imgui/imconfig.h
  external/imgui/imgui.cpp
  external/imgui/imgui.h
  # external/imgui/imgui_demo.cpp
  external/imgui/imgui_draw.cpp
  external/imgui/imgui_impl_glfw.cpp
  external/imgui/imgui_impl_glfw.h
  external/imgui/imgui_impl_opengl3.cpp
  external/imgui/imgui_impl_opengl3.h
  external/imgui/imgui_internal.h
  external/imgui/imgui_stdlib.cpp
  external/imgui/imgui_stdlib.h
  external/imgui/imgui_widgets.cpp
  external/imgui/imstb_rectpack.h
  external/imgui/imstb_textedit.h
  external/imgui/imstb_truetype.h
  src/viewer/font_roboto.cc
  src/viewer/gl/init.cc
  src/viewer/gl/init.h
  src/viewer/gl/objects.cc
  src/viewer/gl/objects.h
  src/viewer/gl/renderer.cc
  src/viewer/gl/renderer.h
  src/viewer/gl/shader.cc
  src/viewer/gl/shader.h
  src/viewer/gl/texture_manager.cc
  src/viewer/gl/texture_manager.h
  src/viewer/gl/trackball.cc
  src/viewer/gl/trackball.h
  src/viewer/viewer.cc
  src/viewer/viewer.h
  src/viewer/shaders/csm.h
  src/viewer/shaders/debug_render_order.h
  src/viewer/shaders/deferred.h
  src/viewer/shaders/forward.h
  src/viewer/shaders/fullscreen_quad.h
  src/viewer/shaders/fxaa.h
  src/viewer/shaders/pbr.h
  src/viewer/shaders/preamble.h
  src/viewer/shaders/sao.h
  src/viewer/shaders/tonemap_gamma_luma.h
  src/viewer/shaders/transparency.h)

target_include_directories(viewer PRIVATE src)
target_include_directories(viewer PUBLIC external)
target_include_directories(viewer PRIVATE external/eigen)  
target_link_libraries(viewer PRIVATE glfw)


# ShapeMaker example executable
add_executable(ShapeMaker src/shape_maker.cc)
target_link_libraries(ShapeMaker PRIVATE shapeml viewer)


# Testing
# Google's gtest unit testing framework is used. It is not included in the
# ShapeML repository and needs to be installed separately.
option(SHAPEML_TESTS "Toggle unit tests and standalone tests." OFF)

if(SHAPEML_TESTS)
  enable_testing()

  add_executable(test_lexer
    src/shapeml/parser/lexer_test.cc
    src/shapeml/parser/lexer.cc
    src/shapeml/parser/token.cc)
  target_include_directories(test_lexer PRIVATE src)
  target_link_libraries(test_lexer PRIVATE gtest gtest_main pthread)
  add_test(test_lexer test_lexer)

  add_executable(test_parser src/shapeml/parser/parser_test.cc)
  target_link_libraries(test_parser PRIVATE shapeml gtest gtest_main pthread)
  add_test(test_parser test_parser)

  add_executable(test_expressions src/shapeml/expressions_test.cc)
  target_link_libraries(test_expressions PRIVATE shapeml gtest gtest_main pthread)
  add_test(test_expressions test_expressions)

  add_executable(test_interpreter src/shapeml/interpreter_test.cc)
  target_link_libraries(test_interpreter PRIVATE shapeml gtest gtest_main pthread)
  add_test(test_interpreter test_interpreter)

  add_executable(test_skeleton
    src/shapeml/geometry/skeleton/skeleton_test.cc
    src/shapeml/geometry/skeleton/skeleton.cc)
  target_include_directories(test_skeleton PRIVATE src)
  target_include_directories(test_skeleton PRIVATE external/eigen)  
  target_link_libraries(test_skeleton PRIVATE gtest gtest_main pthread)
  add_test(test_skeleton test_skeleton)
endif()

# Standalone example executables showing how to use ShapeML's interface for
# creating grammars and shape trees in code.
option(SHAPEML_TESTS_STANDALONE "Build standalone example executables." OFF)

if(SHAPEML_TESTS_STANDALONE)
  add_executable(test_interpreter_standalone src/shapeml/interpreter_test_standalone.cc)
  target_link_libraries(test_interpreter_standalone PRIVATE shapeml)

  add_executable(test_shape_standalone src/shapeml/shape_test_standalone.cc)
  target_link_libraries(test_shape_standalone PRIVATE shapeml)

  # This is only used by one test examples that uses an old school OpenGL context.
  find_package(OpenGL REQUIRED)
  add_executable(test_skeleton_standalone src/shapeml/geometry/skeleton/skeleton_test.cc)
  target_link_libraries(test_skeleton_standalone PRIVATE shapeml ${OPENGL_gl_LIBRARY} glfw)
  target_compile_definitions(test_skeleton_standalone PRIVATE SKELETON_TEST_STANDALONE)
endif()
