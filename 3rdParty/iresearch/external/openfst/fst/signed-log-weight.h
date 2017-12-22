
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
// \file
// LogWeight along with sign information that represents the value X in the
// linear domain as <sign(X), -ln(|X|)>
// The sign is a TropicalWeight:
//  positive, TropicalWeight.Value() > 0.0, recommended value 1.0
//  negative, TropicalWeight.Value() <= 0.0, recommended value -1.0

#ifndef FST_LIB_SIGNED_LOG_WEIGHT_H_
#define FST_LIB_SIGNED_LOG_WEIGHT_H_

#include <fst/float-weight.h>
#include <fst/pair-weight.h>
#include <fst/product-weight.h>


namespace fst {
template <class T>
class SignedLogWeightTpl
    : public PairWeight<TropicalWeight, LogWeightTpl<T> > {
 public:
  typedef TropicalWeight X1;
  typedef LogWeightTpl<T> X2;
  using PairWeight<X1, X2>::Value1;
  using PairWeight<X1, X2>::Value2;

  using PairWeight<X1, X2>::Reverse;
  using PairWeight<X1, X2>::Quantize;
  using PairWeight<X1, X2>::Member;

  typedef SignedLogWeightTpl<T> ReverseWeight;

  SignedLogWeightTpl() : PairWeight<X1, X2>() {}

  SignedLogWeightTpl(const SignedLogWeightTpl<T>& w)
      : PairWeight<X1, X2> (w) { }

  SignedLogWeightTpl(const PairWeight<X1, X2>& w)
      : PairWeight<X1, X2> (w) { }

  SignedLogWeightTpl(const X1& x1, const X2& x2)
      : PairWeight<X1, X2>(x1, x2) { }

  static const SignedLogWeightTpl<T> &Zero() {
    static const SignedLogWeightTpl<T> zero(X1(1.0), X2::Zero());
    return zero;
  }

  static const SignedLogWeightTpl<T> &One() {
    static const SignedLogWeightTpl<T> one(X1(1.0), X2::One());
    return one;
  }

  static const SignedLogWeightTpl<T> &NoWeight() {
    static const SignedLogWeightTpl<T> no_weight(X1(1.0), X2::NoWeight());
    return no_weight;
  }

  static const string &Type() {
    static const string type = "signed_log_" + X1::Type() + "_" + X2::Type();
    return type;
  }

  ProductWeight<X1, X2> Quantize(float delta = kDelta) const {
    return PairWeight<X1, X2>::Quantize();
  }

  ReverseWeight Reverse() const {
    return PairWeight<X1, X2>::Reverse();
  }

  bool Member() const {
    return PairWeight<X1, X2>::Member();
  }

  static uint64 Properties() {
    // not idempotent nor path
    return kLeftSemiring | kRightSemiring | kCommutative;
  }

  size_t Hash() const {
    size_t h1;
    if (Value2() == X2::Zero() || Value1().Value() > 0.0)
      h1 = TropicalWeight(1.0).Hash();
    else
      h1 = TropicalWeight(-1.0).Hash();
    size_t h2 = Value2().Hash();
    const int lshift = 5;
    const int rshift = CHAR_BIT * sizeof(size_t) - 5;
    return h1 << lshift ^ h1 >> rshift ^ h2;
  }
};

template <class T>
inline SignedLogWeightTpl<T> Plus(const SignedLogWeightTpl<T> &w1,
                                  const SignedLogWeightTpl<T> &w2) {
  if (!w1.Member() || !w2.Member())
    return SignedLogWeightTpl<T>::NoWeight();
  bool s1 = w1.Value1().Value() > 0.0;
  bool s2 = w2.Value1().Value() > 0.0;
  T f1 = w1.Value2().Value();
  T f2 = w2.Value2().Value();
  if (f1 == FloatLimits<T>::PosInfinity())
    return w2;
  else if (f2 == FloatLimits<T>::PosInfinity())
    return w1;
  else if (f1 == f2) {
    if (s1 == s2)
      return SignedLogWeightTpl<T>(w1.Value1(), (f2 - log(2.0F)));
    else
      return SignedLogWeightTpl<T>::Zero();
  } else if (f1 > f2) {
    if (s1 == s2) {
      return SignedLogWeightTpl<T>(
        w1.Value1(), (f2 - log(1.0F + exp(f2 - f1))));
    } else {
      return SignedLogWeightTpl<T>(
        w2.Value1(), (f2 - log(1.0F - exp(f2 - f1))));
    }
  } else {
    if (s2 == s1) {
      return SignedLogWeightTpl<T>(
        w2.Value1(), (f1 - log(1.0F + exp(f1 - f2))));
    } else {
      return SignedLogWeightTpl<T>(
        w1.Value1(), (f1 - log(1.0F - exp(f1 - f2))));
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
  if (!w1.Member() || !w2.Member())
    return SignedLogWeightTpl<T>::NoWeight();
  bool s1 = w1.Value1().Value() > 0.0;
  bool s2 = w2.Value1().Value() > 0.0;
  T f1 = w1.Value2().Value();
  T f2 = w2.Value2().Value();
  if (s1 == s2)
    return SignedLogWeightTpl<T>(TropicalWeight(1.0), (f1 + f2));
  else
    return SignedLogWeightTpl<T>(TropicalWeight(-1.0), (f1 + f2));
}

template <class T>
inline SignedLogWeightTpl<T> Divide(const SignedLogWeightTpl<T> &w1,
                                    const SignedLogWeightTpl<T> &w2,
                                    DivideType typ = DIVIDE_ANY) {
  if (!w1.Member() || !w2.Member())
    return SignedLogWeightTpl<T>::NoWeight();
  bool s1 = w1.Value1().Value() > 0.0;
  bool s2 = w2.Value1().Value() > 0.0;
  T f1 = w1.Value2().Value();
  T f2 = w2.Value2().Value();
  if (f2 == FloatLimits<T>::PosInfinity())
    return SignedLogWeightTpl<T>(TropicalWeight(1.0),
      FloatLimits<T>::NumberBad());
  else if (f1 == FloatLimits<T>::PosInfinity())
    return SignedLogWeightTpl<T>(TropicalWeight(1.0),
      FloatLimits<T>::PosInfinity());
  else if (s1 == s2)
    return SignedLogWeightTpl<T>(TropicalWeight(1.0), (f1 - f2));
  else
    return SignedLogWeightTpl<T>(TropicalWeight(-1.0), (f1 - f2));
}

template <class T>
inline bool ApproxEqual(const SignedLogWeightTpl<T> &w1,
                        const SignedLogWeightTpl<T> &w2,
                        float delta = kDelta) {
  bool s1 = w1.Value1().Value() > 0.0;
  bool s2 = w2.Value1().Value() > 0.0;
  if (s1 == s2) {
    return ApproxEqual(w1.Value2(), w2.Value2(), delta);
  } else {
    return w1.Value2() == LogWeightTpl<T>::Zero()
        && w2.Value2() == LogWeightTpl<T>::Zero();
  }
}

template <class T>
inline bool operator==(const SignedLogWeightTpl<T> &w1,
                       const SignedLogWeightTpl<T> &w2) {
  bool s1 = w1.Value1().Value() > 0.0;
  bool s2 = w2.Value1().Value() > 0.0;
  if (s1 == s2)
    return w1.Value2() == w2.Value2();
  else
    return (w1.Value2() == LogWeightTpl<T>::Zero()) &&
           (w2.Value2() == LogWeightTpl<T>::Zero());
}


// Single-precision signed-log weight
typedef SignedLogWeightTpl<float> SignedLogWeight;
// Double-precision signed-log weight
typedef SignedLogWeightTpl<double> SignedLog64Weight;

//
// WEIGHT CONVERTER SPECIALIZATIONS.
//

template <class W1, class W2>
bool SignedLogConvertCheck(W1 w) {
  if (w.Value1().Value() < 0.0) {
    FSTERROR() << "WeightConvert: can't convert weight from \""
               << W1::Type() << "\" to \"" << W2::Type();
    return false;
  }
  return true;
}

// Convert to tropical
template <>
struct WeightConvert<SignedLogWeight, TropicalWeight> {
  TropicalWeight operator()(SignedLogWeight w) const {
    if (!SignedLogConvertCheck<SignedLogWeight, TropicalWeight>(w))
      return TropicalWeight::NoWeight();
    return w.Value2().Value();
  }
};

template <>
struct WeightConvert<SignedLog64Weight, TropicalWeight> {
  TropicalWeight operator()(SignedLog64Weight w) const {
    if (!SignedLogConvertCheck<SignedLog64Weight, TropicalWeight>(w))
      return TropicalWeight::NoWeight();
    return w.Value2().Value();
  }
};

// Convert to log
template <>
struct WeightConvert<SignedLogWeight, LogWeight> {
  LogWeight operator()(SignedLogWeight w) const {
    if (!SignedLogConvertCheck<SignedLogWeight, LogWeight>(w))
      return LogWeight::NoWeight();
    return w.Value2().Value();
  }
};

template <>
struct WeightConvert<SignedLog64Weight, LogWeight> {
  LogWeight operator()(SignedLog64Weight w) const {
    if (!SignedLogConvertCheck<SignedLog64Weight, LogWeight>(w))
      return LogWeight::NoWeight();
    return w.Value2().Value();
  }
};

// Convert to log64
template <>
struct WeightConvert<SignedLogWeight, Log64Weight> {
  Log64Weight operator()(SignedLogWeight w) const {
    if (!SignedLogConvertCheck<SignedLogWeight, Log64Weight>(w))
      return Log64Weight::NoWeight();
    return w.Value2().Value();
  }
};

template <>
struct WeightConvert<SignedLog64Weight, Log64Weight> {
  Log64Weight operator()(SignedLog64Weight w) const {
    if (!SignedLogConvertCheck<SignedLog64Weight, Log64Weight>(w))
      return Log64Weight::NoWeight();
    return w.Value2().Value();
  }
};

// Convert to signed log
template <>
struct WeightConvert<TropicalWeight, SignedLogWeight> {
  SignedLogWeight operator()(TropicalWeight w) const {
    TropicalWeight x1 = 1.0;
    LogWeight x2 = w.Value();
    return SignedLogWeight(x1, x2);
  }
};

template <>
struct WeightConvert<LogWeight, SignedLogWeight> {
  SignedLogWeight operator()(LogWeight w) const {
    TropicalWeight x1 = 1.0;
    LogWeight x2 = w.Value();
    return SignedLogWeight(x1, x2);
  }
};

template <>
struct WeightConvert<Log64Weight, SignedLogWeight> {
  SignedLogWeight operator()(Log64Weight w) const {
    TropicalWeight x1 = 1.0;
    LogWeight x2 = w.Value();
    return SignedLogWeight(x1, x2);
  }
};

template <>
struct WeightConvert<SignedLog64Weight, SignedLogWeight> {
  SignedLogWeight operator()(SignedLog64Weight w) const {
    TropicalWeight x1 = w.Value1();
    LogWeight x2 = w.Value2().Value();
    return SignedLogWeight(x1, x2);
  }
};

// Convert to signed log64
template <>
struct WeightConvert<TropicalWeight, SignedLog64Weight> {
  SignedLog64Weight operator()(TropicalWeight w) const {
    TropicalWeight x1 = 1.0;
    Log64Weight x2 = w.Value();
    return SignedLog64Weight(x1, x2);
  }
};

template <>
struct WeightConvert<LogWeight, SignedLog64Weight> {
  SignedLog64Weight operator()(LogWeight w) const {
    TropicalWeight x1 = 1.0;
    Log64Weight x2 = w.Value();
    return SignedLog64Weight(x1, x2);
  }
};

template <>
struct WeightConvert<Log64Weight, SignedLog64Weight> {
  SignedLog64Weight operator()(Log64Weight w) const {
    TropicalWeight x1 = 1.0;
    Log64Weight x2 = w.Value();
    return SignedLog64Weight(x1, x2);
  }
};

template <>
struct WeightConvert<SignedLogWeight, SignedLog64Weight> {
  SignedLog64Weight operator()(SignedLogWeight w) const {
    TropicalWeight x1 = w.Value1();
    Log64Weight x2 = w.Value2().Value();
    return SignedLog64Weight(x1, x2);
  }
};

}  // namespace fst

#endif  // FST_LIB_SIGNED_LOG_WEIGHT_H_
