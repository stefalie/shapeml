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

#include <cassert>
#include <vector>

#include "shapeml/geometry/vector_types.h"

namespace shapeml {

namespace geometry {

// Triangulates a polygon by ear clipping.
// Follows the implementation of https://github.com/ivanfratric/polypartition.
//
// TODO(stefalie): Add support for polygons with holes.
template <class T>
class Triangulator {
 public:
  bool Triangulize(const IdxVec& face, const std::vector<T>& positions,
                   IdxVecVec* triangles, bool ccw);
  bool Triangulize(const std::vector<T>& polygon, IdxVecVec* triangles,
                   bool ccw);

 private:
  struct Vertex {
    unsigned index;

    bool active = true;
    bool convex;
    bool ear;

    Scalar angle;

    T pos;
    Vertex* prev;
    Vertex* next;
  };

  bool TriangulizeImpl(const IdxVec& face, const std::vector<T>& positions,
                       IdxVecVec* triangles, bool ccw);
  bool TriangulizeImpl(const std::vector<T>& polygon, IdxVecVec* triangles,
                       bool ccw);

  bool IsConvex(const T& p, const T& c, const T& n);
  bool IsInside(const T& p1, const T& p2, const T& p3, const T& p);

  void UpdateVertex(Vertex* v);

  bool ccw_;
  T normal_;  // Only used in the 3D case.
};

// Not having these forward declarations makes Clang sad.
template <>
bool Triangulator<Vec2>::Triangulize(const IdxVec& face,
                                     const Vec2Vec& positions,
                                     IdxVecVec* triangles, bool ccw);
template <>
bool Triangulator<Vec3>::Triangulize(const IdxVec& face,
                                     const Vec3Vec& positions,
                                     IdxVecVec* triangles, bool ccw);
template <>
bool Triangulator<Vec2>::Triangulize(const Vec2Vec& polygon,
                                     IdxVecVec* triangles, bool ccw);
template <>
bool Triangulator<Vec3>::Triangulize(const Vec3Vec& polygon,
                                     IdxVecVec* triangles, bool ccw);
template <>
bool Triangulator<Vec2>::IsConvex(const Vec2& p, const Vec2& c, const Vec2& n);
template <>
bool Triangulator<Vec3>::IsConvex(const Vec3& p, const Vec3& c, const Vec3& n);

extern template class Triangulator<Vec2>;
typedef Triangulator<Vec2> Triangulator2D;

extern template class Triangulator<Vec3>;
typedef Triangulator<Vec3> Triangulator3D;

}  // namespace geometry

}  // namespace shapeml
