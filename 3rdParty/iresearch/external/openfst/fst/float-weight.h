// float-weight.h

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
// Author: riley@google.com (Michael Riley)
//
// \file
// Float weight set and associated semiring operation definitions.
//

#ifndef FST_LIB_FLOAT_WEIGHT_H__
#define FST_LIB_FLOAT_WEIGHT_H__

#include <limits>
#include <climits>
#include <sstream>
#include <string>

#include <fst/util.h>
#include <fst/weight.h>


namespace fst {

// numeric limits class
template <class T>
class FloatLimits {
 public:
  static const T PosInfinity() {
    static const T pos_infinity = numeric_limits<T>::infinity();
    return pos_infinity;
  }

  static const T NegInfinity() {
    static const T neg_infinity = -PosInfinity();
    return neg_infinity;
  }

  static const T NumberBad() {
    static const T number_bad = numeric_limits<T>::quiet_NaN();
    return number_bad;
  }

};

// weight class to be templated on floating-points types
template <class T = float>
class FloatWeightTpl {
 public:
  FloatWeightTpl() {}

  FloatWeightTpl(T f) : value_(f) {}

  FloatWeightTpl(const FloatWeightTpl<T> &w) : value_(w.value_) {}

  FloatWeightTpl<T> &operator=(const FloatWeightTpl<T> &w) {
    value_ = w.value_;
    return *this;
  }

  istream &Read(istream &strm) {
    return ReadType(strm, &value_);
  }

  ostream &Write(ostream &strm) const {
    return WriteType(strm, value_);
  }

  size_t Hash() const {
    union {
      T f;
      size_t s;
    } u;
    u.s = 0;
    u.f = value_;
    return u.s;
  }

  const T &Value() const { return value_; }

 protected:
  void SetValue(const T &f) { value_ = f; }

  inline static string GetPrecisionString() {
    int64 size = sizeof(T);
    if (size == sizeof(float)) return "";
    size *= CHAR_BIT;

    string result;
    Int64ToStr(size, &result);
    return result;
  }

 private:
  T value_;
};

// Single-precision float weight
typedef FloatWeightTpl<float> FloatWeight;

template <class T>
inline bool operator==(const FloatWeightTpl<T> &w1,
                       const FloatWeightTpl<T> &w2) {
  // Volatile qualifier thwarts over-aggressive compiler optimizations
  // that lead to problems esp. with NaturalLess().
  volatile T v1 = w1.Value();
  volatile T v2 = w2.Value();
  return v1 == v2;
}

inline bool operator==(const FloatWeightTpl<double> &w1,
                       const FloatWeightTpl<double> &w2) {
  return operator==<double>(w1, w2);
}

inline bool operator==(const FloatWeightTpl<float> &w1,
                       const FloatWeightTpl<float> &w2) {
  return operator==<float>(w1, w2);
}

template <class T>
inline bool operator!=(const FloatWeightTpl<T> &w1,
                       const FloatWeightTpl<T> &w2) {
  return !(w1 == w2);
}

inline bool operator!=(const FloatWeightTpl<double> &w1,
                       const FloatWeightTpl<double> &w2) {
  return operator!=<double>(w1, w2);
}

inline bool operator!=(const FloatWeightTpl<float> &w1,
                       const FloatWeightTpl<float> &w2) {
  return operator!=<float>(w1, w2);
}

template <class T>
inline bool ApproxEqual(const FloatWeightTpl<T> &w1,
                        const FloatWeightTpl<T> &w2,
                        float delta = kDelta) {
  return w1.Value() <= w2.Value() + delta && w2.Value() <= w1.Value() + delta;
}

template <class T>
inline ostream &operator<<(ostream &strm, const FloatWeightTpl<T> &w) {
  if (w.Value() == FloatLimits<T>::PosInfinity())
    return strm << "Infinity";
  else if (w.Value() == FloatLimits<T>::NegInfinity())
    return strm << "-Infinity";
  else if (w.Value() != w.Value())   // Fails for NaN
    return strm << "BadNumber";
  else
    return strm << w.Value();
}

template <class T>
inline istream &operator>>(istream &strm, FloatWeightTpl<T> &w) {
  string s;
  strm >> s;
  if (s == "Infinity") {
    w = FloatWeightTpl<T>(FloatLimits<T>::PosInfinity());
  } else if (s == "-Infinity") {
    w = FloatWeightTpl<T>(FloatLimits<T>::NegInfinity());
  } else {
    char *p;
    T f = strtod(s.c_str(), &p);
    if (p < s.c_str() + s.size())
      strm.clear(std::ios::badbit);
    else
      w = FloatWeightTpl<T>(f);
  }
  return strm;
}


// Tropical semiring: (min, +, inf, 0)
template <class T>
class TropicalWeightTpl : public FloatWeightTpl<T> {
 public:
  using FloatWeightTpl<T>::Value;

