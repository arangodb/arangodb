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
/// @author Max Neunhoeffer
////////////////////////////////////////////////////////////////////////////////

#include "SortInformation.h"

#include <cstddef>

using namespace arangodb;
using namespace arangodb::aql;

SortInformation::Match SortInformation::isCoveredBy(
    SortInformation const& other) {
  if (!isValid || !other.isValid) {
    return unequal;
  }

  if (isComplex || other.isComplex) {
    return unequal;
  }

  size_t const n = criteria.size();
  for (size_t i = 0; i < n; ++i) {
    if (other.criteria.size() <= i) {
      return otherLessAccurate;
    }

    auto ours = criteria[i];
    auto theirs = other.criteria[i];

    if (std::get<2>(ours) != std::get<2>(theirs)) {
      // sort order is different
      return unequal;
    }

    if (std::get<1>(ours) != std::get<1>(theirs)) {
      // sort criterion is different
      return unequal;
    }
  }

  if (other.criteria.size() > n) {
    return ourselvesLessAccurate;
  }

  return allEqual;
}
