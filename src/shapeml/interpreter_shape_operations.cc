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

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_set>

#include "shapeml/asset_cache.h"
#include "shapeml/evaluator.h"
#include "shapeml/geometry/primitives.h"
#include "shapeml/interpreter.h"
#include "shapeml/shape.h"
#include "shapeml/util/hex_color.h"

namespace shapeml {

template <>
const char* ShapeOpEvaluator::type_name_ = "shape operation";

template class Evaluator<DerivationContext>;

// The rest of this file contains the evaluators for all shape operations.

REGISTER_SHAPE_OP(OpEval_translate, "translate", 3,
                  {ValueType::FLOAT, ValueType::FLOAT, ValueType::FLOAT});
struct OpEval_translate : ShapeOpEvaluator {
  bool Eval() final {
    context_->shape_stack->Top()->Translate(
        Vec3(args_->at(0).f, args_->at(1).f, args_->at(2).f));
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_translateX, "translateX", 1, {ValueType::FLOAT});
struct OpEval_translateX : ShapeOpEvaluator {
  bool Eval() final {
    context_->shape_stack->Top()->TranslateX(args_->at(0).f);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_translateY, "translateY", 1, {ValueType::FLOAT});
struct OpEval_translateY : ShapeOpEvaluator {
  bool Eval() final {
    context_->shape_stack->Top()->TranslateY(args_->at(0).f);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_translateZ, "translateZ", 1, {ValueType::FLOAT});
struct OpEval_translateZ : ShapeOpEvaluator {
  bool Eval() final {
    context_->shape_stack->Top()->TranslateZ(args_->at(0).f);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_translateAbs, "translateAbs", 3,
                  {ValueType::FLOAT, ValueType::FLOAT, ValueType::FLOAT});
struct OpEval_translateAbs : ShapeOpEvaluator {
  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();
    shape->Translate(-shape->WorldTrafo().translation() +
                     Vec3(args_->at(0).f, args_->at(1).f, args_->at(2).f));
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_translateAbsX, "translateAbsX", 1, {ValueType::FLOAT});
struct OpEval_translateAbsX : ShapeOpEvaluator {
  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();
    shape->TranslateX(-shape->WorldTrafo().translation().x() + args_->at(0).f);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_translateAbsY, "translateAbsY", 1, {ValueType::FLOAT});
struct OpEval_translateAbsY : ShapeOpEvaluator {
  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();
    shape->TranslateY(-shape->WorldTrafo().translation().y() + args_->at(0).f);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_translateAbsZ, "translateAbsZ", 1, {ValueType::FLOAT});
struct OpEval_translateAbsZ : ShapeOpEvaluator {
  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();
    shape->TranslateZ(-shape->WorldTrafo().translation().z() + args_->at(0).f);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_size, "size", 3,
                  {ValueType::FLOAT, ValueType::FLOAT, ValueType::FLOAT});
struct OpEval_size : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    for (unsigned i = 0; i < args_->size(); ++i) {
      if (CheckGreaterEqualThanZero(i)) {
        return true;
      }
    }
    return false;
  }

  bool Eval() final {
    context_->shape_stack->Top()->Size(
        Vec3(args_->at(0).f, args_->at(1).f, args_->at(2).f));
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_sizeX, "sizeX", 1, {ValueType::FLOAT});
struct OpEval_sizeX : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckGreaterEqualThanZero(0)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    context_->shape_stack->Top()->SizeX(args_->at(0).f);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_sizeY, "sizeY", 1, {ValueType::FLOAT});
struct OpEval_sizeY : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckGreaterEqualThanZero(0)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    context_->shape_stack->Top()->SizeY(args_->at(0).f);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_sizeZ, "sizeZ", 1, {ValueType::FLOAT});
struct OpEval_sizeZ : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckGreaterEqualThanZero(0)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    context_->shape_stack->Top()->SizeZ(args_->at(0).f);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_scale, "scale", -1, {});
struct OpEval_scale : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckArgNumber({1, 3})) {
      return true;
    }

    for (unsigned i = 0; i < args_->size(); ++i) {
      if (ValidateType(ValueType::FLOAT, i) || CheckGreaterEqualThanZero(i)) {
        return true;
      }
    }
    return false;
  }

  bool Eval() final {
    if (args_->size() == 3) {
      context_->shape_stack->Top()->Scale(
          Vec3(args_->at(0).f, args_->at(1).f, args_->at(2).f));
    } else {  // if (args_->size() == 1)
      const Scalar factor = args_->at(0).f;
      context_->shape_stack->Top()->Scale(Vec3(factor, factor, factor));
    }
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_scaleX, "scaleX", 1, {ValueType::FLOAT});
struct OpEval_scaleX : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckGreaterEqualThanZero(0)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    context_->shape_stack->Top()->ScaleX(args_->at(0).f);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_scaleY, "scaleY", 1, {ValueType::FLOAT});
struct OpEval_scaleY : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckGreaterEqualThanZero(0)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    context_->shape_stack->Top()->ScaleY(args_->at(0).f);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_scaleZ, "scaleZ", 1, {ValueType::FLOAT});
struct OpEval_scaleZ : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckGreaterEqualThanZero(0)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    context_->shape_stack->Top()->ScaleZ(args_->at(0).f);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_scaleCenter, "scaleCenter", -1, {});
struct OpEval_scaleCenter : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckArgNumber({1, 3})) {
      return true;
    }

    for (unsigned i = 0; i < args_->size(); ++i) {
      if (ValidateType(ValueType::FLOAT, i) || CheckGreaterEqualThanZero(i)) {
        return true;
      }
    }
    return false;
  }

  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();
    Vec3 size;

    if (args_->size() == 3) {
      size = Vec3(args_->at(0).f, args_->at(1).f, args_->at(2).f);
    } else {  // if (args_->size() == 1)
      const Scalar factor = args_->at(0).f;
      size = Vec3(factor, factor, factor);
    }

    const Vec3 translation =
        (0.5 * shape->Size()).cwiseProduct(Vec3::Ones() - size);
    shape->Translate(translation);
    shape->Scale(size);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_scaleCenterX, "scaleCenterX", 1, {ValueType::FLOAT});
struct OpEval_scaleCenterX : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckGreaterEqualThanZero(0)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();
    const Scalar size = args_->at(0).f;
    shape->TranslateX(0.5 * shape->SizeX() * (1.0 - size));
    shape->ScaleX(size);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_scaleCenterY, "scaleCenterY", 1, {ValueType::FLOAT});
struct OpEval_scaleCenterY : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckGreaterEqualThanZero(0)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();
    const Scalar size = args_->at(0).f;
    shape->TranslateY(0.5 * shape->SizeY() * (1.0 - size));
    shape->ScaleY(size);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_scaleCenterZ, "scaleCenterZ", 1, {ValueType::FLOAT});
struct OpEval_scaleCenterZ : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckGreaterEqualThanZero(0)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();
    const Scalar size = args_->at(0).f;
    shape->TranslateZ(0.5 * shape->SizeZ() * (1.0 - size));
    shape->ScaleZ(size);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_rotate, "rotate", 4,
                  {ValueType::FLOAT, ValueType::FLOAT, ValueType::FLOAT,
                   ValueType::FLOAT});
struct OpEval_rotate : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckDirectionVector(0)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    const Vec3 axis(args_->at(0).f, args_->at(1).f, args_->at(2).f);
    context_->shape_stack->Top()->Rotate(axis, args_->at(3).f);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_rotateX, "rotateX", 1, {ValueType::FLOAT});
struct OpEval_rotateX : ShapeOpEvaluator {
  bool Eval() final {
    context_->shape_stack->Top()->RotateX(args_->at(0).f);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_rotateY, "rotateY", 1, {ValueType::FLOAT});
struct OpEval_rotateY : ShapeOpEvaluator {
  bool Eval() final {
    context_->shape_stack->Top()->RotateY(args_->at(0).f);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_rotateZ, "rotateZ", 1, {ValueType::FLOAT});
struct OpEval_rotateZ : ShapeOpEvaluator {
  bool Eval() final {
    context_->shape_stack->Top()->RotateZ(args_->at(0).f);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_rotateScope, "rotateScope", 4,
                  {ValueType::FLOAT, ValueType::FLOAT, ValueType::FLOAT,
                   ValueType::FLOAT});
struct OpEval_rotateScope : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckDirectionVector(0) ||
        CheckNonEmptyMesh(context_->shape_stack->Top())) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    const Vec3 axis(args_->at(0).f, args_->at(1).f, args_->at(2).f);
    context_->shape_stack->Top()->RotateScope(axis, args_->at(3).f);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_rotateScopeX, "rotateScopeX", 1, {ValueType::FLOAT});
struct OpEval_rotateScopeX : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckNonEmptyMesh(context_->shape_stack->Top())) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    context_->shape_stack->Top()->RotateScope(Vec3::UnitX(), args_->at(0).f);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_rotateScopeY, "rotateScopeY", 1, {ValueType::FLOAT});
struct OpEval_rotateScopeY : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckNonEmptyMesh(context_->shape_stack->Top())) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    context_->shape_stack->Top()->RotateScope(Vec3::UnitY(), args_->at(0).f);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_rotateScopeZ, "rotateScopeZ", 1, {ValueType::FLOAT});
struct OpEval_rotateScopeZ : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckNonEmptyMesh(context_->shape_stack->Top())) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    context_->shape_stack->Top()->RotateScope(Vec3::UnitZ(), args_->at(0).f);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_rotateScopeXYToXZ, "rotateScopeXYToXZ", 0, {});
struct OpEval_rotateScopeXYToXZ : ShapeOpEvaluator {
  bool Eval() final {
    context_->shape_stack->Top()->RotateScopeXYToXZ();
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_rotateXAxisHorizontal, "rotateZAxisHorizontal", 0, {});
struct OpEval_rotateXAxisHorizontal : ShapeOpEvaluator {
  // See page 57 of the book 'The Algorithmic Beauty of Plants'.
  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();

    Vec3 local_unit_y = Vec3::UnitY();
    if (shape->parent()) {
      local_unit_y = shape->parent()->WorldTrafo().linear().row(1);
    }

    Mat3 rotation = shape->RotationAsMatrix();
    const Vec3& y_axis = rotation.col(1);

    Vec3 new_z_axis = local_unit_y.cross(y_axis);
    if (new_z_axis.squaredNorm() > geometry::EPSILON) {
      new_z_axis.normalize();

      rotation.col(0) = y_axis.cross(new_z_axis);
      // The y-axis stays the same.
      rotation.col(2) = new_z_axis;
      shape->SetRotation(rotation);
    }

    // Below is the old version that doesn't require SetRotation(...).
    //
    // const Mat3& rotation = shape->WorldTrafo().linear();
    // const Vec3 y_axis = rotation.col(1);
    // const Vec3 z_axis = rotation.col(2);
    //
    // Vec3 new_z_axis = Vec3::UnitY().cross(y_axis);
    //
    // // Instead of directly resetting the coordinate frame of the shape as in
    // // the book (which we can't direclty access and overwrite from here), we
    // // calculate how much we have to rotate around the y-axis so that the
    // // current x-axis becomes the new x-axis.
    // if (new_z_axis.squaredNorm() > geometry::EPSILON) {
    //   new_z_axis.normalize();
    //
    //   Scalar cos_angle = z_axis.dot(new_z_axis);
    //   if (cos_angle < 1.0 - geometry::EPSILON) {
    //     // Clamp cos_angle to prevent numerical imprecisions.
    //     cos_angle = std::max(std::min(cos_angle, 1.0), -1.0);
    //
    //     Scalar angle = std::acos(cos_angle);
    //
    //     // Find which direction to rotate in, i.e., the sign of the angle.
    //     if (y_axis.dot(z_axis.cross(new_z_axis)) < 0.0) {
    //       angle = -angle;
    //     }
    //
    //     shape->RotateY(angle * 180.0 / M_PI);
    //   }
    // }

    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_rotateHorizontal, "rotateHorizontal", 0, {});
struct OpEval_rotateHorizontal : ShapeOpEvaluator {
  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();
    const Vec3 up_local =
        shape->WorldTrafo().linear().transpose() * Vec3::UnitY();
    Scalar cos_angle =
        up_local.y();  // Identical to: up_local.dot(Vec3::UnitY());

