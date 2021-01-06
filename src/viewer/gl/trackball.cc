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

#include "viewer/gl/trackball.h"

#include <cmath>

namespace viewer {

namespace gl {

Trackball::Trackball() : radius_(1.0f) {}

Trackball::Trackball(float radius) : radius_(radius) {}

void Trackball::BeginDrag(float x, float y) {
  incremental_rotation_ = Eigen::Matrix4f::Identity();

  anchor_pos_ = Eigen::Vector3f(x, y, 0.0f);
  ProjectOntoSurface(&anchor_pos_);

  current_pos_ = anchor_pos_;
}

void Trackball::Drag(float x, float y) {
  current_pos_ = Eigen::Vector3f(x, y, 0.0f);
  ProjectOntoSurface(&current_pos_);
  ComputeIncremental();
}

void Trackball::ProjectOntoSurface(Eigen::Vector3f* p) const {
  const float radius_squared = radius_ * radius_;
  const float length_squared = p->x() * p->x() + p->y() * p->y();

  if (length_squared <= radius_squared * 0.5f) {
    p->z() = std::sqrt(radius_squared - length_squared);
  } else {
    p->z() = radius_squared / (2.0f * std::sqrt(length_squared));
  }
  float length = std::sqrt(length_squared + p->z() * p->z());
  *p /= length;
}

void Trackball::ComputeIncremental() {
  if (current_pos_ == anchor_pos_) {
    incremental_rotation_ = Eigen::Matrix4f::Identity();
    return;
  }

  const float angle_boost = 80.0f / 180.0f * static_cast<float>(M_PI);

  Eigen::Vector3f axis = anchor_pos_.cross(current_pos_);
  const float angle =
      angle_boost * atan2(axis.norm(), anchor_pos_.dot(current_pos_));
  axis.normalize();
  const Eigen::Matrix3f rot = Eigen::AngleAxisf(angle, axis).toRotationMatrix();
  incremental_rotation_.block(0, 0, 3, 3) = rot;
}

}  // namespace gl

}  // namespace viewer
