////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include "IndexDefinition.h"

#include "Basics/FloatingPoint.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Inspection/VPack.h"
#include "VectorIndex/Definition.h"

#include <velocypack/Iterator.h>

namespace arangodb {

bool indexDefinitionsEqual(IndexType type, velocypack::Slice lhs,
                           velocypack::Slice rhs, bool attributeOrderMatters) {
  // unique must be identical if present
  bool lhsUnique = basics::VelocyPackHelper::getBooleanValue(
      lhs, StaticStrings::IndexUnique, false);
  bool rhsUnique = basics::VelocyPackHelper::getBooleanValue(
      rhs, StaticStrings::IndexUnique, false);
  if (lhsUnique != rhsUnique) {
    return false;
  }

  // sparse must be identical if present
  if (IndexType::Geo2 != type && IndexType::Geo1 != type &&
      IndexType::Geo != type && IndexType::Fulltext != type) {
    bool lhsSparse = basics::VelocyPackHelper::getBooleanValue(
        lhs, StaticStrings::IndexSparse, false);
    bool rhsSparse = basics::VelocyPackHelper::getBooleanValue(
        rhs, StaticStrings::IndexSparse, false);
    if (lhsSparse != rhsSparse) {
      return false;
    }
  }

  VPackSlice value;

  if (IndexType::Geo1 == type || IndexType::Geo == type) {
    // geoJson must be identical if present
    value = lhs.get("geoJson");

    if (value.isBoolean() &&
        !basics::VelocyPackHelper::equal(value, rhs.get("geoJson"), false)) {
      return false;
    }
  } else if (IndexType::Fulltext == type) {
    // minLength
    value = lhs.get("minLength");

    if (value.isNumber() &&
        !basics::VelocyPackHelper::equal(value, rhs.get("minLength"), false)) {
      return false;
    }
  } else if (IndexType::TTL == type) {
    value = lhs.get(StaticStrings::IndexExpireAfter);

    if (value.isNumber() &&
        rhs.get(StaticStrings::IndexExpireAfter).isNumber()) {
      double const expireAfter = value.getNumber<double>();
      value = rhs.get(StaticStrings::IndexExpireAfter);

      if (!FloatingPoint<double>{expireAfter}.AlmostEquals(
              FloatingPoint<double>{value.getNumber<double>()})) {
        return false;
      }
    }
  } else if (IndexType::MDIPrefixed == type) {
    value = lhs.get(StaticStrings::IndexPrefixFields);

    if (value.isArray() &&
        !basics::VelocyPackHelper::equal(
            value, rhs.get(StaticStrings::IndexPrefixFields), false)) {
      return false;
    }
  } else if (IndexType::Vector == type) {
    // check if the parameters are the same
    vector::UserDefinition leftDefinition;
    vector::UserDefinition rightDefinition;
    velocypack::deserialize(lhs.get("params"), leftDefinition);
    velocypack::deserialize(rhs.get("params"), rightDefinition);

    if (leftDefinition != rightDefinition) {
      return false;
    }
  }

  // other index types: fields must be identical if present
  value = lhs.get(StaticStrings::IndexFields);

  if (value.isArray()) {
    if (!attributeOrderMatters) {
      // attributes can be specified in any order
      velocypack::ValueLength const nv = value.length();

      // compare fields in arbitrary order
      auto r = rhs.get(StaticStrings::IndexFields);

      if (!r.isArray() || nv != r.length()) {
        return false;
      }

      for (size_t i = 0; i < nv; ++i) {
        velocypack::Slice const v = value.at(i);

        bool found = false;

        for (VPackSlice vr : VPackArrayIterator(r)) {
          if (basics::VelocyPackHelper::equal(v, vr, false)) {
            found = true;
            break;
          }
        }

        if (!found) {
          return false;
        }
      }
    } else {
      // attribute order matters
      if (!basics::VelocyPackHelper::equal(
              value, rhs.get(StaticStrings::IndexFields), false)) {
        return false;
      }
    }
  }

  return true;
}

}  // namespace arangodb