    if (cos_angle < 1.0 - geometry::EPSILON) {
      // Clamp cos_angle to prevent numerical imprecisions.
      cos_angle = std::max(std::min(cos_angle, 1.0), -1.0);
      const Scalar angle = std::acos(cos_angle) / M_PI * 180.0;
      const Vec3 axis = Vec3::UnitY().cross(up_local);
      shape->Rotate(axis, angle);
    }
    return false;
  }
};

// This shape operation is used for L-systems used in:
// Talton et al., 2011, Metropolis Procedural Modeling.
// See: https://github.com/Sunwinds/metropolis-procedural-modeling
REGISTER_SHAPE_OP(OpEval_turnYAxisToVec, "turnYAxisToVec", 3,
                  {ValueType::FLOAT, ValueType::FLOAT, ValueType::FLOAT});
struct OpEval_turnYAxisToVec : ShapeOpEvaluator {
  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();

    Vec3 param_local(args_->at(0).f, args_->at(1).f, args_->at(2).f);
    if (shape->parent()) {
      param_local =
          shape->parent()->WorldTrafo().linear().transpose() * param_local;
    }

    Mat3 rotation = shape->RotationAsMatrix();

    const Vec3 x = rotation.col(0);
    Vec3 new_y = rotation.col(1) + param_local;
    new_y.normalize();
    const Vec3 normal = x.cross(new_y);
    rotation.col(0) = new_y.cross(normal).normalized();
    rotation.col(1) = new_y;
    rotation.col(2) = -new_y.cross(rotation.col(0));

    shape->SetRotation(rotation);
    return false;
  }
};

// This shape operation is used for L-systems used in:
// Talton et al., 2011, Metropolis Procedural Modeling.
// See: https://github.com/Sunwinds/metropolis-procedural-modeling
REGISTER_SHAPE_OP(OpEval_turnYAxisPerpToVec, "turnYAxisPerpToVec", 3,
                  {ValueType::FLOAT, ValueType::FLOAT, ValueType::FLOAT});
struct OpEval_turnYAxisPerpToVec : ShapeOpEvaluator {
  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();

    Vec3 dir(args_->at(0).f, args_->at(1).f, args_->at(2).f);
    if (shape->parent()) {
      dir = shape->parent()->WorldTrafo().linear().transpose() * dir;
    }

    Mat3 rotation = shape->RotationAsMatrix();

    const Vec3 y = rotation.col(1);
    Vec3 perp_dir = (dir.cross(y)).cross(dir).normalized();
    perp_dir *= dir.norm();
    const Vec3 x = rotation.col(0);
    Vec3 new_y = y + perp_dir;
    new_y.normalize();
    const Vec3 normal = x.cross(new_y);
    rotation.col(0) = new_y.cross(normal).normalized();
    rotation.col(1) = new_y;
    rotation.col(2) = -new_y.cross(rotation.col(0));

    shape->SetRotation(rotation);
    return false;
  }
};

// This shape operation is used for L-systems used in:
// Talton et al., 2011, Metropolis Procedural Modeling.
// See: https://github.com/Sunwinds/metropolis-procedural-modeling
REGISTER_SHAPE_OP(OpEval_turnZAxisToVec, "turnZAxisToVec", 3,
                  {ValueType::FLOAT, ValueType::FLOAT, ValueType::FLOAT});
struct OpEval_turnZAxisToVec : ShapeOpEvaluator {
  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();

    Vec3 dir(args_->at(0).f, args_->at(1).f, args_->at(2).f);
    if (shape->parent()) {
      dir = shape->parent()->WorldTrafo().linear().transpose() * dir;
    }

    Mat3 rotation = shape->RotationAsMatrix();
    Vec3 new_z = rotation.col(2);
    new_z += new_z.dot(dir) > 0.0 ? dir : -dir;
    new_z.normalize();
    const Vec3 normal = rotation.col(0).cross(new_z);
    rotation.col(0) = new_z.cross(normal).normalized();
    rotation.col(1) = -new_z.cross(rotation.col(0));
    rotation.col(2) = new_z;

    shape->SetRotation(rotation);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_center, "center", 0, {});
struct OpEval_center : ShapeOpEvaluator {
  bool Eval() final {
    const Shape* reference_shape = context_->shape_stack->TopMinusN(1);
    if (!reference_shape) {
      reference_shape = context_->shape_stack->Top()->parent();
      if (!reference_shape) {
        std::ostringstream oss;
        oss << "Shape operation '" << *name_ << "' cannot be evaluated ";
        oss << "because the current shape has no reference or parent shape.";
        error_message_ = oss.str();
        return true;
      }
    }
    context_->shape_stack->Top()->CenterInOtherShape(reference_shape, true,
                                                     true, true);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_centerX, "centerX", 0, {});
struct OpEval_centerX : ShapeOpEvaluator {
  bool Eval() final {
    const Shape* reference_shape = context_->shape_stack->TopMinusN(1);
    if (!reference_shape) {
      reference_shape = context_->shape_stack->Top()->parent();
      if (!reference_shape) {
        std::ostringstream oss;
        oss << "Shape operation '" << *name_ << "' cannot be evaluated ";
        oss << "because the current shape has no reference or parent shape.";
        error_message_ = oss.str();
        return true;
      }
    }
    context_->shape_stack->Top()->CenterInOtherShape(reference_shape, true,
                                                     false, false);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_centerY, "centerY", 0, {});
struct OpEval_centerY : ShapeOpEvaluator {
  bool Eval() final {
    const Shape* reference_shape = context_->shape_stack->TopMinusN(1);
    if (!reference_shape) {
      reference_shape = context_->shape_stack->Top()->parent();
      if (!reference_shape) {
        std::ostringstream oss;
        oss << "Shape operation '" << *name_ << "' cannot be evaluated ";
        oss << "because the current shape has no reference or parent shape.";
        error_message_ = oss.str();
        return true;
      }
    }
    context_->shape_stack->Top()->CenterInOtherShape(reference_shape, false,
                                                     true, false);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_centerZ, "centerZ", 0, {});
struct OpEval_centerZ : ShapeOpEvaluator {
  bool Eval() final {
    const Shape* reference_shape = context_->shape_stack->TopMinusN(1);
    if (!reference_shape) {
      reference_shape = context_->shape_stack->Top()->parent();
      if (!reference_shape) {
        std::ostringstream oss;
        oss << "Shape operation '" << *name_ << "' cannot be evaluated ";
        oss << "because the current shape has no reference or parent shape.";
        error_message_ = oss.str();
        return true;
      }
    }
    context_->shape_stack->Top()->CenterInOtherShape(reference_shape, false,
                                                     false, true);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_centerAtOrigin, "centerAtOrigin", 0, {});
struct OpEval_centerAtOrigin : ShapeOpEvaluator {
  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();
    shape->Translate(-shape->Size() * 0.5);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_centerAtOriginX, "centerAtOriginX", 0, {});
struct OpEval_centerAtOriginX : ShapeOpEvaluator {
  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();
    shape->TranslateX(-shape->SizeX() * 0.5);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_centerAtOriginY, "centerAtOriginY", 0, {});
struct OpEval_centerAtOriginY : ShapeOpEvaluator {
  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();
    shape->TranslateY(-shape->SizeY() * 0.5);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_centerAtOriginZ, "centerAtOriginZ", 0, {});
struct OpEval_centerAtOriginZ : ShapeOpEvaluator {
  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();
    shape->TranslateZ(-shape->SizeZ() * 0.5);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_extrude, "extrude", -1, {});
struct OpEval_extrude : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    const Shape* shape = context_->shape_stack->Top();

    if (CheckArgNumber({1, 4})) {
      return true;
    }
    for (unsigned i = 0; i < args_->size(); ++i) {
      if (ValidateType(ValueType::FLOAT, i)) {
        return true;
      }
    }

    // Check that the direction is not the 0-vector.
    int length_param_idx = 0;
    if (args_->size() == 4) {
      if (CheckDirectionVector(0)) {
        return true;
      }
      length_param_idx = 3;
    }

    if (CheckGreaterThanZero(length_param_idx) || CheckNonEmptyMesh(shape) ||
        CheckSingleFaceMesh(shape)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();
    geometry::HalfedgeMeshPtr tmp =
        std::make_shared<geometry::HalfedgeMesh>(*shape->mesh());
    tmp->TransformUnitTrafoAndScale(shape->Size());

    if (args_->size() == 4) {
      const Vec3 direction(args_->at(0).f, args_->at(1).f, args_->at(2).f);

      // Unfortunately there is one more error case that currently cannot be
      // detected in the ValidateSpecial method: if the normal and direction
      // form an angle larger than 90 degrees, we will know that only from the
      // return value of the ExtrudeAlongDirection method.
      if (tmp->ExtrudeAlongDirection(direction, args_->at(3).f)) {
        std::ostringstream oss;
        oss << "In shape operation 'extrude', the angle between the direction";
        oss << " and the face normal must be smaller than 90 degrees.";
        error_message_ = oss.str();
        return true;
      }
    } else {
      const bool ret = tmp->ExtrudeAlongNormal(args_->at(0).f);
      assert(!ret);
      (void)ret;
    }
    shape->SetMeshAndAdaptScope(tmp);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_extrudeWorld, "extrudeWorld", 4,
                  {ValueType::FLOAT, ValueType::FLOAT, ValueType::FLOAT,
                   ValueType::FLOAT});
struct OpEval_extrudeWorld : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    const Shape* shape = context_->shape_stack->Top();
    if (CheckDirectionVector(0) || CheckGreaterThanZero(3) ||
        CheckNonEmptyMesh(shape) || CheckSingleFaceMesh(shape)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();
    geometry::HalfedgeMeshPtr tmp =
        std::make_shared<geometry::HalfedgeMesh>(*shape->mesh());
    tmp->TransformUnitTrafoAndScale(shape->Size());

    const Vec3 world_space_direction(args_->at(0).f, args_->at(1).f,
                                     args_->at(2).f);
    const Vec3 local_direction =
        shape->WorldTrafo().linear().transpose() * world_space_direction;

    if (tmp->ExtrudeAlongDirection(local_direction, args_->at(3).f)) {
      std::ostringstream oss;
      oss << "In shape operation 'extrudeWorld', the angle between the "
             "direction";
      oss << " and the face normal must be smaller than 90 degrees.";
      error_message_ = oss.str();
      return true;
    }
    shape->SetMeshAndAdaptScope(tmp);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_mirrorX, "mirrorX", 0, {});
struct OpEval_mirrorX : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckNonEmptyMesh(context_->shape_stack->Top())) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();
    geometry::HalfedgeMeshPtr tmp =
        std::make_shared<geometry::HalfedgeMesh>(*shape->mesh());
    tmp->Mirror(geometry::Plane3(Vec3::UnitX(), 0.0));
    shape->set_mesh(tmp);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_mirrorY, "mirrorY", 0, {});
struct OpEval_mirrorY : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckNonEmptyMesh(context_->shape_stack->Top())) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();
    geometry::HalfedgeMeshPtr tmp =
        std::make_shared<geometry::HalfedgeMesh>(*shape->mesh());
    tmp->Mirror(geometry::Plane3(Vec3::UnitY(), 0.0));
    shape->set_mesh(tmp);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_mirrorZ, "mirrorZ", 0, {});
