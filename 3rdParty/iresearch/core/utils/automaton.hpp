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

#if defined(_MSC_VER)
  // NOOP
#elif defined (__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wsign-compare"
#endif

#ifndef FST_NO_DYNAMIC_LINKING
#define FST_NO_DYNAMIC_LINKING
#endif

#include <fst/fst.h>

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
  using PayloadType = irs::byte_type;

  static const std::string& Type() {
    static const std::string type = "boolean";
    return type;
  }

  static constexpr BooleanWeight Zero() noexcept { return false; }
  static constexpr BooleanWeight One() noexcept { return true; }
  static constexpr BooleanWeight NoWeight() noexcept { return {}; }

  static constexpr uint64 Properties() noexcept {
    return kLeftSemiring | kRightSemiring |
           kCommutative | kIdempotent | kPath;
  }

  constexpr BooleanWeight() noexcept = default;
  constexpr BooleanWeight(bool v, PayloadType payload = 0) noexcept
    : v_(PayloadType(v)), p_(payload) {
  }

  constexpr bool Member() const noexcept { return Invalid != v_; }
  constexpr BooleanWeight Quantize([[maybe_unused]]float delta = kDelta) const noexcept { return {};  }
  std::istream& Read(std::istream& strm) noexcept {
    v_ = strm.get();
    if (strm.fail()) {
      v_ = Invalid;
    }
    return strm;
  }
  std::ostream& Write(std::ostream &strm) const noexcept {
    strm.put(v_);
    return strm;
  }
  constexpr size_t Hash() const noexcept { return size_t(v_); }
  constexpr ReverseWeight Reverse() const noexcept { return *this; }
  constexpr PayloadType Payload() const noexcept { return p_; }
  constexpr operator bool() const noexcept { return v_ == True; }

  friend constexpr bool operator==(const BooleanWeight& lhs, const BooleanWeight& rhs) noexcept {
    return lhs.Hash() == rhs.Hash();
  }
  friend constexpr bool operator!=(const BooleanWeight& lhs, const BooleanWeight& rhs) noexcept {
    return !(lhs == rhs);
  }
  friend constexpr BooleanWeight Plus(const BooleanWeight& lhs, const BooleanWeight& rhs) noexcept {
    return BooleanWeight(bool(lhs.Hash()) || bool(rhs.Hash()), lhs.Payload() | rhs.Payload());
  }
  friend constexpr BooleanWeight Times(const BooleanWeight& lhs, const BooleanWeight& rhs) noexcept {
    return BooleanWeight(bool(lhs.Hash()) && bool(rhs.Hash()), lhs.Payload() & rhs.Payload());
  }
  friend constexpr BooleanWeight Divide(BooleanWeight, BooleanWeight, DivideType) noexcept {
    return NoWeight();
  }
  friend constexpr BooleanWeight Divide(BooleanWeight, BooleanWeight) noexcept {
    return NoWeight();
  }
  friend std::ostream& operator<<(std::ostream& strm, const BooleanWeight& w) {
    if (w.Member()) {
      strm << "{" << char(bool(w) + 48) << "," << int(w.Payload()) << "}";
    }
    return strm;
  }
  friend constexpr bool ApproxEqual(const BooleanWeight& lhs, const BooleanWeight& rhs,
                                    [[maybe_unused]] float delta = kDelta) {
    return lhs == rhs;
  }

 private:
  static constexpr PayloadType False = 0;
  static constexpr PayloadType True = 1;     // "is true" mask
  static constexpr PayloadType Invalid = 2; // "not a member" value

  PayloadType v_{Invalid};
  PayloadType p_{};
};

template<typename T>
struct RangeLabel {
  using ValueType = T;

  constexpr RangeLabel() noexcept = default;

  constexpr RangeLabel(ValueType label) noexcept
    : min(label), max(label) {
  }
  constexpr RangeLabel(ValueType min, ValueType max) noexcept
    : min(min), max(max) {
  }
  friend std::ostream& operator<<(std::ostream& strm, const RangeLabel& l) {
    strm << '[' << l.min << ".." << l.max << ']';
    return strm;
  }

  ValueType min{kNoLabel};
  ValueType max{kNoLabel};
}; // RangeLabel

constexpr uint64_t EncodeRange(uint32_t min, uint32_t max) noexcept {
  return uint64_t(min) << 32 | uint64_t(max);
}

constexpr uint64_t EncodeRange(uint32_t v) noexcept {
  return EncodeRange(v, v);
}

template<typename Label>
struct EmptyLabel {
  constexpr EmptyLabel() noexcept = default;
  constexpr EmptyLabel& operator=(Label) noexcept { return *this; }
  constexpr bool operator==(EmptyLabel) const noexcept { return true; }
  constexpr bool operator!=(EmptyLabel) const noexcept { return false; }
  constexpr bool operator==(Label) const noexcept { return true; }
  constexpr bool operator!=(Label) const noexcept { return false; }
  constexpr bool operator<(EmptyLabel) const noexcept { return false; }
  constexpr bool operator>(EmptyLabel) const noexcept { return false; }
  constexpr operator Label() const noexcept { return kNoLabel; }
  constexpr operator Label() noexcept { return kNoLabel; }
  constexpr void Write(std::ostream&) const noexcept { }

