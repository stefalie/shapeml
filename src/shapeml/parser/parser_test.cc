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

#include "shapeml/parser/parser.h"

#include <gtest/gtest.h>

#include <fstream>
#include <iostream>

#include "shapeml/grammar.h"
#include "shapeml/parser/lexer.h"

class ParserTest : public testing::Test {
 protected:
  virtual void SetUp() {
    ofs_.open(file_name_);
    if (!ofs_.good()) {
      std::cerr << "ERROR: Couldn't open file " << file_name_ << std::endl;
    }

    old_cerr_stream_buf_ = std::cerr.rdbuf(nullptr);
  }

  void WriteFile(const std::string& content) {
    ofs_ << content;
    ofs_.close();
  }

  virtual void TearDown() {
    std::cerr.rdbuf(old_cerr_stream_buf_);

    if (std::remove(file_name_.c_str()) != 0) {
      std::cerr << "ERROR: Couldn't remove file " << file_name_ << std::endl;
    }
  }

  std::string file_name_ = "test_parser_tmp";
  std::ofstream ofs_;

  std::streambuf* old_cerr_stream_buf_;
};

TEST_F(ParserTest, EmptyFile) {
  WriteFile("");

  shapeml::Grammar grammar;
  shapeml::parser::Parser parser;
  EXPECT_FALSE(parser.Parse(file_name_, &grammar));
}

TEST_F(ParserTest, SimpleExample) {
  const std::string str(R"delim(
param length = 4;
const rest_of_rule = { s(0.0, 0.0, 1.0 * sz) i("assets/teapot.obj") Teapot_ };

rule P(recursion_count) :: (recursion_count > 0) = { End_ ty(length) r(0.0, 10.0, 10.0) P(recursion_count - 1) };
rule P(recursion_count) : 9 :: recursion_count == 0  = { color(0.75, 0.5, 0.25) ^ rest_of_rule };
rule P(recursion_count) : 1 :: recursion_count == 0  = { color(0.25, 0.5, 0.75) ^ rest_of_rule };

/*
 * some comment
 */

const empty_shape_op_string = {};

func simple_func(a) = a + 1;
  )delim");
  WriteFile(str);

  shapeml::Grammar grammar;
  shapeml::parser::Parser parser;
  EXPECT_FALSE(parser.Parse(file_name_, &grammar));
}

TEST_F(ParserTest, InvalidParam) {
  const std::string str(R"delim(
param p0 = 10 + 1;  // ERROR
  )delim");
  WriteFile(str);

  shapeml::Grammar grammar;
  shapeml::parser::Parser parser;
  EXPECT_TRUE(parser.Parse(file_name_, &grammar));
}