struct OpEval_mirrorZ : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckNonEmptyMesh(context_->shape_stack->Top())) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();
    geometry::HalfedgeMeshPtr tmp =
        std::make_shared<geometry::HalfedgeMesh>(*shape->mesh());
    tmp->Mirror(geometry::Plane3(Vec3::UnitZ(), 0.0));
    shape->set_mesh(tmp);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_normalsFlip, "normalsFlip", 0, {});
struct OpEval_normalsFlip : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckNonEmptyMesh(context_->shape_stack->Top())) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();
    geometry::HalfedgeMeshPtr tmp =
        std::make_shared<geometry::HalfedgeMesh>(*shape->mesh());
    tmp->FlipWinding(true);
    shape->set_mesh(tmp);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_normalsFlat, "normalsFlat", 0, {});
struct OpEval_normalsFlat : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckNonEmptyMesh(context_->shape_stack->Top())) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();
    geometry::HalfedgeMeshPtr tmp =
        std::make_shared<geometry::HalfedgeMesh>(*shape->mesh());
    tmp->RemoveFaceVertexNormals();
    shape->set_mesh(tmp);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_normalsSmooth, "normalsSmooth", 0, {});
struct OpEval_normalsSmooth : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckNonEmptyMesh(context_->shape_stack->Top())) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();
    geometry::HalfedgeMeshPtr tmp =
        std::make_shared<geometry::HalfedgeMesh>(*shape->mesh());
    tmp->TransformUnitTrafoAndScale(shape->Size());
    tmp->ComputeFaceVertexNormals();
    shape->set_mesh(tmp);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_trim, "trim", 0, {});
struct OpEval_trim : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckNonEmptyMesh(context_->shape_stack->Top())) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();
    shape->Trim(false);

    if (shape->mesh()->Empty()) {
      error_message_ = "Shape operation 'trim' leaves an empty mesh behind.";
      return true;
    }
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_trimLocal, "trimLocal", 0, {});
struct OpEval_trimLocal : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckNonEmptyMesh(context_->shape_stack->Top())) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();
    shape->Trim(true);

    if (shape->mesh()->Empty()) {
      error_message_ =
          "Shape operation 'trimLocal' leaves an empty mesh behind.";
      return true;
    }
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_trimPlane, "trimPlane", -1, {});
struct OpEval_trimPlane : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckArgNumber({4, 6})) {
      return true;
    }
    for (unsigned i = 0; i < args_->size(); ++i) {
      if (ValidateType(ValueType::FLOAT, i)) {
        return true;
      }
    }
    if (CheckDirectionVector(0)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    const Vec3 direction = Vec3(args_->at(0).f, args_->at(1).f, args_->at(2).f);
    geometry::Plane3 plane;

    if (args_->size() == 6) {
      const Vec3 point = Vec3(args_->at(3).f, args_->at(4).f, args_->at(5).f);
      plane = geometry::Plane3(direction, point);
    } else {
      plane = geometry::Plane3(direction, args_->at(3).f);
    }

    context_->shape_stack->Top()->AddTrimPlane(plane);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_ffdReset, "ffdReset", 3,
                  {ValueType::INT, ValueType::INT, ValueType::INT});
struct OpEval_ffdReset : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    for (int i = 0; i < 3; ++i) {
      if (CheckRange(i, 1, 5)) {
        return true;
      }
    }
    return false;
  }

  bool Eval() final {
    context_->shape_stack->Top()->FFDSetCageAndReset(
        Vec3i(args_->at(0).i, args_->at(1).i, args_->at(2).i));
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_ffdTranslate, "ffdTranslate", 6,
                  {ValueType::INT, ValueType::INT, ValueType::INT,
                   ValueType::FLOAT, ValueType::FLOAT, ValueType::FLOAT});
struct OpEval_ffdTranslate : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    const Shape* shape = context_->shape_stack->Top();
    for (int i = 0; i < 3; ++i) {
      if (CheckRange(i, 0, shape->FFDResolution()[i])) {
        return true;
      }
    }
    return false;
  }

  bool Eval() final {
    context_->shape_stack->Top()->FFDTranslateCP(
        Vec3i(args_->at(0).i, args_->at(1).i, args_->at(2).i),
        Vec3(args_->at(3).f, args_->at(4).f, args_->at(5).f));
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_ffdTranslateX, "ffdTranslateX", 4,
                  {ValueType::INT, ValueType::INT, ValueType::INT,
                   ValueType::FLOAT});
struct OpEval_ffdTranslateX : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    const Shape* shape = context_->shape_stack->Top();
    for (int i = 0; i < 3; ++i) {
      if (CheckRange(i, 0, shape->FFDResolution()[i])) {
        return true;
      }
    }
    return false;
  }

  bool Eval() final {
    context_->shape_stack->Top()->FFDTranslateCP(
        Vec3i(args_->at(0).i, args_->at(1).i, args_->at(2).i),
        Vec3(args_->at(3).f, 0.0, 0.0));
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_ffdTranslateY, "ffdTranslateY", 4,
                  {ValueType::INT, ValueType::INT, ValueType::INT,
                   ValueType::FLOAT});
struct OpEval_ffdTranslateY : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    const Shape* shape = context_->shape_stack->Top();
    for (int i = 0; i < 3; ++i) {
      if (CheckRange(i, 0, shape->FFDResolution()[i])) {
        return true;
      }
    }
    return false;
  }

  bool Eval() final {
    context_->shape_stack->Top()->FFDTranslateCP(
        Vec3i(args_->at(0).i, args_->at(1).i, args_->at(2).i),
        Vec3(0.0, args_->at(3).f, 0.0));
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_ffdTranslateZ, "ffdTranslateZ", 4,
                  {ValueType::INT, ValueType::INT, ValueType::INT,
                   ValueType::FLOAT});
struct OpEval_ffdTranslateZ : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    const Shape* shape = context_->shape_stack->Top();
    for (int i = 0; i < 3; ++i) {
      if (CheckRange(i, 0, shape->FFDResolution()[i])) {
        return true;
      }
    }
    return false;
  }

  bool Eval() final {
    context_->shape_stack->Top()->FFDTranslateCP(
        Vec3i(args_->at(0).i, args_->at(1).i, args_->at(2).i),
        Vec3(0.0, 0.0, args_->at(3).f));
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_ffdApply, "ffdApply", 0, {});
struct OpEval_ffdApply : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckNonEmptyMesh(context_->shape_stack->Top())) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    context_->shape_stack->Top()->FFDApply();
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_uvScale, "uvScale", 2,
                  {ValueType::FLOAT, ValueType::FLOAT});
struct OpEval_uvScale : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    const Shape* shape = context_->shape_stack->Top();

    if (CheckGreaterThanZero(0) || CheckGreaterThanZero(1) ||
        CheckNonEmptyMesh(shape)) {
      return true;
    }

    if (!shape->mesh()->has_uv0()) {
      error_message_ =
          "Shape operation 'uvScale' cannot be applied to shape with mesh "
          "without UV coordinates.";
      return true;
    }
    return false;
  }

  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();

    Affine2 tex_trafo = Affine2::Identity();
    tex_trafo.scale(Vec2(args_->at(0).f, args_->at(1).f));

    geometry::HalfedgeMeshPtr tmp =
        std::make_shared<geometry::HalfedgeMesh>(*shape->mesh());
    tmp->TexTransformUV(tex_trafo);
    shape->set_mesh(tmp);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_uvTranslate, "uvTranslate", 2,
                  {ValueType::FLOAT, ValueType::FLOAT});
struct OpEval_uvTranslate : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    const Shape* shape = context_->shape_stack->Top();

    if (CheckNonEmptyMesh(shape)) {
      return true;
    }

    if (!shape->mesh()->has_uv0()) {
      error_message_ =
          "Shape operation 'uvTranslate' cannot be applied to shape with mesh "
          "without UV coordinates.";
      return true;
    }
    return false;
  }

  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();

    Affine2 tex_trafo = Affine2::Identity();
    tex_trafo.translate(Vec2(args_->at(0).f, args_->at(1).f));

    geometry::HalfedgeMeshPtr tmp =
        std::make_shared<geometry::HalfedgeMesh>(*shape->mesh());
    tmp->TexTransformUV(tex_trafo);
    shape->set_mesh(tmp);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_uvSetupProjectionXY, "uvSetupProjectionXY", -1, {});
struct OpEval_uvSetupProjectionXY : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckArgNumber({2, 4})) {
      return true;
    }
    for (unsigned i = 0; i < args_->size(); ++i) {
      if (ValidateType(ValueType::FLOAT, i)) {
        return true;
      }
    }
    if (CheckGreaterThanZero(0) || CheckGreaterThanZero(1)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();

    if (args_->size() == 2) {
      shape->TextureSetupProjectionInXY(args_->at(0).f, args_->at(1).f);
    } else {  // if (args_->size() == 4)
      shape->TextureSetupProjectionInXY(args_->at(0).f, args_->at(1).f,
                                        args_->at(2).f, args_->at(3).f);
    }
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_uvProject, "uvProject", 0, {});
struct OpEval_uvProject : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckNonEmptyMesh(context_->shape_stack->Top())) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    if (context_->shape_stack->Top()->TextureProjectUV()) {
      error_message_ =
          "Shape operation 'uvProject' failed because there is no ancestor "
          "that has a projection set.";
      return true;
    }
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_mesh, "mesh", 1, {ValueType::STRING});
struct OpEval_mesh : ShapeOpEvaluator {
  bool Eval() final {
    const std::string asset_uri = context_->base_path + *args_->at(0).s;

    AssetCache& asset_cache = AssetCache::Get();
    geometry::HalfedgeMeshPtr mesh = asset_cache.GetMeshRessource(asset_uri);
    if (!mesh) {
      std::ostringstream oss;
      oss << "Shape operation 'mesh' failed to load the file '"
          << *args_->at(0).s << "'.";
      error_message_ = oss.str();
      return true;
    }
    context_->shape_stack->Top()->SetMeshIntoScope(mesh);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_quad, "quad", 0, {});
struct OpEval_quad : ShapeOpEvaluator {
  bool Eval() final {
    geometry::HalfedgeMeshPtr mesh;
    const std::string id = "!quad";

    AssetCache& asset_cache = AssetCache::Get();
    if (asset_cache.HasMeshRessource(id)) {
      mesh = asset_cache.GetMeshRessource(id);
      assert(mesh);
    } else {
      Vec3Vec quad;
      Vec2Vec uvs;
      geometry::PolygonUnitSquare(&quad, &uvs);
      mesh = std::make_shared<geometry::HalfedgeMesh>();
      mesh->FromPolygon(quad, nullptr, &uvs);
      asset_cache.InsertMeshRessource(id, mesh);
    }

    context_->shape_stack->Top()->SetMeshIntoScope(mesh);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_circle, "circle", 1, {ValueType::INT});
struct OpEval_circle : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckRange(0, 3, 256)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    geometry::HalfedgeMeshPtr mesh;
    std::ostringstream oss;
    oss << "!circle_" << args_->at(0).i;
    const std::string id = oss.str();

