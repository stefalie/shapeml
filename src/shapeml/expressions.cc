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

#include "shapeml/expressions.h"

#include <cassert>
#include <iomanip>
#include <sstream>
#include <stack>

#include "shapeml/evaluator.h"
#include "shapeml/grammar.h"
#include "shapeml/interpreter.h"
#include "shapeml/shape.h"

namespace shapeml {

std::ostream& operator<<(std::ostream& stream, const Locator& locator) {
  if (locator.line_number >= 0 && locator.file_name) {
    stream << " (file: " << *locator.file_name;
    stream << ", line: " << locator.line_number << ")";
  }
  return stream;
}

ValueExpr::ValueExpr(const Value& value) : value_(value) {}

Value ValueExpr::Eval() const { return value_; }

void ValueExpr::Accept(ExpressionVisitor* visitor) const {
  visitor->Visit(this);
}

void ValueExpr::PrintToStream(std::ostream* stream) const { *stream << value_; }

ScopeExpr::ScopeExpr(ExprPtr expr) : expr_(expr) {}

Value ScopeExpr::Eval() const { return expr_->Eval(); }

void ScopeExpr::Accept(ExpressionVisitor* visitor) const {
  visitor->Visit(this);
}

void ScopeExpr::PrintToStream(std::ostream* stream) const {
  *stream << '(' << expr_ << ')';
}

const char* OpType2Str(OpType type) {
  switch (type) {
    case OpType::PLUS:
      return "+";
    case OpType::MINUS:
      return "-";
    case OpType::MULT:
      return "*";
    case OpType::DIV:
      return "/";
    case OpType::MODULO:
      return "%";
    case OpType::LESS_EQUAL:
      return "<=";
    case OpType::GREATER_EQUAL:
      return ">=";
    case OpType::LESS:
      return "<";
    case OpType::GREATER:
      return ">";
    case OpType::EQUAL:
      return "==";
    case OpType::NOT_EQUAL:
      return "!=";
    case OpType::AND:
      return "&&";
    case OpType::OR:
      return "||";
    case OpType::NOT:
      return "!";
    case OpType::NEGATE:
      return "-";
  }

  return "";  // Avoid VS warning.
}

OpExpr::OpExpr(ExprPtr left, OpType op, ExprPtr right, const Locator& locator)
    : left_(left), op_(op), right_(right), locator_(locator) {
  assert(op_ != OpType::NOT && op_ != OpType::NEGATE);
}

OpExpr::OpExpr(OpType op, ExprPtr right, const Locator& locator)
    : left_(nullptr), op_(op), right_(right), locator_(locator) {
  assert(op_ == OpType::NOT || op_ == OpType::NEGATE);
}

Value OpExpr::Eval() const {
  Value left = left_ ? left_->Eval() : Value(0);
  Value right = right_->Eval();

  if (left.type == ValueType::SHAPE_OP_STRING ||
      right.type == ValueType::SHAPE_OP_STRING) {
    std::ostringstream oss;
    oss << "Cannot apply operator '" << OpType2Str(op_) << "\' to shape ";
    oss << "operation strings.";
    throw RuntimeError(oss.str(), locator_);
  }

  if (left.type == ValueType::STRING || right.type == ValueType::STRING) {
    if (left.type != ValueType::STRING) {
      left.ChangeType(ValueType::STRING);
    }
    if (right.type != ValueType::STRING) {
      right.ChangeType(ValueType::STRING);
    }

    switch (op_) {
      case OpType::PLUS:
        return Value(*left.s + *right.s);
      case OpType::EQUAL:
        return Value(*left.s == *right.s);
      case OpType::NOT_EQUAL:
        return Value(*left.s != *right.s);
      default:
        std::ostringstream oss;
        oss << "Cannot apply operator '" << OpType2Str(op_)
            << "\' to strings. ";
        throw RuntimeError(oss.str(), locator_);
    }
  }

  bool use_float = false;
  if (op_ == OpType::AND || op_ == OpType::OR) {
    left.ChangeType(ValueType::BOOL);
    right.ChangeType(ValueType::BOOL);
  } else if (op_ == OpType::NOT) {
    right.ChangeType(ValueType::BOOL);
  } else if (op_ == OpType::NEGATE) {
    if (right.type == ValueType::BOOL) {
      right.ChangeType(ValueType::INT);
    } else if (right.type == ValueType::FLOAT) {
      use_float = true;
    }
  } else if (op_ == OpType::MODULO) {
    if (left.type == ValueType::FLOAT || right.type == ValueType::FLOAT) {
      std::ostringstream oss;
      oss << "Cannot apply operator '%' to floats.";
      throw RuntimeError(oss.str(), locator_);
    } else {
      left.ChangeType(ValueType::INT);
      right.ChangeType(ValueType::INT);
    }
  } else {
    if (left.type == ValueType::FLOAT || right.type == ValueType::FLOAT) {
      left.ChangeType(ValueType::FLOAT);
      right.ChangeType(ValueType::FLOAT);
      use_float = true;
    } else {
      left.ChangeType(ValueType::INT);
      right.ChangeType(ValueType::INT);
    }
  }

  Value ret(0);

  switch (op_) {
    case OpType::PLUS:
      ret = (use_float ? Value(left.f + right.f) : Value(left.i + right.i));
      break;
    case OpType::MINUS:
      ret = (use_float ? Value(left.f - right.f) : Value(left.i - right.i));
      break;
    case OpType::MULT:
      ret = (use_float ? Value(left.f * right.f) : Value(left.i * right.i));
      break;
    case OpType::DIV:
      ret = (use_float ? Value(left.f / right.f) : Value(left.i / right.i));
      break;
    case OpType::MODULO:
      ret = Value(left.i % right.i);
      break;
    case OpType::LESS_EQUAL:
      ret = (use_float ? Value(left.f <= right.f) : Value(left.i <= right.i));
      break;
    case OpType::GREATER_EQUAL:
      ret = (use_float ? Value(left.f >= right.f) : Value(left.i >= right.i));
      break;
    case OpType::LESS:
      ret = (use_float ? Value(left.f < right.f) : Value(left.i < right.i));
      break;
    case OpType::GREATER:
      ret = (use_float ? Value(left.f > right.f) : Value(left.i > right.i));
      break;
    case OpType::EQUAL:
      ret = (use_float ? Value(left.f == right.f) : Value(left.i == right.i));
      break;
    case OpType::NOT_EQUAL:
      ret = (use_float ? Value(left.f != right.f) : Value(left.i != right.i));
      break;
    case OpType::AND:
      ret = Value(left.b && right.b);
      break;
    case OpType::OR:
      ret = Value(left.b || right.b);
      break;
    case OpType::NOT:
      ret = Value(!right.b);
      break;
    case OpType::NEGATE:
      ret = (use_float ? Value(-right.f) : Value(-right.i));
      break;
  }

  if (ret.type == ValueType::FLOAT &&
      (std::isinf(ret.f) || std::isnan(ret.f))) {
    std::ostringstream oss;
    oss << "The result of operation '" << OpType2Str(op_);
    oss << "' is NaN or +/- infintiy.";
    throw RuntimeError(oss.str(), locator_);
  }

  return ret;
}

void OpExpr::Accept(ExpressionVisitor* visitor) const { visitor->Visit(this); }

void OpExpr::PrintToStream(std::ostream* stream) const {
  if (op_ != OpType::NOT && op_ != OpType::NEGATE) {
    *stream << left_ << ' ';
  }
  *stream << OpType2Str(op_);
  if (op_ != OpType::NOT && op_ != OpType::NEGATE) {
    *stream << ' ';
  }
  *stream << right_;
}

NameExpr::NameExpr(const std::string& name, const Locator& locator)
    : name_(name), could_be_shape_local_(false), locator_(locator) {}

NameExpr::NameExpr(const std::string& name, const Locator& locator,
                   const ExprVec& params, bool could_be_shape_local)
    : name_(name),
      params_(params),
      could_be_shape_local_(could_be_shape_local),
      locator_(locator) {}

// Creates a deep copy of an expression tree, and also replaces NameExprs with
// corresponding values iff the name is a valid parameter.
class CopyExprAndApplyParams final : public ExpressionVisitor {
 public:
  explicit CopyExprAndApplyParams(const ValueDict& func_params)
      : func_params_(func_params) {}

