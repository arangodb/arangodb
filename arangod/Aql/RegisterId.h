////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_REGISTER_ID_H
#define ARANGOD_AQL_REGISTER_ID_H 1

#include "Basics/debugging.h"

#include <type_traits>

namespace arangodb::aql {

typedef uint16_t RegisterCount;

/// @brief type for register numbers/ids
struct RegisterId {
  enum class Type : uint16_t { Regular = 0, Const = 1 };

  using value_t = uint16_t;
  static_assert(std::is_same_v<value_t, RegisterCount>);

  constexpr RegisterId() = default;

  static constexpr value_t maxRegisterId = 1000;

  // TODO - make constructor explicit (requires a lot of changes in test code)
  constexpr RegisterId(value_t v, Type type = Type::Regular)
      : _value(v), _type(type) {
    TRI_ASSERT(v <= maxRegisterId);
  }

  constexpr static RegisterId makeConst(value_t value) noexcept {
    return RegisterId(value, Type::Const);
  }

  constexpr static RegisterId makeRegular(value_t value) noexcept {
    return RegisterId(value, Type::Regular);
  }

  static RegisterId fromUInt32(uint32_t value);

  uint32_t toUInt32() const noexcept;

  constexpr value_t value() const noexcept {
    TRI_ASSERT(type() <= Type::Const);
    return _value;
  }

  constexpr Type type() const noexcept { return _type; }

  constexpr bool isConstRegister() const noexcept {
    return _type == Type::Const;
  }
  constexpr bool isRegularRegister() const noexcept {
    return _type == Type::Regular;
  }

  constexpr bool isValid() const noexcept {
    return _value < maxRegisterId && _type <= Type::Const;
  }

  constexpr bool operator<(RegisterId const& rhs) const noexcept {
    TRI_ASSERT(type() == rhs.type())
    return _value < rhs._value;
  }

  constexpr bool operator>(RegisterId const& rhs) const noexcept {
    TRI_ASSERT(type() == rhs.type())
    return _value > rhs._value;
  }

  constexpr bool operator==(RegisterId const& rhs) const noexcept {
    return type() == rhs.type() && _value == rhs._value;
  }

  constexpr bool operator!=(RegisterId const& rhs) const noexcept {
    return !(*this == rhs);
  }

  constexpr bool operator==(value_t const& rhs) const noexcept {
    return _value == rhs;
  }

  constexpr bool operator!=(value_t const& rhs) const noexcept {
    return _value != rhs;
  }

 private:
  uint16_t _value = 0;
  Type _type = Type::Regular;
};
static_assert(sizeof(RegisterId) == sizeof(uint32_t));

}  // namespace arangodb::aql

namespace std {
template <>
struct hash<arangodb::aql::RegisterId> {
  size_t operator()(arangodb::aql::RegisterId const& x) const noexcept {
    return std::hash<unsigned>()(x.value());
  }
};

template <>
struct equal_to<arangodb::aql::RegisterId> {
  bool operator()(arangodb::aql::RegisterId const& lhs,
                  arangodb::aql::RegisterId const& rhs) const noexcept {
    return lhs == rhs;
  }
};
}  // namespace std

#endif