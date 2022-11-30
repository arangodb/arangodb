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

#ifndef S2_VALUE_LEXICON_H_
#define S2_VALUE_LEXICON_H_

#include <functional>
#include <limits>
#include <vector>

#include "s2/base/integral_types.h"
#include "s2/util/gtl/dense_hash_set.h"

// ValueLexicon is a class that maps distinct values to sequentially numbered
// integer identifiers.  It automatically eliminates duplicates and uses a
// compact representation.  See also SequenceLexicon.
//
// Each distinct value is mapped to a 32-bit integer.  The space used for each
// value is approximately 7 bytes plus the space needed for the value itself.
// For example, int64 values would need approximately 15 bytes each.  Note
// also that values are referred to using 32-bit ids rather than 64-bit
// pointers.
//
// This class has the same thread-safety properties as "string": const methods
// are thread safe, and non-const methods are not thread safe.
//
// Example usage:
//
//   ValueLexicon<string> lexicon;
//   uint32 cat_id = lexicon.Add("cat");
//   EXPECT_EQ(cat_id, lexicon.Add("cat"));
//   EXPECT_EQ("cat", lexicon.value(cat_id));
//
template <class T,
          class Hasher = std::hash<T>,
          class KeyEqual = std::equal_to<T>>
class ValueLexicon {
 public:
  explicit ValueLexicon(const Hasher& hasher = Hasher(),
                        const KeyEqual& key_equal = KeyEqual());

  // ValueLexicon is movable and copyable.
  ValueLexicon(const ValueLexicon&);
  ValueLexicon& operator=(const ValueLexicon&);
  ValueLexicon(ValueLexicon&&);
  ValueLexicon& operator=(ValueLexicon&&);

  // Clears all data from the lexicon.
  void Clear();

  // Add the given value to the lexicon if it is not already present, and
  // return its integer id.  Ids are assigned sequentially starting from zero.
  uint32 Add(const T& value);

  // Return the number of values in the lexicon.
  uint32 size() const;

  // Return the value with the given id.
  const T& value(uint32 id) const;

 private:
  friend class IdKeyEqual;
  // Choose kEmptyKey to be the last key that will ever be generated.
  static const uint32 kEmptyKey = std::numeric_limits<uint32>::max();

  class IdHasher {
   public:
    IdHasher(const Hasher& hasher, const ValueLexicon* lexicon);
    const Hasher& hasher() const;
    size_t operator()(uint32 id) const;
   private:
    Hasher hasher_;
    const ValueLexicon* lexicon_;
  };

  class IdKeyEqual {
   public:
    IdKeyEqual(const KeyEqual& key_equal, const ValueLexicon* lexicon);
    bool operator()(uint32 id1, uint32 id2) const;
   private:
    KeyEqual key_equal_;
    const ValueLexicon* lexicon_;
  };

  using IdSet = gtl::dense_hash_set<uint32, IdHasher, IdKeyEqual>;

  KeyEqual key_equal_;
  std::vector<T> values_;
  IdSet id_set_;
};


//////////////////   Implementation details follow   ////////////////////


template <class T, class Hasher, class KeyEqual>
const uint32 ValueLexicon<T, Hasher, KeyEqual>::kEmptyKey;

template <class T, class Hasher, class KeyEqual>
ValueLexicon<T, Hasher, KeyEqual>::IdHasher::IdHasher(
    const Hasher& hasher, const ValueLexicon* lexicon)
    : hasher_(hasher), lexicon_(lexicon) {
}

template <class T, class Hasher, class KeyEqual>
const Hasher& ValueLexicon<T, Hasher, KeyEqual>::IdHasher::hasher() const {
  return hasher_;
}

template <class T, class Hasher, class KeyEqual>
inline size_t ValueLexicon<T, Hasher, KeyEqual>::IdHasher::operator()(
    uint32 id) const {
  return hasher_(lexicon_->value(id));
}

template <class T, class Hasher, class KeyEqual>
ValueLexicon<T, Hasher, KeyEqual>::IdKeyEqual::IdKeyEqual(
    const KeyEqual& key_equal, const ValueLexicon* lexicon)
    : key_equal_(key_equal), lexicon_(lexicon) {
}

