// Copyright 2005 Google Inc. All Rights Reserved.
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

// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

// ---
//
// This is just a very thin wrapper over densehashtable.h, just
// like sgi stl's stl_hash_set is a very thin wrapper over
// stl_hashtable.
//
// This is more different from dense_hash_map than you might think,
// because all iterators for sets are const (you obviously can't
// change the key, and for sets there is no value).
//
// NOTE: this is exactly like sparse_hash_set.h, with the word
// "sparse" replaced by "dense", except for the addition of
// set_empty_key().
//
//   YOU MUST CALL SET_EMPTY_KEY() IMMEDIATELY AFTER CONSTRUCTION.
//
// Otherwise your program will die in mysterious ways.  (Note if you
// use the constructor that takes an InputIterator range, you pass in
// the empty key in the constructor, rather than after.  As a result,
// this constructor differs from the standard STL version.)
//
// In other respects, we adhere mostly to the STL semantics for
// hash-map.  One important exception is that insert() may invalidate
// iterators entirely -- STL semantics are that insert() may reorder
// iterators, but they all still refer to something valid in the
// hashtable.  Not so for us.  Likewise, insert() may invalidate
// pointers into the hashtable.  (Whether insert invalidates iterators
// and pointers depends on whether it results in a hashtable resize,
// but that's an implementation detail that may change in the future.)
// On the plus side, delete() doesn't invalidate iterators or pointers
// at all, or even change the ordering of elements.
//
// Also please note:
//
//    1) set_deleted_key():
//         If you want to use erase() you must call set_deleted_key(),
//         in addition to set_empty_key(), after construction.
//         The deleted and empty keys must differ.
//
//    2) Keys equal to the empty key or deleted key (if any) cannot be
//         used as keys for find(), count(), insert(), etc.
//
//    3) min_load_factor():
//         Setting the minimum load factor controls how aggressively the
//         table is shrunk when keys are erased.  Setting it to 0.0
//         guarantees that the hash table will never shrink.
//
//    4) resize(0):
//         When an item is deleted, its memory isn't freed right
//         away.  This allows you to iterate over a hashtable,
//         and call erase(), without invalidating the iterator.
//         To force the memory to be freed, call resize(0).
//         For tr1 compatibility, this can also be called as rehash(0).
// Roughly speaking:
//   (1) dense_hash_set: fastest, uses the most memory unless entries are small
//   (2) sparse_hash_set: slowest, uses the least memory
//   (3) hash_set / unordered_set (STL): in the middle
//
// Typically I use sparse_hash_set when I care about space and/or when
// I need to save the hashtable on disk.  I use hash_set otherwise.  I
// don't personally use dense_hash_set ever; some people use it for
// small sets with lots of lookups.
//
// - dense_hash_set has, typically, about 78% memory overhead (if your
//   data takes up X bytes, the hash_set uses .78X more bytes in overhead).
// - sparse_hash_set has about 4 bits overhead per entry.
// - sparse_hash_set can be 3-7 times slower than the others for lookup and,
//   especially, inserts.  See time_hash_map.cc for details.
//
// See /usr/(local/)?doc/sparsehash-*/dense_hash_set.html
// for information about how to use this class.

#ifndef S2_UTIL_GTL_DENSE_HASH_SET_H_
#define S2_UTIL_GTL_DENSE_HASH_SET_H_

#include <cstdio>
#include <algorithm>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include "s2/base/port.h"
#include "s2/third_party/absl/base/macros.h"
#include "s2/util/gtl/densehashtable.h"  // IWYU pragma: export
#include "s2/util/gtl/libc_allocator_with_realloc.h"

// Some files test for this symbol.
#define S2__DENSE_HASH_SET_H_

namespace gtl {

template <class Value,
          class HashFcn = std::hash<Value>,
          class EqualKey = std::equal_to<Value>,
          class Alloc = libc_allocator_with_realloc<Value> >
class dense_hash_set {
 private:

  // Apparently identity is not stl-standard, so we define our own
  struct Identity {
    typedef const Value& result_type;
    const Value& operator()(const Value& v) const { return v; }
  };
  struct SetKey {
    void operator()(Value* value, const Value& new_key) const {
      *value = new_key;
    }
  };

