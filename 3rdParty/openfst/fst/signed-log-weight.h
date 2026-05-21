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
// LogWeight along with sign information that represents the value X in the
// linear domain as <sign(X), -ln(|X|)>
//
// The sign is a TropicalWeight:
//  positive, TropicalWeight.Value() > 0.0, recommended value 1.0
//  negative, TropicalWeight.Value() <= 0.0, recommended value -1.0

#ifndef FST_SIGNED_LOG_WEIGHT_H_
#define FST_SIGNED_LOG_WEIGHT_H_

#include <cstdint>
#include <random>


#include <fst/float-weight.h>
#include <fst/pair-weight.h>
#include <fst/product-weight.h>


namespace fst {
template <class T>
class SignedLogWeightTpl : public PairWeight<TropicalWeight, LogWeightTpl<T>> {
 public:
  using W1 = TropicalWeight;
  using W2 = LogWeightTpl<T>;
  using ReverseWeight = SignedLogWeightTpl;

  using PairWeight<W1, W2>::Value1;
  using PairWeight<W1, W2>::Value2;

  SignedLogWeightTpl() noexcept : PairWeight<W1, W2>() {}

  // Conversion from plain LogWeightTpl.
  // NOLINTNEXTLINE(google-explicit-constructor)
  SignedLogWeightTpl(const W2 &w2) : PairWeight<W1, W2>(W1(1.0), w2) {}

  explicit SignedLogWeightTpl(const PairWeight<W1, W2> &weight)
      : PairWeight<W1, W2>(weight) {}

  SignedLogWeightTpl(const W1 &w1, const W2 &w2) : PairWeight<W1, W2>(w1, w2) {}

  static const SignedLogWeightTpl &Zero() {
    static const SignedLogWeightTpl zero(W1(1.0), W2::Zero());
    return zero;
  }

  static const SignedLogWeightTpl &One() {
    static const SignedLogWeightTpl one(W1(1.0), W2::One());
    return one;
  }

  static const SignedLogWeightTpl &NoWeight() {
    static const SignedLogWeightTpl no_weight(W1(1.0), W2::NoWeight());
    return no_weight;
  }

  static const std::string &Type() {
    static const std::string *const type =
        new std::string("signed_log_" + W1::Type() + "_" + W2::Type());
    return *type;
  }

  bool IsPositive() const { return Value1().Value() > 0; }

  SignedLogWeightTpl Quantize(float delta = kDelta) const {
    return SignedLogWeightTpl(PairWeight<W1, W2>::Quantize(delta));
  }

  ReverseWeight Reverse() const {
    return SignedLogWeightTpl(PairWeight<W1, W2>::Reverse());
  }

  bool Member() const { return PairWeight<W1, W2>::Member(); }

  // Neither idempotent nor path.
  static constexpr uint64_t Properties() {
    return kLeftSemiring | kRightSemiring | kCommutative;
  }

