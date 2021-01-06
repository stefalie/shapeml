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

#include "shapeml/interpreter.h"

#include <gtest/gtest.h>

#include <fstream>
#include <iostream>

#include "shapeml/grammar.h"
#include "shapeml/parser/parser.h"

using shapeml::Grammar;
using shapeml::Interpreter;
using shapeml::Shape;
using shapeml::parser::Parser;

class InterpreterTest : public testing::Test {
 protected:
  virtual void SetUp() {
    ofs_.open(file_name_);
    if (!ofs_.good()) {
      std::cerr << "ERROR: Couldn't open file " << file_name_ << std::endl;
    }

    old_cout_stream_buf_ = std::cout.rdbuf(&cout_string_buf_);
    old_cerr_stream_buf_ = std::cerr.rdbuf(&cerr_string_buf_);
  }

  void WriteFile(const std::string& content) {
    ofs_ << content;
    ofs_.close();
  }

  virtual void TearDown() {
    std::cout.rdbuf(old_cout_stream_buf_);
    std::cerr.rdbuf(old_cerr_stream_buf_);

    if (std::remove(file_name_.c_str()) != 0) {
      std::cerr << "ERROR: Couldn't remove file " << file_name_ << std::endl;
    }
  }

  std::string file_name_ = "test_interpreter_tmp";
  std::ofstream ofs_;

  std::streambuf* old_cout_stream_buf_;
  std::stringbuf cout_string_buf_;

  std::streambuf* old_cerr_stream_buf_;
  std::stringbuf cerr_string_buf_;
};

TEST_F(InterpreterTest, SuccessfulExecution) {
  const std::string str_grammar(R"delim(
param p0 = 10;
const c0 = 20 * 4 + p0;

func f0(a) = 1 + a + sign(2.22) + p0;

const c1 = { printLn("Hello World!" + (5 + 10) + f0(c0 + 1)) Asdf_ };

rule Axiom = { Terminal_0_ Rule_0 };

rule Rule_0 = { Terminal_1_ ^c1 ^c1 };
  )delim");
  WriteFile(str_grammar);

  const std::string str_output(R"delim(Hello World!15103
Hello World!15103
)delim");

  Grammar grammar;
  Parser parser;
  ASSERT_FALSE(parser.Parse(file_name_, &grammar));

  Shape* root =
      Interpreter::Get().Init(&grammar, "Axiom", 1234, nullptr, nullptr);
  ASSERT_NO_THROW(Interpreter::Get().Derive(root, 100));

  EXPECT_EQ(str_output, cout_string_buf_.str());
}

TEST_F(InterpreterTest, InfiniteLoop) {
  const std::string str_grammar(R"delim(
rule Axiom = { printLn(infini_func(0)) };

func infini_func(p) = infini_func(p) + 1;  // ERROR
  )delim");
  WriteFile(str_grammar);

  const std::string str_error_msg =
      "Function 'infini_func' reached the max recursion depth (20).\n";

  Grammar grammar;
  Parser parser;
  ASSERT_FALSE(parser.Parse(file_name_, &grammar));

  Shape* root =
      Interpreter::Get().Init(&grammar, "Axiom", 1234, nullptr, nullptr);
  ASSERT_NO_THROW(Interpreter::Get().Derive(root, 100));

  const size_t offset =
      cerr_string_buf_.str().length() - str_error_msg.length();
  EXPECT_EQ(str_error_msg, cerr_string_buf_.str().substr(offset));
}