  typedef TropicalWeightTpl<T> ReverseWeight;

  TropicalWeightTpl() : FloatWeightTpl<T>() {}

  TropicalWeightTpl(T f) : FloatWeightTpl<T>(f) {}

  TropicalWeightTpl(const TropicalWeightTpl<T> &w) : FloatWeightTpl<T>(w) {}

  static const TropicalWeightTpl<T> Zero() {
    return TropicalWeightTpl<T>(FloatLimits<T>::PosInfinity()); }

  static const TropicalWeightTpl<T> One() {
    return TropicalWeightTpl<T>(0.0F); }

  static const TropicalWeightTpl<T> NoWeight() {
    return TropicalWeightTpl<T>(FloatLimits<T>::NumberBad()); }

  static const string &Type() {
    static const string type = "tropical" +
        FloatWeightTpl<T>::GetPrecisionString();
    return type;
  }

  bool Member() const {
    // First part fails for IEEE NaN
    return Value() == Value() && Value() != FloatLimits<T>::NegInfinity();
  }

  TropicalWeightTpl<T> Quantize(float delta = kDelta) const {
    if (Value() == FloatLimits<T>::NegInfinity() ||
        Value() == FloatLimits<T>::PosInfinity() ||
        Value() != Value())
      return *this;
    else
      return TropicalWeightTpl<T>(floor(Value()/delta + 0.5F) * delta);
  }

  TropicalWeightTpl<T> Reverse() const { return *this; }

  static uint64 Properties() {
    return kLeftSemiring | kRightSemiring | kCommutative |
        kPath | kIdempotent;
  }
};

// Single precision tropical weight
typedef TropicalWeightTpl<float> TropicalWeight;

template <class T>
inline TropicalWeightTpl<T> Plus(const TropicalWeightTpl<T> &w1,
                                 const TropicalWeightTpl<T> &w2) {
  if (!w1.Member() || !w2.Member())
    return TropicalWeightTpl<T>::NoWeight();
  return w1.Value() < w2.Value() ? w1 : w2;
}

inline TropicalWeightTpl<float> Plus(const TropicalWeightTpl<float> &w1,
                                     const TropicalWeightTpl<float> &w2) {
  return Plus<float>(w1, w2);
}

inline TropicalWeightTpl<double> Plus(const TropicalWeightTpl<double> &w1,
                                      const TropicalWeightTpl<double> &w2) {
  return Plus<double>(w1, w2);
}

template <class T>
inline TropicalWeightTpl<T> Times(const TropicalWeightTpl<T> &w1,
                                  const TropicalWeightTpl<T> &w2) {
  if (!w1.Member() || !w2.Member())
    return TropicalWeightTpl<T>::NoWeight();
  T f1 = w1.Value(), f2 = w2.Value();
  if (f1 == FloatLimits<T>::PosInfinity())
    return w1;
  else if (f2 == FloatLimits<T>::PosInfinity())
    return w2;
  else
    return TropicalWeightTpl<T>(f1 + f2);
}

inline TropicalWeightTpl<float> Times(const TropicalWeightTpl<float> &w1,
                                      const TropicalWeightTpl<float> &w2) {
  return Times<float>(w1, w2);
}

inline TropicalWeightTpl<double> Times(const TropicalWeightTpl<double> &w1,
                                       const TropicalWeightTpl<double> &w2) {
  return Times<double>(w1, w2);
}

template <class T>
inline TropicalWeightTpl<T> Divide(const TropicalWeightTpl<T> &w1,
                                   const TropicalWeightTpl<T> &w2,
                                   DivideType typ = DIVIDE_ANY) {
  if (!w1.Member() || !w2.Member())
    return TropicalWeightTpl<T>::NoWeight();
  T f1 = w1.Value(), f2 = w2.Value();
  if (f2 == FloatLimits<T>::PosInfinity())
    return FloatLimits<T>::NumberBad();
  else if (f1 == FloatLimits<T>::PosInfinity())
    return FloatLimits<T>::PosInfinity();
  else
    return TropicalWeightTpl<T>(f1 - f2);
}

inline TropicalWeightTpl<float> Divide(const TropicalWeightTpl<float> &w1,
                                       const TropicalWeightTpl<float> &w2,
                                       DivideType typ = DIVIDE_ANY) {
  return Divide<float>(w1, w2, typ);
}

inline TropicalWeightTpl<double> Divide(const TropicalWeightTpl<double> &w1,
                                        const TropicalWeightTpl<double> &w2,
                                        DivideType typ = DIVIDE_ANY) {
  return Divide<double>(w1, w2, typ);
}


// Log semiring: (log(e^-x + e^y), +, inf, 0)
template <class T>
class LogWeightTpl : public FloatWeightTpl<T> {
 public:
  using FloatWeightTpl<T>::Value;

