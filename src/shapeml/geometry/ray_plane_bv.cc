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

#include "shapeml/geometry/ray_plane_bv.h"

#include <algorithm>
#include <cfloat>
#include <cmath>

namespace shapeml {

namespace geometry {

Ray3::Ray3() {}

Ray3::Ray3(const Vec3& o, const Vec3& d) {
  origin = o;
  dir = d;
}

Scalar Ray3::DistanceToPoint(const Vec3& p) const {
  return (p - origin).cross(dir.normalized()).norm();
}

Plane3::Plane3() {}

Plane3::Plane3(const Vec3& n, Scalar d) : normal(n), dist(d) {
  normal.normalize();
}

Plane3::Plane3(const Vec3& n, const Vec3& p) : normal(n) {
  normal.normalize();
  dist = normal.dot(p);
}

Plane3 Plane3::FromPoints(const Vec3& p1, const Vec3& p2, const Vec3& p3) {
  return Plane3((p2 - p1).cross(p3 - p1), p1);
}

Scalar Plane3::Distance(const Vec3& p) const { return normal.dot(p) - dist; }
bool Plane3::On(const Vec3& p) const { return fabs(Distance(p)) < EPSILON; }
bool Plane3::Above(const Vec3& p) const { return Distance(p) > 0.0; }
bool Plane3::Below(const Vec3& p) const { return Distance(p) < 0.0; }
bool Plane3::OnOrBelow(const Vec3& p) const { return Distance(p) < EPSILON; }

Plane3 Plane3::Transform(const Isometry3& trafo) const {
  const Vec3 n = trafo.linear().inverse().transpose() * normal;
  const Vec3 p = trafo * (normal * dist);
  return Plane3(n, p);
}

bool IntersectionRayPlane(const Ray3& ray, const Plane3& plane,
                          Vec3* intersection) {
  Scalar lambda;
  return IntersectionRayPlane(ray, plane, intersection, &lambda);
}

bool IntersectionRayPlane(const Ray3& ray, const Plane3& plane,
                          Vec3* intersection, Scalar* lambda) {
  const Scalar divisor = plane.normal.dot(ray.dir);

  // Check ray/plane colinearity.
  if (fabs(divisor) < EPSILON) {
    return false;
  }

  *lambda = (plane.dist - plane.normal.dot(ray.origin)) / divisor;
  *intersection = ray.origin + *lambda * ray.dir;
  return true;
}

// From real-time collision detection book.
bool OBB::Intersect(const OBB& other) const {
  Scalar ra, rb;
  Mat3 R, AbsR;
  // Compute rotation matrix expressing b in a’s coordinate frame
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      R(i, j) = axes[i].dot(other.axes[j]);
    }
  }

  // Compute translation vector t
  Vec3 t = other.center - center;

  // Bring translation into a’s coordinate frame
  t = Vec3(t.dot(axes[0]), t.dot(axes[1]), t.dot(axes[2]));

  // Compute common subexpressions. Add in an epsilon term to
  // counteract arithmetic errors when two edges are parallel and
  // their cross product is (near) null (see text for details)
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      AbsR(i, j) = fabs(R(i, j)) + EPSILON;
    }
  }

  // Test axes L = A0, L = A1, L = A2
  for (int i = 0; i < 3; i++) {
    ra = extent[i];
    rb = other.extent[0] * AbsR(i, 0) + other.extent[1] * AbsR(i, 1) +
         other.extent[2] * AbsR(i, 2);
    if (fabs(t[i]) > ra + rb) {
      return false;
    }
  }

  // Test axes L = B0, L = B1, L = B2
  for (int i = 0; i < 3; i++) {
    ra = extent[0] * AbsR(0, i) + extent[1] * AbsR(1, i) +
         extent[2] * AbsR(2, i);
    rb = other.extent[i];
    if (fabs(t[0] * R(0, i) + t[1] * R(1, i) + t[2] * R(2, i)) > ra + rb) {
      return false;
    }
  }

  // Test axis L = A0 x B0
  ra = extent[1] * AbsR(2, 0) + extent[2] * AbsR(1, 0);
  rb = other.extent[1] * AbsR(0, 2) + other.extent[2] * AbsR(0, 1);
  if (fabs(t[2] * R(1, 0) - t[1] * R(2, 0)) > ra + rb) {
    return false;
  }

  // Test axis L = A0 x B1
  ra = extent[1] * AbsR(2, 1) + extent[2] * AbsR(1, 1);
  rb = other.extent[0] * AbsR(0, 2) + other.extent[2] * AbsR(0, 0);
  if (fabs(t[2] * R(1, 1) - t[1] * R(2, 1)) > ra + rb) {
    return false;
  }

  // Test axis L = A0 x B2
  ra = extent[1] * AbsR(2, 2) + extent[2] * AbsR(1, 2);
  rb = other.extent[0] * AbsR(0, 1) + other.extent[1] * AbsR(0, 0);
  if (fabs(t[2] * R(1, 2) - t[1] * R(2, 2)) > ra + rb) {
    return false;
  }

  // Test axis L = A1 x B0
  ra = extent[0] * AbsR(2, 0) + extent[2] * AbsR(0, 0);
  rb = other.extent[1] * AbsR(1, 2) + other.extent[2] * AbsR(1, 1);
  if (fabs(t[0] * R(2, 0) - t[2] * R(0, 0)) > ra + rb) {
    return false;
  }

  // Test axis L = A1 x B1
  ra = extent[0] * AbsR(2, 1) + extent[2] * AbsR(0, 1);
  rb = other.extent[0] * AbsR(1, 2) + other.extent[2] * AbsR(1, 0);
  if (fabs(t[0] * R(2, 1) - t[2] * R(0, 1)) > ra + rb) {
    return false;
  }

  // Test axis L = A1 x B2
  ra = extent[0] * AbsR(2, 2) + extent[2] * AbsR(0, 2);
  rb = other.extent[0] * AbsR(1, 1) + other.extent[1] * AbsR(1, 0);
  if (fabs(t[0] * R(2, 2) - t[2] * R(0, 2)) > ra + rb) {
    return false;
  }

  // Test axis L = A2 x B0
  ra = extent[0] * AbsR(1, 0) + extent[1] * AbsR(0, 0);
  rb = other.extent[1] * AbsR(2, 2) + other.extent[2] * AbsR(2, 1);
  if (fabs(t[1] * R(0, 0) - t[0] * R(1, 0)) > ra + rb) {
    return false;
  }

  // Test axis L = A2 x B1
  ra = extent[0] * AbsR(1, 1) + extent[1] * AbsR(0, 1);
  rb = other.extent[0] * AbsR(2, 2) + other.extent[2] * AbsR(2, 0);
  if (fabs(t[1] * R(0, 1) - t[0] * R(1, 1)) > ra + rb) {
    return false;
  }

  // Test axis L = A2 x B2
  ra = extent[0] * AbsR(1, 2) + extent[1] * AbsR(0, 2);
  rb = other.extent[0] * AbsR(2, 1) + other.extent[1] * AbsR(2, 0);
  if (fabs(t[1] * R(0, 2) - t[0] * R(1, 2)) > ra + rb) {
    return false;
  }

  // Since no separating axis is found, the OBBs must be intersecting
  return true;
}

