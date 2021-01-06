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

#include "shapeml/interpreter.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <sstream>
#include <utility>

#include "shapeml/evaluator.h"
#include "shapeml/grammar.h"
#include "shapeml/shape.h"

namespace shapeml {

void ShapeStack::Push(std::unique_ptr<Shape> shape) {
  stack_.emplace_back(std::move(shape));
}

void ShapeStack::Pop() {
  assert(!stack_.empty());
  stack_.pop_back();
}

Shape* ShapeStack::Top() const {
  assert(!stack_.empty());
  return stack_.back().get();
}

Shape* ShapeStack::TopMinusN(unsigned n) const {
  const int idx = static_cast<int>(stack_.size()) - n - 1;
  if (idx >= 0) {
    return stack_[idx].get();
  }
  return nullptr;
}

Interpreter& Interpreter::Get() {
  static Interpreter instance;
  return instance;
}

Shape* Interpreter::Init(const Grammar* grammar, const std::string& axiom,
                         unsigned seed, const ValueDict* parameters,
                         geometry::Octree* octree) {
  assert(grammar);
  assert(!axiom.empty());
  grammar_ = grammar;
  seed_ = seed;
  octree_ = octree;

  random_generator_ = std::mt19937(seed_);

  // If no custom parameters are required we simply use the default values form
  // the grammar.
  if (parameters) {
    parameters_ = parameters;
  } else {
    parameters_ = &grammar_->parameters();
  }

  // Evaluate the constants.
  constants_evaluated_.clear();
  for (const std::string& name : grammar_->constant_ordering()) {
    Value eval_result(0);
    try {
      eval_result = grammar_->constants().at(name)->Eval();
    } catch (const RuntimeError& error) {
      std::cerr << "ERROR" << error.where() << ": ";
      std::cerr << "In constant '" << name << "': " << error.what() << '\n';
      return nullptr;
    }
    constants_evaluated_.emplace(name, eval_result);
  }

  function_stack_ = std::stack<ValueDict>();
  dereferencing_stack_counter_ = 0;

  Shape* root = new Shape;
  root->set_name(axiom);
  return root;
}

bool Interpreter::GetGlobalVariable(const std::string& name,
                                    Value* value) const {
  auto it = parameters_->find(name);
  if (it != parameters_->end()) {
    *value = it->second;
    return true;
  }

  it = constants_evaluated_.find(name);
  if (it != constants_evaluated_.end()) {
    *value = it->second;
    return true;
  }

  return false;
}

const Shape* Interpreter::GetCurrentShape() const {
  assert(current_shape_);
  return current_shape_;
}

bool Interpreter::Derive(Shape* shape, int max_derivation_steps) {
  assert(grammar_);
  assert(shape);

  try {
    ShapePtrVec deriv_list_1, deriv_list_2;
    deriv_list_1.push_back(shape);

    for (int i = 0; i < max_derivation_steps; ++i) {
      if (deriv_list_1.empty()) {
        break;
      }

      for (Shape* s : deriv_list_1) {
        assert(s->IsLeaf());
        assert(!s->rule());

        // Update the current shape so that we can access its parameters inside
        // expressions and also in case we have to output an error message in
        // the catch statement (that error message should mention the name of
        // the currently shape).
        current_shape_ = s;

        shape_stack_ = ShapeStack();
        shape_stack_.Push(std::unique_ptr<Shape>(s->CreateOffspring()));
        if (SelectRule(s)) {
          ApplyShapeOpString(s->rule()->successor, &deriv_list_2);
        }
      }

      deriv_list_1.swap(deriv_list_2);
      deriv_list_2.clear();
    }

    if (!deriv_list_1.empty()) {
      std::cerr << "WARNING: Reached max derivation depth (";
      std::cerr << max_derivation_steps << "), but the model is not fully ";
      std::cerr << "derived yet.\n";
    }
  } catch (const RuntimeError& error) {
    assert(current_shape_);
    std::cerr << "ERROR" << error.where() << ": ";
    std::cerr << "In rule '" << current_shape_->name() << "': ";
    std::cerr << error.what() << '\n';
    return false;
  }

  return true;
}

void Interpreter::ApplyShapeOpString(const ShapeOpString& ops,
                                     ShapePtrVec* derivation_list) {
  DerivationContext context;
  context.shape_stack = &shape_stack_;
  context.shape_stack_start_size = shape_stack_.Size();
  context.base_path = grammar_->base_path();
  context.derivation_list = derivation_list;
  context.interpreter = this;

  for (const ShapeOp& op : ops) {
    if (op.is_reference) {
      if (dereferencing_stack_counter_ == 20) {
        std::ostringstream oss;
        oss << "Dereferencing '" << op.name << "' reached the max recursion ";
        oss << "depth (20) for nested references.";
        throw RuntimeError(oss.str(), op.locator);
      }
      ++dereferencing_stack_counter_;

      // Create the shape operation string and fill the dereferencing stack.
      Value ref_shape_op_str =
          MakeExpr<NameExpr>(op.name, op.locator, op.parameters, true)->Eval();
      if (ref_shape_op_str.ChangeType(ValueType::SHAPE_OP_STRING)) {
        std::ostringstream oss;
        oss << '\'' << op.name << "' is not of type shape operation string ";
        oss << "and cannot be referenced as such.";
        throw RuntimeError(oss.str(), op.locator);
      }

      try {
        ApplyShapeOpString(*ref_shape_op_str.ops, derivation_list);
      } catch (const RuntimeError& error) {
        std::ostringstream oss;
        oss << "Inside reference to '" << op.name << "':\n";
        oss << "ERROR" << error.where() << ": " << error.what();
        throw RuntimeError(oss.str(), op.locator);
      }

      --dereferencing_stack_counter_;
      continue;
    }

    ValueVec param_values;
    for (const ExprConstPtr& expr : op.parameters) {
      param_values.push_back(expr->Eval());
    }

    ShapeOpEvaluator* eval = ShapeOpEvaluator::CreateEvaluator(op.name);
    if (eval) {
      eval->InitArgsAndContext(&param_values, &context);

      if (eval->Validate() || eval->ValidateSpecial() || eval->Eval()) {
        RuntimeError error(eval->error_mesage(), op.locator);
        delete eval;
        throw error;
      }
      delete eval;
    } else {  // Otherwise it's a terminal or a non-terminal.
      Shape* new_shape = new Shape(*shape_stack_.Top());
      new_shape->set_name(op.name);
      new_shape->AppendToParent();
      new_shape->set_parameters(param_values);

      if (op.name.back() == '_') {  // Terminal
        if (!new_shape->parameters().empty()) {
          std::cerr << "WARNING (file: " << *op.locator.file_name << ", line: ";
          std::cerr << op.locator.line_number << "): The arguments passed to ";
          std::cerr << "terminal '" << op.name << "' will be ignored.\n";
        }
        if (!new_shape->mesh() || new_shape->mesh()->Empty()) {
          std::cerr << "WARNING (file: " << *op.locator.file_name << ", line: ";
          std::cerr << op.locator.line_number << "): Creation of terminal '";
          std::cerr << op.name << "' with no mesh or an empty one.\n";
        }
        new_shape->set_terminal(true);
      } else {  // Non-terminal
        derivation_list->push_back(new_shape);
      }
    }
  }

  // Check that number of pushes and pops match up. The ShapeOpEvaluator for ]
  // checks that we never pop more than we push. Here we just have to check that
  // all the pushes also get popped again.
  if (shape_stack_.Size() != context.shape_stack_start_size) {
    Locator loc = current_shape_->rule()->last_line_locator;
    throw RuntimeError(
        "There are more pushes '[' than pops ']' in a shape operation string.",
        loc);
  }
}

bool Interpreter::SelectRule(Shape* shape) {
  const std::string& sym = shape->name();
  auto it = grammar_->rules().find(sym);
  if (it == grammar_->rules().end()) {
    std::cerr << "WARNING: No rule or shape operations found for '" << sym;
    std::cerr << "'.\n";
    return false;
  }
  RuleVec rules = it->second;

  // Only keep rules that take the same number of arguments as the shape
  // provides parameters.
  struct CheckNumParamsAndArgs {
    explicit CheckNumParamsAndArgs(size_t n) : num_params(n) {}

    bool operator()(RuleConstPtr rule) const {
      if (rule->arguments.size() != num_params) {
        return true;
      }
      return false;
    }

    size_t num_params;
  } check_num_params_and_args(shape->parameters().size());
  rules.erase(
      std::remove_if(rules.begin(), rules.end(), check_num_params_and_args),
      rules.end());

  if (rules.empty()) {
    std::cerr << "WARNING: No rule found for '" << sym << "' with ";
    std::cerr << shape->parameters().size() << " parameters.\n";
    return false;
  }

  // Filter out rules whose conditions evaluate to false.
  struct Condition {
    explicit Condition(Shape* s) : current_shape(s) {}

    bool operator()(RuleConstPtr rule) const {
      assert(rule->arguments.size() == current_shape->parameters().size());
      if (rule->condition) {
        // Set rule to the current shape so the condition can correctly evaluate
        // any shape attributes and rule parameters that it depends on.
        // parameters.
        current_shape->set_rule(rule);
        Value val = rule->condition->Eval();
        val.ChangeType(ValueType::BOOL);
        current_shape->set_rule(RuleConstPtr());  // Reset the rule.
        return !val.b;
      }
      return false;  // We keep the rule if no condition is set.
    }

    Shape* current_shape;
  } condition(shape);
  rules.erase(std::remove_if(rules.begin(), rules.end(), condition),
              rules.end());

  if (rules.empty()) {
    std::cerr << "WARNING: No rule with true condition found for '" << sym;
    std::cerr << shape->parameters() << "'.\n";
    return false;
  }

  // Select rule
  if (rules.size() == 1) {
    shape->set_rule(rules.front());
  } else {
    // If there is more than one rule left, we pick one at random.
    float total_prob = 0.0;
    std::vector<float> probabilities;
    for (RuleConstPtr rule : rules) {
      shape->set_rule(rule);
      Value val = (rule->probability ? rule->probability->Eval() : Value(1.0));
      shape->set_rule(RuleConstPtr());  // Reset the rule.
      val.ChangeType(ValueType::FLOAT);
      probabilities.push_back(std::max(0.0f, static_cast<float>(val.f)));
      total_prob += probabilities.back();
    }
    for (float& p : probabilities) {
      p /= total_prob;
    }

    auto uniform_dist = std::uniform_real_distribution<float>(0.0, 1.0);
    const float rand = uniform_dist(random_generator_);
    float prob = 0.0;
    for (size_t i = 0; i < rules.size(); ++i) {
      prob += probabilities[i];
      if (rand <= prob) {
        shape->set_rule(rules[i]);
        break;
      }
    }
  }

  return true;
}

}  // namespace shapeml