    AssetCache& asset_cache = AssetCache::Get();
    if (asset_cache.HasMeshRessource(id)) {
      mesh = asset_cache.GetMeshRessource(id);
      assert(mesh);
    } else {
      Vec3Vec circle;
      Vec2Vec uvs;
      geometry::PolygonUnitCircle(args_->at(0).i, &circle, &uvs);
      mesh = std::make_shared<geometry::HalfedgeMesh>();
      mesh->FromPolygon(circle, nullptr, &uvs);
      asset_cache.InsertMeshRessource(id, mesh);
    }

    context_->shape_stack->Top()->SetMeshIntoScope(mesh);
    return false;
  }
};

static const Scalar thres_l_u_t_h = 0.001;

REGISTER_SHAPE_OP(OpEval_shapeL, "shapeL", 2,
                  {ValueType::FLOAT, ValueType::FLOAT});
struct OpEval_shapeL : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckRange(0, thres_l_u_t_h,
                   context_->shape_stack->Top()->SizeZ() - thres_l_u_t_h) ||
        CheckRange(1, thres_l_u_t_h,
                   context_->shape_stack->Top()->SizeX() - thres_l_u_t_h)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    Vec3Vec l;
    Vec2Vec uvs;
    Shape* top = context_->shape_stack->Top();
    geometry::PolygonL(top->SizeX(), top->SizeZ(), args_->at(0).f,
                       args_->at(1).f, &l, &uvs);
    geometry::HalfedgeMeshPtr mesh = std::make_shared<geometry::HalfedgeMesh>();
    mesh->FromPolygon(l, nullptr, &uvs);
    top->SetMeshIntoScope(mesh);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_shapeU, "shapeU", 3,
                  {ValueType::FLOAT, ValueType::FLOAT, ValueType::FLOAT});
struct OpEval_shapeU : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckRange(0, thres_l_u_t_h,
                   context_->shape_stack->Top()->SizeZ() - thres_l_u_t_h) ||
        CheckRange(1, thres_l_u_t_h,
                   context_->shape_stack->Top()->SizeX() - thres_l_u_t_h) ||
        CheckRange(2, thres_l_u_t_h,
                   context_->shape_stack->Top()->SizeX() - thres_l_u_t_h)) {
      return true;
    }

    if (args_->at(1).f + args_->at(2).f >=
        context_->shape_stack->Top()->SizeX() - thres_l_u_t_h) {
      error_message_ =
          "The sum of parameter 2 and 3 of shape operation 'shapeU' needs to "
          "be smaller than 'size_x'.";
      return true;
    }
    return false;
  }

  bool Eval() final {
    Vec3Vec u;
    Vec2Vec uvs;
    Shape* top = context_->shape_stack->Top();
    geometry::PolygonU(top->SizeX(), top->SizeZ(), args_->at(0).f,
                       args_->at(1).f, args_->at(2).f, &u, &uvs);
    geometry::HalfedgeMeshPtr mesh = std::make_shared<geometry::HalfedgeMesh>();
    mesh->FromPolygon(u, nullptr, &uvs);
    top->SetMeshIntoScope(mesh);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_shapeT, "shapeT", 3,
                  {ValueType::FLOAT, ValueType::FLOAT, ValueType::FLOAT});
struct OpEval_shapeT : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckRange(0, thres_l_u_t_h,
                   context_->shape_stack->Top()->SizeZ() - thres_l_u_t_h) ||
        CheckRange(1, thres_l_u_t_h,
                   context_->shape_stack->Top()->SizeX() - thres_l_u_t_h) ||
        CheckRange(2, thres_l_u_t_h, 1.0 - thres_l_u_t_h)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    Vec3Vec u;
    Vec2Vec uvs;
    Shape* top = context_->shape_stack->Top();
    geometry::PolygonT(top->SizeX(), top->SizeZ(), args_->at(0).f,
                       args_->at(1).f, args_->at(2).f, &u, &uvs);
    geometry::HalfedgeMeshPtr mesh = std::make_shared<geometry::HalfedgeMesh>();
    mesh->FromPolygon(u, nullptr, &uvs);
    top->SetMeshIntoScope(mesh);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_shapeH, "shapeH", 4,
                  {ValueType::FLOAT, ValueType::FLOAT, ValueType::FLOAT,
                   ValueType::FLOAT});
struct OpEval_shapeH : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckRange(0, thres_l_u_t_h,
                   context_->shape_stack->Top()->SizeX() - thres_l_u_t_h) ||
        CheckRange(1, thres_l_u_t_h,
                   context_->shape_stack->Top()->SizeZ() - thres_l_u_t_h) ||
        CheckRange(2, thres_l_u_t_h,
                   context_->shape_stack->Top()->SizeX() - thres_l_u_t_h) ||
        CheckRange(3, thres_l_u_t_h, 1.0 - thres_l_u_t_h)) {
      return true;
    }

    if (args_->at(0).f + args_->at(2).f >=
        context_->shape_stack->Top()->SizeX() - thres_l_u_t_h) {
      error_message_ =
          "The sum of parameter 1 and 3 of shape operation 'shapeH' needs to "
          "be smaller than 'size_x'.";
      return true;
    }
    return false;
  }

  bool Eval() final {
    Vec3Vec u;
    Vec2Vec uvs;
    Shape* top = context_->shape_stack->Top();
    geometry::PolygonH(top->SizeX(), top->SizeZ(), args_->at(0).f,
                       args_->at(1).f, args_->at(2).f, args_->at(3).f, &u,
                       &uvs);
    geometry::HalfedgeMeshPtr mesh = std::make_shared<geometry::HalfedgeMesh>();
    mesh->FromPolygon(u, nullptr, &uvs);
    top->SetMeshIntoScope(mesh);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_polygon, "polygon", -1, {});
struct OpEval_polygon : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (args_->size() % 2 == 1) {
      error_message_ =
          "Shape operation 'polygon' needs an even number of parameters.";
      return true;
    }
    if (args_->size() < 6) {
      error_message_ =
          "Shape operation 'polygon' needs at least 6 parameters (at least 3 "
          "positions).";
      return true;
    }

    for (unsigned i = 0; i < args_->size(); ++i) {
      if (ValidateType(ValueType::FLOAT, i)) {
        return true;
      }
    }
    return false;
  }

  bool Eval() final {
    Vec3Vec poly;
    for (unsigned i = 0; i < args_->size(); i += 2) {
      poly.push_back(Vec3(args_->at(i).f, 0.0, args_->at(i + 1).f));
    }

    geometry::AABB aabb(poly);

    Vec2Vec uvs;
    Affine2 uv_trafo = Affine2::Identity();
    uv_trafo.scale(Vec2(0.5 / aabb.extent.x(), 0.5 / aabb.extent.z()));
    uv_trafo.translate(-Vec2(aabb.center.x() - aabb.extent.x(),
                             aabb.center.z() - aabb.extent.z()));
    for (size_t i = 0; i < poly.size(); ++i) {
      uvs.push_back(uv_trafo * Vec2(poly[i].x(), poly[i].z()));
    }

    // The polygon is defined by the user in a xy coordinate system.
    // We have to remap that onto the xz plane.
    Affine3 mirror_trafo = Affine3::Identity();
    mirror_trafo.translate(Vec3(0.0, 0.0, aabb.center.z()));
    mirror_trafo.scale(Vec3(1.0, 1.0, -1.0));
    mirror_trafo.translate(Vec3(0.0, 0.0, -aabb.center.z()));

    for (size_t i = 0; i < poly.size(); ++i) {
      poly[i] = mirror_trafo * poly[i];
    }

    Shape* top = context_->shape_stack->Top();
    geometry::HalfedgeMeshPtr mesh = std::make_shared<geometry::HalfedgeMesh>();
    mesh->FromPolygon(poly, nullptr, &uvs);
    top->SetMeshIntoScope(mesh);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_grid, "grid", 2, {ValueType::INT, ValueType::INT});
struct OpEval_grid : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckRange(0, 1, 256) || CheckRange(1, 1, 256)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    geometry::HalfedgeMeshPtr mesh;
    std::ostringstream oss;
    oss << "!grid_" << args_->at(0).i << '_' << args_->at(1).i;
    const std::string id = oss.str();

    AssetCache& asset_cache = AssetCache::Get();
    if (asset_cache.HasMeshRessource(id)) {
      mesh = asset_cache.GetMeshRessource(id);
      assert(mesh);
    } else {
      Vec3Vec vertices;
      IdxVecVec indices;
      Vec2Vec uvs;
      geometry::MeshUnitGrid(args_->at(0).i, args_->at(1).i, &vertices,
                             &indices, &uvs);
      mesh = std::make_shared<geometry::HalfedgeMesh>();
      mesh->FromIndexedVertices(vertices, indices, nullptr, nullptr, &uvs,
                                &indices, nullptr);
      asset_cache.InsertMeshRessource(id, mesh);
    }

    context_->shape_stack->Top()->SetMeshIntoScope(mesh);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_disk, "disk", 2, {ValueType::INT, ValueType::INT});
struct OpEval_disk : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckRange(0, 3, 256) || CheckRange(1, 1, 256)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    geometry::HalfedgeMeshPtr mesh;
    std::ostringstream oss;
    oss << "!disk_" << args_->at(0).i << '_' << args_->at(1).i;
    const std::string id = oss.str();

    AssetCache& asset_cache = AssetCache::Get();
    if (asset_cache.HasMeshRessource(id)) {
      mesh = asset_cache.GetMeshRessource(id);
      assert(mesh);
    } else {
      Vec3Vec vertices;
      IdxVecVec indices;
      Vec2Vec uvs;
      geometry::MeshUnitDisk(args_->at(0).i, args_->at(1).i, &vertices,
                             &indices, &uvs);
      mesh = std::make_shared<geometry::HalfedgeMesh>();
      mesh->FromIndexedVertices(vertices, indices, nullptr, nullptr, &uvs,
                                &indices, nullptr);
      asset_cache.InsertMeshRessource(id, mesh);
    }

    context_->shape_stack->Top()->SetMeshIntoScope(mesh);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_cube, "cube", 0, {});
struct OpEval_cube : ShapeOpEvaluator {
  bool Eval() final {
    geometry::HalfedgeMeshPtr mesh;
    const std::string id = "!cube";

    AssetCache& asset_cache = AssetCache::Get();
    if (asset_cache.HasMeshRessource(id)) {
      mesh = asset_cache.GetMeshRessource(id);
      assert(mesh);
    } else {
      Vec3Vec vertices;
      IdxVecVec indices;
      Vec2Vec uvs;
      IdxVecVec uv_indices;
      geometry::MeshUnitCube(&vertices, &indices, &uvs, &uv_indices);
      mesh = std::make_shared<geometry::HalfedgeMesh>();
      mesh->FromIndexedVertices(vertices, indices, nullptr, nullptr, &uvs,
                                &uv_indices, nullptr);
      asset_cache.InsertMeshRessource(id, mesh);
    }

    context_->shape_stack->Top()->SetMeshIntoScope(mesh);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_box, "box", 3,
                  {ValueType::INT, ValueType::INT, ValueType::INT});
struct OpEval_box : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    for (int i = 0; i < 3; ++i) {
      if (CheckRange(i, 1, 256)) {
        return true;
      }
    }
    return false;
  }

  bool Eval() final {
    geometry::HalfedgeMeshPtr mesh;
    std::ostringstream oss;
    oss << "!box_" << args_->at(0).i << '_' << args_->at(1).i << '_'
        << args_->at(2).i;
    const std::string id = oss.str();

    AssetCache& asset_cache = AssetCache::Get();
    if (asset_cache.HasMeshRessource(id)) {
      mesh = asset_cache.GetMeshRessource(id);
      assert(mesh);
    } else {
      Vec3Vec vertices;
      IdxVecVec indices;
      Vec2Vec uvs;
      IdxVecVec uv_indices;
      geometry::MeshUnitBox(args_->at(0).i, args_->at(1).i, args_->at(2).i,
                            &vertices, &indices, &uvs, &uv_indices);
      mesh = std::make_shared<geometry::HalfedgeMesh>();
      mesh->FromIndexedVertices(vertices, indices, nullptr, nullptr, &uvs,
                                &uv_indices, nullptr);
      asset_cache.InsertMeshRessource(id, mesh);
    }

    context_->shape_stack->Top()->SetMeshIntoScope(mesh);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_cylinder, "cylinder", 3,
                  {ValueType::INT, ValueType::INT, ValueType::INT});
