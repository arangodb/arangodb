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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "AqlValueGroup.h"

#include "Basics/debugging.h"

using namespace arangodb;
using namespace arangodb::aql;

AqlValueGroupHash::AqlValueGroupHash([[maybe_unused]] size_t num)
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    : _num(num)
#endif
{
}

size_t AqlValueGroupHash::operator()(const std::vector<AqlValue>& value) const {
  uint64_t hash = 0x12345678;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(value.size() == _num);
#endif

  for (AqlValue const& it : value) {
    // we must use the slow hash function here, because a value may have
    // different representations in case its an array/object/number
    // (calls normalizedHash() internally)
    hash = it.hash(hash);
  }

  return static_cast<size_t>(hash);
}

size_t AqlValueGroupHash::operator()(AqlValue const& value) const {
  uint64_t hash = 0x12345678;
  return value.hash(hash);
}

size_t AqlValueGroupHash::operator()(
    HashedAqlValueGroup const& value) const noexcept {
  TRI_ASSERT(operator()(value.values) == value.hash);
  return value.hash;
}

AqlValueGroupEqual::AqlValueGroupEqual(velocypack::Options const* opts)
    : _vpackOptions(opts) {}

bool AqlValueGroupEqual::operator()(const std::vector<AqlValue>& lhs,
                                    const std::vector<AqlValue>& rhs) const {
  size_t const n = lhs.size();

  for (size_t i = 0; i < n; ++i) {
    int res = AqlValue::Compare(_vpackOptions, lhs[i], rhs[i], false);

    if (res != 0) {
      return false;
    }
  }

  return true;
}

bool AqlValueGroupEqual::operator()(AqlValue const& lhs,
                                    AqlValue const& rhs) const {
  return AqlValue::Compare(_vpackOptions, lhs, rhs, false) == 0;
}

bool AqlValueGroupEqual::operator()(HashedAqlValueGroup const& lhs,
                                    HashedAqlValueGroup const& rhs) const {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  AqlValueGroupHash hasher(lhs.values.size());
  TRI_ASSERT(hasher(lhs.values) == lhs.hash);
  TRI_ASSERT(hasher(rhs.values) == rhs.hash);
#endif
  return lhs.hash == rhs.hash && operator()(lhs.values, rhs.values);
}
