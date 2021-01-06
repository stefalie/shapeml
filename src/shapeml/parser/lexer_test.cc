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

#include "shapeml/parser/lexer.h"

#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <iostream>

using shapeml::parser::Lexer;
using shapeml::parser::Token;
using shapeml::parser::TokenType;
using shapeml::parser::TokenVec;

class LexerTest : public testing::Test {
 protected:
  virtual void SetUp() {
    ofs_.open(file_name_);
    if (!ofs_.good()) {
      std::cerr << "ERROR: Couldn't open file " << file_name_ << std::endl;
    }

    old_stream_buf_ = std::cerr.rdbuf(nullptr);
  }

  void WriteFile(const std::string& content) {
    ofs_ << content;
    ofs_.close();
  }

  virtual void TearDown() {
    std::cerr.rdbuf(old_stream_buf_);

    if (std::remove(file_name_.c_str()) != 0) {
      std::cerr << "ERROR: Couldn't remove file " << file_name_ << std::endl;
    }
  }

  std::string file_name_ = "test_lexer_tmp";
  std::ofstream ofs_;

  std::streambuf* old_stream_buf_;
};

TEST_F(LexerTest, EmptyFile) {
  TokenVec ground_truth;
  ground_truth.push_back(Token(TokenType::END_OF_FILE, 1, file_name_));

  WriteFile("");
  Lexer lexer;
  lexer.Init(file_name_);
  TokenVec tokens;
  EXPECT_FALSE(lexer.Tokenize(&tokens));
  EXPECT_EQ(1, tokens.size());
  for (unsigned i = 0; i < std::min(ground_truth.size(), tokens.size()); ++i) {
    EXPECT_EQ(ground_truth[i], tokens[i]) << "Token " << i << " is wrong!";
  }
}

TEST_F(LexerTest, OperatorsAndSmallSymbols) {
  const std::string str(R"delim(
=!+==- - /><<===>==&&&&||];[,)(){:::%}^^

   :)delim");

  TokenVec ground_truth;
  ground_truth.push_back(Token(TokenType::ASSIGN, 2, file_name_));
  ground_truth.push_back(Token(TokenType::OP_NOT, 2, file_name_));
  ground_truth.push_back(Token(TokenType::OP_PLUS, 2, file_name_));
  ground_truth.push_back(Token(TokenType::OP_EQUAL, 2, file_name_));
  ground_truth.push_back(Token(TokenType::OP_MINUS, 2, file_name_));
  ground_truth.push_back(Token(TokenType::OP_MINUS, 2, file_name_));
  ground_truth.push_back(Token(TokenType::OP_DIV, 2, file_name_));
  ground_truth.push_back(Token(TokenType::OP_GREATER, 2, file_name_));
  ground_truth.push_back(Token(TokenType::OP_LESS, 2, file_name_));
  ground_truth.push_back(Token(TokenType::OP_LESS_EQUAL, 2, file_name_));
  ground_truth.push_back(Token(TokenType::OP_EQUAL, 2, file_name_));
  ground_truth.push_back(Token(TokenType::OP_GREATER_EQUAL, 2, file_name_));
  ground_truth.push_back(Token(TokenType::ASSIGN, 2, file_name_));
  ground_truth.push_back(Token(TokenType::OP_AND, 2, file_name_));
  ground_truth.push_back(Token(TokenType::OP_AND, 2, file_name_));
  ground_truth.push_back(Token(TokenType::OP_OR, 2, file_name_));
  ground_truth.push_back(Token(TokenType::BRACKET_SQUARE_CLOSE, 2, file_name_));
  ground_truth.push_back(Token(TokenType::SEMICOLON, 2, file_name_));
  ground_truth.push_back(Token(TokenType::BRACKET_SQUARE_OPEN, 2, file_name_));
  ground_truth.push_back(Token(TokenType::COMMA, 2, file_name_));
  ground_truth.push_back(Token(TokenType::BRACKET_ROUND_CLOSE, 2, file_name_));
  ground_truth.push_back(Token(TokenType::BRACKET_ROUND_OPEN, 2, file_name_));
  ground_truth.push_back(Token(TokenType::BRACKET_ROUND_CLOSE, 2, file_name_));
  ground_truth.push_back(Token(TokenType::BRACKET_CURLY_OPEN, 2, file_name_));
  ground_truth.push_back(Token(TokenType::DOUBLE_COLON, 2, file_name_));
  ground_truth.push_back(Token(TokenType::COLON, 2, file_name_));
  ground_truth.push_back(Token(TokenType::OP_MODULO, 2, file_name_));
  ground_truth.push_back(Token(TokenType::BRACKET_CURLY_CLOSE, 2, file_name_));
  ground_truth.push_back(Token(TokenType::CARET, 2, file_name_));
  ground_truth.push_back(Token(TokenType::CARET, 2, file_name_));
  ground_truth.push_back(Token(TokenType::COLON, 4, file_name_));
  ground_truth.push_back(Token(TokenType::END_OF_FILE, 4, file_name_));

  WriteFile(str);
  Lexer lexer;
  lexer.Init(file_name_);
  TokenVec tokens;
  EXPECT_FALSE(lexer.Tokenize(&tokens));
  EXPECT_EQ(ground_truth.size(), tokens.size());
  for (unsigned i = 0; i < std::min(ground_truth.size(), tokens.size()); ++i) {
    EXPECT_EQ(ground_truth[i], tokens[i]) << "Token " << i << " is wrong!";
  }
}

