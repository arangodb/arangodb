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

#ifndef UTIL_GTL_BTREE_CONTAINER_H__
#define UTIL_GTL_BTREE_CONTAINER_H__

#include <initializer_list>
#include <iosfwd>
#include <utility>

#include "s2/util/gtl/btree.h"  // IWYU pragma: export

namespace gtl {

// A common base class for btree_set, btree_map, btree_multiset and
// btree_multimap.
template <typename Tree>
class btree_container {
  typedef btree_container<Tree> self_type;

 public:
  typedef typename Tree::params_type params_type;
  typedef typename Tree::key_type key_type;
  typedef typename Tree::value_type value_type;
  typedef typename Tree::key_compare key_compare;
  typedef typename Tree::allocator_type allocator_type;
  typedef typename Tree::pointer pointer;
  typedef typename Tree::const_pointer const_pointer;
  typedef typename Tree::reference reference;
  typedef typename Tree::const_reference const_reference;
  typedef typename Tree::size_type size_type;
  typedef typename Tree::difference_type difference_type;
  typedef typename Tree::iterator iterator;
  typedef typename Tree::const_iterator const_iterator;
  typedef typename Tree::reverse_iterator reverse_iterator;
  typedef typename Tree::const_reverse_iterator const_reverse_iterator;

 public:
  // Default constructor.
  btree_container(const key_compare &comp, const allocator_type &alloc)
      : tree_(comp, alloc) {
  }

  // Constructor/assignments.
  btree_container(const self_type &x) = default;
  btree_container(self_type &&x) = default;
  self_type &operator=(const self_type &x) = default;
  self_type &operator=(self_type &&x) = default;

  // Iterator routines.
  iterator begin() { return tree_.begin(); }
  const_iterator begin() const { return tree_.begin(); }
  iterator end() { return tree_.end(); }
  const_iterator end() const { return tree_.end(); }
  reverse_iterator rbegin() { return tree_.rbegin(); }
  const_reverse_iterator rbegin() const { return tree_.rbegin(); }
  reverse_iterator rend() { return tree_.rend(); }
  const_reverse_iterator rend() const { return tree_.rend(); }

  // Lookup routines.
  template <typename K>
  iterator lower_bound(const K &key) {
    return tree_.lower_bound(key);
  }
  template <typename K>
  const_iterator lower_bound(const K &key) const {
    return tree_.lower_bound(key);
  }
  template <typename K>
  iterator upper_bound(const K &key) {
    return tree_.upper_bound(key);
  }
  template <typename K>
  const_iterator upper_bound(const K &key) const {
    return tree_.upper_bound(key);
  }
  template <typename K>
  std::pair<iterator, iterator> equal_range(const K &key) {
    return tree_.equal_range(key);
  }
  template <typename K>
  std::pair<const_iterator, const_iterator> equal_range(const K &key) const {
    return tree_.equal_range(key);
  }

  // Utility routines.
  void clear() {
    tree_.clear();
  }
  void swap(self_type &x) {
    tree_.swap(x.tree_);
  }
  void dump(std::ostream &os) const {
    tree_.dump(os);
  }
  void verify() const {
    tree_.verify();
  }

  // Size routines.
  size_type size() const { return tree_.size(); }
  size_type max_size() const { return tree_.max_size(); }
  bool empty() const { return tree_.empty(); }
  size_type height() const { return tree_.height(); }
  size_type internal_nodes() const { return tree_.internal_nodes(); }
  size_type leaf_nodes() const { return tree_.leaf_nodes(); }
  size_type nodes() const { return tree_.nodes(); }
  size_type bytes_used() const { return tree_.bytes_used(); }
  static double average_bytes_per_value() {
    return Tree::average_bytes_per_value();
  }
  double fullness() const { return tree_.fullness(); }
  double overhead() const { return tree_.overhead(); }

  friend
  bool operator==(const self_type& x, const self_type& y) {
    if (x.size() != y.size()) return false;
    return std::equal(x.begin(), x.end(), y.begin());
  }

  friend
  bool operator!=(const self_type& x, const self_type& y) {
    return !(x == y);
  }

  // The allocator used by the btree.
  allocator_type get_allocator() const {
    return tree_.get_allocator();
  }

  // The key comparator used by the btree.
  key_compare key_comp() const { return tree_.key_comp(); }

  // Exposed only for tests.
  static bool testonly_uses_linear_node_search() {
    return Tree::testonly_uses_linear_node_search();
  }

 protected:
  Tree tree_;
};

template <typename T>
inline std::ostream& operator<<(std::ostream &os, const btree_container<T> &b) {
  b.dump(os);
  return os;
}

// Base class for btree_set.
template <typename Tree>
class btree_unique_container : public btree_container<Tree> {
  typedef btree_unique_container<Tree> self_type;
  typedef btree_container<Tree> super_type;

