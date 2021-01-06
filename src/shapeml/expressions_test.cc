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

#include "shapeml/expressions.h"

#include <gtest/gtest.h>

#include "shapeml/grammar.h"
#include "shapeml/interpreter.h"

using shapeml::AddResult;
using shapeml::ExprPtr;
using shapeml::ExprVec;
using shapeml::FuncPtr;
using shapeml::Function;
using shapeml::Grammar;
using shapeml::Interpreter;
using shapeml::Locator;
using shapeml::MakeExpr;
using shapeml::NameExpr;
using shapeml::OpExpr;
using shapeml::OpType;
using shapeml::RuntimeError;
using shapeml::ShapeOp;
using shapeml::ShapeOpString;
using shapeml::Value;
using shapeml::ValueExpr;
using shapeml::ValueVec;

// TODO(stefalie): Maybe there should be a few more tests, e.g., for boolean
// operations or the unary minus operator. Overall, the tricky cases are
// covered though by the following tests.

TEST(ExpressionsTest, SimpleValidEval) {
  ExprPtr expr1 = MakeExpr<ValueExpr>(Value(6.0));
  ExprPtr expr2 = MakeExpr<ValueExpr>(Value(2));
  ExprPtr expr3 = MakeExpr<OpExpr>(expr1, OpType::MINUS, expr2, Locator());
  ExprPtr expr4 = MakeExpr<ValueExpr>(Value(0.5));
  ExprPtr expr5 = MakeExpr<OpExpr>(expr3, OpType::MULT, expr4, Locator());

  Value result(0);
  ASSERT_NO_THROW(result = expr5->Eval());
  EXPECT_EQ(result, Value(2.0));
}

TEST(ExpressionsTest, StringOperationAddTest) {
  ExprPtr expr1 = MakeExpr<ValueExpr>(Value("Hello"));
  ExprPtr expr2 = MakeExpr<ValueExpr>(Value(" world!"));
  ExprPtr expr3 = MakeExpr<OpExpr>(expr1, OpType::PLUS, expr2, Locator());

  Value result(0);
  ASSERT_NO_THROW(result = expr3->Eval());
  EXPECT_EQ(result, Value("Hello world!"));
}

TEST(ExpressionsTest, StringOperationMinusTest) {
  ExprPtr expr1 = MakeExpr<ValueExpr>(Value("Hello"));
  ExprPtr expr2 = MakeExpr<ValueExpr>(Value(" world!"));
  ExprPtr expr3 = MakeExpr<OpExpr>(expr1, OpType::MINUS, expr2, Locator());

  EXPECT_THROW(expr3->Eval(), RuntimeError);
}

TEST(ExpressionsTest, StringOperationUnaryMinusTest) {
  ExprPtr expr1 = MakeExpr<ValueExpr>(Value("Hello world!"));
  ExprPtr expr2 = MakeExpr<OpExpr>(OpType::NOT, expr1, Locator());

  EXPECT_THROW(expr2->Eval(), RuntimeError);
}

TEST(ExpressionsTest, ModuloPassTest) {
  ExprPtr expr1 = MakeExpr<ValueExpr>(Value(true));
  ExprPtr expr2 = MakeExpr<ValueExpr>(Value(2));
  ExprPtr expr3 = MakeExpr<OpExpr>(expr1, OpType::MODULO, expr2, Locator());

  Value result(0);
  ASSERT_NO_THROW(result = expr3->Eval());
  EXPECT_EQ(result, Value(1));
}

TEST(ExpressionsTest, ModuloFailTest) {
  ExprPtr expr1 = MakeExpr<ValueExpr>(Value(1.1));
  ExprPtr expr2 = MakeExpr<ValueExpr>(Value(2));
  ExprPtr expr3 = MakeExpr<OpExpr>(expr1, OpType::MODULO, expr2, Locator());

  EXPECT_THROW(expr3->Eval(), RuntimeError);
}

