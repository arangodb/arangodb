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

#include <vector>

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
#include <fst/connect.h>
#include <fst/test-properties.h>

#if defined(_MSC_VER)
  // NOOP
#elif defined (__GNUC__)
  #pragma GCC diagnostic pop
#endif

#include "utils/automaton_decl.hpp"
#include "utils/fstext/fst_utils.hpp"
#include "utils/string.hpp"

namespace fst {
namespace fsa {

////////////////////////////////////////////////////////////////////////////////
/// @class BooleanWeight
////////////////////////////////////////////////////////////////////////////////
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
}; // BooleanWeight

////////////////////////////////////////////////////////////////////////////////
/// @struct RangeLabel
////////////////////////////////////////////////////////////////////////////////
struct RangeLabel {
  static constexpr RangeLabel fromRange(uint32_t min, uint32_t max) noexcept {
    return RangeLabel{min, max};
  }
  static constexpr RangeLabel fromRange(uint32_t min) noexcept {
    return fromRange(min, min);
  }
  static constexpr RangeLabel fromLabel(int64_t label) noexcept {
    return RangeLabel{label};
  }

  constexpr RangeLabel() noexcept
    : ilabel{fst::kNoLabel} {
  }

  constexpr RangeLabel(uint32_t min, uint32_t max) noexcept
    : max{max}, min{min} {
  }

  constexpr explicit RangeLabel(int64_t ilabel) noexcept
    : ilabel{ilabel} {
  }

  constexpr operator int64_t() const noexcept {
    return ilabel;
  }

  friend std::ostream& operator<<(std::ostream& strm, const RangeLabel& l) {
    strm << '[' << l.min << ".." << l.max << ']';
    return strm;
  }

  union {
    int64_t ilabel;
#ifdef IRESEARCH_BIG_ENDIAN
    struct {
      uint32_t min;
      uint32_t max;
    };
#else
    struct {
      uint32_t max;
      uint32_t min;
    };
#endif
  };
}; // RangeLabel

////////////////////////////////////////////////////////////////////////////////
/// @struct Transition
////////////////////////////////////////////////////////////////////////////////
template<typename W = BooleanWeight>
struct Transition : RangeLabel {
  using Weight = W;
  using Label = int64_t;
  using StateId = int32_t;

  static const std::string &Type() {
    static const std::string type("Transition");
    return type;
  }

  union {
    StateId nextstate{fst::kNoStateId};
    fstext::EmptyLabel<Label> olabel;
    fstext::EmptyWeight<Weight> weight; // all arcs are trivial
  };

  constexpr Transition() = default;

  constexpr Transition(RangeLabel ilabel, StateId nextstate)
    : RangeLabel{ilabel},
      nextstate(nextstate) {
  }

  constexpr Transition(Label ilabel, StateId nextstate)
    : RangeLabel{ilabel},
      nextstate{nextstate} {
  }

  // satisfy openfst API
  constexpr Transition(Label ilabel, Label, Weight, StateId nextstate)
    : RangeLabel{ilabel},
      nextstate{nextstate} {
  }

  // satisfy openfst API
  constexpr Transition(Label ilabel, Label, StateId nextstate)
    : RangeLabel{ilabel},
      nextstate{nextstate} {
  }
}; // Transition

} // fsa

