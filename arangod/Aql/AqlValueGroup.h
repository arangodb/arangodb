////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_AQL_VALUE_GROUP_H
#define ARANGOD_AQL_AQL_VALUE_GROUP_H 1

#include "Aql/AqlValue.h"
#include "Basics/Common.h"

namespace arangodb {
namespace transaction {
class Methods;
}

namespace aql {

/// @brief hasher for a vector of AQL values
struct AqlValueGroupHash {
  AqlValueGroupHash(transaction::Methods* trx, size_t num)
      : _trx(trx), _num(num) {}

  size_t operator()(std::vector<AqlValue> const& value) const {
    uint64_t hash = 0x12345678;

    TRI_ASSERT(value.size() == _num);

    for (auto const& it : value) {
      // we must use the slow hash function here, because a value may have
      // different representations in case its an array/object/number
      // (calls normalizedHash() internally)
      hash = it.hash(_trx, hash);
    }

    return static_cast<size_t>(hash);
  }

  transaction::Methods* _trx;
  size_t const _num;
};

/// @brief comparator for a vector of AQL values
struct AqlValueGroupEqual {
  explicit AqlValueGroupEqual(transaction::Methods* trx) : _trx(trx) {}

  bool operator()(std::vector<AqlValue> const& lhs, std::vector<AqlValue> const& rhs) const {
    size_t const n = lhs.size();

    for (size_t i = 0; i < n; ++i) {
      int res = AqlValue::Compare(_trx, lhs[i], rhs[i], false);

      if (res != 0) {
        return false;
      }
    }

    return true;
  }

  transaction::Methods* _trx;
};

}  // namespace aql
}  // namespace arangodb

#endif
