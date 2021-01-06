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
#include <unordered_map>
#include <vector>

namespace shapeml {

struct ShapeOp;
typedef std::vector<ShapeOp> ShapeOpString;

// Values
enum class ValueType : char {
  BOOL,
  INT,
  FLOAT,
  STRING,
  SHAPE_OP_STRING,
};

typedef std::vector<ValueType> ValueTypeVec;

const char* ValueType2Str(ValueType type);

struct Value {
  explicit Value(bool val);
  explicit Value(int val);
  explicit Value(double val);
  explicit Value(const std::string& val);
  explicit Value(const char* val) : Value(std::string(val)) {}
  explicit Value(const ShapeOpString& val);
  ~Value();

  Value(const Value& val);
  Value& operator=(const Value& val);

  // Used only for the unit tests.
  bool operator==(const Value& val) const;

  // Tries to change the type to 't'. Returns true if conversion is not legal.
  bool ChangeType(ValueType t);

  ValueType type;
  union {
    bool b;
    int32_t i;
    double f;
    std::string* s;
    ShapeOpString* ops;
  };

 private:
  void DeepCopy(const Value& val);
};

std::ostream& operator<<(std::ostream& stream, const Value& value);

typedef std::unordered_map<std::string, Value> ValueDict;
typedef std::vector<Value> ValueVec;

std::ostream& operator<<(std::ostream& stream, const ValueVec& values);

}  // namespace shapeml