 public:
  typedef typename Tree::key_type key_type;
  typedef typename Tree::data_type data_type;
  typedef typename Tree::value_type value_type;
  typedef typename Tree::mutable_value_type mutable_value_type;
  typedef typename Tree::mapped_type mapped_type;
  typedef typename Tree::size_type size_type;
  typedef typename Tree::key_compare key_compare;
  typedef typename Tree::allocator_type allocator_type;
  typedef typename Tree::iterator iterator;
  typedef typename Tree::const_iterator const_iterator;

 public:
  // Default constructor.
  btree_unique_container(const key_compare &comp = key_compare(),
                         const allocator_type &alloc = allocator_type())
      : super_type(comp, alloc) {}

  // Range constructor.
  template <class InputIterator>
  btree_unique_container(InputIterator b, InputIterator e,
                         const key_compare &comp = key_compare(),
                         const allocator_type &alloc = allocator_type())
      : super_type(comp, alloc) {
    insert(b, e);
  }

  // Initializer list constructor.
  btree_unique_container(std::initializer_list<value_type> init,
                         const key_compare &comp = key_compare(),
                         const allocator_type &alloc = allocator_type())
      : btree_unique_container(init.begin(), init.end(), comp, alloc) {}

  // Constructor/assignments.
  btree_unique_container(const self_type &x) = default;
  btree_unique_container(self_type &&x) = default;
  self_type &operator=(const self_type &x) = default;
  self_type &operator=(self_type &&x) = default;

  // Lookup routines.
  template <typename K>
  iterator find(const K &key) {
    return this->tree_.find_unique(key);
  }
  template <typename K>
  const_iterator find(const K &key) const {
    return this->tree_.find_unique(key);
  }
  template <typename K>
  size_type count(const K &key) const {
    return this->tree_.count_unique(key);
  }

  // Insertion routines.
  std::pair<iterator, bool> insert(const value_type &x) {
    return this->tree_.insert_unique(x);
  }
  template <typename... Args>
  std::pair<iterator, bool> emplace(Args &&... args) {
    mutable_value_type v(std::move(args)...);
    return this->tree_.insert_unique(std::move(v));
  }
  iterator insert(iterator position, const value_type &x) {
    return this->tree_.insert_hint_unique(position, x);
  }
  template <typename... Args>
  iterator emplace_hint(iterator position, Args &&... args) {
    mutable_value_type v(std::move(args)...);
    return this->tree_.insert_hint_unique(position, std::move(v));
  }
  template <typename InputIterator>
  void insert(InputIterator b, InputIterator e) {
    this->tree_.insert_iterator_unique(b, e);
  }
  void insert(std::initializer_list<value_type> init) {
    this->tree_.insert_iterator_unique(init.begin(), init.end());
  }

  // Deletion routines.
  template <typename K>
  int erase(const K &key) {
    return this->tree_.erase_unique(key);
  }
  // Erase the specified iterator from the btree. The iterator must be valid
  // (i.e. not equal to end()).  Return an iterator pointing to the node after
  // the one that was erased (or end() if none exists).
  iterator erase(const iterator &iter) {
    return this->tree_.erase(iter);
  }
  void erase(const iterator &first, const iterator &last) {
    this->tree_.erase(first, last);
  }
};

// Provide swap reachable by ADL for btree_unique_container.
template <typename Tree>
inline void swap(btree_unique_container<Tree> &x,
                 btree_unique_container<Tree> &y) {
  x.swap(y);
}

// Base class for btree_map.
template <typename Tree>
class btree_map_container : public btree_unique_container<Tree> {
  typedef btree_map_container<Tree> self_type;
  typedef btree_unique_container<Tree> super_type;

 public:
  typedef typename Tree::key_type key_type;
  typedef typename Tree::data_type data_type;
  typedef typename Tree::value_type value_type;
  typedef typename Tree::mutable_value_type mutable_value_type;
  typedef typename Tree::mapped_type mapped_type;
  typedef typename Tree::key_compare key_compare;
  typedef typename Tree::allocator_type allocator_type;

 private:
  // A pointer-like object which only generates its value when
  // dereferenced. Used by operator[] to avoid constructing an empty data_type
  // if the key already exists in the map.
  struct generate_value {
    explicit generate_value(const key_type &k) : key(k) {}
    mutable_value_type operator*() const {
      return std::make_pair(key, data_type());
    }
    const key_type &key;
  };

  // Same thing, but when the key value is movable.  We need to generate
  // mutable_value_type, as that's what the btree nodes actually store.
  struct generate_movable_value {
    explicit generate_movable_value(key_type *k) : key(k) {}
    mutable_value_type operator*() const {
      return std::make_pair(std::move(*key), data_type());
    }
    key_type *key;
  };

