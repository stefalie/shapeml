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

#include "shapeml/grammar.h"

#include <ostream>
#include <unordered_set>

#include "shapeml/evaluator.h"
#include "shapeml/interpreter.h"

namespace shapeml {

// The following four methods are used to populate the grammar.
// Note the following restrictions:
// New parameter, constant, or function names cannot be the same as already
// existing parameter, constant, (custom or built-in) function, or built-in
// shape attribute names.
// New names can, however, be the same as shape operation names (not a smart
// thing to do, but possible).
// A function or a rule cannot have two arguments with the same name. The
// argument names can alias all other names (but again, it's a stupid thing to
// do).

AddResult Grammar::AddParameter(const std::string& name, Value val) {
  if (CheckParamConstFuncName(name)) {
    return AddResult::NAME_COLLISION;
  }

  parameters_.emplace(name, val);
  parameter_ordering_.push_back(name);
  return AddResult::OK;
}

AddResult Grammar::AddConstant(const std::string& name, ExprConstPtr expr) {
  if (CheckParamConstFuncName(name)) {
    return AddResult::NAME_COLLISION;
  }

  constants_.emplace(name, expr);
  constant_ordering_.push_back(name);
  return AddResult::OK;
}

AddResult Grammar::AddFunction(FuncConstPtr func) {
  if (CheckParamConstFuncName(func->name)) {
    return AddResult::NAME_COLLISION;
  }
  if (CheckArgumentDuplicates(func->arguments)) {
    return AddResult::DUPLICATE_ARG;
  }

  functions_.emplace(func->name, func);
  function_ordering_.push_back(func);
  return AddResult::OK;
}

AddResult Grammar::AddRule(RuleConstPtr rule) {
  if (ShapeOpEvaluator::HasEvaluator(rule->predecessor)) {
    return AddResult::NAME_COLLISION;
  }
  if (CheckArgumentDuplicates(rule->arguments)) {
    return AddResult::DUPLICATE_ARG;
  }
  if (rule->predecessor.back() == '_') {
    return AddResult::PREDECESSOR_ENDS_IN_UNDERSCORE;
  }

  rules_[rule->predecessor].push_back(rule);
  rule_ordering_.push_back(rule);
  return AddResult::OK;
}

bool Grammar::CheckParamConstFuncName(const std::string& name) const {
  if (parameters_.find(name) != parameters_.end() ||
      constants_.find(name) != constants_.end() ||
      functions_.find(name) != functions_.end() ||
      FunctionEvaluator::HasEvaluator(name) ||
      ShapeAttributeEvaluator::HasEvaluator(name)) {
    return true;
  }
  return false;
}

bool Grammar::CheckArgumentDuplicates(const ArgVec& args) const {
  std::unordered_set<std::string> used_arg_names;
  for (const std::string& arg : args) {
    auto it = used_arg_names.insert(arg);
    if (!it.second) {
      return true;
    }
  }
  return false;
}

std::ostream& operator<<(std::ostream& stream, const Grammar& grammar) {
  bool is_start = true;

  if (!grammar.parameter_ordering().empty()) {
    is_start = false;

    for (const std::string& param : grammar.parameter_ordering()) {
      const auto it = grammar.parameters().find(param);
      stream << "param " << it->first << " = " << it->second << ';' << '\n';
    }
  }

  if (!grammar.constant_ordering().empty()) {
    if (is_start) {
      is_start = false;
    } else {
      stream << '\n';
    }

    for (const std::string& constant : grammar.constant_ordering()) {
      const auto it = grammar.constants().find(constant);
      stream << "const " << it->first << " = " << it->second << ';' << '\n';
    }
  }

  if (!grammar.function_ordering().empty()) {
    if (is_start) {
      is_start = false;
    } else {
      stream << '\n';
    }

    for (FuncConstPtr func : grammar.function_ordering()) {
      stream << *func << '\n';
    }
  }

  if (!grammar.rule_ordering().empty()) {  // Would be strange to have a grammar
                                           // without rules, but who knows.
    for (RuleConstPtr rule : grammar.rule_ordering()) {
      if (is_start) {
        is_start = false;
      } else {
        stream << '\n';
      }

      stream << *rule << '\n';
    }
  }

  return stream;
}

}  // namespace shapeml
