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
// A btree_set<> implements the STL unique sorted associative container
// interface (a.k.a set<>) using a btree. A btree_multiset<> implements the STL
// multiple sorted associative container interface (a.k.a multiset<>) using a
// btree. See btree.h for details of the btree implementation and caveats.
//

#ifndef UTIL_GTL_BTREE_SET_H__
#define UTIL_GTL_BTREE_SET_H__

#include <functional>
#include <memory>
#include <string>

#include "s2/util/gtl/btree.h"            // IWYU pragma: export
#include "s2/util/gtl/btree_container.h"  // IWYU pragma: export

namespace gtl {

template <typename Key, typename Compare = std::less<Key>,
          typename Alloc = std::allocator<Key>, int TargetNodeSize = 256>
class btree_set
    : public internal_btree::btree_unique_container<
          internal_btree::btree<internal_btree::set_params<
              Key, Compare, Alloc, TargetNodeSize, /*Multi=*/false>>> {
  using Base = typename btree_set::btree_unique_container;

 public:
  btree_set() {}
  using Base::Base;
};

template <typename Key, typename Compare = std::less<Key>,
          typename Alloc = std::allocator<Key>, int TargetNodeSize = 256>
class btree_multiset
    : public internal_btree::btree_multi_container<
          internal_btree::btree<internal_btree::set_params<
              Key, Compare, Alloc, TargetNodeSize, /*Multi=*/true>>> {
  using Base = typename btree_multiset::btree_multi_container;

 public:
  btree_multiset() {}
  using Base::Base;
};

}  // namespace gtl

#endif  // UTIL_GTL_BTREE_SET_H__
