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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_TYPES_H
#define ARANGOD_AQL_TYPES_H 1

#include "Aql/ExecutionNodeId.h"
#include "Basics/debugging.h"

#include <Basics/debugging.h>
#include <Containers/HashSetFwd.h>

#include <map>
#include <memory>
#include <set>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace boost {
namespace container {
template<class T>
class new_allocator;
template <class Key, class Compare, class AllocatorOrContainer>
class flat_set;
}
}  // namespace boost

namespace arangodb {

namespace containers {
template <class Key, class Compare = std::less<Key>, class AllocatorOrContainer = boost::container::new_allocator<Key> >
using flat_set = boost::container::flat_set<Key, Compare, AllocatorOrContainer>;
}

namespace aql {
struct Collection;

/// @brief type for variable ids
typedef uint32_t VariableId;

typedef uint16_t RegisterCount;

/// @brief type for register numbers/ids
struct RegisterId {
  enum class Type : uint16_t {
    Regular = 0,
    Const = 1
  };

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

  constexpr static RegisterId fromUInt32(uint32_t value) noexcept {
    auto v = static_cast<value_t>(value);
    auto type = static_cast<Type>(value >> (sizeof(value_t) * 8));
    RegisterId result (v, type);
    TRI_ASSERT(result.isValid());
    return result;
  }

  uint32_t toUInt32() const noexcept {
    uint32_t result = _value;
    result |= static_cast<uint32_t>(_type) << (sizeof(value_t) * 8);
    return result;
  }

  constexpr value_t value() const noexcept {
    TRI_ASSERT(type() <= Type::Const);
    return _value;
  }

  constexpr Type type() const noexcept { return _type; }

  constexpr bool isConstRegister() const noexcept { return _type == Type::Const; }
  constexpr bool isRegularRegister() const noexcept { return _type == Type::Regular; }

  constexpr bool isValid() const noexcept { return _value < maxRegisterId && _type <= Type::Const; }

  constexpr RegisterId& operator++() noexcept { 
    ++_value;
    TRI_ASSERT(isValid());
    return *this;
  }

  constexpr RegisterId& operator++(int) noexcept {
    ++_value;
    TRI_ASSERT(isValid());
    return *this;
  }

  constexpr RegisterId operator+(int16_t v) noexcept {
    return RegisterId(_value + v, _type);
  }

  constexpr RegisterId operator-(int16_t v) noexcept {
    return RegisterId(_value - v, _type);
  }

  constexpr bool operator<(const RegisterId& rhs) const noexcept {
    TRI_ASSERT(type() == rhs.type())
    return _value < rhs._value;
  }

  constexpr bool operator>(const RegisterId& rhs) const noexcept {
    TRI_ASSERT(type() == rhs.type())
    return _value > rhs._value;
  }

  constexpr bool operator<(const RegisterCount& rhs) const noexcept {
    return _value < rhs;
  }

  constexpr bool operator<=(const RegisterId& rhs) const noexcept {
    TRI_ASSERT(type() == rhs.type())
    return _value <= rhs._value;
  }

  constexpr bool operator==(const RegisterId& rhs) const noexcept {
    return type() == rhs.type() && _value == rhs._value;
  }

  constexpr bool operator==(const value_t& rhs) const noexcept {
    return _value == rhs;
  }

  constexpr bool operator!=(const RegisterId& rhs) const noexcept {
    return type() != rhs.type() || _value != rhs._value;
  }

  constexpr bool operator!=(const value_t& rhs) const noexcept {
    return _value != rhs;
  }
 private:
  uint16_t _value = 0;
  Type _type = Type::Regular;
};
static_assert(sizeof(RegisterId) == sizeof(uint32_t));

/// @brief type of a query id
typedef uint64_t QueryId;
typedef uint64_t EngineId;

// Map RemoteID->ServerID->[SnippetId]
using MapRemoteToSnippet =
    std::unordered_map<ExecutionNodeId, std::unordered_map<std::string, std::vector<std::string>>>;

// Enable/Disable block passthrough in fetchers
enum class BlockPassthrough { Disable, Enable };

class ExecutionEngine;
// list of snippets on coordinators
using SnippetList = std::vector<std::unique_ptr<ExecutionEngine>>;
using ServerQueryIdList = std::vector<std::pair<std::string, QueryId>>;

using AqlCollectionMap = std::map<std::string, aql::Collection*, std::less<>>;

struct Variable;
// Note: #include <Containers/HashSet.h> to use the following types
using VarSet = containers::HashSet<Variable const*>;
using VarSetStack = std::vector<VarSet>;
using RegIdSet = containers::HashSet<RegisterId>;
using RegIdSetStack = std::vector<RegIdSet>;
using RegIdOrderedSet = std::set<RegisterId>;
using RegIdOrderedSetStack = std::vector<RegIdOrderedSet>;
// Note: #include <boost/container/flat_set.hpp> to use the following types
using RegIdFlatSet = containers::flat_set<RegisterId>;
using RegIdFlatSetStack = std::vector<containers::flat_set<RegisterId>>;

}  // namespace aql

namespace traverser {
class BaseEngine;
// list of graph engines on coordinators
using GraphEngineList = std::vector<std::unique_ptr<BaseEngine>>;
}  // namespace traverser

enum class ExplainRegisterPlan {
  No = 0,
  Yes
};

}  // namespace arangodb

namespace std {
  template <> struct hash<arangodb::aql::RegisterId> {
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
}

#endif
