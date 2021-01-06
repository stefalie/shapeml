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
#include <sstream>

#include "shapeml/asset_cache.h"
#include "shapeml/evaluator.h"
#include "shapeml/grammar.h"
#include "shapeml/interpreter.h"
#include "shapeml/shape.h"
#include "shapeml/util/noise.h"

namespace shapeml {

template <>
const char* FunctionEvaluator::type_name_ = "function";

template class Evaluator<FuncEvalContext>;

// The rest of this file contains the evaluators for all built-in functions.

REGISTER_FUNCTION(FuncEval_bool, "bool", 1, {ValueType::BOOL});
struct FuncEval_bool : FunctionEvaluator {
  bool Eval() final {
    context_->return_value = args_->at(0);
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_int, "int", 1, {ValueType::INT});
struct FuncEval_int : FunctionEvaluator {
  bool Eval() final {
    context_->return_value = args_->at(0);
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_float, "float", 1, {ValueType::FLOAT});
struct FuncEval_float : FunctionEvaluator {
  bool Eval() final {
    context_->return_value = args_->at(0);
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_string, "string", 1, {ValueType::STRING});
struct FuncEval_string : FunctionEvaluator {
  bool Eval() final {
    context_->return_value = args_->at(0);
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_rand_int, "rand_int", 2,
                  {ValueType::INT, ValueType::INT});
struct FuncEval_rand_int : FunctionEvaluator {
  bool ValidateSpecial() final {
    if (CheckGreaterThan(1, 0)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    auto dist =
        std::uniform_int_distribution<int>(args_->at(0).i, args_->at(1).i);
    context_->return_value =
        Value(dist(context_->interpreter->random_generator()));
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_rand_uniform, "rand_uniform", 2,
                  {ValueType::FLOAT, ValueType::FLOAT});
struct FuncEval_rand_uniform : FunctionEvaluator {
  bool ValidateSpecial() final {
    if (CheckGreaterThan(1, 0)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    auto dist =
        std::uniform_real_distribution<Scalar>(args_->at(0).f, args_->at(1).f);
    context_->return_value =
        Value(dist(context_->interpreter->random_generator()));
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_rand_normal, "rand_normal", 2,
                  {ValueType::FLOAT, ValueType::FLOAT});
struct FuncEval_rand_normal : FunctionEvaluator {
  bool ValidateSpecial() final {
    if (CheckGreaterThanZero(1)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    auto dist =
        std::normal_distribution<Scalar>(args_->at(0).f, args_->at(1).f);
    context_->return_value =
        Value(dist(context_->interpreter->random_generator()));
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_noise, "noise", -1, {});
struct FuncEval_noise : FunctionEvaluator {
  bool ValidateSpecial() final {
    if (CheckArgNumber({2, 3})) {
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
    const float scale = 16.0f / 128;
    const float f0 = static_cast<float>(args_->at(0).f);
    const float f1 = static_cast<float>(args_->at(1).f);

    if (args_->size() == 2) {
      context_->return_value =
          Value(util::PerlinNoise2(f0 * scale, f1 * scale));
    } else /* (args_->size() == 3) */ {
      const float f2 = static_cast<float>(args_->at(2).f);
      context_->return_value =
          Value(util::PerlinNoise3(f0 * scale, f1 * scale, f2 * scale));
    }
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_fBm, "fBm", -1, {});
struct FuncEval_fBm : FunctionEvaluator {
  bool ValidateSpecial() final {
    if (CheckArgNumber({3, 5, 4, 6})) {
      return true;
    }
    for (unsigned i = 0; i < args_->size(); ++i) {
      // Number of octaves needs to be an int.
      // In 2D it's the 3rd, in 3D the 4th arguments.
      if ((args_->size() % 2 == 1 && i == 2) ||
          (args_->size() % 2 == 0 && i == 3)) {
        if (ValidateType(ValueType::INT, i)) {
          return true;
        }
      } else {
        if (ValidateType(ValueType::FLOAT, i)) {
          return true;
        }
      }
    }
    return false;
  }

  bool Eval() final {
    const float scale = 4.0f / 128.0f;
    const float f0 = static_cast<float>(args_->at(0).f);
    const float f1 = static_cast<float>(args_->at(1).f);

    if (args_->size() == 3) {
      context_->return_value =
          Value(util::fBm2(f0 * scale, f1 * scale, args_->at(2).i));
    } else if (args_->size() == 5) {
      const float f3 = static_cast<float>(args_->at(3).f);
      const float f4 = static_cast<float>(args_->at(4).f);
      context_->return_value =
          Value(util::fBm2(f0 * scale, f1 * scale, args_->at(2).i, f3, f4));
    } else if (args_->size() == 4) {
      const float f2 = static_cast<float>(args_->at(2).f);
      context_->return_value =
          Value(util::fBm3(f0 * scale, f1 * scale, f2 * scale, args_->at(3).i));
    } else /* if (args_->size() == 6) */ {
      const float f2 = static_cast<float>(args_->at(2).f);
      const float f4 = static_cast<float>(args_->at(4).f);
      const float f5 = static_cast<float>(args_->at(5).f);
      context_->return_value = Value(util::fBm3(
          f0 * scale, f1 * scale, f2 * scale, args_->at(3).i, f4, f5));
    }
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_abs, "abs", -1, {});
struct FuncEval_abs : FunctionEvaluator {
  bool ValidateSpecial() final {
    if (CheckArgNumber({1})) {
      return true;
    }

    Value& p1 = args_->at(0);
    bool error = false;
    if (p1.type != ValueType::FLOAT && p1.type != ValueType::INT) {
      error = true;
    }

    if (error) {
      std::ostringstream oss;
      oss << "The parameter for 'abs' needs to be";
      oss << ValueType2Str(ValueType::INT) << " or ";
      oss << ValueType2Str(ValueType::FLOAT) << '.';
      error_message_ = oss.str();
      return true;
    }
    return false;
  }

  bool Eval() final {
    context_->return_value = (args_->at(0).type == ValueType::FLOAT)
                                 ? Value(std::fabs(args_->at(0).f))
                                 : Value(std::abs(args_->at(0).i));
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_ceil, "ceil", 1, {ValueType::FLOAT});
struct FuncEval_ceil : FunctionEvaluator {
  bool Eval() final {
    context_->return_value = Value(std::ceil(args_->at(0).f));
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_floor, "floor", 1, {ValueType::FLOAT});
struct FuncEval_floor : FunctionEvaluator {
  bool Eval() final {
    context_->return_value = Value(std::floor(args_->at(0).f));
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_round, "round", 1, {ValueType::FLOAT});
struct FuncEval_round : FunctionEvaluator {
  bool Eval() final {
    context_->return_value = Value(std::round(args_->at(0).f));
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_sign, "sign", 1, {ValueType::FLOAT});
struct FuncEval_sign : FunctionEvaluator {
  bool Eval() final {
    context_->return_value = Value(args_->at(0).f >= 0.0 ? 1 : -1);
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_fract, "fract", 1, {ValueType::FLOAT});
struct FuncEval_fract : FunctionEvaluator {
  bool Eval() final {
    context_->return_value =
        Value(args_->at(0).f - static_cast<int>(args_->at(0).f));
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_sin, "sin", 1, {ValueType::FLOAT});
struct FuncEval_sin : FunctionEvaluator {
  bool Eval() final {
    context_->return_value = Value(std::sin(args_->at(0).f / 180.0 * M_PI));
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_cos, "cos", 1, {ValueType::FLOAT});
struct FuncEval_cos : FunctionEvaluator {
  bool Eval() final {
    context_->return_value = Value(std::cos(args_->at(0).f / 180.0 * M_PI));
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_tan, "tan", 1, {ValueType::FLOAT});
struct FuncEval_tan : FunctionEvaluator {
  bool Eval() final {
    context_->return_value = Value(std::tan(args_->at(0).f / 180.0 * M_PI));
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_asin, "asin", 1, {ValueType::FLOAT});
struct FuncEval_asin : FunctionEvaluator {
  bool ValidateSpecial() final {
    if (CheckRange(0, -1.0, 1.0)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    context_->return_value = Value(std::asin(args_->at(0).f) * 180.0 / M_PI);
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_acos, "acos", 1, {ValueType::FLOAT});
struct FuncEval_acos : FunctionEvaluator {
  bool ValidateSpecial() final {
    if (CheckRange(0, -1.0, 1.0)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    context_->return_value = Value(std::acos(args_->at(0).f) * 180.0 / M_PI);
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_atan, "atan", 1, {ValueType::FLOAT});
struct FuncEval_atan : FunctionEvaluator {
  bool Eval() final {
    context_->return_value = Value(std::atan(args_->at(0).f) * 180.0 / M_PI);
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_atan2, "atan2", 2,
                  {ValueType::FLOAT, ValueType::FLOAT});
struct FuncEval_atan2 : FunctionEvaluator {
  bool Eval() final {
    context_->return_value =
        Value(std::atan2(args_->at(0).f, args_->at(1).f) * 180.0 / M_PI);
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_sqrt, "sqrt", 1, {ValueType::FLOAT});
struct FuncEval_sqrt : FunctionEvaluator {
  bool ValidateSpecial() final {
    if (CheckGreaterEqualThanZero(0)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    context_->return_value = Value(std::sqrt(args_->at(0).f));
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_pow, "pow", 2, {ValueType::FLOAT, ValueType::FLOAT});
struct FuncEval_pow : FunctionEvaluator {
  bool Eval() final {
    context_->return_value = Value(std::pow(args_->at(0).f, args_->at(1).f));
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_exp, "exp", 1, {ValueType::FLOAT});
struct FuncEval_exp : FunctionEvaluator {
  bool Eval() final {
    context_->return_value = Value(std::exp(args_->at(0).f));
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_log, "log", 1, {ValueType::FLOAT});
struct FuncEval_log : FunctionEvaluator {
  bool ValidateSpecial() final {
    if (CheckGreaterThanZero(0)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    context_->return_value = Value(std::log(args_->at(0).f));
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_log10, "log10", 1, {ValueType::FLOAT});
struct FuncEval_log10 : FunctionEvaluator {
  bool ValidateSpecial() final {
    if (CheckGreaterThanZero(0)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    context_->return_value = Value(std::log10(args_->at(0).f));
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_max, "max", -1, {});
struct FuncEval_max : FunctionEvaluator {
  bool ValidateSpecial() final {
    if (CheckArgNumber({2})) {
      return true;
    }

    Value& p1 = args_->at(0);
    Value& p2 = args_->at(1);
    bool error = false;
    if (p1.type == ValueType::FLOAT || p2.type == ValueType::FLOAT) {
      if (p1.ChangeType(ValueType::FLOAT) || p2.ChangeType(ValueType::FLOAT)) {
        error = true;
      }
    } else {
      if (p1.ChangeType(ValueType::INT) || p2.ChangeType(ValueType::INT)) {
        error = true;
      }
    }

    if (error) {
      std::ostringstream oss;
      oss << "Cannot convert all parameters for 'min' to ";
      oss << ValueType2Str(ValueType::INT) << " or ";
      oss << ValueType2Str(ValueType::FLOAT) << '.';
      error_message_ = oss.str();
      return true;
    }
    return false;
  }

  bool Eval() final {
    if (args_->at(0).type == ValueType::INT) {
      context_->return_value = Value(std::max(args_->at(0).i, args_->at(1).i));
    } else {
      context_->return_value = Value(std::max(args_->at(0).f, args_->at(1).f));
    }
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_min, "min", -1, {});
struct FuncEval_min : FunctionEvaluator {
  bool ValidateSpecial() final {
    if (CheckArgNumber({2})) {
      return true;
    }

    Value& p1 = args_->at(0);
    Value& p2 = args_->at(1);
    bool error = false;
    if (p1.type == ValueType::FLOAT || p2.type == ValueType::FLOAT) {
      if (p1.ChangeType(ValueType::FLOAT) || p2.ChangeType(ValueType::FLOAT)) {
        error = true;
      }
    } else {
      if (p1.ChangeType(ValueType::INT) || p2.ChangeType(ValueType::INT)) {
        error = true;
      }
    }

    if (error) {
      std::ostringstream oss;
      oss << "Cannot convert all parameters for 'min' to ";
      oss << ValueType2Str(ValueType::INT) << " or ";
      oss << ValueType2Str(ValueType::FLOAT) << '.';
      error_message_ = oss.str();
      return true;
    }
    return false;
  }

  bool Eval() final {
    if (args_->at(0).type == ValueType::INT) {
      context_->return_value = Value(std::min(args_->at(0).i, args_->at(1).i));
    } else {
      context_->return_value = Value(std::min(args_->at(0).f, args_->at(1).f));
    }
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_clamp, "clamp", 3,
                  {ValueType::FLOAT, ValueType::FLOAT, ValueType::FLOAT});
struct FuncEval_clamp : FunctionEvaluator {
  bool ValidateSpecial() final {
    if (CheckGreaterEqualThan(1, 0)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    if (args_->at(2).f < args_->at(0).f) {
      context_->return_value = args_->at(0);
    } else if (args_->at(2).f > args_->at(1).f) {
      context_->return_value = args_->at(1);
    } else {
      context_->return_value = args_->at(2);
    }
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_step, "step", 2,
                  {ValueType::FLOAT, ValueType::FLOAT});
struct FuncEval_step : FunctionEvaluator {
  bool Eval() final {
    if (args_->at(1).f < args_->at(0).f) {
      context_->return_value = Value(0.0);
    }
    context_->return_value = Value(1.0);
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_smooth_step, "smooth_step", 3,
                  {ValueType::FLOAT, ValueType::FLOAT, ValueType::FLOAT});
struct FuncEval_smooth_step : FunctionEvaluator {
  bool ValidateSpecial() final {
    if (CheckGreaterThan(1, 0)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    // Based on:
    // https://www.khronos.org/registry/OpenGL-Refpages/gl4/html/smoothstep.xhtml
    const Scalar edge0 = args_->at(0).f;
    const Scalar edge1 = args_->at(1).f;
    Scalar t = (args_->at(2).f - edge0) / (edge1 - edge0);
    t = std::max(std::min(t, 1.0), 0.0);  // clamp(0, 1, t)

    context_->return_value = Value(t * t * (3.0 - 2.0 * t));
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_lerp, "lerp", 3,
                  {ValueType::FLOAT, ValueType::FLOAT, ValueType::FLOAT});
struct FuncEval_lerp : FunctionEvaluator {
  bool ValidateSpecial() final {
    if (CheckRange(2, 0.0, 1.0)) {
      return true;
    }
    return false;
  }

  bool Eval() final {
    const Scalar alpha = args_->at(2).f;
    context_->return_value =
        Value(args_->at(0).f * (1.0 - alpha) + args_->at(1).f * alpha);
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_pi, "pi", 0, {});
struct FuncEval_pi : FunctionEvaluator {
  bool Eval() final {
    context_->return_value = Value(M_PI);
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_seed, "seed", 0, {});
struct FuncEval_seed : FunctionEvaluator {
  bool Eval() final {
    context_->return_value =
        Value(static_cast<int>(context_->interpreter->seed()));
    return false;
  }
};

REGISTER_FUNCTION(FuncEval_mesh_info, "mesh_info", 2,
                  {ValueType::STRING, ValueType::STRING});
struct FuncEval_mesh_info : FunctionEvaluator {
  bool ValidateSpecial() final {
    const Value& p2 = args_->at(1);
    if (*p2.s != "size_x" && *p2.s != "size_y" && *p2.s != "size_z") {
      std::ostringstream oss;
      oss << "Parameter 2 for 'mesh_info' is invalid: " << p2 << ".)";
      oss << "Allowed values are only \"size_x\", \"size_y\", and \"size_z\".";
      error_message_ = oss.str();
      return true;
    }
    return false;
  }

  bool Eval() final {
    const Value& p1 = args_->at(0);
    const Value& p2 = args_->at(1);
    std::string asset_uri = *p1.s;
    // Prepend the base path if it's not a built-in mesh.
    if (!asset_uri.empty() && asset_uri[0] != '!') {
      asset_uri = context_->interpreter->grammar()->base_path() + asset_uri;
    }

    AssetCache& asset_cache = AssetCache::Get();
    geometry::HalfedgeMeshPtr mesh = asset_cache.GetMeshRessource(asset_uri);

    if (!mesh) {
      std::ostringstream oss;
      oss << "Shape operation 'mesh_info' failed to access the file '" << *p1.s;
      oss << "' or the file doesn't contain a mesh.";
      error_message_ = oss.str();
      return true;
    }

    geometry::AABB aabb;
    mesh->FillAABB(&aabb);

    if (*p2.s == "size_x") {
      context_->return_value = Value(aabb.extent.x() * 2.0);
    } else if (*p2.s == "size_y") {
      context_->return_value = Value(aabb.extent.y() * 2.0);
    } else if (*p2.s == "size_z") {
      context_->return_value = Value(aabb.extent.z() * 2.0);
    } else {
      assert(false);
    }

    return false;
  }
};

REGISTER_FUNCTION(FuncEval_file_exists, "file_exists", 1, {ValueType::STRING});
struct FuncEval_file_exists : FunctionEvaluator {
  bool ValidateSpecial() final {
    if (*args_->at(0).s == "") {
      error_message_ = "The parameter for shape operation 'file_exists' cannot";
      error_message_ += "be the empty string.";
      return true;
    }
    return false;
  }

  bool Eval() final {
    const Value& p1 = args_->at(0);
    const std::string file_uri =
        context_->interpreter->grammar()->base_path() + *p1.s;

    const std::ifstream file(file_uri.c_str());
    context_->return_value = Value(file.good());
    return false;
  }
};

}  // namespace shapeml