  typedef LogWeightTpl ReverseWeight;

  LogWeightTpl() : FloatWeightTpl<T>() {}

  LogWeightTpl(T f) : FloatWeightTpl<T>(f) {}

  LogWeightTpl(const LogWeightTpl<T> &w) : FloatWeightTpl<T>(w) {}

  static const LogWeightTpl<T> Zero() {
    return LogWeightTpl<T>(FloatLimits<T>::PosInfinity());
  }

  static const LogWeightTpl<T> One() {
    return LogWeightTpl<T>(0.0F);
  }

  static const LogWeightTpl<T> NoWeight() {
    return LogWeightTpl<T>(FloatLimits<T>::NumberBad()); }

  static const string &Type() {
    static const string type = "log" + FloatWeightTpl<T>::GetPrecisionString();
    return type;
  }

  bool Member() const {
    // First part fails for IEEE NaN
    return Value() == Value() && Value() != FloatLimits<T>::NegInfinity();
  }

  LogWeightTpl<T> Quantize(float delta = kDelta) const {
    if (Value() == FloatLimits<T>::NegInfinity() ||
        Value() == FloatLimits<T>::PosInfinity() ||
        Value() != Value())
      return *this;
    else
      return LogWeightTpl<T>(floor(Value()/delta + 0.5F) * delta);
  }

  LogWeightTpl<T> Reverse() const { return *this; }