  size_t Hash() const {
    size_t h1;
    if (Value2() == W2::Zero() || IsPositive()) {
      h1 = TropicalWeight(1.0).Hash();
    } else {
      h1 = TropicalWeight(-1.0).Hash();
    }
    size_t h2 = Value2().Hash();
    static constexpr int lshift = 5;
    static constexpr int rshift = CHAR_BIT * sizeof(size_t) - 5;
    return h1 << lshift ^ h1 >> rshift ^ h2;
  }
};

template <class T>
inline SignedLogWeightTpl<T> Plus(const SignedLogWeightTpl<T> &w1,
                                  const SignedLogWeightTpl<T> &w2) {
  using W1 = TropicalWeight;
  using W2 = LogWeightTpl<T>;
  if (!w1.Member() || !w2.Member()) return SignedLogWeightTpl<T>::NoWeight();
  const auto s1 = w1.IsPositive();
  const auto s2 = w2.IsPositive();
  const bool equal = (s1 == s2);
  const auto f1 = w1.Value2().Value();
  const auto f2 = w2.Value2().Value();
  if (f1 == FloatLimits<T>::PosInfinity()) {
    return w2;
  } else if (f2 == FloatLimits<T>::PosInfinity()) {
    return w1;
  } else if (f1 == f2) {
    if (equal) {
      return SignedLogWeightTpl<T>(W1(w1.Value1()), W2(f2 - M_LN2));
    } else {
      return SignedLogWeightTpl<T>::Zero();
    }
  } else if (f1 > f2) {
    if (equal) {
      return SignedLogWeightTpl<T>(W1(w1.Value1()),
                                   W2(f2 - internal::LogPosExp(f1 - f2)));
    } else {
      return SignedLogWeightTpl<T>(W1(w2.Value1()),
                                   W2((f2 - internal::LogNegExp(f1 - f2))));
    }
  } else {
    if (equal) {
      return SignedLogWeightTpl<T>(W1(w2.Value1()),
                                   W2((f1 - internal::LogPosExp(f2 - f1))));
    } else {
      return SignedLogWeightTpl<T>(W1(w1.Value1()),
                                   W2((f1 - internal::LogNegExp(f2 - f1))));
    }
  }
}

template <class T>
inline SignedLogWeightTpl<T> Minus(const SignedLogWeightTpl<T> &w1,
                                   const SignedLogWeightTpl<T> &w2) {
  SignedLogWeightTpl<T> minus_w2(-w2.Value1().Value(), w2.Value2());
  return Plus(w1, minus_w2);
}

template <class T>
inline SignedLogWeightTpl<T> Times(const SignedLogWeightTpl<T> &w1,
                                   const SignedLogWeightTpl<T> &w2) {
  using W2 = LogWeightTpl<T>;
  if (!w1.Member() || !w2.Member()) return SignedLogWeightTpl<T>::NoWeight();
  const auto s1 = w1.IsPositive();
  const auto s2 = w2.IsPositive();
  const auto f1 = w1.Value2().Value();
  const auto f2 = w2.Value2().Value();
  if (s1 == s2) {
    return SignedLogWeightTpl<T>(TropicalWeight(1.0), W2(f1 + f2));
  } else {
    return SignedLogWeightTpl<T>(TropicalWeight(-1.0), W2(f1 + f2));
  }
}

template <class T>
inline SignedLogWeightTpl<T> Divide(const SignedLogWeightTpl<T> &w1,
                                    const SignedLogWeightTpl<T> &w2,
                                    DivideType typ = DIVIDE_ANY) {
  using W2 = LogWeightTpl<T>;
  if (!w1.Member() || !w2.Member()) return SignedLogWeightTpl<T>::NoWeight();
  const auto s1 = w1.IsPositive();
  const auto s2 = w2.IsPositive();
  const auto f1 = w1.Value2().Value();
  const auto f2 = w2.Value2().Value();
  if (f2 == FloatLimits<T>::PosInfinity()) {
    return SignedLogWeightTpl<T>(TropicalWeight(1.0),
                                 W2(FloatLimits<T>::NumberBad()));
  } else if (f1 == FloatLimits<T>::PosInfinity()) {
    return SignedLogWeightTpl<T>(TropicalWeight(1.0),
                                 W2(FloatLimits<T>::PosInfinity()));
  } else if (s1 == s2) {
    return SignedLogWeightTpl<T>(TropicalWeight(1.0), W2(f1 - f2));
  } else {
    return SignedLogWeightTpl<T>(TropicalWeight(-1.0), W2(f1 - f2));
  }
}

template <class T>
inline bool ApproxEqual(const SignedLogWeightTpl<T> &w1,
                        const SignedLogWeightTpl<T> &w2, float delta = kDelta) {
  using W2 = LogWeightTpl<T>;
  if (w1.IsPositive() == w2.IsPositive()) {
    return ApproxEqual(w1.Value2(), w2.Value2(), delta);
  } else {
    return ApproxEqual(w1.Value2(), W2::Zero(), delta) &&
           ApproxEqual(w2.Value2(), W2::Zero(), delta);
  }
}

template <class T>
inline bool operator==(const SignedLogWeightTpl<T> &w1,
                       const SignedLogWeightTpl<T> &w2) {
  using W2 = LogWeightTpl<T>;
  if (w1.IsPositive() == w2.IsPositive()) {
    return w1.Value2() == w2.Value2();
  } else {
    return w1.Value2() == W2::Zero() && w2.Value2() == W2::Zero();
  }
}

template <class T>
inline bool operator!=(const SignedLogWeightTpl<T> &w1,
                       const SignedLogWeightTpl<T> &w2) {
  return !(w1 == w2);
}

// All functions and operators with a LogWeightTpl arg need to be
// explicitly specified since the implicit constructor will not be
// tried in conjunction with function overloading.

template <class T>
inline SignedLogWeightTpl<T> Plus(const LogWeightTpl<T> &w1,
                                  const SignedLogWeightTpl<T> &w2) {
  return Plus(SignedLogWeightTpl<T>(w1), w2);
}

template <class T>
inline SignedLogWeightTpl<T> Plus(const SignedLogWeightTpl<T> &w1,
                                  const LogWeightTpl<T> &w2) {
  return Plus(w1, SignedLogWeightTpl<T>(w2));
}

template <class T>
inline SignedLogWeightTpl<T> Minus(const LogWeightTpl<T> &w1,
                                   const SignedLogWeightTpl<T> &w2) {
  return Minus(SignedLogWeightTpl<T>(w1), w2);
}

template <class T>
inline SignedLogWeightTpl<T> Minus(const SignedLogWeightTpl<T> &w1,
                                   const LogWeightTpl<T> &w2) {
  return Minus(w1, SignedLogWeightTpl<T>(w2));
}

template <class T>
inline SignedLogWeightTpl<T> Times(const LogWeightTpl<T> &w1,
                                   const SignedLogWeightTpl<T> &w2) {
  return Times(SignedLogWeightTpl<T>(w1), w2);
}

template <class T>
inline SignedLogWeightTpl<T> Times(const SignedLogWeightTpl<T> &w1,
                                   const LogWeightTpl<T> &w2) {
  return Times(w1, SignedLogWeightTpl<T>(w2));
}

template <class T>
inline SignedLogWeightTpl<T> Divide(const LogWeightTpl<T> &w1,
                                    const SignedLogWeightTpl<T> &w2,
                                    DivideType typ = DIVIDE_ANY) {
  return Divide(SignedLogWeightTpl<T>(w1), w2, typ);
}

template <class T>
inline SignedLogWeightTpl<T> Divide(const SignedLogWeightTpl<T> &w1,
                                    const LogWeightTpl<T> &w2,
                                    DivideType typ = DIVIDE_ANY) {
  return Divide(w1, SignedLogWeightTpl<T>(w2), typ);
}

template <class T>
inline bool ApproxEqual(const LogWeightTpl<T> &w1,
                        const SignedLogWeightTpl<T> &w2, float delta = kDelta) {
  return ApproxEqual(LogWeightTpl<T>(w1), w2, delta);
}

template <class T>
inline bool ApproxEqual(const SignedLogWeightTpl<T> &w1,
                        const LogWeightTpl<T> &w2, float delta = kDelta) {
  return ApproxEqual(w1, LogWeightTpl<T>(w2), delta);
}

template <class T>
inline bool operator==(const LogWeightTpl<T> &w1,
                       const SignedLogWeightTpl<T> &w2) {
  return SignedLogWeightTpl<T>(w1) == w2;
}

template <class T>
inline bool operator==(const SignedLogWeightTpl<T> &w1,
                       const LogWeightTpl<T> &w2) {
  return w1 == SignedLogWeightTpl<T>(w2);
}

template <class T>
inline bool operator!=(const LogWeightTpl<T> &w1,
                       const SignedLogWeightTpl<T> &w2) {
  return SignedLogWeightTpl<T>(w1) != w2;
}

template <class T>
inline bool operator!=(const SignedLogWeightTpl<T> &w1,
                       const LogWeightTpl<T> &w2) {
  return w1 != SignedLogWeightTpl<T>(w2);
}

// Single-precision signed-log weight.
using SignedLogWeight = SignedLogWeightTpl<float>;

// Double-precision signed-log weight.
using SignedLog64Weight = SignedLogWeightTpl<double>;

template <class W1, class W2>
bool SignedLogConvertCheck(W1 weight) {
  if (weight.Value1().Value() < 0.0) {
    FSTERROR() << "WeightConvert: Can't convert weight " << weight << " from "
               << W1::Type() << " to " << W2::Type();
    return false;
  }
  return true;
}

// Specialization using the Kahan compensated summation
template <class T>
class Adder<SignedLogWeightTpl<T>> {
 public:
  using Weight = SignedLogWeightTpl<T>;
  using W1 = TropicalWeight;
  using W2 = LogWeightTpl<T>;