TEST(ExpressionsTest, FunctionUnknownTest) {
  Grammar grammar;
  Interpreter::Get().Init(&grammar, "Axiom", 1234, nullptr, nullptr);

  ExprPtr expr1 = MakeExpr<NameExpr>("unknown", Locator());

  EXPECT_THROW(expr1->Eval(), RuntimeError);
}

TEST(ExpressionsTest, FunctionWrongNumParamsTest) {
  ExprVec params;
  params.push_back(MakeExpr<ValueExpr>(Value(90.0)));
  params.push_back(MakeExpr<ValueExpr>(Value(180.0)));
  ExprPtr expr1 = MakeExpr<NameExpr>("sin", Locator(), params, false);

  EXPECT_THROW(expr1->Eval(), RuntimeError);
}

TEST(ExpressionsTest, FunctionPassTest) {
  ExprVec exprs;
  ValueVec expectations;
  ExprVec params;

  params = {MakeExpr<ValueExpr>(Value(1.23))};
  exprs.push_back(MakeExpr<NameExpr>("ceil", Locator(), params, false));
  expectations.push_back(Value(2.0));

  params = {MakeExpr<ValueExpr>(Value(1.23))};
  exprs.push_back(MakeExpr<NameExpr>("floor", Locator(), params, false));
  expectations.push_back(Value(1.0));

  params = {MakeExpr<ValueExpr>(Value(1.5))};
  exprs.push_back(MakeExpr<NameExpr>("round", Locator(), params, false));
  expectations.push_back(Value(2.0));

  params = {MakeExpr<ValueExpr>(Value(-2.1))};
  exprs.push_back(MakeExpr<NameExpr>("sign", Locator(), params, false));
  expectations.push_back(Value(-1));

  params = {MakeExpr<ValueExpr>(Value(2.34))};
  exprs.push_back(MakeExpr<NameExpr>("fract", Locator(), params, false));
  expectations.push_back(Value(0.34));

  params = {MakeExpr<ValueExpr>(Value(2.34))};
  exprs.push_back(MakeExpr<NameExpr>("int", Locator(), params, false));
  expectations.push_back(Value(2));

  params = {MakeExpr<ValueExpr>(Value(90.0))};
  exprs.push_back(MakeExpr<NameExpr>("sin", Locator(), params, false));
  expectations.push_back(Value(1.0));

  params = {MakeExpr<ValueExpr>(Value(180.0))};
  exprs.push_back(MakeExpr<NameExpr>("cos", Locator(), params, false));
  expectations.push_back(Value(-1.0));

  params = {MakeExpr<ValueExpr>(Value(45.0))};
  exprs.push_back(MakeExpr<NameExpr>("tan", Locator(), params, false));
  expectations.push_back(Value(1.0));

  params = {MakeExpr<ValueExpr>(Value(-1.0))};
  exprs.push_back(MakeExpr<NameExpr>("asin", Locator(), params, false));
  expectations.push_back(Value(-90.0));

  params = {MakeExpr<ValueExpr>(Value(-1.0))};
  exprs.push_back(MakeExpr<NameExpr>("acos", Locator(), params, false));
  expectations.push_back(Value(180.0));

  params = {MakeExpr<ValueExpr>(Value(1.0))};
  exprs.push_back(MakeExpr<NameExpr>("atan", Locator(), params, false));
  expectations.push_back(Value(45.0));

  params = {MakeExpr<ValueExpr>(Value(-1.0)), MakeExpr<ValueExpr>(Value(-1.0))};
  exprs.push_back(MakeExpr<NameExpr>("atan2", Locator(), params, false));
  expectations.push_back(Value(-135.0));

  params = {MakeExpr<ValueExpr>(Value(25.0))};
  exprs.push_back(MakeExpr<NameExpr>("sqrt", Locator(), params, false));
  expectations.push_back(Value(5.0));

  params = {MakeExpr<ValueExpr>(Value(3.0)), MakeExpr<ValueExpr>(Value(4.0))};
  exprs.push_back(MakeExpr<NameExpr>("pow", Locator(), params, false));
  expectations.push_back(Value(81.0));

  params = {MakeExpr<ValueExpr>(Value(7.0))};
  exprs.push_back(MakeExpr<NameExpr>("exp", Locator(), params, false));
  expectations.push_back(Value(std::exp(7.0)));

  params = {MakeExpr<ValueExpr>(Value(1.0))};
  exprs.push_back(MakeExpr<NameExpr>("log", Locator(), params, false));
  expectations.push_back(Value(0.0));

  params = {MakeExpr<ValueExpr>(Value(10000.0))};
  exprs.push_back(MakeExpr<NameExpr>("log10", Locator(), params, false));
  expectations.push_back(Value(4.0));

  params = {MakeExpr<ValueExpr>(Value(3.0)), MakeExpr<ValueExpr>(Value(4))};
  exprs.push_back(MakeExpr<NameExpr>("max", Locator(), params, false));
  expectations.push_back(Value(4.0));

  params = {MakeExpr<ValueExpr>(Value(3)), MakeExpr<ValueExpr>(Value(4))};
  exprs.push_back(MakeExpr<NameExpr>("min", Locator(), params, false));
  expectations.push_back(Value(3));

  ValueVec results;
  for (unsigned i = 0; i < exprs.size(); ++i) {
    ASSERT_NO_THROW(results.push_back(exprs[i]->Eval()))
        << "Caught exception in function " << i << '.';
  }
  for (unsigned i = 0; i < exprs.size(); ++i) {
    EXPECT_EQ(results[i], expectations[i]) << "Function " << i << " is wrong.";
  }
}