 public:
  // Default constructor.
  btree_map_container(const key_compare &comp = key_compare(),
                      const allocator_type &alloc = allocator_type())
      : super_type(comp, alloc) {
  }

  // Range constructor.
  template <class InputIterator>
  btree_map_container(InputIterator b, InputIterator e,
                      const key_compare &comp = key_compare(),
                      const allocator_type &alloc = allocator_type())
      : super_type(b, e, comp, alloc) {
  }

  // Initializer list constructor.
  btree_map_container(std::initializer_list<value_type> init,
                      const key_compare &comp = key_compare(),
                      const allocator_type &alloc = allocator_type())
      : btree_map_container(init.begin(), init.end(), comp, alloc) {}

  // Constructor/assignments.
  btree_map_container(const self_type &x) = default;
  btree_map_container(self_type &&x) = default;
  self_type &operator=(const self_type &x) = default;
  self_type &operator=(self_type &&x) = default;

  // Insertion routines.
  data_type& operator[](const key_type &key) {
    return this->tree_.insert_unique(key, generate_value(key)).first->second;
  }
  data_type &operator[](key_type &&key) {
    return this->tree_.insert_unique(key, generate_movable_value(&key))
        .first->second;
  }
};

// Provide swap reachable by ADL for btree_map_container.
template <typename Tree>
inline void swap(btree_map_container<Tree> &x, btree_map_container<Tree> &y) {
  x.swap(y);
}

// A common base class for btree_multiset and btree_multimap.
template <typename Tree>
class btree_multi_container : public btree_container<Tree> {
  typedef btree_multi_container<Tree> self_type;
  typedef btree_container<Tree> super_type;

 public:
  typedef typename Tree::key_type key_type;
  typedef typename Tree::value_type value_type;
  typedef typename Tree::mapped_type mapped_type;
  typedef typename Tree::size_type size_type;
  typedef typename Tree::key_compare key_compare;
  typedef typename Tree::allocator_type allocator_type;
  typedef typename Tree::iterator iterator;
  typedef typename Tree::const_iterator const_iterator;

 public:
  // Default constructor.
  btree_multi_container(const key_compare &comp = key_compare(),
                        const allocator_type &alloc = allocator_type())
      : super_type(comp, alloc) {
  }

  // Range constructor.
  template <class InputIterator>
  btree_multi_container(InputIterator b, InputIterator e,
                        const key_compare &comp = key_compare(),
                        const allocator_type &alloc = allocator_type())
      : super_type(comp, alloc) {
    insert(b, e);
  }

  // Initializer list constructor.
  btree_multi_container(std::initializer_list<value_type> init,
                        const key_compare &comp = key_compare(),
                        const allocator_type &alloc = allocator_type())
      : btree_multi_container(init.begin(), init.end(), comp, alloc) {}

  // Constructor/assignments.
  btree_multi_container(const self_type &x) = default;
  btree_multi_container(self_type &&x) = default;
  self_type &operator=(const self_type &x) = default;
  self_type &operator=(self_type &&x) = default;

  // Lookup routines.
  template <typename K>
  iterator find(const K &key) {
    return this->tree_.find_multi(key);
  }
  template <typename K>
  const_iterator find(const K &key) const {
    return this->tree_.find_multi(key);
  }
  template <typename K>
  size_type count(const K &key) const {
    return this->tree_.count_multi(key);
  }

  // Insertion routines.
  iterator insert(const value_type &x) {
    return this->tree_.insert_multi(x);
  }
  iterator insert(iterator position, const value_type &x) {
    return this->tree_.insert_hint_multi(position, x);
  }
  template <typename InputIterator>
  void insert(InputIterator b, InputIterator e) {
    this->tree_.insert_iterator_multi(b, e);
  }
  void insert(std::initializer_list<value_type> init) {
    this->tree_.insert_iterator_multi(init.begin(), init.end());
  }

  // Deletion routines.
  template <typename K>
  int erase(const K &key) {
    return this->tree_.erase_multi(key);
  }
  // Erase the specified iterator from the btree. The iterator must be valid
  // (i.e. not equal to end()).  Return an iterator pointing to the node after
  // the one that was erased (or end() if none exists).
  iterator erase(const iterator &iter) {
    return this->tree_.erase(iter);
  }
  void erase(const iterator &first, const iterator &last) {
    this->tree_.erase(first, last);
  }
};

// Provide swap reachable by ADL for btree_multi_container.
template <typename Tree>
inline void swap(btree_multi_container<Tree> &x,
                 btree_multi_container<Tree> &y) {
  x.swap(y);
}

}  // namespace gtl

#endif  // UTIL_GTL_BTREE_CONTAINER_H__
