////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGODB_PROGRAM_OPTIONS_PARAMETERS_H
#define ARANGODB_PROGRAM_OPTIONS_PARAMETERS_H 1

#include "Basics/Common.h"
#include "Basics/Exceptions.h"
#include "Basics/fpconv.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include <atomic>
#include <limits>
#include <numeric>
#include <type_traits>

namespace arangodb {
namespace options {

// convert a string into a number, base version for signed integer types
template <typename T>
inline typename std::enable_if<std::is_signed<T>::value, T>::type toNumber(
    std::string const& value) {
  auto v = static_cast<T>(std::stoll(value));
  if (v < (std::numeric_limits<T>::min)() || v > (std::numeric_limits<T>::max)()) {
    throw std::out_of_range(value);
  }
  return static_cast<T>(v);
}

// convert a string into a number, base version for unsigned integer types
template <typename T>
inline typename std::enable_if<std::is_unsigned<T>::value, T>::type toNumber(
    std::string const& value) {
  auto v = static_cast<T>(std::stoull(value));
  if (v < (std::numeric_limits<T>::min)() || v > (std::numeric_limits<T>::max)()) {
    throw std::out_of_range(value);
  }
  return static_cast<T>(v);
}

// convert a string into a number, version for double values
template <>
inline double toNumber<double>(std::string const& value) {
  return std::stod(value);
}

// convert a string into another type, specialized version for numbers
template <typename T>
typename std::enable_if<std::is_arithmetic<T>::value, T>::type fromString(
    std::string const& value) {
  return toNumber<T>(value);
}

// convert a string into another type, specialized version for string -> string
template <typename T>
typename std::enable_if<std::is_same<T, std::string>::value, T>::type fromString(
    std::string const& value) {
  return value;
}

// stringify a value, base version for any type
template <typename T>
inline std::string stringifyValue(T const& value) {
  return std::to_string(value);
}

// stringify a value, base version for any type
template <>
inline std::string stringifyValue<double>(double const& value) {
  char buf[32];
  int length = fpconv_dtoa(value, &buf[0]);
  return std::string(&buf[0], static_cast<size_t>(length));
}

// stringify a boolean value, specialized version
template <>
inline std::string stringifyValue<bool>(bool const& value) {
  return value ? "true" : "false";
}

// stringify a string value, specialized version
template <>
inline std::string stringifyValue<std::string>(std::string const& value) {
  return "\"" + value + "\"";
}

// abstract base parameter type struct
struct Parameter {
  Parameter() = default;
  virtual ~Parameter() {}

  virtual bool requiresValue() const { return true; }
  virtual std::string name() const = 0;
  virtual std::string valueString() const = 0;
  virtual std::string set(std::string const&) = 0;

  virtual std::string typeDescription() const {
    return std::string("<") + name() + std::string(">");
  }

  virtual void toVPack(VPackBuilder&) const = 0;
};

// specialized type for boolean values
struct BooleanParameter : public Parameter {
  typedef bool ValueType;

  explicit BooleanParameter(ValueType* ptr, bool required = false)
      : ptr(ptr), required(required) {}

  bool requiresValue() const override { return required; }
  std::string name() const override { return "boolean"; }
  std::string valueString() const override { return stringifyValue(*ptr); }

  std::string set(std::string const& value) override {
    if (!required && value.empty()) {
      // the empty value "" is considered "true", e.g. "--force" will mean "--force true" 
      *ptr = true;
      return "";
    }
    if (value == "true" || value == "false" || value == "on" ||
        value == "off" || value == "1" || value == "0") {
      *ptr = (value == "true" || value == "on" || value == "1");
      return "";
    }
    return "invalid value. expecting 'true' or 'false'";
  }

  std::string typeDescription() const override {
    return Parameter::typeDescription();
  }

  void toVPack(VPackBuilder& builder) const override {
    builder.add(VPackValue(*ptr));
  }

  ValueType* ptr;
  bool required;
};

// specialized type for atomic boolean values
struct AtomicBooleanParameter : public Parameter {
  typedef std::atomic<bool> ValueType;

  explicit AtomicBooleanParameter(ValueType* ptr, bool required = false)
      : ptr(ptr), required(required) {}

  bool requiresValue() const override { return required; }
  std::string name() const override { return "boolean"; }
  std::string valueString() const override { return stringifyValue(ptr->load()); }

  std::string set(std::string const& value) override {
    if (!required && value.empty()) {
      // the empty value "" is considered "true", e.g. "--force" will mean "--force true" 
      *ptr = true;
      return "";
    }
    if (value == "true" || value == "false" || value == "on" ||
        value == "off" || value == "1" || value == "0") {
      ptr->store(value == "true" || value == "on" || value == "1");
      return "";
    }
    return "invalid value. expecting 'true' or 'false'";
  }

  std::string typeDescription() const override {
    return Parameter::typeDescription();
  }

  void toVPack(VPackBuilder& builder) const override {
    builder.add(VPackValue(ptr->load()));
  }

  ValueType* ptr;
  bool required;
};

// specialized type for numeric values
// this templated type needs a concrete number type
template <typename T>
struct NumericParameter : public Parameter {
  typedef T ValueType;

  explicit NumericParameter(ValueType* ptr) : ptr(ptr) {}

  std::string valueString() const override { return stringifyValue(*ptr); }

  std::string set(std::string const& value) override {
    try {
      ValueType v = toNumber<ValueType>(value);
      if (v >= (std::numeric_limits<T>::min)() &&
          v <= (std::numeric_limits<T>::max)()) {
        *ptr = v;
        return "";
      }
    } catch (...) {
      return "invalid numeric value";
    }
    return "number out of range";
  }

  void toVPack(VPackBuilder& builder) const override {
    builder.add(VPackValue(*ptr));
  }

