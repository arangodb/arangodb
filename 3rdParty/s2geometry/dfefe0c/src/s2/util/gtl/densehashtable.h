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
// A dense hashtable is a particular implementation of
// a hashtable: one that is meant to minimize memory allocation.
// It does this by using an array to store all the data.  We
// steal a value from the key space to indicate "empty" array
// elements (ie indices where no item lives) and another to indicate
// "deleted" elements.
//
// (Note it is possible to change the value of the delete key
// on the fly; you can even remove it, though after that point
// the hashtable is insert_only until you set it again.  The empty
// value however can't be changed.)
//
// To minimize allocation and pointer overhead, we use internal
// probing, in which the hashtable is a single table, and collisions
// are resolved by trying to insert again in another bucket.  The
// most cache-efficient internal probing schemes are linear probing
// (which suffers, alas, from clumping) and quadratic probing, which
// is what we implement by default.
//
// Type requirements: value_type is required to be Move Constructible
// and Default Constructible. It is not required to be (and commonly
// isn't) Assignable.
//
// You probably shouldn't use this code directly.  Use dense_hash_map<>
// or dense_hash_set<> instead.

// You can change the following below:
// HT_OCCUPANCY_PCT      -- how full before we double size
// HT_EMPTY_PCT          -- how empty before we halve size
// HT_MIN_BUCKETS        -- default smallest bucket size
//
// You can also change enlarge_factor (which defaults to
// HT_OCCUPANCY_PCT), and shrink_factor (which defaults to
// HT_EMPTY_PCT) with set_resizing_parameters().
//
// How to decide what values to use?
// shrink_factor's default of .4 * OCCUPANCY_PCT, is probably good.
// HT_MIN_BUCKETS is probably unnecessary since you can specify
// (indirectly) the starting number of buckets at construct-time.
// For enlarge_factor, you can use this chart to try to trade-off
// expected lookup time to the space taken up.  By default, this
// code uses quadratic probing, though you can change it to linear
// via JUMP_ below if you really want to.
//
// From
// L = N / M,
// where N is the number of data items in the table and M is the table size.
// NUMBER OF PROBES / LOOKUP       Successful            Unsuccessful
// Quadratic collision resolution   1 - ln(1-L) - L/2    1/(1-L) - L - ln(1-L)
// Linear collision resolution     [1+1/(1-L)]/2         [1+1/(1-L)^2]/2
//
// -- enlarge_factor --           0.10  0.50  0.60  0.75  0.80  0.90  0.99
// QUADRATIC COLLISION RES.
//    probes/successful lookup    1.05  1.44  1.62  2.01  2.21  2.85  5.11
//    probes/unsuccessful lookup  1.11  2.19  2.82  4.64  5.81  11.4  103.6
// LINEAR COLLISION RES.
//    probes/successful lookup    1.06  1.5   1.75  2.5   3.0   5.5   50.5
//    probes/unsuccessful lookup  1.12  2.5   3.6   8.5   13.0  50.0  5000.0

#ifndef S2_UTIL_GTL_DENSEHASHTABLE_H_
#define S2_UTIL_GTL_DENSEHASHTABLE_H_

#include <cassert>
#include <cstddef>
#include <cstdio>              // for FILE, fwrite, fread
#include <algorithm>            // For swap(), eg
#include <functional>
#include <iterator>             // For iterator tags
#include <limits>               // for numeric_limits
#include <memory>               // For uninitialized_fill
#include <new>
#include <string>
#include <utility>
#include <vector>
#include <type_traits>

#include "s2/util/gtl/hashtable_common.h"
#include "s2/util/gtl/libc_allocator_with_realloc.h"
#include "s2/base/port.h"
#include <stdexcept>                 // For length_error

namespace gtl {


// Some files test for this symbol.
#define S2__DENSEHASHTABLE_H_

// The probing method
// Linear probing
// #define JUMP_(key, num_probes)    ( 1 )
// Quadratic probing
#define JUMP_(key, num_probes)    (num_probes)

// The weird mod in the offset is entirely to quiet compiler warnings
// as is the cast to int after doing the "x mod 256"
#define PUT_(take_from, offset)  do {                                    \
    if (putc(static_cast<int>(offset >= sizeof(take_from)*8)             \
                              ? 0 : ((take_from) >> (offset)) % 256, fp) \
        == EOF)                                                          \
    return false;                                                        \
} while (0)

#define GET_(add_to, offset)  do {                                      \
  if ((x=getc(fp)) == EOF)                                              \
    return false;                                                       \
  else if (offset >= sizeof(add_to) * 8)                                \
    assert(x == 0);   /* otherwise it's too big for us to represent */  \
  else                                                                  \
    add_to |= (static_cast<size_type>(x) << ((offset) % (sizeof(add_to)*8))); \
} while (0)


// Hashtable class, used to implement the hashed associative containers
// hash_set and hash_map.

// Value: what is stored in the table (each bucket is a Value).
// Key: something in a 1-to-1 correspondence to a Value, that can be used
//      to search for a Value in the table (find() takes a Key).
// HashFcn: Takes a Key and returns an integer, the more unique the better.
// ExtractKey: given a Value, returns the unique Key associated with it.
//             Must have a result_type enum indicating the return type of
//             operator().
// SetKey: given a Value* and a Key, modifies the value such that
//         ExtractKey(value) == key.  We guarantee this is only called
//         with key == deleted_key or key == empty_key.
// EqualKey: Given two Keys, says whether they are the same (that is,
//           if they are both associated with the same Value).
// Alloc: STL allocator to use to allocate memory.

template <class Value, class Key, class HashFcn,
          class ExtractKey, class SetKey, class EqualKey, class Alloc>
class dense_hashtable;

template <class V, class K, class HF, class ExK, class SetK, class EqK, class A>
struct dense_hashtable_const_iterator;

// We're just an array, but we need to skip over empty and deleted elements
template <class V, class K, class HF, class ExK, class SetK, class EqK, class A>
struct dense_hashtable_iterator {
 private:
  typedef typename A::template rebind<V>::other value_alloc_type;

 public:
  typedef dense_hashtable_iterator<V, K, HF, ExK, SetK, EqK, A>
      iterator;
  typedef dense_hashtable_const_iterator<V, K, HF, ExK, SetK, EqK, A>
      const_iterator;

  typedef std::forward_iterator_tag iterator_category;  // very little defined!
  typedef V value_type;
  typedef typename value_alloc_type::difference_type difference_type;
  typedef typename value_alloc_type::size_type size_type;
  typedef typename value_alloc_type::reference reference;
  typedef typename value_alloc_type::pointer pointer;

  // "Real" constructor and default constructor
  dense_hashtable_iterator(
      const dense_hashtable<V, K, HF, ExK, SetK, EqK, A> *h,
      pointer it, pointer it_end, bool advance)
    : ht(h), pos(it), end(it_end)   {
    if (advance)  advance_past_empty_and_deleted();
  }
  dense_hashtable_iterator() { }
  // The default destructor is fine; we don't define one
  // The default operator= is fine; we don't define one

  // Happy dereferencer
  reference operator*() const { return *pos; }
  pointer operator->() const { return &(operator*()); }

