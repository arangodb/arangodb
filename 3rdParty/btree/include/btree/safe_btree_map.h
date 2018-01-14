// Copyright 2013 Google Inc. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// The safe_btree_map<> is like btree_map<> except that it removes the caveat
// about insertion and deletion invalidating existing iterators at a small cost
// in making iterators larger and slower.
//
// Revalidation occurs whenever an iterator is accessed.  References
// and pointers returned by safe_btree_map<> iterators are not stable,
// they are potentially invalidated by any non-const method on the map.
//
// BEGIN INCORRECT EXAMPLE
//   for (auto i = safe_map->begin(); i != safe_map->end(); ++i) {
//     const T *value = &i->second;  // DO NOT DO THIS
//     [code that modifies safe_map and uses value];
//   }
// END INCORRECT EXAMPLE
#ifndef UTIL_BTREE_SAFE_BTREE_MAP_H__
#define UTIL_BTREE_SAFE_BTREE_MAP_H__

#include <functional>
#include <memory>
#include <utility>

#include "btree_container.h"
#include "btree_map.h"
#include "safe_btree.h"

namespace btree {

// The safe_btree_map class is needed mainly for its constructors.
template <typename Key, typename Value,
          typename Compare = std::less<Key>,
          typename Alloc = std::allocator<std::pair<const Key, Value> >,
          int TargetNodeSize = 256>
class safe_btree_map : public btree_map_container<
  safe_btree<btree_map_params<Key, Value, Compare, Alloc, TargetNodeSize> > > {

  typedef safe_btree_map<Key, Value, Compare, Alloc, TargetNodeSize> self_type;
  typedef btree_map_params<
    Key, Value, Compare, Alloc, TargetNodeSize> params_type;
  typedef safe_btree<params_type> btree_type;
  typedef btree_map_container<btree_type> super_type;

 public:
  typedef typename btree_type::key_compare key_compare;
  typedef typename btree_type::allocator_type allocator_type;

 public:
  // Default constructor.
  safe_btree_map(const key_compare &comp = key_compare(),
                 const allocator_type &alloc = allocator_type())
      : super_type(comp, alloc) {
  }

  // Copy constructor.
  safe_btree_map(const self_type &x)
      : super_type(x) {
  }

  // Range constructor.
  template <class InputIterator>
  safe_btree_map(InputIterator b, InputIterator e,
                 const key_compare &comp = key_compare(),
                 const allocator_type &alloc = allocator_type())
      : super_type(b, e, comp, alloc) {
  }
};

template <typename K, typename V, typename C, typename A, int N>
inline void swap(safe_btree_map<K, V, C, A, N> &x,
                 safe_btree_map<K, V, C, A, N> &y) {
  x.swap(y);
}

} // namespace btree

#endif  // UTIL_BTREE_SAFE_BTREE_MAP_H__
