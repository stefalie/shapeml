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

#include <memory>
#include <ostream>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include "shapeml/value.h"

namespace shapeml {

class Interpreter;
struct ShapeOp;
typedef std::vector<ShapeOp> ShapeOpString;

// This is used to annotate 'things' that can be evaluated. This is helpful to
// locate errors.
struct Locator {
  int line_number = -1;
  const std::string* file_name =
      nullptr;  // Will use the same pointer as the original token.
};

std::ostream& operator<<(std::ostream& stream, const Locator& locator);

// Expressions (Mostly for storing trees of arithmetic expressions.)
class Expression;
typedef std::shared_ptr<Expression> ExprPtr;
typedef std::shared_ptr<const Expression> ExprConstPtr;
typedef std::vector<ExprPtr> ExprVec;
typedef std::unordered_map<std::string, ExprConstPtr> ExprConstDict;

struct ExpressionVisitor;

class Expression {
 public:
  virtual ~Expression() = default;

  // TODO(stefalie): This could potentially also be implemented via visitor
  // pattern. The ExpressionVisitor was unfortunately only added after all this
  // was already in place.
  virtual Value Eval() const = 0;

  virtual void Accept(ExpressionVisitor* visitor) const = 0;

  virtual void PrintToStream(std::ostream* stream) const = 0;
};

class ValueExpr : public Expression {
 public:
  explicit ValueExpr(const Value& value);

  Value Eval() const final;

  void Accept(ExpressionVisitor* visitor) const final;

  void PrintToStream(std::ostream* stream) const final;

  void set_value(const Value& value) { value_ = value; }
  const Value& value() const { return value_; }

 protected:
  Value value_;
};

class ScopeExpr : public Expression {
 public:
  explicit ScopeExpr(ExprPtr expr);

  Value Eval() const final;

  void Accept(ExpressionVisitor* visitor) const final;

  void PrintToStream(std::ostream* stream) const final;

  void set_expr(ExprPtr expr) { expr_ = expr; }
  ExprConstPtr expr() const { return expr_; }

 private:
  ExprPtr expr_;
};

enum class OpType : char {
  PLUS = 0,
  MINUS,
  MULT,
  DIV,
  MODULO,
  LESS_EQUAL,
  GREATER_EQUAL,
  LESS,
  GREATER,
  EQUAL,
  NOT_EQUAL,
  AND,
  OR,
  NOT,
  NEGATE,
};

const char* OpType2Str(OpType type);

class OpExpr : public Expression {
 public:
  OpExpr(ExprPtr left, OpType op, ExprPtr right, const Locator& locator);

  // Only to be used for unary (! or -) operators.
  OpExpr(OpType op, ExprPtr right, const Locator& locator);

  Value Eval() const final;

  void Accept(ExpressionVisitor* visitor) const final;

  void PrintToStream(std::ostream* stream) const final;

  void set_left(ExprPtr left) { left_ = left; }
  ExprConstPtr left() const { return left_; }

  void set_op(OpType op) { op_ = op; }
  OpType op() const { return op_; }

  void set_right(ExprPtr right) { right_ = right; }
  ExprConstPtr right() const { return right_; }

  void set_locator(const Locator& locator) { locator_ = locator; }
  const Locator& locator() const { return locator_; }

 private:
  ExprPtr left_;
  OpType op_;
  ExprPtr right_;

  Locator locator_;
};

struct FuncEvalContext {
  Interpreter* interpreter;
  Value return_value = Value(0);
};

class NameExpr : public Expression {
 public:
  NameExpr(const std::string& name, const Locator& locator);
  NameExpr(const std::string& name, const Locator& locator,
           const ExprVec& params, bool could_be_shape_local);

  Value Eval() const final;

  void Accept(ExpressionVisitor* visitor) const final;

  void PrintToStream(std::ostream* stream) const final;

  void set_name(const std::string& name) { name_ = name; }
  const std::string& name() const { return name_; }

  void set_params(const ExprVec& params) { params_ = params; }
  const ExprVec& params() const { return params_; }

  void set_could_be_shape_local(bool could_be_shape_local) {
    could_be_shape_local_ = could_be_shape_local;
  }
  bool could_be_shape_local() const { return could_be_shape_local_; }

  void set_locator(const Locator& locator) { locator_ = locator; }
  const Locator& locator() const { return locator_; }

 private:
  std::string name_;
  ExprVec params_;
  bool could_be_shape_local_;

  Locator locator_;
};

template <typename T, typename... Args>
ExprPtr MakeExpr(Args... args) {
  return std::make_shared<T>(args...);
}

std::ostream& operator<<(std::ostream& stream, const Expression* expr);

struct ExpressionVisitor {
  virtual void Visit(const ValueExpr* value_expr) = 0;
  virtual void Visit(const ScopeExpr* scope_expr) = 0;
  virtual void Visit(const OpExpr* op_expr) = 0;
  virtual void Visit(const NameExpr* name_expr) = 0;
};

// Shape operations, functions, and production rules.
typedef std::vector<std::string> ArgVec;

struct ShapeOp {
  std::string name;
  ExprVec parameters;
  bool is_reference = false;
  Locator locator;
};

void PrintShapeOpStringToStream(const ShapeOpString& ops, bool line_breaks,
                                std::ostream* stream);

struct Function {
  std::string name;
  ArgVec arguments;
  ExprConstPtr expression;
};

std::ostream& operator<<(std::ostream& stream, const Function& func);

struct Rule {
  std::string predecessor;
  ArgVec arguments;
  ExprConstPtr probability;
  ExprConstPtr condition;
  ShapeOpString successor;

  Locator last_line_locator;
};

std::ostream& operator<<(std::ostream& stream, const Rule& rule);

typedef std::shared_ptr<Function> FuncPtr;
typedef std::shared_ptr<const Function> FuncConstPtr;
typedef std::unordered_map<std::string, FuncConstPtr> FuncDict;
typedef std::shared_ptr<Rule> RulePtr;
typedef std::shared_ptr<const Rule> RuleConstPtr;
typedef std::vector<RuleConstPtr> RuleVec;
typedef std::unordered_map<std::string, RuleVec> RuleDict;

}  // namespace shapeml