  // Arithmetic.  The only hard part is making sure that
  // we're not on an empty or marked-deleted array element
  void advance_past_empty_and_deleted() {
    while (pos != end && (ht->test_empty(*this) || ht->test_deleted(*this)) )
      ++pos;
  }
  iterator& operator++() {
    assert(pos != end);
    ++pos;
    advance_past_empty_and_deleted();
    return *this;
  }
  iterator operator++(int /*unused*/) {
    auto tmp(*this);
    ++*this;
    return tmp;
  }

  // Comparison.
  bool operator==(const iterator& it) const { return pos == it.pos; }
  bool operator!=(const iterator& it) const { return pos != it.pos; }


  // The actual data
  const dense_hashtable<V, K, HF, ExK, SetK, EqK, A> *ht;
  pointer pos, end;
};


// Now do it all again, but with const-ness!
template <class V, class K, class HF, class ExK, class SetK, class EqK, class A>
struct dense_hashtable_const_iterator {
 private:
  typedef typename A::template rebind<V>::other value_alloc_type;

 public:
  typedef dense_hashtable_iterator<V, K, HF, ExK, SetK, EqK, A>
      iterator;
  typedef dense_hashtable_const_iterator<V, K, HF, ExK, SetK, EqK, A>
      const_iterator;

  typedef std::forward_iterator_tag iterator_category;  // very little defined!
  typedef V value_type;
  typedef typename value_alloc_type::difference_type difference_type;
  typedef typename value_alloc_type::size_type size_type;
  typedef typename value_alloc_type::const_reference reference;
  typedef typename value_alloc_type::const_pointer pointer;

  // "Real" constructor and default constructor
  dense_hashtable_const_iterator(
      const dense_hashtable<V, K, HF, ExK, SetK, EqK, A> *h,
      pointer it, pointer it_end, bool advance)
    : ht(h), pos(it), end(it_end)   {
    if (advance)  advance_past_empty_and_deleted();
  }
  dense_hashtable_const_iterator()
    : ht(nullptr), pos(pointer()), end(pointer()) { }
  // This lets us convert regular iterators to const iterators
  dense_hashtable_const_iterator(const iterator &it)
    : ht(it.ht), pos(it.pos), end(it.end) { }
  // The default destructor is fine; we don't define one
  // The default operator= is fine; we don't define one

  // Happy dereferencer
  reference operator*() const { return *pos; }
  pointer operator->() const { return &(operator*()); }

  // Arithmetic.  The only hard part is making sure that
  // we're not on an empty or marked-deleted array element
  void advance_past_empty_and_deleted() {
    while (pos != end && (ht->test_empty(*this) || ht->test_deleted(*this)))
      ++pos;
  }
  const_iterator& operator++()   {
    assert(pos != end);
    ++pos;
    advance_past_empty_and_deleted();
    return *this;
  }
  const_iterator operator++(int /*unused*/) {
    auto tmp(*this);
    ++*this;
    return tmp;
  }

  // Comparison.
  bool operator==(const const_iterator& it) const { return pos == it.pos; }
  bool operator!=(const const_iterator& it) const { return pos != it.pos; }


  // The actual data
  const dense_hashtable<V, K, HF, ExK, SetK, EqK, A> *ht;
  pointer pos, end;
};

template <class Value, class Key, class HashFcn,
          class ExtractKey, class SetKey, class EqualKey, class Alloc>
class dense_hashtable {
 private:
  typedef typename Alloc::template rebind<Value>::other value_alloc_type;


 public:
  typedef Key key_type;
  typedef Value value_type;
  typedef HashFcn hasher;
  typedef EqualKey key_equal;
  typedef Alloc allocator_type;

  typedef typename value_alloc_type::size_type size_type;
  typedef typename value_alloc_type::difference_type difference_type;
  typedef typename value_alloc_type::reference reference;
  typedef typename value_alloc_type::const_reference const_reference;
  typedef typename value_alloc_type::pointer pointer;
  typedef typename value_alloc_type::const_pointer const_pointer;
  typedef dense_hashtable_iterator<Value, Key, HashFcn,
                                   ExtractKey, SetKey, EqualKey, Alloc>
  iterator;

  typedef dense_hashtable_const_iterator<Value, Key, HashFcn,
                                         ExtractKey, SetKey, EqualKey, Alloc>
  const_iterator;

  // These come from tr1.  For us they're the same as regular iterators.
  typedef iterator local_iterator;
  typedef const_iterator const_local_iterator;


  // How full we let the table get before we resize, by default.
  // Knuth says .8 is good -- higher causes us to probe too much,
  // though it saves memory.
  static const int HT_OCCUPANCY_PCT;  // defined at the bottom of this file

  // How empty we let the table get before we resize lower, by default.
  // (0.0 means never resize lower.)
  // It should be less than OCCUPANCY_PCT / 2 or we thrash resizing
  static const int HT_EMPTY_PCT;      // defined at the bottom of this file

  // Minimum size we're willing to let hashtables be.
  // Must be a power of two, and at least 4.
  // Note, however, that for a given hashtable, the initial size is a
  // function of the first constructor arg, and may be >HT_MIN_BUCKETS.
  static const size_type HT_MIN_BUCKETS = 4;

  // By default, if you don't specify a hashtable size at
  // construction-time, we use this size.  Must be a power of two, and
  // at least HT_MIN_BUCKETS.
  static const size_type HT_DEFAULT_STARTING_BUCKETS = 32;

  // ITERATOR FUNCTIONS
  iterator begin() {
    return iterator(this, table, table + num_buckets, true);
  }
  iterator end() {
    return iterator(this, table + num_buckets, table + num_buckets, true);
  }
  const_iterator begin() const {
    return const_iterator(this, table, table+num_buckets, true);
  }
  const_iterator end() const   {
    return const_iterator(this, table + num_buckets, table+num_buckets, true);
  }

  // These come from tr1 unordered_map.  They iterate over 'bucket' n.
  // We'll just consider bucket n to be the n-th element of the table.
  local_iterator begin(size_type i) {
    return local_iterator(this, table + i, table + i+1, false);
  }
  local_iterator end(size_type i) {
    local_iterator it = begin(i);
    if (!test_empty(i) && !test_deleted(i))
      ++it;
    return it;
  }
  const_local_iterator begin(size_type i) const {
    return const_local_iterator(this, table + i, table + i+1, false);
  }
  const_local_iterator end(size_type i) const {
    const_local_iterator it = begin(i);
    if (!test_empty(i) && !test_deleted(i))
      ++it;
    return it;
  }

  // ACCESSOR FUNCTIONS for the things we templatize on, basically
  hasher hash_funct() const               { return settings; }
  key_equal key_eq() const                { return key_info; }
  value_alloc_type get_allocator() const { return key_info; }

  // Accessor function for statistics gathering.
  int num_table_copies() const { return settings.num_ht_copies(); }

 private:
  // Annoyingly, we can't copy values around, because they might have
  // const components (they're probably std::pair<const X, Y>).  We use
  // explicit destructor invocation and placement new to get around
  // this.  Arg.
  static void set_value(pointer dst, const_reference src) {
    dst->~value_type();   // delete the old value, if any
    new(dst) value_type(src);
  }

