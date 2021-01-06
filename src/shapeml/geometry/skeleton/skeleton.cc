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

#include "shapeml/geometry/skeleton/skeleton.h"
// #define SKELETON_DEBUG_OUTPUT
#include <algorithm>
#include <cfloat>
#ifdef SKELETON_DEBUG_OUTPUT
#include <iostream>
#endif
#include <unordered_set>

namespace shapeml {

namespace geometry {

namespace skeleton {

bool SamePos(const Vec2& lhs, const Vec2& rhs) {
  return fabs((lhs - rhs).squaredNorm()) < EPSILON;
}

Scalar Cross2D(const Vec2& v1, const Vec2& v2) {
  return v1.x() * v2.y() - v1.y() * v2.x();
}

Skeleton::Skeleton(const Vec2Vec& vertices, const std::vector<Vec2Vec>& holes) {
  // Initialize vertices and edges.
  const unsigned count = (unsigned)vertices.size();
  unsigned holes_count = 0;
  for (const auto& hole : holes) {
    holes_count += (unsigned)hole.size();
  }
  vertices_.resize(count + holes_count);
  positions_.resize(count + holes_count);
  distances_.assign(count + holes_count, 0.0);
  edges_.resize(count + holes_count);
  neighbors_.resize(count + holes_count);
  for (unsigned i = 0; i < count; ++i) {
    const unsigned prev = (i - 1 + count) % count;
    const unsigned next = (i + 1) % count;
    vertices_[i].index = i;
    vertices_[i].v_prev = &vertices_[prev];
    vertices_[i].v_next = &vertices_[next];
    vertices_[i].e_prev = &edges_[(i - 1 + count) % count];
    vertices_[i].e_next = &edges_[i];
    vertices_[i].pos_index = i;
    positions_[i] = vertices[i];

    edges_[i].from = &vertices_[i];
    edges_[i].to = vertices_[i].v_next;

    neighbors_[i].push_back(prev);
    neighbors_[i].push_back(next);
  }

  unsigned i = count;
  for (const auto& hole : holes) {
    for (const Vec2& vec : hole) {
      const unsigned hole_count = (unsigned)hole.size();
      const unsigned j = i - count;
      const unsigned prev = count + (j - 1 + hole_count) % hole_count;
      const unsigned next = count + (j + 1) % hole_count;
      vertices_[i].index = i;
      vertices_[i].v_prev = &vertices_[prev];
      vertices_[i].v_next = &vertices_[next];
      vertices_[i].e_prev = &edges_[count + (j - 1 + hole_count) % hole_count];
      vertices_[i].e_next = &edges_[i];
      vertices_[i].pos_index = i;

      positions_[i] = vec;

      edges_[i].from = &vertices_[i];
      edges_[i].to = vertices_[i].v_next;

      neighbors_[i].push_back(prev);
      neighbors_[i].push_back(next);

      ++i;
    }
  }

  // Calculate bisectors.
  for (unsigned i = 0; i < vertices_.size(); ++i) {
    if (!Bisector(&vertices_[i])) {
      vertices_[i].marked = true;
      vertices_[i].v_prev->v_next = vertices_[i].v_next;
      vertices_[i].v_next->v_prev = vertices_[i].v_prev;
    }
  }

  // Fill priority queue.
  // Done a bit differently from the paper, I don't wanna add the same event
  // twice. So we just add one event per edge.
  queue_ = PriorityQueue();
  for (unsigned i = 0; i < vertices_.size(); ++i) {
    const OrigEdge* edge = &edges_[i];
    EdgeEvent(edge, edge->from, edge->to);
    SplitEvent(edge->from);
  }

#ifdef SKELETON_DEBUG_OUTPUT
  std::cout << "Initial events done.\n\n";
#endif

  // Process the queue.
  int n = 0;
  while (!queue_.empty()) {
    if (++n == 20) break;
    EventList same_dist;
    same_dist.push_back(queue_.top());
    queue_.pop();

    // Gather all other events at the same distance.
    while (!queue_.empty() &&
           fabs(queue_.top().distance - same_dist.front().distance) < EPSILON) {
      same_dist.push_back(queue_.top());
      queue_.pop();
    }

#ifdef SKELETON_DEBUG_OUTPUT
    std::cout << "#EVENTS AT SAME DIST: " << same_dist.size() << '\n';
#endif

    // Iterate through chains.
    while (!same_dist.empty()) {
      EventList same_loc;
      same_loc.push_back(same_dist.front());
      same_dist.pop_front();

      EventList::iterator it = same_dist.begin();
      while (it != same_dist.end()) {
        if (SamePos(it->pos, same_loc.front().pos)) {
          same_loc.push_back(*it);
          it = same_dist.erase(it);
        } else {
          ++it;
        }
      }

      // Find all involved edges.
      struct Edge {
        Vertex* from;
        Vertex* to;
        bool operator==(const Edge& rhs) const {
          return from == rhs.from && to == rhs.to;
        }
      };
      struct EdgeHash {
        std::size_t operator()(const Edge& edge) const {
          // Is this a good hash?
          return (std::hash<Vertex*>()(edge.from) ^
                  (std::hash<Vertex*>()(edge.to) << 1));
        }
      };

#ifdef SKELETON_DEBUG_OUTPUT
      std::cout << "#EVENTS AT SAME LOC: " << same_loc.size() << '\n';
#endif

      std::unordered_set<Edge, EdgeHash> edge_set;
      std::unordered_set<unsigned int> checked_reflex_verts;

      for (const Event& event : same_loc) {
        if (ValidEvent(event)) {
#ifdef SKELETON_DEBUG_OUTPUT
          std::cout << "Collect edges for event at " << event.pos.x() << " "
                    << event.pos.y() << " at distance " << event.distance
                    << '\n';
#endif

          if (event.type == EventType::EDGE) {
            edge_set.insert({event.va->v_prev, event.va});
            edge_set.insert({event.va, event.vb});
            edge_set.insert({event.vb, event.vb->v_next});
          } else {
            if (!checked_reflex_verts.insert(event.v->index).second) {
              continue;
            }

            edge_set.insert({event.v->v_prev, event.v});
            edge_set.insert({event.v, event.v->v_next});
            OppositeEdgeList opposites;

#ifdef SKELETON_DEBUG_OUTPUT
            std::cout << "Find opposites for " << event.v->index << '\n';
#endif

            OppositeEdges(event.v, &opposites);
            for (const OppositeEdge& o : opposites) {
              assert(o.distance >= event.distance - EPSILON);
              edge_set.insert({o.y, o.x});
            }
          }
        }
      }
      typedef std::list<Edge> Chain;
      Chain edge_list;
      std::copy(edge_set.begin(), edge_set.end(),
                std::back_inserter(edge_list));

      // Construct chains from the edge list.
      std::list<Chain> chains;
      while (!edge_list.empty()) {
        chains.push_back(Chain());
        Chain& chain = chains.back();
        chain.push_back(edge_list.front());
        edge_list.pop_front();

        Chain::iterator it = edge_list.begin();
        while (it != edge_list.end()) {
          if (it->from == chain.back().to) {
            chain.push_back(*it);
            edge_list.erase(it);
            it = edge_list.begin();
          } else if (it->to == chain.front().from) {
            chain.push_front(*it);
            edge_list.erase(it);
            it = edge_list.begin();
          } else {
            ++it;
          }
        }
      }

      if (chains.empty()) {
        continue;
      }

      // Sort the chains clockwise around the intersection point.
      const Vec2 intersection = same_loc.front().pos;
      struct ChainSort {
        ChainSort(const Skeleton* s, const Vec2& i) : skeleton_(s), inter_(i) {}
        bool operator()(const Chain& c1, const Chain& c2) {
          // I hope my assumption that none of the edges of one chain can
          // 'shadow' other chains is true.
          const Vec2 orient1 =
              (skeleton_->Pos(c1.front().to) - inter_).normalized();
          const Vec2 orient2 =
              (skeleton_->Pos(c2.front().to) - inter_).normalized();
          const Scalar angle1 = atan2(orient1.y(), orient1.x()) + M_PI;
          const Scalar angle2 = atan2(orient2.y(), orient2.x()) + M_PI;
          return angle1 < angle2;
        }
        const Skeleton* skeleton_;
        const Vec2 inter_;
      } chain_sort(this, intersection);
      chains.sort(chain_sort);

      const Scalar distance = same_loc.front().distance;

#ifdef SKELETON_DEBUG_OUTPUT
      std::cout << "Handle event at distance " << distance;
      for (const auto& c : chains) {
        std::cout << " [";
        for (const auto& cc : c) {
          std::cout << cc.from->index << " ";
        }
        std::cout << c.back().to->index << "]";
      }
      std::cout << " at point " << intersection.x() << ", " << intersection.y()
                << ". Adding " << chains.size() << " vertices starting at "
                << vertices_.size() << ".\n";
#endif

      const unsigned new_idx = (unsigned)positions_.size();
      positions_.push_back(intersection);
      distances_.push_back(distance);
      neighbors_.push_back(IdxVec());

      // Handle the chains.
      for (auto cit = chains.begin(); cit != chains.end(); ++cit) {
        const Chain& chain = *cit;
        auto cit2 = cit;
        ++cit2;
        const Chain& next_chain =
            (cit2 == chains.end()) ? chains.front() : *cit2;

        // Create new vertex.
        vertices_.push_back(Vertex());
        Vertex* new_vertex = &vertices_.back();
        new_vertex->index = (unsigned)vertices_.size() - 1;
        new_vertex->pos_index = new_idx;

        // Output arcs and mark vertices as used.
        // There's always at least one element per chain.
        assert(chain.size() > 0);
        for (auto jt = ++chain.begin(); jt != chain.end(); ++jt) {
          jt->from->marked = true;
          skeleton_.push_back({new_idx, jt->from->pos_index});
          neighbors_[new_idx].push_back(jt->from->pos_index);
          neighbors_[jt->from->pos_index].push_back(new_idx);

#ifdef SKELETON_DEBUG_OUTPUT
          std::cout << "- Add arc: " << new_idx << " -> " << jt->from->pos_index
                    << '\n';
#endif
        }

        // If we have a loop, close it.
        if (chain.front().from == chain.back().to) {
          assert(chains.size() == 1);
          chain.front().from->marked = true;
          const unsigned idx = chain.front().from->pos_index;
          skeleton_.push_back({new_idx, idx});
          neighbors_[new_idx].push_back(idx);
          neighbors_[idx].push_back(new_idx);
          new_vertex->marked = true;

#ifdef SKELETON_DEBUG_OUTPUT
          std::cout << "Closing a loop.\n";
          std::cout << "- Add arc: " << new_idx << " -> " << idx << '\n';
#endif
        } else {
          // Link the new vertex into the LAV.
          new_vertex->v_prev = next_chain.front().from;
          next_chain.front().from->v_next = new_vertex;
          new_vertex->v_next = chain.back().to;
          chain.back().to->v_prev = new_vertex;
          new_vertex->e_prev = new_vertex->v_prev->e_next;
          new_vertex->e_next = new_vertex->v_next->e_prev;

          if (!Bisector(new_vertex)) {
            new_vertex->marked = true;
            new_vertex->v_prev->v_next = new_vertex->v_next;
            new_vertex->v_next->v_prev = new_vertex->v_prev;
          }
          FinishOrUpdateEvents(new_vertex);
        }
      }  // Same chain
    }    // Same location
  }      // Same distance

#ifdef SKELETON_DEBUG_OUTPUT
  std::cout << "# of arcs: " << skeleton_.size() << "\n\n";
#endif
}

Vec2 Skeleton::EdgeDir(const OrigEdge* edge) const {
  return (Pos(edge->to) - Pos(edge->from)).normalized();
}

// TODO(stefalie): Implement weighted skeleton.
bool Skeleton::Bisector(const Vec2& dir1, const Vec2& dir2, Vec2* bisector,
                        bool* reflex) {
  const Vec2 n1 = Vec2(dir1.y(), -dir1.x());
  const Vec2 n2 = Vec2(-dir2.y(), dir2.x());

  // Both edges are parallel.
  if (n1.dot(n2) > 1.0 - EPSILON) {
    *bisector = Vec2(0.0, 0.0);
    *reflex = false;  // Probably doesn't matter.
    return false;
  } else if (-n1.dot(n2) > 1.0 - EPSILON) {
    *bisector =
        (dir1 + dir2)
            .normalized();  // Just returning one of them should be ok too.
    *reflex = false;
    return true;
  }

  // This could be simplified, but it might be more stable numerically to keep
  // it that way.
  if (Cross2D(dir2, dir1) >= 0.0) {
    *bisector = ((dir1 + n1) + (dir2 + n2)).normalized();
    *reflex = false;
  } else {
    *bisector = -((dir1 - n1) + (dir2 - n2)).normalized();
    *reflex = true;
  }
  return true;
}

bool Skeleton::Bisector(Vertex* vertex) {
  const Vec2 edge_to_prev = -EdgeDir(vertex->e_prev);
  const Vec2 edge_to_next = EdgeDir(vertex->e_next);
  return Bisector(edge_to_prev, edge_to_next, &vertex->bisector,
                  &vertex->reflex);
}

bool Skeleton::Intersect(const Vec2& pos1, const Vec2& dir1, const Vec2& pos2,
                         const Vec2& dir2, Vec2* inter, Scalar* t) const {
  // Find intersection of both lines.
  // http://stackoverflow.com/questions/563198/how-do-you-detect-where-two-line-segments-intersect
  const Scalar denom = Cross2D(dir1, dir2);
  if (fabs(denom) < EPSILON) {
    return false;
  }

  *t = Cross2D(pos2 - pos1, dir2) / denom;
  *inter = pos1 + *t * dir1;
  return true;
}

Scalar Skeleton::Distance(const Vec2& x, const Vec2& p, const Vec2& dir) const {
  const Vec2 norm(-dir.y(), dir.x());
  return (x - p).dot(norm);
}

void Skeleton::EdgeEvent(const OrigEdge* e_orig, Vertex* from, Vertex* to) {
  Vec2 inter;
  Scalar t1, t2;

  // The double intersection test here is likely a overkill.
  if (Intersect(Pos(from), from->bisector, Pos(to), to->bisector, &inter,
                &t1) &&
      Intersect(Pos(to), to->bisector, Pos(from), from->bisector, &inter,
                &t2)) {
    if (t1 < 0.0 || t2 < 0.0) {  // TODO(stefalie): Use -EPSILON instead?
      return;
    }
    Event event;
    event.pos = inter;
    event.type = EventType::EDGE;
    event.distance = Distance(event.pos, Pos(e_orig->from), EdgeDir(e_orig));
    assert(event.distance > 0.0);
    event.va = from;
    event.vb = to;
    queue_.push(event);

#ifdef SKELETON_DEBUG_OUTPUT
    std::cout << "- Event: edge (" << from->index << ", " << to->index << "), "
              << "dist: " << event.distance << ", pos: " << event.pos.x() << ' '
              << event.pos.y() << '\n';
#endif
  }
}

void Skeleton::SplitEvent(Vertex* reflex) {
  // If we have a reflex vertex, also calculate split events.
  if (reflex->reflex) {
    OppositeEdgeList opposites;
    OppositeEdges(reflex, &opposites);

    // Would be sufficient to keep only the closest one(s).
    Event event;
    event.type = EventType::SPLIT;
    for (const OppositeEdge& o : opposites) {
      event.distance = o.distance;
      event.pos = o.b;
      event.v = reflex;
      queue_.push(event);

#ifdef SKELETON_DEBUG_OUTPUT
      std::cout << "- Event: split (" << o.y->index << ", " << o.x->index
                << ") of vertex " << reflex->index
                << ", dist: " << event.distance << ", pos: " << event.pos.x()
                << " " << event.pos.y() << '\n';
#endif
    }
  }
}

void Skeleton::OppositeEdges(const Vertex* reflex,
                             OppositeEdgeList* opposites) {
  opposites->clear();
  assert(reflex->reflex);
  const Vec2& v_pos = Pos(reflex);
  const Vec2 v_dir1 = EdgeDir(reflex->e_prev);
  const Vec2 v_dir2 = EdgeDir(reflex->e_next);

  OppositeEdge opp;
  for (unsigned i = 0; i < vertices_.size(); ++i) {
    opp.y = &vertices_[(i + reflex->index) % vertices_.size()];
    opp.x = opp.y->v_next;

    if (opp.y->marked || opp.y == reflex || opp.y == reflex->v_prev) {
      continue;
    }

    const OrigEdge* edge = opp.y->e_next;
    const Vec2& e_pos = Pos(edge->from);
    const Vec2 e_dir = EdgeDir(edge);

    Vec2 inter;
    Vec2 bisector;
    Scalar t;
    bool has_inter = false;
    bool tmp;

    // Check if the edge is behind v. This might not be necessary at all but
    // it's suggested in the paper.
    if (Intersect(v_pos, reflex->bisector, e_pos, e_dir, &inter, &t)) {
      if (t < 0.0) {
        continue;
      }
    }

    // Find intersection with 'opposite' edge.
    // It's possible that one of v's edges is parallel to the opposite, and
    // therefore we have to check both.
    if (Intersect(v_pos, -v_dir1, e_pos, e_dir, &inter, &t)) {
      Bisector(-v_dir1, e_dir, &bisector, &tmp);
      has_inter = true;
    } else if (Intersect(v_pos, v_dir2, e_pos, e_dir, &inter, &t)) {
      Bisector(-e_dir, v_dir2, &bisector, &tmp);
      has_inter = true;
    }

    if (has_inter) {
      if (!Intersect(v_pos, reflex->bisector, inter, bisector, &opp.b, &t)) {
        continue;
      }

      if (Distance(opp.b, Pos(opp.y), opp.y->bisector) > EPSILON) {
        continue;
      }

      if (Distance(opp.b, Pos(opp.x), opp.x->bisector) < -EPSILON) {
        continue;
      }

      if (Distance(opp.b, Pos(opp.y), e_dir) < -EPSILON) {
        continue;
      }

      opp.distance = Distance(opp.b, e_pos, e_dir);
      if (opp.distance < -EPSILON) {
        continue;
      }

      // If one or both of the two bisectors of the opposite edge is co-linear
      // with the edge itself, we have a degenrate case, and we should check
      // that the point B itself lies on the edge. (Check that B is not only on
      // the line defined by Y and X, but also that B is actually between Y and
      // X.)
      const Vec2 e_norm(-e_dir.y(), e_dir.x());
      if (fabs(opp.y->bisector.dot(e_dir)) > 1.0 - EPSILON &&
          Distance(opp.b, Pos(opp.y), e_norm) > EPSILON) {
        continue;
      }
      if (fabs(opp.x->bisector.dot(e_dir)) > 1.0 - EPSILON &&
          Distance(opp.b, Pos(opp.x), e_norm) < -EPSILON) {
        continue;
      }

#ifdef SKELETON_DEBUG_OUTPUT
      std::cout << "\tFound opposite edge (" << opp.y->index << ", "
                << opp.x->index << ") of vertex " << reflex->index << " at "
                << "dist " << opp.distance << ".\n";
#endif

      opposites->push_back(opp);
    }
  }

  // Only keep the opposites edges with the min distance.
  Scalar min_dist = DBL_MAX;
  for (const auto& o : *opposites) {
    if (o.distance < min_dist) {
      min_dist = o.distance;
    }
  }

  OppositeEdgeList::iterator it = opposites->begin();
  while (it != opposites->end()) {
    if (it->distance - min_dist > EPSILON) {
      it = opposites->erase(it);
    } else {
      ++it;
    }
  }

#ifndef NDEBUG
  for (const OppositeEdge& o : *opposites) {
    assert(fabs(opposites->front().distance - o.distance) < EPSILON);
    assert(SamePos(opposites->front().b, o.b));
  }
#endif
}

void Skeleton::FinishOrUpdateEvents(Vertex* vertex) {
  // If the LAV has only two elements left, we add one more arc and are
  // done. Otherwise we add new edge events.
  if (vertex->v_prev->v_prev == vertex) {
#ifdef SKELETON_DEBUG_OUTPUT
    std::cout << "Only two vertices left in LAV, finish it.\n";
    std::cout << "- Add arc: " << vertex->pos_index << " -> "
              << vertex->v_prev->pos_index << '\n';
#endif

    skeleton_.push_back({vertex->pos_index, vertex->v_prev->pos_index});
    neighbors_[vertex->pos_index].push_back(vertex->v_prev->pos_index);
    neighbors_[vertex->v_prev->pos_index].push_back(vertex->pos_index);
    vertex->marked = vertex->v_prev->marked = true;
  } else {
    // Create new events.
    EdgeEvent(vertex->e_prev, vertex->v_prev, vertex);
    EdgeEvent(vertex->e_next, vertex, vertex->v_next);
    SplitEvent(vertex);
  }
}

bool Skeleton::EventCmp::operator()(const Event& lhs, const Event& rhs) {
  return lhs.distance > rhs.distance;
}

bool Skeleton::ValidEvent(const Event& event) {
  if ((event.type == EventType::SPLIT && event.v->marked) ||
      (event.type == EventType::EDGE &&
       (event.va->marked || event.vb->marked))) {
#ifdef SKELETON_DEBUG_OUTPUT
    const bool split_event = event.type == EventType::SPLIT;
    std::cout << "Invalid " << (split_event ? "split" : "edge")
              << " event at vertex "
              << (split_event ? event.v->index : event.va->index) << ".\n";
#endif

    return false;
  } else {
    return true;
  }
}

}  // namespace skeleton

}  // namespace geometry

}  // namespace shapeml