  explicit Adder(Weight w = Weight::Zero())
      : ssum_(w.IsPositive()), sum_(w.Value2().Value()), c_(0.0) {}

  Weight Add(const Weight &w) {
    const auto sw = w.IsPositive();
    const auto f = w.Value2().Value();
    const bool equal = (ssum_ == sw);

    if (!Sum().Member() || f == FloatLimits<T>::PosInfinity()) {
      return Sum();
    } else if (!w.Member() || sum_ == FloatLimits<T>::PosInfinity()) {
      sum_ = f;
      ssum_ = sw;
      c_ = 0.0;
    } else if (f == sum_) {
      if (equal) {
        sum_ = internal::KahanLogSum(sum_, f, &c_);
      } else {
        sum_ = FloatLimits<T>::PosInfinity();
        ssum_ = true;
        c_ = 0.0;
      }
    } else if (f > sum_) {
      if (equal) {
        sum_ = internal::KahanLogSum(sum_, f, &c_);
      } else {
        sum_ = internal::KahanLogDiff(sum_, f, &c_);
      }
    } else {
      if (equal) {
        sum_ = internal::KahanLogSum(f, sum_, &c_);
      } else {
        sum_ = internal::KahanLogDiff(f, sum_, &c_);
        ssum_ = sw;
      }
    }
    return Sum();
  }