  static void set_value(pointer dst, value_type&& src) {  // NOLINT
    dst->~value_type();
    new(dst) value_type(std::move(src));
  }

  void destroy_buckets(size_type first, size_type last) {
    for ( ; first != last; ++first)
      table[first].~value_type();
  }

  // DELETE HELPER FUNCTIONS
  // This lets the user describe a key that will indicate deleted
  // table entries.  This key should be an "impossible" entry --
  // if you try to insert it for real, you won't be able to retrieve it!
  // (NB: while you pass in an entire value, only the key part is looked
  // at.  This is just because I don't know how to assign just a key.)
 private:
  // Gets rid of any deleted entries we have.
  void squash_deleted() {
    if (num_deleted > 0) {
      rebucket(settings.min_buckets(size(), num_buckets));
    }
    assert(num_deleted == 0);
  }

  // Test if the given key is the deleted indicator.  Requires
  // num_deleted > 0, for correctness of read(), and because that
  // guarantees that key_info.delkey is valid.
  bool test_deleted_key(const key_type& key) const {
    assert(num_deleted > 0);
    return equals(key_info.delkey, key);
  }

 public:
  void set_deleted_key(const key_type &key) {
    // the empty indicator (if specified) and the deleted indicator
    // must be different
    assert((!settings.use_empty() || !equals(key, key_info.empty)) &&
           "Passed the empty-key to set_deleted_key");
    // It's only safe to change what "deleted" means if we purge deleted guys
    squash_deleted();
    settings.set_use_deleted(true);
    key_info.delkey = key;
  }
  key_type deleted_key() const {
    assert(settings.use_deleted()
           && "Must set deleted key before calling deleted_key");
    return key_info.delkey;
  }

  // These are public so the iterators can use them
  // True if the item at position bucknum is "deleted" marker
  bool test_deleted(size_type bucknum) const {
    // Invariant: !use_deleted() implies num_deleted is 0.
    assert(settings.use_deleted() || num_deleted == 0);
    return num_deleted > 0 && test_deleted_key(get_key(table[bucknum]));
  }
  bool test_deleted(const iterator &it) const {
    // Invariant: !use_deleted() implies num_deleted is 0.
    assert(settings.use_deleted() || num_deleted == 0);
    return num_deleted > 0 && test_deleted_key(get_key(*it));
  }
  bool test_deleted(const const_iterator &it) const {
    // Invariant: !use_deleted() implies num_deleted is 0.
    assert(settings.use_deleted() || num_deleted == 0);
    return num_deleted > 0 && test_deleted_key(get_key(*it));
  }

 private:
  void check_use_deleted(const char* caller) {
    (void)caller;    // could log it if the assert failed
    assert(settings.use_deleted());
  }

  // Write the deleted key to the position specified.
  // Requires: !test_deleted(it)
  void set_deleted(iterator &it) {
    check_use_deleted("set_deleted()");
    assert(!test_deleted(it));
    // &* converts from iterator to value-type.
    set_key(&(*it), key_info.delkey);
  }

  // We also allow to set/clear the deleted bit on a const iterator.
  // We allow a const_iterator for the same reason you can delete a
  // const pointer: it's convenient, and semantically you can't use
  // 'it' after it's been deleted anyway, so its const-ness doesn't
  // really matter.
  // Requires: !test_deleted(it)
  void set_deleted(const_iterator &it) {
    check_use_deleted("set_deleted()");
    assert(!test_deleted(it));
    set_key(const_cast<pointer>(&(*it)), key_info.delkey);
  }

  // EMPTY HELPER FUNCTIONS
  // This lets the user describe a key that will indicate empty (unused)
  // table entries.  This key should be an "impossible" entry --
  // if you try to insert it for real, you won't be able to retrieve it!
  // (NB: while you pass in an entire value, only the key part is looked
  // at.  This is just because I don't know how to assign just a key.)
 public:
  // These are public so the iterators can use them
  // True if the item at position bucknum is "empty" marker
  bool test_empty(size_type bucknum) const {
    assert(settings.use_empty());  // we always need to know what's empty!
    return equals(key_info.empty, get_key(table[bucknum]));
  }
  bool test_empty(const iterator &it) const {
    assert(settings.use_empty());  // we always need to know what's empty!
    return equals(key_info.empty, get_key(*it));
  }
  bool test_empty(const const_iterator &it) const {
    assert(settings.use_empty());  // we always need to know what's empty!
    return equals(key_info.empty, get_key(*it));
  }

 private:
  bool test_empty(size_type bucknum, const_pointer table) const {
    assert(settings.use_empty());
    return equals(key_info.empty, get_key(table[bucknum]));
  }

  void fill_range_with_empty(pointer table_start, pointer table_end) {
    for (; table_start != table_end; ++table_start) {
      new (table_start) value_type();
      set_key(table_start, key_info.empty);
    }
  }

 public:
  // TODO(user): change all callers of this to pass in a key instead,
  //                 and take a const key_type instead of const value_type.
  void set_empty_key(const_reference val) {
    // Once you set the empty key, you can't change it
    assert(!settings.use_empty() && "Calling set_empty_key multiple times");
    // The deleted indicator (if specified) and the empty indicator
    // must be different.
    const key_type& key = get_key(val);
    assert((!settings.use_deleted() || !equals(key, key_info.delkey)) &&
           "Setting the empty key the same as the deleted key");
    settings.set_use_empty(true);
    key_info.empty.~key_type();
    new (&key_info.empty) key_type(key);

    assert(!table);                  // must set before first use
    // num_buckets was set in constructor even though table was nullptr
    table = get_internal_allocator().allocate(num_buckets);
    fill_range_with_empty(table, table + num_buckets);
  }
  // TODO(user): this should return the key by const reference.
  value_type empty_key() const {
    assert(settings.use_empty());
    value_type ret = value_type();
    set_key(&ret, key_info.empty);
    return ret;
  }

  // FUNCTIONS CONCERNING SIZE
 public:
  size_type size() const      { return num_elements - num_deleted; }
  size_type max_size() const { return get_allocator().max_size(); }
  bool empty() const          { return size() == 0; }
  size_type bucket_count() const      { return num_buckets; }
  size_type max_bucket_count() const  { return max_size(); }
  size_type nonempty_bucket_count() const { return num_elements; }
  // These are tr1 methods.  Their idea of 'bucket' doesn't map well to
  // what we do.  We just say every bucket has 0 or 1 items in it.
  size_type bucket_size(size_type i) const {
    return begin(i) == end(i) ? 0 : 1;
  }

 private:
  // Because of the above, size_type(-1) is never legal; use it for errors
  static const size_type ILLEGAL_BUCKET = size_type(-1);

