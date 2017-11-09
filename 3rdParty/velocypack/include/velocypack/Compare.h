////////////////////////////////////////////////////////////////////////////////
/// @brief Library to build up VPack documents.
///
/// DISCLAIMER
///
/// Copyright 2015 ArangoDB GmbH, Cologne, Germany
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
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#ifndef VELOCYPACK_COMPARE_H
#define VELOCYPACK_COMPARE_H 1

#include "velocypack/velocypack-common.h"
#include "velocypack/Collection.h"
#include "velocypack/Exception.h"
#include "velocypack/Iterator.h"
#include "velocypack/Slice.h"
#include "velocypack/ValueType.h"

#include <set>

namespace arangodb {
namespace velocypack {
struct NormalizedCompare {

static bool equalsNumbers(ValueType lhsType,
                          Slice lhs, Slice rhs) {
  if (lhsType == rhs.type()) {
    // both types are equal
    if (lhsType == ValueType::Int || lhsType == ValueType::SmallInt) {
      // use exact comparisons. no need to cast to double
      return (lhs.getIntUnchecked() == rhs.getIntUnchecked());
    }

    if (lhsType == ValueType::UInt) {
      // use exact comparisons. no need to cast to double
      return (lhs.getUIntUnchecked() == rhs.getUIntUnchecked());
    }
    // fallthrough to double comparison
  }

  return (lhs.getNumericValue<double>() == rhs.getNumericValue<double>());
}

static bool equalsStrings(Slice lhs, Slice rhs) {
  ValueLength nl;
  char const* left = lhs.getString(nl);
  VELOCYPACK_ASSERT(left != nullptr);
  ValueLength nr;
  char const* right = rhs.getString(nr);
  VELOCYPACK_ASSERT(right != nullptr);
  return (nl == nr && (memcmp(left, right, nl) == 0));
}

static bool equals(Slice lhs, Slice rhs) {
  lhs = lhs.resolveExternals();
  rhs = rhs.resolveExternals();
  ValueType lhsType = valueTypeGroup(lhs.type());
  ValueType rhsType = valueTypeGroup(rhs.type());

  if (lhsType != rhsType) {
    // unequal types => not equal
    return false;
  }
  
  switch (lhsType) {
    case ValueType::Illegal:
    case ValueType::None:
    case ValueType::Null:
      // Null equals Null
    case ValueType::MinKey:
    case ValueType::MaxKey: {
      return true;
    }
    case ValueType::Bool: {
      return (lhs.getBoolean() == rhs.getBoolean());
    }
    case ValueType::Double:
    case ValueType::Int:
    case ValueType::UInt:
    case ValueType::SmallInt: {
      return equalsNumbers(lhsType, lhs, rhs);
    }
    case ValueType::String: {
      return equalsStrings(lhs, rhs);
    }
    case ValueType::Array: {
      ArrayIterator lhsValue(lhs);
      ArrayIterator rhsValue(rhs);

      ValueLength const n = lhsValue.size();
      if (n != rhsValue.size()) {
        // unequal array lengths
        return false;
      }
      for (ValueLength i = 0; i < n; ++i) {
        // recurse
        if (!equals(lhsValue.value(), rhsValue.value())) {
          return false;
        }
        lhsValue.next();
        rhsValue.next();
      }
      return true;
    }
    case ValueType::Object: {
      std::set<std::string> keys;
      // get all keys and bring them in order
      Collection::unorderedKeys(lhs, keys);
      Collection::unorderedKeys(rhs, keys);
      for (auto const& key : keys) {
        // recurse
        if (!equals(lhs.get(key), rhs.get(key))) {
          return false;
        }
      }
      return true;
    }
    case ValueType::Custom: {
      throw Exception(Exception::NotImplemented, "equals comparison for Custom type is not implemented");
    }
    default: {
      throw Exception(Exception::InternalError, "invalid value type for equals comparison");
    }
  }
}

};

}
}

#endif
