// equivalent.h

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
// Author: wojciech@google.com (Wojciech Skut)
//
// \file Functions and classes to determine the equivalence of two
// FSTs.

#ifndef FST_LIB_EQUIVALENT_H__
#define FST_LIB_EQUIVALENT_H__

#include <algorithm>
#include <deque>
using std::deque;
#include <unordered_map>
using std::unordered_map;
using std::unordered_multimap;
#include <utility>
using std::pair; using std::make_pair;
#include <vector>
using std::vector;

#include <fst/encode.h>
#include <fst/push.h>
#include <fst/union-find.h>
#include <fst/vector-fst.h>


namespace fst {

// Traits-like struct holding utility functions/typedefs/constants for
// the equivalence algorithm.
//
// Encoding device: in order to make the statesets of the two acceptors
// disjoint, we map Arc::StateId on the type MappedId. The states of
// the first acceptor are mapped on odd numbers (s -> 2s + 1), and
// those of the second one on even numbers (s -> 2s + 2). The number 0
// is reserved for an implicit (non-final) 'dead state' (required for
// the correct treatment of non-coaccessible states; kNoStateId is
// mapped to kDeadState for both acceptors). The union-find algorithm
// operates on the mapped IDs.
template <class Arc>
struct EquivalenceUtil {
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Weight Weight;
  typedef StateId MappedId;  // ID for an equivalence class.

  // MappedId for an implicit dead state.
  static const MappedId kDeadState = 0;

  // MappedId for lookup failure.
  static const MappedId kInvalidId = -1;