  // Used after a string of deletes.  Returns true if we actually shrunk.
  // TODO(user): take a delta so we can take into account inserts
  // done after shrinking.  Maybe make part of the Settings class?
  bool maybe_shrink() {
    assert(num_elements >= num_deleted);
    assert((bucket_count() & (bucket_count()-1)) == 0);  // is a power of two
    assert(bucket_count() >= HT_MIN_BUCKETS);
    bool retval = false;

    // If you construct a hashtable with < HT_DEFAULT_STARTING_BUCKETS,
    // we'll never shrink until you get relatively big, and we'll never
    // shrink below HT_DEFAULT_STARTING_BUCKETS.  Otherwise, something
    // like "dense_hash_set<int> x; x.insert(4); x.erase(4);" will
    // shrink us down to HT_MIN_BUCKETS buckets, which is too small.
    const size_type num_remain = num_elements - num_deleted;
    const size_type shrink_threshold = settings.shrink_threshold();
    if (shrink_threshold > 0 && num_remain < shrink_threshold &&
        bucket_count() > HT_DEFAULT_STARTING_BUCKETS) {
      const float shrink_factor = settings.shrink_factor();
      size_type sz = bucket_count() / 2;    // find how much we should shrink
      while (sz > HT_DEFAULT_STARTING_BUCKETS &&
             num_remain < sz * shrink_factor) {
        sz /= 2;                            // stay a power of 2
      }
      rebucket(sz);
      retval = true;
    }
    settings.set_consider_shrink(false);    // because we just considered it
    return retval;
  }

  // We'll let you resize a hashtable -- though this makes us copy all!
  // When you resize, you say, "make it big enough for this many more elements"
  // Returns true if we actually resized, false if size was already ok.
  bool resize_delta(size_type delta) {
    bool did_resize = false;
    if (settings.consider_shrink()) {  // see if lots of deletes happened
      if (maybe_shrink())
        did_resize = true;
    }
    if (num_elements >= std::numeric_limits<size_type>::max() - delta) {
      throw std::length_error("resize overflow");
    }

    assert(settings.enlarge_threshold() < bucket_count());
    // Check if our work is done.
    if (bucket_count() >= HT_MIN_BUCKETS &&
        num_elements + delta <= settings.enlarge_threshold()) {
      return did_resize;
    }

    // Sometimes, we need to resize just to get rid of all the
    // "deleted" buckets that are clogging up the hashtable.  So when
    // deciding whether to resize, count the deleted buckets (which
    // are currently taking up room).  But later, when we decide what
    // size to resize to, *don't* count deleted buckets, since they
    // get discarded during the resize.
    const size_type needed_size = settings.min_buckets(num_elements + delta, 0);
    if (needed_size <= bucket_count())      // we have enough buckets
      return did_resize;

    // We will rebucket.
    size_type resize_to =
      settings.min_buckets(num_elements - num_deleted + delta, bucket_count());

    if (resize_to < needed_size) {
      // This situation means that we have enough deleted elements,
      // that once we purge them, we won't actually have needed to
      // grow.  But we may want to grow anyway: if we just purge one
      // element, say, we'll have to grow anyway next time we
      // insert.  Might as well grow now, since we're already going
      // through the trouble of rebucketing in order to purge the
      // deleted elements.  (Safety note: Can resize_to * 2 overflow? No.
      // The output of min_buckets() is always a power of two, so resize_to
      // and needed_size are powers of two. That plus resize_to < needed_size
      // proves that overflow isn't a concern.)
      const size_type target =
          static_cast<size_type>(settings.shrink_size(resize_to*2));
      if (num_elements - num_deleted + delta >= target) {
        // Good, we won't be below the shrink threshhold even if we double.
        resize_to *= 2;
      }
    }
    rebucket(resize_to);
    return true;
  }

  // We require table be non-null and empty before calling this.
  void resize_table(size_type /*old_size*/, size_type new_size,
                    std::true_type) {
    table = get_internal_allocator().realloc_or_die(table, new_size);
  }

  void resize_table(size_type old_size, size_type new_size, std::false_type) {
    get_internal_allocator().deallocate(table, old_size);
    table = get_internal_allocator().allocate(new_size);
  }

  // Copy (or, if Iter is a move_iterator, move) the elements from
  // [src_first, src_last) into dest_table, which we assume has size
  // dest_bucket_count and has been initialized to the empty key.
  template<class Iter>
  void copy_elements(Iter src_first, Iter src_last, pointer dest_table,
                     size_type dest_bucket_count) {
    assert((dest_bucket_count & (dest_bucket_count - 1)) == 0);  // a power of 2
    // We use a normal iterator to get non-deleted bcks from ht
    // We could use insert() here, but since we know there are
    // no duplicates and no deleted items, we can be more efficient
    for (; src_first != src_last; ++src_first) {
      size_type num_probes = 0;               // how many times we've probed
      size_type bucknum;
      const size_type bucket_count_minus_one = dest_bucket_count - 1;
      for (bucknum = hash(get_key(*src_first)) & bucket_count_minus_one;
           !test_empty(bucknum, dest_table);  // not empty
           bucknum = (bucknum +
                      JUMP_(key, num_probes)) & bucket_count_minus_one) {
        ++num_probes;
        assert(num_probes < dest_bucket_count
               && "Hashtable is full: an error in key_equal<> or hash<>");
      }
      // Copies or moves the value into dest_table.
      set_value(&dest_table[bucknum], *src_first);
    }
  }

  // Used to actually do the rehashing when we grow/shrink a hashtable
  void copy_from(const dense_hashtable &ht, size_type min_buckets_wanted) {
    size_type size = ht.size();  // clear_to_size() sets ht.size() to 0.
    clear_to_size(settings.min_buckets(ht.size(), min_buckets_wanted));
    copy_elements(ht.begin(), ht.end(), table, bucket_count());
    num_elements = size;
    settings.inc_num_ht_copies();
  }

  // Rebuckets and resizes the hashtable.  Gets rid of any deleted entries.
  void rebucket(size_type new_num_buckets) {
    if (table == nullptr) {
      // When we eventually allocate the table, it will have this many buckets.
      num_buckets = new_num_buckets;
      return;
    }
    assert(settings.use_empty());
    assert((new_num_buckets & (new_num_buckets - 1)) == 0);  // a power of two
    // If settings.shrink_factor() is zero then we must not shrink.
    assert(settings.shrink_factor() > 0 || new_num_buckets >= num_buckets);
    pointer new_table = get_internal_allocator().allocate(new_num_buckets);

    fill_range_with_empty(new_table, new_table + new_num_buckets);
    copy_elements(std::make_move_iterator(begin()),
                  std::make_move_iterator(end()),
                  new_table, new_num_buckets);

    destroy_buckets(0, num_buckets);  // Destroy table's elements.
    get_internal_allocator().deallocate(table, num_buckets);

    table = new_table;
    num_buckets = new_num_buckets;
    assert(num_elements >= num_deleted);
    num_elements -= num_deleted;
    num_deleted = 0;
    settings.reset_thresholds(bucket_count());
    settings.inc_num_ht_copies();
  }

  // Required by the spec for hashed associative container
 public:
  // Though the docs say this should be num_buckets, I think it's much
  // more useful as num_elements.  As a special feature, calling with
  // req_elements==0 will cause us to shrink if we can, saving space.
  void resize(size_type req_elements) {       // resize to this or larger
    if ( settings.consider_shrink() || req_elements == 0 )
      maybe_shrink();
    if ( req_elements > num_elements )
      resize_delta(req_elements - num_elements);
  }

