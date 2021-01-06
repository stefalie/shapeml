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

#include "shapeml/geometry/triangulator.h"

namespace shapeml {

namespace geometry {

// Generic part of the implementation:

template <class T>
bool Triangulator<T>::TriangulizeImpl(const IdxVec& face,
                                      const std::vector<T>& positions,
                                      IdxVecVec* triangles, bool ccw) {
  const int num_verts = static_cast<int>(face.size());
  if (num_verts < 3) {
    assert(!"ERROR: Invalid face with only 2 vertices.");
    return true;
  } else if (num_verts == 3) {
    triangles->push_back(face);
    return false;
  }

  ccw_ = ccw;

  Vertex* vertices = new Vertex[num_verts];

  for (int i = 0; i < num_verts; ++i) {
    vertices[i].index = face[i];
    vertices[i].pos = positions[face[i]];
    vertices[i].prev = &vertices[(i + num_verts - 1) % num_verts];
    vertices[i].next = &vertices[(i + 1) % num_verts];
  }

  for (int i = 0; i < num_verts; ++i) {
    UpdateVertex(&vertices[i]);
  }

  for (int i = 0; i < num_verts - 3; ++i) {
    // Find the most extruded ear.
    Vertex* ear = nullptr;
    for (int j = 0; j < num_verts; ++j) {
      if (!vertices[j].active || !vertices[j].ear) {
        continue;
      }

      if (!ear) {
        ear = &vertices[j];
      } else if (vertices[j].angle > ear->angle) {
        ear = &vertices[j];
      }
    }

    if (!ear) {
      assert(!"ERROR: Most likely wrong winding.");
      delete[] vertices;
      return true;
    }

    triangles->push_back({ear->prev->index, ear->index, ear->next->index});

    ear->active = false;
    ear->next->prev = ear->prev;
    ear->prev->next = ear->next;

    if (i < num_verts - 4) {
      UpdateVertex(ear->prev);
      UpdateVertex(ear->next);
    }
  }
  for (int i = 0; i < num_verts; ++i) {
    if (vertices[i].active) {
      triangles->push_back({vertices[i].prev->index, vertices[i].index,
                            vertices[i].next->index});
      break;
    }
  }

  delete[] vertices;
  return false;
}

template <class T>
bool Triangulator<T>::TriangulizeImpl(const std::vector<T>& polygon,
                                      IdxVecVec* triangles, bool ccw) {
  IdxVec face(polygon.size());
  for (unsigned i = 0; i < face.size(); ++i) {
    face[i] = i;
  }
  return Triangulize(face, polygon, triangles, ccw);
}

template <class T>
void Triangulator<T>::UpdateVertex(Vertex* v) {
  Vertex* p = v->prev;
  Vertex* n = v->next;
  v->convex = IsConvex(p->pos, v->pos, n->pos);

  v->angle = (n->pos - v->pos).normalized().dot((p->pos - v->pos).normalized());

  if (v->convex) {
    // If convex, check if the triangle at the current vertex could be an
    // an ear, i.e., none of the remaining vertices are inside the triangle.
    v->ear = true;
    Vertex* it = n->next;
    while (it != p) {
      if (IsInside(p->pos, v->pos, n->pos, it->pos)) {
        v->ear = false;
        break;
      }
      it = it->next;
    }
  } else {
    v->ear = false;
  }
}

template <class T>
bool Triangulator<T>::IsInside(const T& p1, const T& p2, const T& p3,
                               const T& p) {
  if (IsConvex(p1, p, p2)) {
    return false;
  }
  if (IsConvex(p2, p, p3)) {
    return false;
  }
  if (IsConvex(p3, p, p1)) {
    return false;
  }
  return true;
}

// Specialized parts of the implementation:

template <>
bool Triangulator<Vec2>::Triangulize(const IdxVec& face,
                                     const Vec2Vec& positions,
                                     IdxVecVec* triangles, bool ccw) {
  return TriangulizeImpl(face, positions, triangles, ccw);
}

// Implements the Newell method to compute a polygon's normal.
// See Filippo Tampieri, GPU Gems 3, Newell's Method for Computing the Plane
// Equation of a Polygon.
static Vec3 PolygonNormal(const Vec3Vec& polygon) {
  Vec3 normal = Vec3::Zero();
  for (size_t i = 0; i < polygon.size(); ++i) {
    const Vec3& p = polygon[i];
    const Vec3& pn = polygon[(i + 1) % polygon.size()];
    normal[0] += (p[1] - pn[1]) * (p[2] + pn[2]);
    normal[1] += (p[2] - pn[2]) * (p[0] + pn[0]);
    normal[2] += (p[0] - pn[0]) * (p[1] + pn[1]);
  }

  return normal.normalized();
}

template <>
bool Triangulator<Vec3>::Triangulize(const IdxVec& face,
                                     const Vec3Vec& positions,
                                     IdxVecVec* triangles, bool ccw) {
  Vec3Vec tmp;
  for (size_t i = 0; i < face.size(); ++i) {
    tmp.push_back(positions[face[i]]);
  }
  normal_ = PolygonNormal(tmp);
  return TriangulizeImpl(face, positions, triangles, ccw);
}

template <>
bool Triangulator<Vec2>::Triangulize(const Vec2Vec& polygon,
                                     IdxVecVec* triangles, bool ccw) {
  return TriangulizeImpl(polygon, triangles, ccw);
}

template <>
bool Triangulator<Vec3>::Triangulize(const Vec3Vec& polygon,
                                     IdxVecVec* triangles, bool ccw) {
  normal_ = PolygonNormal(polygon);
  return TriangulizeImpl(polygon, triangles, ccw);
}

// The condition below used to be >= 0.0. That's problematic however in the
// case where a vertex lies exactly on an edge (but the vertex is not
// connected with that edge). That's a slightly degenerate case. E.g.:
//   /| /|
//  /_|/_|
//
template <>
bool Triangulator<Vec2>::IsConvex(const Vec2& p, const Vec2& c, const Vec2& n) {
  const Vec2 dir_n = (ccw_ ? (n - c) : (p - c)).normalized();
  const Vec2 dir_p = (ccw_ ? (p - c) : (n - c)).normalized();
  return (dir_n.x() * dir_p.y() - dir_n.y() * dir_p.x()) > EPSILON;
}

// The same applies as in the above comment.
template <>
bool Triangulator<Vec3>::IsConvex(const Vec3& p, const Vec3& c, const Vec3& n) {
  const Vec3 dir_n = (ccw_ ? (n - c) : (p - c)).normalized();
  const Vec3 dir_p = (ccw_ ? (p - c) : (n - c)).normalized();
  return dir_n.cross(dir_p).dot(normal_) > EPSILON;
}

// If these are not here at the bottom, Clang will fail to link.
template class Triangulator<Vec2>;
template class Triangulator<Vec3>;

}  // namespace geometry

}  // namespace shapeml