template<typename W>
uint64 ComputeProperties(
    const Fst<fsa::Transition<W>> &fst, uint64 mask,
    uint64 *known, bool use_stored) {
  using Arc = fsa::Transition<W>;
  using Label = typename Arc::Label;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;
  const auto fst_props = fst.Properties(kFstProperties, false);  // FST-stored.
  // Check stored FST properties first if allowed.
  if (use_stored) {
    const auto known_props = KnownProperties(fst_props);
    // If FST contains required info, return it.
    if ((known_props & mask) == mask) {
      if (known) *known = known_props;
      return fst_props;
    }
  }
  // Computes (trinary) properties explicitly.
  // Initialize with binary properties (already known).
  uint64 comp_props = fst_props & kBinaryProperties;
  // Computes these trinary properties with a DFS. We compute only those that
  // need a DFS here, since we otherwise would like to avoid a DFS since its
  // stack could grow large.
  uint64 dfs_props = kCyclic | kAcyclic | kInitialCyclic | kInitialAcyclic |
                     kAccessible | kNotAccessible | kCoAccessible |
                     kNotCoAccessible;
  std::vector<StateId> scc;
  if (mask & (dfs_props | kWeightedCycles | kUnweightedCycles)) {
    SccVisitor<Arc> scc_visitor(&scc, nullptr, nullptr, &comp_props);
    DfsVisit(fst, &scc_visitor);
  }
  // Computes any remaining trinary properties via a state and arcs iterations
  if (mask & ~(kBinaryProperties | dfs_props)) {
    comp_props |= kAcceptor | kNoEpsilons | kNoIEpsilons | kNoOEpsilons |
                  kILabelSorted | kOLabelSorted | kUnweighted | kTopSorted |
                  kString;
    if (mask & (kIDeterministic | kNonIDeterministic)) {
      comp_props |= kIDeterministic;
    }
    if (mask & (kODeterministic | kNonODeterministic)) {
      comp_props |= kODeterministic;
    }
    if (mask & (dfs_props | kWeightedCycles | kUnweightedCycles)) {
      comp_props |= kUnweightedCycles;
    }

    struct RangeLabelComparer {
      bool operator()(Label lhs, Label rhs) const noexcept {
        if (lhs > rhs) {
          std::swap(lhs, rhs);
        }

        fsa::RangeLabel lhsRange{lhs};
        fsa::RangeLabel rhsRange{rhs};

        return lhsRange.min < rhsRange.min && lhsRange.max < rhsRange.min;
      }
    };

    using Labels = std::set<Label, RangeLabelComparer>;

    std::unique_ptr<Labels> ilabels;
    std::unique_ptr<Labels> olabels;
    StateId nfinal = 0;
    for (StateIterator<Fst<Arc>> siter(fst); !siter.Done(); siter.Next()) {
      StateId s = siter.Value();
      Arc prev_arc;
      // Creates these only if we need to.
      if (mask & (kIDeterministic | kNonIDeterministic)) {
        ilabels.reset(new Labels());
      }
      if (mask & (kODeterministic | kNonODeterministic)) {
        olabels.reset(new Labels());
      }
      bool first_arc = true;
      for (ArcIterator<Fst<Arc>> aiter(fst, s); !aiter.Done(); aiter.Next()) {
        const auto &arc = aiter.Value();
        if (ilabels && ilabels->find(arc.ilabel) != ilabels->end()) {
          comp_props |= kNonIDeterministic;
          comp_props &= ~kIDeterministic;
        }
        if (olabels && olabels->find(arc.olabel) != olabels->end()) {
          comp_props |= kNonODeterministic;
          comp_props &= ~kODeterministic;
        }
        if (arc.ilabel != arc.olabel) {
          comp_props |= kNotAcceptor;
          comp_props &= ~kAcceptor;
        }
        if (arc.ilabel == 0 && arc.olabel == 0) {
          comp_props |= kEpsilons;
          comp_props &= ~kNoEpsilons;
        }
        if (arc.ilabel == 0) {
          comp_props |= kIEpsilons;
          comp_props &= ~kNoIEpsilons;
        }
        if (arc.olabel == 0) {
          comp_props |= kOEpsilons;
          comp_props &= ~kNoOEpsilons;
        }
        if (!first_arc) {
          if (arc.ilabel < prev_arc.ilabel) {
            comp_props |= kNotILabelSorted;
            comp_props &= ~kILabelSorted;
          }
          if (arc.olabel < prev_arc.olabel) {
            comp_props |= kNotOLabelSorted;
            comp_props &= ~kOLabelSorted;
          }
        }
        if (arc.weight != Weight::One() && arc.weight != Weight::Zero()) {
          comp_props |= kWeighted;
          comp_props &= ~kUnweighted;
          if ((comp_props & kUnweightedCycles) &&
              scc[s] == scc[arc.nextstate]) {
            comp_props |= kWeightedCycles;
            comp_props &= ~kUnweightedCycles;
          }
        }
        if (arc.nextstate <= s) {
          comp_props |= kNotTopSorted;
          comp_props &= ~kTopSorted;
        }
        if (arc.nextstate != s + 1) {
          comp_props |= kNotString;
          comp_props &= ~kString;
        }
        prev_arc = arc;
        first_arc = false;
        if (ilabels) ilabels->insert(arc.ilabel);
        if (olabels) olabels->insert(arc.olabel);
      }

      if (nfinal > 0) {  // Final state not last.
        comp_props |= kNotString;
        comp_props &= ~kString;
      }
      const auto final_weight = fst.Final(s);
      if (final_weight != Weight::Zero()) {  // Final state.
        if (final_weight != Weight::One()) {
          comp_props |= kWeighted;
          comp_props &= ~kUnweighted;
        }
        ++nfinal;
      } else {  // Non-final state.
        if (fst.NumArcs(s) != 1) {
          comp_props |= kNotString;
          comp_props &= ~kString;
        }
      }
    }
    if (fst.Start() != kNoStateId && fst.Start() != 0) {
      comp_props |= kNotString;
      comp_props &= ~kString;
    }
  }
  if (known) *known = KnownProperties(comp_props);
  return comp_props;
}

} // fst

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

#endif // IRESEARCH_AUTOMATON_H
