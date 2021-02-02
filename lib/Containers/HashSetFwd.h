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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_CONTAINERS_HASH_SET_FWD_H
#define ARANGODB_CONTAINERS_HASH_SET_FWD_H

#include <functional>

namespace emilib {

template <typename T>
struct HashSetEqualTo;

template <typename KeyT, typename HashT, typename EqT>
class HashSet;

template <typename KeyT, typename HashT, typename EqT>
bool operator==(HashSet<KeyT, HashT, EqT> const&, HashSet<KeyT, HashT, EqT> const&);

}  // namespace emilib

// map emilib::HashSet into arangodb namespace
namespace arangodb {
namespace containers {

template <typename T>
using HashSetEqualTo = emilib::HashSetEqualTo<T>;

template <typename KeyT, typename HashT = std::hash<KeyT>, typename EqT = HashSetEqualTo<KeyT>>
using HashSet = emilib::HashSet<KeyT, HashT, EqT>;

}  // namespace containers
}  // namespace arangodb

#endif  // ARANGODB_CONTAINERS_HASH_SET_FWD_H
