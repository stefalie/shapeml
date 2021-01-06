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

#include <Eigen/Dense>

namespace viewer {

namespace gl {

// The following trackball class is heavily inspired by:
// http://image.diku.dk/research/trackballs/index.html.
class Trackball {
 public:
  Trackball();
  explicit Trackball(float radius);

  const Eigen::Matrix4f& incremental_rotation() const {
    return incremental_rotation_;
  }

  void BeginDrag(float x, float y);
  void Drag(float x, float y);

 private:
  void ProjectOntoSurface(Eigen::Vector3f* p) const;

  void ComputeIncremental();

  float radius_;
  Eigen::Vector3f anchor_pos_;
  Eigen::Vector3f current_pos_;
  Eigen::Matrix4f incremental_rotation_;
};

}  // namespace gl

}  // namespace viewer