TEST_F(LexerTest, KeywordsAndIDs) {
  const std::string str(R"delim(
true_,false, 	 wejusthad2spacesAND_aTAB true,
trues,
falses,
param
whatconst const
param paramrule rule rulefunc func Func Rule Param _rule
wrong_end
_wrong_6end
11wrong_6__end __
  )delim");

  TokenVec ground_truth;
  ground_truth.push_back(Token::FromID("true_", 2, file_name_));
  ground_truth.push_back(Token(TokenType::COMMA, 2, file_name_));
  ground_truth.push_back(Token(TokenType::FALSE, 2, file_name_));
  ground_truth.push_back(Token(TokenType::COMMA, 2, file_name_));
  ground_truth.push_back(
      Token::FromID("wejusthad2spacesAND_aTAB", 2, file_name_));
  ground_truth.push_back(Token(TokenType::TRUE, 2, file_name_));
  ground_truth.push_back(Token(TokenType::COMMA, 2, file_name_));
  ground_truth.push_back(Token::FromID("trues", 3, file_name_));
  ground_truth.push_back(Token(TokenType::COMMA, 3, file_name_));
  ground_truth.push_back(Token::FromID("falses", 4, file_name_));
  ground_truth.push_back(Token(TokenType::COMMA, 4, file_name_));
  ground_truth.push_back(Token(TokenType::PARAM, 5, file_name_));
  ground_truth.push_back(Token::FromID("whatconst", 6, file_name_));
  ground_truth.push_back(Token(TokenType::CONST, 6, file_name_));
  ground_truth.push_back(Token(TokenType::PARAM, 7, file_name_));
  ground_truth.push_back(Token::FromID("paramrule", 7, file_name_));
  ground_truth.push_back(Token(TokenType::RULE, 7, file_name_));
  ground_truth.push_back(Token::FromID("rulefunc", 7, file_name_));
  ground_truth.push_back(Token(TokenType::FUNC, 7, file_name_));
  ground_truth.push_back(Token::FromID("Func", 7, file_name_));
  ground_truth.push_back(Token::FromID("Rule", 7, file_name_));
  ground_truth.push_back(Token::FromID("Param", 7, file_name_));
  ground_truth.push_back(Token::FromID("_rule", 7, file_name_));
  ground_truth.push_back(Token::FromID("wrong_end", 8, file_name_));
  ground_truth.push_back(Token::FromID("_wrong_6end", 9, file_name_));
  ground_truth.push_back(Token(11, 10, file_name_));
  ground_truth.push_back(Token::FromID("wrong_6__end", 10, file_name_));
  ground_truth.push_back(Token::FromID("__", 10, file_name_));
  ground_truth.push_back(Token(TokenType::END_OF_FILE, 11, file_name_));

  WriteFile(str);
  Lexer lexer;
  lexer.Init(file_name_);
  TokenVec tokens;
  EXPECT_FALSE(lexer.Tokenize(&tokens));
  EXPECT_EQ(ground_truth.size(), tokens.size());
  for (unsigned i = 0; i < std::min(ground_truth.size(), tokens.size()); ++i) {
    EXPECT_EQ(ground_truth[i], tokens[i]) << "Token " << i << " is wrong!";
  }
}

