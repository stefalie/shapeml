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

#include "shapeml/parser/parser.h"

#include <cassert>
#include <cstring>
#include <exception>
#include <iostream>
#include <memory>
#include <sstream>
#include <stack>
#include <vector>

#include "shapeml/grammar.h"
#include "shapeml/parser/lexer.h"

namespace shapeml {

namespace parser {

class ParsingError : public std::exception {
 public:
  ParsingError() noexcept {}

  explicit ParsingError(TokenType expected_tok) noexcept {
    what_ = TokenType2Str(expected_tok);
  }

  explicit ParsingError(const std::vector<TokenType>& expected_toks) noexcept {
    assert(expected_toks.size() > 0);
    std::ostringstream oss;
    oss << '(';
    oss << TokenType2Str(expected_toks[0]);
    for (size_t i = 1; i < expected_toks.size(); ++i) {
      oss << ", " << TokenType2Str(expected_toks[i]);
    }
    oss << ')';
    what_ = oss.str();
  }

  const char* what() const noexcept final { return what_.c_str(); }

 private:
  std::string what_;
};

class SemanticError : public std::exception {
 public:
  explicit SemanticError(const std::string& what) noexcept : what_(what) {}

  const char* what() const noexcept final { return what_.c_str(); }

 private:
  const std::string what_;
};

bool Parser::Parse(const std::string& file_name, Grammar* grammar) {
  assert(grammar);
  grammar_ = grammar;

#ifdef _WIN32
  size_t slash = file_name.find_last_of("\\/");
#else
  size_t slash = file_name.rfind("/");
#endif
  if (slash != std::string::npos) {
    grammar_->set_base_path(file_name.substr(0, slash + 1));
  }
  grammar_->set_file_name(file_name.substr(slash + 1));

  lexer_ = new Lexer();
  if (lexer_->Init(file_name)) {
    std::cerr << "ERROR: Initialization of lexer failed.\n";
    return true;
  }

  lookahead_ = lexer_->Scan();
  shape_locals_allowed_in_exprs_ = false;

  bool error = false;
  try {
    stmt_list();
  } catch (const ParsingError& exception) {
    if (lookahead_.type() != TokenType::ERROR) {
      std::cerr << "ERROR (file: " << *lookahead_.file_name()
                << ", line: " << lookahead_.line_number() << "): ";
      std::cerr << "Unexpected token of type "
                << TokenType2Str(lookahead_.type()) << '.';
      if (strlen(exception.what()) > 0) {
        std::cerr << " Expected a token of type " << exception.what() << '.';
      }
      std::cerr << '\n';
    }
    error = true;
  } catch (const SemanticError& exception) {
    std::cerr << "ERROR (file: " << *lookahead_.file_name()
              << ", line: " << lookahead_.line_number() << "): ";
    std::cerr << exception.what() << '\n';
    error = true;
  }

  delete lexer_;
  lexer_ = nullptr;
  grammar_ = nullptr;

  assert(error || (lookahead_.type() == TokenType::END_OF_FILE));
  return error;
}

bool Parser::If(TokenType type) {
  if (lookahead_.type() == type) {
    return true;
  }
  return false;
}

void Parser::Match(TokenType type) {
  if (!MatchIf(type)) {
    throw ParsingError(type);
  }
}

bool Parser::MatchIf(TokenType type) {
  if (lookahead_.type() == type) {
    lookahead_ = lexer_->Scan();
    return true;
  }
  return false;
}

bool Parser::MatchIfAndLocate(TokenType type, Locator* location) {
  if (lookahead_.type() == type) {
    location->line_number = lookahead_.line_number();
    location->file_name = lookahead_.file_name();
    lookahead_ = lexer_->Scan();
    return true;
  }
  return false;
}

void Parser::MatchInt(int* i) {
  if (lookahead_.type() == TokenType::INT) {
    *i = lookahead_.i();
    lookahead_ = lexer_->Scan();
  } else {
    throw ParsingError(TokenType::INT);
  }
}

void Parser::MatchFloat(double* f) {
  if (lookahead_.type() == TokenType::FLOAT) {
    *f = lookahead_.f();
    lookahead_ = lexer_->Scan();
  } else {
    throw ParsingError(TokenType::FLOAT);
  }
}

void Parser::MatchString(std::string* str) {
  if (lookahead_.type() == TokenType::STRING) {
    *str = lookahead_.s();
    lookahead_ = lexer_->Scan();
  } else {
    throw ParsingError(TokenType::STRING);
  }
}

void Parser::MatchID(std::string* str) {
  if (lookahead_.type() == TokenType::ID) {
    *str = lookahead_.s();
    lookahead_ = lexer_->Scan();
  } else {
    throw ParsingError(TokenType::ID);
  }
}

void Parser::stmt_list() {
  while (!If(TokenType::END_OF_FILE)) {
    if (If(TokenType::ERROR)) {
      throw ParsingError();
    }
    stmt();
    Match(TokenType::SEMICOLON);
  }
}

void Parser::stmt() {
  if (If(TokenType::PARAM)) {
    parameter();
  } else if (If(TokenType::CONST)) {
    constant();
  } else if (If(TokenType::FUNC)) {
    function();
  } else if (If(TokenType::RULE)) {
    rule();
  } else {
    throw ParsingError(
        {TokenType::PARAM, TokenType::CONST, TokenType::FUNC, TokenType::RULE});
  }
}

void Parser::parameter() {
  Match(TokenType::PARAM);
  std::string name;
  MatchID(&name);
  Match(TokenType::ASSIGN);
  ExprConstPtr lit = param_literal();
  if (grammar_->AddParameter(name, lit->Eval()) == AddResult::NAME_COLLISION) {
    std::ostringstream oss;
    oss << "Cannot create parameter '" << name << "'.";
    oss << " It is either a reserved name or there already exists a variable "
           "with that name.";
    throw SemanticError(oss.str());
  }
}

void Parser::constant() {
  Match(TokenType::CONST);
  std::string name;
  MatchID(&name);
  Match(TokenType::ASSIGN);
  ExprConstPtr exp = generic_expr();
  if (grammar_->AddConstant(name, exp) == AddResult::NAME_COLLISION) {
    std::ostringstream oss;
    oss << "Cannot create constant '" << name << "'.";
    oss << " It is either a reserved name or there already exists a variable "
           "with that name.";
    throw SemanticError(oss.str());
  }
}

void Parser::function() {
  Match(TokenType::FUNC);
  FuncPtr func = std::make_shared<Function>();
  MatchID(&func->name);
  arguments(&func->arguments);
  Match(TokenType::ASSIGN);
  func->expression = generic_expr();
  AddResult add = grammar_->AddFunction(func);
  if (add == AddResult::NAME_COLLISION) {
    std::ostringstream oss;
    oss << "Cannot create function '" << func->name << "'.";
    oss << " It is either a reserved name or there already exists a variable "
           "with that name.";
    throw SemanticError(oss.str());
  } else if (add == AddResult::DUPLICATE_ARG) {
    std::ostringstream oss;
    oss << "Cannot create function '" << func->name
        << "' which takes several arguments with the same name.";
    throw SemanticError(oss.str());
  }
}

void Parser::rule() {
  Match(TokenType::RULE);
  RulePtr rule = std::make_shared<Rule>();
  MatchID(&rule->predecessor);
  arguments(&rule->arguments);
  shape_locals_allowed_in_exprs_ = true;
  rule->probability = probability();
  rule->condition = condition();
  shape_locals_allowed_in_exprs_ = false;
  Match(TokenType::ASSIGN);
  shape_op_string(&rule->successor);
  rule->last_line_locator = {lookahead_.line_number(), lookahead_.file_name()};
  AddResult add = grammar_->AddRule(rule);
  if (add == AddResult::NAME_COLLISION) {
    std::ostringstream oss;
    oss << "Cannot create production rule '" << rule->predecessor << "'.";
    oss << " This name is reserved for predefined shape operations.";
    throw SemanticError(oss.str());
  } else if (add == AddResult::DUPLICATE_ARG) {
    std::ostringstream oss;
    oss << "Cannot create production rule '" << rule->predecessor << "' which ";
    oss << "takes several arguments with the same name.";
    throw SemanticError(oss.str());
  } else if (add == AddResult::PREDECESSOR_ENDS_IN_UNDERSCORE) {
    std::ostringstream oss;
    oss << "Cannot create production rule '" << rule->predecessor << "'.";
    oss << " Names that end with an underscore are reserved for terminals.";
    throw SemanticError(oss.str());
  }
}

void Parser::arguments(ArgVec* args) {
  if (MatchIf(TokenType::BRACKET_ROUND_OPEN)) {
    arg_list(args);
    if (!If(TokenType::BRACKET_ROUND_CLOSE)) {
      throw ParsingError({TokenType::COMMA, TokenType::BRACKET_ROUND_CLOSE});
    }
    Match(TokenType::BRACKET_ROUND_CLOSE);
  }
}

void Parser::arg_list(ArgVec* args) {
  args->push_back(std::string());
  MatchID(&args->back());
  while (MatchIf(TokenType::COMMA)) {
    args->push_back(std::string());
    MatchID(&args->back());
  }
}

ExprPtr Parser::probability() {
  ExprPtr ret;
  if (MatchIf(TokenType::COLON)) {
    ret = expr();
  }
  return ret;
}

ExprPtr Parser::condition() {
  ExprPtr ret;
  if (MatchIf(TokenType::DOUBLE_COLON)) {
    ret = expr();
  }
  return ret;
}

ExprPtr Parser::generic_expr() {
  ExprPtr ret;
  if (If(TokenType::BRACKET_CURLY_OPEN)) {
    ShapeOpString ops;
    shape_op_string(&ops);
    ret = MakeExpr<ValueExpr>(Value(ops));
  } else {
    ret = expr();
  }
  return ret;
}

void Parser::shape_op_string(ShapeOpString* ops) {
  Match(TokenType::BRACKET_CURLY_OPEN);
  shape_op_list(ops);
  Match(TokenType::BRACKET_CURLY_CLOSE);
}

void Parser::shape_op_list(ShapeOpString* ops) {
  while (!If(TokenType::BRACKET_CURLY_CLOSE)) {
    ops->push_back(shape_op());
  }
}

ShapeOp Parser::shape_op() {
  ShapeOp ret;
  if (MatchIf(TokenType::CARET)) {
    ret.is_reference = true;
    ret.locator = {lookahead_.line_number(), lookahead_.file_name()};
    MatchID(&ret.name);
  } else {
    ret.locator = {lookahead_.line_number(), lookahead_.file_name()};
    shape_op_id(&ret.name);
  }
  const bool prev_shape_locals_allowed = shape_locals_allowed_in_exprs_;
  shape_locals_allowed_in_exprs_ = true;
  parameters(&ret.parameters);
  shape_locals_allowed_in_exprs_ = prev_shape_locals_allowed;
  return ret;
}

void Parser::shape_op_id(std::string* name) {
  if (If(TokenType::ID)) {
    MatchID(name);
  } else if (MatchIf(TokenType::BRACKET_SQUARE_OPEN)) {
    *name = '[';
  } else if (MatchIf(TokenType::BRACKET_SQUARE_CLOSE)) {
    *name = ']';
  } else {
    throw ParsingError(
        {TokenType::ID, TokenType::CARET, TokenType::BRACKET_SQUARE_OPEN,
         TokenType::BRACKET_SQUARE_CLOSE, TokenType::BRACKET_CURLY_CLOSE});
  }
}

void Parser::parameters(ExprVec* exprs) {
  if (MatchIf(TokenType::BRACKET_ROUND_OPEN)) {
    param_list(exprs);
    if (!If(TokenType::BRACKET_ROUND_CLOSE)) {
      throw ParsingError({TokenType::COMMA, TokenType::BRACKET_ROUND_CLOSE});
    }
    Match(TokenType::BRACKET_ROUND_CLOSE);
  }
}

void Parser::param_list(ExprVec* exprs) {
  exprs->push_back(generic_expr());
  while (MatchIf(TokenType::COMMA)) {
    exprs->push_back(generic_expr());
  }
}

ExprPtr Parser::expr() {
  ExprPtr ret = join();
  Locator loc;
  while (MatchIfAndLocate(TokenType::OP_OR, &loc)) {
    ret = MakeExpr<OpExpr>(ret, OpType::OR, join(), loc);
  }
  return ret;
}

ExprPtr Parser::join() {
  ExprPtr ret = equality();
  Locator loc;
  while (MatchIfAndLocate(TokenType::OP_AND, &loc)) {
    ret = MakeExpr<OpExpr>(ret, OpType::AND, equality(), loc);
  }
  return ret;
}

ExprPtr Parser::equality() {
  ExprPtr ret = rel();
  Locator loc;
  while (true) {
    if (MatchIfAndLocate(TokenType::OP_EQUAL, &loc)) {
      ret = MakeExpr<OpExpr>(ret, OpType::EQUAL, rel(), loc);
    } else if (MatchIfAndLocate(TokenType::OP_NOT_EQUAL, &loc)) {
      ret = MakeExpr<OpExpr>(ret, OpType::NOT_EQUAL, rel(), loc);
    } else {
      break;
    }
  }
  return ret;
}

ExprPtr Parser::rel() {
  ExprPtr ret = arithmetic_expr();
  Locator loc;
  while (true) {
    if (MatchIfAndLocate(TokenType::OP_LESS, &loc)) {
      ret = MakeExpr<OpExpr>(ret, OpType::LESS, arithmetic_expr(), loc);
    } else if (MatchIfAndLocate(TokenType::OP_LESS_EQUAL, &loc)) {
      ret = MakeExpr<OpExpr>(ret, OpType::LESS_EQUAL, arithmetic_expr(), loc);
    } else if (MatchIfAndLocate(TokenType::OP_GREATER, &loc)) {
      ret = MakeExpr<OpExpr>(ret, OpType::GREATER, arithmetic_expr(), loc);
    } else if (MatchIfAndLocate(TokenType::OP_GREATER_EQUAL, &loc)) {
      ret =
          MakeExpr<OpExpr>(ret, OpType::GREATER_EQUAL, arithmetic_expr(), loc);
    } else {
      break;
    }
  }
  return ret;
}

ExprPtr Parser::arithmetic_expr() {
  ExprPtr ret = term();
  Locator loc;
  while (true) {
    if (MatchIfAndLocate(TokenType::OP_PLUS, &loc)) {
      ret = MakeExpr<OpExpr>(ret, OpType::PLUS, term(), loc);
    } else if (MatchIfAndLocate(TokenType::OP_MINUS, &loc)) {
      ret = MakeExpr<OpExpr>(ret, OpType::MINUS, term(), loc);
    } else {
      break;
    }
  }
  return ret;
}

ExprPtr Parser::term() {
  ExprPtr ret = unary();
  Locator loc;
  while (true) {
    if (MatchIfAndLocate(TokenType::OP_MULT, &loc)) {
      ret = MakeExpr<OpExpr>(ret, OpType::MULT, unary(), loc);
    } else if (MatchIfAndLocate(TokenType::OP_DIV, &loc)) {
      ret = MakeExpr<OpExpr>(ret, OpType::DIV, unary(), loc);
    } else if (MatchIfAndLocate(TokenType::OP_MODULO, &loc)) {
      ret = MakeExpr<OpExpr>(ret, OpType::MODULO, unary(), loc);
    } else {
      break;
    }
  }
  return ret;
}

ExprPtr Parser::unary() {
  ExprPtr ret;
  Locator loc;
  if (MatchIfAndLocate(TokenType::OP_NOT, &loc)) {
    ret = MakeExpr<OpExpr>(OpType::NOT, factor(), loc);
  } else if (MatchIfAndLocate(TokenType::OP_MINUS, &loc)) {
    ret = MakeExpr<OpExpr>(OpType::NEGATE, factor(), loc);
  } else {
    ret = factor();
  }
  return ret;
}

ExprPtr Parser::factor() {
  ExprPtr ret;
  if (MatchIf(TokenType::BRACKET_ROUND_OPEN)) {
    ret = MakeExpr<ScopeExpr>(expr());
    Match(TokenType::BRACKET_ROUND_CLOSE);
  } else if (If(TokenType::ID)) {
    ret = named_expr();
  } else {
    ret = literal();
  }
  return ret;
}

ExprPtr Parser::named_expr() {
  std::string id;
  Locator loc = {lookahead_.line_number(), lookahead_.file_name()};
  MatchID(&id);
  ExprVec params;
  parameters(&params);
  return MakeExpr<NameExpr>(id, loc, params, shape_locals_allowed_in_exprs_);
}

ExprPtr Parser::literal() {
  ExprPtr ret;
  if (MatchIf(TokenType::TRUE)) {
    ret = MakeExpr<ValueExpr>(Value(true));
  } else if (MatchIf(TokenType::FALSE)) {
    ret = MakeExpr<ValueExpr>(Value(false));
  } else if (If(TokenType::INT)) {
    int32_t i;
    MatchInt(&i);
    ret = MakeExpr<ValueExpr>(Value(i));
  } else if (If(TokenType::FLOAT)) {
    double d;
    MatchFloat(&d);
    ret = MakeExpr<ValueExpr>(Value(d));
  } else if (If(TokenType::STRING)) {
    std::string str;
    MatchString(&str);
    ret = MakeExpr<ValueExpr>(Value(str));
  } else {
    throw ParsingError();
  }
  return ret;
}

ExprPtr Parser::param_literal() {
  ExprPtr ret;
  if (MatchIf(TokenType::OP_MINUS)) {
    if (If(TokenType::INT)) {
      int32_t i;
      MatchInt(&i);
      i = -i;
      ret = MakeExpr<ValueExpr>(Value(i));
    } else if (If(TokenType::FLOAT)) {
      double d;
      MatchFloat(&d);
      d = -d;
      ret = MakeExpr<ValueExpr>(Value(d));
    } else {
      throw ParsingError({TokenType::INT, TokenType::FLOAT});
    }
  } else {
    ret = literal();
  }
  return ret;
}

}  // namespace parser

}  // namespace shapeml
