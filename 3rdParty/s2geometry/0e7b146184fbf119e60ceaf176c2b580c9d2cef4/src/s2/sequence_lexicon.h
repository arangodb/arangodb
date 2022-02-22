// Copyright 2016 Google Inc. All Rights Reserved.
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

// Author: ericv@google.com (Eric Veach)

#ifndef S2_SEQUENCE_LEXICON_H_
#define S2_SEQUENCE_LEXICON_H_

#include <functional>
#include <limits>
#include <vector>

#include "s2/base/integral_types.h"
#include "s2/util/gtl/dense_hash_set.h"
#include "s2/util/hash/mix.h"

// SequenceLexicon is a class for compactly representing sequences of values
// (e.g., tuples).  It automatically eliminates duplicates, and maps the
// remaining sequences to sequentially increasing integer ids.  See also
// ValueLexicon and IdSetLexicon.
//
// Each distinct sequence is mapped to a 32-bit integer.  The space used for
// each sequence is approximately 11 bytes plus the memory needed to represent
// the sequence elements.  For example, a sequence of three "double"s would
// need about 11 + 3*8 = 35 bytes.  Note also that sequences are referred to
// using 32-bit ids rather than 64-bit pointers.
//
// This class has the same thread-safety properties as "string": const methods
// are thread safe, and non-const methods are not thread safe.
//
// Example usage:
//
//   SequenceLexicon<string> lexicon;
//   vector<string> pets {"cat", "dog", "parrot"};
//   uint32 pets_id = lexicon.Add(pets);
//   S2_CHECK_EQ(pets_id, lexicon.Add(pets));
//   string values;
//   for (const auto& pet : lexicon.sequence(pets_id)) {
//     values += pet;
//   }
//   S2_CHECK_EQ("catdogparrot", values);
//
template <class T,
          class Hasher = std::hash<T>,
          class KeyEqual = std::equal_to<T>>
class SequenceLexicon {
 public:
  explicit SequenceLexicon(const Hasher& hasher = Hasher(),
                           const KeyEqual& key_equal = KeyEqual());

  // SequenceLexicon is movable and copyable.
  SequenceLexicon(const SequenceLexicon&);
  SequenceLexicon& operator=(const SequenceLexicon&);
  SequenceLexicon(SequenceLexicon&&);
  SequenceLexicon& operator=(SequenceLexicon&&);

  // Clears all data from the lexicon.
  void Clear();

  // Add the given sequence of values to the lexicon if it is not already
  // present, and return its integer id.  Ids are assigned sequentially
  // starting from zero.  "begin" and "end" are forward iterators over a
  // sequence of values of type T.
  template <class FwdIterator>
  uint32 Add(FwdIterator begin, FwdIterator end);

  // Add the given sequence of values to the lexicon if it is not already
  // present, and return its integer id.  Ids are assigned sequentially
  // starting from zero.  This is a convenience method equivalent to
  // Add(std::begin(container), std::end(container)).
  template <class Container>
  uint32 Add(const Container& container);

  // Return the number of value sequences in the lexicon.
  uint32 size() const;

  // Iterator type; please treat this as an opaque forward iterator.
  using Iterator = typename std::vector<T>::const_iterator;

  // A class representing a sequence of values.
  class Sequence {
   public:
    Iterator begin() const { return begin_; }
    Iterator end() const { return end_; }
    size_t size() const { return end_ - begin_; }

   private:
    friend class SequenceLexicon;
    Sequence(Iterator begin, Iterator end) : begin_(begin), end_(end) {}
    Iterator begin_, end_;
  };
  // Return the value sequence with the given id.  This method can be used
  // with range-based for loops as follows:
  //   for (const auto& value : lexicon.sequence(id)) { ... }
  Sequence sequence(uint32 id) const;

 private:
  friend class IdKeyEqual;
  // Choose kEmptyKey to be the last key that will ever be generated.
  static const uint32 kEmptyKey = std::numeric_limits<uint32>::max();

