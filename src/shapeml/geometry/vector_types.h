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
#include <vector>

namespace shapeml {

// The following numeric types are used so often that we put them directly into
// the shapeml namespace (and not in shapeml::geometry).

typedef double Scalar;

typedef Eigen::Vector2d Vec2;
typedef Eigen::Vector3d Vec3;
typedef Eigen::Vector4d Vec4;
typedef Eigen::Matrix3d Mat3;
typedef Eigen::Matrix4d Mat4;
typedef Eigen::Affine2d Affine2;
typedef Eigen::Affine3d Affine3;
typedef Eigen::Isometry3d Isometry3;

typedef Eigen::Vector2f Vec2f;
typedef Eigen::Vector3f Vec3f;
typedef Eigen::Vector4f Vec4f;
typedef Eigen::Matrix4f Mat4f;

typedef Eigen::Vector2i Vec2i;
typedef Eigen::Vector3i Vec3i;

typedef Eigen::Quaterniond Quaternion;

typedef std::vector<uint32_t> IdxVec;
typedef std::vector<std::vector<uint32_t>> IdxVecVec;

typedef std::vector<Scalar> ScalarVec;
typedef std::vector<Vec2> Vec2Vec;
typedef std::vector<Vec3> Vec3Vec;

namespace geometry {

const Scalar EPSILON = 0.0000001;

}  // namespace geometry

}  // namespace shapeml