  ValueType* ptr;
};

// concrete int16 number value type
struct Int16Parameter : public NumericParameter<int16_t> {
  typedef int16_t ValueType;

  explicit Int16Parameter(int16_t* ptr) : NumericParameter<int16_t>(ptr) {}

  std::string name() const override { return "int16"; }
};

// concrete uint16 number value type
struct UInt16Parameter : public NumericParameter<uint16_t> {
  typedef uint16_t ValueType;

  explicit UInt16Parameter(ValueType* ptr) : NumericParameter<uint16_t>(ptr) {}

  std::string name() const override { return "uint16"; }
};

// concrete int32 number value type
struct Int32Parameter : public NumericParameter<int32_t> {
  typedef int32_t ValueType;

  explicit Int32Parameter(ValueType* ptr) : NumericParameter<int32_t>(ptr) {}

  std::string name() const override { return "int32"; }
};

// concrete uint32 number value type
struct UInt32Parameter : public NumericParameter<uint32_t> {
  typedef uint32_t ValueType;

  explicit UInt32Parameter(ValueType* ptr) : NumericParameter<uint32_t>(ptr) {}

  std::string name() const override { return "uint32"; }
};

// concrete int64 number value type
struct Int64Parameter : public NumericParameter<int64_t> {
  typedef int64_t ValueType;

  explicit Int64Parameter(ValueType* ptr) : NumericParameter<int64_t>(ptr) {}

  std::string name() const override { return "int64"; }
};

// concrete uint64 number value type
struct UInt64Parameter : public NumericParameter<uint64_t> {
  typedef uint64_t ValueType;

  explicit UInt64Parameter(ValueType* ptr) : NumericParameter<uint64_t>(ptr) {}

  std::string name() const override { return "uint64"; }
};

template <typename T>
struct BoundedParameter : public T {
  BoundedParameter(typename T::ValueType* ptr, typename T::ValueType minValue,
                   typename T::ValueType maxValue)
      : T(ptr), minValue(minValue), maxValue(maxValue) {}

  std::string set(std::string const& value) override {
    try {
      typename T::ValueType v = toNumber<typename T::ValueType>(value);
      if (v >= (std::numeric_limits<typename T::ValueType>::min)() &&
          v <= (std::numeric_limits<typename T::ValueType>::max)() && v >= minValue &&
          v <= maxValue) {
        *this->ptr = v;
        return "";
      }
    } catch (...) {
      return "invalid numeric value";
    }
    return "number out of allowed range (" + std::to_string(minValue) + " - " +
           std::to_string(maxValue) + ")";
  }

  typename T::ValueType minValue;
  typename T::ValueType maxValue;
};

// concrete double number value type
struct DoubleParameter : public NumericParameter<double> {
  typedef double ValueType;

  explicit DoubleParameter(ValueType* ptr) : NumericParameter<double>(ptr) {}

  std::string name() const override { return "double"; }
};

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

  void toVPack(VPackBuilder& builder) const override {
    builder.add(VPackValue(*ptr));
  }

  ValueType* ptr;
};

// specialized type for discrete values (defined in the unordered_set)
// this templated type needs a concrete value type
template <typename T>
struct DiscreteValuesParameter : public T {
  DiscreteValuesParameter(typename T::ValueType* ptr,
                   std::unordered_set<typename T::ValueType> const& allowed)
      : T(ptr), allowed(allowed) {

  if (allowed.find(*ptr) == allowed.end()) {
    // default value is not in list of allowed values
    std::string msg("invalid default value for DiscreteValues parameter: ");
    msg.append(stringifyValue(*ptr));
    msg.append(". allowed values: ");
    size_t i = 0;
    for (auto const& it : allowed) {
      if (i > 0) {
        msg.append(" or ");
      }
      msg.append(stringifyValue(it));
      ++i;
    }

    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL, msg.c_str());
  }
}

  std::string set(std::string const& value) override {
    auto it = allowed.find(fromString<typename T::ValueType>(value));

    if (it == allowed.end()) {
      return "invalid value " + value;
    }

    return T::set(value);
  }

  std::unordered_set<typename T::ValueType> allowed;
};

// specialized type for vectors of values
// this templated type needs a concrete value type
template <typename T>
struct VectorParameter : public Parameter {
  explicit VectorParameter(std::vector<typename T::ValueType>* ptr)
      : ptr(ptr) {}

  std::string name() const override {
    typename T::ValueType dummy;
    T param(&dummy);
    return std::string(param.name()) + "...";
  }

  std::string valueString() const override {
    std::string value;
    for (size_t i = 0; i < ptr->size(); ++i) {
      if (i > 0) {
        value.append(", ");
      }
      value.append(stringifyValue(ptr->at(i)));
    }
    return value;
  }

  std::string set(std::string const& value) override {
    typename T::ValueType dummy;
    T param(&dummy);
    std::string result = param.set(value);
    if (result.empty()) {
      ptr->push_back(*(param.ptr));
    }
    return result;
  }

  void toVPack(VPackBuilder& builder) const override {
    builder.openArray();
    for (size_t i = 0; i < ptr->size(); ++i) {
      builder.add(VPackValue(ptr->at(i)));
    }
    builder.close();
  }

  std::vector<typename T::ValueType>* ptr;
};

// a type that's useful for obsolete parameters that do nothing
struct ObsoleteParameter : public Parameter {
  bool requiresValue() const override { return false; }
  std::string name() const override { return "obsolete"; }
  std::string valueString() const override { return "-"; }
  std::string set(std::string const&) override { return ""; }
  void toVPack(VPackBuilder& builder) const override {
    builder.add(VPackValue(VPackValueType::Null));
  }
};
}
}

#endif
