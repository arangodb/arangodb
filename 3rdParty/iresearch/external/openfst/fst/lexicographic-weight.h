// lexicographic-weight.h

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
// Author: rws@google.com (Richard Sproat)
//
// \file
// Lexicographic weight set and associated semiring operation definitions.
//
// A lexicographic weight is a sequence of weights, each of which must have the
// path property and Times() must be (strongly) cancellative
// (for all a,b,c != Zero(): Times(c, a) = Times(c, b) => a = b,
// Times(a, c) = Times(b, c) => a = b).
// The + operation on two weights a and b is the lexicographically
// prior of a and b.

#ifndef FST_LIB_LEXICOGRAPHIC_WEIGHT_H__
#define FST_LIB_LEXICOGRAPHIC_WEIGHT_H__

#include <string>

#include <fst/pair-weight.h>
#include <fst/weight.h>


namespace fst {

template<class W1, class W2>
class LexicographicWeight : public PairWeight<W1, W2> {
 public:
  using PairWeight<W1, W2>::Value1;
  using PairWeight<W1, W2>::Value2;
  using PairWeight<W1, W2>::SetValue1;
  using PairWeight<W1, W2>::SetValue2;
  using PairWeight<W1, W2>::Zero;
  using PairWeight<W1, W2>::One;
  using PairWeight<W1, W2>::NoWeight;
  using PairWeight<W1, W2>::Quantize;
  using PairWeight<W1, W2>::Reverse;

  typedef LexicographicWeight<typename W1::ReverseWeight,
                              typename W2::ReverseWeight>
  ReverseWeight;

  LexicographicWeight() {}

  LexicographicWeight(const PairWeight<W1, W2>& w)
      : PairWeight<W1, W2>(w) {}

  LexicographicWeight(W1 w1, W2 w2) : PairWeight<W1, W2>(w1, w2) {
    uint64 props = kPath;
    if ((W1::Properties() & props) != props) {
      FSTERROR() << "LexicographicWeight must "
                 << "have the path property: " << W1::Type();
      SetValue1(W1::NoWeight());
    }
    if ((W2::Properties() & props) != props) {
      FSTERROR() << "LexicographicWeight must "
                 << "have the path property: " << W2::Type();
      SetValue2(W2::NoWeight());
    }
  }

  static const LexicographicWeight<W1, W2> &Zero() {
    static const LexicographicWeight<W1, W2> zero(PairWeight<W1, W2>::Zero());
    return zero;
  }

  static const LexicographicWeight<W1, W2> &One() {
    static const LexicographicWeight<W1, W2> one(PairWeight<W1, W2>::One());
    return one;
  }

  static const LexicographicWeight<W1, W2> &NoWeight() {
    static const LexicographicWeight<W1, W2> no_weight(
        PairWeight<W1, W2>::NoWeight());
    return no_weight;
  }

  static const string &Type() {
    static const string type = W1::Type() + "_LT_" + W2::Type();
    return type;
  }

  bool Member() const {
    if (!Value1().Member() || !Value2().Member()) return false;
    // Lexicographic weights cannot mix zeroes and non-zeroes.
    if (Value1() == W1::Zero() && Value2() == W2::Zero()) return true;
    if (Value1() != W1::Zero() && Value2() != W2::Zero()) return true;
    return false;
  }

  LexicographicWeight<W1, W2> Quantize(float delta = kDelta) const {
    return PairWeight<W1, W2>::Quantize();
  }

  ReverseWeight Reverse() const {
    return PairWeight<W1, W2>::Reverse();
  }

  static uint64 Properties() {
    uint64 props1 = W1::Properties();
    uint64 props2 = W2::Properties();
    return props1 & props2 & (kLeftSemiring | kRightSemiring | kPath |
                              kIdempotent | kCommutative);
  }
};

template <class W1, class W2>
inline LexicographicWeight<W1, W2> Plus(const LexicographicWeight<W1, W2> &w,
                                        const LexicographicWeight<W1, W2> &v) {
  if (!w.Member() || !v.Member())
    return LexicographicWeight<W1, W2>::NoWeight();
  NaturalLess<W1> less1;
  NaturalLess<W2> less2;
  if (less1(w.Value1(), v.Value1())) return w;
  if (less1(v.Value1(), w.Value1())) return v;
  if (less2(w.Value2(), v.Value2())) return w;
  if (less2(v.Value2(), w.Value2())) return v;
  return w;
}

template <class W1, class W2>
inline LexicographicWeight<W1, W2> Times(const LexicographicWeight<W1, W2> &w,
                                         const LexicographicWeight<W1, W2> &v) {
  return LexicographicWeight<W1, W2>(Times(w.Value1(), v.Value1()),
                                     Times(w.Value2(), v.Value2()));
}

template <class W1, class W2>
inline LexicographicWeight<W1, W2> Divide(const LexicographicWeight<W1, W2> &w,
                                          const LexicographicWeight<W1, W2> &v,
                                          DivideType typ = DIVIDE_ANY) {
  return LexicographicWeight<W1, W2>(Divide(w.Value1(), v.Value1(), typ),
                                     Divide(w.Value2(), v.Value2(), typ));
}

}  // namespace fst

#endif  // FST_LIB_LEXICOGRAPHIC_WEIGHT_H__
