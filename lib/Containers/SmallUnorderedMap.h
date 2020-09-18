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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGODB_CONTAINERS_SMALL_UNORDERED_MAP_H
#define ARANGODB_CONTAINERS_SMALL_UNORDERED_MAP_H 1

#include <unordered_map>

#include "Containers/details/short_alloc.h"

namespace arangodb {
namespace containers {

template <class K, class V, std::size_t BufSize = 64,
          std::size_t ElementAlignment = alignof(std::pair<const K, V>)>
using SmallUnorderedMap =
    std::unordered_map<K, V, std::hash<K>, std::equal_to<K>, short_alloc<std::pair<const K, V>, BufSize, ElementAlignment>>;

}  // namespace containers
}  // namespace arangodb

#endif
