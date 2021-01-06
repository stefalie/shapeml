// Shape Modeling Language (ShapeML)
// Copyright (C) 2018  Stefan Lienhard
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

// This is an example test program that shows how to programatically create a
// (very simple) ShapeML grammar, how to execute it, and how to export the
// resulting meshes as an OBJ file.

#include <iostream>

#include "shapeml/exporter.h"
#include "shapeml/grammar.h"
#include "shapeml/interpreter.h"
#include "shapeml/shape.h"

void MakeGrammar(shapeml::Grammar* grammar);

int main(int, char*[]) {
  shapeml::Grammar grammar;
  MakeGrammar(&grammar);
  std::cout << "The grammar is:\n" << grammar << '\n';

  shapeml::Interpreter& interpreter = shapeml::Interpreter::Get();
  shapeml::Shape* root =
      interpreter.Init(&grammar, "Axiom", 1234, nullptr, nullptr);
  if (!root) {
    return 1;
  }

  if (!interpreter.Derive(root, 20)) {
    delete root;
    return 1;
  }

  std::cout << "The shape tree is:\n";
  root->PrintToStreamRecursively(&std::cout);

  std::cout
      << "\nThe OBJ model is stored in test_interpreter_standalone.obj.\n";
  shapeml::Exporter(root, shapeml::ExportType::OBJ,
                    "test_interpreter_standalone", grammar.base_path(), true);

  return 0;
}

void MakeGrammar(shapeml::Grammar* grammar) {
  using shapeml::AddResult;
  using shapeml::ExprPtr;
  using shapeml::ExprVec;
  using shapeml::Locator;
  using shapeml::MakeExpr;
  using shapeml::NameExpr;
  using shapeml::OpExpr;
  using shapeml::OpType;
  using shapeml::Rule;
  using shapeml::RulePtr;
  using shapeml::ShapeOp;
  using shapeml::ShapeOpString;
  using shapeml::Value;
  using shapeml::ValueExpr;
  AddResult add_result;
  ExprPtr expr1, expr2, expr3;

  // The axiom
  RulePtr axiom = std::make_shared<Rule>();
  axiom->predecessor = "Axiom";

  axiom->successor.push_back(ShapeOp());
  axiom->successor.back().name = "RecursiveRule";
  expr1 = MakeExpr<ValueExpr>(Value(8));
  axiom->successor.back().parameters.push_back(expr1);

  add_result = grammar->AddRule(axiom);
  assert(add_result == AddResult::OK);

  // Recursive rule. First part.
  RulePtr recursive_rule_1 = std::make_shared<Rule>();
  recursive_rule_1->predecessor = "RecursiveRule";
  recursive_rule_1->arguments.push_back("n");

  expr2 = MakeExpr<NameExpr>("n", Locator(), ExprVec(), true);
  expr3 = MakeExpr<ValueExpr>(Value(0));
  expr1 = MakeExpr<OpExpr>(expr2, OpType::GREATER, expr3, Locator());
  recursive_rule_1->condition = expr1;

  recursive_rule_1->successor.push_back(ShapeOp());
  recursive_rule_1->successor.back().name = "SimpleHouse";
  expr2 = MakeExpr<NameExpr>("n", Locator(), ExprVec(), true);
  expr3 = MakeExpr<ValueExpr>(Value(0.15));
  expr1 = MakeExpr<OpExpr>(expr2, OpType::MULT, expr3, Locator());
  recursive_rule_1->successor.back().parameters.push_back(expr1);

  recursive_rule_1->successor.push_back(ShapeOp());
  recursive_rule_1->successor.back().name = "translateX";
  expr1 = MakeExpr<ValueExpr>(Value(2));
  recursive_rule_1->successor.back().parameters.push_back(expr1);

  recursive_rule_1->successor.push_back(ShapeOp());
  recursive_rule_1->successor.back().name = "RecursiveRule";
  expr2 = MakeExpr<NameExpr>("n", Locator(), ExprVec(), true);
  expr3 = MakeExpr<ValueExpr>(Value(1));
  expr1 = MakeExpr<OpExpr>(expr2, OpType::MINUS, expr3, Locator());
  recursive_rule_1->successor.back().parameters.push_back(expr1);

  add_result = grammar->AddRule(recursive_rule_1);
  assert(add_result == AddResult::OK);

  // Recursive rule. Second part.
  RulePtr recursive_rule_2 = std::make_shared<Rule>();
  recursive_rule_2->predecessor = "RecursiveRule";
  recursive_rule_2->arguments.push_back("n");

  expr2 = MakeExpr<NameExpr>("n", Locator(), ExprVec(), true);
  expr3 = MakeExpr<ValueExpr>(Value(0));
  expr1 = MakeExpr<OpExpr>(expr2, OpType::EQUAL, expr3, Locator());
  recursive_rule_2->condition = expr1;

  add_result = grammar->AddRule(recursive_rule_2);
  assert(add_result == AddResult::OK);

  // A rule that creates an L-shaped mass model with a hip roof.
  RulePtr simple_house = std::make_shared<Rule>();
  simple_house->predecessor = "SimpleHouse";
  simple_house->arguments.push_back("height");

  // The bottom part
  simple_house->successor.push_back(ShapeOp());
  simple_house->successor.back().name = "shapeL";
  expr1 = MakeExpr<ValueExpr>(Value(0.6));
  expr2 = MakeExpr<ValueExpr>(Value(0.4));
  simple_house->successor.back().parameters.push_back(expr1);
  simple_house->successor.back().parameters.push_back(expr2);

  simple_house->successor.push_back(ShapeOp());
  simple_house->successor.back().name = "extrude";
  expr1 = MakeExpr<NameExpr>("height", Locator(), ExprVec(), true);
  simple_house->successor.back().parameters.push_back(expr1);

  simple_house->successor.push_back(ShapeOp());
  simple_house->successor.back().name = "color";
  expr1 = MakeExpr<ValueExpr>(Value("#EBDBB2"));
  simple_house->successor.back().parameters.push_back(expr1);

  simple_house->successor.push_back(ShapeOp());
  simple_house->successor.back().name = "SimpleHouseBottom_";

  // The roof
  ShapeOpString op_str;

  op_str.push_back(ShapeOp());
  op_str.back().name = "rotateScopeXYToXZ";

  op_str.push_back(ShapeOp());
  op_str.back().name = "roofHip";
  expr1 = MakeExpr<ValueExpr>(Value(30.0));
  expr2 = MakeExpr<ValueExpr>(Value(0.05));
  op_str.back().parameters.push_back(expr1);
  op_str.back().parameters.push_back(expr2);

  op_str.push_back(ShapeOp());
  op_str.back().name = "color";
  expr1 = MakeExpr<ValueExpr>(Value("#FE8019"));
  op_str.back().parameters.push_back(expr1);

  op_str.push_back(ShapeOp());
  op_str.back().name = "SimpleHouseRoof_";

  simple_house->successor.push_back(ShapeOp());
  simple_house->successor.back().name = "splitFace";
  expr1 = MakeExpr<ValueExpr>(Value("top"));
  expr2 = MakeExpr<ValueExpr>(Value(op_str));
  simple_house->successor.back().parameters.push_back(expr1);
  simple_house->successor.back().parameters.push_back(expr2);

  add_result = grammar->AddRule(simple_house);
  assert(add_result == AddResult::OK);
  (void)add_result;
}