bool OBB::Contains(const Vec3& p) const {
  const Vec3 d = p - center;
  for (int i = 0; i < 3; ++i) {
    if (fabs(d.dot(axes[i])) > extent[i]) {
      return false;
    }
  }
  return true;
}

bool OBB::Contains(const OBB& other) const {
  const Vec3 ex = other.axes[0] * other.extent[0];
  const Vec3 ey = other.axes[1] * other.extent[1];
  const Vec3 ez = other.axes[2] * other.extent[2];
  return (Contains(other.center - ex - ey - ez) &&
          Contains(other.center + ex - ey - ez) &&
          Contains(other.center - ex + ey - ez) &&
          Contains(other.center + ex + ey - ez) &&
          Contains(other.center - ex - ey + ez) &&
          Contains(other.center + ex - ey + ez) &&
          Contains(other.center - ex + ey + ez) &&
          Contains(other.center + ex + ey + ez));
}

bool TestOBBPlane(const OBB& obb, const Plane3& plane) {
  // Compute the projection interval radius of b onto L(t) = b.c + t * p.n
  Scalar r = obb.extent[0] * fabs(plane.normal.dot(obb.axes[0])) +
             obb.extent[1] * fabs(plane.normal.dot(obb.axes[1])) +
             obb.extent[2] * fabs(plane.normal.dot(obb.axes[2]));

  // Compute distance of box center from plane
  Scalar s = plane.normal.dot(obb.center) - plane.dist;

  // Intersection occurs when distance s falls within [-r,+r] interval
  return fabs(s) <= r;
}

AABB::AABB()
    : center(Vec3::Zero()), extent(Vec3(-DBL_MAX, -DBL_MAX, -DBL_MAX)) {}

AABB::AABB(const Vec3Vec& vertices) {
  Vec3 min(DBL_MAX, DBL_MAX, DBL_MAX);
  Vec3 max(-DBL_MAX, -DBL_MAX, -DBL_MAX);

  for (const auto& v : vertices) {
    for (int i = 0; i < 3; ++i) {
      if (v[i] > max[i]) {
        max[i] = v[i];
      }
      if (v[i] < min[i]) {
        min[i] = v[i];
      }
    }
  }

  center = (max + min) * 0.5;
  extent = (max - min) * 0.5;
}

void AABB::Merge(const AABB& other) {
  Vec3Vec verts;
  verts.push_back(center + extent);
  verts.push_back(center - extent);
  verts.push_back(other.center + other.extent);
  verts.push_back(other.center - other.extent);
  *this = AABB(verts);
}

bool AABB::Intersect(const AABB& other) const {
  if (fabs(center[0] - other.center[0]) > (extent[0] + other.extent[0])) {
    return false;
  }
  if (fabs(center[1] - other.center[1]) > (extent[1] + other.extent[1])) {
    return false;
  }
  if (fabs(center[2] - other.center[2]) > (extent[2] + other.extent[2])) {
    return false;
  }
  return true;
}

bool TestAABBPlane(const AABB& aabb, const Plane3& plane) {
  // Compute the projection interval radius of b onto L(t) = b.c + t * p.n
  Scalar r = aabb.extent[0] * fabs(plane.normal[0]) +
             aabb.extent[1] * fabs(plane.normal[1]) +
             aabb.extent[2] * fabs(plane.normal[2]);

  // Compute distance of box center from plane
  Scalar s = plane.normal.dot(aabb.center) - plane.dist;

  // Intersection occurs when distance s falls within [-r,+r] interval
  return fabs(s) <= r;
}

}  // namespace geometry

}  // namespace shapeml
