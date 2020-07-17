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
namespace velocypack {
struct Options;
}

namespace aql {

/// @brief hasher for a vector of AQL values
struct AqlValueGroupHash {
  explicit AqlValueGroupHash(size_t num);

  size_t operator()(std::vector<AqlValue> const& value) const;
  size_t operator()(AqlValue const& value) const;

  size_t const _num;
};

/// @brief comparator for a vector of AQL values
struct AqlValueGroupEqual {
  explicit AqlValueGroupEqual(velocypack::Options const*);

  bool operator()(std::vector<AqlValue> const& lhs, std::vector<AqlValue> const& rhs) const;
  bool operator()(AqlValue const& lhs, AqlValue const& rhs) const;

  velocypack::Options const* _vpackOptions;
};


}  // namespace aql
}  // namespace arangodb

#endif
