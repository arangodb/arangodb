// sparse-tuple-weight.h

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
// Copyright 2005-2010 Google, Inc.
// Author: krr@google.com (Kasturi Rangan Raghavan)
// Inspiration: allauzen@google.com (Cyril Allauzen)
// \file
// Sparse version of tuple-weight, based on tuple-weight.h
//   Internally stores sparse key, value pairs in linked list
//   Default value elemnt is the assumed value of unset keys
//   Internal singleton implementation that stores first key,
//   value pair as a initialized member variable to avoide
//   unnecessary allocation on heap.
// Use SparseTupleWeightIterator to iterate through the key,value pairs
// Note: this does NOT iterate through the default value.
//
// Sparse tuple weight set operation definitions.

#ifndef FST_LIB_SPARSE_TUPLE_WEIGHT_H__
#define FST_LIB_SPARSE_TUPLE_WEIGHT_H__

#include<string>
#include<list>
#include<stack>
#include<unordered_map>
using std::unordered_map;
using std::unordered_multimap;

#include <fst/weight.h>


namespace fst {

template<class W, class K>
class SparseTupleWeightIterator;

// Arbitrary dimension tuple weight, stored as a sorted linked-list
// W is any weight class,
// K is the key value type. kNoKey(-1) is reserved for internal use
template <class W, class K = int>
class SparseTupleWeight {
 public:
  typedef pair<K, W> Pair;
  typedef SparseTupleWeight<typename W::ReverseWeight, K> ReverseWeight;

  const static K kNoKey = -1;
  SparseTupleWeight() {
    Init();
  }

  template <class Iterator>
  SparseTupleWeight(Iterator begin, Iterator end) {
    Init();
    // Assumes input iterator is sorted
    for (Iterator it = begin; it != end; ++it)
      Push(*it);
  }


  SparseTupleWeight(const K& key, const W &w) {
    Init();
    Push(key, w);
  }

  SparseTupleWeight(const W &w) {
    Init(w);
  }

  SparseTupleWeight(const SparseTupleWeight<W, K> &w) {
    Init(w.DefaultValue());
    SetDefaultValue(w.DefaultValue());
    for (SparseTupleWeightIterator<W, K> it(w); !it.Done(); it.Next()) {
      Push(it.Value());
    }
  }

  static const SparseTupleWeight<W, K> &Zero() {
    static SparseTupleWeight<W, K> zero;
    return zero;
  }

  static const SparseTupleWeight<W, K> &One() {
    static SparseTupleWeight<W, K> one(W::One());
    return one;
  }

  static const SparseTupleWeight<W, K> &NoWeight() {
    static SparseTupleWeight<W, K> no_weight(W::NoWeight());
    return no_weight;
  }

  istream &Read(istream &strm) {
    ReadType(strm, &default_);
    ReadType(strm, &first_);
    return ReadType(strm, &rest_);
  }

  ostream &Write(ostream &strm) const {
    WriteType(strm, default_);
    WriteType(strm, first_);
    return WriteType(strm, rest_);
  }

  SparseTupleWeight<W, K> &operator=(const SparseTupleWeight<W, K> &w) {
    if (this == &w) return *this; // check for w = w
    Init(w.DefaultValue());
    for (SparseTupleWeightIterator<W, K> it(w); !it.Done(); it.Next()) {
      Push(it.Value());
    }
    return *this;
  }

  bool Member() const {
    if (!DefaultValue().Member()) return false;
    for (SparseTupleWeightIterator<W, K> it(*this); !it.Done(); it.Next()) {
      if (!it.Value().second.Member()) return false;
    }
    return true;
  }

  // Assumes H() function exists for the hash of the key value
  size_t Hash() const {
    uint64 h = 0;
    std::hash<K> H;
    for (SparseTupleWeightIterator<W, K> it(*this); !it.Done(); it.Next()) {
      h = 5 * h + H(it.Value().first);
      h = 13 * h + it.Value().second.Hash();
    }
    return size_t(h);
  }

  SparseTupleWeight<W, K> Quantize(float delta = kDelta) const {
    SparseTupleWeight<W, K> w;
    for (SparseTupleWeightIterator<W, K> it(*this); !it.Done(); it.Next()) {
      w.Push(it.Value().first, it.Value().second.Quantize(delta));
    }
    return w;
  }

  ReverseWeight Reverse() const {
    SparseTupleWeight<W, K> w;
    for (SparseTupleWeightIterator<W, K> it(*this); !it.Done(); it.Next()) {
      w.Push(it.Value().first, it.Value().second.Reverse());
    }
    return w;
  }

  // Common initializer among constructors.
  void Init() {
    Init(W::Zero());
  }

  void Init(const W& default_value) {
    first_.first = kNoKey;
    /* initialized to the reserved key value */
    default_ = default_value;
    rest_.clear();
  }

  size_t Size() const {
    if (first_.first == kNoKey)
      return 0;
    else
      return  rest_.size() + 1;
  }

  inline void Push(const K &k, const W &w, bool default_value_check = true) {
    Push(std::make_pair(k, w), default_value_check);
  }

  inline void Push(const Pair &p, bool default_value_check = true) {
    if (default_value_check && p.second == default_) return;
    if (first_.first == kNoKey) {
      first_ = p;
    } else {
      rest_.push_back(p);
    }
  }

  void SetDefaultValue(const W& val) { default_ = val; }

  const W& DefaultValue() const { return default_; }

