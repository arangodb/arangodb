// rmfinalepsilon.h

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
// Author: johans@google.com (Johan Schalkwyk)
//
// \file
// Function to remove of final states that have epsilon only input arcs.

#ifndef FST_LIB_RMFINALEPSILON_H__
#define FST_LIB_RMFINALEPSILON_H__

#include <unordered_set>
using std::unordered_set;
using std::unordered_multiset;
#include <vector>
using std::vector;

#include <fst/connect.h>
#include <fst/mutable-fst.h>


namespace fst {

template<class A>
void RmFinalEpsilon(MutableFst<A>* fst) {
  typedef typename A::StateId StateId;
  typedef typename A::Weight Weight;

  // Determine the coaccesibility of states.
  vector<bool> access;
  vector<bool> coaccess;
  uint64 props = 0;
  SccVisitor<A> scc_visitor(0, &access, &coaccess, &props);
  DfsVisit(*fst, &scc_visitor);

  // Find potential list of removable final states. These are final states
  // that have no outgoing transitions or final states that have a
  // non-coaccessible future. Complexity O(S)
  unordered_set<StateId> finals;
  for (StateIterator<Fst<A> > siter(*fst); !siter.Done(); siter.Next()) {
    StateId s = siter.Value();
    if (fst->Final(s) != Weight::Zero()) {
      bool future_coaccess = false;
      for (ArcIterator<Fst<A> > aiter(*fst, s); !aiter.Done(); aiter.Next()) {
        const A& arc = aiter.Value();
        if (coaccess[arc.nextstate]) {
          future_coaccess = true;
          break;
        }
      }
      if (!future_coaccess) {
        finals.insert(s);
      }
    }
  }

  // Move the final weight. Complexity O(E)
  vector<A> arcs;
  for (StateIterator<Fst<A> > siter(*fst); !siter.Done(); siter.Next()) {
    StateId s = siter.Value();
    Weight w(fst->Final(s));

    arcs.clear();
    for (ArcIterator<Fst<A> > aiter(*fst, s); !aiter.Done(); aiter.Next()) {
      const A& arc = aiter.Value();
      // is next state in the list of finals
      if (finals.find(arc.nextstate) != finals.end()) {
        // sum up all epsilon arcs
        if (arc.ilabel == 0 && arc.olabel == 0) {
          w = Plus(Times(fst->Final(arc.nextstate), arc.weight), w);
        } else {
          arcs.push_back(arc);
        }
      } else {
        arcs.push_back(arc);
      }
    }

    // If some arcs (epsilon arcs) were deleted, delete all
    // arcs and add back only the non epsilon arcs
    if (arcs.size() < fst->NumArcs(s)) {
      fst->DeleteArcs(s);
      fst->SetFinal(s, w);
      for (size_t i = 0; i < arcs.size(); ++i) {
        fst->AddArc(s, arcs[i]);
      }
    }
  }

  Connect(fst);
}

}  // namespace fst

#endif  // FST_LIB_RMFINALEPSILON_H__