TEST_F(LexerTest, Numbers) {
  const std::string str(R"delim(
1635486,
-20,10e+10 10.123,
10.1asd
asdf.20
  )delim");

  TokenVec ground_truth;
  ground_truth.push_back(Token(1635486, 2, file_name_));
  ground_truth.push_back(Token(TokenType::COMMA, 2, file_name_));
  ground_truth.push_back(Token(TokenType::OP_MINUS, 3, file_name_));
  ground_truth.push_back(Token(20, 3, file_name_));
  ground_truth.push_back(Token(TokenType::COMMA, 3, file_name_));
  ground_truth.push_back(Token(10, 3, file_name_));
  ground_truth.push_back(Token::FromID("e", 3, file_name_));
  ground_truth.push_back(Token(TokenType::OP_PLUS, 3, file_name_));
  ground_truth.push_back(Token(10, 3, file_name_));
  ground_truth.push_back(Token(10.123, 3, file_name_));
  ground_truth.push_back(Token(TokenType::COMMA, 3, file_name_));
  ground_truth.push_back(Token(10.1, 4, file_name_));
  ground_truth.push_back(Token::FromID("asd", 4, file_name_));
  ground_truth.push_back(Token::FromID("asdf", 5, file_name_));
  ground_truth.push_back(Token(TokenType::ERROR, 5, file_name_));

  WriteFile(str);
  Lexer lexer;
  lexer.Init(file_name_);
  TokenVec tokens;
  EXPECT_TRUE(lexer.Tokenize(&tokens));
  EXPECT_EQ(ground_truth.size(), tokens.size());
  for (unsigned i = 0; i < std::min(ground_truth.size(), tokens.size()); ++i) {
    EXPECT_EQ(ground_truth[i], tokens[i]) << "Token " << i << " is wrong!";
  }
}

TEST_F(LexerTest, IntOverflow) {
  const std::string str(R"delim(
// This is the max 32bit signed integer value 2^31-1
2147483647
// The following one should cause an overflow, 2^31
2147483648
  )delim");

  TokenVec ground_truth;
  ground_truth.push_back(Token(2147483647, 3, file_name_));
  ground_truth.push_back(Token(TokenType::ERROR, 5, file_name_));

  WriteFile(str);
  Lexer lexer;
  lexer.Init(file_name_);
  TokenVec tokens;
  EXPECT_TRUE(lexer.Tokenize(&tokens));
  EXPECT_EQ(ground_truth.size(), tokens.size());
  for (unsigned i = 0; i < std::min(ground_truth.size(), tokens.size()); ++i) {
    EXPECT_EQ(ground_truth[i], tokens[i]) << "Token " << i << " is wrong.";
  }
}

