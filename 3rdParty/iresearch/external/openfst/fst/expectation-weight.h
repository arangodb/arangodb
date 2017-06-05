
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
// Inspiration: shumash@google.com (Masha Maria Shugrina)
// \file
// Expectation semiring as described by Jason Eisner:
// See: doi=10.1.1.22.9398
// Multiplex semiring operations and identities:
//    One: <One, Zero>
//    Zero: <Zero, Zero>
//    Plus: <a1, b1> + <a2, b2> = < (a1 + a2) , (b1 + b2) >
//    Times: <a1, b1> * <a2, b2> = < (a1 * a2) , [(a1 * b2) + (a2 * b1)] >
//    Division: Undefined (currently)
//
// Usually used to store the pair <probability, random_variable> so that
// ShortestDistance[Fst<ArcTpl<ExpectationWeight<P, V> > >]
//    == < PosteriorProbability, Expected_Value[V] >

#ifndef FST_LIB_EXPECTATION_WEIGHT_H_
#define FST_LIB_EXPECTATION_WEIGHT_H_

#include<string>

#include <fst/pair-weight.h>


namespace fst {

// X1 is usually a probability weight like LogWeight
// X2 is usually a random variable or vector
//    see SignedLogWeight or SparsePowerWeight
//
// If X1 is distinct from X2, it is required that there is an external
// product between X1 and X2 and if both semriring are commutative, or
// left or right semirings, then result must have those properties.
template <class X1, class X2>
class ExpectationWeight : public PairWeight<X1, X2> {
 public:
  using PairWeight<X1, X2>::Value1;
  using PairWeight<X1, X2>::Value2;

  using PairWeight<X1, X2>::Reverse;
  using PairWeight<X1, X2>::Quantize;
  using PairWeight<X1, X2>::Member;

  typedef X1 W1;
  typedef X2 W2;

  typedef ExpectationWeight<typename X1::ReverseWeight,
                            typename X2::ReverseWeight> ReverseWeight;

  ExpectationWeight() : PairWeight<X1, X2>(Zero()) { }

  ExpectationWeight(const ExpectationWeight<X1, X2>& w)
      : PairWeight<X1, X2> (w) { }

  ExpectationWeight(const PairWeight<X1, X2>& w)
      : PairWeight<X1, X2> (w) { }

  ExpectationWeight(const X1& x1, const X2& x2)
      : PairWeight<X1, X2>(x1, x2) { }

  static const ExpectationWeight<X1, X2> &Zero() {
    static const ExpectationWeight<X1, X2> zero(X1::Zero(), X2::Zero());
    return zero;
  }

  static const ExpectationWeight<X1, X2> &One() {
    static const ExpectationWeight<X1, X2> one(X1::One(), X2::Zero());
    return one;
  }

  static const ExpectationWeight<X1, X2> &NoWeight() {
    static const ExpectationWeight<X1, X2> no_weight(X1::NoWeight(),
                                                     X2::NoWeight());
    return no_weight;
  }

  static const string &Type() {
    static const string type = "expectation_" + X1::Type() + "_" + X2::Type();
    return type;
  }

  PairWeight<X1, X2> Quantize(float delta = kDelta) const {
    return PairWeight<X1, X2>::Quantize();
  }

  ReverseWeight Reverse() const {
    return PairWeight<X1, X2>::Reverse();
  }

  bool Member() const {
    return PairWeight<X1, X2>::Member();
  }

  static uint64 Properties() {
    uint64 props1 = W1::Properties();
    uint64 props2 = W2::Properties();
    return props1 & props2 & (kLeftSemiring | kRightSemiring |
                              kCommutative | kIdempotent);
  }
};

template <class X1, class X2>
inline ExpectationWeight<X1, X2> Plus(const ExpectationWeight<X1, X2> &w,
                                      const ExpectationWeight<X1, X2> &v) {
  return ExpectationWeight<X1, X2>(Plus(w.Value1(), v.Value1()),
                                   Plus(w.Value2(), v.Value2()));
}


template <class X1, class X2>
inline ExpectationWeight<X1, X2> Times(const ExpectationWeight<X1, X2> &w,
                                       const ExpectationWeight<X1, X2> &v) {
  return ExpectationWeight<X1, X2>(Times(w.Value1(), v.Value1()),
                                   Plus(Times(w.Value1(), v.Value2()),
                                        Times(w.Value2(), v.Value1())));
}

template <class X1, class X2>
inline ExpectationWeight<X1, X2> Divide(const ExpectationWeight<X1, X2> &w,
                                        const ExpectationWeight<X1, X2> &v,
                                        DivideType typ = DIVIDE_ANY) {
  FSTERROR() << "ExpectationWeight::Divide: not implemented";
  return ExpectationWeight<X1, X2>::NoWeight();
}

}  // namespace fst

#endif  // FST_LIB_EXPECTATION_WEIGHT_H_
