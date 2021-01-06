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

#include <ostream>
#include <string>
#include <unordered_set>
#include <vector>

namespace shapeml {

namespace parser {

enum class TokenType : char {
  ERROR,
  END_OF_FILE,
  NEW_LINE,
  TRUE,
  FALSE,
  INT,
  FLOAT,
  STRING,
  ID,
  COMMA,
  SEMICOLON,
  DOUBLE_COLON,
  COLON,
  CARET,
  HASHTAG,
  BRACKET_ROUND_OPEN,
  BRACKET_ROUND_CLOSE,
  BRACKET_SQUARE_OPEN,
  BRACKET_SQUARE_CLOSE,
  BRACKET_CURLY_OPEN,
  BRACKET_CURLY_CLOSE,
  CONST,
  FUNC,
  PARAM,
  RULE,
  OP_PLUS,
  OP_MINUS,
  OP_MULT,
  OP_DIV,
  OP_MODULO,
  OP_LESS_EQUAL,
  OP_GREATER_EQUAL,
  OP_LESS,
  OP_GREATER,
  OP_EQUAL,
  OP_NOT_EQUAL,
  OP_AND,
  OP_OR,
  OP_NOT,
  ASSIGN,
};

const char* TokenType2Str(TokenType type);

// Read only struct for tokens.
class Token {
 public:
  explicit Token(TokenType t, int line, const std::string& file_name);
  explicit Token(int val, int line, const std::string& file_name);
  explicit Token(double val, int line, const std::string& file_name);
  ~Token();

  Token(const Token& tok);
  Token& operator=(const Token& tok);

  // For most tokens, the above constructors can be used, but for IDs and
  // strings the constructor would not be able to distinguish, therefore these
  // methods.
  static Token FromString(const std::string& str, int line,
                          const std::string& file_name);
  static Token FromID(const std::string& id, int line,
                      const std::string& file_name);

  // Used only for the unit tests.
  bool operator==(const Token& tok) const;

  // Getters and setters.
  TokenType type() const { return type_; }

  int line_number() const { return line_number_; }
  const std::string* file_name() const { return file_name_; }

  int i() const { return i_; }
  double f() const { return f_; }
  const std::string& s() const { return *s_; }

 private:
  void SetFileName(const std::string& file_name);
  void DeepCopy(const Token& tok);

  TokenType type_;

  int line_number_;
  const std::string* file_name_;

  union {
    int32_t i_;
    double f_;
    std::string* s_;
  };

  static std::unordered_set<std::string> file_name_cache_;
};

typedef std::vector<Token> TokenVec;

std::ostream& operator<<(std::ostream& stream, const Token& tok);

}  // namespace parser

}  // namespace shapeml
