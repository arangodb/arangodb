// test-properties.h

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
// Functions to manipulate and test property bits

#ifndef FST_LIB_TEST_PROPERTIES_H__
#define FST_LIB_TEST_PROPERTIES_H__

#include <unordered_set>
using std::unordered_set;
using std::unordered_multiset;

#include <fst/dfs-visit.h>
#include <fst/connect.h>


DECLARE_bool(fst_verify_properties);

namespace fst {

// For a binary property, the bit is always returned set.
// For a trinary (i.e. two-bit) property, both bits are
// returned set iff either corresponding input bit is set.
inline uint64 KnownProperties(uint64 props) {
  return kBinaryProperties | (props & kTrinaryProperties) |
    ((props & kPosTrinaryProperties) << 1) |
    ((props & kNegTrinaryProperties) >> 1);
}

// Tests compatibility between two sets of properties
inline bool CompatProperties(uint64 props1, uint64 props2) {
  uint64 known_props1 = KnownProperties(props1);
  uint64 known_props2 = KnownProperties(props2);
  uint64 known_props = known_props1 & known_props2;
  uint64 incompat_props = (props1 & known_props) ^ (props2 & known_props);
  if (incompat_props) {
    uint64 prop = 1;
    for (int i = 0; i < 64; ++i, prop <<= 1)
      if (prop & incompat_props)
        LOG(ERROR) << "CompatProperties: mismatch: " << PropertyNames[i]
                   << ": props1 = " << (props1 & prop ? "true" : "false")
                   << ", props2 = " << (props2 & prop ? "true" : "false");
    return false;
  } else {
    return true;
  }
}

// Computes FST property values defined in properties.h.  The value of
// each property indicated in the mask will be determined and returned
// (these will never be unknown here). In the course of determining
// the properties specifically requested in the mask, certain other
// properties may be determined (those with little additional expense)
// and their values will be returned as well. The complete set of
// known properties (whether true or false) determined by this
// operation will be assigned to the the value pointed to by KNOWN.
// If 'use_stored' is true, pre-computed FST properties may be used
// when possible. This routine is seldom called directly; instead it
// is used to implement fst.Properties(mask, true).
template<class Arc>
uint64 ComputeProperties(const Fst<Arc> &fst, uint64 mask, uint64 *known,
                         bool use_stored) {
  typedef typename Arc::Label Label;
  typedef typename Arc::Weight Weight;
  typedef typename Arc::StateId StateId;

  uint64 fst_props = fst.Properties(kFstProperties, false);  // Fst-stored

  // Check stored FST properties first if allowed.
  if (use_stored) {
    uint64 known_props = KnownProperties(fst_props);
    // If FST contains required info, return it.
    if ((known_props & mask) == mask) {
      *known = known_props;
      return fst_props;
    }
  }

  // Compute (trinary) properties explicitly.

  // Initialize with binary properties (already known).
  uint64 comp_props = fst_props & kBinaryProperties;

  // Compute these trinary properties with a DFS. We compute only those
  // that need a DFS here, since we otherwise would like to avoid a DFS
  // since its stack could grow large.
  uint64 dfs_props = kCyclic | kAcyclic | kInitialCyclic | kInitialAcyclic |
                     kAccessible | kNotAccessible |
                     kCoAccessible | kNotCoAccessible;
  if (mask & dfs_props) {
    SccVisitor<Arc> scc_visitor(&comp_props);
    DfsVisit(fst, &scc_visitor);
  }

  // Compute any remaining trinary properties via a state and arcs iterations
  if (mask & ~(kBinaryProperties | dfs_props)) {
    comp_props |= kAcceptor | kNoEpsilons | kNoIEpsilons | kNoOEpsilons |
        kILabelSorted | kOLabelSorted | kUnweighted | kTopSorted | kString;
    if (mask & (kIDeterministic | kNonIDeterministic))
      comp_props |= kIDeterministic;
    if (mask & (kODeterministic | kNonODeterministic))
      comp_props |= kODeterministic;

    unordered_set<Label> *ilabels = 0;
    unordered_set<Label> *olabels = 0;

    StateId nfinal = 0;
    for (StateIterator< Fst<Arc> > siter(fst);
         !siter.Done();
         siter.Next()) {
      StateId s = siter.Value();

      Arc prev_arc;
      // Create these only if we need to
      if (mask & (kIDeterministic | kNonIDeterministic))
        ilabels = new unordered_set<Label>;
      if (mask & (kODeterministic | kNonODeterministic))
        olabels = new unordered_set<Label>;

      bool first_arc = true;
      for (ArcIterator< Fst<Arc> > aiter(fst, s);
           !aiter.Done();
           aiter.Next()) {
        const Arc &arc =aiter.Value();

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
        if (ilabels)
          ilabels->insert(arc.ilabel);
        if (olabels)
          olabels->insert(arc.olabel);
      }

      if (nfinal > 0) {             // final state not last
        comp_props |= kNotString;
        comp_props &= ~kString;
      }

      Weight final = fst.Final(s);

      if (final != Weight::Zero()) {  // final state
        if (final != Weight::One()) {
          comp_props |= kWeighted;
          comp_props &= ~kUnweighted;
        }
        ++nfinal;
      } else {                        // non-final state
        if (fst.NumArcs(s) != 1) {
          comp_props |= kNotString;
          comp_props &= ~kString;
        }
      }

      delete ilabels;
      delete olabels;
    }

    if (fst.Start() != kNoStateId && fst.Start() != 0) {
      comp_props |= kNotString;
      comp_props &= ~kString;
    }
  }

  *known = KnownProperties(comp_props);
  return comp_props;
}

// This is a wrapper around ComputeProperties that will cause a fatal
// error if the stored properties and the computed properties are
// incompatible when 'FLAGS_fst_verify_properties' is true.  This
// routine is seldom called directly; instead it is used to implement
// fst.Properties(mask, true).
template<class Arc>
uint64 TestProperties(const Fst<Arc> &fst, uint64 mask, uint64 *known) {
  if (FLAGS_fst_verify_properties) {
    uint64 stored_props = fst.Properties(kFstProperties, false);
    uint64 computed_props = ComputeProperties(fst, mask, known, false);
    if (!CompatProperties(stored_props, computed_props))
      LOG(FATAL) << "TestProperties: stored Fst properties incorrect"
                 << " (stored: props1, computed: props2)";
    return computed_props;
  } else {
    return ComputeProperties(fst, mask, known, true);
  }
}

}  // namespace fst

#endif  // FST_LIB_TEST_PROPERTIES_H__