  // Get and change the value of shrink_factor and enlarge_factor.  The
  // description at the beginning of this file explains how to choose
  // the values.  Setting the shrink parameter to 0.0 ensures that the
  // table never shrinks.
  void get_resizing_parameters(float* shrink, float* grow) const {
    *shrink = settings.shrink_factor();
    *grow = settings.enlarge_factor();
  }
  void set_resizing_parameters(float shrink, float grow) {
    settings.set_resizing_parameters(shrink, grow);
    settings.reset_thresholds(bucket_count());
  }

  // CONSTRUCTORS -- as required by the specs, we take a size,
  // but also let you specify a hashfunction, key comparator,
  // and key extractor.  We also define a copy constructor and =.
  // DESTRUCTOR -- needs to free the table
  explicit dense_hashtable(size_type expected_max_items_in_table = 0,
                           const HashFcn& hf = HashFcn(),
                           const EqualKey& eql = EqualKey(),
                           const ExtractKey& ext = ExtractKey(),
                           const SetKey& set = SetKey(),
                           const Alloc& alloc = Alloc())
      : settings(hf),
        key_info(ext, set, eql, alloc_impl<value_alloc_type>(alloc)),
        num_deleted(0),
        num_elements(0),
        num_buckets(expected_max_items_in_table == 0
                        ? HT_DEFAULT_STARTING_BUCKETS
                        : settings.min_buckets(expected_max_items_in_table, 0)),
        table(nullptr) {
    // table is nullptr until the empty key is set. However, we set num_buckets
    // here so we know how much space to allocate once the empty key is set.
    settings.reset_thresholds(bucket_count());
  }

  // As a convenience for resize(), we allow an optional second argument
  // which lets you make this new hashtable a different size than ht
  dense_hashtable(const dense_hashtable& ht,
                  size_type min_buckets_wanted = HT_DEFAULT_STARTING_BUCKETS)
      : settings(ht.settings),
        key_info(ht.key_info.as_extract_key(), ht.key_info.as_set_key(),
                 ht.key_info.as_equal_key(),
                 alloc_impl<value_alloc_type>(
                     std::allocator_traits<value_alloc_type>::
                         select_on_container_copy_construction(
                             ht.key_info.as_value_alloc()))),
        num_deleted(0),
        num_elements(0),
        num_buckets(0),
        table(nullptr) {
    key_info.delkey = ht.key_info.delkey;
    key_info.empty = ht.key_info.empty;
    if (!ht.settings.use_empty()) {
      // If use_empty isn't set, copy_from will crash, so we do our own copying.
      assert(ht.empty());
      num_buckets = settings.min_buckets(ht.size(), min_buckets_wanted);
      settings.reset_thresholds(bucket_count());
      return;
    }
    settings.reset_thresholds(bucket_count());
    copy_from(ht, min_buckets_wanted);   // copy_from() ignores deleted entries
  }

  dense_hashtable& operator=(const dense_hashtable& ht) {
    if (&ht == this)  return *this;        // don't copy onto ourselves
    settings = ht.settings;
    key_info.as_extract_key() = ht.key_info.as_extract_key();
    key_info.as_set_key() = ht.key_info.as_set_key();
    key_info.as_equal_key() = ht.key_info.as_equal_key();
    if (std::allocator_traits<
            value_alloc_type>::propagate_on_container_copy_assignment::value) {
      // If we're about to overwrite our allocator, we need to free all
      // memory using our old allocator.
      if (key_info.as_value_alloc() != ht.key_info.as_value_alloc()) {
        destroy_table();
      }
      static_cast<alloc_impl<value_alloc_type>&>(key_info) =
          static_cast<const alloc_impl<value_alloc_type>&>(ht.key_info);
    }
    key_info.empty = ht.key_info.empty;
    key_info.delkey = ht.key_info.delkey;

    if (ht.settings.use_empty()) {
      // copy_from() calls clear and sets num_deleted to 0 too
      copy_from(ht, HT_MIN_BUCKETS);
    } else {
      assert(ht.empty());
      destroy_table();
    }

    // we purposefully don't copy the allocator, which may not be copyable
    return *this;
  }

  dense_hashtable(dense_hashtable&& ht)
      : settings(std::move(ht.settings)),
        key_info(std::move(ht.key_info)),
        num_deleted(ht.num_deleted),
        num_elements(ht.num_elements),
        num_buckets(ht.num_buckets),
        table(ht.table) {
    ht.num_deleted = 0;
    ht.num_elements = 0;
    ht.table = nullptr;
    ht.num_buckets = HT_DEFAULT_STARTING_BUCKETS;
    ht.settings.set_use_empty(false);
    ht.settings.set_use_deleted(false);
  }

  dense_hashtable& operator=(dense_hashtable&& ht) {
    if (&ht == this) return *this;        // don't move onto ourselves

    const bool can_move_table =
        std::allocator_traits<
            Alloc>::propagate_on_container_move_assignment::value ||
        key_info.as_value_alloc() == ht.key_info.as_value_alloc();

    // First, deallocate with this's allocator.
    destroy_table();

    if (std::allocator_traits<
            value_alloc_type>::propagate_on_container_move_assignment::value) {
      // This moves the allocator.
      key_info = std::move(ht.key_info);
    } else {
      // Move all other base classes of key_info from ht, but don't move the
      // allocator.
      key_info.as_extract_key() = std::move(ht.key_info.as_extract_key());
      key_info.as_set_key() = std::move(ht.key_info.as_set_key());
      key_info.as_equal_key() = std::move(ht.key_info.as_equal_key());
      key_info.delkey = std::move(ht.key_info.delkey);
      key_info.empty = std::move(ht.key_info.empty);
    }

    settings = std::move(ht.settings);
    num_deleted = ht.num_deleted;
    ht.num_deleted = 0;
    num_elements = ht.num_elements;
    ht.num_elements = 0;
    num_buckets = ht.num_buckets;
    ht.num_buckets = HT_DEFAULT_STARTING_BUCKETS;
    ht.settings.set_use_empty(false);
    ht.settings.set_use_deleted(false);

    if (can_move_table) {
      // We can transfer ownership of the table from ht to this because either
      // we're propagating the allocator or ht's allocator is equal to this's.
      table = ht.table;
      ht.table = nullptr;
    } else if (ht.table) {
      // We can't transfer ownership of any memory from ht to this, so the
      // best we can do is move element-by-element.
      table = get_internal_allocator().allocate(num_buckets);
      for (size_type i = 0; i < num_buckets; ++i) {
        new(table + i) Value(std::move(ht.table[i]));
      }
      ht.destroy_table();
    }

    return *this;
  }

  ~dense_hashtable() { destroy_table(); }

  // Many STL algorithms use swap instead of copy constructors
  void swap(dense_hashtable& ht) {
    if (this == &ht) return;  // swap with self.
    using std::swap;
    swap(settings, ht.settings);
    // Swap everything in key_info but the allocator.
    swap(key_info.as_extract_key(), ht.key_info.as_extract_key());
    swap(key_info.as_set_key(), ht.key_info.as_set_key());
    swap(key_info.as_equal_key(), ht.key_info.as_equal_key());
    if (std::allocator_traits<
            value_alloc_type>::propagate_on_container_swap::value) {
      swap(static_cast<alloc_impl<value_alloc_type>&>(key_info),
           static_cast<alloc_impl<value_alloc_type>&>(ht.key_info));
    } else {
      // Swapping when allocators are unequal and
      // propagate_on_container_swap is false is undefined behavior.
      S2_CHECK(key_info.as_value_alloc() == ht.key_info.as_value_alloc());
    }
    swap(key_info.empty, ht.key_info.empty);
    swap(key_info.delkey, ht.key_info.delkey);
    swap(num_deleted, ht.num_deleted);
    swap(num_elements, ht.num_elements);
    swap(num_buckets, ht.num_buckets);
    swap(table, ht.table);
  }

