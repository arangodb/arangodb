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

#ifndef S2_UTIL_GTL_BTREE_MAP_H_
#define S2_UTIL_GTL_BTREE_MAP_H_

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
class btree_map
    : public internal_btree::btree_map_container<
          internal_btree::btree<internal_btree::map_params<
              Key, Value, Compare, Alloc, TargetNodeSize, /*Multi=*/false>>> {
  using Base = typename btree_map::btree_map_container;

 public:
  btree_map() {}
  using Base::Base;
};

template <typename K, typename V, typename C, typename A, int T>
void swap(btree_map<K, V, C, A, T> &x, btree_map<K, V, C, A, T> &y) {
  return x.swap(y);
}

template <typename Key, typename Value, typename Compare = std::less<Key>,
          typename Alloc = std::allocator<std::pair<const Key, Value>>,
          int TargetNodeSize = 256>
class btree_multimap
    : public internal_btree::btree_multimap_container<
          internal_btree::btree<internal_btree::map_params<
              Key, Value, Compare, Alloc, TargetNodeSize, /*Multi=*/true>>> {
  using Base = typename btree_multimap::btree_multimap_container;

 public:
  btree_multimap() {}
  using Base::Base;
};

template <typename K, typename V, typename C, typename A, int T>
void swap(btree_multimap<K, V, C, A, T> &x, btree_multimap<K, V, C, A, T> &y) {
  return x.swap(y);
}

}  // namespace gtl

#endif  // S2_UTIL_GTL_BTREE_MAP_H_
