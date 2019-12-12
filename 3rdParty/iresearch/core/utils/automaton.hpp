////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_AUTOMATON_H
#define IRESEARCH_AUTOMATON_H

#include "shared.hpp"

#include <functional>
#include <iostream>
#include <ios>


#if defined(_MSC_VER)
  // NOOP
#elif defined (__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wsign-compare"
#endif

#ifndef FST_NO_DYNAMIC_LINKING
#define FST_NO_DYNAMIC_LINKING
#endif

#include <fst/vector-fst.h>
#include <fst/arcsort.h>
#include <fst/matcher.h>
#include <fst/determinize.h>

#if defined(_MSC_VER)
  // NOOP
#elif defined (__GNUC__)
  #pragma GCC diagnostic pop
#endif

#include "automaton_decl.hpp"
#include "string.hpp"

NS_BEGIN(fst)
NS_BEGIN(fsa)

class BooleanWeight {
 public:
  using ReverseWeight = BooleanWeight;

  static const std::string& Type() {
    static const std::string type = "boolean";
    return type;
  }

  static constexpr BooleanWeight Zero() noexcept { return false; }
  static constexpr BooleanWeight One() noexcept { return true; }
  static constexpr BooleanWeight NoWeight() noexcept { return {}; }

  constexpr BooleanWeight() noexcept : v_(Value::NO_WEIGHT) { }
  constexpr BooleanWeight(bool v) noexcept : v_(Value(char(v))) { }

  static constexpr uint64 Properties() noexcept {
    return kLeftSemiring | kRightSemiring |
           kCommutative | kIdempotent | kPath;
  }

  constexpr bool Member() const noexcept { return v_ != Value::NO_WEIGHT; }
  constexpr BooleanWeight Quantize(float delta = kDelta) const noexcept { return {}; }
  std::istream& Read(std::istream& strm) noexcept {
    v_ = Value(strm.get());
    return strm;
  }
  std::ostream& Write(std::ostream &strm) const noexcept {
    strm << char(v_);
    return strm;
  }
  constexpr size_t Hash() const noexcept { return size_t(v_); }
  constexpr ReverseWeight Reverse() const noexcept { return *this; }
  constexpr operator bool() const noexcept { return v_ == Value::TRUE; }

  friend constexpr bool operator==(const BooleanWeight& lhs, const BooleanWeight& rhs) noexcept {
    return lhs.v_ == rhs.v_;
  }
  friend constexpr bool operator!=(const BooleanWeight& lhs, const BooleanWeight& rhs) noexcept {
    return !(lhs == rhs);
  }
  friend BooleanWeight Plus(const BooleanWeight& lhs, const BooleanWeight& rhs) noexcept {
    return { bool(lhs.v_) || bool(rhs.v_) };
  }
  friend BooleanWeight Times(const BooleanWeight& lhs, const BooleanWeight& rhs) noexcept {
    return { bool(lhs.v_) && bool(rhs.v_) };
  }
  friend BooleanWeight Divide(BooleanWeight, BooleanWeight, DivideType) noexcept {
    return { };
  }
  friend std::ostream& operator<<(std::ostream& strm, const BooleanWeight& w) {
    if (Value::NO_WEIGHT != w.v_) {
      strm << char(w.v_ + 48);
    }
    return strm;
  }
  friend bool ApproxEqual(const BooleanWeight& lhs, const BooleanWeight& rhs,
                          float delta = kDelta) {
    return lhs == rhs;
  }

 private:
  enum Value : char {
    NO_WEIGHT = -1,
    FALSE = 0,
    TRUE = 1
  };

  Value v_;
};

struct MinMaxLabel {
  MinMaxLabel(int32_t label = kNoLabel) noexcept
    : min(label), max(label) {
  }
  MinMaxLabel(int32_t min, int32_t max) noexcept
    : min(min), max(max) {
  }

  bool operator==(const MinMaxLabel& rhs) const noexcept {
    return min == rhs.min && max == rhs.max;
  }
  bool operator!=(const MinMaxLabel& rhs) const noexcept {
    return !(*this == rhs);
  }
  bool operator<(const MinMaxLabel& rhs) const noexcept {
    return min < rhs.min || max < rhs.max;
  }
  bool operator>(const MinMaxLabel& rhs) const noexcept {
    return min > rhs.min || max > rhs.max;
  }
  bool operator>=(const MinMaxLabel& rhs) const noexcept {
    return !(*this < rhs);
  }
  void Write(std::ostream& strm) const {
    strm << min << max;
  }
  friend std::ostream& operator<<(std::ostream& strm, const MinMaxLabel& l) {
    strm << '[' << l.min << ".." << l.max << ']';
    return strm;
  }

  int32_t min{ kNoLabel };
  int32_t max{ kNoLabel };
};

struct EmptyLabel {
  EmptyLabel() noexcept { }
  EmptyLabel(const MinMaxLabel&) noexcept { }
  EmptyLabel(int32_t) noexcept { }
  bool operator==(EmptyLabel) const noexcept { return true; }
  bool operator!=(EmptyLabel) const noexcept { return false; }
  bool operator==(const MinMaxLabel&) const noexcept { return true; }
  bool operator!=(const MinMaxLabel&) const noexcept { return true; }
  bool operator==(int32_t) const noexcept { return true; }
  bool operator!=(int32_t) const noexcept { return false; }
  bool operator<(EmptyLabel) const noexcept { return false; }
  bool operator>(EmptyLabel) const noexcept { return false; }
  operator int() const noexcept { return kNoLabel; }
  operator MinMaxLabel() const noexcept { return kNoLabel; }
  void Write(std::ostream&) const { }

