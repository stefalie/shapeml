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

#include <vector>

#include "shapeml/geometry/vector_types.h"

namespace shapeml {

namespace geometry {

struct Ray3 {
  Ray3();
  Ray3(const Vec3& o, const Vec3& d);

  Scalar DistanceToPoint(const Vec3& p) const;

  Vec3 origin;
  Vec3 dir;
};

struct Plane3 {
  Plane3();
  Plane3(const Vec3& n, Scalar d);
  Plane3(const Vec3& n, const Vec3& p);
  static Plane3 FromPoints(const Vec3& p1, const Vec3& p2, const Vec3& p3);

  Scalar Distance(const Vec3& p) const;
  bool On(const Vec3& p) const;
  bool Above(const Vec3& p) const;
  bool Below(const Vec3& p) const;
  bool OnOrBelow(const Vec3& p) const;

  Plane3 Transform(const Isometry3& trafo) const;

  Vec3 normal;
  Scalar dist;
};

bool IntersectionRayPlane(const Ray3& ray, const Plane3& plane,
                          Vec3* intersection, Scalar* lambda);
bool IntersectionRayPlane(const Ray3& ray, const Plane3& plane,
                          Vec3* intersection);

struct OBB {
  Vec3 center;
  Vec3 axes[3];
  Vec3 extent;

  bool Intersect(const OBB& other) const;

  bool Contains(const Vec3& p) const;
  bool Contains(const OBB& other) const;
};

bool TestOBBPlane(const OBB& obb, const Plane3& plane);

struct AABB {
  AABB();
  explicit AABB(const Vec3Vec& vertices);
  void Merge(const AABB& other);
  Vec3 center;
  Vec3 extent;

  bool Intersect(const AABB& other) const;
};

bool TestAABBPlane(const AABB& aabb, const Plane3& plane);

}  // namespace geometry

}  // namespace shapeml