 private:
  void destroy_table() {
    if (table) {
      destroy_buckets(0, num_buckets);
      get_internal_allocator().deallocate(table, num_buckets);
      table = nullptr;
    }
  }

  void clear_to_size(size_type new_num_buckets) {
    if (!table) {
      table = get_internal_allocator().allocate(new_num_buckets);
    } else {
      destroy_buckets(0, num_buckets);
      if (new_num_buckets != num_buckets) {   // resize, if necessary
        typedef std::integral_constant<bool,
            std::is_same<value_alloc_type,
                         libc_allocator_with_realloc<value_type> >::value>
            realloc_ok;
        resize_table(num_buckets, new_num_buckets, realloc_ok());
      }
    }
    assert(table);
    fill_range_with_empty(table, table + new_num_buckets);
    num_elements = 0;
    num_deleted = 0;
    num_buckets = new_num_buckets;          // our new size
    settings.reset_thresholds(bucket_count());
  }

 public:
  // It's always nice to be able to clear a table without deallocating it
  void clear() {
    // If the table is already empty, and the number of buckets is
    // already as we desire, there's nothing to do.
    const size_type new_num_buckets = settings.min_buckets(0, 0);
    if (num_elements == 0 && new_num_buckets == num_buckets) {
      return;
    }
    clear_to_size(new_num_buckets);
  }

  // Clear the table without resizing it.
  // Mimicks the stl_hashtable's behaviour when clear()-ing in that it
  // does not modify the bucket count
  void clear_no_resize() {
    if (num_elements > 0) {
      assert(table);
      destroy_buckets(0, num_buckets);
      fill_range_with_empty(table, table + num_buckets);
    }
    // don't consider to shrink before another erase()
    settings.reset_thresholds(bucket_count());
    num_elements = 0;
    num_deleted = 0;
  }

  // LOOKUP ROUTINES
 private:
  template <class K>
  void assert_key_is_not_empty_or_deleted(const K& key) const {
    assert(settings.use_empty() && "set_empty_key() was not called");
    assert(!equals(key, key_info.empty) &&
           "Using the empty key as a regular key");
    assert((!settings.use_deleted() || !equals(key, key_info.delkey))
           && "Using the deleted key as a regular key");
  }

  template <class K>
  std::pair<size_type, size_type> find_position(const K& key) const {
    return find_position_using_hash(hash(key), key);
  }

  // Returns a pair of positions: 1st where the object is, 2nd where
  // it would go if you wanted to insert it.  1st is ILLEGAL_BUCKET
  // if object is not found; 2nd is ILLEGAL_BUCKET if it is.
  // Note: because of deletions where-to-insert is not trivial: it's the
  // first deleted bucket we see, as long as we don't find the key later
  template <class K>
  std::pair<size_type, size_type> find_position_using_hash(
      const size_type key_hash, const K& key) const {
    assert_key_is_not_empty_or_deleted(key);
    size_type num_probes = 0;              // how many times we've probed
    const size_type bucket_count_minus_one = bucket_count() - 1;
    size_type bucknum = key_hash & bucket_count_minus_one;
    size_type insert_pos = ILLEGAL_BUCKET;  // where we would insert
    while (1) {                             // probe until something happens
      if (test_empty(bucknum)) {            // bucket is empty
        if (insert_pos == ILLEGAL_BUCKET)   // found no prior place to insert
          return std::pair<size_type, size_type>(ILLEGAL_BUCKET, bucknum);
        else
          return std::pair<size_type, size_type>(ILLEGAL_BUCKET, insert_pos);

      } else if (test_deleted(bucknum)) {
        // keep searching, but mark to insert
        if ( insert_pos == ILLEGAL_BUCKET )
          insert_pos = bucknum;

      } else if (equals(key, get_key(table[bucknum]))) {
        return std::pair<size_type, size_type>(bucknum, ILLEGAL_BUCKET);
      }
      ++num_probes;                        // we're doing another probe
      bucknum = (bucknum + JUMP_(key, num_probes)) & bucket_count_minus_one;
      assert(num_probes < bucket_count()
             && "Hashtable is full: an error in key_equal<> or hash<>");
    }
  }

  template <class K>
  std::pair<size_type, bool> find_if_present(const K& key) const {
    return find_if_present_using_hash(hash(key), key);
  }

  // Return where the key is (if at all), and if it is present.  If
  // the key isn't present then the first part of the return value is
  // undefined.  The same information can be extracted from the result
  // of find_position(), but that tends to be slower in practice.
  template <class K>
  std::pair<size_type, bool> find_if_present_using_hash(
      const size_type key_hash, const K& key) const {
    assert_key_is_not_empty_or_deleted(key);
    size_type num_probes = 0;              // how many times we've probed
    const size_type bucket_count_minus_one = bucket_count() - 1;
    size_type bucknum = key_hash & bucket_count_minus_one;
    while (1) {                             // probe until something happens
      if (equals(key, get_key(table[bucknum]))) {
        return std::pair<size_type, bool>(bucknum, true);
      } else if (test_empty(bucknum)) {
        return std::pair<size_type, bool>(0, false);
      }
      ++num_probes;                        // we're doing another probe
      bucknum = (bucknum + JUMP_(key, num_probes)) & bucket_count_minus_one;
      assert(num_probes < bucket_count()
             && "Hashtable is full: an error in key_equal<> or hash<>");
    }
  }

 private:
  template <class K>
  iterator find_impl(const K& key) {
    std::pair<size_type, bool> pos = find_if_present(key);
    return pos.second ?
        iterator(this, table + pos.first, table + num_buckets, false) :
        end();
  }

  template <class K>
  const_iterator find_impl(const K& key) const {
    std::pair<size_type, bool> pos = find_if_present(key);
    return pos.second ?
        const_iterator(this, table + pos.first, table + num_buckets, false) :
        end();
  }

  template <class K>
  size_type bucket_impl(const K& key) const {
    std::pair<size_type, size_type> pos = find_position(key);
    return pos.first == ILLEGAL_BUCKET ? pos.second : pos.first;
  }

  template <class K>
  size_type count_impl(const K& key) const {
    return find_if_present(key).second ? 1 : 0;
  }

  template <class K>
  std::pair<iterator, iterator>
  equal_range_impl(const K& key) {
    iterator pos = find(key);
    if (pos == end()) {
      return std::pair<iterator, iterator>(pos, pos);
    } else {
      const iterator startpos = pos++;
      return std::pair<iterator, iterator>(startpos, pos);
    }
  }

  template <class K>
  std::pair<const_iterator, const_iterator>
  equal_range_impl(const K& key) const {
    const_iterator pos = find(key);
    if (pos == end()) {
      return std::pair<const_iterator, const_iterator>(pos, pos);
    } else {
      const const_iterator startpos = pos++;
      return std::pair<const_iterator, const_iterator>(startpos, pos);
    }
  }