  static uint64 Properties() {
    return kLeftSemiring | kRightSemiring | kCommutative;
  }
};

// Single-precision log weight
typedef LogWeightTpl<float> LogWeight;
// Double-precision log weight
typedef LogWeightTpl<double> Log64Weight;

template <class T>
inline T LogExp(T x) { return log(1.0F + exp(-x)); }

template <class T>
inline LogWeightTpl<T> Plus(const LogWeightTpl<T> &w1,
                            const LogWeightTpl<T> &w2) {
  T f1 = w1.Value(), f2 = w2.Value();
  if (f1 == FloatLimits<T>::PosInfinity())
    return w2;
  else if (f2 == FloatLimits<T>::PosInfinity())
    return w1;
  else if (f1 > f2)
    return LogWeightTpl<T>(f2 - LogExp(f1 - f2));
  else
    return LogWeightTpl<T>(f1 - LogExp(f2 - f1));
}

inline LogWeightTpl<float> Plus(const LogWeightTpl<float> &w1,
                                const LogWeightTpl<float> &w2) {
  return Plus<float>(w1, w2);
}

inline LogWeightTpl<double> Plus(const LogWeightTpl<double> &w1,
                                 const LogWeightTpl<double> &w2) {
  return Plus<double>(w1, w2);
}

template <class T>
inline LogWeightTpl<T> Times(const LogWeightTpl<T> &w1,
                             const LogWeightTpl<T> &w2) {
  if (!w1.Member() || !w2.Member())
    return LogWeightTpl<T>::NoWeight();
  T f1 = w1.Value(), f2 = w2.Value();
  if (f1 == FloatLimits<T>::PosInfinity())
    return w1;
  else if (f2 == FloatLimits<T>::PosInfinity())
    return w2;
  else
    return LogWeightTpl<T>(f1 + f2);
}

inline LogWeightTpl<float> Times(const LogWeightTpl<float> &w1,
                                 const LogWeightTpl<float> &w2) {
  return Times<float>(w1, w2);
}

inline LogWeightTpl<double> Times(const LogWeightTpl<double> &w1,
                                  const LogWeightTpl<double> &w2) {
  return Times<double>(w1, w2);
}

template <class T>
inline LogWeightTpl<T> Divide(const LogWeightTpl<T> &w1,
                              const LogWeightTpl<T> &w2,
                              DivideType typ = DIVIDE_ANY) {
  if (!w1.Member() || !w2.Member())
    return LogWeightTpl<T>::NoWeight();
  T f1 = w1.Value(), f2 = w2.Value();
  if (f2 == FloatLimits<T>::PosInfinity())
    return FloatLimits<T>::NumberBad();
  else if (f1 == FloatLimits<T>::PosInfinity())
    return FloatLimits<T>::PosInfinity();
  else
    return LogWeightTpl<T>(f1 - f2);
}

inline LogWeightTpl<float> Divide(const LogWeightTpl<float> &w1,
                                  const LogWeightTpl<float> &w2,
                                  DivideType typ = DIVIDE_ANY) {
  return Divide<float>(w1, w2, typ);
}

inline LogWeightTpl<double> Divide(const LogWeightTpl<double> &w1,
                                   const LogWeightTpl<double> &w2,
                                   DivideType typ = DIVIDE_ANY) {
  return Divide<double>(w1, w2, typ);
}

// MinMax semiring: (min, max, inf, -inf)
template <class T>
class MinMaxWeightTpl : public FloatWeightTpl<T> {
 public:
  using FloatWeightTpl<T>::Value;

  typedef MinMaxWeightTpl<T> ReverseWeight;

  MinMaxWeightTpl() : FloatWeightTpl<T>() {}

  MinMaxWeightTpl(T f) : FloatWeightTpl<T>(f) {}

  MinMaxWeightTpl(const MinMaxWeightTpl<T> &w) : FloatWeightTpl<T>(w) {}

  static const MinMaxWeightTpl<T> Zero() {
    return MinMaxWeightTpl<T>(FloatLimits<T>::PosInfinity());
  }

  static const MinMaxWeightTpl<T> One() {
    return MinMaxWeightTpl<T>(FloatLimits<T>::NegInfinity());
  }

  static const MinMaxWeightTpl<T> NoWeight() {
    return MinMaxWeightTpl<T>(FloatLimits<T>::NumberBad()); }

  static const string &Type() {
    static const string type = "minmax" +
        FloatWeightTpl<T>::GetPrecisionString();
    return type;
  }

  bool Member() const {
    // Fails for IEEE NaN
    return Value() == Value();
  }

  MinMaxWeightTpl<T> Quantize(float delta = kDelta) const {
    // If one of infinities, or a NaN
    if (Value() == FloatLimits<T>::NegInfinity() ||
        Value() == FloatLimits<T>::PosInfinity() ||
        Value() != Value())
      return *this;
    else
      return MinMaxWeightTpl<T>(floor(Value()/delta + 0.5F) * delta);
  }

  MinMaxWeightTpl<T> Reverse() const { return *this; }

