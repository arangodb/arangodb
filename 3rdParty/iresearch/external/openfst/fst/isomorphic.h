// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.
//
// Function to test two FSTs are isomorphic, i.e., they are equal up to a state
// and arc re-ordering. FSTs should be deterministic when viewed as
// unweighted automata. False negatives (but not false positives) are possible
// when the inputs are nondeterministic (when viewed as unweighted automata).

#ifndef FST_ISOMORPHIC_H_
#define FST_ISOMORPHIC_H_

#include <algorithm>
#include <queue>
#include <type_traits>
#include <vector>

#include <fst/log.h>

#include <fst/fst.h>


namespace fst {
namespace internal {

// Orders weights for equality checking; delta is ignored.
template <class Weight, typename std::enable_if<
                            IsIdempotent<Weight>::value>::type * = nullptr>
bool WeightCompare(const Weight &w1, const Weight &w2, float, bool *) {
  static const NaturalLess<Weight> less;
  return less(w1, w2);
}

template <class Weight, typename std::enable_if<
                            !IsIdempotent<Weight>::value>::type * = nullptr>
bool WeightCompare(const Weight &w1, const Weight &w2, float delta,
                   bool *error) {
  // No natural order; use hash.
  const auto q1 = w1.Quantize(delta);
  const auto q2 = w2.Quantize(delta);
  const auto n1 = q1.Hash();
  const auto n2 = q2.Hash();
  // Hash not unique; very unlikely to happen.
  if (n1 == n2 && q1 != q2) {
    VLOG(1) << "Isomorphic: Weight hash collision";
    *error = true;
  }
  return n1 < n2;
}

template <class Arc>
class Isomorphism {
  using StateId = typename Arc::StateId;

 public:
  Isomorphism(const Fst<Arc> &fst1, const Fst<Arc> &fst2, float delta)
      : fst1_(fst1.Copy()),
        fst2_(fst2.Copy()),
        delta_(delta),
        error_(false),
        nondet_(false),
        comp_(delta, &error_) {}

  // Checks if input FSTs are isomorphic.
  bool IsIsomorphic() {
    if (fst1_->Start() == kNoStateId && fst2_->Start() == kNoStateId) {
      return true;
    }
    if (fst1_->Start() == kNoStateId || fst2_->Start() == kNoStateId) {
      VLOG(1) << "Isomorphic: Only one of the FSTs is empty.";
      return false;
    }
    PairState(fst1_->Start(), fst2_->Start());
    while (!queue_.empty()) {
      const auto &pr = queue_.front();
      if (!IsIsomorphicState(pr.first, pr.second)) {
        if (nondet_) {
          VLOG(1) << "Isomorphic: Non-determinism as an unweighted automaton. "
                  << "state1: " << pr.first << " state2: " << pr.second;
          error_ = true;
        }
        return false;
      }
      queue_.pop();
    }
    return true;
  }

  bool Error() const { return error_; }

 private:
  // Orders arcs for equality checking.
  class ArcCompare {
   public:
    ArcCompare(float delta, bool *error) : delta_(delta), error_(error) {}

    bool operator()(const Arc &arc1, const Arc &arc2) const {
      if (arc1.ilabel < arc2.ilabel) return true;
      if (arc1.ilabel > arc2.ilabel) return false;
      if (arc1.olabel < arc2.olabel) return true;
      if (arc1.olabel > arc2.olabel) return false;
      if (!ApproxEqual(arc1.weight, arc2.weight, delta_)) {
        return WeightCompare(arc1.weight, arc2.weight, delta_, error_);
      } else {
        return arc1.nextstate < arc2.nextstate;
      }
    }

   private:
    const float delta_;
    bool *error_;
  };

  // Maintains state correspondences and queue.
  bool PairState(StateId s1, StateId s2) {
    if (state_pairs_.size() <= s1) state_pairs_.resize(s1 + 1, kNoStateId);
    if (state_pairs_[s1] == s2) {
      return true;  // Already seen this pair.
    } else if (state_pairs_[s1] != kNoStateId) {
      return false;  // s1 already paired with another s2.
    }
    VLOG(3) << "Pairing states: (" << s1 << ", " << s2 << ")";
    state_pairs_[s1] = s2;
    queue_.emplace(s1, s2);
    return true;
  }

  // Checks if state pair is isomorphic.
  bool IsIsomorphicState(StateId s1, StateId s2);

