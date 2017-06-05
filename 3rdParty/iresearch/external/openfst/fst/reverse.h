// reverse.h

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
// Functions and classes to sort arcs in an FST.

#ifndef FST_LIB_REVERSE_H__
#define FST_LIB_REVERSE_H__

#include <algorithm>
#include <vector>
using std::vector;

#include <fst/cache.h>


namespace fst {

// Reverses an FST. The reversed result is written to an output
// MutableFst.  If A transduces string x to y with weight a, then the
// reverse of A transduces the reverse of x to the reverse of y with
// weight a.Reverse().
//
// Typically, a = a.Reverse() and Arc = RevArc (e.g. for
// TropicalWeight or LogWeight).  In general, e.g. when the weights
// only form a left or right semiring, the output arc type must match
// the input arc type except having the reversed Weight type.
//
// When 'require_superinitial == false', a superinitial state is
// *not* created in the reversed Fst iff the input Fst has exactly
// 1 final state (which becomes the initial state of the reversed Fst)
// that has a final weight of Weight::One() *or* does not belong to
// any cycle.
// When 'require_superinitial == true', a superinitial state is
// always created.
template<class Arc, class RevArc>
void Reverse(const Fst<Arc> &ifst, MutableFst<RevArc> *ofst,
             bool require_superinitial = true) {
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Weight Weight;
  typedef typename RevArc::Weight RevWeight;

  ofst->DeleteStates();
  ofst->SetInputSymbols(ifst.InputSymbols());
  ofst->SetOutputSymbols(ifst.OutputSymbols());
  if (ifst.Properties(kExpanded, false))
    ofst->ReserveStates(CountStates(ifst) + 1);
  StateId istart = ifst.Start();
  StateId ostart = kNoStateId;
  StateId offset = 0;
  uint64 dfs_iprops = 0, dfs_oprops = 0;

  if (!require_superinitial) {
    for (StateIterator<Fst<Arc> > siter(ifst);
         !siter.Done();
         siter.Next()) {
      StateId is = siter.Value();
      if (ifst.Final(is) == Weight::Zero()) continue;
      if (ostart != kNoStateId) {
        ostart = kNoStateId;
        break;
      } else {
        ostart = is;
      }
    }

    if (ostart != kNoStateId && ifst.Final(ostart) != Weight::One()) {
      vector<StateId> scc;
      SccVisitor<Arc> scc_visitor(&scc, 0, 0, &dfs_iprops);
      DfsVisit(ifst, &scc_visitor);
      if (count(scc.begin(), scc.end(), scc[ostart]) > 1) {
        ostart = kNoStateId;
      } else {
        for (ArcIterator<Fst<Arc> > aiter(ifst, ostart);
             !aiter.Done(); aiter.Next()) {
          if (aiter.Value().nextstate == ostart) {
            ostart = kNoStateId;
            break;
          }
        }
      }
      if (ostart != kNoStateId)
        dfs_oprops = kInitialAcyclic;
    }
  }

  if (ostart == kNoStateId) {  // Super-initial requested or needed.
    ostart = ofst->AddState();
    offset = 1;
  }

  for (StateIterator<Fst<Arc> > siter(ifst);
       !siter.Done();
       siter.Next()) {
    StateId is = siter.Value();
    StateId os = is + offset;
    while (ofst->NumStates() <= os)
      ofst->AddState();
    if (is == istart)
      ofst->SetFinal(os, RevWeight::One());

    Weight final = ifst.Final(is);
    if ((final != Weight::Zero()) && (offset == 1)) {
      RevArc oarc(0, 0, final.Reverse(), os);
      ofst->AddArc(0, oarc);
    }

    for (ArcIterator<Fst<Arc> > aiter(ifst, is);
         !aiter.Done();
         aiter.Next()) {
      const Arc &iarc = aiter.Value();
      StateId nos = iarc.nextstate + offset;
      typename RevArc::Weight weight = iarc.weight.Reverse();
      if (!offset && (nos == ostart))
        weight = Times(ifst.Final(ostart).Reverse(), weight);
      RevArc oarc(iarc.ilabel, iarc.olabel, weight, os);
      while (ofst->NumStates() <= nos)
        ofst->AddState();
      ofst->AddArc(nos, oarc);
    }
  }
  ofst->SetStart(ostart);
  if (offset == 0 && ostart == istart)
    ofst->SetFinal(ostart, ifst.Final(ostart).Reverse());

  uint64 iprops = ifst.Properties(kCopyProperties, false) | dfs_iprops;
  uint64 oprops = ofst->Properties(kFstProperties, false) | dfs_oprops;
  ofst->SetProperties(ReverseProperties(iprops, offset == 1) | oprops,
                      kFstProperties);
}

}  // namespace fst

#endif  // FST_LIB_REVERSE_H__
