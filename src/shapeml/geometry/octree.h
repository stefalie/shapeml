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

#include <map>
#include <vector>

#include "shapeml/geometry/ray_plane_bv.h"

namespace shapeml {

namespace geometry {

class Octree;

class OctreeElement {
 public:
  explicit OctreeElement(const AABB& aabb) : aabb_(aabb) {}

  // If you copy an octree element, the new element will not be in the octree.
  // This is to prevent that two elements share the same entry in the octree.
  // TODO(stefalie): Consider making a new entry in the octree for the copy,
  // maybe that would be a cleaner solution. Or maybe derive random classes from
  // OctreeElement was a terrible idea in the first place.
  OctreeElement(const OctreeElement& other);
  OctreeElement& operator=(const OctreeElement& other);

  virtual ~OctreeElement() = default;

  void RemoveFromOctree();

  const Octree* octree_cell() const { return octree_cell_; }

 private:
  AABB aabb_;

  Octree* octree_cell_ = nullptr;

  friend class Octree;
};

class Octree {
 public:
  Octree(const Vec3& center, double half_width);
  ~Octree();

  // Precondition is that the element is contained in the octree.
  void InsertObject(OctreeElement* elem);

  void IntersectWith(const AABB& elem,
                     std::vector<OctreeElement*>* candidates) const;

  int NumElements() const;

  void CalcStat(std::map<int, int>* count_per_level) const;

 private:
  void RemoveObject(OctreeElement* elem);

  void InsertObjectRec(OctreeElement* elem);
  void AddKid(unsigned idx);
  void DeleteInvalidBottomUp();

  Vec3 center_;
  double half_width_;

  Octree* children_[8];

  std::vector<OctreeElement*> objects_;

  unsigned level_ = 0;

  const bool LOOSE_OCTREE = true;
  const unsigned MAX_LEVEL = 15;

  // Used for cleaning up empty nodes.
  Octree* parent_ = nullptr;
  int kid_idx_ = -1;

  friend class OctreeElement;
};

}  // namespace geometry

}  // namespace shapeml