 public:
  iterator find(const key_type& key) { return find_impl(key); }

  const_iterator find(const key_type& key) const { return find_impl(key); }

  // This is a tr1 method: the bucket a given key is in, or what bucket
  // it would be put in, if it were to be inserted.  Shrug.
  size_type bucket(const key_type& key) const { return bucket_impl(key); }

  // Counts how many elements have key key.  For maps, it's either 0 or 1.
  size_type count(const key_type &key) const { return count_impl(key); }

  // Likewise, equal_range doesn't really make sense for us.  Oh well.
  std::pair<iterator, iterator>
  equal_range(const key_type& key) { return equal_range_impl(key); }

  std::pair<const_iterator, const_iterator>
  equal_range(const key_type& key) const { return equal_range_impl(key); }



  // INSERTION ROUTINES
 private:
  // Private method used by insert_noresize and find_or_insert.
  // 'obj' is either value_type&& or const value_type&.
  template <typename U>
  iterator insert_at(U&& obj, size_type pos) {
    if (size() >= max_size()) {
      throw std::length_error("insert overflow");
    }
    if ( test_deleted(pos) ) {      // just replace if it's been del.
      assert(num_deleted > 0);
      --num_deleted;                // used to be, now it isn't
    } else {
      ++num_elements;               // replacing an empty bucket
    }
    set_value(&table[pos], std::forward<U>(obj));
    return iterator(this, table + pos, table + num_buckets, false);
  }

  // If you know *this is big enough to hold obj, use this routine
  // 'obj' is value_type&& or const value_type&.
  template <typename U>
  std::pair<iterator, bool> insert_noresize(U&& obj) {  // NOLINT
    return insert_noresize_using_hash(hash(get_key(obj)), std::forward<U>(obj));
  }

  // If you know *this is big enough to hold obj, use this routine
  // 'obj' is value_type&& or const value_type&.
  template <typename U>
  std::pair<iterator, bool> insert_noresize_using_hash(const size_type key_hash,
                                                       U&& obj) {
    const std::pair<size_type, size_type> pos =
        find_position_using_hash(key_hash, get_key(obj));
    if (pos.first != ILLEGAL_BUCKET) {      // object was already there
      return std::pair<iterator, bool>(iterator(this, table + pos.first,
                                          table + num_buckets, false),
                                 false);          // false: we didn't insert
    } else {                                 // pos.second says where to put it
      iterator i = insert_at(std::forward<U>(obj), pos.second);
      return std::pair<iterator, bool>(i, true);
    }
  }

  // Specializations of insert(it, it) depending on the power of the iterator:
  // (1) Iterator supports operator-, resize before inserting
  template <class ForwardIterator>
  void insert(ForwardIterator f, ForwardIterator l, std::forward_iterator_tag) {
    size_t dist = std::distance(f, l);
    if (dist >= std::numeric_limits<size_type>::max()) {
      throw std::length_error("insert-range overflow");
    }
    resize_delta(static_cast<size_type>(dist));
    for ( ; dist > 0; --dist, ++f) {
      insert_noresize(*f);
    }
  }

  // (2) Arbitrary iterator, can't tell how much to resize
  template <class InputIterator>
  void insert(InputIterator f, InputIterator l, std::input_iterator_tag) {
    for ( ; f != l; ++f)
      insert(*f);
  }

 public:
  // This is the normal insert routine, used by the outside world
  std::pair<iterator, bool> insert(const value_type& obj) {
    resize_delta(1);                      // adding an object, grow if need be
    return insert_noresize(obj);
  }

  std::pair<iterator, bool> insert(value_type&& obj) {  // NOLINT
    resize_delta(1);                      // adding an object, grow if need be
    return insert_noresize(std::move(obj));
  }

  // When inserting a lot at a time, we specialize on the type of iterator
  template <class InputIterator>
  void insert(InputIterator f, InputIterator l) {
    // specializes on iterator type
    insert(f, l,
           typename std::iterator_traits<InputIterator>::iterator_category());
  }

  template <class DefaultValue>
  value_type& find_or_insert(const key_type& key) {
    return find_or_insert_using_hash<DefaultValue>(hash(key), key);
  }

  // DefaultValue is a functor that takes a key and returns a value_type
  // representing the default value to be inserted if none is found.
  template <class DefaultValue>
  value_type& find_or_insert_using_hash(const size_type key_hash,
                                        const key_type& key) {
    const std::pair<size_type, size_type> pos =
        find_position_using_hash(key_hash, key);
    DefaultValue default_value;
    if (pos.first != ILLEGAL_BUCKET) {  // object was already there
      return table[pos.first];
    } else if (resize_delta(1)) {        // needed to rehash to make room
      // Since we resized, we can't use pos, so recalculate where to insert.
      return *insert_noresize(default_value(key)).first;
    } else {                             // no need to rehash, insert right here
      return *insert_at(default_value(key), pos.second);
    }
  }


  // DELETION ROUTINES
 private:
  template <class K>
  size_type erase_impl(const K& key) {
    iterator pos = find(key);
    if (pos != end()) {
      assert(!test_deleted(pos));  // or find() shouldn't have returned it
      set_deleted(pos);
      ++num_deleted;
      // will think about shrink after next insert
      settings.set_consider_shrink(true);
      return 1;                    // because we deleted one thing
    } else {
      return 0;                    // because we deleted nothing
    }
  }

 public:
  size_type erase(const key_type& key) {
    return erase_impl(key);
  }


  void erase(iterator pos) {
    if (pos == end()) return;    // sanity check
    set_deleted(pos);
    ++num_deleted;
    // will think about shrink after next insert
    settings.set_consider_shrink(true);
  }

  void erase(iterator f, iterator l) {
    for (; f != l; ++f) {
      set_deleted(f);
      ++num_deleted;
    }
    // will think about shrink after next insert
    settings.set_consider_shrink(true);
  }

  // We allow you to erase a const_iterator just like we allow you to
  // erase an iterator.  This is in parallel to 'delete': you can delete
  // a const pointer just like a non-const pointer.  The logic is that
  // you can't use the object after it's erased anyway, so it doesn't matter
  // if it's const or not.
  void erase(const_iterator pos) {
    if (pos == end()) return;    // sanity check
    set_deleted(pos);
    ++num_deleted;
    // will think about shrink after next insert
    settings.set_consider_shrink(true);
  }
  void erase(const_iterator f, const_iterator l) {
    for ( ; f != l; ++f) {
      set_deleted(f);
      ++num_deleted;
    }
    // will think about shrink after next insert
    settings.set_consider_shrink(true);
  }


  // COMPARISON
  bool operator==(const dense_hashtable& ht) const {
    if (size() != ht.size()) {
      return false;
    } else if (this == &ht) {
      return true;
    } else {
      // Iterate through the elements in "this" and see if the
      // corresponding element is in ht
      for ( const_iterator it = begin(); it != end(); ++it ) {
        const_iterator it2 = ht.find(get_key(*it));
        if ((it2 == ht.end()) || (*it != *it2)) {
          return false;
        }
      }
      return true;
    }
  }
  bool operator!=(const dense_hashtable& ht) const {
    return !(*this == ht);
  }


