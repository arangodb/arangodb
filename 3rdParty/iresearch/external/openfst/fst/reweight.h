// reweight.h

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
// Author: allauzen@google.com (Cyril Allauzen)
//
// \file
// Function to reweight an FST.

#ifndef FST_LIB_REWEIGHT_H__
#define FST_LIB_REWEIGHT_H__

#include <vector>
using std::vector;

#include <fst/mutable-fst.h>


namespace fst {

enum ReweightType { REWEIGHT_TO_INITIAL, REWEIGHT_TO_FINAL };

// Reweight FST according to the potentials defined by the POTENTIAL
// vector in the direction defined by TYPE. Weight needs to be left
// distributive when reweighting towards the initial state and right
// distributive when reweighting towards the final states.
//
// An arc of weight w, with an origin state of potential p and
// destination state of potential q, is reweighted by p\wq when
// reweighting towards the initial state and by pw/q when reweighting
// towards the final states.
template <class Arc>
void Reweight(MutableFst<Arc> *fst,
              const vector<typename Arc::Weight> &potential,
              ReweightType type) {
  typedef typename Arc::Weight Weight;

  if (fst->NumStates() == 0)
    return;

  if (type == REWEIGHT_TO_FINAL && !(Weight::Properties() & kRightSemiring)) {
    FSTERROR() << "Reweight: Reweighting to the final states requires "
               << "Weight to be right distributive: "
               << Weight::Type();
    fst->SetProperties(kError, kError);
    return;
  }

  if (type == REWEIGHT_TO_INITIAL && !(Weight::Properties() & kLeftSemiring)) {
    FSTERROR() << "Reweight: Reweighting to the initial state requires "
               << "Weight to be left distributive: "
               << Weight::Type();
    fst->SetProperties(kError, kError);
    return;
  }

  StateIterator< MutableFst<Arc> > sit(*fst);
  for (; !sit.Done(); sit.Next()) {
    typename Arc::StateId state = sit.Value();
    if (state == potential.size())
      break;
    typename Arc::Weight weight = potential[state];
    if (weight != Weight::Zero()) {
      for (MutableArcIterator< MutableFst<Arc> > ait(fst, state);
           !ait.Done();
           ait.Next()) {
        Arc arc = ait.Value();
        if (arc.nextstate >= potential.size())
          continue;
        typename Arc::Weight nextweight = potential[arc.nextstate];
        if (nextweight == Weight::Zero())
          continue;
        if (type == REWEIGHT_TO_INITIAL)
          arc.weight = Divide(Times(arc.weight, nextweight), weight,
                              DIVIDE_LEFT);
        if (type == REWEIGHT_TO_FINAL)
          arc.weight = Divide(Times(weight, arc.weight), nextweight,
                              DIVIDE_RIGHT);
        ait.SetValue(arc);
      }
      if (type == REWEIGHT_TO_INITIAL)
        fst->SetFinal(state, Divide(fst->Final(state), weight, DIVIDE_LEFT));
    }
    if (type == REWEIGHT_TO_FINAL)
      fst->SetFinal(state, Times(weight, fst->Final(state)));
  }

  // This handles elements past the end of the potentials array.
  for (; !sit.Done(); sit.Next()) {
    typename Arc::StateId state = sit.Value();
    if (type == REWEIGHT_TO_FINAL)
      fst->SetFinal(state, Times(Weight::Zero(), fst->Final(state)));
  }

  typename Arc::Weight startweight = fst->Start() < potential.size() ?
      potential[fst->Start()] : Weight::Zero();
  if ((startweight != Weight::One()) && (startweight != Weight::Zero())) {
    if (fst->Properties(kInitialAcyclic, true) & kInitialAcyclic) {
      typename Arc::StateId state = fst->Start();
      for (MutableArcIterator< MutableFst<Arc> > ait(fst, state);
           !ait.Done();
           ait.Next()) {
        Arc arc = ait.Value();
        if (type == REWEIGHT_TO_INITIAL)
          arc.weight = Times(startweight, arc.weight);
        else
          arc.weight = Times(
              Divide(Weight::One(), startweight, DIVIDE_RIGHT),
              arc.weight);
        ait.SetValue(arc);
      }
      if (type == REWEIGHT_TO_INITIAL)
        fst->SetFinal(state, Times(startweight, fst->Final(state)));
      else
        fst->SetFinal(state, Times(Divide(Weight::One(), startweight,
                                          DIVIDE_RIGHT),
                                   fst->Final(state)));
    } else {
      typename Arc::StateId state = fst->AddState();
      Weight w = type == REWEIGHT_TO_INITIAL ?  startweight :
                 Divide(Weight::One(), startweight, DIVIDE_RIGHT);
      Arc arc(0, 0, w, fst->Start());
      fst->AddArc(state, arc);
      fst->SetStart(state);
    }
  }

  fst->SetProperties(ReweightProperties(
                         fst->Properties(kFstProperties, false)),
                     kFstProperties);
}

}  // namespace fst

#endif  // FST_LIB_REWEIGHT_H_
