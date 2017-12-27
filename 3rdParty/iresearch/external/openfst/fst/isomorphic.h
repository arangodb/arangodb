// isomorphic.h

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
// Function to test two FSTs are isomorphic - i.e.  they are equal up
// to a state and arc re-ordering.  FSTs should be deterministic when
// viewed as unweighted automata.

#ifndef FST_LIB_ISOMORPHIC_H__
#define FST_LIB_ISOMORPHIC_H__

#include <algorithm>

#include <fst/fst.h>


namespace fst {

template <class Arc>
class Isomorphism {
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Weight Weight;

 public:
  Isomorphism(const Fst<Arc> &fst1, const Fst<Arc> &fst2, float delta)
      : fst1_(fst1.Copy()), fst2_(fst2.Copy()), delta_(delta), error_(false),
        comp_(delta, &error_) { }

  ~Isomorphism() {
    delete fst1_;
    delete fst2_;
  }

  // Checks if input FSTs are isomorphic
  bool IsIsomorphic() {
    if (fst1_->Start() == kNoStateId && fst2_->Start() == kNoStateId)
    return true;

    if (fst1_->Start() == kNoStateId || fst2_->Start() == kNoStateId)
      return false;

    PairState(fst1_->Start(), fst2_->Start());
    while (!queue_.empty()) {
      const pair<StateId, StateId> &pr = queue_.front();
      if (!IsIsomorphicState(pr.first, pr.second))
        return false;
      queue_.pop_front();
    }
    return true;
  }

  bool Error() const { return error_; }

 private:
  // Orders arcs for equality checking.
  class ArcCompare {
   public:
    ArcCompare(float delta, bool *error)
        : delta_(delta), error_(error) { }

    bool operator() (const Arc &arc1, const Arc &arc2) const {
      if (arc1.ilabel < arc2.ilabel)
        return true;
      if (arc1.ilabel > arc2.ilabel)
        return false;

      if (arc1.olabel < arc2.olabel)
        return true;
      if (arc1.olabel > arc2.olabel)
        return false;

      return WeightCompare(arc1.weight, arc2.weight, delta_, error_);
    }
   private:
    float delta_;
    bool *error_;
  };

  // Orders weights for equality checking.
  static bool WeightCompare(Weight w1, Weight w2, float delta, bool *error);

  // Maintains state correspondences and queue.
  bool PairState(StateId s1, StateId s2) {
    if (state_pairs_.size() <= s1)
      state_pairs_.resize(s1 + 1, kNoStateId);
    if (state_pairs_[s1] == s2)
      return true;          // already seen this pair
    else if (state_pairs_[s1] != kNoStateId)
      return false;         // s1 already paired with another s2

    state_pairs_[s1] = s2;
    queue_.push_back(std::make_pair(s1, s2));
    return true;
  }

  // Checks if state pair is isomorphic
  bool IsIsomorphicState(StateId s1, StateId s2);

  Fst<Arc> *fst1_;
  Fst<Arc> *fst2_;
  float delta_;                          // Weight equality delta
  vector<Arc> arcs1_;                    // for sorting arcs on FST1
  vector<Arc> arcs2_;                    // for sorting arcs on FST2
  vector<StateId> state_pairs_;          // maintains state correspondences
  list<pair<StateId, StateId> > queue_;  // queue of states to process
  bool error_;                           // error flag
  ArcCompare comp_;

  DISALLOW_COPY_AND_ASSIGN(Isomorphism);
};

template<class Arc>
bool Isomorphism<Arc>::WeightCompare(Weight w1, Weight w2,
                                     float delta, bool *error) {
  if (Weight::Properties() & kIdempotent) {
    NaturalLess<Weight> less;
    return less(w1, w2);
  } else {  // No natural order; use hash
    Weight q1 = w1.Quantize(delta);
    Weight q2 = w2.Quantize(delta);
    size_t n1 = q1.Hash();
    size_t n2 = q2.Hash();

    // Hash not unique. Very unlikely to happen.
    if (n1 == n2 && q1 != q2) {
      LOG(ERROR) << "Isomorphic: weight hash collision";
      *error = true;
    }
    return n1 < n2;
  }
}

template<class Arc>
bool Isomorphism<Arc>::IsIsomorphicState(StateId s1, StateId s2) {
  if (!ApproxEqual(fst1_->Final(s1), fst2_->Final(s2), delta_))
    return false;
  if (fst1_->NumArcs(s1) != fst2_->NumArcs(s2))
    return false;

  ArcIterator< Fst<Arc> > aiter1(*fst1_, s1);
  ArcIterator< Fst<Arc> > aiter2(*fst2_, s2);

  arcs1_.clear();
  arcs2_.clear();
  for (; !aiter1.Done(); aiter1.Next(), aiter2.Next()) {
    arcs1_.push_back(aiter1.Value());
    arcs2_.push_back(aiter2.Value());
  }

  std::sort(arcs1_.begin(), arcs1_.end(), comp_);
  std::sort(arcs2_.begin(), arcs2_.end(), comp_);

  Arc arc0;
  for (size_t i = 0; i < arcs1_.size(); ++i) {
    const Arc &arc1 = arcs1_[i];
    const Arc &arc2 = arcs2_[i];

    if (arc1.ilabel != arc2.ilabel)
      return false;
    if (arc1.olabel != arc2.olabel)
      return false;
    if (!ApproxEqual(arc1.weight, arc2.weight, delta_))
      return false;
    if (!PairState(arc1.nextstate, arc2.nextstate))
      return false;

    if (i > 0) {  // Checks for non-determinism
      const Arc &arc0 = arcs1_[i - 1];
      if (arc1.ilabel == arc0.ilabel && arc1.olabel == arc0.olabel &&
          ApproxEqual(arc1.weight, arc0.weight, delta_)) {
        LOG(ERROR) << "Isomorphic: non-determinism as an unweighted automaton";
        error_ = true;
        return false;
      }
    }
  }
  return true;
}

// Tests if two Fsts have the same states and arcs up to a reordering.
// Inputs should be non-deterministic when viewed as unweighted automata
// (cf. Encode()).  Returns optional error value (useful when
// FLAGS_error_fatal = false).
template <class Arc>
bool Isomorphic(const Fst<Arc> &fst1, const Fst<Arc> &fst2,
                float delta = kDelta, bool *error = 0) {
  Isomorphism<Arc> iso(fst1, fst2, delta);
  bool ret = iso.IsIsomorphic();
  if (iso.Error()) {
    FSTERROR() << "Isomorphic: can not determine if inputs are isomorphic";
    if (error) *error = true;
    return false;
  } else {
    if (error) *error = false;
    return ret;
  }
}

}  // namespace fst


#endif  // FST_LIB_ISOMORPHIC_H__
