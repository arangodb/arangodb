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

#ifndef S2_ID_SET_LEXICON_H_
#define S2_ID_SET_LEXICON_H_

#include <limits>
#include <vector>

#include "s2/base/integral_types.h"
#include "s2/base/logging.h"
#include "s2/sequence_lexicon.h"

// IdSetLexicon is a class for compactly representing sets of non-negative
// integers such as array indices ("id sets").  It is especially suitable when
// either (1) there are many duplicate sets, or (2) there are many singleton
// or empty sets.  See also ValueLexicon and SequenceLexicon.
//
// Each distinct id set is mapped to a 32-bit integer.  Empty and singleton
// sets take up no additional space whatsoever; the set itself is represented
// by the unique id assigned to the set. Sets of size 2 or more occupy about
// 11 bytes per set plus 4 bytes per element (as compared to 24 bytes per set
// plus 4 bytes per element for std::vector).  Duplicate sets are
// automatically eliminated.  Note also that id sets are referred to using
// 32-bit integers rather than 64-bit pointers.
//
// This class is especially useful in conjunction with ValueLexicon<T>.  For
// example, suppose that you want to label objects with a set of strings.  You
// could use a ValueLexicon<string> to map the strings to "label ids" (32-bit
// integers), and then use IdSetLexicon to map each set of labels to a "label
// set id".  Each reference to that label set then takes up only 4 bytes.
//
// Example usage:
//
//   ValueLexicon<string> labels_;
//   IdSetLexicon label_sets_;
//
//   int32 GetLabelSet(const vector<string>& label_strings) {
//     vector<int32> label_ids;
//     for (const auto& str : label_strings) {
//       label_ids.push_back(labels_.Add(str));
//     }
//     return label_sets_.Add(label_ids);
//   }
//
//   int label_set_id = GetLabelSet(...);
//   for (auto id : label_sets_.id_set(label_set_id)) {
//     S2_LOG(INFO) << id;
//   }
//
// This class is similar to SequenceLexicon, except:
//
// 1. Empty and singleton sets are represented implicitly; they use no space.
// 2. Sets are represented rather than sequences; the ordering of values is
//    not important and duplicates are removed.
// 3. The values must be 32-bit non-negative integers (only).
class IdSetLexicon {
 public:
  IdSetLexicon();
  ~IdSetLexicon();

  // IdSetLexicon is movable and copyable.
  IdSetLexicon(const IdSetLexicon&);
  IdSetLexicon& operator=(const IdSetLexicon&);
  IdSetLexicon(IdSetLexicon&&);
  IdSetLexicon& operator=(IdSetLexicon&&);

  // Clears all data from the lexicon.
  void Clear();

  // Add the given set of integers to the lexicon if it is not already
  // present, and return the unique id for this set.  "begin" and "end" are
  // forward iterators over a sequence of values that can be converted to
  // non-negative 32-bit integers.  The values are automatically sorted and
  // duplicates are removed.  Returns a signed integer representing this set.
  //
  // REQUIRES: All values in [begin, end) are non-negative 32-bit integers.
  template <class FwdIterator>
  int32 Add(FwdIterator begin, FwdIterator end);

  // Add the given set of integers to the lexicon if it is not already
  // present, and return the unique id for this set.  This is a convenience
  // method equivalent to Add(std::begin(container), std::end(container)).
  template <class Container>
  int32 Add(const Container& container);

  // Convenience method that returns the unique id for a singleton set.
  // Note that because singleton sets take up no space, this method is
  // const.  Equivalent to calling Add(&id, &id + 1).
  int32 AddSingleton(int32 id) const;

  // Convenience method that returns the unique id for the empty set.  Note
  // that because the empty set takes up no space and has a fixed id, this
  // method is static.  Equivalent to calling Add() with an empty container.
  static int32 EmptySetId();

  // Iterator type; please treat this as an opaque forward iterator.
  using Iterator = const int32*;

  // This class represents a set of integers stored in the IdSetLexicon.
  class IdSet {
   public:
    Iterator begin() const;
    Iterator end() const;
    size_t size() const;

   private:
    friend class IdSetLexicon;
    IdSet();
    IdSet(Iterator begin, Iterator end);
    explicit IdSet(int32 singleton_id);
    Iterator begin_, end_;
    int32 singleton_id_;
  };
  // Return the set of integers corresponding to an id returned by Add().
  IdSet id_set(int32 set_id) const;

 private:
  // Choose kEmptySetId to be the last id that will ever be generated.
  // (Non-negative ids are reserved for singleton sets.)
  static const int32 kEmptySetId = std::numeric_limits<int32>::min();
  int32 AddInternal(std::vector<int32>* ids);

  SequenceLexicon<int32> id_sets_;

  std::vector<int32> tmp_;  // temporary storage used during Add()
};


//////////////////   Implementation details follow   ////////////////////


inline IdSetLexicon::Iterator IdSetLexicon::IdSet::begin() const {
  return begin_;
}

inline IdSetLexicon::Iterator IdSetLexicon::IdSet::end() const {
  return end_;
}

inline size_t IdSetLexicon::IdSet::size() const {
  return end_ - begin_;
}

inline IdSetLexicon::IdSet::IdSet()
    : begin_(&singleton_id_), end_(begin_) {
}

inline IdSetLexicon::IdSet::IdSet(Iterator begin, Iterator end)
    : begin_(begin), end_(end) {
}

inline IdSetLexicon::IdSet::IdSet(int32 singleton_id)
    : begin_(&singleton_id_), end_(&singleton_id_ + 1),
      singleton_id_(singleton_id) {
}

inline int32 IdSetLexicon::AddSingleton(int32 id) const {
  S2_DCHECK_GE(id, 0);
  S2_DCHECK_LE(id, std::numeric_limits<int32>::max());
  // Singleton sets are represented by their element.
  return id;
}

/*static*/ inline int32 IdSetLexicon::EmptySetId() {
  return kEmptySetId;
}

template <class FwdIterator>
int32 IdSetLexicon::Add(FwdIterator begin, FwdIterator end) {
  tmp_.clear();
  for (; begin != end; ++begin) {
    S2_DCHECK_GE(*begin, 0);
    S2_DCHECK_LE(*begin, std::numeric_limits<int32>::max());
    tmp_.push_back(*begin);
  }
  return AddInternal(&tmp_);
}

template <class Container>
int32 IdSetLexicon::Add(const Container& container) {
  return Add(std::begin(container), std::end(container));
}

#endif  // S2_ID_SET_LEXICON_H_