struct OpEval_cylinder : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckRange(0, 3, 256) || CheckRange(1, 1, 256) ||
        CheckRange(2, 1, 256)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    geometry::HalfedgeMeshPtr mesh;
    std::ostringstream oss;
    oss << "!cylinder_" << args_->at(0).i << '_' << args_->at(1).i << '_'
        << args_->at(2).i;
    const std::string id = oss.str();

    AssetCache& asset_cache = AssetCache::Get();
    if (asset_cache.HasMeshRessource(id)) {
      mesh = asset_cache.GetMeshRessource(id);
      assert(mesh);
    } else {
      Vec3Vec vertices;
      IdxVecVec indices;
      Vec3Vec normals;
      IdxVecVec normal_indices;
      Vec2Vec uvs;
      IdxVecVec uv_indices;
      geometry::MeshUnitCylinder(args_->at(0).i, args_->at(1).i, args_->at(2).i,
                                 &vertices, &indices, &normals, &normal_indices,
                                 &uvs, &uv_indices);
      mesh = std::make_shared<geometry::HalfedgeMesh>();
      mesh->FromIndexedVertices(vertices, indices, &normals, &normal_indices,
                                &uvs, &uv_indices, nullptr);
      asset_cache.InsertMeshRessource(id, mesh);
    }

    context_->shape_stack->Top()->SetMeshIntoScope(mesh);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_cylinderFlat, "cylinderFlat", 3,
                  {ValueType::INT, ValueType::INT, ValueType::INT});
struct OpEval_cylinderFlat : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckRange(0, 3, 256) || CheckRange(1, 1, 256) ||
        CheckRange(2, 1, 256)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    geometry::HalfedgeMeshPtr mesh;
    std::ostringstream oss;
    oss << "!cylinderFlat_" << args_->at(0).i << '_' << args_->at(1).i << '_'
        << args_->at(2).i;
    const std::string id = oss.str();

    AssetCache& asset_cache = AssetCache::Get();
    if (asset_cache.HasMeshRessource(id)) {
      mesh = asset_cache.GetMeshRessource(id);
      assert(mesh);
    } else {
      Vec3Vec vertices;
      IdxVecVec indices;
      Vec2Vec uvs;
      IdxVecVec uv_indices;
      geometry::MeshUnitCylinder(args_->at(0).i, args_->at(1).i, args_->at(2).i,
                                 &vertices, &indices, nullptr, nullptr, &uvs,
                                 &uv_indices);
      mesh = std::make_shared<geometry::HalfedgeMesh>();
      mesh->FromIndexedVertices(vertices, indices, nullptr, nullptr, &uvs,
                                &uv_indices, nullptr);
      asset_cache.InsertMeshRessource(id, mesh);
    }

    context_->shape_stack->Top()->SetMeshIntoScope(mesh);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_cone, "cone", 3,
                  {ValueType::INT, ValueType::INT, ValueType::INT});
struct OpEval_cone : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckRange(0, 3, 256) || CheckRange(1, 1, 256) ||
        CheckRange(2, 1, 256)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    geometry::HalfedgeMeshPtr mesh;
    std::ostringstream oss;
    oss << "!cone_" << args_->at(0).i << '_' << args_->at(1).i << '_'
        << args_->at(2).i;
    const std::string id = oss.str();

    AssetCache& asset_cache = AssetCache::Get();
    if (asset_cache.HasMeshRessource(id)) {
      mesh = asset_cache.GetMeshRessource(id);
      assert(mesh);
    } else {
      Vec3Vec vertices;
      IdxVecVec indices;
      Vec3Vec normals;
      IdxVecVec normal_indices;
      Vec2Vec uvs;
      IdxVecVec uv_indices;
      geometry::MeshUnitCone(args_->at(0).i, args_->at(1).i, args_->at(2).i,
                             &vertices, &indices, &normals, &normal_indices,
                             &uvs, &uv_indices);
      mesh = std::make_shared<geometry::HalfedgeMesh>();
      mesh->FromIndexedVertices(vertices, indices, &normals, &normal_indices,
                                &uvs, &uv_indices, nullptr);
      asset_cache.InsertMeshRessource(id, mesh);
    }

    context_->shape_stack->Top()->SetMeshIntoScope(mesh);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_coneFlat, "coneFlat", 3,
                  {ValueType::INT, ValueType::INT, ValueType::INT});
struct OpEval_coneFlat : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckRange(0, 3, 256) || CheckRange(1, 1, 256) ||
        CheckRange(2, 1, 256)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    geometry::HalfedgeMeshPtr mesh;
    std::ostringstream oss;
    oss << "!coneFlat_" << args_->at(0).i << '_' << args_->at(1).i << '_'
        << args_->at(2).i;
    const std::string id = oss.str();

    AssetCache& asset_cache = AssetCache::Get();
    if (asset_cache.HasMeshRessource(id)) {
      mesh = asset_cache.GetMeshRessource(id);
      assert(mesh);
    } else {
      Vec3Vec vertices;
      IdxVecVec indices;
      Vec2Vec uvs;
      IdxVecVec uv_indices;
      geometry::MeshUnitCone(args_->at(0).i, args_->at(1).i, args_->at(2).i,
                             &vertices, &indices, nullptr, nullptr, &uvs,
                             &uv_indices);
      mesh = std::make_shared<geometry::HalfedgeMesh>();
      mesh->FromIndexedVertices(vertices, indices, nullptr, nullptr, &uvs,
                                &uv_indices, nullptr);
      asset_cache.InsertMeshRessource(id, mesh);
    }

    context_->shape_stack->Top()->SetMeshIntoScope(mesh);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_sphere, "sphere", 2, {ValueType::INT, ValueType::INT});
struct OpEval_sphere : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckRange(0, 3, 256) || CheckRange(1, 2, 256)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    geometry::HalfedgeMeshPtr mesh;
    std::ostringstream oss;
    oss << "!sphere_" << args_->at(0).i << '_' << args_->at(1).i;
    const std::string id = oss.str();

    AssetCache& asset_cache = AssetCache::Get();
    if (asset_cache.HasMeshRessource(id)) {
      mesh = asset_cache.GetMeshRessource(id);
      assert(mesh);
    } else {
      Vec3Vec vertices;
      IdxVecVec indices;
      Vec3Vec normals;
      IdxVecVec normal_indices;
      Vec2Vec uvs;
      IdxVecVec uv_indices;
      geometry::MeshUnitSphere(args_->at(0).i, args_->at(1).i, &vertices,
                               &indices, &normals, &normal_indices, &uvs,
                               &uv_indices);
      mesh = std::make_shared<geometry::HalfedgeMesh>();
      mesh->FromIndexedVertices(vertices, indices, &normals, &normal_indices,
                                &uvs, &uv_indices, nullptr);
      asset_cache.InsertMeshRessource(id, mesh);
    }

    context_->shape_stack->Top()->SetMeshIntoScope(mesh);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_sphereFlat, "sphereFlat", 2,
                  {ValueType::INT, ValueType::INT});
struct OpEval_sphereFlat : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckRange(0, 3, 256) || CheckRange(1, 2, 256)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    geometry::HalfedgeMeshPtr mesh;
    std::ostringstream oss;
    oss << "!sphereFlat_" << args_->at(0).i << '_' << args_->at(1).i;
    const std::string id = oss.str();

    AssetCache& asset_cache = AssetCache::Get();
    if (asset_cache.HasMeshRessource(id)) {
      mesh = asset_cache.GetMeshRessource(id);
      assert(mesh);
    } else {
      Vec3Vec vertices;
      IdxVecVec indices;
      Vec2Vec uvs;
      IdxVecVec uv_indices;
      geometry::MeshUnitSphere(args_->at(0).i, args_->at(1).i, &vertices,
                               &indices, nullptr, nullptr, &uvs, &uv_indices);
      mesh = std::make_shared<geometry::HalfedgeMesh>();
      mesh->FromIndexedVertices(vertices, indices, nullptr, nullptr, &uvs,
                                &uv_indices, nullptr);
      asset_cache.InsertMeshRessource(id, mesh);
    }

    context_->shape_stack->Top()->SetMeshIntoScope(mesh);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_torus, "torus", 4,
                  {ValueType::FLOAT, ValueType::FLOAT, ValueType::INT,
                   ValueType::INT});
struct OpEval_torus : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckGreaterThanZero(1)) {
      return true;
    }

    if (args_->at(0).f < args_->at(1).f) {
      error_message_ =
          "Parameter 0 for shape operation 'torus' needs to be larger than "
          "parameter 1.";
      return true;
    }

    if (CheckRange(2, 3, 256) || CheckRange(3, 3, 256)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    geometry::HalfedgeMeshPtr mesh;
    std::ostringstream oss;
    oss << "!torus_" << args_->at(0).i << '_' << args_->at(1).i;
    oss << '_' << args_->at(2).i << '_' << args_->at(3).i;
    const std::string id = oss.str();

    AssetCache& asset_cache = AssetCache::Get();
    if (asset_cache.HasMeshRessource(id)) {
      mesh = asset_cache.GetMeshRessource(id);
      assert(mesh);
    } else {
      Vec3Vec vertices;
      IdxVecVec indices;
      Vec3Vec normals;
      IdxVecVec normal_indices;
      Vec2Vec uvs;
      IdxVecVec uv_indices;
      geometry::MeshTorus(args_->at(0).f, args_->at(1).f, args_->at(2).i,
                          args_->at(3).i, &vertices, &indices, &normals, &uvs,
                          &uv_indices);
      mesh = std::make_shared<geometry::HalfedgeMesh>();
      mesh->FromIndexedVertices(vertices, indices, &normals, &indices, &uvs,
                                &uv_indices, nullptr);
      asset_cache.InsertMeshRessource(id, mesh);
    }

    context_->shape_stack->Top()->SetMeshIntoScope(mesh);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_torusFlat, "torusFlat", 4,
                  {ValueType::FLOAT, ValueType::FLOAT, ValueType::INT,
                   ValueType::INT});