  friend bool operator==(const MinMaxLabel&, EmptyLabel) noexcept { return true; }
  friend bool operator!=(const MinMaxLabel&, EmptyLabel) noexcept { return false; }
  friend bool operator==(int32_t, EmptyLabel) noexcept { return true; }
  friend bool operator!=(int32_t, EmptyLabel) noexcept { return false; }
  friend std::ostream& operator<<(std::ostream& strm, EmptyLabel) {
    return strm;
  }
};

struct Transition {
  using Weight = BooleanWeight;
  using Label = int32_t;
  using StateId = int32_t;

  static const std::string &Type() {
    static const std::string type("Transition");
    return type;
  }

  Label ilabel;
  Label olabel;
  //EmptyLabel olabel;
  StateId nextstate;
  Weight weight{Weight::Zero()}; // all arcs are trivial

  Transition() noexcept(std::is_nothrow_default_constructible<Weight>::value) {}

  Transition(Label ilabel, StateId nextstate)
    : ilabel(ilabel),
      olabel(ilabel),
      nextstate(nextstate) {
  }

  // satisfy openfst API
  Transition(Label ilabel, Label, BooleanWeight, StateId nextstate)
    : ilabel(ilabel),
      olabel(ilabel),
      nextstate(nextstate) {
  }

  // satisfy openfst API
  Transition(Label ilabel, Label, StateId nextstate)
    : ilabel(ilabel),
      olabel(ilabel),
      nextstate(nextstate) {
  }
};

//struct MinMaxTransition {
//  using Weight = BooleanWeight;
//  using Label = int64_t;
//  using StateId = int32_t;
//
//  static const std::string &Type() {
//    static const auto type = std::string("Transition");
//    return type;
//  }
//  static int64_t label(int32_t min, int32_t max) noexcept {
//    return int64_t(min) | (int64_t(max) << 32);
//  }
//
//  Label ilabel;
//  Label olabel;
//  StateId nextstate;
//  Weight weight;
//
//  MinMaxTransition() noexcept(std::is_nothrow_default_constructible<Weight>::value) {}
//
//  MinMaxTransition(int32_t min, int32_t max, StateId nextstate)
//    : ilabel(MinMaxTransition::label(min, max)),
//      olabel(kNoLabel),
//      nextstate(nextstate) {
//  }
//
//  MinMaxTransition(Label label, StateId nextstate)
//    : ilabel(label),
//      olabel(kNoLabel),
//      nextstate(nextstate) {
//  }
//
//  // satisfy openfst API
//  MinMaxTransition(Label ilabel, Label /*olabel*/, BooleanWeight, StateId nextstate)
//    : ilabel(ilabel),
//      olabel(kNoLabel),
//      nextstate(nextstate) {
//  }
//
//  int32_t min() const noexcept { return int32_t(ilabel & INT64_C(0xFFFFFFFF)); }
//  int32_t max() const noexcept { return int32_t((ilabel >> 32) & INT64_C(0xFFFFFFFF)); }
//};

struct MinMaxTransition {
  using Weight = BooleanWeight;
  using Label = MinMaxLabel;
  using StateId = int32_t;

  static const std::string &Type() {
    static const std::string type("MinMaxTransition");
    return type;
  }

  MinMaxLabel ilabel;
  StateId nextstate;
  Weight weight;
  EmptyLabel olabel;

  MinMaxTransition() noexcept(std::is_nothrow_default_constructible<Weight>::value &&
                        std::is_nothrow_default_constructible<MinMaxLabel>::value &&
                        std::is_nothrow_default_constructible<EmptyLabel>::value) {
  }

  MinMaxTransition(int32_t label, StateId nextstate)
    : ilabel(label, label),
      nextstate(nextstate) {
  }

  MinMaxTransition(int32_t min, int32_t max, StateId nextstate)
    : ilabel(min, max),
      nextstate(nextstate) {
  }

  // satisfy openfst API
  MinMaxTransition(Label ilabel, Label, BooleanWeight, StateId nextstate)
    : ilabel(ilabel),
      nextstate(nextstate) {
  }
};

using Automaton = VectorFst<Transition>;
using AutomatonMatcher = SortedMatcher<Automaton>;
using MinMaxAutomaton = VectorFst<MinMaxTransition>;

constexpr const int32_t kEps   = 0;        // match all + don't consume symbol
constexpr const int32_t kRho   = irs::integer_traits<int32_t>::const_max; // match rest + consume symbol
constexpr const int32_t kPhi   = kRho - 1; // match rest + don't consume symbol
constexpr const int32_t kSigma = kPhi - 1; // match all + consume symbol

constexpr const int32_t kMinLabel = 0;
constexpr const int32_t kMaxLabel = kSigma - 1;

NS_END // fsa
NS_END // fst

NS_BEGIN(std)

inline void swap(::fst::fsa::MinMaxLabel& lhs, ::fst::fsa::EmptyLabel& /*rhs*/) noexcept {
  lhs = ::fst::kNoLabel;
}

inline void swap(int32_t& lhs, ::fst::fsa::EmptyLabel& /*rhs*/) noexcept {
  lhs = ::fst::kNoLabel;
}

template<>
struct hash<::fst::fsa::MinMaxLabel> {
  size_t operator()(const ::fst::fsa::MinMaxLabel& label) const noexcept {
    return hash<uint64_t>()(uint64_t(label.min) | uint64_t(label.max) << 32);
  }
};

template<>
struct hash<::fst::fsa::EmptyLabel> {
  size_t operator()(::fst::fsa::EmptyLabel) const noexcept {
    return 0;
  }
};

NS_END

#endif // IRESEARCH_AUTOMATON_H
