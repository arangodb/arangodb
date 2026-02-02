// Copyright 2005-2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the 'License');
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an 'AS IS' BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.
//
// Expectation semiring, as described in:
//
//   Eisner, J. 2002. Parameter estimation for probabilistic finite-state
//   transducers. In Proceedings of the 40th Annual Meeting of the
//   Association for Computational Linguistics, pages 1-8.
//
// Multiplex semiring operations and identities:
//
//    One: <One, Zero>
//    Zero: <Zero, Zero>
//    Plus: <a1, b1> + <a2, b2> = <(a1 + a2), (b1 + b2)>
//    Times: <a1, b1> + <a2, b2> = <(a1 * a2), [(a1 * b2) + (a2 * b1)]>
//    Division (see Divide() for proof):
//      For a left-semiring:
//        <a1, b1> / <a2, b2> = <a1 / a2, (b1 - b2 * (a1 / a2)) / a2>
//      For a right-semiring:
//        <a1, b1> / <a2, b2> = <a1 / a2, (b1 - (a1 / a2) * b2) / a2>
//
// It is commonly used to store a probability, random variable pair so that
// the shortest distance gives the posterior probability and the associated
// expected value.

#ifndef FST_EXPECTATION_WEIGHT_H_
#define FST_EXPECTATION_WEIGHT_H_

#include <cstdint>
#include <string>

#include <fst/log.h>

#include <fst/pair-weight.h>


namespace fst {

// W1 is usually a probability weight like LogWeight.
// W2 is usually a random variable or vector (see SignedLogWeight or
// SparsePowerWeight).
//
// If W1 is distinct from W2, it is required that there is an external product
// between W1 and W2 (that is, both Times(W1, W2) -> W2 and Times(W2, W1) -> W2
// must be defined) and if both semirings are commutative, or left or right
// semirings, then the result must have those properties.
template <class W1, class W2>
class ExpectationWeight : public PairWeight<W1, W2> {
 public:
  using PairWeight<W1, W2>::Value1;
  using PairWeight<W1, W2>::Value2;

  using PairWeight<W1, W2>::Reverse;
  using PairWeight<W1, W2>::Quantize;
  using PairWeight<W1, W2>::Member;

  using ReverseWeight =
      ExpectationWeight<typename W1::ReverseWeight, typename W2::ReverseWeight>;

  ExpectationWeight() : PairWeight<W1, W2>(Zero()) {}

  explicit ExpectationWeight(const PairWeight<W1, W2> &weight)
      : PairWeight<W1, W2>(weight) {}

  ExpectationWeight(const W1 &w1, const W2 &w2) : PairWeight<W1, W2>(w1, w2) {}

  static const ExpectationWeight &Zero() {
    static const ExpectationWeight zero(W1::Zero(), W2::Zero());
    return zero;
  }

  static const ExpectationWeight &One() {
    static const ExpectationWeight one(W1::One(), W2::Zero());
    return one;
  }

  static const ExpectationWeight &NoWeight() {
    static const ExpectationWeight no_weight(W1::NoWeight(), W2::NoWeight());
    return no_weight;
  }

  static const std::string &Type() {
    static const std::string *const type =
        new std::string("expectation_" + W1::Type() + "_" + W2::Type());
    return *type;
  }

  ExpectationWeight Quantize(float delta = kDelta) const {
    return ExpectationWeight(PairWeight<W1, W2>::Quantize(delta));
  }

  ReverseWeight Reverse() const {
    return ReverseWeight(PairWeight<W1, W2>::Reverse());
  }

  bool Member() const { return PairWeight<W1, W2>::Member(); }