  void Visit(const ValueExpr* value_expr) {
    expr_ = MakeExpr<ValueExpr>(value_expr->value());
  }
  void Visit(const ScopeExpr* scope_expr) {
    scope_expr->expr()->Accept(this);
    expr_ = MakeExpr<ScopeExpr>(expr_);
  }
  void Visit(const OpExpr* op_expr) {
    if (op_expr->left()) {
      op_expr->left()->Accept(this);
      ExprPtr left_tmp = expr_;
      op_expr->right()->Accept(this);
      expr_ =
          MakeExpr<OpExpr>(left_tmp, op_expr->op(), expr_, op_expr->locator());
    } else {
      op_expr->right()->Accept(this);
      expr_ = MakeExpr<OpExpr>(op_expr->op(), expr_, op_expr->locator());
    }
  }
  void Visit(const NameExpr* name_expr) {
    auto it = func_params_.find(name_expr->name());
    if (it == func_params_.end()) {
      ExprVec new_params;
      for (const ExprPtr p : name_expr->params()) {
        p->Accept(this);
        new_params.push_back(expr_);
      }
      expr_ = MakeExpr<NameExpr>(name_expr->name(), name_expr->locator(),
                                 new_params, name_expr->could_be_shape_local());
    } else {
      // Error checking, make certain that there are no parameters.
      if (!name_expr->params().empty()) {
        std::ostringstream oss;
        oss << "Passing arguments to parameters is not allowed: '";
        oss << name_expr->name() << "'.";
        throw RuntimeError(oss.str(), name_expr->locator());
      }
      expr_ = MakeExpr<ValueExpr>(it->second);
    }
  }