struct OpEval_torusFlat : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckGreaterThanZero(1)) {
      return true;
    }

    if (args_->at(0).f < args_->at(1).f) {
      error_message_ =
          "Parameter 0 for shape operation 'torusFlat' needs to be larger than "
          "parameter 1.";
      return true;
    }

    if (CheckRange(2, 3, 256) || CheckRange(3, 3, 256)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    geometry::HalfedgeMeshPtr mesh;
    std::ostringstream oss;
    oss << "!torusFlat_" << args_->at(0).i << '_' << args_->at(1).i;
    oss << '_' << args_->at(2).i << '_' << args_->at(3).i;
    const std::string id = oss.str();

    AssetCache& asset_cache = AssetCache::Get();
    if (asset_cache.HasMeshRessource(id)) {
      mesh = asset_cache.GetMeshRessource(id);
      assert(mesh);
    } else {
      Vec3Vec vertices;
      IdxVecVec indices;
      Vec2Vec uvs;
      IdxVecVec uv_indices;
      geometry::MeshTorus(args_->at(0).f, args_->at(1).f, args_->at(2).i,
                          args_->at(3).i, &vertices, &indices, nullptr, &uvs,
                          &uv_indices);
      mesh = std::make_shared<geometry::HalfedgeMesh>();
      mesh->FromIndexedVertices(vertices, indices, nullptr, nullptr, &uvs,
                                &uv_indices, nullptr);
      asset_cache.InsertMeshRessource(id, mesh);
    }

    context_->shape_stack->Top()->SetMeshIntoScope(mesh);
    return false;
  }
};

static const Scalar roof_angle_min = 0.001;
static const Scalar roof_angle_max = 85.0;

REGISTER_SHAPE_OP(OpEval_roofHip, "roofHip", -1, {});
struct OpEval_roofHip : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckArgNumber({1, 2})) {
      return true;
    }

    if (ValidateType(ValueType::FLOAT, 0) ||
        CheckRange(0, roof_angle_min, roof_angle_max)) {
      return true;
    }
    if (args_->size() == 2 &&
        (ValidateType(ValueType::FLOAT, 1) || CheckGreaterThanZero(1))) {
      return true;
    }

    const Shape* shape = context_->shape_stack->Top();
    if (CheckNonEmptyMesh(shape) || CheckSingleFaceMesh(shape)) {
      return true;
    }

    return false;
  }

  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();
    geometry::HalfedgeMeshPtr tmp =
        std::make_shared<geometry::HalfedgeMesh>(*shape->mesh());
    tmp->TransformUnitTrafoAndScale(shape->Size());
    const Scalar overhang = args_->size() == 2 ? args_->at(1).f : 0.0;
    tmp->ExtrudeRoofHipOrGable(args_->at(0).f, overhang, false, 0.0);
    shape->SetMeshAndAdaptScope(tmp);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_roofGable, "roofGable", -1, {});
struct OpEval_roofGable : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckArgNumber({1, 3})) {
      return true;
    }

    if (ValidateType(ValueType::FLOAT, 0) ||
        CheckRange(0, roof_angle_min, roof_angle_max)) {
      return true;
    }
    if (args_->size() == 3 &&
        (ValidateType(ValueType::FLOAT, 1) || CheckGreaterThanZero(1) ||
         ValidateType(ValueType::FLOAT, 2) || CheckGreaterThanZero(2))) {
      return true;
    }

    const Shape* shape = context_->shape_stack->Top();
    if (CheckNonEmptyMesh(shape) || CheckSingleFaceMesh(shape)) {
      return true;
    }

    return false;
  }

  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();
    geometry::HalfedgeMeshPtr tmp =
        std::make_shared<geometry::HalfedgeMesh>(*shape->mesh());
    tmp->TransformUnitTrafoAndScale(shape->Size());
    const Scalar overhang_side = args_->size() == 3 ? args_->at(1).f : 0.0;
    const Scalar overhang_gable = args_->size() == 3 ? args_->at(2).f : 0.0;
    tmp->ExtrudeRoofHipOrGable(args_->at(0).f, overhang_side, true,
                               overhang_gable);
    shape->SetMeshAndAdaptScope(tmp);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_roofPyramid, "roofPyramid", -1, {});
struct OpEval_roofPyramid : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckArgNumber({1, 2})) {
      return true;
    }

    if (ValidateType(ValueType::FLOAT, 0) || CheckGreaterThanZero(0)) {
      return true;
    }
    if (args_->size() == 2 &&
        (ValidateType(ValueType::FLOAT, 1) || CheckGreaterThanZero(1))) {
      return true;
    }

    const Shape* shape = context_->shape_stack->Top();
    if (CheckNonEmptyMesh(shape) || CheckSingleFaceMesh(shape)) {
      return true;
    }

    return false;
  }

  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();
    geometry::HalfedgeMeshPtr tmp =
        std::make_shared<geometry::HalfedgeMesh>(*shape->mesh());
    tmp->TransformUnitTrafoAndScale(shape->Size());
    tmp->ExtrudeRoofPyramid(args_->at(0).f,
                            args_->size() == 2 ? args_->at(1).f : 0.0);
    shape->SetMeshAndAdaptScope(tmp);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_roofShed, "roofShed", 1, {ValueType::FLOAT});
struct OpEval_roofShed : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckRange(0, roof_angle_min, roof_angle_max)) {
      return true;
    }

    const Shape* shape = context_->shape_stack->Top();
    if (CheckNonEmptyMesh(shape) || CheckSingleFaceMesh(shape)) {
      return true;
    }

    return false;
  }

  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();
    geometry::HalfedgeMeshPtr tmp =
        std::make_shared<geometry::HalfedgeMesh>(*shape->mesh());
    tmp->TransformUnitTrafoAndScale(shape->Size());
    tmp->ExtrudeRoofShed(args_->at(0).f);
    shape->SetMeshAndAdaptScope(tmp);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_color, "color", -1, {});
struct OpEval_color : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckArgNumber({1, 3, 4})) {
      return true;
    }

    if (args_->size() > 1) {
      for (unsigned i = 0; i < static_cast<unsigned>(args_->size()); ++i) {
        if (ValidateType(ValueType::FLOAT, i) || CheckRange(i, 0.0, 1.0)) {
          return true;
        }
      }
      return false;
    }

    if (ValidateType(ValueType::STRING, 0)) {
      return true;
    }
    if (util::ValidateHexColor(*args_->at(0).s, true)) {
      error_message_ = "The parameter for shape operation 'color' is not a";
      error_message_ += " valid color in hex format.";
      return true;
    }
    return false;
  }

  bool Eval() final {
    Vec4 color;
    if (args_->size() == 4) {
      color =
          Vec4(args_->at(0).f, args_->at(1).f, args_->at(2).f, args_->at(3).f);
    } else if (args_->size() == 3) {
      color = Vec4(args_->at(0).f, args_->at(1).f, args_->at(2).f, 1.0);
    } else /* if (args_->size() == 1) */ {
      color = util::ParseHexColor(*args_->at(0).s);
    }
    context_->shape_stack->Top()->SetColor(color);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_metallic, "metallic", 1, {ValueType::FLOAT});
struct OpEval_metallic : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckRange(0, 0.0, 1.0)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    context_->shape_stack->Top()->SetMetallic(args_->at(0).f);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_roughness, "roughness", 1, {ValueType::FLOAT});
struct OpEval_roughness : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckRange(0, 0.0, 1.0)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    context_->shape_stack->Top()->SetRoughness(args_->at(0).f);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_reflectance, "reflectance", 1, {ValueType::FLOAT});
struct OpEval_reflectance : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckRange(0, 0.0, 1.0)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    context_->shape_stack->Top()->SetReflectance(args_->at(0).f);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_texture, "texture", 1, {ValueType::STRING});
struct OpEval_texture : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (*args_->at(0).s == "") {
      error_message_ =
          "The parameter for shape operation 'texture' cannot be the empty "
          "string.";
      return true;
    }
    return false;
  }

  bool Eval() final {
    const std::string asset_uri = context_->base_path + *args_->at(0).s;
    const std::ifstream file(asset_uri.c_str());
    if (!file.good()) {
      std::ostringstream oss;
      oss << "The parameter for shape operation 'texture' is not a "
             "valid/existing path";
      oss << " to a file '" << *args_->at(0).s << "'.";
      error_message_ = oss.str();
      return true;
    }

    context_->shape_stack->Top()->SetTextureDiffuse(*args_->at(0).s);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_textureNone, "textureNone", 0, {});
struct OpEval_textureNone : ShapeOpEvaluator {
  bool Eval() final {
    context_->shape_stack->Top()->SetTextureDiffuse("");
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_materialName, "materialName", 1, {ValueType::STRING});
struct OpEval_materialName : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (*args_->at(0).s == "") {
      error_message_ =
          "The parameter for shape operation 'materialName' cannot be the "
          "empty string.";
      return true;
    }

    const std::string& name = *args_->at(0).s;
    for (const char c : name) {
      if (!(isalpha(c) || isdigit(c) || c == '-' || c == '_')) {
        error_message_ = "The 'materialName' must consist of [_-0-9a-zA-Z].";
        return true;
      }
    }
    return false;
  }

  bool Eval() final {
    context_->shape_stack->Top()->SetMaterialName(*args_->at(0).s);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_print, "print", 1, {ValueType::STRING});
struct OpEval_print : ShapeOpEvaluator {
  bool Eval() final {
    std::cout << *args_->at(0).s;
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_printLn, "printLn", 1, {ValueType::STRING});
struct OpEval_printLn : ShapeOpEvaluator {
  bool Eval() final {
    std::cout << *args_->at(0).s << '\n';
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_octreeAdd, "octreeAdd", 0, {});
struct OpEval_octreeAdd : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckNonEmptyMesh(context_->shape_stack->Top())) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    const Shape* shape = context_->shape_stack->Top();

    // This will get deleted when the octree is destroyed.
    OcclusionShape* occ_shape = new OcclusionShape(shape);

    // TODO(stefalie): Currently, we only do queries based on the OBBs of the
    // shape's scopes. Ideally, in the future, we will insert the complete mesh
    // in to the octree (likely in the form of a BVH of AABBs).
    context_->interpreter->octree()->InsertObject(occ_shape);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_push, "[", 0, {});
struct OpEval_push : ShapeOpEvaluator {
  bool Eval() final {
    context_->shape_stack->Push(
        std::make_unique<Shape>(*context_->shape_stack->Top()));
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_pop, "]", 0, {});
struct OpEval_pop : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (context_->shape_stack->Size() <= context_->shape_stack_start_size) {
      error_message_ =
          "There are more pops ']' than pushes '[' in a shape operation "
          "string.";
      return true;
    }
    return false;
  }

  bool Eval() final {
    context_->shape_stack->Pop();
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_set, "set", -1, {});
struct OpEval_set : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    // Check number of arguments.
    if (args_->size() != 2) {
      std::ostringstream oss;
      oss << "Shape operation 'set' takes 2 arguments but " << args_->size()
          << " were provided.";
      error_message_ = oss.str();
      return true;
    }

    // Check type of first argument.
    if (ValidateType(ValueType::STRING, 0)) {
      return true;
    }

    // Check that the first argument is a valid name for a custom shape
    // attribute (or more generally for a variable).
    const std::string& attr_name = *args_->at(0).s;
    bool attr_name_invalid = false;

    if (attr_name.empty() || !(isalpha(attr_name[0]) || attr_name[0] == '_')) {
      attr_name_invalid = true;
    }
    for (size_t i = 1; i < attr_name.size(); ++i) {
      if (!(isalpha(attr_name[0]) || isdigit(attr_name[0]) ||
            attr_name[0] == '_')) {
        attr_name_invalid = true;
        break;
      }
    }

    if (attr_name_invalid) {
      std::ostringstream oss;
      oss << "Parameter 1 for shape operation 'set' is not a valid name for a "
             "custom shape attribute.";
      oss << " (Provided name: " << args_->at(0) << ".)";
      error_message_ = oss.str();
      return true;
    }
    return false;
  }

  bool Eval() final {
    context_->shape_stack->Top()->SetCustomAttribute(*args_->at(0).s,
                                                     args_->at(1));
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_hide, "hide", 0, {});
struct OpEval_hide : ShapeOpEvaluator {
  bool Eval() final {
    context_->shape_stack->Top()->set_visible(false);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_show, "show", 0, {});
struct OpEval_show : ShapeOpEvaluator {
  bool Eval() final {
    context_->shape_stack->Top()->set_visible(true);
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_repeat, "repeat", 2,
                  {ValueType::INT, ValueType::SHAPE_OP_STRING});
struct OpEval_repeat : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckRange(0, 0, 100000)) {
      return true;
    }
    return false;
  }

  bool Eval() override {
    for (int i = 0; i < args_->at(0).i; ++i) {
      context_->shape_stack->Push(
          std::make_unique<Shape>(*context_->shape_stack->Top()));
      context_->shape_stack->Top()->set_index(i);
      try {
        context_->interpreter->ApplyShapeOpString(*args_->at(1).ops,
                                                  context_->derivation_list);
      } catch (const RuntimeError& error) {
        PropagateError(error);
        return true;
      }
      context_->shape_stack->Pop();
    }
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_repeatNoPush, "repeatNoPush", 2,
                  {ValueType::INT, ValueType::SHAPE_OP_STRING});
