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

#include "shapeml/parser/lexer.h"

#include <cassert>
#include <iostream>
#include <limits>
#include <utility>

namespace shapeml {

namespace parser {

bool Lexer::Init(const std::string& file_name) {
  LexerState state;
  state.file_name = file_name;
  state.line_number = 1;
  state.line_start = true;

  state.input.open(state.file_name);
  if (!state.input.good()) {
    if (!state_stack_.empty()) {
      --LineNumber();  // Since line number jumped ahead.
    }
    Error() << "Cannot open file '" << state.file_name << "' for lexing.\n";
    return true;
  }

  state_stack_.push(std::move(state));
  return false;
}

bool Lexer::Tokenize(TokenVec* tokens) {
  assert(tokens->empty());

  Token tok = Scan();
  while (tok.type() != TokenType::END_OF_FILE &&
         tok.type() != TokenType::ERROR) {
    tokens->push_back(tok);
    tok = Scan();
  }
  tokens->push_back(tok);

  return tok.type() == TokenType::ERROR;
}

Token Lexer::Scan() {
  assert(!state_stack_.empty() && Input().is_open());

  bool line_start = LineStart();

  Token tok = NextToken();
  while (tok.type() == TokenType::NEW_LINE) {
    line_start = true;
    tok = NextToken();
  }
  LineStart() = false;

  if (tok.type() == TokenType::HASHTAG) {
    // That the preprocessor directive starts on a new line has to be checked
    // here.
    if (!line_start) {
      Error() << "Preprocessor directives must be on their own line.\n";
      return Token(TokenType::ERROR, LineNumber(), FileName());
    }
    if (HandlePreprocessorDirectives()) {
      return Token(TokenType::ERROR, LineNumber(), FileName());
    }

    // That the preprocessor directive ends with new line is directly checked
    // inside 'HandlePreprocessorDirectives'.
    return Scan();
  } else if (tok.type() == TokenType::END_OF_FILE && state_stack_.size() > 1) {
    state_stack_.pop();
    return Scan();
  }

  return tok;
}

Token Lexer::NextToken() {
  assert(!state_stack_.empty() && Input().is_open());

  while (true) {
    char ch = Input().get();

    // EOF
    if (!Input().good()) {
      return Token(TokenType::END_OF_FILE, LineNumber(), FileName());
    }

    if (ch == ' ' || ch == '\t') {
      // White spaces
    } else if (ch == '\n') {
      // Line feed
      return Token(TokenType::NEW_LINE, LineNumber()++, FileName());
    } else if (ch == '\r') {
      // Carriage return and crlf on Windows
      if (Input().peek() == '\n') {  // Handle \r\n case.
        Input().get();
      }
      return Token(TokenType::NEW_LINE, LineNumber()++, FileName());
    } else if (ch == '/' && Input().peek() == '*') {
      // Comments /* ... */
      Input().get();

      ch = Input().get();
      while (true) {
        if (ch == '*' && Input().peek() == '/') {
          Input().get();
          break;
        }

        if (!Input().good()) {
          Error() << "Reached EOF before closing comment with */.\n";
          return Token(TokenType::ERROR, LineNumber(), FileName());
        }

        if (ch == '\n') {
          ++LineNumber();
        } else if (ch == '\r') {
          ++LineNumber();
          if (Input().peek() == '\n') {  // Handle \r\n case.
            Input().get();
          }
        }

        ch = Input().get();
      }
    } else if (ch == '/' && Input().peek() == '/') {
      // Comments // ...
      Input().get();
      while (Input().peek() != '\n' && Input().peek() != '\r' &&
             Input().good()) {
        ch = Input().get();
      }
    } else if (isdigit(ch)) {
      // Doubles and integers
      //
      // We handle overflows similar to:
      // http://web.cecs.pdx.edu/~harry/compilers/yapp/lexer.c.
      bool overflow_int = false;

      int sum = 0;
      double sum_d = 0.0;
      while (isdigit(ch)) {
        const int new_sum = 10 * sum + (ch - '0');
        if (new_sum < sum) {
          overflow_int = true;
        }
        sum = new_sum;
        sum_d = 10.0 * sum_d + static_cast<double>(ch - '0');
        ch = Input().get();
      }

      if (ch != '.') {
        Input().putback(ch);
        if (overflow_int) {
          Error() << "Integer overflow.\n";
          return Token(TokenType::ERROR, LineNumber(), FileName());
        }
        return Token(sum, LineNumber(), FileName());
      } else {
        ch = Input().get();
        if (!isdigit(ch)) {
          Error() << "Expected a digit after the decimal point.\n";
          return Token(TokenType::ERROR, LineNumber(), FileName());
        }

        double exp = 1.0;
        do {
          exp *= 10.0;
          sum_d = sum_d + static_cast<double>(ch - '0') / exp;
          ch = Input().get();
        } while (isdigit(ch));
        Input().putback(ch);

        if (sum_d > std::numeric_limits<double>::max()) {
          Error() << "Floating point number at infinity.\n";
          return Token(TokenType::ERROR, LineNumber(), FileName());
        }
        return Token(sum_d, LineNumber(), FileName());
      }
    } else if (ch == '"') {
      // Strings
      std::string str;
      while (true) {
        ch = Input().get();

        if (!Input().good()) {
          Error() << "Reached EOF inside string.\n";
          return Token(TokenType::ERROR, LineNumber(), FileName());
        }
        if (ch < 32 || ch > 126) {
          Error() << "Illegal character in string.\n";
          return Token(TokenType::ERROR, LineNumber(), FileName());
        }

        if (ch == '"') {
          break;
        } else if (ch == '\\' && Input().peek() == '"') {
          Input().get();
          str += '"';
        } else if (ch == '\\' && Input().peek() == 'n') {
          Input().get();
          str += '\n';
        } else if (ch == '\\' && Input().peek() == 't') {
          Input().get();
          str += '\t';
        } else {
          str += ch;
        }
      }
      return Token::FromString(str, LineNumber(), FileName());
    } else if (isalpha(ch) || ch == '_') {
      // Identifiers and keywords
      std::string str;
      while (isalpha(ch) || isdigit(ch) || ch == '_') {
        str += ch;
        ch = Input().get();
      }
      Input().putback(ch);

      const auto it = keyword_table_.find(str);
      if (it != keyword_table_.end()) {
        return Token(it->second, LineNumber(), FileName());
      }
      return Token::FromID(str, LineNumber(), FileName());
    } else if (ch == ',') {  // Operators and special chars from here on.
      return Token(TokenType::COMMA, LineNumber(), FileName());
    } else if (ch == ';') {
      return Token(TokenType::SEMICOLON, LineNumber(), FileName());
    } else if (ch == ':' && Input().peek() == ':') {
      Input().get();
      return Token(TokenType::DOUBLE_COLON, LineNumber(), FileName());
    } else if (ch == ':') {  // Needs to be after ::.
      return Token(TokenType::COLON, LineNumber(), FileName());
    } else if (ch == '^') {
      return Token(TokenType::CARET, LineNumber(), FileName());
    } else if (ch == '#') {
      return Token(TokenType::HASHTAG, LineNumber(), FileName());
    } else if (ch == '(') {
      return Token(TokenType::BRACKET_ROUND_OPEN, LineNumber(), FileName());
    } else if (ch == ')') {
      return Token(TokenType::BRACKET_ROUND_CLOSE, LineNumber(), FileName());
    } else if (ch == '[') {
      return Token(TokenType::BRACKET_SQUARE_OPEN, LineNumber(), FileName());
    } else if (ch == ']') {
      return Token(TokenType::BRACKET_SQUARE_CLOSE, LineNumber(), FileName());
    } else if (ch == '{') {
      return Token(TokenType::BRACKET_CURLY_OPEN, LineNumber(), FileName());
    } else if (ch == '}') {
      return Token(TokenType::BRACKET_CURLY_CLOSE, LineNumber(), FileName());
    } else if (ch == '+') {
      return Token(TokenType::OP_PLUS, LineNumber(), FileName());
    } else if (ch == '-') {
      return Token(TokenType::OP_MINUS, LineNumber(), FileName());
    } else if (ch == '*') {
      return Token(TokenType::OP_MULT, LineNumber(), FileName());
    } else if (ch == '/') {
      return Token(TokenType::OP_DIV, LineNumber(), FileName());
    } else if (ch == '%') {
      return Token(TokenType::OP_MODULO, LineNumber(), FileName());
    } else if (ch == '<' && Input().peek() == '=') {
      Input().get();
      return Token(TokenType::OP_LESS_EQUAL, LineNumber(), FileName());
    } else if (ch == '>' && Input().peek() == '=') {
      Input().get();
      return Token(TokenType::OP_GREATER_EQUAL, LineNumber(), FileName());
    } else if (ch == '<') {  // Needs to be after <=.
      return Token(TokenType::OP_LESS, LineNumber(), FileName());
    } else if (ch == '>') {  // Needs to be after >=.
      return Token(TokenType::OP_GREATER, LineNumber(), FileName());
    } else if (ch == '=' && Input().peek() == '=') {
      Input().get();
      return Token(TokenType::OP_EQUAL, LineNumber(), FileName());
    } else if (ch == '!' && Input().peek() == '=') {
      Input().get();
      return Token(TokenType::OP_NOT_EQUAL, LineNumber(), FileName());
    } else if (ch == '&' && Input().peek() == '&') {
      Input().get();
      return Token(TokenType::OP_AND, LineNumber(), FileName());
    } else if (ch == '|' && Input().peek() == '|') {
      Input().get();
      return Token(TokenType::OP_OR, LineNumber(), FileName());
    } else if (ch == '!') {
      return Token(TokenType::OP_NOT, LineNumber(), FileName());
    } else if (ch == '=') {  // Needs to be after ==.
      return Token(TokenType::ASSIGN, LineNumber(), FileName());
    } else {
      // Unmatched input error
      Error() << "Unknown char: '" << ch << "'.\n";
      return Token(TokenType::ERROR, LineNumber(), FileName());
    }
  }
}

bool Lexer::HandlePreprocessorDirectives() {
  // We could create its own, complete preprocessor parser here, but since we
  // only support '#include' for now, let's just do everything here.
  Token tok = NextToken();
  if (tok.type() != TokenType::ID) {
    Error(tok.type()) << "Expected a token of type "
                      << TokenType2Str(TokenType::ID)
                      << " that is recognized by the preprocesor.\n";
    return true;
  }

  std::string include_path;

  if (tok.s() == "include") {
    tok = NextToken();
    if (tok.type() != TokenType::STRING) {
      Error(tok.type()) << "Expected a token of type "
                        << TokenType2Str(TokenType::STRING)
                        << " that contains a path to a file.\n";
      return true;
    }

    include_path = tok.s();
  } else {
    Error() << '\'' << tok.s() << "\' is not a valid preprocessor directive.\n";
    return true;
  }

  tok = NextToken();
  if (tok.type() != TokenType::NEW_LINE) {
    Error(tok.type()) << "Preprocessor directives must be on their own line.\n";
    return true;
  }
  LineStart() = true;

  if (!include_path.empty()) {
#ifdef _WIN32
    size_t slash = FileName().find_last_of("\\/");
#else
    size_t slash = FileName().rfind("/");
#endif
    include_path = FileName().substr(0, slash + 1) + include_path;

    if (state_stack_.size() == 20) {
      --LineNumber();  // Since line number jumped ahead.
      Error() << "To many nested include files. Reached max level of 20.\n";
      return true;
    }

    if (Init(include_path)) {
      return true;
    }
  }

  return false;
}

Lexer::InitKeywordTable::InitKeywordTable() {
  keyword_table_["true"] = TokenType::TRUE;
  keyword_table_["false"] = TokenType::FALSE;
  keyword_table_["const"] = TokenType::CONST;
  keyword_table_["func"] = TokenType::FUNC;
  keyword_table_["param"] = TokenType::PARAM;
  keyword_table_["rule"] = TokenType::RULE;
}

std::unordered_map<std::string, TokenType> Lexer::keyword_table_;

Lexer::InitKeywordTable Lexer::init_keyword_table_;

std::ostream& Lexer::Error() {
  if (state_stack_.empty()) {
    std::cerr << "ERROR: ";
  } else {
    std::cerr << "ERROR (file: " << FileName();
    std::cerr << ", line " << LineNumber() << "): ";
  }
  return std::cerr;
}

std::ostream& Lexer::Error(TokenType expected) {
  Error() << "Unexpected token of type " << TokenType2Str(expected) << ". ";
  return std::cerr;
}

}  // namespace parser

}  // namespace shapeml
