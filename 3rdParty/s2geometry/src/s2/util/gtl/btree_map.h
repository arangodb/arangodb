// Copyright 2007 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS-IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

//
// A btree_map<> implements the STL unique sorted associative container
// interface and the pair associative container interface (a.k.a map<>) using a
// btree. A btree_multimap<> implements the STL multiple sorted associative
// container interface and the pair associative container interface (a.k.a
// multimap<>) using a btree. See btree.h for details of the btree
// implementation and caveats.
//

#ifndef UTIL_GTL_BTREE_MAP_H__
#define UTIL_GTL_BTREE_MAP_H__

#include <algorithm>
#include <functional>
#include <memory>
#include <string>
#include <utility>

#include "s2/util/gtl/btree.h"            // IWYU pragma: export
#include "s2/util/gtl/btree_container.h"  // IWYU pragma: export

namespace gtl {

template <typename Key, typename Value, typename Compare = std::less<Key>,
          typename Alloc = std::allocator<std::pair<const Key, Value>>,
          int TargetNodeSize = 256>
using btree_map = btree_map_container<
    btree<btree_map_params<Key, Value, Compare, Alloc, TargetNodeSize>>>;

template <typename Key, typename Value, typename Compare = std::less<Key>,
          typename Alloc = std::allocator<std::pair<const Key, Value>>,
          int TargetNodeSize = 256>
using btree_multimap = btree_multi_container<
    btree<btree_map_params<Key, Value, Compare, Alloc, TargetNodeSize>>>;

}  // namespace gtl

#endif  // UTIL_GTL_BTREE_MAP_H__
