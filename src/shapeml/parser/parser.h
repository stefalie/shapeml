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

#include "shapeml/expressions.h"
#include "shapeml/parser/token.h"

// This class defines a recursive-descent parser for ShapeML grammars.
// Keywords: top-down, LL(1)
// Dragon book references: 2.4.2, 4.4.1-4.4.3, and Appendix A
//
//
// Grammar definition:
//
// Symbols written in capitals stand for terminal tokens (e.g., ATTR or ID).
// Terimal tokens such as ASSIGN, BRACKET_ROUND_OPEN, COLON, etc. will simply
// be written as =, (, :, etc.
//
//  stmt_list ::= stmt ; stmt_list
//              | EOF
//
//  stmt ::= parameter
//         | constant
//         | function
//         | rule
//
//  parameter ::= PARAM ID = param_literal
//
//  constant ::= CONST ID = generic_expr
//
//  function ::= FUNC ID arguments = generic_expr
//
//  rule ::= RULE ID arguments probability condition = shape_op_string
//
//  arguments ::= ( arg_list )
//              | epsilon
//
//  arg_list ::= ID , arg_list
//             | ID
//
//  probability ::= : expr
//              ::= epsilon
//
//  condition ::= :: expr
//              | epsilon
//
//  generic_expr ::= expr
//                 | shape_op_string
//
//  shape_op_string ::= { shape_op_list }
//
//  shape_op_list ::= shape_op shape_op_list
//                  | epsilon
//
//  shape_op ::= shape_op_name parameters
//             | ^ ID parameters
//
//  shape_op_id ::= ID
//                | [
//                | ]
//
//  parameters ::= ( param_list )
//               | epsilon
//
//  param_list ::= generic_expr , param_list
//               | generic_expr
//
//  expr ::= expr || join
//         | join
//
//  join ::= join && equality
//         | equality
//
//  equality ::= equality == rel
//             | equality != rel
//             | rel
//
//  rel ::= arithmetic_expr < arithmetic_expr
//        | arithmetic_expr <= arithmetic_expr
//        | arithmetic_expr >= arithmetic_expr
//        | arithmetic_expr > arithmetic_expr
//        | arithmetic_expr
//
//  arithmetic_expr ::= arithmetic_expr + term
//                    | arithmetic_expr - term
//                    | term
//
//  term ::= term * unary
//         | term / unary
//         | unary
//
//  unary ::= ! factor
//          | - factor
//          | factor
//
//  factor ::= ( expr )
//           | named_expr
//           | literal
//
//  named_expr ::= ID paramters
//
//  literal ::= TRUE
//            | FALSE
//            | INT
//            | FLOAT
//            | STRING
//
//  param_literal ::= - INT
//                  | - FLOAT
//                  | literal
//

namespace shapeml {

class Grammar;

namespace parser {

class Lexer;

class Parser {
 public:
  // Returns true if parsing fails.
  bool Parse(const std::string& file_name, Grammar* grammar);

 private:
  // Terminal matching functions.
  bool If(TokenType type);
  void Match(TokenType type);
  bool MatchIf(TokenType type);
  bool MatchIfAndLocate(TokenType type, Locator* location);

  // Terminal matching functions that return the token's value.
  void MatchInt(int* i);
  void MatchFloat(double* f);
  void MatchString(std::string* s);
  void MatchID(std::string* s);

  // Recursive NT functions.
  void stmt_list();
  void stmt();
  void parameter();
  void constant();
  void function();
  void rule();
  void arguments(ArgVec* args);
  void arg_list(ArgVec* args);
  ExprPtr probability();
  ExprPtr condition();
  ExprPtr generic_expr();
  void shape_op_string(ShapeOpString* ops);
  void shape_op_list(ShapeOpString* ops);
  ShapeOp shape_op();
  void shape_op_id(std::string* name);
  void parameters(ExprVec* exprs);
  void param_list(ExprVec* exprs);
  ExprPtr expr();
  ExprPtr join();
  ExprPtr equality();
  ExprPtr rel();
  ExprPtr arithmetic_expr();
  ExprPtr term();
  ExprPtr unary();
  ExprPtr factor();
  ExprPtr named_expr();
  ExprPtr literal();
  ExprPtr param_literal();

  Grammar* grammar_ = nullptr;
  Lexer* lexer_ = nullptr;

  Token lookahead_ = Token(TokenType::ERROR, -1, "");

  bool shape_locals_allowed_in_exprs_ = false;
};

}  // namespace parser

}  // namespace shapeml
