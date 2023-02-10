////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Basics/fpconv.h"
#include "ProgramOptions/UnitsHelper.h"

#include <velocypack/Builder.h>

#include <atomic>
#include <cstdint>
#include <limits>
#include <numeric>
#include <string>
#include <type_traits>
#include <unordered_set>
#include <vector>

namespace arangodb::options {

// helper function to strip-non-numeric data from a string
std::string removeWhitespaceAndComments(std::string const& value);

// convert a string into a number, base version for signed and unsigned integer
// types
template<typename T>
inline T toNumber(std::string value, T base = 1) {
  // replace leading spaces, replace trailing spaces & comments
  value = removeWhitespaceAndComments(value);

  if constexpr (std::is_signed_v<T>) {
    return UnitsHelper::parseNumberWithUnit<T, int64_t>(value, base);
  } else {
    return UnitsHelper::parseNumberWithUnit<T, uint64_t>(value, base);
  }
}

// convert a string into a number, version for double values
template<>
inline double toNumber<double>(std::string value, double /*base*/) {
  // replace leading spaces, replace trailing spaces & comments
  return std::stod(removeWhitespaceAndComments(value));
}

// convert a string into another type, specialized version for numbers
template<typename T>
typename std::enable_if<std::is_arithmetic<T>::value, T>::type fromString(
    std::string const& value) {
  return toNumber<T>(value, static_cast<T>(1));
}

// convert a string into another type, specialized version for string -> string
template<typename T>
typename std::enable_if<std::is_same<T, std::string>::value, T>::type
fromString(std::string const& value) {
  return value;
}

// stringify a value, base version for any type
template<typename T>
inline std::string stringifyValue(T const& value) {
  return std::to_string(value);
}

// stringify a double value, specialized version
template<>
inline std::string stringifyValue<double>(double const& value) {
  char buf[32];
  int length = fpconv_dtoa(value, &buf[0]);
  return std::string(&buf[0], static_cast<std::size_t>(length));
}

// stringify a boolean value, specialized version
template<>
inline std::string stringifyValue<bool>(bool const& value) {
  return value ? "true" : "false";
}

// stringify a string value, specialized version
template<>
inline std::string stringifyValue<std::string>(std::string const& value) {
  return "\"" + value + "\"";
}

template<typename T>
inline std::string stringifyValues(T const& values) {
  std::string value;
  for (std::size_t i = 0; i < values.size(); ++i) {
    if (i > 0) {
      value.append(", ");
    }
    value.append(stringifyValue(values.at(i)));
  }
  return value;
}

template<typename T>
inline std::string joinValues(T const& values) {
  std::string result;
  std::size_t i = 0;
  for (auto const& it : values) {
    if (i++ > 0) {
      result.append(", ");
    }
    result.append(it);
  }
  return result;
}

// abstract base parameter type struct
struct Parameter {
  Parameter() = default;
  virtual ~Parameter() = default;

  virtual void flushValue() {}

  virtual bool requiresValue() const { return true; }
  virtual std::string name() const = 0;
  virtual std::string valueString() const = 0;
  virtual std::string set(std::string const&) = 0;
  virtual std::string description() const { return std::string(); }

  virtual std::string typeDescription() const {
    return std::string("<") + name() + std::string(">");
  }

  virtual void toVelocyPack(VPackBuilder&, bool detailed) const = 0;
};

// specialized type for boolean values
template<typename ValueType>
struct BooleanParameterBase : public Parameter {
  explicit BooleanParameterBase(ValueType* ptr, bool required = false)
      : ptr(ptr), required(required) {}

  bool requiresValue() const override { return required; }
  std::string name() const override { return "boolean"; }
  std::string valueString() const override { return stringifyValue(*ptr); }

  std::string set(std::string const& value) override {
    if (!required && value.empty()) {
      // the empty value "" is considered "true", e.g. "--force" will mean
      // "--force true"
      *ptr = true;
      return "";
    }
    if (value == "true" || value == "false" || value == "on" ||
        value == "off" || value == "1" || value == "0" || value == "yes" ||
        value == "no") {
      *ptr =
          (value == "true" || value == "on" || value == "1" || value == "yes");
      return "";
    }
    return "invalid value for type " + this->name() +
           ". expecting 'true' or 'false'";
  }

  std::string typeDescription() const override {
    return Parameter::typeDescription();
  }

  void toVelocyPack(VPackBuilder& builder, bool detailed) const override {
    builder.add(VPackValue(*ptr));
    if (detailed) {
      builder.add("required", VPackValue(required));
    }
  }

  ValueType* ptr;
  bool required;
};

// specialized type for normal boolean values
using BooleanParameter = BooleanParameterBase<bool>;

// specialized type for atomic boolean values
using AtomicBooleanParameter = BooleanParameterBase<std::atomic<bool>>;

// specialized type for numeric values
// this templated type needs a concrete number type
template<typename T>
struct NumericParameter : public Parameter {
  typedef T ValueType;

