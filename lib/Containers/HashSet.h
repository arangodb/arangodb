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
/// @author Jan Steemann
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_CONTAINERS_HASH_SET_H
#define ARANGODB_CONTAINERS_HASH_SET_H 1

// Include implementation and forward-declarations in our namespace:

#include "Containers/HashSetFwd.h"
#include "Containers/details/HashSetImpl.h"

#include <algorithm>

namespace emilib {

template <typename KeyT, typename HashT, typename EqT>
bool operator==(HashSet<KeyT, HashT, EqT> const& left, HashSet<KeyT, HashT, EqT> const& right) {
  if (left.size() != right.size()) {
    return false;
  }

  return std::all_of(left.begin(), left.end(),
                     [&right](auto const& it) { return right.contains(it); });
}

}  // namespace emilib

#endif
