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

#include "shapeml/geometry/skeleton/roof.h"

#include <cfloat>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "shapeml/geometry/ray_plane_bv.h"
#include "shapeml/geometry/skeleton/skeleton.h"
#include "shapeml/geometry/triangulator.h"

namespace shapeml {

namespace geometry {

namespace skeleton {

static void ConnectFaces(const Skeleton& skeleton, const Vec2Vec& vec2s,
                         bool triangulate, IdxVecVec* faces);

void Roof(const Skeleton& skeleton, Scalar angle, bool gable, bool triangulate,
          Vec3Vec* vertices, IdxVecVec* faces) {
  assert(angle > EPSILON && angle < (90.0 - EPSILON));
  const Scalar angle_rad = angle * M_PI / 180.0;

  vertices->clear();
  faces->clear();

  const Vec2Vec& vec2s = skeleton.positions();
  for (size_t i = 0; i < vec2s.size(); ++i) {
    vertices->push_back(Vec3(
        vec2s[i].x(), tan(angle_rad) * skeleton.distances()[i], -vec2s[i].y()));
  }

  // Gables
  int num_orig_verts = static_cast<int>(skeleton.edges().size());
  const std::vector<Arc>& arcs = skeleton.skeleton();

  if (gable && arcs.size() > 3) {
    std::unordered_map<int, int> arc_map;
    std::unordered_map<int, std::unordered_set<int>> arc_inv_map;
    for (const Arc& arc : arcs) {
      arc_map[arc.v2] = arc.v1;
      arc_inv_map[arc.v1].insert(arc.v2);
    }

    for (int i = 0; i < num_orig_verts; ++i) {
      auto it = arc_map.find(i);
      if (it == arc_map.end()) {
        continue;
      }
      int v = it->second;

      int in = i;
      do {
        in = (in + 1) % num_orig_verts;
      } while (arc_map.find(in) == arc_map.end());

      // Skip reflex vertices.
      if (skeleton.vertices()[i].reflex || skeleton.vertices()[in].reflex) {
        continue;
      }

      if (v == arc_map[in]) {
        // The point that we want to shift to create a vertical roof face has
        // either 2 incoming and 1 outgoing arc(s) or 3 incoming and 0
        // outgoing arcs.
        Vec3 v_offset;
        bool do_offset = false;
        if (arc_inv_map[v].size() == 2 && arc_map.find(v) != arc_map.end()) {
          v_offset = (*vertices)[v] - (*vertices)[arc_map[v]];
          do_offset = true;
        } else if (arc_inv_map[v].size() == 3 &&
                   arc_map.find(v) == arc_map.end()) {
          auto it = arc_inv_map[v].begin();
          while (*it == i || *it == in) {
            ++it;
          }
          assert(it != arc_inv_map[v].end());

          v_offset = (*vertices)[v] - (*vertices)[*it];
          do_offset = true;
        }

        if (do_offset) {
          const Vec2& e = skeleton.EdgeDir(skeleton.vertices()[i].e_next);
          Plane3 plane(Vec3(e.y(), 0.0, e.x()), (*vertices)[i]);
          Ray3 ray_v((*vertices)[v], v_offset);
          IntersectionRayPlane(ray_v, plane, &(*vertices)[v]);
        }
      }
    }
  }

  ConnectFaces(skeleton, vec2s, triangulate, faces);
}

static void ConnectFaces(const Skeleton& skeleton, const Vec2Vec& vec2s,
                         bool triangulate, IdxVecVec* faces) {
  // We do a ccw loop of the face. We simply follow leftmost neighbors at any
  // given point.
  const auto& edges = skeleton.edges();
  const auto& neighbors = skeleton.neighbors();
  std::unordered_set<int> used_orig_edges;
  for (const OrigEdge& edge : edges) {
    IdxVec face;
    int second_last = edge.from->pos_index;

    if (used_orig_edges.find(second_last) != used_orig_edges.end()) {
      continue;
    }

    int last = edge.to->pos_index;
    face.push_back(second_last);

    do {
      face.push_back(last);
      if (second_last < static_cast<int>(edges.size()) &&
          last < static_cast<int>(edges.size())) {
        used_orig_edges.insert(second_last);
      } else if (second_last >= static_cast<int>(edges.size()) &&
                 last < static_cast<int>(edges.size())) {
        used_orig_edges.insert(last);
      }

      const Vec2 last_dir = (vec2s[last] - vec2s[second_last]).normalized();

      int best_n = -1;
      Scalar best_angle = DBL_MAX;

      for (int n : neighbors[last]) {
        if (n == second_last) {
          continue;
        }

        const Vec2 dir = (vec2s[n] - vec2s[last]).normalized();

        const Scalar angle = -atan2(Cross2D(last_dir, dir), last_dir.dot(dir));
        if (angle < best_angle) {
          best_n = n;
          best_angle = angle;
        }
      }

      second_last = last;
      last = best_n;
    } while (last != static_cast<int>(face.front()));

    faces->push_back(face);
  }

  if (triangulate) {
    Triangulator2D triangulator;
    std::vector<IdxVec> new_faces;
    for (const IdxVec& face : *faces) {
      triangulator.Triangulize(face, vec2s, &new_faces, true);
    }
    *faces = new_faces;
  }

  // Add the bottom face.
  IdxVec bottom;
  const OrigEdge* it = &edges.front();
  do {
    bottom.push_back(it->to->pos_index);
    it = it->from->e_prev;
  } while (it != &edges.front());
  if (triangulate) {
    Triangulator2D triangulator;
    triangulator.Triangulize(bottom, vec2s, faces, false);
  } else {
    faces->push_back(bottom);
  }
}

}  // namespace skeleton

}  // namespace geometry

}  // namespace shapeml
