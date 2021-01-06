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

#include "shapeml/parser/token.h"

#include <cmath>

namespace shapeml {

namespace parser {

const char* TokenType2Str(TokenType type) {
  switch (type) {
#define Case(token)      \
  case TokenType::token: \
    return #token
    Case(ERROR);
    Case(END_OF_FILE);
    Case(NEW_LINE);
    Case(TRUE);
    Case(FALSE);
    Case(INT);
    Case(FLOAT);
    Case(STRING);
    Case(ID);
    Case(COMMA);
    Case(SEMICOLON);
    Case(DOUBLE_COLON);
    Case(COLON);
    Case(CARET);
    Case(HASHTAG);
    Case(BRACKET_ROUND_OPEN);
    Case(BRACKET_ROUND_CLOSE);
    Case(BRACKET_SQUARE_OPEN);
    Case(BRACKET_SQUARE_CLOSE);
    Case(BRACKET_CURLY_OPEN);
    Case(BRACKET_CURLY_CLOSE);
    Case(CONST);
    Case(FUNC);
    Case(PARAM);
    Case(RULE);
    Case(OP_PLUS);
    Case(OP_MINUS);
    Case(OP_MULT);
    Case(OP_DIV);
    Case(OP_MODULO);
    Case(OP_LESS_EQUAL);
    Case(OP_GREATER_EQUAL);
    Case(OP_LESS);
    Case(OP_GREATER);
    Case(OP_EQUAL);
    Case(OP_NOT_EQUAL);
    Case(OP_AND);
    Case(OP_OR);
    Case(OP_NOT);
    Case(ASSIGN);
#undef Case
  }

  return "";  // Avoid VS warning.
}

Token::Token(TokenType t, int line, const std::string& file_name)
    : type_(t), line_number_(line) {
  SetFileName(file_name);
}

Token::Token(int val, int line, const std::string& file_name)
    : type_(TokenType::INT), line_number_(line), i_(val) {
  SetFileName(file_name);
}

Token::Token(double val, int line, const std::string& file_name)
    : type_(TokenType::FLOAT), line_number_(line), f_(val) {
  SetFileName(file_name);
}

Token::~Token() {
  if (type_ == TokenType::STRING || type_ == TokenType::ID) {
    delete s_;
  }
}

Token::Token(const Token& tok) { DeepCopy(tok); }

Token& Token::operator=(const Token& tok) {
  std::string* tmp = nullptr;
  if (type_ == TokenType::STRING || type_ == TokenType::ID) {
    tmp = s_;
  }
  DeepCopy(tok);
  if (tmp) {
    delete tmp;
  }
  return *this;
}

Token Token::FromString(const std::string& str, int line,
                        const std::string& file_name) {
  Token tok(TokenType::STRING, line, file_name);
  tok.s_ = new std::string(str);
  return tok;
}

Token Token::FromID(const std::string& id, int line,
                    const std::string& file_name) {
  Token tok(TokenType::ID, line, file_name);
  tok.s_ = new std::string(id);
  return tok;
}

bool Token::operator==(const Token& tok) const {
  if (type_ != tok.type_) {
    return false;
  }

  if (line_number_ != tok.line_number_) {
    return false;
  }

  if (*file_name_ != *tok.file_name_) {
    return false;
  }

  switch (type_) {
    case TokenType::INT:
      return i_ == tok.i_;
    case TokenType::FLOAT:
      // TODO(stefalie): Epsilon test is sketchy, should probably be the exact
      // epsilon to the next smaller/larger (std::nextafterf) of the smaller
      // of both values?
      // We have the same problem in expr_rule.cc for value comparison.
      return std::fabs(f_ - tok.f_) < 0.00000001;
    case TokenType::STRING:
    case TokenType::ID:
      return *s_ == *tok.s_;
    default:
      break;
  }

  return true;
}

void Token::SetFileName(const std::string& file_name) {
  if (!file_name.empty()) {
    auto pair = file_name_cache_.insert(file_name);
    file_name_ = &(*pair.first);
  }
}

void Token::DeepCopy(const Token& tok) {
  line_number_ = tok.line_number_;
  file_name_ = tok.file_name_;
  switch (tok.type_) {
    case TokenType::INT:
      i_ = tok.i_;
      break;
    case TokenType::FLOAT:
      f_ = tok.f_;
      break;
    case TokenType::STRING:
    case TokenType::ID:
      s_ = new std::string(*tok.s_);
      break;
    default:
      break;
  }
  type_ = tok.type_;
}

std::ostream& operator<<(std::ostream& stream, const Token& tok) {
  stream << "Token (type: " << TokenType2Str(tok.type());
  switch (tok.type()) {
    case TokenType::INT:
      stream << ", value: " << tok.i();
      break;
    case TokenType::FLOAT:
      stream << ", value: " << tok.f();
      break;
    case TokenType::STRING:
      stream << ", value: \"" << tok.s() << '"';
      break;
    case TokenType::ID:
      stream << ", value: " << tok.s();
      break;
    default:
      break;
  }
  stream << ", line: " << tok.line_number();
  stream << ", file: " << *tok.file_name() << ")";
  return stream;
}

std::unordered_set<std::string> Token::file_name_cache_;

}  // namespace parser

}  // namespace shapeml
