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
// FST property bits.

#ifndef FST_PROPERTIES_H_
#define FST_PROPERTIES_H_

#include <sys/types.h>

#include <cstdint>
#include <vector>

#include <fst/compat.h>
#include <fst/log.h>
#include <string_view>

namespace fst {

// The property bits here assert facts about an FST. If individual bits are
// added, then the composite properties below, the property functions and
// property names in properties.cc, and TestProperties() in test-properties.h
// should be updated.

// BINARY PROPERTIES
//
// For each property below, there is a single bit. If it is set, the property is
// true. If it is not set, the property is false.

// The Fst is an ExpandedFst.
inline constexpr uint64_t kExpanded = 0x0000000000000001ULL;

// The Fst is a MutableFst.
inline constexpr uint64_t kMutable = 0x0000000000000002ULL;

// An error was detected while constructing/using the FST.
inline constexpr uint64_t kError = 0x0000000000000004ULL;

// TRINARY PROPERTIES
//
// For each of these properties below there is a pair of property bits, one
// positive and one negative. If the positive bit is set, the property is true.
// If the negative bit is set, the property is false. If neither is set, the
// property has unknown value. Both should never be simultaneously set. The
// individual positive and negative bit pairs should be adjacent with the
// positive bit at an odd and lower position.

// ilabel == olabel for each arc.
inline constexpr uint64_t kAcceptor = 0x0000000000010000ULL;
// ilabel != olabel for some arc.
inline constexpr uint64_t kNotAcceptor = 0x0000000000020000ULL;

// ilabels unique leaving each state.
inline constexpr uint64_t kIDeterministic = 0x0000000000040000ULL;
// ilabels not unique leaving some state.
inline constexpr uint64_t kNonIDeterministic = 0x0000000000080000ULL;

// olabels unique leaving each state.
inline constexpr uint64_t kODeterministic = 0x0000000000100000ULL;
// olabels not unique leaving some state.
inline constexpr uint64_t kNonODeterministic = 0x0000000000200000ULL;

// FST has input/output epsilons.
inline constexpr uint64_t kEpsilons = 0x0000000000400000ULL;
// FST has no input/output epsilons.
inline constexpr uint64_t kNoEpsilons = 0x0000000000800000ULL;

// FST has input epsilons.
inline constexpr uint64_t kIEpsilons = 0x0000000001000000ULL;
// FST has no input epsilons.
inline constexpr uint64_t kNoIEpsilons = 0x0000000002000000ULL;

// FST has output epsilons.
inline constexpr uint64_t kOEpsilons = 0x0000000004000000ULL;
// FST has no output epsilons.
inline constexpr uint64_t kNoOEpsilons = 0x0000000008000000ULL;

// ilabels sorted wrt < for each state.
inline constexpr uint64_t kILabelSorted = 0x0000000010000000ULL;
// ilabels not sorted wrt < for some state.
inline constexpr uint64_t kNotILabelSorted = 0x0000000020000000ULL;

// olabels sorted wrt < for each state.
inline constexpr uint64_t kOLabelSorted = 0x0000000040000000ULL;
// olabels not sorted wrt < for some state.
inline constexpr uint64_t kNotOLabelSorted = 0x0000000080000000ULL;

// Non-trivial arc or final weights.
inline constexpr uint64_t kWeighted = 0x0000000100000000ULL;
// Only trivial arc and final weights.
inline constexpr uint64_t kUnweighted = 0x0000000200000000ULL;

// FST has cycles.
inline constexpr uint64_t kCyclic = 0x0000000400000000ULL;
// FST has no cycles.
inline constexpr uint64_t kAcyclic = 0x0000000800000000ULL;

// FST has cycles containing the initial state.
inline constexpr uint64_t kInitialCyclic = 0x0000001000000000ULL;
// FST has no cycles containing the initial state.
inline constexpr uint64_t kInitialAcyclic = 0x0000002000000000ULL;

// FST is topologically sorted.
inline constexpr uint64_t kTopSorted = 0x0000004000000000ULL;
// FST is not topologically sorted.
inline constexpr uint64_t kNotTopSorted = 0x0000008000000000ULL;

// All states reachable from the initial state.
inline constexpr uint64_t kAccessible = 0x0000010000000000ULL;
// Not all states reachable from the initial state.
inline constexpr uint64_t kNotAccessible = 0x0000020000000000ULL;

// All states can reach a final state.
inline constexpr uint64_t kCoAccessible = 0x0000040000000000ULL;
// Not all states can reach a final state.
inline constexpr uint64_t kNotCoAccessible = 0x0000080000000000ULL;

// If NumStates() > 0, then state 0 is initial, state NumStates() - 1 is final,
// there is a transition from each non-final state i to state i + 1, and there
// are no other transitions.
inline constexpr uint64_t kString = 0x0000100000000000ULL;

// Not a string FST.
inline constexpr uint64_t kNotString = 0x0000200000000000ULL;

// FST has at least one weighted cycle.
inline constexpr uint64_t kWeightedCycles = 0x0000400000000000ULL;

// FST has no weighted cycles. Any cycles that may be present are unweighted.
inline constexpr uint64_t kUnweightedCycles = 0x0000800000000000ULL;

// COMPOSITE PROPERTIES

// Properties of an empty machine.
inline constexpr uint64_t kNullProperties =
    kAcceptor | kIDeterministic | kODeterministic | kNoEpsilons | kNoIEpsilons |
    kNoOEpsilons | kILabelSorted | kOLabelSorted | kUnweighted | kAcyclic |
    kInitialAcyclic | kTopSorted | kAccessible | kCoAccessible | kString |
    kUnweightedCycles;

// Properties of a string FST compiled into a string.
inline constexpr uint64_t kCompiledStringProperties =
    kAcceptor | kString | kUnweighted | kIDeterministic | kODeterministic |
    kILabelSorted | kOLabelSorted | kAcyclic | kInitialAcyclic |
    kUnweightedCycles | kTopSorted | kAccessible | kCoAccessible;

// Properties that are preserved when an FST is copied.
inline constexpr uint64_t kCopyProperties =
    kError | kAcceptor | kNotAcceptor | kIDeterministic | kNonIDeterministic |
    kODeterministic | kNonODeterministic | kEpsilons | kNoEpsilons |
    kIEpsilons | kNoIEpsilons | kOEpsilons | kNoOEpsilons | kILabelSorted |
    kNotILabelSorted | kOLabelSorted | kNotOLabelSorted | kWeighted |
    kUnweighted | kCyclic | kAcyclic | kInitialCyclic | kInitialAcyclic |
    kTopSorted | kNotTopSorted | kAccessible | kNotAccessible | kCoAccessible |
    kNotCoAccessible | kString | kNotString | kWeightedCycles |
    kUnweightedCycles;

// Properties that are intrinsic to the FST.
inline constexpr uint64_t kIntrinsicProperties =
    kExpanded | kMutable | kAcceptor | kNotAcceptor | kIDeterministic |
    kNonIDeterministic | kODeterministic | kNonODeterministic | kEpsilons |
    kNoEpsilons | kIEpsilons | kNoIEpsilons | kOEpsilons | kNoOEpsilons |
    kILabelSorted | kNotILabelSorted | kOLabelSorted | kNotOLabelSorted |
    kWeighted | kUnweighted | kCyclic | kAcyclic | kInitialCyclic |
    kInitialAcyclic | kTopSorted | kNotTopSorted | kAccessible |
    kNotAccessible | kCoAccessible | kNotCoAccessible | kString | kNotString |
    kWeightedCycles | kUnweightedCycles;

// Properties that are (potentially) extrinsic to the FST.
inline constexpr uint64_t kExtrinsicProperties = kError;

// Properties that are preserved when an FST start state is set.
inline constexpr uint64_t kSetStartProperties =
    kExpanded | kMutable | kError | kAcceptor | kNotAcceptor | kIDeterministic |
    kNonIDeterministic | kODeterministic | kNonODeterministic | kEpsilons |
    kNoEpsilons | kIEpsilons | kNoIEpsilons | kOEpsilons | kNoOEpsilons |
    kILabelSorted | kNotILabelSorted | kOLabelSorted | kNotOLabelSorted |
    kWeighted | kUnweighted | kCyclic | kAcyclic | kTopSorted | kNotTopSorted |
    kCoAccessible | kNotCoAccessible | kWeightedCycles | kUnweightedCycles;

// Properties that are preserved when an FST final weight is set.
inline constexpr uint64_t kSetFinalProperties =
    kExpanded | kMutable | kError | kAcceptor | kNotAcceptor | kIDeterministic |
    kNonIDeterministic | kODeterministic | kNonODeterministic | kEpsilons |
    kNoEpsilons | kIEpsilons | kNoIEpsilons | kOEpsilons | kNoOEpsilons |
    kILabelSorted | kNotILabelSorted | kOLabelSorted | kNotOLabelSorted |
    kCyclic | kAcyclic | kInitialCyclic | kInitialAcyclic | kTopSorted |
    kNotTopSorted | kAccessible | kNotAccessible | kWeightedCycles |
    kUnweightedCycles;

// Properties that are preserved when an FST state is added.
inline constexpr uint64_t kAddStateProperties =
    kExpanded | kMutable | kError | kAcceptor | kNotAcceptor | kIDeterministic |
    kNonIDeterministic | kODeterministic | kNonODeterministic | kEpsilons |
    kNoEpsilons | kIEpsilons | kNoIEpsilons | kOEpsilons | kNoOEpsilons |
    kILabelSorted | kNotILabelSorted | kOLabelSorted | kNotOLabelSorted |
    kWeighted | kUnweighted | kCyclic | kAcyclic | kInitialCyclic |
    kInitialAcyclic | kTopSorted | kNotTopSorted | kNotAccessible |
    kNotCoAccessible | kNotString | kWeightedCycles | kUnweightedCycles;

// Properties that are preserved when an FST arc is added.
inline constexpr uint64_t kAddArcProperties =
    kExpanded | kMutable | kError | kNotAcceptor | kNonIDeterministic |
    kNonODeterministic | kEpsilons | kIEpsilons | kOEpsilons |
    kNotILabelSorted | kNotOLabelSorted | kWeighted | kCyclic | kInitialCyclic |
    kNotTopSorted | kAccessible | kCoAccessible | kWeightedCycles;

// Properties that are preserved when an FST arc is set.
inline constexpr uint64_t kSetArcProperties = kExpanded | kMutable | kError;

// Properties that are preserved when FST states are deleted.
inline constexpr uint64_t kDeleteStatesProperties =
    kExpanded | kMutable | kError | kAcceptor | kIDeterministic |
    kODeterministic | kNoEpsilons | kNoIEpsilons | kNoOEpsilons |
    kILabelSorted | kOLabelSorted | kUnweighted | kAcyclic | kInitialAcyclic |
    kTopSorted | kUnweightedCycles;

// Properties that are preserved when FST arcs are deleted.
inline constexpr uint64_t kDeleteArcsProperties =
    kExpanded | kMutable | kError | kAcceptor | kIDeterministic |
    kODeterministic | kNoEpsilons | kNoIEpsilons | kNoOEpsilons |
    kILabelSorted | kOLabelSorted | kUnweighted | kAcyclic | kInitialAcyclic |
    kTopSorted | kNotAccessible | kNotCoAccessible | kUnweightedCycles;

// Properties that are preserved when an FST's states are reordered.
inline constexpr uint64_t kStateSortProperties =
    kExpanded | kMutable | kError | kAcceptor | kNotAcceptor | kIDeterministic |
    kNonIDeterministic | kODeterministic | kNonODeterministic | kEpsilons |
    kNoEpsilons | kIEpsilons | kNoIEpsilons | kOEpsilons | kNoOEpsilons |
    kILabelSorted | kNotILabelSorted | kOLabelSorted | kNotOLabelSorted |
    kWeighted | kUnweighted | kCyclic | kAcyclic | kInitialCyclic |
    kInitialAcyclic | kAccessible | kNotAccessible | kCoAccessible |
    kNotCoAccessible | kWeightedCycles | kUnweightedCycles;

// Properties that are preserved when an FST's arcs are reordered.
inline constexpr uint64_t kArcSortProperties =
    kExpanded | kMutable | kError | kAcceptor | kNotAcceptor | kIDeterministic |
    kNonIDeterministic | kODeterministic | kNonODeterministic | kEpsilons |
    kNoEpsilons | kIEpsilons | kNoIEpsilons | kOEpsilons | kNoOEpsilons |
    kWeighted | kUnweighted | kCyclic | kAcyclic | kInitialCyclic |
    kInitialAcyclic | kTopSorted | kNotTopSorted | kAccessible |
    kNotAccessible | kCoAccessible | kNotCoAccessible | kString | kNotString |
    kWeightedCycles | kUnweightedCycles;

// Properties that are preserved when an FST's input labels are changed.
inline constexpr uint64_t kILabelInvariantProperties =
    kExpanded | kMutable | kError | kODeterministic | kNonODeterministic |
    kOEpsilons | kNoOEpsilons | kOLabelSorted | kNotOLabelSorted | kWeighted |
    kUnweighted | kCyclic | kAcyclic | kInitialCyclic | kInitialAcyclic |
    kTopSorted | kNotTopSorted | kAccessible | kNotAccessible | kCoAccessible |
    kNotCoAccessible | kString | kNotString | kWeightedCycles |
    kUnweightedCycles;

// Properties that are preserved when an FST's output labels are changed.
inline constexpr uint64_t kOLabelInvariantProperties =
    kExpanded | kMutable | kError | kIDeterministic | kNonIDeterministic |
    kIEpsilons | kNoIEpsilons | kILabelSorted | kNotILabelSorted | kWeighted |
    kUnweighted | kCyclic | kAcyclic | kInitialCyclic | kInitialAcyclic |
    kTopSorted | kNotTopSorted | kAccessible | kNotAccessible | kCoAccessible |
    kNotCoAccessible | kString | kNotString | kWeightedCycles |
    kUnweightedCycles;

// Properties that are preserved when an FST's weights are changed. This
// assumes that the set of states that are non-final is not changed.
inline constexpr uint64_t kWeightInvariantProperties =
    kExpanded | kMutable | kError | kAcceptor | kNotAcceptor | kIDeterministic |
    kNonIDeterministic | kODeterministic | kNonODeterministic | kEpsilons |
    kNoEpsilons | kIEpsilons | kNoIEpsilons | kOEpsilons | kNoOEpsilons |
    kILabelSorted | kNotILabelSorted | kOLabelSorted | kNotOLabelSorted |
    kCyclic | kAcyclic | kInitialCyclic | kInitialAcyclic | kTopSorted |
    kNotTopSorted | kAccessible | kNotAccessible | kCoAccessible |
    kNotCoAccessible | kString | kNotString;

// Properties that are preserved when a superfinal state is added and an FST's
// final weights are directed to it via new transitions.
inline constexpr uint64_t kAddSuperFinalProperties =
    kExpanded | kMutable | kError | kAcceptor | kNotAcceptor |
    kNonIDeterministic | kNonODeterministic | kEpsilons | kIEpsilons |
    kOEpsilons | kNotILabelSorted | kNotOLabelSorted | kWeighted | kUnweighted |
    kCyclic | kAcyclic | kInitialCyclic | kInitialAcyclic | kNotTopSorted |
    kNotAccessible | kCoAccessible | kNotCoAccessible | kNotString |
    kWeightedCycles | kUnweightedCycles;

// Properties that are preserved when a superfinal state is removed and the
// epsilon transitions directed to it are made final weights.
inline constexpr uint64_t kRmSuperFinalProperties =
    kExpanded | kMutable | kError | kAcceptor | kNotAcceptor | kIDeterministic |
    kODeterministic | kNoEpsilons | kNoIEpsilons | kNoOEpsilons |
    kILabelSorted | kOLabelSorted | kWeighted | kUnweighted | kCyclic |
    kAcyclic | kInitialCyclic | kInitialAcyclic | kTopSorted | kAccessible |
    kCoAccessible | kNotCoAccessible | kString | kWeightedCycles |
    kUnweightedCycles;

// All binary properties.
inline constexpr uint64_t kBinaryProperties = 0x0000000000000007ULL;

// All trinary properties.
inline constexpr uint64_t kTrinaryProperties = 0x0000ffffffff0000ULL;

// COMPUTED PROPERTIES

// 1st bit of trinary properties.
inline constexpr uint64_t kPosTrinaryProperties =
    kTrinaryProperties & 0x5555555555555555ULL;

// 2nd bit of trinary properties.
inline constexpr uint64_t kNegTrinaryProperties =
    kTrinaryProperties & 0xaaaaaaaaaaaaaaaaULL;

// All properties.
inline constexpr uint64_t kFstProperties =
    kBinaryProperties | kTrinaryProperties;

// PROPERTY FUNCTIONS and STRING NAMES (defined in properties.cc).

// Below are functions for getting property bit vectors when executing
// mutation operations.
inline uint64_t SetStartProperties(uint64_t inprops);

template <typename Weight>
uint64_t SetFinalProperties(uint64_t inprops, const Weight &old_weight,
                            const Weight &new_weight);

inline uint64_t AddStateProperties(uint64_t inprops);

template <typename A>
uint64_t AddArcProperties(uint64_t inprops, typename A::StateId s, const A &arc,
                          const A *prev_arc);

inline uint64_t DeleteStatesProperties(uint64_t inprops);

inline uint64_t DeleteAllStatesProperties(uint64_t inprops,
                                          uint64_t staticProps);

inline uint64_t DeleteArcsProperties(uint64_t inprops);

uint64_t ClosureProperties(uint64_t inprops, bool star, bool delayed = false);

uint64_t ComplementProperties(uint64_t inprops);

uint64_t ComposeProperties(uint64_t inprops1, uint64_t inprops2);

uint64_t ConcatProperties(uint64_t inprops1, uint64_t inprops2,
                          bool delayed = false);

uint64_t DeterminizeProperties(uint64_t inprops, bool has_subsequential_label,
                               bool distinct_psubsequential_labels);

uint64_t FactorWeightProperties(uint64_t inprops);

uint64_t InvertProperties(uint64_t inprops);

uint64_t ProjectProperties(uint64_t inprops, bool project_input);

uint64_t RandGenProperties(uint64_t inprops, bool weighted);

uint64_t RelabelProperties(uint64_t inprops);

uint64_t ReplaceProperties(const std::vector<uint64_t> &inprops, size_t root,
                           bool epsilon_on_call, bool epsilon_on_return,
                           bool out_epsilon_on_call, bool out_epsilon_on_return,
                           bool replace_transducer, bool no_empty_fst,
                           bool all_ilabel_sorted, bool all_olabel_sorted,
                           bool all_negative_or_dense);

uint64_t ReverseProperties(uint64_t inprops, bool has_superinitial);

uint64_t ReweightProperties(uint64_t inprops, bool added_start_epsilon);

uint64_t RmEpsilonProperties(uint64_t inprops, bool delayed = false);

uint64_t ShortestPathProperties(uint64_t props, bool tree = false);

uint64_t SynchronizeProperties(uint64_t inprops);

uint64_t UnionProperties(uint64_t inprops1, uint64_t inprops2,
                         bool delayed = false);

// Definitions of inlined functions.

uint64_t SetStartProperties(uint64_t inprops) {
  auto outprops = inprops & kSetStartProperties;
  if (inprops & kAcyclic) {
    outprops |= kInitialAcyclic;
  }
  return outprops;
}

uint64_t AddStateProperties(uint64_t inprops) {
  return inprops & kAddStateProperties;
}

uint64_t DeleteStatesProperties(uint64_t inprops) {
  return inprops & kDeleteStatesProperties;
}

uint64_t DeleteAllStatesProperties(uint64_t inprops, uint64_t staticprops) {
  const auto outprops = inprops & kError;
  return outprops | kNullProperties | staticprops;
}

uint64_t DeleteArcsProperties(uint64_t inprops) {
  return inprops & kDeleteArcsProperties;
}

// Definitions of template functions.

template <typename Weight>
uint64_t SetFinalProperties(uint64_t inprops, const Weight &old_weight,
                            const Weight &new_weight) {
  auto outprops = inprops;
  if (old_weight != Weight::Zero() && old_weight != Weight::One()) {
    outprops &= ~kWeighted;
  }
  if (new_weight != Weight::Zero() && new_weight != Weight::One()) {
    outprops |= kWeighted;
    outprops &= ~kUnweighted;
  }
  outprops &= kSetFinalProperties | kWeighted | kUnweighted;
  return outprops;
}

/// Gets the properties for the MutableFst::AddArc method.
///
/// \param inprops  the current properties of the FST
/// \param s        the ID of the state to which an arc is being added.
/// \param arc      the arc being added to the state with the specified ID
/// \param prev_arc the previously-added (or "last") arc of state s, or nullptr
//                  if s currently has no arcs.
template <typename Arc>
uint64_t AddArcProperties(uint64_t inprops, typename Arc::StateId s,
                          const Arc &arc, const Arc *prev_arc) {
  using Weight = typename Arc::Weight;
  auto outprops = inprops;
  if (arc.ilabel != arc.olabel) {
    outprops |= kNotAcceptor;
    outprops &= ~kAcceptor;
  }
  if (arc.ilabel == 0) {
    outprops |= kIEpsilons;
    outprops &= ~kNoIEpsilons;
    if (arc.olabel == 0) {
      outprops |= kEpsilons;
      outprops &= ~kNoEpsilons;
    }
  }
  if (arc.olabel == 0) {
    outprops |= kOEpsilons;
    outprops &= ~kNoOEpsilons;
  }
  if (prev_arc) {
    if (prev_arc->ilabel > arc.ilabel) {
      outprops |= kNotILabelSorted;
      outprops &= ~kILabelSorted;
    }
    if (prev_arc->olabel > arc.olabel) {
      outprops |= kNotOLabelSorted;
      outprops &= ~kOLabelSorted;
    }
  }
  if (arc.weight != Weight::Zero() && arc.weight != Weight::One()) {
    outprops |= kWeighted;
    outprops &= ~kUnweighted;
  }
  if (arc.nextstate <= s) {
    outprops |= kNotTopSorted;
    outprops &= ~kTopSorted;
  }
  outprops &= kAddArcProperties | kAcceptor | kNoEpsilons | kNoIEpsilons |
              kNoOEpsilons | kILabelSorted | kOLabelSorted | kUnweighted |
              kTopSorted;
  if (outprops & kTopSorted) {
    outprops |= kAcyclic | kInitialAcyclic;
  }
  return outprops;
}

namespace internal {

extern const std::string_view PropertyNames[];

// For a binary property, the bit is always returned set. For a trinary (i.e.,
// two-bit) property, both bits are returned set iff either corresponding input
// bit is set.
inline uint64_t KnownProperties(uint64_t props) {
  return kBinaryProperties | (props & kTrinaryProperties) |
         ((props & kPosTrinaryProperties) << 1) |
         ((props & kNegTrinaryProperties) >> 1);
}

// Tests compatibility between two sets of properties.
inline bool CompatProperties(uint64_t props1, uint64_t props2) {
  const auto known_props1 = KnownProperties(props1);
  const auto known_props2 = KnownProperties(props2);
  const auto known_props = known_props1 & known_props2;
  const auto incompat_props = (props1 & known_props) ^ (props2 & known_props);
  if (incompat_props) {
    uint64_t prop = 1;
    for (int i = 0; i < 64; ++i, prop <<= 1) {
      if (prop & incompat_props) {
        LOG(ERROR) << "CompatProperties: Mismatch: "
                   << internal::PropertyNames[i]
                   << ": props1 = " << (props1 & prop ? "true" : "false")
                   << ", props2 = " << (props2 & prop ? "true" : "false");
      }
    }
    return false;
  } else {
    return true;
  }
}

}  // namespace internal
}  // namespace fst

#endif  // FST_PROPERTIES_H_