  ExprPtr expr() const { return expr_; }

 private:
  ExprPtr expr_;
  ValueDict func_params_;
};

Value NameExpr::Eval() const {
  // TODO(stefalie): I know, I know, singletons are bad. I was young and didn't
  // know what I was doing.
  Interpreter& interpreter = Interpreter::Get();
  std::stack<ValueDict>& func_stack = interpreter.function_stack();

  // Search function arguments (if we're inside a custom function).
  // If there are function arguments, we are definitely inside of a custom
  // function.
  if (!func_stack.empty()) {
    auto it = func_stack.top().find(name_);
    if (it != func_stack.top().end()) {
      if (!params_.empty()) {
        std::ostringstream oss;
        oss << "Arguments cannot have parameters: '" << name_ << "'.";
        throw RuntimeError(oss.str(), locator_);
      }
      return it->second;
    }
  }

  // Search rule arguments and custom shape attributes.
  // If we're not inside a custom function, we could be in the execution of a
  // production rule. In that case, accessing rule arguments and shape
  // attributes is legal.
  if (could_be_shape_local_) {
    Value val(0);
    if (interpreter.GetCurrentShape()->GetParameter(name_, &val)) {
      if (!params_.empty()) {
        std::ostringstream oss;
        oss << "Passing arguments to rule parameters is not allowed: '";
        oss << name_ << "'.";
        throw RuntimeError(oss.str(), locator_);
      }
      return val;
    }
    if (interpreter.GetShapeStackTop()->GetCustomAttribute(name_, &val)) {
      if (!params_.empty()) {
        std::ostringstream oss;
        oss << "Passing arguments to custom shape attributes is not ";
        oss << "allowed: '" << name_ << "'.";
        throw RuntimeError(oss.str(), locator_);
      }
      return val;
    }
  }

  // Search built-in functions.
  FunctionEvaluator* func = FunctionEvaluator::CreateEvaluator(name_);
  if (func) {
    ValueVec args;
    for (ExprConstPtr p : params_) {
      args.push_back(p->Eval());
    }

    FuncEvalContext context;
    context.interpreter = &interpreter;

    func->InitArgsAndContext(&args, &context);
    if (func->Validate() || func->ValidateSpecial() || func->Eval()) {
      RuntimeError error(func->error_mesage(), locator_);
      delete func;
      throw error;
    }

    if (context.return_value.type == ValueType::FLOAT &&
        (std::isinf(context.return_value.f) ||
         std::isnan(context.return_value.f))) {
      std::ostringstream oss;
      oss << "The result of function '" << *func->name();
      oss << "' is NaN or +/- infintiy.";
      throw RuntimeError(oss.str(), locator_);
    }
    delete func;
    return context.return_value;
  }

  // Search built-in shape attributes.
  if (could_be_shape_local_) {
    ShapeAttributeEvaluator* attr =
        ShapeAttributeEvaluator::CreateEvaluator(name_);
    if (attr) {
      ValueVec args;
      for (ExprConstPtr p : params_) {
        args.push_back(p->Eval());
      }

      AttrEvalContext context;
      context.shape = interpreter.GetShapeStackTop();
      context.interpreter = &interpreter;

      attr->InitArgsAndContext(&args, &context);
      if (attr->Validate() || attr->ValidateSpecial() || attr->Eval()) {
        RuntimeError error(attr->error_mesage(), locator_);
        delete attr;
        throw error;
      }

      delete attr;
      return context.return_value;
    }
  }

  // Search parameters and constants.
  Value val(0);
  if (interpreter.GetGlobalVariable(name_, &val)) {
    if (!params_.empty()) {
      std::ostringstream oss;
      oss << "Passing arguments to global variables (parameters or ";
      oss << "constants) is not allowed: '" << name_ << "'.";
      throw RuntimeError(oss.str(), locator_);
    }
    return val;
  }

  // Search custom functions.
  const FuncDict& funcs = interpreter.grammar()->functions();
  auto jt = funcs.find(name_);
  if (jt != funcs.end()) {
    if (func_stack.size() == 20) {
      std::ostringstream oss;
      oss << "Function '" << name_ << "' reached the max recursion depth (20).";
      throw RuntimeError(oss.str(), locator_);
    }

    FuncConstPtr func = jt->second;
    if (func->arguments.size() != params_.size()) {
      std::ostringstream oss;
      oss << "Custom function '" << name_ << "' takes ";
      oss << func->arguments.size() << " arguments but ";
      oss << params_.size() << " were provided.";
      throw RuntimeError(oss.str(), locator_);
    }

    // Evaluate custom function.
    ValueDict stack_elem;
    for (size_t i = 0; i < params_.size(); ++i) {
      stack_elem.emplace(func->arguments[i], params_[i]->Eval());
    }
    func_stack.push(stack_elem);

    Value ret(0);
    try {
      ret = func->expression->Eval();

      // If we're returning a shape operations string, and if there are any
      // arguments, then we have to create a new shape operation string and
      // replace all occurences of parameters with their corresponding values.
      if (ret.type == ValueType::SHAPE_OP_STRING && !stack_elem.empty()) {
        // Clone all expressions in the shape operation string.
        CopyExprAndApplyParams copy_expr_and_apply_params(stack_elem);
        for (ShapeOp& op : *ret.ops) {
          for (ExprPtr& e : op.parameters) {
            e->Accept(&copy_expr_and_apply_params);
            e = copy_expr_and_apply_params.expr();
          }
        }
      }
    } catch (const RuntimeError& error) {
      std::ostringstream oss;
      oss << "Inside function '" << name_ << "':\n";
      oss << "ERROR" << error.where() << ": " << error.what();
      throw RuntimeError(oss.str(), locator_);
    }

    func_stack.pop();
    return ret;
  }

  std::ostringstream oss;
  oss << "There is no variable with the name '" << name_ << "'.";
  throw RuntimeError(oss.str(), locator_);

  // This should never be reached, but it won't compile without a return value.
  return Value(0);
}

void NameExpr::Accept(ExpressionVisitor* visitor) const {
  visitor->Visit(this);
}

void NameExpr::PrintToStream(std::ostream* stream) const {
  *stream << name_;
  if (!params_.empty()) {
    *stream << "(";
    *stream << params_[0];
    for (size_t i = 1; i < params_.size(); ++i) {
      *stream << ", " << params_[i];
    }
    *stream << ")";
  }
}

std::ostream& operator<<(std::ostream& stream, const Expression* expr) {
  expr->PrintToStream(&stream);
  return stream;
}

void PrintShapeOpStringToStream(const ShapeOpString& ops, bool line_breaks,
                                std::ostream* stream) {
  if (line_breaks) {
    *stream << "{\n";
    for (const ShapeOp& c : ops) {
      *stream << "  " << c.name;
      if (c.parameters.size() > 0) {
        *stream << '(' << c.parameters[0];
        for (size_t i = 1; i < c.parameters.size(); ++i) {
          *stream << ", " << c.parameters[i];
        }
        *stream << ')';
      }
      *stream << "\n";
    }
    *stream << "}";
  } else {
    *stream << '{';
    for (const ShapeOp& c : ops) {
      *stream << ' ' << c.name;
      if (c.parameters.size() > 0) {
        *stream << '(' << c.parameters[0];
        for (size_t i = 1; i < c.parameters.size(); ++i) {
          *stream << ", " << c.parameters[i];
        }
        *stream << ')';
      }
    }
    *stream << " }";
  }
}

std::ostream& operator<<(std::ostream& stream, const ArgVec& args) {
  if (!args.empty()) {
    stream << '(' << args[0];
    for (size_t i = 1; i < args.size(); ++i) {
      stream << ", " << args[i];
    }
    stream << ')';
  }
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const Function& func) {
  stream << "func " << func.name << func.arguments << " = " << func.expression
         << ';';
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const Rule& rule) {
  stream << "rule " << rule.predecessor << rule.arguments;
  if (rule.probability) {
    stream << " : " << rule.probability;
  }
  if (rule.condition) {
    stream << " :: " << rule.condition;
  }

  stream << " = ";
  PrintShapeOpStringToStream(rule.successor, true, &stream);
  stream << ';';
  return stream;
}

}  // namespace shapeml