struct OpEval_repeatNoPush : OpEval_repeat {
  bool Eval() final {
    const int tmp_index = context_->shape_stack->Top()->index();
    for (int i = 0; i < args_->at(0).i; ++i) {
      context_->shape_stack->Top()->set_index(i);
      try {
        context_->interpreter->ApplyShapeOpString(*args_->at(1).ops,
                                                  context_->derivation_list);
      } catch (const RuntimeError& error) {
        PropagateError(error);
        return true;
      }
    }
    context_->shape_stack->Top()->set_index(tmp_index);
    return false;
  }
};

static bool ApplySplit(ShapeOpEvaluator* eval, const Vec3& dir, Scalar size,
                       const ScalarVec& sizes,
                       const std::vector<const ShapeOpString*>& ops) {
  assert(sizes.size() == ops.size());
  if (sizes.empty()) {
    return false;
  }

  DerivationContext* context = eval->context();
  Shape* shape = context->shape_stack->Top();

  geometry::HalfedgeMeshPtr tmp =
      std::make_shared<geometry::HalfedgeMesh>(*shape->mesh());
  tmp->TransformUnitTrafoAndScale(shape->Size());

  Scalar distance = 0.0;
  for (int i = 0; i < static_cast<int>(sizes.size()); ++i) {
    distance += sizes[i];

    geometry::HalfedgeMeshPtr below, above;
    if (i < static_cast<int>(sizes.size() - 1) ||
        distance < size - geometry::EPSILON) {
      tmp->Split(geometry::Plane3(dir, distance), &below, &above);
      tmp = above;
    } else {
      below = tmp;
    }

    if (!below->Empty()) {
      context->shape_stack->Push(
          std::make_unique<Shape>(*context->shape_stack->Top()));
      context->shape_stack->Top()->SetMeshAndAdaptScope(below);
      context->shape_stack->Top()->set_index(i);
      try {
        context->interpreter->ApplyShapeOpString(*ops[i],
                                                 context->derivation_list);
      } catch (const RuntimeError& error) {
        eval->PropagateError(error);
        return true;
      }
      context->shape_stack->Pop();
    }
  }

  return false;
}

// This could also be done in terms of the 'ApplyGenericSplit' function below.
static bool ApplyRepeatSplit(ShapeOpEvaluator* eval, int dir_idx, Scalar size,
                             const ShapeOpString* ops) {
  assert(dir_idx >= 0 && dir_idx < 3);

  const Scalar total_size =
      eval->context()->shape_stack->Top()->Size()[dir_idx];
  Vec3 dir = Vec3::Zero();
  dir[dir_idx] = 1.0;

  const int num_reps =
      std::max(static_cast<int>(std::round(total_size / size)), 1);
  const Scalar final_size = total_size / num_reps;

  const ScalarVec sizes(num_reps, final_size);
  const std::vector<const ShapeOpString*> ops_vec(num_reps, ops);

  return ApplySplit(eval, dir, total_size, sizes, ops_vec);
}

REGISTER_SHAPE_OP(OpEval_splitRepeatX, "splitRepeatX", 2,
                  {ValueType::FLOAT, ValueType::SHAPE_OP_STRING});
struct OpEval_splitRepeatX : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckNonEmptyMesh(context_->shape_stack->Top())) {
      return true;
    }
    if (CheckGreaterEqualThanValue(0, 0.0001)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    return ApplyRepeatSplit(this, 0, args_->at(0).f, args_->at(1).ops);
  }
};

REGISTER_SHAPE_OP(OpEval_splitRepeatY, "splitRepeatY", 2,
                  {ValueType::FLOAT, ValueType::SHAPE_OP_STRING});
struct OpEval_splitRepeatY : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckNonEmptyMesh(context_->shape_stack->Top())) {
      return true;
    }
    if (CheckGreaterEqualThanValue(0, 0.0001)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    return ApplyRepeatSplit(this, 1, args_->at(0).f, args_->at(1).ops);
  }
};

REGISTER_SHAPE_OP(OpEval_splitRepeatZ, "splitRepeatZ", 2,
                  {ValueType::FLOAT, ValueType::SHAPE_OP_STRING});
struct OpEval_splitRepeatZ : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckNonEmptyMesh(context_->shape_stack->Top())) {
      return true;
    }
    if (CheckGreaterEqualThanValue(0, 0.0001)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    return ApplyRepeatSplit(this, 2, args_->at(0).f, args_->at(1).ops);
  }
};

struct SplitRepeat {
  int start;
  int end;
};

static bool ParseGenericSplit(const std::string& pattern,
                              std::vector<bool>* fixed,
                              std::vector<bool>* outer,
                              std::vector<SplitRepeat>* repeats) {
  if (pattern.empty()) {
    return true;
  }

  bool is_outer = true;

  for (size_t i = 0; i < pattern.size(); ++i) {
    switch (pattern[i]) {
      case '(':
        if (!is_outer) {
          return true;
        }
        is_outer = false;
        if (repeats) {
          repeats->push_back(SplitRepeat());
          repeats->back().start = static_cast<int>(fixed->size());
        }
        break;
      case ')':
        if (is_outer) {
          return true;
        }
        is_outer = true;
        if (repeats) {
          repeats->back().end = static_cast<int>(fixed->size());
        }
        break;
      case 'f':
        fixed->push_back(true);
        outer->push_back(is_outer);
        break;
      case 's':
        fixed->push_back(false);
        outer->push_back(is_outer);
        break;
      default:
        return true;
    }
  }
  if (!is_outer) {
    return true;
  }

  return false;
}

static bool ValidateGenericSplit(ShapeOpEvaluator* eval) {
  if (eval->CheckNonEmptyMesh(eval->context()->shape_stack->Top())) {
    return true;
  }

  if (eval->args()->size() < 3) {
    std::ostringstream oss;
    oss << "Shape operation '" << *eval->name() << "' needs at least 3 ";
    oss << "arguments.";
    eval->set_error_message(oss.str());
    return true;
  }

  if (eval->ValidateType(ValueType::STRING, 0)) {
    return true;
  }

  std::vector<bool> fixed, outer;
  if (ParseGenericSplit(*eval->args()->at(0).s, &fixed, &outer, nullptr)) {
    std::ostringstream oss;
    oss << "Shape operation '" << *eval->name() << "' is called with an ";
    oss << "invalid split pattern '" << *eval->args()->at(0).s << "'.";
    eval->set_error_message(oss.str());
    return true;
  }

  const size_t num_required_args = 1 + fixed.size() * 2;
  if (eval->args()->size() != num_required_args) {
    std::ostringstream oss;
    oss << "Shape operation '" << *eval->name() << "' needs ";
    oss << num_required_args << " arguments, but " << eval->args()->size();
    oss << " were provided.";
    eval->set_error_message(oss.str());
    return true;
  }

  for (unsigned i = 1; i < eval->args()->size(); i += 2) {
    if (eval->ValidateType(ValueType::FLOAT, i) ||
        eval->CheckGreaterEqualThanValue(i, 0.0001) ||
        eval->ValidateType(ValueType::SHAPE_OP_STRING, i + 1)) {
      return true;
    }
  }
  return false;
}

static bool ApplyGenericSplit(ShapeOpEvaluator* eval, int dir_idx) {
  assert(dir_idx >= 0 && dir_idx < 3);

  std::vector<bool> fixed, is_outer;
  std::vector<SplitRepeat> repeats;
  ParseGenericSplit(*eval->args()->at(0).s, &fixed, &is_outer, &repeats);

  ScalarVec sizes;
  std::vector<const ShapeOpString*> ops_vec;
  for (size_t i = 1; i < eval->args()->size(); i += 2) {
    sizes.push_back(eval->args()->at(i).f);
    ops_vec.push_back(eval->args()->at(i + 1).ops);
  }

  assert(sizes.size() == fixed.size() && fixed.size() == is_outer.size());

  const Scalar total_size =
      eval->context()->shape_stack->Top()->Size()[dir_idx];
  Vec3 dir = Vec3::Zero();
  dir[dir_idx] = 1.0;

  Scalar outer_fixed = 0.0;
  Scalar outer_stretch = 0.0;
  int num_outer_stretch = 0;
  Scalar inner_fixed = 0.0;
  Scalar inner_stretch = 0.0;
  int num_inner_stretch = 0;

  for (size_t i = 0; i < sizes.size(); ++i) {
    if (is_outer[i]) {
      if (fixed[i]) {
        outer_fixed += sizes[i];
      } else {
        outer_stretch += sizes[i];
        ++num_outer_stretch;
      }
    } else {
      if (fixed[i]) {
        inner_fixed += sizes[i];
      } else {
        inner_stretch += sizes[i];
        ++num_inner_stretch;
      }
    }
  }

  // Calculate repetition counts.
  const Scalar outer = outer_fixed + outer_stretch;
  const Scalar inner = inner_fixed + inner_stretch;

  int num_reps = 0;
  if (outer < total_size - geometry::EPSILON && inner > 0) {
    const int num_reps_ideal =
        static_cast<int>(std::round((total_size - outer) / inner));

    int num_reps_max = INT_MAX;
    if (inner_fixed > 0.0) {
      num_reps_max = static_cast<int>((total_size - outer_fixed) / inner_fixed);
    }
    assert(num_reps_ideal >= 0 && num_reps_max >= 0);

    num_reps = std::min(num_reps_ideal, num_reps_max);

    // There is the special case that the ideal number of repetitions is zero,
    // but the outer part cannot stretch to fill the everything, then it is
    // sometimes possible that everything can be filled by having a single
    // repetition of a stretchable 'inner'.
    if (num_reps_ideal == 0 && num_reps_max > 0 && num_outer_stretch == 0 &&
        num_inner_stretch != 0 &&
        (total_size - outer_fixed) >= inner_fixed - geometry::EPSILON) {
      num_reps = 1;
    }
  }

  // Compute stretching.
  const Scalar avail_stretch =
      total_size - outer_fixed - num_reps * inner_fixed;

  const Scalar req_stretch = outer_stretch + num_reps * inner_stretch;
  const Scalar stretch_scale_factor = avail_stretch / req_stretch;

  assert(avail_stretch > -geometry::EPSILON || num_reps == 0);

  for (size_t i = 0; i < sizes.size(); ++i) {
    if (!fixed[i]) {
      sizes[i] *= stretch_scale_factor;
    }
  }

  // Compute resulting sizes.
  ScalarVec final_sizes;
  std::vector<const ShapeOpString*> final_ops_vec;

  Scalar sum_sizes = 0.0;
  int rep_idx = 0;
  int i = 0;
  while (i < static_cast<int>(sizes.size())) {
    // Deal with entering or leaving an inner pattern.
    if (rep_idx < static_cast<int>(repeats.size())) {
      if (i == repeats[rep_idx].start) {
        const int num_elems = repeats[rep_idx].end - i;

        // Add elements of inner pattern 'num_reps' times.
        for (int j = 0; j < num_reps; ++j) {
          for (int k = 0; k < num_elems; ++k) {
            assert(sizes[i + k] > 0.0);
            final_sizes.push_back(sizes[i + k]);
            final_ops_vec.push_back(ops_vec[i + k]);
            sum_sizes += final_sizes.back();
            assert(sum_sizes < total_size + geometry::EPSILON);
          }
        }

        i += num_elems;
        ++rep_idx;
        continue;
      }
    }

    // Break out when the entire length is used up.
    if (sum_sizes + sizes[i] >= total_size + geometry::EPSILON) {
      break;
    }

    // Add outer elements.
    final_sizes.push_back(sizes[i]);
    final_ops_vec.push_back(ops_vec[i]);
    sum_sizes += final_sizes.back();
    ++i;
  }

  assert(sum_sizes < total_size + geometry::EPSILON);
  assert(
      (num_outer_stretch == 0 && (num_inner_stretch == 0 || num_reps == 0)) ||
      fabs(sum_sizes - total_size) < geometry::EPSILON);

  return ApplySplit(eval, dir, total_size, final_sizes, final_ops_vec);
}