template <class T, class Hasher, class KeyEqual>
inline bool ValueLexicon<T, Hasher, KeyEqual>::IdKeyEqual::operator()(
    uint32 id1, uint32 id2) const {
  if (id1 == id2) return true;
  if (id1 == lexicon_->kEmptyKey || id2 == lexicon_->kEmptyKey) {
    return false;
  }
  return key_equal_(lexicon_->value(id1), lexicon_->value(id2));
}

template <class T, class Hasher, class KeyEqual>
ValueLexicon<T, Hasher, KeyEqual>::ValueLexicon(const Hasher& hasher,
                                                const KeyEqual& key_equal)
    : key_equal_(key_equal),
      id_set_(0, IdHasher(hasher, this),
                 IdKeyEqual(key_equal, this)) {
  id_set_.set_empty_key(kEmptyKey);
}

template <class T, class Hasher, class KeyEqual>
ValueLexicon<T, Hasher, KeyEqual>::ValueLexicon(const ValueLexicon& x)
    : key_equal_(x.key_equal_), values_(x.values_),
      // Unfortunately we can't copy "id_set_" because we need to change the
      // "this" pointers associated with hasher() and key_equal().
      id_set_(x.id_set_.begin(), x.id_set_.end(), kEmptyKey, 0,
              IdHasher(x.id_set_.hash_funct().hasher(), this),
              IdKeyEqual(x.key_equal_, this)) {
}

template <class T, class Hasher, class KeyEqual>
ValueLexicon<T, Hasher, KeyEqual>::ValueLexicon(ValueLexicon&& x)
    : key_equal_(std::move(x.key_equal_)), values_(std::move(x.values_)),
      // Unfortunately we can't move "id_set_" because we need to change the
      // "this" pointers associated with hasher() and key_equal().
      id_set_(x.id_set_.begin(), x.id_set_.end(), kEmptyKey, 0,
              IdHasher(x.id_set_.hash_funct().hasher(), this),
              IdKeyEqual(x.key_equal_, this)) {
}

template <class T, class Hasher, class KeyEqual>
ValueLexicon<T, Hasher, KeyEqual>&
ValueLexicon<T, Hasher, KeyEqual>::operator=(const ValueLexicon& x) {
  // Note that self-assignment is handled correctly by this code.
  key_equal_ = x.key_equal_;
  values_ = x.values_;
  // Unfortunately we can't copy-assign "id_set_" because we need to change
  // the "this" pointers associated with hasher() and key_equal().
  id_set_ = IdSet(x.id_set_.begin(), x.id_set_.end(), kEmptyKey, 0,
                  IdHasher(x.id_set_.hash_funct().hasher(), this),
                  IdKeyEqual(x.key_equal_, this));
  return *this;
}

template <class T, class Hasher, class KeyEqual>
ValueLexicon<T, Hasher, KeyEqual>&
ValueLexicon<T, Hasher, KeyEqual>::operator=(ValueLexicon&& x) {
  // Note that move self-assignment has undefined behavior.
  key_equal_ = std::move(x.key_equal_);
  values_ = std::move(x.values_);
  // Unfortunately we can't move-assign "id_set_" because we need to change
  // the "this" pointers associated with hasher() and key_equal().
  id_set_ = IdSet(x.id_set_.begin(), x.id_set_.end(), kEmptyKey, 0,
                  IdHasher(x.id_set_.hash_funct().hasher(), this),
                  IdKeyEqual(x.key_equal_, this));
  return *this;
}

template <class T, class Hasher, class KeyEqual>
void ValueLexicon<T, Hasher, KeyEqual>::Clear() {
  values_.clear();
  id_set_.clear();
}

template <class T, class Hasher, class KeyEqual>
uint32 ValueLexicon<T, Hasher, KeyEqual>::Add(const T& value) {
  if (!values_.empty() && key_equal_(value, values_.back())) {
    return values_.size() - 1;
  }
  values_.push_back(value);
  auto result = id_set_.insert(values_.size() - 1);
  if (result.second) {
    return values_.size() - 1;
  } else {
    values_.pop_back();
    return *result.first;
  }
}

template <class T, class Hasher, class KeyEqual>
inline uint32 ValueLexicon<T, Hasher, KeyEqual>::size() const {
  return values_.size();
}

template <class T, class Hasher, class KeyEqual>
inline const T& ValueLexicon<T, Hasher, KeyEqual>::value(uint32 id) const {
  return values_[id];
}

#endif  // S2_VALUE_LEXICON_H_
