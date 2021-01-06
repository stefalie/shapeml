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

#include "shapeml/geometry/vector_types.h"

namespace shapeml {

namespace geometry {

// If the uvs parameter (or also the normals parameter for the mesh functions)
// is left empty (nullptr), no uv coordinates (or normals) will be generated.

void PolygonUnitSquare(Vec3Vec* polygon, Vec2Vec* uvs);

void PolygonUnitCircle(unsigned n_phi, Vec3Vec* polygon, Vec2Vec* uvs);

void PolygonL(Scalar w, Scalar h, Scalar back_width, Scalar left_width,
              Vec3Vec* polygon, Vec2Vec* uvs);

void PolygonU(Scalar w, Scalar h, Scalar back_width, Scalar left_width,
              Scalar right_width, Vec3Vec* polygon, Vec2Vec* uvs);

void PolygonT(Scalar w, Scalar h, Scalar top_width, Scalar vertical_width,
              Scalar vertical_position, Vec3Vec* polygon, Vec2Vec* uvs);

void PolygonH(Scalar w, Scalar h, Scalar left_width, Scalar horizontal_width,
              Scalar right_width, Scalar horizontal_position, Vec3Vec* polygon,
              Vec2Vec* uvs);

// Vertex and uv indices are identical.
void MeshUnitGrid(unsigned n_x, unsigned n_z, Vec3Vec* vertices,
                  IdxVecVec* indices, Vec2Vec* uvs);

// Vertex and uv indices are identical.
void MeshUnitDisk(unsigned n_phi, unsigned n_rad, Vec3Vec* vertices,
                  IdxVecVec* indices, Vec2Vec* uvs);

void MeshUnitCube(Vec3Vec* vertices, IdxVecVec* indices, Vec2Vec* uvs,
                  IdxVecVec* uv_indices);

void MeshUnitBox(unsigned n_x, unsigned n_y, unsigned n_z, Vec3Vec* vertices,
                 IdxVecVec* indices, Vec2Vec* uvs, IdxVecVec* uv_indices);

void MeshUnitCylinder(unsigned n_phi, unsigned n_y, unsigned n_rad,
                      Vec3Vec* vertices, IdxVecVec* indices, Vec3Vec* normals,
                      IdxVecVec* normal_indices, Vec2Vec* uvs,
                      IdxVecVec* uv_indices);

void MeshUnitCone(unsigned n_phi, unsigned n_y, unsigned n_rad,
                  Vec3Vec* vertices, IdxVecVec* indices, Vec3Vec* normals,
                  IdxVecVec* normal_indices, Vec2Vec* uvs,
                  IdxVecVec* uv_indices);

void MeshUnitSphere(unsigned n_phi, unsigned n_theta, Vec3Vec* vertices,
                    IdxVecVec* indices, Vec3Vec* normals,
                    IdxVecVec* normal_indices, Vec2Vec* uvs,
                    IdxVecVec* uv_indices);

// Vertex and normal indices are identical.
void MeshTorus(Scalar radius_1, Scalar radius_2, unsigned n_phi,
               unsigned n_theta, Vec3Vec* vertices, IdxVecVec* indices,
               Vec3Vec* normals, Vec2Vec* uvs, IdxVecVec* uv_indices);

}  // namespace geometry

}  // namespace shapeml
