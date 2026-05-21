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
// Product weight set and associated semiring operation definitions.

#ifndef FST_PRODUCT_WEIGHT_H_
#define FST_PRODUCT_WEIGHT_H_

#include <cstdint>
#include <random>
#include <string>
#include <utility>


#include <fst/pair-weight.h>
#include <fst/weight.h>


namespace fst {

// Product semiring: W1 * W2.
template <class W1, class W2>
class ProductWeight : public PairWeight<W1, W2> {
 public:
  using ReverseWeight =
      ProductWeight<typename W1::ReverseWeight, typename W2::ReverseWeight>;

  ProductWeight() {}

  explicit ProductWeight(const PairWeight<W1, W2> &weight)
      : PairWeight<W1, W2>(weight) {}

  ProductWeight(W1 w1, W2 w2)
      : PairWeight<W1, W2>(std::move(w1), std::move(w2)) {}

  static const ProductWeight &Zero() {
    static const ProductWeight zero(PairWeight<W1, W2>::Zero());
    return zero;
  }

  static const ProductWeight &One() {
    static const ProductWeight one(PairWeight<W1, W2>::One());
    return one;
  }

  static const ProductWeight &NoWeight() {
    static const ProductWeight no_weight(PairWeight<W1, W2>::NoWeight());
    return no_weight;
  }

  static const std::string &Type() {
    static const std::string *const type =
        new std::string(W1::Type() + "_X_" + W2::Type());
    return *type;
  }

  static constexpr uint64_t Properties() {
    return W1::Properties() & W2::Properties() &
           (kLeftSemiring | kRightSemiring | kCommutative | kIdempotent);
  }

  ProductWeight Quantize(float delta = kDelta) const {
    return ProductWeight(PairWeight<W1, W2>::Quantize(delta));
  }

  ReverseWeight Reverse() const {
    return ReverseWeight(PairWeight<W1, W2>::Reverse());
  }
};

template <class W1, class W2>
inline ProductWeight<W1, W2> Plus(const ProductWeight<W1, W2> &w1,
                                  const ProductWeight<W1, W2> &w2) {
  return ProductWeight<W1, W2>(Plus(w1.Value1(), w2.Value1()),
                               Plus(w1.Value2(), w2.Value2()));
}

template <class W1, class W2>
inline ProductWeight<W1, W2> Times(const ProductWeight<W1, W2> &w1,
                                   const ProductWeight<W1, W2> &w2) {
  return ProductWeight<W1, W2>(Times(w1.Value1(), w2.Value1()),
                               Times(w1.Value2(), w2.Value2()));
}

template <class W1, class W2>
inline ProductWeight<W1, W2> Divide(const ProductWeight<W1, W2> &w1,
                                    const ProductWeight<W1, W2> &w2,
                                    DivideType typ = DIVIDE_ANY) {
  return ProductWeight<W1, W2>(Divide(w1.Value1(), w2.Value1(), typ),
                               Divide(w1.Value2(), w2.Value2(), typ));
}

// Specialization for product weight
template <class W1, class W2>
class Adder<ProductWeight<W1, W2>> {
 public:
  using Weight = ProductWeight<W1, W2>;

  Adder() {}

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
class WeightGenerate<ProductWeight<W1, W2>> {
 public:
  using Weight = ProductWeight<W1, W2>;
  using Generate = WeightGenerate<PairWeight<W1, W2>>;

  explicit WeightGenerate(uint64_t seed = std::random_device()(),
                          bool allow_zero = true)
      : generate_(seed, allow_zero) {}

  Weight operator()() const { return Weight(generate_()); }

 private:
  const Generate generate_;
};

}  // namespace fst

#endif  // FST_PRODUCT_WEIGHT_H_
