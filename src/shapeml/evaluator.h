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

#include <cassert>
#include <string>
#include <unordered_map>
#include <vector>

#include "shapeml/expressions.h"
#include "shapeml/interpreter.h"
#include "shapeml/shape.h"
#include "shapeml/value.h"

namespace shapeml {

// Helper interface that encapsulates the execution or evaluation of custom
// functions, custom shape attributes, and most importantly shape operations.
template <class T>
class Evaluator {
 public:
  virtual ~Evaluator() {}

  void Init(const std::string* name, int num_params,
            const ValueTypeVec* param_types);

  void InitArgsAndContext(ValueVec* args, T* context);

  // Validate methods are used to check if the arguments are valid, and also if
  // the evaluator can be evaluated in the given situation. They will return
  // true if an error occurs.
  bool Validate();
  virtual bool ValidateSpecial() { return false; }

  virtual bool Eval() = 0;

  const std::string* name() const { return name_; }

  const std::string& error_mesage() const { return error_message_; }

  ValueVec* args() { return args_; }
  T* context() { return context_; }

  void set_error_message(const std::string& error_message) {
    error_message_ = error_message;
  }

 protected:
  // For all instances:
  static const char* type_name_;

  // Const properties of the evaluator.
  const std::string* name_;
  int num_params_;  // -1 means the Validate() step is skipped.
  const ValueTypeVec* param_types_;

  // Temp properties
  ValueVec* args_;
  T* context_;
  std::string error_message_;

  // Below follows functionality for the 'Evaluator' factory.
 public:
  static Evaluator<T>* CreateEvaluator(const std::string& name);

  static bool HasEvaluator(const std::string& name);

  typedef Evaluator<T>*(InstanceCreator)();
  static void RegisterEvaluator(const std::string& name, int num_params,
                                const ValueTypeVec& param_types,
                                InstanceCreator* instance_creator);

 private:
  struct FactoryEntry {
    InstanceCreator* instance_creator;
    int num_params;
    ValueTypeVec param_types;
  };

  // I wanted to have the following, but VS didn't get the initialization order
  // correct. :-(
  // static std::unordered_map<std::string, FactoryEntry> evaluators_;
  // Seems to be the same bug as explained here:
  // https://developercommunity.visualstudio.com/content/problem/134886/initialization-of-template-static-variable-wrong.html
  // TODO(stefalie): Fix when VS is fixed.
  static std::unordered_map<std::string, FactoryEntry>& Evaluators();

 public:
  // Helper methods (mostly) for input validation.
  bool ValidateType(ValueType expected_type, unsigned idx);
  bool ValidateTypes();
  bool CheckArgNumber(const std::vector<int>& nums_args);
  bool CheckNonEmptyMesh(const Shape* shape);
  bool CheckSingleFaceMesh(const Shape* shape);
  bool CheckDirectionVector(unsigned idx);
  bool CheckGreaterThanZero(unsigned idx);
  bool CheckGreaterEqualThanZero(unsigned idx);
  bool CheckGreaterThan(unsigned idx1, unsigned idx2);
  bool CheckGreaterEqualThan(unsigned idx1, unsigned idx2);
  bool CheckGreaterEqualThanValue(unsigned idx, double val);
  bool CheckRange(unsigned idx, double min, double max);
  bool CheckRange(unsigned idx, int min, int max);
  void PropagateError(const RuntimeError& error);
};

template <class T, class U>
static Evaluator<T>* FactoryInstanceCreator() {
  return new U();
}

template <class T, class U>
struct FactoryRegistration {
  FactoryRegistration(const std::string& name, int num_params,
                      const ValueTypeVec& param_types) {
    Evaluator<T>::RegisterEvaluator(name, num_params, param_types,
                                    &FactoryInstanceCreator<T, U>);
  }
};

#define REGISTER_EVALUATOR(context_type, evaluator_type, name, num_params, \
                           ...)                                            \
  struct evaluator_type;                                                   \
  static FactoryRegistration<context_type, evaluator_type>                 \
      evaluator_type##_registration(name, num_params, __VA_ARGS__)

// Not having these forward declarations makes Clang sad. But having them makes
// MSVC angry.
#ifndef _MSC_VER
template <>
const char* Evaluator<DerivationContext>::type_name_;
template <>
const char* Evaluator<AttrEvalContext>::type_name_;
template <>
const char* Evaluator<FuncEvalContext>::type_name_;
#endif

extern template class Evaluator<DerivationContext>;
extern template class Evaluator<AttrEvalContext>;
extern template class Evaluator<FuncEvalContext>;

typedef Evaluator<DerivationContext> ShapeOpEvaluator;
typedef Evaluator<AttrEvalContext> ShapeAttributeEvaluator;
typedef Evaluator<FuncEvalContext> FunctionEvaluator;

#define REGISTER_SHAPE_OP(evaluator_type, name, num_params, ...)          \
  REGISTER_EVALUATOR(DerivationContext, evaluator_type, name, num_params, \
                     __VA_ARGS__)