  explicit NumericParameter(
      ValueType* ptr, ValueType base = 1,
      ValueType minValue = std::numeric_limits<ValueType>::min(),
      ValueType maxValue = std::numeric_limits<ValueType>::max(),
      bool minInclusive = true, bool maxInclusive = true)
      : ptr(ptr),
        base(base),
        minValue(minValue),
        maxValue(maxValue),
        minInclusive(minInclusive),
        maxInclusive(maxInclusive) {}

  std::string valueString() const override { return stringifyValue(*ptr); }

  std::string set(std::string const& value) override {
    try {
      ValueType v = toNumber<ValueType>(value, base);
      if (((minInclusive && v >= minValue) ||
           (!minInclusive && v > minValue)) &&
          ((maxInclusive && v <= maxValue) ||
           (!maxInclusive && v < maxValue))) {
        *ptr = v;
        return "";
      }
      return "number '" + value + "' is outside of allowed range " +
             (minInclusive ? "[" : "(") + std::to_string(this->minValue) +
             " - " + std::to_string(this->maxValue) +
             (maxInclusive ? "]" : ")") + " for type " + this->name();
    } catch (...) {
      return "invalid numeric value '" + value + "' for type " + this->name();
    }
  }

  void toVelocyPack(VPackBuilder& builder, bool detailed) const override {
    builder.add(VPackValue(*ptr));
    if (detailed) {
      builder.add("base", VPackValue(base));
      builder.add("minValue", VPackValue(minValue));
      builder.add("maxValue", VPackValue(maxValue));
      builder.add("minInclusive", VPackValue(minInclusive));
      builder.add("maxInclusive", VPackValue(maxInclusive));
    }
  }

  std::string name() const override {
    if constexpr (std::is_same_v<ValueType, std::int16_t>) {
      return "int16";
    } else if constexpr (std::is_same_v<ValueType, std::uint16_t>) {
      return "uint16";
    } else if constexpr (std::is_same_v<ValueType, std::int32_t>) {
      return "int32";
    } else if constexpr (std::is_same_v<ValueType, std::uint32_t>) {
      return "uint32";
    } else if constexpr (std::is_same_v<ValueType, std::int64_t>) {
      return "int64";
    } else if constexpr (std::is_same_v<ValueType, std::uint64_t>) {
      return "uint64";
    } else if constexpr (std::is_same_v<ValueType, std::size_t>) {
      return "size";
    } else if constexpr (std::is_same_v<ValueType, double>) {
      return "double";
    } else {
      static_assert("unsupported ValueType");
    }
  }

  ValueType* ptr;
  ValueType base = 1;
  ValueType minValue = std::numeric_limits<ValueType>::min();
  ValueType maxValue = std::numeric_limits<ValueType>::max();
  bool minInclusive = true;
  bool maxInclusive = true;
};

// concrete int16 number value type
using Int16Parameter = NumericParameter<std::int16_t>;

// concrete uint16 number value type
using UInt16Parameter = NumericParameter<std::uint16_t>;

// concrete int32 number value type
using Int32Parameter = NumericParameter<std::int32_t>;

// concrete uint32 number value type
using UInt32Parameter = NumericParameter<std::uint32_t>;

// concrete int64 number value type
using Int64Parameter = NumericParameter<std::int64_t>;

// concrete uint64 number value type
using UInt64Parameter = NumericParameter<std::uint64_t>;

// concrete size_t number value type
using SizeTParameter = NumericParameter<std::size_t>;

// concrete double number value type
using DoubleParameter = NumericParameter<double>;

// string value type
struct StringParameter : public Parameter {
  typedef std::string ValueType;

  explicit StringParameter(ValueType* ptr) : ptr(ptr) {}

  std::string name() const override { return "string"; }
  std::string valueString() const override { return stringifyValue(*ptr); }

  std::string set(std::string const& value) override {
    *ptr = value;
    return "";
  }

  void toVelocyPack(VPackBuilder& builder, bool /*detailed*/) const override {
    builder.add(VPackValue(*ptr));
  }

  ValueType* ptr;
};

// specialized type for discrete values (defined in the unordered_set)
// this templated type needs a concrete value type
template<typename T>
struct DiscreteValuesParameter : public T {
  DiscreteValuesParameter(
      typename T::ValueType* ptr,
      std::unordered_set<typename T::ValueType> const& allowed)
      : T(ptr), allowed(allowed) {
    if (allowed.find(*ptr) == allowed.end()) {
      // default value is not in list of allowed values
      std::string msg("invalid default value for DiscreteValues parameter: '");
      msg.append(stringifyValue(*ptr));
      msg.append("'. ");
      msg.append(description());
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, msg);
    }
  }

