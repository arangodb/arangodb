// push.h

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
// Class to reweight/push an FST.
// And utility functions to weigh and reweight an FST.

#ifndef FST_LIB_PUSH_H__
#define FST_LIB_PUSH_H__

#include <vector>
using std::vector;

#include <fst/factor-weight.h>
#include <fst/fst.h>
#include <fst/arc-map.h>
#include <fst/reweight.h>
#include <fst/shortest-distance.h>


namespace fst {

// Compute the total weight (sum of the weights of all accepting paths) from
// the output of ShortestDistance. 'distance' is the shortest distance from the
// initial state when 'reverse == false' and to the final states when
// 'reverse == true'.
template <class Arc>
typename Arc::Weight ComputeTotalWeight(
    const Fst<Arc> &fst,
    const vector<typename Arc::Weight> &distance,
    bool reverse) {
  if (reverse)
    return fst.Start() < distance.size() ?
        distance[fst.Start()] : Arc::Weight::Zero();

  typename Arc::Weight sum = Arc::Weight::Zero();
  for (typename Arc::StateId s = 0; s < distance.size(); ++s)
    sum = Plus(sum, Times(distance[s], fst.Final(s)));
  return sum;
}

// Divide the weight of every accepting path by 'w'. The weight 'w' is
// divided at the final states if 'at_final == true' and at the
// initial state otherwise.
template <class Arc>
void RemoveWeight(MutableFst<Arc> *fst, typename Arc::Weight w, bool at_final) {
  if ((w == Arc::Weight::One()) || (w == Arc::Weight::Zero()))
      return;

  if (at_final) {
    // Remove 'w' from the final states
    for (StateIterator< MutableFst<Arc> > sit(*fst);
         !sit.Done();
         sit.Next())
      fst->SetFinal(sit.Value(),
                    Divide(fst->Final(sit.Value()), w,  DIVIDE_RIGHT));
  } else {  // at_final == false
    // Remove 'w' from the initial state
    typename Arc::StateId start = fst->Start();
    for (MutableArcIterator<MutableFst<Arc> > ait(fst, start);
         !ait.Done();
         ait.Next()) {
      Arc arc = ait.Value();
      arc.weight = Divide(arc.weight, w, DIVIDE_LEFT);
      ait.SetValue(arc);
    }
    fst->SetFinal(start, Divide(fst->Final(start), w, DIVIDE_LEFT));
  }
}

// Pushes the weights in FST in the direction defined by TYPE.  If
// pushing towards the initial state, the sum of the weight of the
// outgoing transitions and final weight at a non-initial state is
// equal to One() in the resulting machine.  If pushing towards the
// final state, the same property holds on the reverse machine.
//
// Weight needs to be left distributive when pushing towards the
// initial state and right distributive when pushing towards the final
// states.
template <class Arc>
void Push(MutableFst<Arc> *fst,
          ReweightType type,
          float delta = kDelta,
          bool remove_total_weight = false) {
  vector<typename Arc::Weight> distance;
  ShortestDistance(*fst, &distance, type == REWEIGHT_TO_INITIAL, delta);
  typename Arc::Weight total_weight = Arc::Weight::One();
  if (remove_total_weight)
    total_weight = ComputeTotalWeight(*fst, distance,
                                      type == REWEIGHT_TO_INITIAL);
  Reweight(fst, distance, type);
  if (remove_total_weight)
    RemoveWeight(fst, total_weight, type == REWEIGHT_TO_FINAL);
}

const uint32 kPushWeights = 0x0001;
const uint32 kPushLabels =  0x0002;
const uint32 kPushRemoveTotalWeight = 0x0004;
const uint32 kPushRemoveCommonAffix = 0x0008;

// OFST obtained from IFST by pushing weights and/or labels according
// to PTYPE in the direction defined by RTYPE.  Weight needs to be
// left distributive when pushing weights towards the initial state
// and right distributive when pushing weights towards the final
// states.
template <class Arc, ReweightType rtype>
void Push(const Fst<Arc> &ifst,
          MutableFst<Arc> *ofst,
          uint32 ptype,
          float delta = kDelta) {

  if ((ptype & (kPushWeights | kPushLabels)) == kPushWeights) {
    *ofst = ifst;
    Push(ofst, rtype, delta, ptype & kPushRemoveTotalWeight);
  } else if (ptype & kPushLabels) {
    const GallicType gtype = rtype == REWEIGHT_TO_INITIAL
        ? GALLIC_LEFT : GALLIC_RIGHT;
    vector<typename GallicArc<Arc, gtype>::Weight> gdistance;
    VectorFst<GallicArc<Arc, gtype> > gfst;
    ArcMap(ifst, &gfst, ToGallicMapper<Arc, gtype>());
    if (ptype & kPushWeights ) {
      ShortestDistance(gfst, &gdistance, rtype == REWEIGHT_TO_INITIAL, delta);
    } else {
      ArcMapFst<Arc, Arc, RmWeightMapper<Arc> >
        uwfst(ifst, RmWeightMapper<Arc>());
      ArcMapFst<Arc, GallicArc<Arc, gtype>, ToGallicMapper<Arc, gtype> >
        guwfst(uwfst, ToGallicMapper<Arc, gtype>());
      ShortestDistance(guwfst, &gdistance, rtype == REWEIGHT_TO_INITIAL, delta);
    }
    typename GallicArc<Arc, gtype>::Weight total_weight =
        GallicArc<Arc, gtype>::Weight::One();
    if (ptype & (kPushRemoveTotalWeight | kPushRemoveCommonAffix)) {
      total_weight = ComputeTotalWeight(
          gfst, gdistance, rtype == REWEIGHT_TO_INITIAL);
      total_weight = typename GallicArc<Arc, gtype>::Weight(
          ptype & kPushRemoveCommonAffix ? total_weight.Value1()
          : StringWeight<typename Arc::Label, GALLIC_STRING_TYPE(gtype)>::One(),
          ptype & kPushRemoveTotalWeight ? total_weight.Value2()
          : Arc::Weight::One());
    }
    Reweight(&gfst, gdistance, rtype);
    if (ptype & (kPushRemoveTotalWeight | kPushRemoveCommonAffix))
      RemoveWeight(&gfst, total_weight, rtype == REWEIGHT_TO_FINAL);
    FactorWeightFst< GallicArc<Arc, gtype>, GallicFactor<typename Arc::Label,
      typename Arc::Weight, gtype> > fwfst(gfst);
    ArcMap(fwfst, ofst, FromGallicMapper<Arc, gtype>());
    ofst->SetOutputSymbols(ifst.OutputSymbols());
  } else {
    LOG(WARNING) << "Push: pushing type is set to 0: "
                 << "pushing neither labels nor weights.";
    *ofst = ifst;
  }
}

}  // namespace fst

#endif /* FST_LIB_PUSH_H_ */