  class IdHasher {
   public:
    IdHasher(const Hasher& hasher, const SequenceLexicon* lexicon);
    const Hasher& hasher() const;
    size_t operator()(uint32 id) const;
   private:
    Hasher hasher_;
    const SequenceLexicon* lexicon_;
  };

  class IdKeyEqual {
   public:
    IdKeyEqual(const KeyEqual& key_equal, const SequenceLexicon* lexicon);
    const KeyEqual& key_equal() const;
    bool operator()(uint32 id1, uint32 id2) const;
   private:
    KeyEqual key_equal_;
    const SequenceLexicon* lexicon_;
  };

  using IdSet = gtl::dense_hash_set<uint32, IdHasher, IdKeyEqual>;

  std::vector<T> values_;
  std::vector<uint32> begins_;
  IdSet id_set_;
};


//////////////////   Implementation details follow   ////////////////////


template <class T, class Hasher, class KeyEqual>
const uint32 SequenceLexicon<T, Hasher, KeyEqual>::kEmptyKey;

template <class T, class Hasher, class KeyEqual>
SequenceLexicon<T, Hasher, KeyEqual>::IdHasher::IdHasher(
    const Hasher& hasher, const SequenceLexicon* lexicon)
    : hasher_(hasher), lexicon_(lexicon) {
}

template <class T, class Hasher, class KeyEqual>
const Hasher& SequenceLexicon<T, Hasher, KeyEqual>::IdHasher::hasher() const {
  return hasher_;
}

template <class T, class Hasher, class KeyEqual>
size_t SequenceLexicon<T, Hasher, KeyEqual>::IdHasher::operator()(
    uint32 id) const {
  HashMix mix;
  for (const auto& value : lexicon_->sequence(id)) {
    mix.Mix(hasher_(value));
  }
  return mix.get();
}

template <class T, class Hasher, class KeyEqual>
SequenceLexicon<T, Hasher, KeyEqual>::IdKeyEqual::IdKeyEqual(
    const KeyEqual& key_equal, const SequenceLexicon* lexicon)
    : key_equal_(key_equal), lexicon_(lexicon) {
}

template <class T, class Hasher, class KeyEqual>
const KeyEqual& SequenceLexicon<T, Hasher, KeyEqual>::IdKeyEqual::key_equal()
    const {
  return key_equal_;
}

template <class T, class Hasher, class KeyEqual>
bool SequenceLexicon<T, Hasher, KeyEqual>::IdKeyEqual::operator()(
    uint32 id1, uint32 id2) const {
  if (id1 == id2) return true;
  if (id1 == lexicon_->kEmptyKey || id2 == lexicon_->kEmptyKey) {
    return false;
  }
  SequenceLexicon::Sequence seq1 = lexicon_->sequence(id1);
  SequenceLexicon::Sequence seq2 = lexicon_->sequence(id2);
  return (seq1.size() == seq2.size() &&
          std::equal(seq1.begin(), seq1.end(), seq2.begin(), key_equal_));
}

template <class T, class Hasher, class KeyEqual>
SequenceLexicon<T, Hasher, KeyEqual>::SequenceLexicon(const Hasher& hasher,
                                                      const KeyEqual& key_equal)
    : id_set_(0, IdHasher(hasher, this),
              IdKeyEqual(key_equal, this)) {
  id_set_.set_empty_key(kEmptyKey);
  begins_.push_back(0);
}

template <class T, class Hasher, class KeyEqual>
SequenceLexicon<T, Hasher, KeyEqual>::SequenceLexicon(const SequenceLexicon& x)
    : values_(x.values_), begins_(x.begins_),
      // Unfortunately we can't copy "id_set_" because we need to change the
      // "this" pointers associated with hasher() and key_equal().
      id_set_(x.id_set_.begin(), x.id_set_.end(), kEmptyKey, 0,
              IdHasher(x.id_set_.hash_funct().hasher(), this),
              IdKeyEqual(x.id_set_.key_eq().key_equal(), this)) {
}

