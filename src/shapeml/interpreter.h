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
#include <exception>
#include <memory>
#include <random>
#include <stack>
#include <string>
#include <vector>

#include "shapeml/expressions.h"

namespace shapeml {

namespace geometry {
class Octree;
}
class Grammar;
class Shape;
typedef std::vector<Shape*> ShapePtrVec;

class ShapeStack {
 public:
  // This method takes owernship of the pointer and will free the memory once
  // it's not used anymore.
  void Push(std::unique_ptr<Shape> shape);

  void Pop();

  Shape* Top() const;

  // Returns the shape with index last_idx - n, or nullptr if it doesn't exist.
  Shape* TopMinusN(unsigned n) const;

  int Size() const { return static_cast<int>(stack_.size()); }

 private:
  std::deque<std::unique_ptr<Shape>> stack_;
};

class RuntimeError : public std::exception {
 public:
  explicit RuntimeError(const std::string& what, const Locator& where) noexcept
      : what_(what), where_(where) {}

  const char* what() const noexcept final { return what_.c_str(); }
  const Locator& where() const noexcept { return where_; }

 private:
  const std::string what_;
  const Locator where_;
};

// A derivation context contains information or pointers to information that
// shape operation evaluators might need access to.
struct DerivationContext {
  // 'shape_stack' and 'base_path' are added for convenience. They could also be
  // indirectly retrieved from the 'interpreter'.
  ShapeStack* shape_stack;
  int shape_stack_start_size;
  std::string base_path;
  ShapePtrVec* derivation_list;
  Interpreter* interpreter;
};

struct AttrEvalContext {
  const Shape* shape;
  Interpreter* interpreter;
  Value return_value = Value(0);
};

class Interpreter {
 public:
  // TODO(stefalie): I know, I know, singletons are bad. If we ever decide to
  // make grammar derivation multi-threaded (i.e., each thread derives a
  // different model), then we would have to get rid of the singleton and have
  // an interpreter for each thread. The singleton Get() would have to be
  // replaced with a get method to return the thread local interpreter.
  static Interpreter& Get();

  // 'grammar' needs to be a pointer to a valid grammar.
  // 'octree' can be nullptr. That means that occlusions queries won't work
  // (i.e., queries will always return "none" as if there are no overlaps).
  // 'parameters' can be nullptr. If that is the case, the interpreter will take
  // the parameter default values from the grammar.
  // Returns nullptr if evaluation of any of the grammar's constants fails,
  // otherwise it returns a pointer the a newly created root shape.
  Shape* Init(const Grammar* grammar, const std::string& axiom, unsigned seed,
              const ValueDict* parameters, geometry::Octree* octree);

  // Returns true if the requested global variable exists.
  bool GetGlobalVariable(const std::string& name, Value* value) const;

  // Used to access parameters of the shape that is currently being derived.
  const Shape* GetCurrentShape() const;

  // Used to access shape attributes.
  const Shape* GetShapeStackTop() const { return shape_stack_.Top(); }

  // Returns true if derivation was successful.
  bool Derive(Shape* shape, int max_derivation_steps);

  void ApplyShapeOpString(const ShapeOpString& ops,
                          ShapePtrVec* derivation_list);

  // Setters and getters.
  std::mt19937& random_generator() { return random_generator_; }
  unsigned seed() const { return seed_; }

  const ValueDict* parameters() const { return parameters_; }
  const ValueDict& constants_evaluated() const { return constants_evaluated_; }

  const Grammar* grammar() const { return grammar_; }
  geometry::Octree* octree() const { return octree_; }

  std::stack<ValueDict>& function_stack() { return function_stack_; }

 private:
  Interpreter() = default;
  Interpreter(const Interpreter&) = delete;
  Interpreter& operator=(const Interpreter&) = delete;

  // Returns true if a rule could be set for the given shape.
  bool SelectRule(Shape* shape);

  const Grammar* grammar_ = nullptr;

  std::mt19937 random_generator_;
  unsigned seed_ = 666;

  const ValueDict* parameters_;
  ValueDict constants_evaluated_;

  geometry::Octree* octree_ = nullptr;

  Shape* current_shape_ = nullptr;
  ShapeStack shape_stack_;
  std::stack<ValueDict> function_stack_;
  int dereferencing_stack_counter_;
};

}  // namespace shapeml
