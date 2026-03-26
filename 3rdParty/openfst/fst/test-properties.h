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
// Functions to manipulate and test property bits.

#ifndef FST_TEST_PROPERTIES_H_
#define FST_TEST_PROPERTIES_H_

#include <cstdint>

#include <fst/flags.h>

#include <fst/connect.h>
#include <fst/dfs-visit.h>

#include <unordered_set>

DECLARE_bool(fst_verify_properties);

namespace fst {
namespace internal {

// Computes FST property values defined in properties.h. The value of each
// property indicated in the mask will be determined and returned (these will
// never be unknown here). In the course of determining the properties
// specifically requested in the mask, certain other properties may be
// determined (those with little additional expense) and their values will be
// returned as well. The complete set of known properties (whether true or
// false) determined by this operation will be assigned to the the value pointed
// to by KNOWN. 'mask & required_mask' is used to determine whether the stored
// properties can be used. This routine is seldom called directly; instead it is
// used to implement fst.Properties(mask, /*test=*/true).
template <class Arc>
uint64_t ComputeProperties(const Fst<Arc> &fst, uint64_t mask,
                           uint64_t *known) {
  using Label = typename Arc::Label;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;
  const auto fst_props = fst.Properties(kFstProperties, false);  // FST-stored.
  // Computes (trinary) properties explicitly.
  // Initialize with binary properties (already known).
  uint64_t comp_props = fst_props & kBinaryProperties;
  // Computes these trinary properties with a DFS. We compute only those that
  // need a DFS here, since we otherwise would like to avoid a DFS since its
  // stack could grow large.
  constexpr uint64_t kDfsProps =
      kCyclic | kAcyclic | kInitialCyclic | kInitialAcyclic | kAccessible |
      kNotAccessible | kCoAccessible | kNotCoAccessible;
  std::vector<StateId> scc;
  if (mask & (kDfsProps | kWeightedCycles | kUnweightedCycles)) {
    SccVisitor<Arc> scc_visitor(&scc, nullptr, nullptr, &comp_props);
    DfsVisit(fst, &scc_visitor);
  }
  // Computes any remaining trinary properties via a state and arcs iterations
  if (mask & ~(kBinaryProperties | kDfsProps)) {
    comp_props |= kAcceptor | kNoEpsilons | kNoIEpsilons | kNoOEpsilons |
                  kILabelSorted | kOLabelSorted | kUnweighted | kTopSorted |
                  kString;
    if (mask & (kIDeterministic | kNonIDeterministic)) {
      comp_props |= kIDeterministic;
    }
    if (mask & (kODeterministic | kNonODeterministic)) {
      comp_props |= kODeterministic;
    }
    if (mask & (kDfsProps | kWeightedCycles | kUnweightedCycles)) {
      comp_props |= kUnweightedCycles;
    }
    std::unique_ptr<std::unordered_set<Label>> ilabels;
    std::unique_ptr<std::unordered_set<Label>> olabels;
    StateId nfinal = 0;
    for (StateIterator<Fst<Arc>> siter(fst); !siter.Done(); siter.Next()) {
      StateId s = siter.Value();
      Arc prev_arc;
      // Creates these only if we need to.
      if (mask & (kIDeterministic | kNonIDeterministic)) {
        ilabels = std::make_unique<std::unordered_set<Label>>();
      }
      if (mask & (kODeterministic | kNonODeterministic)) {
        olabels = std::make_unique<std::unordered_set<Label>>();
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

// Similar to ComputeProperties, but uses the properties already stored
// in the FST when possible.
template <class Arc>
uint64_t ComputeOrUseStoredProperties(const Fst<Arc> &fst, uint64_t mask,
                                      uint64_t *known) {
  // Check stored FST properties first.
  const auto fst_props = fst.Properties(kFstProperties, /*test=*/false);
  const auto known_props = KnownProperties(fst_props);
  // If FST contains required info, return it.
  if ((known_props & mask) == mask) {
    if (known) *known = known_props;
    return fst_props;
  }

  return ComputeProperties(fst, mask, known);
}

// This is a wrapper around ComputeProperties that will cause a fatal error if
// the stored properties and the computed properties are incompatible when
// FST_FLAGS_fst_verify_properties is true. This routine is seldom called directly;
// instead it is used to implement fst.Properties(mask, /*test=*/true).
template <class Arc>
uint64_t TestProperties(const Fst<Arc> &fst, uint64_t mask, uint64_t *known) {
  if (FST_FLAGS_fst_verify_properties) {
    const auto stored_props = fst.Properties(kFstProperties, false);
    const auto computed_props = ComputeProperties(fst, mask, known);
    if (!CompatProperties(stored_props, computed_props)) {
      FSTERROR() << "TestProperties: stored FST properties incorrect"
                 << " (stored: props1, computed: props2)";
    }
    return computed_props;
  } else {
    return ComputeOrUseStoredProperties(fst, mask, known);
  }
}

// If all the properties of 'fst' corresponding to 'check_mask' are known,
// returns the stored properties. Otherwise, the properties corresponding to
// both 'check_mask' and 'test_mask' are computed. This is used to check for
// newly-added properties that might not be set in old binary files.
template <class Arc>
uint64_t CheckProperties(const Fst<Arc> &fst, uint64_t check_mask,
                         uint64_t test_mask) {
  auto props = fst.Properties(kFstProperties, false);
  if (FST_FLAGS_fst_verify_properties) {
    props = TestProperties(fst, check_mask | test_mask, /*known=*/nullptr);
  } else if ((KnownProperties(props) & check_mask) != check_mask) {
    props = ComputeProperties(fst, check_mask | test_mask, /*known=*/nullptr);
  }
  return props & (check_mask | test_mask);
}

}  // namespace internal
}  // namespace fst

#endif  // FST_TEST_PROPERTIES_H_