REGISTER_SHAPE_OP(OpEval_splitX, "splitX", -1, {});
struct OpEval_splitX : ShapeOpEvaluator {
  bool ValidateSpecial() final { return ValidateGenericSplit(this); }

  bool Eval() final { return ApplyGenericSplit(this, 0); }
};

REGISTER_SHAPE_OP(OpEval_splitY, "splitY", -1, {});
struct OpEval_splitY : ShapeOpEvaluator {
  bool ValidateSpecial() final { return ValidateGenericSplit(this); }

  bool Eval() final { return ApplyGenericSplit(this, 1); }
};

REGISTER_SHAPE_OP(OpEval_splitZ, "splitZ", -1, {});
struct OpEval_splitZ : ShapeOpEvaluator {
  bool ValidateSpecial() final { return ValidateGenericSplit(this); }

  bool Eval() final { return ApplyGenericSplit(this, 2); }
};

REGISTER_SHAPE_OP(OpEval_splitFace, "splitFace", -1, {});
struct OpEval_splitFace : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckNonEmptyMesh(context_->shape_stack->Top())) {
      return true;
    }

    if (args_->empty()) {
      error_message_ = "Shape operation 'splitFace' cannot be called without ";
      error_message_ += "arguments.";
      return true;
    }

    if (args_->size() % 2 != 0) {
      error_message_ = "Shape operation 'splitFace' needs an even number of ";
      error_message_ += "arguments.";
      return true;
    }

    static const std::unordered_set<std::string> valid_selectors = {
        "front",  "back", "left",       "right",    "top",
        "bottom", "all",  "horizontal", "vertical", "side"};

    for (int i = 0; i < static_cast<int>(args_->size()); i += 2) {
      if (ValidateType(ValueType::STRING, i)) {
        return true;
      }

      if (valid_selectors.find(*args_->at(i).s) == valid_selectors.end()) {
        std::ostringstream oss;
        oss << "Parameter " << (i + 1) << " for shape operation 'splitFace' is";
        oss << " not a valid string selector.";
        error_message_ = oss.str();
        return true;
      }

      if (ValidateType(ValueType::SHAPE_OP_STRING, i + 1)) {
        return true;
      }
    }
    return false;
  }

  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();

    std::unordered_set<unsigned> used_face_indices;
    int index_counter = 0;

    geometry::HalfedgeMeshPtr tmp =
        std::make_shared<geometry::HalfedgeMesh>(*shape->mesh());
    tmp->TransformUnitTrafoAndScale(shape->Size());

    for (size_t i = 0; i < args_->size(); i += 2) {
      const std::string& selector = *args_->at(i).s;
      const ShapeOpString& ops = *args_->at(i + 1).ops;

      for (int j = 0; j < tmp->NumFaces(); ++j) {
        if (used_face_indices.find(j) != used_face_indices.end()) {
          continue;
        }

        const Vec3 normal = tmp->GetFaceNormal(j);
        bool is_match = false;

        if (selector == "all") {
          is_match = true;
        } else if (selector == "horizontal") {
          if (fabs(normal.dot(Vec3::UnitY())) >= cos(11.25 / 180.0 * M_PI)) {
            is_match = true;
          }
        } else if (selector == "vertical") {
          if (fabs(normal.dot(Vec3::UnitY())) < cos(78.75 / 180.0 * M_PI)) {
            is_match = true;
          }
        } else if (selector == "side") {
          if (fabs(normal.dot(Vec3::UnitY())) < cos(11.25 / 180.0 * M_PI)) {
            is_match = true;
          }
        } else if (selector == "left") {
          if (normal.x() < 0.0 && fabs(normal.x()) >= fabs(normal.y()) &&
              fabs(normal.x()) > fabs(normal.z())) {
            is_match = true;
          }
        } else if (selector == "right") {
          if (normal.x() > 0.0 && fabs(normal.x()) >= fabs(normal.y()) &&
              fabs(normal.x()) > fabs(normal.z())) {
            is_match = true;
          }
        } else if (selector == "bottom") {
          if (normal.y() < 0.0 && fabs(normal.y()) >= fabs(normal.z()) &&
              fabs(normal.y()) > fabs(normal.x())) {
            is_match = true;
          }
        } else if (selector == "top") {
          if (normal.y() > 0.0 && fabs(normal.y()) >= fabs(normal.z()) &&
              fabs(normal.y()) > fabs(normal.x())) {
            is_match = true;
          }
        } else if (selector == "back") {
          if (normal.z() < 0.0 && fabs(normal.z()) >= fabs(normal.x()) &&
              fabs(normal.z()) > fabs(normal.y())) {
            is_match = true;
          }
        } else if (selector == "front") {
          if (normal.z() > 0.0 && fabs(normal.z()) >= fabs(normal.x()) &&
              fabs(normal.z()) > fabs(normal.y())) {
            is_match = true;
          }
        }

        if (is_match) {
          used_face_indices.insert(j);

          //  Create the new shape.
          geometry::HalfedgeMeshPtr face_component = tmp->GetFaceComponent(j);

          context_->shape_stack->Push(std::make_unique<Shape>(*shape));
          context_->shape_stack->Top()->SetMeshAfterFaceSplit(face_component,
                                                              normal);
          context_->shape_stack->Top()->set_index(index_counter++);
          try {
            context_->interpreter->ApplyShapeOpString(
                ops, context_->derivation_list);
          } catch (const RuntimeError& error) {
            PropagateError(error);
            return true;
          }
          context_->shape_stack->Pop();
        }
      }
    }
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_splitFaceAlongDir, "splitFaceAlongDir", 5,
                  {ValueType::FLOAT, ValueType::FLOAT, ValueType::FLOAT,
                   ValueType::FLOAT, ValueType::SHAPE_OP_STRING});

struct OpEval_splitFaceAlongDir : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    if (CheckNonEmptyMesh(context_->shape_stack->Top()) ||
        CheckDirectionVector(0) || CheckRange(3, 0.0, 180.0)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();
    const Vec3 direction =
        Vec3(args_->at(0).f, args_->at(1).f, args_->at(2).f).normalized();

    geometry::HalfedgeMeshPtr tmp =
        std::make_shared<geometry::HalfedgeMesh>(*shape->mesh());
    tmp->TransformUnitTrafoAndScale(shape->Size());

    IdxVec face_indices;
    int index_counter = 0;
    for (int i = 0; i < tmp->NumFaces(); ++i) {
      const Vec3 normal = tmp->GetFaceNormal(i);
      const Scalar cos_angle = normal.dot(direction);

      if (cos_angle >= cos(args_->at(3).f / 180.0 * M_PI) - geometry::EPSILON) {
        geometry::HalfedgeMeshPtr face_component = tmp->GetFaceComponent(i);

        context_->shape_stack->Push(std::make_unique<Shape>(*shape));
        context_->shape_stack->Top()->SetMeshAfterFaceSplit(face_component,
                                                            normal);
        context_->shape_stack->Top()->set_index(index_counter++);
        try {
          context_->interpreter->ApplyShapeOpString(*args_->back().ops,
                                                    context_->derivation_list);
        } catch (const RuntimeError& error) {
          PropagateError(error);
          return true;
        }
        context_->shape_stack->Pop();
      }
    }
    return false;
  }
};

REGISTER_SHAPE_OP(OpEval_splitFaceByIndex, "splitFaceByIndex", -1, {});
struct OpEval_splitFaceByIndex : ShapeOpEvaluator {
  bool ValidateSpecial() final {
    const Shape* shape = context_->shape_stack->Top();
    CheckNonEmptyMesh(shape);

    if (args_->size() < 2) {
      std::ostringstream oss;
      oss << "Shape operation 'splitFaceByIndex' requires at least 2 ";
      oss << "arguments.";
      error_message_ = oss.str();
      return true;
    }

    for (unsigned i = 0; i < args_->size() - 1; ++i) {
      if (ValidateType(ValueType::INT, i)) {
        return true;
      }

      if (args_->at(i).i < 0 || args_->at(i).i >= shape->mesh()->NumFaces()) {
        std::ostringstream oss;
        oss << "Parameter " << (i + 1) << " for shape operation ";
        oss << "'splitFaceByIndex' must be non-negative and smaller than the ";
        oss << "number of faces in the shape's mesh.";
        error_message_ = oss.str();
        return true;
      }
    }

    if (ValidateType(ValueType::SHAPE_OP_STRING,
                     static_cast<unsigned>(args_->size()) - 1)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    Shape* shape = context_->shape_stack->Top();

    IdxVec face_indices;
    for (size_t i = 0; i < args_->size() - 1; ++i) {
      face_indices.push_back(args_->at(i).i);
    }

    geometry::HalfedgeMeshPtr tmp =
        std::make_shared<geometry::HalfedgeMesh>(*shape->mesh());
    tmp->TransformUnitTrafoAndScale(shape->Size());

    for (unsigned i = 0; i < face_indices.size(); ++i) {
      geometry::HalfedgeMeshPtr face_component =
          tmp->GetFaceComponent(face_indices[i]);
      const Vec3 normal = tmp->GetFaceNormal(face_indices[i]);

      context_->shape_stack->Push(std::make_unique<Shape>(*shape));
      context_->shape_stack->Top()->SetMeshAfterFaceSplit(face_component,
                                                          normal);
      context_->shape_stack->Top()->set_index(i);
      try {
        context_->interpreter->ApplyShapeOpString(*args_->back().ops,
                                                  context_->derivation_list);
      } catch (const RuntimeError& error) {
        PropagateError(error);
        return true;
      }
      context_->shape_stack->Pop();
    }
    return false;
  }
};

}  // namespace shapeml