  // The actual data
  typedef dense_hashtable<Value, Value, HashFcn, Identity, SetKey,
                          EqualKey, Alloc> ht;
  ht rep;

 public:
  typedef typename ht::key_type key_type;
  typedef typename ht::value_type value_type;
  typedef typename ht::hasher hasher;
  typedef typename ht::key_equal key_equal;
  typedef Alloc allocator_type;

  typedef typename ht::size_type size_type;
  typedef typename ht::difference_type difference_type;
  typedef typename ht::const_pointer pointer;
  typedef typename ht::const_pointer const_pointer;
  typedef typename ht::const_reference reference;
  typedef typename ht::const_reference const_reference;

  typedef typename ht::const_iterator iterator;
  typedef typename ht::const_iterator const_iterator;
  typedef typename ht::const_local_iterator local_iterator;
  typedef typename ht::const_local_iterator const_local_iterator;


  // Iterator functions -- recall all iterators are const
  iterator begin() const                  { return rep.begin(); }
  iterator end() const                    { return rep.end(); }

  // These come from tr1's unordered_set. For us, a bucket has 0 or 1 elements.
  ABSL_DEPRECATED(
      "This method is slated for removal.  Please migrate to "
      "gtl::flat_hash_set.")
  local_iterator begin(size_type i) const { return rep.begin(i); }

  ABSL_DEPRECATED(
      "This method is slated for removal.  Please migrate to "
      "gtl::flat_hash_set.")
  local_iterator end(size_type i) const   { return rep.end(i); }


  // Accessor functions
  allocator_type get_allocator() const    { return rep.get_allocator(); }
  hasher hash_funct() const               { return rep.hash_funct(); }
  hasher hash_function() const            { return hash_funct(); }  // tr1 name
  key_equal key_eq() const                { return rep.key_eq(); }


  // Constructors
  dense_hash_set() {}

  explicit dense_hash_set(size_type expected_max_items_in_table,
                          const hasher& hf = hasher(),
                          const key_equal& eql = key_equal(),
                          const allocator_type& alloc = allocator_type())
      : rep(expected_max_items_in_table, hf, eql, Identity(), SetKey(), alloc) {
  }

  template <class InputIterator>
  dense_hash_set(InputIterator f, InputIterator l,
                 const key_type& empty_key_val,
                 size_type expected_max_items_in_table = 0,
                 const hasher& hf = hasher(),
                 const key_equal& eql = key_equal(),
                 const allocator_type& alloc = allocator_type())
      : rep(expected_max_items_in_table, hf, eql, Identity(), SetKey(), alloc) {
    set_empty_key(empty_key_val);
    rep.insert(f, l);
  }
  // We use the default copy constructor
  // We use the default operator=()
  // We use the default destructor

  void clear()                        { rep.clear(); }
  // This clears the hash set without resizing it down to the minimum
  // bucket count, but rather keeps the number of buckets constant
  void clear_no_resize()              { rep.clear_no_resize(); }
  void swap(dense_hash_set& hs)       { rep.swap(hs.rep); }


  // Functions concerning size
  size_type size() const              { return rep.size(); }
  size_type max_size() const          { return rep.max_size(); }
  bool empty() const                  { return rep.empty(); }
  size_type bucket_count() const      { return rep.bucket_count(); }

  ABSL_DEPRECATED(
      "This method is slated for removal.  Please migrate to "
      "gtl::flat_hash_set.")
  size_type max_bucket_count() const  { return rep.max_bucket_count(); }