  // I/O
  // We support reading and writing hashtables to disk.  Alas, since
  // I don't know how to write a hasher or key_equal, you have to make
  // sure everything but the table is the same.  We compact before writing.
 private:
  // Every time the disk format changes, this should probably change too
  typedef unsigned long MagicNumberType;
  static const MagicNumberType MAGIC_NUMBER = 0x13578642;

  template <class A>
  class alloc_impl : public A {
   public:
    typedef typename A::pointer pointer;
    typedef typename A::size_type size_type;

    // Convert a normal allocator to one that has realloc_or_die()
    alloc_impl(const A& a) : A(a) { }

    // realloc_or_die should only be used when using the default
    // allocator (libc_allocator_with_realloc).
    pointer realloc_or_die(pointer /*ptr*/, size_type /*n*/) {
      fprintf(stderr, "realloc_or_die is only supported for "
                      "libc_allocator_with_realloc\n");
      exit(1);
      return nullptr;
    }
  };

  // A template specialization of alloc_impl for
  // libc_allocator_with_realloc that can handle realloc_or_die.
  template <class A>
  class alloc_impl<libc_allocator_with_realloc<A> >
      : public libc_allocator_with_realloc<A> {
   public:
    typedef typename libc_allocator_with_realloc<A>::pointer pointer;
    typedef typename libc_allocator_with_realloc<A>::size_type size_type;

    alloc_impl(const libc_allocator_with_realloc<A>& a)
        : libc_allocator_with_realloc<A>(a) { }

    pointer realloc_or_die(pointer ptr, size_type n) {
      pointer retval = this->reallocate(ptr, n);
      if (retval == nullptr) {
        fprintf(stderr, "sparsehash: FATAL ERROR: failed to reallocate "
                "%lu elements for ptr %p", static_cast<unsigned long>(n), ptr);
        exit(1);
      }
      return retval;
    }
  };

  // Package functors with another class to eliminate memory needed for
  // zero-size functors.  Since ExtractKey and hasher's operator() might
  // have the same function signature, they must be packaged in
  // different classes.
  struct Settings :
      sh_hashtable_settings<key_type, hasher, size_type, HT_MIN_BUCKETS> {
    explicit Settings(const hasher& hf)
        : sh_hashtable_settings<key_type, hasher, size_type, HT_MIN_BUCKETS>(
            hf, HT_OCCUPANCY_PCT / 100.0f, HT_EMPTY_PCT / 100.0f) {}
  };

  // Packages ExtractKey, SetKey, EqualKey functors, allocator and deleted and
  // empty key values.
  struct KeyInfo : public ExtractKey,
                   public SetKey,
                   public EqualKey,
                   public alloc_impl<value_alloc_type> {
    KeyInfo(const ExtractKey& ek, const SetKey& sk, const EqualKey& eq,
            const alloc_impl<value_alloc_type>& a)
        : ExtractKey(ek),
          SetKey(sk),
          EqualKey(eq),
          alloc_impl<value_alloc_type>(a),
          delkey(),
          empty() {}

    // Accessors for convenient access to base classes.
    ExtractKey& as_extract_key() { return *this; }
    const ExtractKey& as_extract_key() const { return *this; }
    SetKey& as_set_key() { return *this; }
    const SetKey& as_set_key() const { return *this; }
    EqualKey& as_equal_key() { return *this; }
    const EqualKey& as_equal_key() const { return *this; }
    value_alloc_type& as_value_alloc() { return *this; }
    const value_alloc_type& as_value_alloc() const { return *this; }

    // We want to return the exact same type as ExtractKey: Key or const Key&
    typename ExtractKey::result_type get_key(const_reference v) const {
      return ExtractKey::operator()(v);
    }
    void set_key(pointer v, const key_type& k) const {
      SetKey::operator()(v, k);
    }

    // We only ever call EqualKey::operator()(key_type, K) -- we never use the
    // other order of args.  This allows consumers to get away with implementing
    // only half of operator==.
    template <class K>
    bool equals(const key_type& a, const K& b) const {
      return EqualKey::operator()(a, b);
    }

    pointer allocate(size_type size) {
      pointer memory = alloc_impl<value_alloc_type>::allocate(size);
      assert(memory != nullptr);
      return memory;
    }

    // Which key marks deleted entries.
    // TODO(user): make a pointer, and get rid of use_deleted (benchmark!)
    typename std::remove_const<key_type>::type delkey;
    // Key value used to mark unused entries.
    typename std::remove_const<key_type>::type empty;
  };

  // Returns the alloc_impl<value_alloc_type> used to allocate and deallocate
  // the table. This can be different from the one returned by get_allocator().
  alloc_impl<value_alloc_type>& get_internal_allocator() { return key_info; }

  // Utility functions to access the templated operators
  size_type hash(const key_type& v) const {
    return settings.hash(v);
  }
  bool equals(const key_type& a, const key_type& b) const {
    return key_info.equals(a, b);
  }


  typename ExtractKey::result_type get_key(const_reference v) const {
    return key_info.get_key(v);
  }
  void set_key(pointer v, const key_type& k) const {
    key_info.set_key(v, k);
  }

 private:
  // Actual data
  Settings settings;
  KeyInfo key_info;

  size_type num_deleted;  // how many occupied buckets are marked deleted
  size_type num_elements;
  size_type num_buckets;
  pointer table;
};


// We need a global swap as well
template <class V, class K, class HF, class ExK, class SetK, class EqK, class A>
inline void swap(dense_hashtable<V, K, HF, ExK, SetK, EqK, A> &x,
                 dense_hashtable<V, K, HF, ExK, SetK, EqK, A> &y) {
  x.swap(y);
}

#undef JUMP_
#undef PUT_
#undef GET_

template <class V, class K, class HF, class ExK, class SetK, class EqK, class A>
const typename dense_hashtable<V, K, HF, ExK, SetK, EqK, A>::size_type
  dense_hashtable<V, K, HF, ExK, SetK, EqK, A>::ILLEGAL_BUCKET;

// How full we let the table get before we resize.  Knuth says .8 is
// good -- higher causes us to probe too much, though saves memory.
// However, we go with .5, getting better performance at the cost of
// more space (a trade-off densehashtable explicitly chooses to make).
// Feel free to play around with different values, though, via
// max_load_factor() and/or set_resizing_parameters().
template <class V, class K, class HF, class ExK, class SetK, class EqK, class A>
const int dense_hashtable<V,K,HF,ExK,SetK,EqK,A>::HT_OCCUPANCY_PCT = 50;

// How empty we let the table get before we resize lower.
// It should be less than OCCUPANCY_PCT / 2 or we thrash resizing.
template <class V, class K, class HF, class ExK, class SetK, class EqK, class A>
const int dense_hashtable<V, K, HF, ExK, SetK, EqK, A>::HT_EMPTY_PCT =
  static_cast<int>(
      0.4 * dense_hashtable<V, K, HF, ExK, SetK, EqK, A>::HT_OCCUPANCY_PCT);

}

#endif  // S2_UTIL_GTL_DENSEHASHTABLE_H_