TEST_F(LexerTest, Strings) {
  const std::string str(R"delim(
"string" "anot\\her \" str 1 ing"
"-%!+==/><<==:::[]}{)(=>==&&||" "" "\"'\"'"
  )delim");

  TokenVec ground_truth;
  ground_truth.push_back(Token::FromString("string", 2, file_name_));
  ground_truth.push_back(
      Token::FromString("anot\\\\her \" str 1 ing", 2, file_name_));
  ground_truth.push_back(
      Token::FromString("-%!+==/><<==:::[]}{)(=>==&&||", 3, file_name_));
  ground_truth.push_back(Token::FromString("", 3, file_name_));
  ground_truth.push_back(Token::FromString("\"'\"'", 3, file_name_));
  ground_truth.push_back(Token(TokenType::END_OF_FILE, 4, file_name_));

  WriteFile(str);
  Lexer lexer;
  lexer.Init(file_name_);
  TokenVec tokens;
  EXPECT_FALSE(lexer.Tokenize(&tokens));
  EXPECT_EQ(ground_truth.size(), tokens.size());
  for (unsigned i = 0; i < std::min(ground_truth.size(), tokens.size()); ++i) {
    EXPECT_EQ(ground_truth[i], tokens[i]) << "Token " << i << " is wrong!";
  }
}

TEST_F(LexerTest, UnmatchedChar) {
  const std::string str(R"delim(
.// Illegal char in the end.
  )delim");

  TokenVec ground_truth;
  ground_truth.push_back(Token(TokenType::ERROR, 2, file_name_));

  WriteFile(str);
  Lexer lexer;
  lexer.Init(file_name_);
  TokenVec tokens;
  EXPECT_TRUE(lexer.Tokenize(&tokens));
  EXPECT_EQ(1, tokens.size());
  EXPECT_EQ(ground_truth.size(), tokens.size());
  for (unsigned i = 0; i < std::min(ground_truth.size(), tokens.size()); ++i) {
    EXPECT_EQ(ground_truth[i], tokens[i]) << "Token " << i << " is wrong!";
  }
}