  Weight Sum() const { return Weight(W1(ssum_ ? 1.0 : -1.0), W2(sum_)); }

  void Reset(Weight w = Weight::Zero()) {
    ssum_ = w.IsPositive();
    sum_ = w.Value2().Value();
    c_ = 0.0;
  }

 private:
  bool ssum_;   // true iff sign of sum is positive
  double sum_;  // unsigned sum
  double c_;    // Kahan compensation
};

// Converts to tropical.
template <>
struct WeightConvert<SignedLogWeight, TropicalWeight> {
  TropicalWeight operator()(const SignedLogWeight &weight) const {
    if (!SignedLogConvertCheck<SignedLogWeight, TropicalWeight>(weight)) {
      return TropicalWeight::NoWeight();
    }
    return TropicalWeight(weight.Value2().Value());
  }
};

template <>
struct WeightConvert<SignedLog64Weight, TropicalWeight> {
  TropicalWeight operator()(const SignedLog64Weight &weight) const {
    if (!SignedLogConvertCheck<SignedLog64Weight, TropicalWeight>(weight)) {
      return TropicalWeight::NoWeight();
    }
    return TropicalWeight(weight.Value2().Value());
  }
};

// Converts to log.
template <>
struct WeightConvert<SignedLogWeight, LogWeight> {
  LogWeight operator()(const SignedLogWeight &weight) const {
    if (!SignedLogConvertCheck<SignedLogWeight, LogWeight>(weight)) {
      return LogWeight::NoWeight();
    }
    return LogWeight(weight.Value2().Value());
  }
};

template <>
struct WeightConvert<SignedLog64Weight, LogWeight> {
  LogWeight operator()(const SignedLog64Weight &weight) const {
    if (!SignedLogConvertCheck<SignedLog64Weight, LogWeight>(weight)) {
      return LogWeight::NoWeight();
    }
    return LogWeight(weight.Value2().Value());
  }
};

// Converts to log64.
template <>
struct WeightConvert<SignedLogWeight, Log64Weight> {
  Log64Weight operator()(const SignedLogWeight &weight) const {
    if (!SignedLogConvertCheck<SignedLogWeight, Log64Weight>(weight)) {
      return Log64Weight::NoWeight();
    }
    return Log64Weight(weight.Value2().Value());
  }
};

template <>
struct WeightConvert<SignedLog64Weight, Log64Weight> {
  Log64Weight operator()(const SignedLog64Weight &weight) const {
    if (!SignedLogConvertCheck<SignedLog64Weight, Log64Weight>(weight)) {
      return Log64Weight::NoWeight();
    }
    return Log64Weight(weight.Value2().Value());
  }
};

// Converts to real.
template <>
struct WeightConvert<SignedLogWeight, RealWeight> {
  RealWeight operator()(const SignedLogWeight &weight) const {
    return RealWeight(weight.Value1().Value() * exp(-weight.Value2().Value()));
  }
};

template <>
struct WeightConvert<SignedLog64Weight, RealWeight> {
  RealWeight operator()(const SignedLog64Weight &weight) const {
    return RealWeight(weight.Value1().Value() * exp(-weight.Value2().Value()));
  }
};

// Converts to real64.
template <>
struct WeightConvert<SignedLogWeight, Real64Weight> {
  Real64Weight operator()(const SignedLogWeight &weight) const {
    return Real64Weight(weight.Value1().Value() *
                        exp(-weight.Value2().Value()));
  }
};

template <>
struct WeightConvert<SignedLog64Weight, Real64Weight> {
  Real64Weight operator()(const SignedLog64Weight &weight) const {
    return Real64Weight(weight.Value1().Value() *
                        exp(-weight.Value2().Value()));
  }
};

// Converts to signed log.
template <>
struct WeightConvert<TropicalWeight, SignedLogWeight> {
  SignedLogWeight operator()(const TropicalWeight &weight) const {
    return SignedLogWeight(1.0, weight.Value());
  }
};

template <>
struct WeightConvert<LogWeight, SignedLogWeight> {
  SignedLogWeight operator()(const LogWeight &weight) const {
    return SignedLogWeight(1.0, weight.Value());
  }
};

template <>
struct WeightConvert<Log64Weight, SignedLogWeight> {
  SignedLogWeight operator()(const Log64Weight &weight) const {
    return SignedLogWeight(1.0, weight.Value());
  }
};

template <>
struct WeightConvert<RealWeight, SignedLogWeight> {
  SignedLogWeight operator()(const RealWeight &weight) const {
    return SignedLogWeight(weight.Value() >= 0 ? 1.0 : -1.0,
                           -log(std::abs(weight.Value())));
  }
};

template <>
struct WeightConvert<Real64Weight, SignedLogWeight> {
  SignedLogWeight operator()(const Real64Weight &weight) const {
    return SignedLogWeight(weight.Value() >= 0 ? 1.0 : -1.0,
                           -log(std::abs(weight.Value())));
  }
};

template <>
struct WeightConvert<SignedLog64Weight, SignedLogWeight> {
  SignedLogWeight operator()(const SignedLog64Weight &weight) const {
    return SignedLogWeight(weight.Value1(), weight.Value2().Value());
  }
};

// Converts to signed log64.
template <>
struct WeightConvert<TropicalWeight, SignedLog64Weight> {
  SignedLog64Weight operator()(const TropicalWeight &weight) const {
    return SignedLog64Weight(1.0, weight.Value());
  }
};

template <>
struct WeightConvert<LogWeight, SignedLog64Weight> {
  SignedLog64Weight operator()(const LogWeight &weight) const {
    return SignedLog64Weight(1.0, weight.Value());
  }
};

template <>
struct WeightConvert<Log64Weight, SignedLog64Weight> {
  SignedLog64Weight operator()(const Log64Weight &weight) const {
    return SignedLog64Weight(1.0, weight.Value());
  }
};

template <>
struct WeightConvert<RealWeight, SignedLog64Weight> {
  SignedLog64Weight operator()(const RealWeight &weight) const {
    return SignedLog64Weight(weight.Value() >= 0 ? 1.0 : -1.0,
                             -log(std::abs(weight.Value())));
  }
};

template <>
struct WeightConvert<Real64Weight, SignedLog64Weight> {
  SignedLog64Weight operator()(const Real64Weight &weight) const {
    return SignedLog64Weight(weight.Value() >= 0 ? 1.0 : -1.0,
                             -log(std::abs(weight.Value())));
  }
};

template <>
struct WeightConvert<SignedLogWeight, SignedLog64Weight> {
  SignedLog64Weight operator()(const SignedLogWeight &weight) const {
    return SignedLog64Weight(weight.Value1(), weight.Value2().Value());
  }
};

// This function object returns SignedLogWeightTpl<T>'s that are random integers
// chosen from [0, num_random_weights) times a random sign. This is intended
// primarily for testing.
template <class T>
class WeightGenerate<SignedLogWeightTpl<T>> {
 public:
  using Weight = SignedLogWeightTpl<T>;
  using W1 = typename Weight::W1;
  using W2 = typename Weight::W2;

  explicit WeightGenerate(uint64_t seed = std::random_device()(),
                          bool allow_zero = true,
                          size_t num_random_weights = kNumRandomWeights)
      : rand_(seed),
        allow_zero_(allow_zero),
        num_random_weights_(num_random_weights) {}

  Weight operator()() const {
    static constexpr W1 negative(-1.0);
    static constexpr W1 positive(+1.0);
    const bool sign = std::bernoulli_distribution(.5)(rand_);
    const int sample = std::uniform_int_distribution<>(
        0, num_random_weights_ + allow_zero_ - 1)(rand_);
    if (allow_zero_ && sample == num_random_weights_) {
      return Weight(sign ? positive : negative, W2::Zero());
    }
    return Weight(sign ? positive : negative, W2(sample));
  }

 private:
  mutable std::mt19937_64 rand_;
  const bool allow_zero_;
  const size_t num_random_weights_;
};

}  // namespace fst

#endif  // FST_SIGNED_LOG_WEIGHT_H_