#define REGISTER_ATTRIBUTE_EVALUATOR(evaluator_type, name, num_params, ...) \
  REGISTER_EVALUATOR(AttrEvalContext, evaluator_type, name, num_params,     \
                     __VA_ARGS__)
#define REGISTER_FUNCTION(evaluator_type, name, num_params, ...)        \
  REGISTER_EVALUATOR(FuncEvalContext, evaluator_type, name, num_params, \
                     __VA_ARGS__)

// Implementation:

template <class T>
void Evaluator<T>::Init(const std::string* name, int num_params,
                        const ValueTypeVec* param_types) {
  name_ = name;
  num_params_ = num_params;
  param_types_ = param_types;
}

template <class T>
void Evaluator<T>::InitArgsAndContext(ValueVec* args, T* context) {
  args_ = args;
  context_ = context;
}

template <class T>
bool Evaluator<T>::Validate() {
  if (num_params_ == -1) {
    return false;
  }

  if (CheckArgNumber({num_params_})) {
    return true;
  }

  return ValidateTypes();
}

template <class T>
Evaluator<T>* Evaluator<T>::CreateEvaluator(const std::string& name) {
  const auto& it = Evaluators().find(name);
  if (it != Evaluators().end()) {
    Evaluator<T>* ret = (*it->second.instance_creator)();
    ret->Init(&it->first, it->second.num_params, &it->second.param_types);
    return ret;
  }
  return nullptr;
}

template <class T>
bool Evaluator<T>::HasEvaluator(const std::string& name) {
  return (Evaluators().find(name) != Evaluators().end());
}

template <class T>
void Evaluator<T>::RegisterEvaluator(const std::string& name, int num_params,
                                     const ValueTypeVec& param_types,
                                     InstanceCreator* instance_creator) {
  assert(Evaluators().find(name) == Evaluators().end());
  assert((num_params == -1 && param_types.empty()) ||
         num_params == static_cast<int>(param_types.size()));
  FactoryEntry entry;
  entry.instance_creator = instance_creator;
  entry.num_params = num_params;
  entry.param_types = param_types;
  Evaluators().emplace(name, entry);
}

// template<class T>
// std::unordered_map<std::string, typename Evaluator<T>::FactoryEntry>
// Evaluator<T>::evaluators_;
template <class T>
std::unordered_map<std::string, typename Evaluator<T>::FactoryEntry>&
Evaluator<T>::Evaluators() {
  static std::unordered_map<std::string, typename Evaluator<T>::FactoryEntry>
      evaluators;
  return evaluators;
}

template <class T>
bool Evaluator<T>::ValidateType(ValueType expected_type, unsigned idx) {
  if (args_->at(idx).ChangeType(expected_type)) {
    std::ostringstream oss;
    oss << "Cannot convert parameter " << (idx + 1) << " for " << type_name_;
    oss << " '" << *name_ << "' to " << ValueType2Str(expected_type) << ".";
    error_message_ = oss.str();
    return true;
  }
  return false;
}

template <class T>
bool Evaluator<T>::ValidateTypes() {
  for (int i = 0; i < static_cast<int>(param_types_->size()); ++i) {
    if (ValidateType(param_types_->at(i), i)) {
      return true;
    }
  }
  return false;
}

template <class T>
bool Evaluator<T>::CheckArgNumber(const std::vector<int>& nums_args) {
  assert(nums_args.size() > 0);

  for (int num_args : nums_args) {
    if (num_args == static_cast<int>(args_->size())) {
      return false;
    }
  }

  std::ostringstream oss;
  oss << "The " << type_name_ << " '" << *name_ << "' takes ";
  if (nums_args.size() > 1) {
    for (size_t i = 0; i < (nums_args.size() - 1); ++i) {
      oss << nums_args[i];
      if (nums_args.size() > 2) {
        oss << ", ";
      } else {
        oss << ' ';
      }
    }
    oss << "or ";
  }
  oss << nums_args.back() << " arguments, but ";
  oss << args_->size() << " were provided.";
  error_message_ = oss.str();
  return true;
}

template <class T>
bool Evaluator<T>::CheckNonEmptyMesh(const Shape* shape) {
  if (!shape->mesh()) {
    std::ostringstream oss;
    oss << "The " << type_name_ << " '" << *name_ << "' cannot be evaluated ";
    oss << "for a shape that has no mesh set.";
    error_message_ = oss.str();
    return true;
  }

  if (shape->mesh()->Empty()) {
    std::ostringstream oss;
    oss << "The " << type_name_ << " '" << *name_ << "' cannot be evaluated ";
    oss << "for a shape with an empty mesh.";
    error_message_ = oss.str();
    return true;
  }
  return false;
}

template <class T>
bool Evaluator<T>::CheckSingleFaceMesh(const Shape* shape) {
  if (shape->mesh()->NumFaces() != 1) {
    std::ostringstream oss;
    oss << "The " << type_name_ << " '" << *name_ << "' is only defined for ";
    oss << "shapes with meshes with exactly 1 face.";
    error_message_ = oss.str();
    return true;
  }
  return false;
}