  static uint64 Properties() {
    return kLeftSemiring | kRightSemiring | kCommutative | kIdempotent | kPath;
  }
};

// Single-precision min-max weight
typedef MinMaxWeightTpl<float> MinMaxWeight;

// Min
template <class T>
inline MinMaxWeightTpl<T> Plus(
    const MinMaxWeightTpl<T> &w1, const MinMaxWeightTpl<T> &w2) {
  if (!w1.Member() || !w2.Member())
    return MinMaxWeightTpl<T>::NoWeight();
  return w1.Value() < w2.Value() ? w1 : w2;
}

inline MinMaxWeightTpl<float> Plus(
    const MinMaxWeightTpl<float> &w1, const MinMaxWeightTpl<float> &w2) {
  return Plus<float>(w1, w2);
}

inline MinMaxWeightTpl<double> Plus(
    const MinMaxWeightTpl<double> &w1, const MinMaxWeightTpl<double> &w2) {
  return Plus<double>(w1, w2);
}

// Max
template <class T>
inline MinMaxWeightTpl<T> Times(
    const MinMaxWeightTpl<T> &w1, const MinMaxWeightTpl<T> &w2) {
  if (!w1.Member() || !w2.Member())
    return MinMaxWeightTpl<T>::NoWeight();
  return w1.Value() >= w2.Value() ? w1 : w2;
}

inline MinMaxWeightTpl<float> Times(
    const MinMaxWeightTpl<float> &w1, const MinMaxWeightTpl<float> &w2) {
  return Times<float>(w1, w2);
}

inline MinMaxWeightTpl<double> Times(
    const MinMaxWeightTpl<double> &w1, const MinMaxWeightTpl<double> &w2) {
  return Times<double>(w1, w2);
}

// Defined only for special cases
template <class T>
inline MinMaxWeightTpl<T> Divide(const MinMaxWeightTpl<T> &w1,
                                 const MinMaxWeightTpl<T> &w2,
                                 DivideType typ = DIVIDE_ANY) {
  if (!w1.Member() || !w2.Member())
    return MinMaxWeightTpl<T>::NoWeight();
  // min(w1, x) = w2, w1 >= w2 => min(w1, x) = w2, x = w2
  return w1.Value() >= w2.Value() ? w1 : FloatLimits<T>::NumberBad();
}

inline MinMaxWeightTpl<float> Divide(const MinMaxWeightTpl<float> &w1,
                                     const MinMaxWeightTpl<float> &w2,
                                     DivideType typ = DIVIDE_ANY) {
  return Divide<float>(w1, w2, typ);
}

inline MinMaxWeightTpl<double> Divide(const MinMaxWeightTpl<double> &w1,
                                      const MinMaxWeightTpl<double> &w2,
                                      DivideType typ = DIVIDE_ANY) {
  return Divide<double>(w1, w2, typ);
}

//
// WEIGHT CONVERTER SPECIALIZATIONS.
//

// Convert to tropical
template <>
struct WeightConvert<LogWeight, TropicalWeight> {
  TropicalWeight operator()(LogWeight w) const { return w.Value(); }
};

template <>
struct WeightConvert<Log64Weight, TropicalWeight> {
  TropicalWeight operator()(Log64Weight w) const { return w.Value(); }
};

// Convert to log
template <>
struct WeightConvert<TropicalWeight, LogWeight> {
  LogWeight operator()(TropicalWeight w) const { return w.Value(); }
};

template <>
struct WeightConvert<Log64Weight, LogWeight> {
  LogWeight operator()(Log64Weight w) const { return w.Value(); }
};

// Convert to log64
template <>
struct WeightConvert<TropicalWeight, Log64Weight> {
  Log64Weight operator()(TropicalWeight w) const { return w.Value(); }
};

template <>
struct WeightConvert<LogWeight, Log64Weight> {
  Log64Weight operator()(LogWeight w) const { return w.Value(); }
};

}  // namespace fst

#endif  // FST_LIB_FLOAT_WEIGHT_H__