 private:
  // Assumed default value of uninitialized keys, by default W::Zero()
  W default_;

  // Key values pairs are first stored in first_, then fill rest_
  // this way we can avoid dynamic allocation in the common case
  // where the weight is a single key,val pair.
  Pair first_;
  list<Pair> rest_;

  friend class SparseTupleWeightIterator<W, K>;
};

template<class W, class K>
class SparseTupleWeightIterator {
 public:
  typedef typename SparseTupleWeight<W, K>::Pair Pair;
  typedef typename list<Pair>::const_iterator const_iterator;
  typedef typename list<Pair>::iterator iterator;

  explicit SparseTupleWeightIterator(const SparseTupleWeight<W, K>& w)
    : first_(w.first_), rest_(w.rest_), init_(true),
      iter_(rest_.begin()) {}

  bool Done() const {
    if (init_)
      return first_.first == SparseTupleWeight<W, K>::kNoKey;
    else
      return iter_ == rest_.end();
  }

  const Pair& Value() const { return init_ ? first_ : *iter_; }

  void Next() {
    if (init_)
      init_ = false;
    else
      ++iter_;
  }

  void Reset() {
    init_ = true;
    iter_ = rest_.begin();
  }

 private:
  const Pair &first_;
  const list<Pair> & rest_;
  bool init_;  // in the initialized state?
  typename list<Pair>::const_iterator iter_;

  DISALLOW_COPY_AND_ASSIGN(SparseTupleWeightIterator);
};

template<class W, class K, class M>
inline void SparseTupleWeightMap(
  SparseTupleWeight<W, K>* ret,
  const SparseTupleWeight<W, K>& w1,
  const SparseTupleWeight<W, K>& w2,
  const M& operator_mapper) {
  SparseTupleWeightIterator<W, K> w1_it(w1);
  SparseTupleWeightIterator<W, K> w2_it(w2);
  const W& v1_def = w1.DefaultValue();
  const W& v2_def = w2.DefaultValue();
  ret->SetDefaultValue(operator_mapper.Map(0, v1_def, v2_def));
  while (!w1_it.Done() || !w2_it.Done()) {
    const K& k1 = (w1_it.Done()) ? w2_it.Value().first : w1_it.Value().first;
    const K& k2 = (w2_it.Done()) ? w1_it.Value().first : w2_it.Value().first;
    const W& v1 = (w1_it.Done()) ? v1_def : w1_it.Value().second;
    const W& v2 = (w2_it.Done()) ? v2_def : w2_it.Value().second;
    if (k1 == k2) {
      ret->Push(k1, operator_mapper.Map(k1, v1, v2));
      if (!w1_it.Done()) w1_it.Next();
      if (!w2_it.Done()) w2_it.Next();
    } else if (k1 < k2) {
      ret->Push(k1, operator_mapper.Map(k1, v1, v2_def));
      w1_it.Next();
    } else {
      ret->Push(k2, operator_mapper.Map(k2, v1_def, v2));
      w2_it.Next();
    }
  }
}

template <class W, class K>
inline bool operator==(const SparseTupleWeight<W, K> &w1,
                       const SparseTupleWeight<W, K> &w2) {
  const W& v1_def = w1.DefaultValue();
  const W& v2_def = w2.DefaultValue();
  if (v1_def != v2_def) return false;

  SparseTupleWeightIterator<W, K> w1_it(w1);
  SparseTupleWeightIterator<W, K> w2_it(w2);
  while (!w1_it.Done() || !w2_it.Done()) {
    const K& k1 = (w1_it.Done()) ? w2_it.Value().first : w1_it.Value().first;
    const K& k2 = (w2_it.Done()) ? w1_it.Value().first : w2_it.Value().first;
    const W& v1 = (w1_it.Done()) ? v1_def : w1_it.Value().second;
    const W& v2 = (w2_it.Done()) ? v2_def : w2_it.Value().second;
    if (k1 == k2) {
      if (v1 != v2) return false;
      if (!w1_it.Done()) w1_it.Next();
      if (!w2_it.Done()) w2_it.Next();
    } else if (k1 < k2) {
      if (v1 != v2_def) return false;
      w1_it.Next();
    } else {
      if (v1_def != v2) return false;
      w2_it.Next();
    }
  }
  return true;
}

template <class W, class K>
inline bool operator!=(const SparseTupleWeight<W, K> &w1,
                       const SparseTupleWeight<W, K> &w2) {
  return !(w1 == w2);
}

template <class W, class K>
inline ostream &operator<<(ostream &strm, const SparseTupleWeight<W, K> &w) {
  CompositeWeightWriter writer(strm);
  writer.WriteBegin();
  writer.WriteElement(w.DefaultValue());
  for (SparseTupleWeightIterator<W, K> it(w); !it.Done(); it.Next()) {
    writer.WriteElement(it.Value().first);
    writer.WriteElement(it.Value().second);
  }
  writer.WriteEnd();
  return strm;
}

template <class W, class K>
inline istream &operator>>(istream &strm, SparseTupleWeight<W, K> &w) {
  CompositeWeightReader reader(strm);
  reader.ReadBegin();
  W def;
  bool more = reader.ReadElement(&def);
  w.Init(def);

  while (more) {
    K key;
    reader.ReadElement(&key);

    W v;
    more = reader.ReadElement(&v);
    w.Push(key, v);
  }
  reader.ReadEnd();
  return strm;
}

}  // namespace fst

#endif  // FST_LIB_SPARSE_TUPLE_WEIGHT_H__