template <class T>
bool Evaluator<T>::CheckDirectionVector(unsigned idx) {
  Vec3 direction(args_->at(idx).f, args_->at(idx + 1).f, args_->at(idx + 2).f);
  if (direction.norm() < geometry::EPSILON) {
    std::ostringstream oss;
    oss << "Parameters ";
    oss << (idx + 1) << ", " << (idx + 2) << ", and " << (idx + 3);
    oss << " for " << type_name_ << " '" << *name_;
    oss << "' need to define a vector with norm greater than 0.";
    error_message_ = oss.str();
    return true;
  }
  return false;
}

template <class T>
bool Evaluator<T>::CheckGreaterThanZero(unsigned idx) {
  if ((args_->at(idx).type == ValueType::FLOAT && args_->at(idx).f <= 0.0) ||
      (args_->at(idx).type == ValueType::INT && args_->at(idx).i <= 0)) {
    std::ostringstream oss;
    oss << "Parameter " << (idx + 1) << " for " << type_name_ << " '" << *name_;
    oss << "' needs to be greater than 0.";
    error_message_ = oss.str();
    return true;
  }
  return false;
}

template <class T>
bool Evaluator<T>::CheckGreaterEqualThanZero(unsigned idx) {
  if ((args_->at(idx).type == ValueType::FLOAT && args_->at(idx).f < 0.0) ||
      (args_->at(idx).type == ValueType::INT && args_->at(idx).i < 0)) {
    std::ostringstream oss;
    oss << "Parameter " << (idx + 1) << " for " << type_name_ << " '" << *name_;
    oss << "' must not be smaller than 0.";
    error_message_ = oss.str();
    return true;
  }
  return false;
}

template <class T>
bool Evaluator<T>::CheckGreaterThan(unsigned idx1, unsigned idx2) {
  if ((args_->at(idx1).type == ValueType::FLOAT &&
       args_->at(idx1).f <= args_->at(idx2).f) ||
      (args_->at(idx1).type == ValueType::INT &&
       args_->at(idx1).i <= args_->at(idx2).i)) {
    std::ostringstream oss;
    oss << "Parameter " << (idx1 + 1) << " for " << type_name_ << " '";
    oss << *name_ << "' needs to be greater than parameter " << (idx2 + 1);
    oss << ". ";
    error_message_ = oss.str();
    return true;
  }
  return false;
}

template <class T>
bool Evaluator<T>::CheckGreaterEqualThan(unsigned idx1, unsigned idx2) {
  if ((args_->at(idx1).type == ValueType::FLOAT &&
       args_->at(idx1).f < args_->at(idx2).f) ||
      (args_->at(idx1).type == ValueType::INT &&
       args_->at(idx1).i < args_->at(idx2).i)) {
    std::ostringstream oss;
    oss << "Parameter " << (idx1 + 1) << " for " << type_name_ << " '";
    oss << *name_ << "' must not be smaller than parameter " << (idx2 + 1);
    oss << ". ";
    error_message_ = oss.str();
    return true;
  }
  return false;
}

template <class T>
bool Evaluator<T>::CheckGreaterEqualThanValue(unsigned idx, double val) {
  assert(args_->at(idx).type == ValueType::FLOAT);
  if (args_->at(idx).f < val) {
    std::ostringstream oss;
    oss << "Parameter " << (idx + 1) << " for " << type_name_ << " '";
    oss << *name_ << "' must not be smaller than " << val << ". ";
    error_message_ = oss.str();
    return true;
  }
  return false;
}

template <class T>
bool Evaluator<T>::CheckRange(unsigned idx, double min, double max) {
  assert(args_->at(idx).type == ValueType::FLOAT);
  if (args_->at(idx).f < min || args_->at(idx).f > max) {
    std::ostringstream oss;
    oss << "Parameter " << (idx + 1) << " for " << type_name_ << " '" << *name_;
    oss << "' must be in the range [" << min << ", " << max << "].";
    error_message_ = oss.str();
    return true;
  }
  return false;
}

template <class T>
bool Evaluator<T>::CheckRange(unsigned idx, int min, int max) {
  assert(args_->at(idx).type == ValueType::INT);
  if (args_->at(idx).i < min || args_->at(idx).i > max) {
    std::ostringstream oss;
    oss << "Parameter " << (idx + 1) << " for " << type_name_ << " '" << *name_;
    oss << "' must be in the range [" << min << ", " << max << "].";
    error_message_ = oss.str();
    return true;
  }
  return false;
}

template <class T>
void Evaluator<T>::PropagateError(const RuntimeError& error) {
  std::ostringstream oss;
  oss << "Inside " << type_name_ << " '" << *name_ << "':\n";
  oss << "ERROR" << error.where() << ": " << error.what();
  error_message_ = oss.str();
}

}  // namespace shapeml
