////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Max Neunhoeffer
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef VELOCYPACK_VALUE_H
#define VELOCYPACK_VALUE_H 1

#include <cstdint>
#include <string>
#include <string_view>

#include "velocypack/velocypack-common.h"
#include "velocypack/ValueType.h"

namespace arangodb {
namespace velocypack {
class StringRef;

class Value {
  // Convenience class for more compact notation

  friend class Builder;

 public:
  enum class CType {
    None = 0,
    Bool = 1,
    Double = 2,
    Int64 = 3,
    UInt64 = 4,
    String = 5,
    CharPtr = 6,
    VoidPtr = 7,
    StringView = 8
  };

 private:
  ValueType const _valueType;
  CType const _cType;  // denotes variant used, 0: none

  union ValueUnion {
    constexpr explicit ValueUnion(bool b) noexcept : b(b) {}
    constexpr explicit ValueUnion(double d) noexcept : d(d) {}
    constexpr explicit ValueUnion(int64_t i) noexcept : i(i) {}
    constexpr explicit ValueUnion(uint64_t u) noexcept : u(u) {}
    constexpr explicit ValueUnion(std::string const* s) noexcept : s(s) {}
    constexpr explicit ValueUnion(char const* c) noexcept : c(c) {}
    constexpr explicit ValueUnion(void const* e) noexcept : e(e) {}
    constexpr explicit ValueUnion(std::string_view const* sv) noexcept : sv(sv) {}

    bool b;                      // 1: bool
    double d;                    // 2: double
    int64_t i;                   // 3: int64_t
    uint64_t u;                  // 4: uint64_t
    std::string const* s;        // 5: std::string
    char const* c;               // 6: char const*
    void const* e;               // 7: external
    std::string_view const* sv;  // 8: std::string_view
  } const _value;

 public:
  Value() = delete;

  // creates a Value with the specified type Array or Object
  explicit Value(ValueType t, bool allowUnindexed = false);

  constexpr explicit Value(bool b, ValueType t = ValueType::Bool) noexcept
      : _valueType(t), _cType(CType::Bool), _value(b) {}

  constexpr explicit Value(double d, ValueType t = ValueType::Double) noexcept
      : _valueType(t), _cType(CType::Double), _value(d) {}

  constexpr explicit Value(void const* e, ValueType t = ValueType::External) noexcept
      : _valueType(t), _cType(CType::VoidPtr), _value(e) {}

  constexpr explicit Value(char const* c, ValueType t = ValueType::String) noexcept
      : _valueType(t), _cType(CType::CharPtr), _value(c) {}

  constexpr explicit Value(int32_t i, ValueType t = ValueType::Int) noexcept
      : _valueType(t), _cType(CType::Int64), _value(static_cast<int64_t>(i)) {}

  constexpr explicit Value(uint32_t u, ValueType t = ValueType::UInt) noexcept
      : _valueType(t), _cType(CType::UInt64), _value(static_cast<uint64_t>(u)) {}

  constexpr explicit Value(int64_t i, ValueType t = ValueType::Int) noexcept
      : _valueType(t), _cType(CType::Int64), _value(i) {}

  constexpr explicit Value(uint64_t u, ValueType t = ValueType::UInt) noexcept
      : _valueType(t), _cType(CType::UInt64), _value(u) {}

#ifdef __APPLE__
  // MacOS uses the following typedefs:
  // - typedef unsigned int         uint32_t;
  // - typedef unsigned long long   uint64_t;
  // - typedef unsigned long        size_t;
  // not defining the method for type unsigned long will prevent
  // users from constructing Value objects with a size_t input

  // however, defining the method on Linux and with MSVC will lead
  // to ambiguous overloads, so this is restricted to __APPLE__ only
  constexpr explicit Value(unsigned long i, ValueType t = ValueType::UInt) noexcept
      : _valueType(t), _cType(CType::UInt64), _value(static_cast<uint64_t>(i)) {}
#endif

  constexpr explicit Value(std::string const& s, ValueType t = ValueType::String) noexcept
      : _valueType(t), _cType(CType::String), _value(&s) {}

  constexpr explicit Value(std::string_view const& sv, ValueType t = ValueType::String) noexcept
      : _valueType(t), _cType(CType::StringView), _value(&sv) {}

  constexpr ValueType valueType() const noexcept { return _valueType; }

  constexpr CType cType() const noexcept { return _cType; }

  // whether or not the underlying Array/Object can be unindexed.
  // it is only allowed to call this for Array/Object types!
  bool unindexed() const;

  constexpr bool isString() const noexcept { return _valueType == ValueType::String; }

  constexpr bool getBool() const noexcept {
    VELOCYPACK_ASSERT(_cType == CType::Bool);
    return _value.b;
  }

  constexpr double getDouble() const noexcept {
    VELOCYPACK_ASSERT(_cType == CType::Double);
    return _value.d;
  }

  constexpr int64_t getInt64() const noexcept {
    VELOCYPACK_ASSERT(_cType == CType::Int64);
    return _value.i;
  }

  constexpr uint64_t getUInt64() const noexcept {
    VELOCYPACK_ASSERT(_cType == CType::UInt64);
    return _value.u;
  }

  constexpr std::string const* getString() const noexcept {
    VELOCYPACK_ASSERT(_cType == CType::String);
    return _value.s;
  }

  constexpr std::string_view const* getStringView() const noexcept {
    VELOCYPACK_ASSERT(_cType == CType::StringView);
    return _value.sv;
  }

  constexpr void const* getExternal() const noexcept {
    VELOCYPACK_ASSERT(_cType == CType::VoidPtr);
    return _value.e;
  }

  constexpr char const* getCharPtr() const noexcept {
    VELOCYPACK_ASSERT(_cType == CType::CharPtr);
    return _value.c;
  }
};

class ValuePair {
  uint8_t const* _start;
  uint64_t const _size;
  ValueType const _type;

 public:
  constexpr ValuePair(uint8_t const* start, uint64_t size,
                      ValueType type = ValueType::Binary) noexcept
      : _start(start), _size(size), _type(type) {}

  ValuePair(char const* start, uint64_t size,
            ValueType type = ValueType::Binary) noexcept
      : ValuePair(reinterpret_cast<uint8_t const*>(start), size, type) {}

  constexpr explicit ValuePair(uint64_t size, ValueType type = ValueType::Binary) noexcept
      : _start(nullptr), _size(size), _type(type) {}

  explicit ValuePair(StringRef const& value, ValueType type = ValueType::Binary) noexcept;

  uint8_t const* getStart() const noexcept { return _start; }

  uint64_t getSize() const noexcept { return _size; }

  ValueType valueType() const noexcept { return _type; }

  bool isString() const noexcept { return _type == ValueType::String; }
};

}  // namespace arangodb::velocypack
}  // namespace arangodb

#endif
