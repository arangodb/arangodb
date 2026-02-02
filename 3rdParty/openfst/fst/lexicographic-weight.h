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
// Lexicographic weight set and associated semiring operation definitions.
//
// A lexicographic weight is a sequence of weights, each of which must have the
// path property and Times() must be (strongly) cancellative
// (for all a,b,c != Zero(): Times(c, a) = Times(c, b) => a = b,
// Times(a, c) = Times(b, c) => a = b).
// The + operation on two weights a and b is the lexicographically
// prior of a and b.

#ifndef FST_LEXICOGRAPHIC_WEIGHT_H_
#define FST_LEXICOGRAPHIC_WEIGHT_H_

#include <cstdint>
#include <random>
#include <string>

#include <fst/log.h>

#include <fst/pair-weight.h>
#include <fst/weight.h>


namespace fst {

template <class W1, class W2>
class LexicographicWeight : public PairWeight<W1, W2> {
 public:
  static_assert(IsPath<W1>::value, "W1 must have path property.");
  static_assert(IsPath<W2>::value, "W2 must have path property.");

  using ReverseWeight = LexicographicWeight<typename W1::ReverseWeight,
                                            typename W2::ReverseWeight>;

  using PairWeight<W1, W2>::Value1;
  using PairWeight<W1, W2>::Value2;
  using PairWeight<W1, W2>::SetValue1;
  using PairWeight<W1, W2>::SetValue2;
  using PairWeight<W1, W2>::Zero;
  using PairWeight<W1, W2>::One;
  using PairWeight<W1, W2>::NoWeight;
  using PairWeight<W1, W2>::Quantize;
  using PairWeight<W1, W2>::Reverse;

  LexicographicWeight() {}

  explicit LexicographicWeight(const PairWeight<W1, W2> &w)
      : PairWeight<W1, W2>(w) {}

  LexicographicWeight(W1 w1, W2 w2) : PairWeight<W1, W2>(w1, w2) {}

  static const LexicographicWeight &Zero() {
    static const LexicographicWeight zero(PairWeight<W1, W2>::Zero());
    return zero;
  }

  static const LexicographicWeight &One() {
    static const LexicographicWeight one(PairWeight<W1, W2>::One());
    return one;
  }

  static const LexicographicWeight &NoWeight() {
    static const LexicographicWeight no_weight(PairWeight<W1, W2>::NoWeight());
    return no_weight;
  }

  static const std::string &Type() {
    static const std::string *const type =
        new std::string(W1::Type() + "_LT_" + W2::Type());
    return *type;
  }

  bool Member() const {
    if (!Value1().Member() || !Value2().Member()) return false;
    // Lexicographic weights cannot mix zeroes and non-zeroes.
    if (Value1() == W1::Zero() && Value2() == W2::Zero()) return true;
    if (Value1() != W1::Zero() && Value2() != W2::Zero()) return true;
    return false;
  }

  LexicographicWeight Quantize(float delta = kDelta) const {
    return LexicographicWeight(PairWeight<W1, W2>::Quantize());
  }

  ReverseWeight Reverse() const {
    return ReverseWeight(PairWeight<W1, W2>::Reverse());
  }

  static constexpr uint64_t Properties() {
    return W1::Properties() & W2::Properties() &
           (kLeftSemiring | kRightSemiring | kPath | kIdempotent |
            kCommutative);
  }
};

template <class W1, class W2>
inline LexicographicWeight<W1, W2> Plus(const LexicographicWeight<W1, W2> &w,
                                        const LexicographicWeight<W1, W2> &v) {
  if (!w.Member() || !v.Member()) {
    return LexicographicWeight<W1, W2>::NoWeight();
  }
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

// This function object generates weights by calling the underlying generators
// for the templated weight types, like all other pair weight types. However,
// for lexicographic weights, we cannot generate zeroes for the two subweights
// separately: weights are members iff both members are zero or both members
// are non-zero. This is intended primarily for testing.
template <class W1, class W2>
class WeightGenerate<LexicographicWeight<W1, W2>> {
 public:
  using Weight = LexicographicWeight<W1, W1>;
  using Generate1 = WeightGenerate<W1>;
  using Generate2 = WeightGenerate<W2>;

  explicit WeightGenerate(uint64_t seed = std::random_device()(),
                          bool allow_zero = true,
                          size_t num_random_weights = kNumRandomWeights)
      : rand_(seed),
        allow_zero_(allow_zero),
        num_random_weights_(num_random_weights),
        generator1_(seed, false, num_random_weights),
        generator2_(seed, false, num_random_weights) {}

  Weight operator()() const {
    if (allow_zero_) {
      const int sample =
          std::uniform_int_distribution<>(0, num_random_weights_)(rand_);
      if (sample == num_random_weights_) return Weight(W1::Zero(), W2::Zero());
    }
    return Weight(generator1_(), generator2_());
  }

 private:
  mutable std::mt19937_64 rand_;
  const bool allow_zero_;
  const size_t num_random_weights_;
  const Generate1 generator1_;
  const Generate2 generator2_;
};

}  // namespace fst

#endif  // FST_LEXICOGRAPHIC_WEIGHT_H_
