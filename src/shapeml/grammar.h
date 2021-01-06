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

#include <string>
#include <vector>

#include "shapeml/expressions.h"

namespace shapeml {

enum class AddResult : char {
  OK,
  NAME_COLLISION,
  DUPLICATE_ARG,
  PREDECESSOR_ENDS_IN_UNDERSCORE,
};

class Grammar {
 public:
  // Methods to populate the grammar.
  // These methods return true in case of a name clash, i.e., trying to define
  // several variables with the same name. This also includes name clashes with
  // built-in function, shape operation, and shape attribute names.
  AddResult AddParameter(const std::string& name, Value val);
  AddResult AddConstant(const std::string& name, ExprConstPtr expr);
  AddResult AddFunction(FuncConstPtr func);
  AddResult AddRule(RuleConstPtr rule);

  // Setters and getters.
  void set_base_path(const std::string& base_path) { base_path_ = base_path; }
  const std::string& base_path() const { return base_path_; }

  void set_file_name(const std::string& file_name) { file_name_ = file_name; }
  const std::string& file_name() const { return file_name_; }

  const ValueDict& parameters() const { return parameters_; }
  const std::vector<std::string>& parameter_ordering() const {
    return parameter_ordering_;
  }

  const ExprConstDict& constants() const { return constants_; }
  const std::vector<std::string>& constant_ordering() const {
    return constant_ordering_;
  }

  const FuncDict& functions() const { return functions_; }
  const std::vector<FuncConstPtr>& function_ordering() const {
    return function_ordering_;
  }

  const RuleDict& rules() const { return rules_; }
  const std::vector<RuleConstPtr>& rule_ordering() const {
    return rule_ordering_;
  }

 private:
  bool CheckParamConstFuncName(const std::string& name) const;
  bool CheckArgumentDuplicates(const ArgVec& args) const;

  std::string base_path_;  // Path to directory that contains the grammar file.
  std::string file_name_;  // The file name of the grammar.

  ValueDict parameters_;
  std::vector<std::string> parameter_ordering_;

  ExprConstDict constants_;
  std::vector<std::string> constant_ordering_;

  FuncDict functions_;
  std::vector<FuncConstPtr> function_ordering_;

  RuleDict rules_;
  std::vector<RuleConstPtr> rule_ordering_;
};

std::ostream& operator<<(std::ostream& stream, const Grammar& grammar);

}  // namespace shapeml