template <class T, class Hasher, class KeyEqual>
SequenceLexicon<T, Hasher, KeyEqual>::SequenceLexicon(SequenceLexicon&& x)
    : values_(std::move(x.values_)), begins_(std::move(x.begins_)),
      // Unfortunately we can't move "id_set_" because we need to change the
      // "this" pointers associated with hasher() and key_equal().
      id_set_(x.id_set_.begin(), x.id_set_.end(), kEmptyKey, 0,
              IdHasher(x.id_set_.hash_funct().hasher(), this),
              IdKeyEqual(x.id_set_.key_eq().key_equal(), this)) {
}

template <class T, class Hasher, class KeyEqual>
SequenceLexicon<T, Hasher, KeyEqual>&
SequenceLexicon<T, Hasher, KeyEqual>::operator=(const SequenceLexicon& x) {
  // Note that self-assignment is handled correctly by this code.
  values_ = x.values_;
  begins_ = x.begins_;
  // Unfortunately we can't copy-assign "id_set_" because we need to change
  // the "this" pointers associated with hasher() and key_equal().
  id_set_ = IdSet(x.id_set_.begin(), x.id_set_.end(), kEmptyKey, 0,
                  IdHasher(x.id_set_.hash_funct().hasher(), this),
                  IdKeyEqual(x.id_set_.key_eq().key_equal(), this));
  return *this;
}

template <class T, class Hasher, class KeyEqual>
SequenceLexicon<T, Hasher, KeyEqual>&
SequenceLexicon<T, Hasher, KeyEqual>::operator=(SequenceLexicon&& x) {
  // Note that move self-assignment has undefined behavior.
  values_ = std::move(x.values_);
  begins_ = std::move(x.begins_);
  // Unfortunately we can't move-assign "id_set_" because we need to change
  // the "this" pointers associated with hasher() and key_equal().
  id_set_ = IdSet(x.id_set_.begin(), x.id_set_.end(), kEmptyKey, 0,
                  IdHasher(x.id_set_.hash_funct().hasher(), this),
                  IdKeyEqual(x.id_set_.key_eq().key_equal(), this));
  return *this;
}

template <class T, class Hasher, class KeyEqual>
void SequenceLexicon<T, Hasher, KeyEqual>::Clear() {
  values_.clear();
  begins_.clear();
  id_set_.clear();
  begins_.push_back(0);
}

template <class T, class Hasher, class KeyEqual>
template <class FwdIterator>
uint32 SequenceLexicon<T, Hasher, KeyEqual>::Add(FwdIterator begin,
                                                 FwdIterator end) {
  for (; begin != end; ++begin) {
    values_.push_back(*begin);
  }
  begins_.push_back(values_.size());
  uint32 id = begins_.size() - 2;
  auto result = id_set_.insert(id);
  if (result.second) {
    return id;
  } else {
    begins_.pop_back();
    values_.resize(begins_.back());
    return *result.first;
  }
}

template <class T, class Hasher, class KeyEqual>
template <class Container>
uint32 SequenceLexicon<T, Hasher, KeyEqual>::Add(const Container& container) {
  return Add(std::begin(container), std::end(container));
}

template <class T, class Hasher, class KeyEqual>
inline uint32 SequenceLexicon<T, Hasher, KeyEqual>::size() const {
  return begins_.size() - 1;
}

template <class T, class Hasher, class KeyEqual>
inline typename SequenceLexicon<T, Hasher, KeyEqual>::Sequence
SequenceLexicon<T, Hasher, KeyEqual>::sequence(uint32 id) const {
  return Sequence(values_.begin() + begins_[id],
                  values_.begin() + begins_[id + 1]);
}

#endif  // S2_SEQUENCE_LEXICON_H_