TEST(ExpressionsTest, ShapeOperationStringTest) {
  ShapeOpString ops1;
  ShapeOpString ops2;
  ExprPtr expr1 = MakeExpr<ValueExpr>(Value(ops1));
  ExprPtr expr2 = MakeExpr<ValueExpr>(Value(ops2));

  for (char i = static_cast<char>(OpType::PLUS);
       i < static_cast<char>(OpType::NOT); ++i) {
    ExprPtr expr3 = MakeExpr<OpExpr>(expr1, (OpType)i, expr2, Locator());

    EXPECT_THROW(expr3->Eval(), RuntimeError);
  }
  for (char i = static_cast<char>(OpType::NOT);
       i <= static_cast<char>(OpType::NEGATE); ++i) {
    ExprPtr expr3 = MakeExpr<OpExpr>((OpType)i, expr2, Locator());

    EXPECT_THROW(expr3->Eval(), RuntimeError);
  }
}

TEST(ExpressionsTest, SimpleCustomFunctionTest) {
  Grammar grammar;
  FuncPtr func = std::make_shared<Function>();
  func->name = "my_func";
  func->arguments = {"my_arg"};
  ExprPtr expr1 = MakeExpr<NameExpr>("my_arg", Locator());
  ExprPtr expr2 = MakeExpr<ValueExpr>(Value(1));
  ExprPtr expr3 = MakeExpr<OpExpr>(expr1, OpType::PLUS, expr2, Locator());
  func->expression = expr3;
  ASSERT_EQ(grammar.AddFunction(func), AddResult::OK);
  Interpreter::Get().Init(&grammar, "Axiom", 1234, nullptr, nullptr);

  ExprVec params = {MakeExpr<ValueExpr>(Value(10))};
  ExprPtr expr4 = MakeExpr<NameExpr>("my_func", Locator(), params, false);

  Value result(0);
  ASSERT_NO_THROW(result = expr4->Eval());
  EXPECT_EQ(result, Value(11));
}

TEST(ExpressionsTest, RecursiveFunctionFailTest) {
  Grammar grammar;
  FuncPtr func = std::make_shared<Function>();
  func->name = "rec_func";
  ExprPtr expr1 = MakeExpr<NameExpr>("rec_func", Locator());
  func->expression = expr1;
  grammar.AddFunction(func);
  Interpreter::Get().Init(&grammar, "Axiom", 1234, nullptr, nullptr);

  ExprPtr expr2 = MakeExpr<NameExpr>("rec_func", Locator());

  EXPECT_THROW(expr2->Eval(), RuntimeError);
}