  std::string set(std::string const& value) override {
    auto it = allowed.find(fromString<typename T::ValueType>(value));

    if (it == allowed.end()) {
      std::string msg("invalid value '");
      msg.append(value);
      msg.append("'. ");
      msg.append(description());
      return msg;
    }

    return T::set(value);
  }

  // cppcheck-suppress virtualCallInConstructor ; bogus warning
  std::string description() const override {
    std::vector<std::string> values;
    for (auto const& it : allowed) {
      values.emplace_back(stringifyValue(it));
    }
    std::sort(values.begin(), values.end());
    return std::string("Possible values: ") + joinValues(values);
  }

  std::unordered_set<typename T::ValueType> allowed;
};

// specialized type for vectors of values
// this templated type needs a concrete value type
template<typename T>
struct VectorParameter : public Parameter {
  explicit VectorParameter(std::vector<typename T::ValueType>* ptr)
      : ptr(ptr) {}
  std::string name() const override {
    typename T::ValueType dummy;
    T param(&dummy);
    return std::string(param.name()) + "...";
  }

  std::string valueString() const override { return stringifyValues(*ptr); }

  std::string set(std::string const& value) override {
    typename T::ValueType dummy;
    T param(&dummy);
    std::string result = param.set(value);
    if (result.empty()) {
      ptr->push_back(*(param.ptr));
    }
    return result;
  }

  void flushValue() override { ptr->clear(); }

  void toVelocyPack(VPackBuilder& builder, bool /*detailed*/) const override {
    builder.openArray();
    for (std::size_t i = 0; i < ptr->size(); ++i) {
      builder.add(VPackValue(ptr->at(i)));
    }
    builder.close();
  }

  std::vector<typename T::ValueType>* ptr;
};

// specialized type for a vector of discrete values (defined in the
// unordered_set) this templated type needs a concrete value type
template<typename T>
struct DiscreteValuesVectorParameter : public Parameter {
  explicit DiscreteValuesVectorParameter(
      std::vector<typename T::ValueType>* ptr,
      std::unordered_set<typename T::ValueType> const& allowed)
      : ptr(ptr), allowed(allowed) {
    for (std::size_t i = 0; i < ptr->size(); ++i) {
      if (allowed.find(ptr->at(i)) == allowed.end()) {
        // default value is not in list of allowed values
        std::string msg(
            "invalid default value for DiscreteValues parameter: '");
        msg.append(stringifyValue(ptr->at(i)));
        msg.append("'. ");
        msg.append(description());
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, msg);
      }
    }
  }

  std::string name() const override {
    typename T::ValueType dummy;
    T param(&dummy);
    return std::string(param.name()) + "...";
  }

  void flushValue() override { ptr->clear(); }

  std::string valueString() const override {
    std::string value;
    for (std::size_t i = 0; i < ptr->size(); ++i) {
      if (i > 0) {
        value.append(", ");
      }
      value.append(stringifyValue(ptr->at(i)));
    }
    return value;
  }

  std::string set(std::string const& value) override {
    auto it = allowed.find(fromString<typename T::ValueType>(value));

    if (it == allowed.end()) {
      std::string msg("invalid value '");
      msg.append(value);
      msg.append("'. ");
      msg.append(description());
      return msg;
    }

    typename T::ValueType dummy;
    T param(&dummy);
    std::string result = param.set(value);

    if (result.empty()) {
      ptr->push_back(*(param.ptr));
    }
    return result;
  }

  void toVelocyPack(VPackBuilder& builder, bool /*detailed*/) const override {
    builder.openArray();
    for (std::size_t i = 0; i < ptr->size(); ++i) {
      builder.add(VPackValue(ptr->at(i)));
    }
    builder.close();
  }

  std::string description() const override {
    std::vector<std::string> values;
    values.reserve(allowed.size());
    for (auto const& it : allowed) {
      values.emplace_back(stringifyValue(it));
    }
    std::sort(values.begin(), values.end());
    return std::string("Possible values: ") + joinValues(values);
  }

  std::vector<typename T::ValueType>* ptr;
  std::unordered_set<typename T::ValueType> allowed;
};

// a type that's useful for obsolete parameters that do nothing
struct ObsoleteParameter : public Parameter {
  explicit ObsoleteParameter(bool requiresValue) : required(requiresValue) {}
  bool requiresValue() const override { return required; }
  std::string name() const override { return "obsolete"; }
  std::string valueString() const override { return "-"; }
  std::string set(std::string const&) override { return ""; }
  void toVelocyPack(VPackBuilder& builder, bool /*detailed*/) const override {
    builder.add(VPackValue(VPackValueType::Null));
  }

  bool required;
};
}  // namespace arangodb::options
