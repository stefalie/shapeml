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

#include "shapeml/value.h"

#include <cassert>
#include <cmath>
#include <iomanip>
#include <sstream>

#include "shapeml/expressions.h"

namespace shapeml {

const char* ValueType2Str(ValueType type) {
  switch (type) {
    case ValueType::FLOAT:
      return "float";
    case ValueType::INT:
      return "int";
    case ValueType::BOOL:
      return "bool";
    case ValueType::STRING:
      return "string";
    case ValueType::SHAPE_OP_STRING:
      return "shape operation string";
  }

  return "";  // Avoid VS warning.
}

Value::Value(bool val) : type(ValueType::BOOL), b(val) {}

Value::Value(int val) : type(ValueType::INT), i(val) {}

Value::Value(double val) : type(ValueType::FLOAT), f(val) {}

Value::Value(const std::string& val)
    : type(ValueType::STRING), s(new std::string(val)) {}

Value::Value(const ShapeOpString& val)
    : type(ValueType::SHAPE_OP_STRING), ops(new ShapeOpString(val)) {}

Value::~Value() {
  if (type == ValueType::STRING) {
    delete s;
  }
  if (type == ValueType::SHAPE_OP_STRING) {
    delete ops;
  }
}

Value::Value(const Value& val) { DeepCopy(val); }

Value& Value::operator=(const Value& val) {
  std::string* tmp_s = nullptr;
  if (type == ValueType::STRING) {
    tmp_s = s;
  }
  ShapeOpString* tmp_ops = nullptr;
  if (type == ValueType::SHAPE_OP_STRING) {
    tmp_ops = ops;
  }
  DeepCopy(val);
  if (tmp_s) {
    delete tmp_s;
  }
  if (tmp_ops) {
    delete tmp_ops;
  }
  return *this;
}

bool Value::operator==(const Value& val) const {
  if (type != val.type) {
    return false;
  }

  switch (type) {
    case ValueType::BOOL:
      return b == val.b;
    case ValueType::INT:
      return i == val.i;
    case ValueType::FLOAT:
      // TODO(stefalie): Epsilon test is sketchy, should probably be the exact
      // epsilon to the next smaller/larger (std::nextafterf) of the smaller
      // of both values?
      // We have the same problem in token.cc for token comparison.
      return fabs(f - val.f) < 0.00000001;
    case ValueType::STRING:
      return *s == *val.s;
    case ValueType::SHAPE_OP_STRING:
      // TODO(stefalie): This is missing. But it isn't used anywhere.
      assert(false);
      return false;
  }

  return true;
}

template <class T>
std::string ToString(T t) {
  std::ostringstream oss;
  oss << t;
  return oss.str();
}

bool Value::ChangeType(ValueType t) {
  bool error = false;

  switch (t) {
    case ValueType::FLOAT:
      switch (type) {
        case ValueType::FLOAT:
          break;
        case ValueType::INT:
          f = static_cast<double>(i);
          break;
        case ValueType::BOOL:
          f = static_cast<double>(b);
          break;
        case ValueType::STRING:
        case ValueType::SHAPE_OP_STRING:
          error = true;
          break;
      }
      break;
    case ValueType::INT:
      switch (type) {
        case ValueType::FLOAT:
          i = static_cast<int>(f);
          break;
        case ValueType::INT:
          break;
        case ValueType::BOOL:
          i = static_cast<int>(b);
          break;
        case ValueType::STRING:
        case ValueType::SHAPE_OP_STRING:
          error = true;
          break;
      }
      break;
    case ValueType::BOOL:
      switch (type) {
        case ValueType::FLOAT:
          b = static_cast<bool>(f);
          break;
        case ValueType::INT:
          b = static_cast<bool>(i);
          break;
        case ValueType::BOOL:
          break;
        case ValueType::STRING:
        case ValueType::SHAPE_OP_STRING:
          error = true;
          break;
      }
      break;
    case ValueType::STRING:
      switch (type) {
        case ValueType::FLOAT:
          s = new std::string(ToString(f));
          break;
        case ValueType::INT:
          s = new std::string(ToString(i));
          break;
        case ValueType::BOOL:
          s = new std::string(ToString(b));
          break;
        case ValueType::STRING:
          break;
        case ValueType::SHAPE_OP_STRING:
          error = true;
          break;
      }
      break;
    case ValueType::SHAPE_OP_STRING:
      switch (type) {
        case ValueType::FLOAT:
        case ValueType::INT:
        case ValueType::BOOL:
        case ValueType::STRING:
          error = true;
          break;
        case ValueType::SHAPE_OP_STRING:
          break;
      }
      break;
  }

  if (error) {
    return true;
  }

  type = t;
  return false;
}

void Value::DeepCopy(const Value& val) {
  switch (val.type) {
    case ValueType::FLOAT:
      f = val.f;
      break;
    case ValueType::INT:
      i = val.i;
      break;
    case ValueType::BOOL:
      b = val.b;
      break;
    case ValueType::STRING:
      s = new std::string(*val.s);
      break;
    case ValueType::SHAPE_OP_STRING:
      ops = new ShapeOpString(*val.ops);
      break;
  }
  type = val.type;
}

std::ostream& operator<<(std::ostream& stream, const Value& value) {
  switch (value.type) {
    case ValueType::FLOAT: {
      std::ostringstream oss;
      oss << std::showpoint << std::fixed << std::setprecision(2) << value.f;
      stream << oss.str();
      break;
    }
    case ValueType::INT:
      stream << value.i;
      break;
    case ValueType::BOOL:
      if (value.b) {
        stream << "true";
      } else {
        stream << "false";
      }
      break;
    case ValueType::STRING:
      stream << '"' << *value.s << '"';
      break;
    case ValueType::SHAPE_OP_STRING:
      PrintShapeOpStringToStream(*value.ops, false, &stream);
      break;
  }
  return stream;
}

std::ostream& operator<<(std::ostream& stream, const ValueVec& values) {
  if (!values.empty()) {
    stream << '(' << values[0];
    for (size_t i = 1; i < values.size(); ++i) {
      stream << ", " << values[i];
    }
    stream << ')';
  }
  return stream;
}

}  // namespace shapeml