TEST_F(LexerTest, Comments) {
  const std::string str(R"delim(
// this is a comment
, /* asdf */ , // ,
, /* , //
   * This is comment test without anything
   * More empty comment lines

   * fun stuff ->!+==-->/><<===>==&&||~',)():
   */,
/*/
  )delim");

  TokenVec ground_truth;
  ground_truth.push_back(Token(TokenType::COMMA, 3, file_name_));
  ground_truth.push_back(Token(TokenType::COMMA, 3, file_name_));
  ground_truth.push_back(Token(TokenType::COMMA, 4, file_name_));
  ground_truth.push_back(Token(TokenType::COMMA, 9, file_name_));
  ground_truth.push_back(Token(TokenType::ERROR, 11, file_name_));

  WriteFile(str);
  Lexer lexer;
  lexer.Init(file_name_);
  TokenVec tokens;
  EXPECT_TRUE(lexer.Tokenize(&tokens));
  EXPECT_EQ(ground_truth.size(), tokens.size());
  for (unsigned i = 0; i < std::min(ground_truth.size(), tokens.size()); ++i) {
    EXPECT_EQ(ground_truth[i], tokens[i]) << "Token " << i << " is wrong!";
  }
}

TEST_F(LexerTest, RealExample) {
  const std::string str(R"delim(
const length = 4;
const rest_of_rule = { s(0.0, 0.0, 1.0 * sz) i("assets/teapot.obj") Teapot_ };

rule P(recursion_count) :: (recursion_count > 0) = { End_ ty(length) r(0.0, 10.0, 10.0) P(recursion_count - 1) };
rule P(recursion_count) : 9 :: recursion_count == 0  = { color(0.75, 0.5, 0.25) ^rest_of_rule };
rule P(recursion_count) : 1 :: recursion_count == 0  = { color(0.25, 0.5, 0.75) ^rest_of_rule };
  )delim");

  TokenVec ground_truth;
  int line_no = 2;
  ground_truth.push_back(Token(TokenType::CONST, line_no, file_name_));
  ground_truth.push_back(Token::FromID("length", line_no, file_name_));
  ground_truth.push_back(Token(TokenType::ASSIGN, line_no, file_name_));
  ground_truth.push_back(Token(4, line_no, file_name_));
  ground_truth.push_back(Token(TokenType::SEMICOLON, line_no, file_name_));

  ++line_no;
  ground_truth.push_back(Token(TokenType::CONST, line_no, file_name_));
  ground_truth.push_back(Token::FromID("rest_of_rule", line_no, file_name_));
  ground_truth.push_back(Token(TokenType::ASSIGN, line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_CURLY_OPEN, line_no, file_name_));
  ground_truth.push_back(Token::FromID("s", line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_ROUND_OPEN, line_no, file_name_));
  ground_truth.push_back(Token(0.0, line_no, file_name_));
  ground_truth.push_back(Token(TokenType::COMMA, line_no, file_name_));
  ground_truth.push_back(Token(0.0, line_no, file_name_));
  ground_truth.push_back(Token(TokenType::COMMA, line_no, file_name_));
  ground_truth.push_back(Token(1.0, line_no, file_name_));
  ground_truth.push_back(Token(TokenType::OP_MULT, line_no, file_name_));
  ground_truth.push_back(Token::FromID("sz", line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_ROUND_CLOSE, line_no, file_name_));
  ground_truth.push_back(Token::FromID("i", line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_ROUND_OPEN, line_no, file_name_));
  ground_truth.push_back(
      Token::FromString("assets/teapot.obj", line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_ROUND_CLOSE, line_no, file_name_));
  ground_truth.push_back(Token::FromID("Teapot_", line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_CURLY_CLOSE, line_no, file_name_));
  ground_truth.push_back(Token(TokenType::SEMICOLON, line_no, file_name_));

  line_no += 2;
  ground_truth.push_back(Token(TokenType::RULE, line_no, file_name_));
  ground_truth.push_back(Token::FromID("P", line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_ROUND_OPEN, line_no, file_name_));
  ground_truth.push_back(Token::FromID("recursion_count", line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_ROUND_CLOSE, line_no, file_name_));
  ground_truth.push_back(Token(TokenType::DOUBLE_COLON, line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_ROUND_OPEN, line_no, file_name_));
  ground_truth.push_back(Token::FromID("recursion_count", line_no, file_name_));
  ground_truth.push_back(Token(TokenType::OP_GREATER, line_no, file_name_));
  ground_truth.push_back(Token(0, line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_ROUND_CLOSE, line_no, file_name_));
  ground_truth.push_back(Token(TokenType::ASSIGN, line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_CURLY_OPEN, line_no, file_name_));
  ground_truth.push_back(Token::FromID("End_", line_no, file_name_));
  ground_truth.push_back(Token::FromID("ty", line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_ROUND_OPEN, line_no, file_name_));
  ground_truth.push_back(Token::FromID("length", line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_ROUND_CLOSE, line_no, file_name_));
  ground_truth.push_back(Token::FromID("r", line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_ROUND_OPEN, line_no, file_name_));
  ground_truth.push_back(Token(0.0, line_no, file_name_));
  ground_truth.push_back(Token(TokenType::COMMA, line_no, file_name_));
  ground_truth.push_back(Token(10.0, line_no, file_name_));
  ground_truth.push_back(Token(TokenType::COMMA, line_no, file_name_));
  ground_truth.push_back(Token(10.0, line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_ROUND_CLOSE, line_no, file_name_));
  ground_truth.push_back(Token::FromID("P", line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_ROUND_OPEN, line_no, file_name_));
  ground_truth.push_back(Token::FromID("recursion_count", line_no, file_name_));
  ground_truth.push_back(Token(TokenType::OP_MINUS, line_no, file_name_));
  ground_truth.push_back(Token(1, line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_ROUND_CLOSE, line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_CURLY_CLOSE, line_no, file_name_));
  ground_truth.push_back(Token(TokenType::SEMICOLON, line_no, file_name_));

  ++line_no;
  ground_truth.push_back(Token(TokenType::RULE, line_no, file_name_));
  ground_truth.push_back(Token::FromID("P", line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_ROUND_OPEN, line_no, file_name_));
  ground_truth.push_back(Token::FromID("recursion_count", line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_ROUND_CLOSE, line_no, file_name_));
  ground_truth.push_back(Token(TokenType::COLON, line_no, file_name_));
  ground_truth.push_back(Token(9, line_no, file_name_));
  ground_truth.push_back(Token(TokenType::DOUBLE_COLON, line_no, file_name_));
  ground_truth.push_back(Token::FromID("recursion_count", line_no, file_name_));
  ground_truth.push_back(Token(TokenType::OP_EQUAL, line_no, file_name_));
  ground_truth.push_back(Token(0, line_no, file_name_));
  ground_truth.push_back(Token(TokenType::ASSIGN, line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_CURLY_OPEN, line_no, file_name_));
  ground_truth.push_back(Token::FromID("color", line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_ROUND_OPEN, line_no, file_name_));
  ground_truth.push_back(Token(0.75, line_no, file_name_));
  ground_truth.push_back(Token(TokenType::COMMA, line_no, file_name_));
  ground_truth.push_back(Token(0.5, line_no, file_name_));
  ground_truth.push_back(Token(TokenType::COMMA, line_no, file_name_));
  ground_truth.push_back(Token(0.25, line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_ROUND_CLOSE, line_no, file_name_));
  ground_truth.push_back(Token(TokenType::CARET, line_no, file_name_));
  ground_truth.push_back(Token::FromID("rest_of_rule", line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_CURLY_CLOSE, line_no, file_name_));
  ground_truth.push_back(Token(TokenType::SEMICOLON, line_no, file_name_));

  ++line_no;
  ground_truth.push_back(Token(TokenType::RULE, line_no, file_name_));
  ground_truth.push_back(Token::FromID("P", line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_ROUND_OPEN, line_no, file_name_));
  ground_truth.push_back(Token::FromID("recursion_count", line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_ROUND_CLOSE, line_no, file_name_));
  ground_truth.push_back(Token(TokenType::COLON, line_no, file_name_));
  ground_truth.push_back(Token(1, line_no, file_name_));
  ground_truth.push_back(Token(TokenType::DOUBLE_COLON, line_no, file_name_));
  ground_truth.push_back(Token::FromID("recursion_count", line_no, file_name_));
  ground_truth.push_back(Token(TokenType::OP_EQUAL, line_no, file_name_));
  ground_truth.push_back(Token(0, line_no, file_name_));
  ground_truth.push_back(Token(TokenType::ASSIGN, line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_CURLY_OPEN, line_no, file_name_));
  ground_truth.push_back(Token::FromID("color", line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_ROUND_OPEN, line_no, file_name_));
  ground_truth.push_back(Token(0.25, line_no, file_name_));
  ground_truth.push_back(Token(TokenType::COMMA, line_no, file_name_));
  ground_truth.push_back(Token(0.5, line_no, file_name_));
  ground_truth.push_back(Token(TokenType::COMMA, line_no, file_name_));
  ground_truth.push_back(Token(0.75, line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_ROUND_CLOSE, line_no, file_name_));
  ground_truth.push_back(Token(TokenType::CARET, line_no, file_name_));
  ground_truth.push_back(Token::FromID("rest_of_rule", line_no, file_name_));
  ground_truth.push_back(
      Token(TokenType::BRACKET_CURLY_CLOSE, line_no, file_name_));
  ground_truth.push_back(Token(TokenType::SEMICOLON, line_no, file_name_));

  ++line_no;
  ground_truth.push_back(Token(TokenType::END_OF_FILE, line_no, file_name_));

  WriteFile(str);
  Lexer lexer;
  lexer.Init(file_name_);
  TokenVec tokens;
  EXPECT_FALSE(lexer.Tokenize(&tokens));
  EXPECT_EQ(ground_truth.size(), tokens.size());
  for (unsigned i = 0; i < std::min(ground_truth.size(), tokens.size()); ++i) {
    EXPECT_EQ(ground_truth[i], tokens[i]) << "Token " << i << " is wrong!";
  }
}
