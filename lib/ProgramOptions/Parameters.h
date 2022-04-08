////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
#include "Basics/NumberUtils.h"
#include "Basics/fpconv.h"

#include <velocypack/Builder.h>

#include <atomic>
#include <limits>
#include <numeric>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>

namespace arangodb {
namespace options {

// helper function to strip-non-numeric data from a string
std::string removeCommentsFromNumber(std::string const& value);

// convert a string into a number, base version for signed or unsigned integer
// types
template<typename T>
inline T toNumber(std::string value, T base = 1) {
  // replace leading spaces, replace trailing spaces & comments
  value = removeCommentsFromNumber(value);

  auto n = value.size();
  int64_t m = 1;
  int64_t d = 1;
  bool seen = false;
  if (n > 3) {
    std::string suffix = value.substr(n - 3);

    if (suffix == "kib" || suffix == "KiB" || suffix == "KIB") {
      m = 1024;
      value = value.substr(0, n - 3);
      seen = true;
    } else if (suffix == "mib" || suffix == "MiB" || suffix == "MIB") {
      m = 1024 * 1024;
      value = value.substr(0, n - 3);
      seen = true;
    } else if (suffix == "gib" || suffix == "GiB" || suffix == "GIB") {
      m = 1024 * 1024 * 1024;
      value = value.substr(0, n - 3);
      seen = true;
    }
  }
  if (!seen && n > 2) {
    std::string suffix = value.substr(n - 2);

    if (suffix == "kb" || suffix == "KB") {
      m = 1000;
      value = value.substr(0, n - 2);
      seen = true;
    } else if (suffix == "mb" || suffix == "MB") {
      m = 1000 * 1000;
      value = value.substr(0, n - 2);
      seen = true;
    } else if (suffix == "gb" || suffix == "GB") {
      m = 1000 * 1000 * 1000;
      value = value.substr(0, n - 2);
      seen = true;
    }
  }
  if (!seen && n > 1) {
    std::string suffix = value.substr(n - 1);

    if (suffix == "k" || suffix == "K") {
      m = 1000;
      value = value.substr(0, n - 1);
    } else if (suffix == "m" || suffix == "M") {
      m = 1000 * 1000;
      value = value.substr(0, n - 1);
    } else if (suffix == "g" || suffix == "G") {
      m = 1000 * 1000 * 1000;
      value = value.substr(0, n - 1);
    } else if (suffix == "%") {
      m = static_cast<int64_t>(base);
      d = 100;
      value = value.substr(0, n - 1);
    }
  }

  char const* p = value.data();
  char const* e = p + value.size();
  // skip leading whitespace
  while (p < e && (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r')) {
    ++p;
  }

  bool valid = true;
  auto v = arangodb::NumberUtils::atoi<T>(p, e, valid);
  if (!valid) {
    throw std::out_of_range(value);
  }
  return static_cast<T>(v * m / d);
}

// convert a string into a number, version for double values
template<>
inline double toNumber<double>(std::string value, double /*base*/) {
  // replace leading spaces, replace trailing spaces & comments
  return std::stod(removeCommentsFromNumber(value));
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
  return std::string(&buf[0], static_cast<size_t>(length));
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
  std::string valueString() const override {
    return stringifyValue(ptr->load());
  }

  std::string set(std::string const& value) override {
    if (!required && value.empty()) {
      // the empty value "" is considered "true", e.g. "--force" will mean
      // "--force true"
      *ptr = true;
      return "";
    }
    if (value == "true" || value == "false" || value == "on" ||
        value == "off" || value == "1" || value == "0") {
      ptr->store(value == "true" || value == "on" || value == "1");
      return "";
    }
    return "invalid value for type " + this->name() +
           ". expecting 'true' or 'false'";
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
template<typename T>
struct NumericParameter : public Parameter {
  typedef T ValueType;

  explicit NumericParameter(ValueType* ptr, bool unusedParam = false)
      : ptr(ptr), base(1) {}
  NumericParameter(ValueType* ptr, ValueType base) : ptr(ptr), base(base) {}

  std::string valueString() const override { return stringifyValue(*ptr); }

  std::string set(std::string const& value) override {
    try {
      ValueType v = toNumber<ValueType>(value, base);
      *ptr = v;
      return "";
    } catch (...) {
      return "invalid numeric value '" + value + "' for type " + this->name();
    }
  }

  void toVPack(VPackBuilder& builder) const override {
    builder.add(VPackValue(*ptr));
  }

  ValueType* ptr;
  ValueType base;
};

// concrete int16 number value type
struct Int16Parameter : public NumericParameter<int16_t> {
  typedef int16_t ValueType;

  explicit Int16Parameter(ValueType* ptr, bool unusedParam = false)
      : NumericParameter<ValueType>(ptr) {}
  Int16Parameter(ValueType* ptr, ValueType base)
      : NumericParameter<ValueType>(ptr, base) {}

  std::string name() const override { return "int16"; }
};

// concrete uint16 number value type
struct UInt16Parameter : public NumericParameter<uint16_t> {
  typedef uint16_t ValueType;

  explicit UInt16Parameter(ValueType* ptr, bool unusedParam = false)
      : NumericParameter<ValueType>(ptr) {}
  UInt16Parameter(ValueType* ptr, ValueType base)
      : NumericParameter<ValueType>(ptr, base) {}

  std::string name() const override { return "uint16"; }
};

// concrete int32 number value type
struct Int32Parameter : public NumericParameter<int32_t> {
  typedef int32_t ValueType;

  explicit Int32Parameter(ValueType* ptr, bool unusedParam = false)
      : NumericParameter<ValueType>(ptr) {}
  Int32Parameter(ValueType* ptr, ValueType base)
      : NumericParameter<ValueType>(ptr, base) {}

  std::string name() const override { return "int32"; }
};

// concrete uint32 number value type
struct UInt32Parameter : public NumericParameter<uint32_t> {
  typedef uint32_t ValueType;

  explicit UInt32Parameter(ValueType* ptr, bool unusedParam = false)
      : NumericParameter<ValueType>(ptr) {}
  UInt32Parameter(ValueType* ptr, ValueType base)
      : NumericParameter<ValueType>(ptr, base) {}

  std::string name() const override { return "uint32"; }
};

// concrete int64 number value type
struct Int64Parameter : public NumericParameter<int64_t> {
  typedef int64_t ValueType;

  explicit Int64Parameter(ValueType* ptr, bool unusedParam = false)
      : NumericParameter<ValueType>(ptr) {}
  Int64Parameter(ValueType* ptr, ValueType base)
      : NumericParameter<ValueType>(ptr, base) {}

  std::string name() const override { return "int64"; }
};

// concrete uint64 number value type
struct UInt64Parameter : public NumericParameter<uint64_t> {
  typedef uint64_t ValueType;

  explicit UInt64Parameter(ValueType* ptr, bool unusedParam = false)
      : NumericParameter<ValueType>(ptr) {}
  UInt64Parameter(ValueType* ptr, ValueType base)
      : NumericParameter<ValueType>(ptr, base) {}

  std::string name() const override { return "uint64"; }
};

// concrete size_t number value type
struct SizeTParameter : public NumericParameter<std::size_t> {
  typedef std::size_t ValueType;

  explicit SizeTParameter(ValueType* ptr, bool unusedParam = false)
      : NumericParameter<ValueType>(ptr) {}
  SizeTParameter(ValueType* ptr, ValueType base)
      : NumericParameter<ValueType>(ptr, base) {}

  std::string name() const override { return "size"; }
};

template<typename T>
struct BoundedParameter : public T {
  BoundedParameter(typename T::ValueType* ptr, typename T::ValueType minValue,
                   typename T::ValueType maxValue)
      : T(ptr), minValue(minValue), maxValue(maxValue) {}

  std::string set(std::string const& value) override {
    try {
      typename T::ValueType v = toNumber<typename T::ValueType>(
          value, static_cast<typename T::ValueType>(1));
      if (v >= minValue && v <= maxValue) {
        *this->ptr = v;
        return "";
      }
    } catch (...) {
      return "invalid numeric value '" + value + "' for type " + this->name();
    }
    return "number '" + value + "' out of allowed range (" +
           std::to_string(minValue) + " - " + std::to_string(maxValue) + ")";
  }

  typename T::ValueType minValue;
  typename T::ValueType maxValue;
};

// concrete double number value type
struct DoubleParameter : public NumericParameter<double> {
  typedef double ValueType;

  explicit DoubleParameter(ValueType* ptr, bool unusedParam = false)
      : NumericParameter<double>(ptr) {}

  std::string name() const override { return "double"; }
};

// string value type
struct StringParameter : public Parameter {
  typedef std::string ValueType;

  explicit StringParameter(ValueType* ptr, bool unusedParam = false)
      : ptr(ptr) {}

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
template<typename T>
struct DiscreteValuesParameter : public T {
  typedef std::unordered_set<typename T::ValueType> DiscreteType;

  DiscreteValuesParameter(
      typename T::ValueType* ptr,
      std::unordered_set<typename T::ValueType> const& allowed,
      bool unusedParam = false)
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
    std::string msg("Possible values: ");
    std::vector<std::string> values;
    for (auto const& it : allowed) {
      values.emplace_back(stringifyValue(it));
    }
    std::sort(values.begin(), values.end());

    size_t i = 0;
    for (auto const& it : values) {
      if (i > 0) {
        msg.append(", ");
      }
      msg.append(it);
      ++i;
    }
    return msg;
  }

  DiscreteType allowed;
};

// specialized type for vectors of values
// this templated type needs a concrete value type
template<typename T>
struct VectorParameter : public Parameter {
  typedef std::vector<typename T::ValueType> ValueType;

  explicit VectorParameter(std::vector<typename T::ValueType>* ptr,
                           bool unusedParam = false)
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

  void flushValue() override { ptr->clear(); }

  void toVPack(VPackBuilder& builder) const override {
    builder.openArray();
    for (size_t i = 0; i < ptr->size(); ++i) {
      builder.add(VPackValue(ptr->at(i)));
    }
    builder.close();
  }

  ValueType* ptr;
};

// specialized type for a vector of discrete values (defined in the
// unordered_set) this templated type needs a concrete value type
template<typename T>
struct DiscreteValuesVectorParameter : public Parameter {
  explicit DiscreteValuesVectorParameter(
      std::vector<typename T::ValueType>* ptr,
      std::unordered_set<typename T::ValueType> const& allowed)
      : ptr(ptr), allowed(allowed) {
    for (size_t i = 0; i < ptr->size(); ++i) {
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
    for (size_t i = 0; i < ptr->size(); ++i) {
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

  void toVPack(VPackBuilder& builder) const override {
    builder.openArray();
    for (size_t i = 0; i < ptr->size(); ++i) {
      builder.add(VPackValue(ptr->at(i)));
    }
    builder.close();
  }

  std::string description() const override {
    std::string msg("Possible values: ");
    std::vector<std::string> values;
    for (auto const& it : allowed) {
      values.emplace_back(stringifyValue(it));
    }
    std::sort(values.begin(), values.end());

    size_t i = 0;
    for (auto const& it : values) {
      if (i > 0) {
        msg.append(", ");
      }
      msg.append(it);
      ++i;
    }
    return msg;
  }

  std::vector<typename T::ValueType>* ptr;
  std::unordered_set<typename T::ValueType> allowed;
};

// a type that's useful for obsolete parameters that do nothing
struct ObsoleteParameter : public Parameter {
  explicit ObsoleteParameter(bool requiresValue, bool unusedParam = false)
      : required(requiresValue) {}
  bool requiresValue() const override { return required; }
  std::string name() const override { return "obsolete"; }
  std::string valueString() const override { return "-"; }
  std::string set(std::string const&) override { return ""; }
  void toVPack(VPackBuilder& builder) const override {
    builder.add(VPackValue(VPackValueType::Null));
  }

  bool required;
};

// helper function to parse "context=123"
void parseContext(std::string const& rawValue, std::string& context,
                  std::string& value);

// specialized type for context enriched values
// you can specify a context with "abc=123" as value. If you ever
// need a global context value with a "alphanum*=" start specify it
// as "=alphanum*=".
template<typename T>
struct ContextParameter : public Parameter {
  template<typename, typename = void>
  struct hasDiscreteType : std::false_type {
    typedef bool discret;
  };

  template<typename W>
  struct hasDiscreteType<W, std::void_t<typename W::DiscreteType>>
      : std::true_type {
    typedef typename W::DiscreteType discret;
  };

  ContextParameter(std::unordered_map<std::string, typename T::ValueType>* map,
                   typename T::ValueType proto)
      : map{map}, allowed{true}, proto{proto} {}

  ContextParameter(std::unordered_map<std::string, typename T::ValueType>* map,
                   hasDiscreteType<T>::discret const& allowed,
                   typename T::ValueType proto)
      : map{map}, allowed{allowed}, proto{proto} {}

  std::string name() const override {
    typename T::ValueType dummy = proto;
    T param(&dummy, allowed);
    return "context=" + std::string(param.name());
  }

  template<typename A>
  std::string stringValue(A const& a) const {
    return stringifyValue(a);
  }

  template<typename A>
  std::string stringValue(std::vector<A> const& a) const {
    std::string value;

    for (size_t i = 0; i < a.size(); ++i) {
      if (i > 0) {
        value.append(", ");
      }
      value.append(stringifyValue(a.at(i)));
    }

    return value;
  }

  std::string valueString() const override { return stringValue(proto); }

  std::string set(std::string const& rawValue) override {
    std::string context;
    std::string value;
    parseContext(rawValue, context, value);

    if (map->find(context) == map->end()) {
      map->emplace(context, proto);
    }

    typename T::ValueType* ptr = &((*map)[context]);
    T param(ptr, allowed);

    return param.set(value);
  }

  void flushValue() override { map->clear(); }

  void toVPack(VPackBuilder& builder) const override {
    auto const& itr = map->find("");

    if (map->size() == 1 && itr != map->end()) {
      typename T::ValueType* ptr = &(itr->second);
      T param(ptr, allowed);
      param.toVPack(builder);
    } else if (map->size() == 0) {
      typename T::ValueType ptr = proto;
      T param(&ptr, allowed);
      param.toVPack(builder);
    } else {
      builder.openObject();

      for (auto context : *map) {
        builder.add(VPackValue(context.first));

        typename T::ValueType* ptr = &(context.second);
        T param(ptr, allowed);
        param.toVPack(builder);
      }

      builder.close();
    }
  }

  std::unordered_map<std::string, typename T::ValueType>* map;
  hasDiscreteType<T>::discret allowed;
  typename T::ValueType proto;
};
}  // namespace options
}  // namespace arangodb