  std::unique_ptr<Fst<Arc>> fst1_;
  std::unique_ptr<Fst<Arc>> fst2_;
  float delta_;                          // Weight equality delta.
  std::vector<Arc> arcs1_;               // For sorting arcs on FST1.
  std::vector<Arc> arcs2_;               // For sorting arcs on FST2.
  std::vector<StateId> state_pairs_;     // Maintains state correspondences.
  std::queue<std::pair<StateId, StateId>> queue_;  // Queue of state pairs.
  bool error_;                           // Error flag.
  bool nondet_;                          // Nondeterminism detected.
  ArcCompare comp_;
};

template <class Arc>
bool Isomorphism<Arc>::IsIsomorphicState(StateId s1, StateId s2) {
  if (!ApproxEqual(fst1_->Final(s1), fst2_->Final(s2), delta_)) return false;
  const auto narcs1 = fst1_->NumArcs(s1);
  const auto narcs2 = fst2_->NumArcs(s2);
  if (narcs1 != narcs2) {
    VLOG(1) << "Isomorphic: NumArcs not equal. "
            << "fst1.NumArcs(" << s1 << ") = " << narcs1 << ", "
            << "fst2.NumArcs(" << s2 << ") = " << narcs2;
    return false;
  }
  ArcIterator<Fst<Arc>> aiter1(*fst1_, s1);
  ArcIterator<Fst<Arc>> aiter2(*fst2_, s2);
  arcs1_.clear();
  arcs1_.reserve(narcs1);
  arcs2_.clear();
  arcs2_.reserve(narcs2);
  for (; !aiter1.Done(); aiter1.Next(), aiter2.Next()) {
    arcs1_.push_back(aiter1.Value());
    arcs2_.push_back(aiter2.Value());
  }
  std::sort(arcs1_.begin(), arcs1_.end(), comp_);
  std::sort(arcs2_.begin(), arcs2_.end(), comp_);
  for (size_t i = 0; i < arcs1_.size(); ++i) {
    const auto &arc1 = arcs1_[i];
    const auto &arc2 = arcs2_[i];
    if (arc1.ilabel != arc2.ilabel) {
      VLOG(1) << "Isomorphic: ilabels not equal. "
              << "arc1: *" << arc1.ilabel << "* " << arc1.olabel
              << " " << arc1.weight << " " << arc1.nextstate
              << "arc2: *" << arc2.ilabel << "* " << arc2.olabel
              << " " << arc2.weight << " " << arc2.nextstate;
      return false;
    }
    if (arc1.olabel != arc2.olabel) {
      VLOG(1) << "Isomorphic: olabels not equal. "
              << "arc1: " << arc1.ilabel << " *" << arc1.olabel
              << "* " << arc1.weight << " " << arc1.nextstate
              << "arc2: " << arc2.ilabel << " *" << arc2.olabel
              << "* " << arc2.weight << " " << arc2.nextstate;
      return false;
    }
    if (!ApproxEqual(arc1.weight, arc2.weight, delta_)) {
      VLOG(1) << "Isomorphic: weights not ApproxEqual. "
              << "arc1: " << arc1.ilabel << " " << arc1.olabel
              << " *" << arc1.weight << "* " << arc1.nextstate
              << "arc2: " << arc2.ilabel << " " << arc2.olabel
              << " *" << arc2.weight << "* " << arc2.nextstate;
      return false;
    }
    if (!PairState(arc1.nextstate, arc2.nextstate)) {
      VLOG(1) << "Isomorphic: nextstates could not be paired. "
              << "arc1: " << arc1.ilabel << " " << arc1.olabel
              << " " << arc1.weight << " *" << arc1.nextstate
              << "* arc2: " << arc2.ilabel << " " << arc2.olabel
              << " " << arc2.weight << " *" << arc2.nextstate << "*";
      return false;
    }
    if (i > 0) {  // Checks for non-determinism.
      const auto &arc0 = arcs1_[i - 1];
      if (arc1.ilabel == arc0.ilabel && arc1.olabel == arc0.olabel &&
          ApproxEqual(arc1.weight, arc0.weight, delta_)) {
        // Any subsequent matching failure maybe a false negative
        // since we only consider one permutation when pairing destination
        // states of nondeterministic transitions.
        VLOG(1) << "Isomorphic: Detected non-determinism as an unweighted "
                << "automaton; deferring error. "
                << "arc1: " << arc1.ilabel << " " << arc1.olabel
                << " " << arc1.weight << " " << arc1.nextstate
                << "arc2: " << arc2.ilabel << " " << arc2.olabel
                << " " << arc2.weight << " " << arc2.nextstate;
        nondet_ = true;
      }
    }
  }
  return true;
}

}  // namespace internal

// Tests if two FSTs have the same states and arcs up to a reordering.
// Inputs should be nondeterministic when viewed as unweighted automata.
// When the inputs are nondeterministic, the algorithm only considers one
// permutation for each set of equivalent nondeterministic transitions
// (the permutation that preserves state ID ordering) and hence might return
// false negatives (but it never returns false positives).
template <class Arc>
bool Isomorphic(const Fst<Arc> &fst1, const Fst<Arc> &fst2,
                float delta = kDelta) {
  internal::Isomorphism<Arc> iso(fst1, fst2, delta);
  const bool result = iso.IsIsomorphic();
  if (iso.Error()) {
    FSTERROR() << "Isomorphic: Cannot determine if inputs are isomorphic";
    return false;
  } else {
    return result;
  }
}

}  // namespace fst

#endif  // FST_ISOMORPHIC_H_
