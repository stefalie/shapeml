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

#include <deque>
#include <list>
#include <queue>
#include <vector>

#include "shapeml/geometry/vector_types.h"

// A straight skeleton implementation based on:
//  - Felkel, Obdrzalek, 1998, Straight Skeleton Implementation
//  - Kelly, 2014, Unwritten Procedural Modeling with the Straight Skeleton
// Former paper is easy to read, but parts of it are believed to be incorrect.

namespace shapeml {

namespace geometry {

namespace skeleton {

struct Vertex;

struct OrigEdge {
  Vertex* from;
  Vertex* to;
  // Scalar weight = 1.0;  // TODO(stefalie): Implement weighted skeleton.
};

struct Vertex {
  unsigned pos_index;

  Vertex* v_prev = nullptr;
  Vertex* v_next = nullptr;
  const OrigEdge* e_prev = nullptr;
  const OrigEdge* e_next = nullptr;

  Vec2 bisector;
  bool reflex = false;

  bool marked = false;

  unsigned index;
};

struct Arc {
  unsigned v1;
  unsigned v2;
};

Scalar Cross2D(const Vec2& v1, const Vec2& v2);

// Note: Be careful not to copy instances of this class since some of the
// internal data structures depend on pointers to other internal data
// structures. These pointer become invalid if the container is copied around.
// This definitely made coding the skeleton easier but was not necessary a good
// design choice for the skeleton interface.
class Skeleton {
 public:
  Skeleton(const Vec2Vec& vertices, const std::vector<Vec2Vec>& holes);

  // Disallow
  Skeleton(const Skeleton& skeleton) = delete;
  const Skeleton& operator=(const Skeleton& skeleton) = delete;

  Vec2 EdgeDir(const OrigEdge* edge) const;

  // Getters
  const std::deque<Vertex>& vertices() const { return vertices_; }

  const Vec2Vec& positions() const { return positions_; }

  const ScalarVec& distances() const { return distances_; }

  const std::vector<OrigEdge>& edges() const { return edges_; }

  const std::vector<Arc>& skeleton() const { return skeleton_; }

  const IdxVecVec& neighbors() const { return neighbors_; }

 private:
  // Returns false if both directions are parallel.
  bool Bisector(const Vec2& dir1, const Vec2& dir2, Vec2* bisector,
                bool* reflex);
  bool Bisector(Vertex* vertex);

  // Returns true if an intersection was found.
  bool Intersect(const Vec2& pos1, const Vec2& dir1, const Vec2& pos2,
                 const Vec2& dir2, Vec2* inter, Scalar* t) const;

  // Point-line distance. Returns distance from the left side of the line.
  // Dir must be normalized.
  Scalar Distance(const Vec2& x, const Vec2& p, const Vec2& dir) const;

  // from and to are the vertices that we consider to collapse.
  void EdgeEvent(const OrigEdge* e_orig, Vertex* from, Vertex* to);

  void SplitEvent(Vertex* reflex);

  // Finds all opposite edges for a reflex vertex.
  struct OppositeEdge {
    Vec2 b;
    Scalar distance;
    Vertex* y;  // x & y are the notation from Fig. 5.
    Vertex* x;
  };
  typedef std::list<OppositeEdge> OppositeEdgeList;
  void OppositeEdges(const Vertex* reflex, OppositeEdgeList* opposites);

  void FinishOrUpdateEvents(Vertex* vertex);

  // Events
  enum class EventType : char {
    EDGE,
    SPLIT,
  };

  struct Event {
    Vec2 pos;
    EventType type;

    Scalar distance;

    union {
      struct {
        Vertex* va;  // Used for edge events.
        Vertex* vb;  // "
      };
      Vertex* v;  // Used for split events.
    };
  };
  typedef std::list<Event> EventList;

  struct EventCmp {
    bool operator()(const Event& lhs, const Event& rhs);
  };

  typedef std::priority_queue<Event, std::vector<Event>, EventCmp>
      PriorityQueue;

  bool ValidEvent(const Event& event);

  // The vertices are stored in doubly-linked lists. But to not have to deal
  // with GC, we just put them all into a deque (doesn't invalidate pointers if
  // we only add/remove add start/end) and we still have random access.
  std::deque<Vertex> vertices_;

  Vec2Vec positions_;
  ScalarVec distances_;  // Corresponding distances for the vertices.

  std::vector<OrigEdge> edges_;

  std::vector<Arc> skeleton_;
  IdxVecVec neighbors_;

  PriorityQueue queue_;

  // Helper functions
  const Vec2& Pos(const Vertex* vertex) const {
    return positions_[vertex->pos_index];
  }
};

}  // namespace skeleton

}  // namespace geometry

}  // namespace shapeml
