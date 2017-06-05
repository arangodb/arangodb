// tuple-weight.h

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
// Author: allauzen@google (Cyril Allauzen)
//
// \file
// Tuple weight set operation definitions.

#ifndef FST_LIB_TUPLE_WEIGHT_H__
#define FST_LIB_TUPLE_WEIGHT_H__

#include <string>
#include <vector>
using std::vector;

#include <fst/weight.h>


namespace fst {

// n-tuple weight, element of the n-th catersian power of W
template <class W, unsigned int n>
class TupleWeight {
 public:
  typedef TupleWeight<typename W::ReverseWeight, n> ReverseWeight;

  TupleWeight() {}

  TupleWeight(const TupleWeight &w) {
    for (size_t i = 0; i < n; ++i)
      values_[i] = w.values_[i];
  }

  template <class Iterator>
  TupleWeight(Iterator begin, Iterator end) {
    for (Iterator iter = begin; iter != end; ++iter)
      values_[iter - begin] = *iter;
  }

  TupleWeight(const W &w) {
    for (size_t i = 0; i < n; ++i)
      values_[i] = w;
  }

  static const TupleWeight<W, n> &Zero() {
    static const TupleWeight<W, n> zero(W::Zero());
    return zero;
  }

  static const TupleWeight<W, n> &One() {
    static const TupleWeight<W, n> one(W::One());
    return one;
  }

  static const TupleWeight<W, n> &NoWeight() {
    static const TupleWeight<W, n> no_weight(W::NoWeight());
    return no_weight;
  }

  static unsigned int Length() {
    return n;
  }

  istream &Read(istream &strm) {
    for (size_t i = 0; i < n; ++i)
      values_[i].Read(strm);
    return strm;
  }

  ostream &Write(ostream &strm) const {
    for (size_t i = 0; i < n; ++i)
      values_[i].Write(strm);
    return strm;
  }

  TupleWeight<W, n> &operator=(const TupleWeight<W, n> &w) {
    for (size_t i = 0; i < n; ++i)
      values_[i] = w.values_[i];
    return *this;
  }

  bool Member() const {
    bool member = true;
    for (size_t i = 0; i < n; ++i)
      member = member && values_[i].Member();
    return member;
  }

  size_t Hash() const {
    uint64 hash = 0;
    for (size_t i = 0; i < n; ++i)
      hash = 5 * hash + values_[i].Hash();
    return size_t(hash);
  }

  TupleWeight<W, n> Quantize(float delta = kDelta) const {
    TupleWeight<W, n> w;
    for (size_t i = 0; i < n; ++i)
      w.values_[i] = values_[i].Quantize(delta);
    return w;
  }

  ReverseWeight Reverse() const {
    TupleWeight<W, n> w;
    for (size_t i = 0; i < n; ++i)
      w.values_[i] = values_[i].Reverse();
    return w;
  }

  const W& Value(size_t i) const { return values_[i]; }

  void SetValue(size_t i, const W &w) { values_[i] = w; }

 private:
  W values_[n];
};

template <class W, unsigned int n>
inline bool operator==(const TupleWeight<W, n> &w1,
                       const TupleWeight<W, n> &w2) {
  bool equal = true;
  for (size_t i = 0; i < n; ++i)
    equal = equal && (w1.Value(i) == w2.Value(i));
  return equal;
}

template <class W, unsigned int n>
inline bool operator!=(const TupleWeight<W, n> &w1,
                       const TupleWeight<W, n> &w2) {
  bool not_equal = false;
  for (size_t i = 0; (i < n) && !not_equal; ++i)
    not_equal = not_equal || (w1.Value(i) != w2.Value(i));
  return not_equal;
}

template <class W, unsigned int n>
inline bool ApproxEqual(const TupleWeight<W, n> &w1,
                        const TupleWeight<W, n> &w2,
                        float delta = kDelta) {
  bool approx_equal = true;
  for (size_t i = 0; i < n; ++i)
    approx_equal = approx_equal &&
        ApproxEqual(w1.Value(i), w2.Value(i), delta);
  return approx_equal;
}

template <class W, unsigned int n>
inline ostream &operator<<(ostream &strm, const TupleWeight<W, n> &w) {
  CompositeWeightWriter writer(strm);
  writer.WriteBegin();
  for (size_t i = 0; i < n; ++i)
    writer.WriteElement(w.Value(i));
  writer.WriteEnd();
  return strm;
}

template <class W, unsigned int n>
inline istream &operator>>(istream &strm, TupleWeight<W, n> &w) {
  CompositeWeightReader reader(strm);
  reader.ReadBegin();
  W v;
  // Reads first n-1 elements
  for (size_t i = 0; i < n - 1; ++i) {
    reader.ReadElement(&v);
    w.SetValue(i, v);
  }
  // Reads  n-th element
  reader.ReadElement(&v, true);
  w.SetValue(n - 1 , v);

  reader.ReadEnd();
  return strm;
}



}  // namespace fst

#endif  // FST_LIB_TUPLE_WEIGHT_H__