  friend constexpr bool operator==(Label, EmptyLabel) noexcept { return true; }
  friend constexpr bool operator!=(Label, EmptyLabel) noexcept { return false; }
  friend constexpr std::ostream& operator<<(std::ostream& strm, EmptyLabel) noexcept {
    return strm;
  }
}; // EmptyLabel

template<typename Weight>
struct EmptyWeight {
  using ReverseWeight = EmptyWeight;

  constexpr EmptyWeight& operator=(Weight) noexcept { return *this; }

  constexpr ReverseWeight Reverse() const noexcept { return *this; }
  constexpr EmptyWeight Quantize([[maybe_unused]]float delta = kDelta) const noexcept { return {};  }
  constexpr operator Weight() const noexcept { return Weight::One(); }
  constexpr operator Weight() noexcept { return Weight::One(); }
  constexpr bool operator==(EmptyWeight) const noexcept { return true; }
  constexpr bool operator!=(EmptyWeight) const noexcept { return false; }

  std::ostream& Write(std::ostream& strm) const {
    Weight::One().Write(strm);
    return strm;
  }

  std::istream& Read(std::istream& strm) {
    Weight().Read(strm);
    return strm;
  }
}; // EmptyWeight

template<typename W = BooleanWeight, typename L = int32_t>
struct Transition {
  using Weight = W;
  using Label = L;
  using StateId = int32_t;

  static const std::string &Type() {
    static const std::string type("Transition");
    return type;
  }

  Label ilabel{fst::kNoLabel};
  union {
    StateId nextstate{fst::kNoStateId};
    EmptyLabel<Label> olabel;
    EmptyWeight<Weight> weight; // all arcs are trivial
  };

  constexpr Transition() = default;

  constexpr Transition(Label ilabel, StateId nextstate)
    : ilabel(ilabel),
      nextstate(nextstate) {
  }

  // satisfy openfst API
  constexpr Transition(Label ilabel, Label, Weight, StateId nextstate)
    : ilabel(ilabel),
      nextstate(nextstate) {
  }

  // satisfy openfst API
  constexpr Transition(Label ilabel, Label, StateId nextstate)
    : ilabel(ilabel),
      nextstate(nextstate) {
  }
}; // Transition

static_assert(sizeof(Transition<>) == sizeof(Transition<>::Label) + sizeof(Transition<>::StateId));

constexpr const int32_t kEps   = 0;        // match all + don't consume symbol
constexpr const int32_t kRho   = irs::integer_traits<int32_t>::const_max; // match rest + consume symbol
constexpr const int32_t kPhi   = kRho - 1; // match rest + don't consume symbol
constexpr const int32_t kSigma = kPhi - 1; // match all + consume symbol

constexpr const int32_t kMinLabel = 0;
constexpr const int32_t kMaxLabel = kSigma - 1;

NS_END // fsa
NS_END // fst

NS_BEGIN(std)

template<typename T, typename W>
inline void swap(::fst::fsa::RangeLabel<T>& lhs,
                 typename ::fst::fsa::EmptyLabel<W>& /*rhs*/) noexcept {
  lhs = ::fst::kNoLabel;
}

template<typename W>
inline void swap(int32_t& lhs, typename ::fst::fsa::EmptyLabel<W>& /*rhs*/) noexcept {
  lhs = ::fst::kNoLabel;
}

template<typename W>
inline void swap(typename ::fst::fsa::EmptyLabel<W>& /*lhs*/, int32_t& rhs) noexcept {
  rhs = ::fst::kNoLabel;
}

template<typename T>
struct hash<::fst::fsa::RangeLabel<T>> {
  size_t operator()(const ::fst::fsa::RangeLabel<T>& label) const noexcept {
    return hash<uint64_t>()(uint64_t(label.min) | uint64_t(label.max) << 32);
  }
};

template<typename W>
struct hash<typename ::fst::fsa::EmptyLabel<W>> {
  size_t operator()(typename ::fst::fsa::EmptyLabel<W>) const noexcept {
    return 0;
  }
};

NS_END

#if defined(_MSC_VER)
  // NOOP
#elif defined (__GNUC__)
  #pragma GCC diagnostic push
  #pragma GCC diagnostic ignored "-Wsign-compare"
#endif

#include <fst/vector-fst.h>
#include <fst/matcher.h>

#if defined(_MSC_VER)
  // NOOP
#elif defined (__GNUC__)
  #pragma GCC diagnostic pop
#endif

NS_BEGIN(fst)
NS_BEGIN(fsa)

template<typename W, typename L>
using AutomatonMatcher = SortedMatcher<Automaton<W, L>>;

NS_END
NS_END

#endif // IRESEARCH_AUTOMATON_H