  // Maps state ID to the representative of the corresponding
  // equivalence class. The parameter 'which_fst' takes the values 1
  // and 2, identifying the input FST.
  static MappedId MapState(StateId s, int32 which_fst) {
    return
      (kNoStateId == s)
      ?
      kDeadState
      :
      (static_cast<MappedId>(s) << 1) + which_fst;
  }
  // Maps set ID to State ID.
  static StateId UnMapState(MappedId id) {
    return static_cast<StateId>((--id) >> 1);
  }
  // Convenience function: checks if state with MappedId 's' is final
  // in acceptor 'fa'.
  static bool IsFinal(const Fst<Arc> &fa, MappedId s) {
    return
      (kDeadState == s) ?
      false : (fa.Final(UnMapState(s)) != Weight::Zero());
  }
  // Convenience function: returns the representative of 'id' in 'sets',
  // creating a new set if needed.
  static MappedId FindSet(UnionFind<MappedId> *sets, MappedId id) {
    MappedId repr = sets->FindSet(id);
    if (repr != kInvalidId) {
      return repr;
    } else {
      sets->MakeSet(id);
      return id;
    }
  }
};

template <class Arc> const
typename EquivalenceUtil<Arc>::MappedId EquivalenceUtil<Arc>::kDeadState;

template <class Arc> const
typename EquivalenceUtil<Arc>::MappedId EquivalenceUtil<Arc>::kInvalidId;


// Equivalence checking algorithm: determines if the two FSTs
// <code>fst1</code> and <code>fst2</code> are equivalent. The input
// FSTs must be deterministic input-side epsilon-free acceptors,
// unweighted or with weights over a left semiring. Two acceptors are
// considered equivalent if they accept exactly the same set of
// strings (with the same weights).
//
// The algorithm (cf. Aho, Hopcroft and Ullman, "The Design and
// Analysis of Computer Programs") successively constructs sets of
// states that can be reached by the same prefixes, starting with a
// set containing the start states of both acceptors. A disjoint tree
// forest (the union-find algorithm) is used to represent the sets of
// states. The algorithm returns 'false' if one of the constructed
// sets contains both final and non-final states. Returns optional error
// value (useful when FLAGS_error_fatal = false).
//
// Complexity: quasi-linear, i.e. O(n G(n)), where
//   n = |S1| + |S2| is the number of states in both acceptors
//   G(n) is a very slowly growing function that can be approximated
//        by 4 by all practical purposes.
//
template <class Arc>
bool Equivalent(const Fst<Arc> &fst1,
                const Fst<Arc> &fst2,
                double delta = kDelta, bool *error = 0) {
  typedef typename Arc::Weight Weight;
  if (error) *error = false;

  // Check that the symbol table are compatible
  if (!CompatSymbols(fst1.InputSymbols(), fst2.InputSymbols()) ||
      !CompatSymbols(fst1.OutputSymbols(), fst2.OutputSymbols())) {
    FSTERROR() << "Equivalent: input/output symbol tables of 1st argument "
               << "do not match input/output symbol tables of 2nd argument";
    if (error) *error = true;
    return false;
  }
  // Check properties first:
  uint64 props = kNoEpsilons | kIDeterministic | kAcceptor;
  if (fst1.Properties(props, true) != props) {
    FSTERROR() << "Equivalent: first argument not an"
               << " epsilon-free deterministic acceptor";
    if (error) *error = true;
    return false;
  }
  if (fst2.Properties(props, true) != props) {
    FSTERROR() << "Equivalent: second argument not an"
               << " epsilon-free deterministic acceptor";
    if (error) *error = true;
    return false;
  }

  if ((fst1.Properties(kUnweighted , true) != kUnweighted)
      || (fst2.Properties(kUnweighted , true) != kUnweighted)) {
    VectorFst<Arc> efst1(fst1);
    VectorFst<Arc> efst2(fst2);
    Push(&efst1, REWEIGHT_TO_INITIAL, delta);
    Push(&efst2, REWEIGHT_TO_INITIAL, delta);
    ArcMap(&efst1, QuantizeMapper<Arc>(delta));
    ArcMap(&efst2, QuantizeMapper<Arc>(delta));
    EncodeMapper<Arc> mapper(kEncodeWeights|kEncodeLabels, ENCODE);
    ArcMap(&efst1, &mapper);
    ArcMap(&efst2, &mapper);
    return Equivalent(efst1, efst2);
  }

  // Convenience typedefs:
  typedef typename Arc::StateId StateId;
  typedef EquivalenceUtil<Arc> Util;
  typedef typename Util::MappedId MappedId;
  enum { FST1 = 1, FST2 = 2 };  // Required by Util::MapState(...)

  MappedId s1 = Util::MapState(fst1.Start(), FST1);
  MappedId s2 = Util::MapState(fst2.Start(), FST2);

  // The union-find structure.
  UnionFind<MappedId> eq_classes(1000, Util::kInvalidId);

  // Initialize the union-find structure.
  eq_classes.MakeSet(s1);
  eq_classes.MakeSet(s2);

  // Data structure for the (partial) acceptor transition function of
  // fst1 and fst2: input labels mapped to pairs of MappedId's
  // representing destination states of the corresponding arcs in fst1
  // and fst2, respectively.
  typedef
    unordered_map<typename Arc::Label, pair<MappedId, MappedId> >
    Label2StatePairMap;

  Label2StatePairMap arc_pairs;

  // Pairs of MappedId's to be processed, organized in a queue.
  deque<pair<MappedId, MappedId> > q;

  bool ret = true;
  // Early return if the start states differ w.r.t. being final.
  if (Util::IsFinal(fst1, s1) != Util::IsFinal(fst2, s2)) {
    ret = false;
  }

  // Main loop: explores the two acceptors in a breadth-first manner,
  // updating the equivalence relation on the statesets. Loop
  // invariant: each block of states contains either final states only
  // or non-final states only.
  for (q.push_back(std::make_pair(s1, s2)); ret && !q.empty(); q.pop_front()) {
    s1 = q.front().first;
    s2 = q.front().second;

    // Representatives of the equivalence classes of s1/s2.
    MappedId rep1 = Util::FindSet(&eq_classes, s1);
    MappedId rep2 = Util::FindSet(&eq_classes, s2);

    if (rep1 != rep2) {
      eq_classes.Union(rep1, rep2);
      arc_pairs.clear();

      // Copy outgoing arcs starting at s1 into the hashtable.
      if (Util::kDeadState != s1) {
        ArcIterator<Fst<Arc> > arc_iter(fst1, Util::UnMapState(s1));
        for (; !arc_iter.Done(); arc_iter.Next()) {
          const Arc &arc = arc_iter.Value();
          if (arc.weight != Weight::Zero()) {  // Zero-weight arcs
                                                   // are treated as
                                                   // non-exisitent.
            arc_pairs[arc.ilabel].first = Util::MapState(arc.nextstate, FST1);
          }
        }
      }
      // Copy outgoing arcs starting at s2 into the hashtable.
      if (Util::kDeadState != s2) {
        ArcIterator<Fst<Arc> > arc_iter(fst2, Util::UnMapState(s2));
        for (; !arc_iter.Done(); arc_iter.Next()) {
          const Arc &arc = arc_iter.Value();
          if (arc.weight != Weight::Zero()) {  // Zero-weight arcs
                                                   // are treated as
                                                   // non-existent.
            arc_pairs[arc.ilabel].second = Util::MapState(arc.nextstate, FST2);
          }
        }
      }
      // Iterate through the hashtable and process pairs of target
      // states.
      for (typename Label2StatePairMap::const_iterator
             arc_iter = arc_pairs.begin();
           arc_iter != arc_pairs.end();
           ++arc_iter) {
        const pair<MappedId, MappedId> &p = arc_iter->second;
        if (Util::IsFinal(fst1, p.first) != Util::IsFinal(fst2, p.second)) {
          // Detected inconsistency: return false.
          ret = false;
          break;
        }
        q.push_back(p);
      }
    }
  }

  if (fst1.Properties(kError, false) || fst2.Properties(kError, false)) {
    if (error) *error = true;
    return false;
  }

  return ret;
}

}  // namespace fst

#endif  // FST_LIB_EQUIVALENT_H__
