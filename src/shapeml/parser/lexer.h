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

#include <fstream>
#include <stack>
#include <string>
#include <unordered_map>

#include "shapeml/parser/token.h"

namespace shapeml {

namespace parser {

class Lexer {
 public:
  // Returns true if an error occurred.
  bool Init(const std::string& file_name);

  // Returns true if an error occurred.
  bool Tokenize(TokenVec* tokens);

  Token Scan();

 private:
  Token NextToken();
  bool HandlePreprocessorDirectives();

  static struct InitKeywordTable { InitKeywordTable(); } init_keyword_table_;
  static std::unordered_map<std::string, TokenType> keyword_table_;

  struct LexerState {
    std::string file_name;
    std::ifstream input;
    int line_number;
    bool line_start;
  };
  std::stack<LexerState> state_stack_;

  std::string& FileName() { return state_stack_.top().file_name; }
  std::ifstream& Input() { return state_stack_.top().input; }
  int& LineNumber() { return state_stack_.top().line_number; }
  bool& LineStart() { return state_stack_.top().line_start; }

  std::ostream& Error();
  std::ostream& Error(TokenType expected);
};

}  // namespace parser

}  // namespace shapeml
