// statesort.h

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
// Function to sort states of an Fst.

#ifndef FST_LIB_STATESORT_H__
#define FST_LIB_STATESORT_H__

#include <vector>
using std::vector;
#include <algorithm>

#include <fst/mutable-fst.h>


namespace fst {

// Sorts the input states of an FST, modifying it. ORDER[i] gives the
// the state Id after sorting that corresponds to state Id i before
// sorting.  ORDER must be a permutation of FST's states ID sequence:
// (0, 1, 2, ..., fst->NumStates() - 1).
template <class Arc>
void StateSort(MutableFst<Arc> *fst,
               const vector<typename Arc::StateId> &order) {
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Weight Weight;

  if (order.size() != fst->NumStates()) {
    FSTERROR() << "StateSort: bad order vector size: " << order.size();
    fst->SetProperties(kError, kError);
    return;
  }

  if (fst->Start() == kNoStateId)
    return;

  uint64 props = fst->Properties(kStateSortProperties, false);

  vector<bool> done(order.size(), false);
  vector<Arc> arcsa, arcsb;
  vector<Arc> *arcs1 = &arcsa, *arcs2 = &arcsb;

  fst->SetStart(order[fst->Start()]);

  for (StateIterator< MutableFst<Arc> > siter(*fst);
       !siter.Done();
       siter.Next()) {
    StateId s1 = siter.Value(), s2;
    if (done[s1])
      continue;
    Weight final1 = fst->Final(s1), final2 = Weight::Zero();
    arcs1->clear();
    for (ArcIterator< MutableFst<Arc> > aiter(*fst, s1);
         !aiter.Done();
         aiter.Next())
      arcs1->push_back(aiter.Value());
    for (; !done[s1]; s1 = s2, final1 = final2, swap(arcs1, arcs2)) {
      s2 = order[s1];
      if (!done[s2]) {
        final2 = fst->Final(s2);
        arcs2->clear();
        for (ArcIterator< MutableFst<Arc> > aiter(*fst, s2);
             !aiter.Done();
             aiter.Next())
          arcs2->push_back(aiter.Value());
      }
      fst->SetFinal(s2, final1);
      fst->DeleteArcs(s2);
      for (size_t i = 0; i < arcs1->size(); ++i) {
        Arc arc = (*arcs1)[i];
        arc.nextstate = order[arc.nextstate];
        fst->AddArc(s2, arc);
      }
      done[s1] = true;
    }
  }
  fst->SetProperties(props, kFstProperties);
}

}  // namespace fst

#endif  // FST_LIB_STATESORT_H__