  // These are tr1 methods.  bucket() is the bucket the key is or would be in.
  ABSL_DEPRECATED(
      "This method is slated for removal.  Please migrate to "
      "gtl::flat_hash_set.")
  size_type bucket_size(size_type i) const    { return rep.bucket_size(i); }
  ABSL_DEPRECATED(
      "This method is slated for removal.  Please migrate to "
      "gtl::flat_hash_set.")
  size_type bucket(const key_type& key) const { return rep.bucket(key); }
  float load_factor() const {
    return size() * 1.0f / bucket_count();
  }
  float max_load_factor() const {
    float shrink, grow;
    rep.get_resizing_parameters(&shrink, &grow);
    return grow;
  }
  void max_load_factor(float new_grow) {
    float shrink, grow;
    rep.get_resizing_parameters(&shrink, &grow);
    rep.set_resizing_parameters(shrink, new_grow);
  }
  // These aren't tr1 methods but perhaps ought to be.
  ABSL_DEPRECATED(
      "This method is slated for removal.  Please migrate to "
      "gtl::flat_hash_set.")
  float min_load_factor() const {
    float shrink, grow;
    rep.get_resizing_parameters(&shrink, &grow);
    return shrink;
  }
  void min_load_factor(float new_shrink) {
    float shrink, grow;
    rep.get_resizing_parameters(&shrink, &grow);
    rep.set_resizing_parameters(new_shrink, grow);
  }
  // Deprecated; use min_load_factor() or max_load_factor() instead.
  void set_resizing_parameters(float shrink, float grow) {
    rep.set_resizing_parameters(shrink, grow);
  }

  void resize(size_type hint)         { rep.resize(hint); }
  void rehash(size_type hint)         { resize(hint); }     // the tr1 name

  // Lookup routines
  iterator find(const key_type& key) const           { return rep.find(key); }
  size_type count(const key_type& key) const         { return rep.count(key); }
  std::pair<iterator, iterator> equal_range(const key_type& key) const {
    return rep.equal_range(key);
  }


  // Insertion routines
  std::pair<iterator, bool> insert(const value_type& obj) {
    std::pair<typename ht::iterator, bool> p = rep.insert(obj);
    return std::pair<iterator, bool>(p.first, p.second);   // const to non-const
  }
  std::pair<iterator, bool> insert(value_type&& obj) {  // NOLINT
    std::pair<typename ht::iterator, bool> p = rep.insert(std::move(obj));
    return std::pair<iterator, bool>(p.first, p.second);   // const to non-const
  }
  template <class InputIterator> void insert(InputIterator f, InputIterator l) {
    rep.insert(f, l);
  }
  void insert(const_iterator f, const_iterator l) {
    rep.insert(f, l);
  }
  // Required for std::insert_iterator; the passed-in iterator is ignored.
  iterator insert(iterator, const value_type& obj) {
    return insert(obj).first;
  }
  iterator insert(iterator, value_type&& obj) {  // NOLINT
    return insert(std::move(obj)).first;
  }

  // Unlike std::set, we cannot construct an element in place, as we do not have
  // a layer of indirection like std::set nodes. Therefore, emplace* methods do
  // not provide a performance advantage over insert + move.
  template <typename... Args>
  std::pair<iterator, bool> emplace(Args&&... args) {
    return rep.insert(value_type(std::forward<Args>(args)...));
  }
  // The passed-in const_iterator is ignored.
  template <typename... Args>
  iterator emplace_hint(const_iterator, Args&&... args) {
    return rep.insert(value_type(std::forward<Args>(args)...)).first;
  }

  // Deletion and empty routines
  // THESE ARE NON-STANDARD!  I make you specify an "impossible" key
  // value to identify deleted and empty buckets.  You can change the
  // deleted key as time goes on, or get rid of it entirely to be insert-only.
  void set_empty_key(const key_type& key)     { rep.set_empty_key(key); }
  void set_deleted_key(const key_type& key)   { rep.set_deleted_key(key); }

  // These are standard
  size_type erase(const key_type& key)               { return rep.erase(key); }
  void erase(iterator it)                            { rep.erase(it); }
  void erase(iterator f, iterator l)                 { rep.erase(f, l); }


  // Comparison
  bool operator==(const dense_hash_set& hs) const    { return rep == hs.rep; }
  bool operator!=(const dense_hash_set& hs) const    { return rep != hs.rep; }
};

template <class Val, class HashFcn, class EqualKey, class Alloc>
inline void swap(dense_hash_set<Val, HashFcn, EqualKey, Alloc>& hs1,
                 dense_hash_set<Val, HashFcn, EqualKey, Alloc>& hs2) {
  hs1.swap(hs2);
}

}

#endif  // S2_UTIL_GTL_DENSE_HASH_SET_H_
