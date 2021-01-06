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

#include "shapeml/geometry/octree.h"

namespace shapeml {

namespace geometry {

OctreeElement::OctreeElement(const OctreeElement& other) {
  aabb_ = other.aabb_;
  octree_cell_ = nullptr;
}

OctreeElement& OctreeElement::operator=(const OctreeElement& other) {
  aabb_ = other.aabb_;
  octree_cell_ = nullptr;
  return *this;
}

void OctreeElement::RemoveFromOctree() {
  if (octree_cell_) {
    octree_cell_->RemoveObject(this);
    octree_cell_ = nullptr;
  }
}

Octree::Octree(const Vec3& center, double half_width)
    : center_(center), half_width_(half_width) {
  std::fill_n(children_, 8, nullptr);
}

Octree::~Octree() {
  for (Octree* c : children_) {
    if (c) {
      delete c;
    }
  }
  for (OctreeElement* e : objects_) {
    delete e;
  }
}

void Octree::InsertObject(OctreeElement* elem) {
  // TODO(stefalie):
  // Safety check to make sure that elem is contained within the octree cell. If
  // not, the element is simply added to the root (that also means that this
  // method here should only be called on the root). I never tested this, so
  // it's not in the code.
  //
  // Vec3 min = elem->aabb_.center - elem->aabb_.extent;
  // Vec3 max = elem->aabb_.center + elem->aabb_.extent;
  //
  // const double loose_offset = LOOSE_OCTREE ? 0.5 * half_width_ : 0.0;
  // for (int i = 0; i < 3; ++i) {
  //   if (max[i] > center_[i] + half_width + loose_offset ||
  //       min[i] < center_[i] - half_width - loose_offset) {
  //     objects_.push_back(elem);
  //     elem->octree_cell_ = this;
  //     return;
  //   }
  // }

  InsertObjectRec(elem);
}

void Octree::IntersectWith(const AABB& elem,
                           std::vector<OctreeElement*>* candidates) const {
  // Intersect with elements at this level.
  for (const auto& o : objects_) {
    if (elem.Intersect(o->aabb_)) {
      candidates->push_back(o);
    }
  }

  // Visit children.
  Vec3 min = elem.center - elem.extent;
  Vec3 max = elem.center + elem.extent;
  const double loose_offset = LOOSE_OCTREE ? 0.5 * half_width_ : 0.0;

  for (int i = 0; i < 8; ++i) {
    if (children_[i]) {
      bool check_cell = true;

      // Check overlap.
      for (int j = 0; j < 3; ++j) {
        if ((i >> j) & 1) {
          if (max[j] < center_[j] - loose_offset) {
            check_cell = false;
            break;
          }
        } else {
          if (min[j] > center_[j] + loose_offset) {
            check_cell = false;
            break;
          }
        }
      }

      if (check_cell) {
        children_[i]->IntersectWith(elem, candidates);
      }
    }
  }
}

int Octree::NumElements() const {
  int ret = static_cast<int>(objects_.size());
  for (const auto& c : children_) {
    if (c) {
      ret += c->NumElements();
    }
  }
  return ret;
}

void Octree::CalcStat(std::map<int, int>* count_per_level) const {
  count_per_level->at(level_) += static_cast<int>(objects_.size());
  for (const auto& c : children_) {
    if (c) {
      c->CalcStat(count_per_level);
    }
  }
}

void Octree::InsertObjectRec(OctreeElement* elem) {
  unsigned char index = 0;
  bool straddle = false;

  const double loose_offset = LOOSE_OCTREE ? 0.5 * half_width_ : 0.0;
  for (int i = 0; i < 3; ++i) {
    const double delta = elem->aabb_.center[i] - center_[i];

    // This test is only valid if it's guaranteed that the element is inside
    // the octree cell.
    if (fabs(delta) + loose_offset <= elem->aabb_.extent[i]) {
      straddle = true;
      break;
    }

    if (delta > 0.0) {
      index |= (1 << i);
    }
  }

  if (!straddle && level_ < MAX_LEVEL) {
    if (!children_[index]) {
      AddKid(index);
    }
    return children_[index]->InsertObjectRec(elem);
  } else {
    objects_.push_back(elem);
    elem->octree_cell_ = this;
  }
}

void Octree::RemoveObject(OctreeElement* elem) {
  for (auto it = objects_.begin(); it != objects_.end(); ++it) {
    if (*it == elem) {
      objects_.erase(it);
      elem->octree_cell_ = nullptr;

      DeleteInvalidBottomUp();
      return;
    }
  }
  assert(false);
}

void Octree::AddKid(unsigned idx) {
  assert(idx < 8);

  const double step = half_width_ * 0.5;
  Vec3 offset(0.0, 0.0, 0.0);
  offset.x() = (idx & 1) ? step : -step;
  offset.y() = (idx & 2) ? step : -step;
  offset.z() = (idx & 4) ? step : -step;

  children_[idx] = new Octree(center_ + offset, step);
  children_[idx]->level_ = level_ + 1;
  children_[idx]->parent_ = this;
  children_[idx]->kid_idx_ = idx;
}

void Octree::DeleteInvalidBottomUp() {
  Octree* curr = this;

  while (curr) {
    if (curr->objects_.size() > 0) {
      return;
    }

    for (int i = 0; i < 8; ++i) {
      if (curr->children_[i]) {
        return;
      }
    }

    Octree* tmp = curr;
    curr = curr->parent_;
    if (curr) {
      // Don't delete the root.
      curr->children_[tmp->kid_idx_] = nullptr;
      delete tmp;
    }
  }
}

}  // namespace geometry

}  // namespace shapeml