  static constexpr uint64_t Properties() {
    return W1::Properties() & W2::Properties() &
           (kLeftSemiring | kRightSemiring | kCommutative | kIdempotent);
  }
};

template <class W1, class W2>
inline ExpectationWeight<W1, W2> Plus(const ExpectationWeight<W1, W2> &w1,
                                      const ExpectationWeight<W1, W2> &w2) {
  return ExpectationWeight<W1, W2>(Plus(w1.Value1(), w2.Value1()),
                                   Plus(w1.Value2(), w2.Value2()));
}

template <class W1, class W2>
inline ExpectationWeight<W1, W2> Times(const ExpectationWeight<W1, W2> &w1,
                                       const ExpectationWeight<W1, W2> &w2) {
  return ExpectationWeight<W1, W2>(
      Times(w1.Value1(), w2.Value1()),
      Plus(Times(w1.Value1(), w2.Value2()), Times(w1.Value2(), w2.Value1())));
}

// Requires
// * Divide(W1, W1) -> W1
// * Divide(W2, W1) -> W2
// * Times(W1, W2) -> W2
//   (already required by Times(ExpectationWeight, ExpectationWeight).)
// * Minus(W2, W2) -> W2
//   (not part of the Weight interface, so Divide will not compile if
//    Minus is not defined).
template <class W1, class W2>
inline ExpectationWeight<W1, W2> Divide(const ExpectationWeight<W1, W2> &w1,
                                        const ExpectationWeight<W1, W2> &w2,
                                        DivideType typ) {
  // No special cases are required for !w1.Member(), !w2.Member(), or
  // w2 == Zero(), since Minus and Divide will already return NoWeight()
  // in these cases.

  // For a right-semiring, by the definition of Divide, we are looking for
  // z = x / y such that (x / y) * y = x.
  // Let <x1, x2> = x, <y1, y2> = y, <z1, z2> = z.
  // <z1, z2> * <y1, y2> = <x1, x2>.
  // By the definition of Times:
  // z1 * y1 = x1 and
  // z1 * y2 + z2 * y1 = x2.
  // So z1 = x1 / y1, and
  // z2 * y2 = x2 - z1 * y2
  // z2 = (x2 - z1 * y2) / y2.
  // The left-semiring case is symmetric. The commutative case allows
  // additional simplification to
  // z2 = z1 * (x2 / x1 - y2 / y1) if x1 != 0
  // z2 = x2 / y1 if x1 == 0, but this requires testing against 0
  // with ApproxEquals. We just use the right-semiring result in
  // this case.
  const auto w11 = w1.Value1();
  const auto w12 = w1.Value2();
  const auto w21 = w2.Value1();
  const auto w22 = w2.Value2();
  const W1 q1 = Divide(w11, w21, typ);
  if (typ == DIVIDE_LEFT) {
    const W2 q2 = Divide(Minus(w12, Times(w22, q1)), w21, typ);
    return ExpectationWeight<W1, W2>(q1, q2);
  } else {
    // Right or commutative semiring.
    const W2 q2 = Divide(Minus(w12, Times(q1, w22)), w21, typ);
    return ExpectationWeight<W1, W2>(q1, q2);
  }
}

// Specialization for expectation weight.
template <class W1, class W2>
class Adder<ExpectationWeight<W1, W2>> {
 public:
  using Weight = ExpectationWeight<W1, W2>;

  Adder() = default;

  explicit Adder(Weight w) : adder1_(w.Value1()), adder2_(w.Value2()) {}

  Weight Add(const Weight &w) {
    adder1_.Add(w.Value1());
    adder2_.Add(w.Value2());
    return Sum();
  }

  Weight Sum() const { return Weight(adder1_.Sum(), adder2_.Sum()); }

  void Reset(Weight w = Weight::Zero()) {
    adder1_.Reset(w.Value1());
    adder2_.Reset(w.Value2());
  }

 private:
  Adder<W1> adder1_;
  Adder<W2> adder2_;
};

// This function object generates weights by calling the underlying generators
// for the template weight types, like all other pair weight types. This is
// intended primarily for testing.
template <class W1, class W2>
class WeightGenerate<ExpectationWeight<W1, W2>> {
 public:
  using Weight = ExpectationWeight<W1, W2>;
  using Generate = WeightGenerate<PairWeight<W1, W2>>;

  explicit WeightGenerate(uint64_t seed = std::random_device()(),
                          bool allow_zero = true)
      : generate_(seed, allow_zero) {}

  Weight operator()() const { return Weight(generate_()); }

 private:
  const Generate generate_;
};

}  // namespace fst

#endif  // FST_EXPECTATION_WEIGHT_H_
