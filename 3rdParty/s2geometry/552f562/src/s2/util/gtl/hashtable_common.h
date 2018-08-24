// Copyright 2010 Google Inc. All Rights Reserved.
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

#ifndef S2_UTIL_GTL_HASHTABLE_COMMON_H_
#define S2_UTIL_GTL_HASHTABLE_COMMON_H_

#include <cassert>
#include <cstddef>
#include <algorithm>

#include <stdexcept>                 // For length_error

// Settings contains parameters for growing and shrinking the table.
// It also packages zero-size functor (ie. hasher).  One invariant
// enforced in enlarge_size() is that we never allow all slots
// occupied.  (This is unlikely to matter to users, because using
// a load near 1 is slow and not recommended.  It allows other code
// to assume there is at least one empty bucket.)
//
// It does some munging of the hash value in cases where we think
// (fear) the original hash function might not be very good.  In
// particular, the default hash of pointers is the identity hash,
// so probably all the low bits are 0.  We identify when we think
// we're hashing a pointer, and chop off the low bits.  Note this
// isn't perfect: even when the key is a pointer, we can't tell
// for sure that the hash is the identity hash.  If it's not, this
// is needless work (and possibly, though not likely, harmful).

template<typename Key, typename HashFunc,
         typename SizeType, int HT_MIN_BUCKETS>
class sh_hashtable_settings : public HashFunc {
 public:
  typedef Key key_type;
  typedef HashFunc hasher;
  typedef SizeType size_type;

 public:
  sh_hashtable_settings(const hasher& hf,
                        const float ht_occupancy_flt,
                        const float ht_empty_flt)
      : hasher(hf),
        enlarge_threshold_(0),
        shrink_threshold_(0),
        consider_shrink_(false),
        use_empty_(false),
        use_deleted_(false),
        num_ht_copies_(0) {
    set_enlarge_factor(ht_occupancy_flt);
    set_shrink_factor(ht_empty_flt);
  }

  template<class K>
  size_type hash(const K& v) const {
    // We munge the hash value when we don't trust hasher::operator().  It is
    // very important that we use hash_munger<Key> instead of hash_munger<K>.
    // Within a given hashtable, all hash values must be munged in the same way.
    return hash_munger<Key>::MungedHash(hasher::operator()(v));
  }

  float enlarge_factor() const {
    return enlarge_factor_;
  }
  void set_enlarge_factor(float f) {
    enlarge_factor_ = f;
  }
  float shrink_factor() const {
    return shrink_factor_;
  }
  void set_shrink_factor(float f) {
    shrink_factor_ = f;
  }

  size_type enlarge_threshold() const {
    return enlarge_threshold_;
  }
  void set_enlarge_threshold(size_type t) {
    enlarge_threshold_ = t;
  }
  size_type shrink_threshold() const {
    return shrink_threshold_;
  }
  void set_shrink_threshold(size_type t) {
    shrink_threshold_ = t;
  }

  size_type enlarge_size(size_type x) const {
    return std::min<size_type>(x - 1, x * enlarge_factor_);
  }
  size_type shrink_size(size_type x) const {
    return static_cast<size_type>(x * shrink_factor_);
  }

  bool consider_shrink() const {
    return consider_shrink_;
  }
  void set_consider_shrink(bool t) {
    consider_shrink_ = t;
  }

  bool use_empty() const {
    return use_empty_;
  }
  void set_use_empty(bool t) {
    use_empty_ = t;
  }

  bool use_deleted() const {
    return use_deleted_;
  }
  void set_use_deleted(bool t) {
    use_deleted_ = t;
  }

  size_type num_ht_copies() const {
    return static_cast<size_type>(num_ht_copies_);
  }
  void inc_num_ht_copies() {
    ++num_ht_copies_;
  }

  // Reset the enlarge and shrink thresholds
  void reset_thresholds(size_type num_buckets) {
    set_enlarge_threshold(enlarge_size(num_buckets));
    set_shrink_threshold(shrink_size(num_buckets));
    // whatever caused us to reset already considered
    set_consider_shrink(false);
  }

  // Caller is resposible for calling reset_threshold right after
  // set_resizing_parameters.
  void set_resizing_parameters(float shrink, float grow) {
    assert(shrink >= 0.0);
    assert(grow <= 1.0);
    if (shrink > grow/2.0f)
      shrink = grow / 2.0f;     // otherwise we thrash hashtable size
    set_shrink_factor(shrink);
    set_enlarge_factor(grow);
  }

  // This is the smallest size a hashtable can be without being too crowded.
  // If you like, you can give a min #buckets as well as a min #elts.
  // This is guaranteed to return a power of two.
  size_type min_buckets(size_type num_elts, size_type min_buckets_wanted) {
    float enlarge = enlarge_factor();
    size_type sz = HT_MIN_BUCKETS;             // min buckets allowed
    while ( sz < min_buckets_wanted ||
            num_elts >= static_cast<size_type>(sz * enlarge) ) {
      // This just prevents overflowing size_type, since sz can exceed
      // max_size() here.
      if (static_cast<size_type>(sz * 2) < sz) {
        throw std::length_error("resize overflow");  // protect against overflow
      }
      sz *= 2;
    }
    return sz;
  }

 private:
  template<class HashKey> class hash_munger {
   public:
    static size_t MungedHash(size_t hash) {
      return hash;
    }
  };
  // This matches when the hashtable key is a pointer.
  template<class HashKey> class hash_munger<HashKey*> {
   public:
    static size_t MungedHash(size_t hash) {
      // TODO(user): consider rotating instead:
      //    static const int shift = (sizeof(void *) == 4) ? 2 : 3;
      //    return (hash << (sizeof(hash) * 8) - shift)) | (hash >> shift);
      // This matters if we ever change sparse/dense_hash_* to compare
      // hashes before comparing actual values.  It's speedy on x86.
      return hash / sizeof(void*);   // get rid of known-0 bits
    }
  };

  size_type enlarge_threshold_;  // table.size() * enlarge_factor
  size_type shrink_threshold_;   // table.size() * shrink_factor
  float enlarge_factor_;         // how full before resize
  float shrink_factor_;          // how empty before resize
  // consider_shrink=true if we should try to shrink before next insert
  bool consider_shrink_;
  bool use_empty_;    // used only by densehashtable, not sparsehashtable
  bool use_deleted_;  // false until delkey has been set
  // num_ht_copies is a counter incremented every Copy/Move
  unsigned int num_ht_copies_;
};

// This traits class checks whether T::is_transparent exists and names a type.
//
//   struct Foo { using is_transparent = void; };
//   struct Bar {};
//   static_assert(sh_is_transparent<Foo>::value, "Foo is transparent.");
//   staitc_assert(!sh_is_transparent<Bar>::value, "Bar is not transparent.");
template<class T>
struct sh_is_transparent {
 private:
  struct No { char x; };
  struct Yes { No x[2]; };

  template <class U>
  static Yes Test(typename U::is_transparent*);
  template<class U> static No Test(...);

 public:
  enum { value = sizeof(Test<T>(nullptr)) == sizeof(Yes) };
};


#endif  // S2_UTIL_GTL_HASHTABLE_COMMON_H_
