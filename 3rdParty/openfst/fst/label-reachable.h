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
// Class to determine if a non-epsilon label can be read as the first
// non-epsilon symbol along some path from a given state.

#ifndef FST_LABEL_REACHABLE_H_
#define FST_LABEL_REACHABLE_H_

#include <memory>
#include <utility>
#include <vector>

#include <fst/log.h>

#include <fst/accumulator.h>
#include <fst/arcsort.h>
#include <fst/interval-set.h>
#include <fst/state-reachable.h>
#include <fst/util.h>
#include <fst/vector-fst.h>

#include <unordered_map>

namespace fst {

// Stores shareable data for label reachable class copies.
template <typename Label>
class LabelReachableData {
 public:
  using LabelIntervalSet = IntervalSet<Label>;
  using Interval = typename LabelIntervalSet::Interval;

  explicit LabelReachableData(bool reach_input, bool keep_relabel_data = true)
      : reach_input_(reach_input),
        keep_relabel_data_(keep_relabel_data),
        have_relabel_data_(true),
        final_label_(kNoLabel) {}

  ~LabelReachableData() {}

  bool ReachInput() const { return reach_input_; }

  std::vector<LabelIntervalSet> *MutableIntervalSets() {
    return &interval_sets_;
  }

  const LabelIntervalSet &GetIntervalSet(int s) const {
    return interval_sets_[s];
  }

  int NumIntervalSets() const { return interval_sets_.size(); }

  std::unordered_map<Label, Label> *MutableLabel2Index() {
    if (!have_relabel_data_) {
      FSTERROR() << "LabelReachableData: No relabeling data";
    }
    return &label2index_;
  }

  const std::unordered_map<Label, Label> *Label2Index() const {
    if (!have_relabel_data_) {
      FSTERROR() << "LabelReachableData: No relabeling data";
    }
    return &label2index_;
  }

  void SetFinalLabel(Label final_label) { final_label_ = final_label; }

  Label FinalLabel() const { return final_label_; }

  static LabelReachableData *Read(std::istream &istrm,
                                  const FstReadOptions &opts) {
    // NB: Using `new` to access private constructor.
    auto data = fst::WrapUnique(new LabelReachableData());
    ReadType(istrm, &data->reach_input_);
    ReadType(istrm, &data->keep_relabel_data_);
    data->have_relabel_data_ = data->keep_relabel_data_;
    if (data->keep_relabel_data_) ReadType(istrm, &data->label2index_);
    ReadType(istrm, &data->final_label_);
    ReadType(istrm, &data->interval_sets_);
    return data.release();
  }

  bool Write(std::ostream &ostrm, const FstWriteOptions &opts) const {
    WriteType(ostrm, reach_input_);
    WriteType(ostrm, keep_relabel_data_);
    if (keep_relabel_data_) WriteType(ostrm, label2index_);
    WriteType(ostrm, FinalLabel());
    WriteType(ostrm, interval_sets_);
    return true;
  }

 private:
  LabelReachableData() {}

  bool reach_input_;                               // Input labels considered?
  bool keep_relabel_data_;                         // Save label2index_ to file?
  bool have_relabel_data_;                         // Using label2index_?
  Label final_label_;                              // Final label.
  std::unordered_map<Label, Label> label2index_;  // Finds index for a label.
  std::vector<LabelIntervalSet> interval_sets_;    // Interval sets per state.
};

// Apply a new state order to a vector of LabelIntervalSets. order[i] gives
// the StateId after sorting that corresponds to the StateId i before
// sorting; it must therefore be a permutation of the input FST's StateId
// sequence.
template <typename Label, typename StateId>
bool StateSort(std::vector<IntervalSet<Label>> *interval_sets,
               const std::vector<StateId> &order) {
  if (order.size() != interval_sets->size()) {
    FSTERROR() << "StateSort: Bad order vector size: " << order.size()
               << ", expected: " << interval_sets->size();
    return false;
  }
  std::vector<IntervalSet<Label>> reordered_interval_sets(
      interval_sets->size());
  // TODO(jrosenstock): Use storage-efficient cycle-following algorithm
  // from StateSort(MutableFst *, order).
  for (StateId s = 0; s < order.size(); ++s) {
    reordered_interval_sets[order[s]] = std::move((*interval_sets)[s]);
  }
  *interval_sets = std::move(reordered_interval_sets);
  return true;
}

// Apply a new state order to LabelReachableData.
template <typename Label, typename StateId>
bool StateSort(LabelReachableData<Label> *data,
               const std::vector<StateId> &order) {
  return StateSort(data->MutableIntervalSets(), order);
}

// Functor to find the LowerBound of a Label using an ArcIterator.
// Used by LabelReachable.  Other, more efficient implementations of
// this interface specialized to certain FST types may be used instead.
template <class Arc>
class LabelLowerBound {
 public:
  using Label = typename Arc::Label;
  using StateId = typename Arc::StateId;

  // Initializes with the FST that will later supply the ArcIterator for
  // `operator()`.  `reach_input` specified whether `operator()` will search
  // input or output labels.  If `is_copy == true`, then `fst` is a copy
  // of the one previously passed to `Init`, so any expensive initialization
  // can be skipped.
  template <class FST>
  void Init(const FST &fst, bool reach_input, bool is_copy) {
    reach_input_ = reach_input;
  }

  // Sets state that will be searched by `operator()`.
  void SetState(StateId aiter_s) {}

  // Positions `aiter` at the first Arc with `label >= match_label` in the
  // half-open interval `[aiter_begin, aiter_end)`.  Returns the position
  // of `aiter`.  `aiter` must be an iterator of the FST that was passed to
  // `Init`.
  template <class ArcIterator>
  ssize_t operator()(ArcIterator *aiter, ssize_t aiter_begin, ssize_t aiter_end,
                     Label match_label) const {
    // Only needs to compute the ilabel or olabel of arcs when performing the
    // binary search.
    aiter->SetFlags(reach_input_ ? kArcILabelValue : kArcOLabelValue,
                    kArcValueFlags);
    ssize_t low = aiter_begin;
    ssize_t high = aiter_end;
    while (low < high) {
      const ssize_t mid = low + (high - low) / 2;
      aiter->Seek(mid);
      auto label = reach_input_ ? aiter->Value().ilabel : aiter->Value().olabel;
      if (label < match_label) {
        low = mid + 1;
      } else {
        high = mid;
      }
    }
    aiter->Seek(low);
    aiter->SetFlags(kArcValueFlags, kArcValueFlags);
    return low;
  }

 private:
  bool reach_input_ = false;
};

// Tests reachability of labels from a given state. If reach_input is true, then
// input labels are considered, o.w. output labels are considered. To test for
// reachability from a state s, first do SetState(s), then a label l can be
// reached from state s of FST f iff Reach(r) is true where r = Relabel(l). The
// relabeling is required to ensure the consecutive ones property (C1P); this
// allows a compact representation of the reachable labels. See Section 2.3.3 of
// "A Generalized Composition Algorithm for Weighted Finite-State Transducers",
// Cyril Allauzen, Michael Riley, Johan Schalkwyk, Interspeech 2009.
// https://static.googleusercontent.com/media/research.google.com/en//pubs/archive/35539.pdf

// The whole FST can be relabeled instead with Relabel(&f, reach_input) so that
// the test Reach(r) applies directly to the labels of the transformed FST f.
// The relabeled FST will also be sorted appropriately for composition.
//
// Reachablity of a final state from state s (via an epsilon path) can be
// tested with ReachFinal().
//
// Reachability can also be tested on the set of labels specified by an arc
// iterator, useful for FST composition. In particular, Reach(aiter, ...) is
// true if labels on the input (output) side of the transitions of the arc
// iterator, when iter_input is true (false), can be reached from the state s.
// The iterator labels must have already been relabeled.
//
// With the arc iterator test of reachability, the begin position, end position
// and accumulated arc weight of the matches can be returned. The optional
// template argument controls how reachable arc weights are accumulated. The
// default uses semiring Plus(). Alternative ones can be used to distribute the
// weights in composition in various ways.
template <class Arc, class Accumulator = DefaultAccumulator<Arc>,
          class D = LabelReachableData<typename Arc::Label>,
          class LB = LabelLowerBound<Arc>>
class LabelReachable {
 public:
  using Label = typename Arc::Label;
  using StateId = typename Arc::StateId;
  using Weight = typename Arc::Weight;
  using Data = D;
  using LowerBound = LB;

  using LabelIntervalSet = typename Data::LabelIntervalSet;

  using Interval = typename LabelIntervalSet::Interval;

  LabelReachable(const Fst<Arc> &fst, bool reach_input,
                 std::unique_ptr<Accumulator> accumulator = nullptr,
                 bool keep_relabel_data = true)
      : fst_(std::make_unique<VectorFst<Arc>>(fst)),
        s_(kNoStateId),
        data_(std::make_shared<Data>(reach_input, keep_relabel_data)),
        accumulator_(accumulator ? std::move(accumulator)
                                 : std::make_unique<Accumulator>()) {
    const auto ins = fst_->NumStates();
    TransformFst();
    FindIntervals(ins);
    fst_.reset();
  }

  explicit LabelReachable(std::shared_ptr<Data> data,
                          std::unique_ptr<Accumulator> accumulator = nullptr)
      : s_(kNoStateId),
        data_(std::move(data)),
        accumulator_(accumulator ? std::move(accumulator)
                                 : std::make_unique<Accumulator>()) {}

  LabelReachable(const LabelReachable &reachable, bool safe = false)
      : s_(kNoStateId),
        data_(reachable.data_),
        accumulator_(
            std::make_unique<Accumulator>(*reachable.accumulator_, safe)),
        lower_bound_(reachable.lower_bound_),
        reach_fst_input_(reachable.reach_fst_input_),
        error_(reachable.error_) {}

  ~LabelReachable() {
    if (ncalls_ > 0) {
      VLOG(2) << "# of calls: " << ncalls_;
      VLOG(2) << "# of intervals/call: " << (nintervals_ / ncalls_);
    }
  }

  // Relabels w.r.t labels that give compact label sets.
  Label Relabel(Label label) {
    if (label == 0 || error_) return label;
    const auto &label2index = *data_->Label2Index();
    auto iter = label2index.find(label);
    if (iter != label2index.end()) return iter->second;
    auto &relabel = oov_label2index_[label];
    if (!relabel) {
      // Adds new label.
      relabel = label2index.size() + oov_label2index_.size() + 1;
    }
    return relabel;
  }

  // Relabels FST w.r.t to labels that give compact label sets.
  void Relabel(MutableFst<Arc> *fst, bool relabel_input) {
    for (StateIterator<MutableFst<Arc>> siter(*fst); !siter.Done();
         siter.Next()) {
      for (MutableArcIterator<MutableFst<Arc>> aiter(fst, siter.Value());
           !aiter.Done(); aiter.Next()) {
        auto arc = aiter.Value();
        if (relabel_input) {
          arc.ilabel = Relabel(arc.ilabel);
        } else {
          arc.olabel = Relabel(arc.olabel);
        }
        aiter.SetValue(arc);
      }
    }
    if (relabel_input) {
      ArcSort(fst, ILabelCompare<Arc>());
      fst->SetInputSymbols(nullptr);
    } else {
      ArcSort(fst, OLabelCompare<Arc>());
      fst->SetOutputSymbols(nullptr);
    }
  }

  // Returns relabeling pairs (cf. relabel.h::Relabel()). If avoid_collisions is
  // true, extra pairs are added to ensure no collisions when relabeling
  // automata that have labels unseen here.
  void RelabelPairs(std::vector<std::pair<Label, Label>> *pairs,
                    bool avoid_collisions = false) {
    pairs->clear();
    const auto &label2index = *data_->Label2Index();
    // Maps labels to their new values in [1, label2index().size()].
    for (const auto &kv : label2index) {
      if (kv.second != data_->FinalLabel()) {
        pairs->emplace_back(kv);
      }
    }
    // Maps oov labels to their values > label2index().size().
    pairs->insert(pairs->end(), oov_label2index_.begin(),
                  oov_label2index_.end());
    if (avoid_collisions) {
      // Ensures any label in [1, label2index().size()] is mapped either
      // by the above steps or to label2index() + 1 (to avoid collisions).
      for (size_t i = 1; i <= label2index.size(); ++i) {
        const auto it = label2index.find(i);
        bool unmapped = it == label2index.end();
        if (unmapped) unmapped = oov_label2index_.count(i) == 0;
        if (unmapped || it->second == data_->FinalLabel()) {
          pairs->emplace_back(i, label2index.size() + 1);
        }
      }
    }
  }

  // Set current state. Optionally set state associated
  // with arc iterator to be passed to Reach.
  void SetState(StateId s, StateId aiter_s = kNoStateId) {
    s_ = s;
    if (aiter_s != kNoStateId) {
      accumulator_->SetState(aiter_s);
      if (accumulator_->Error()) error_ = true;
      lower_bound_.SetState(aiter_s);
    }
  }

  // Can reach this label from current state?
  // Original labels must be transformed by the Relabel methods above.
  bool Reach(Label label) const {
    if (label == 0 || error_) return false;
    return data_->GetIntervalSet(s_).Member(label);
  }

  // Can reach final state (via epsilon transitions) from this state?
  bool ReachFinal() const {
    if (error_) return false;
    return data_->GetIntervalSet(s_).Member(data_->FinalLabel());
  }

  // Initialize with secondary FST to be used with Reach(Iterator,...).
  // If reach_input = true, then arc input labels are considered in
  // Reach(aiter, ...), o.w. output labels are considered. If copy is true, then
  // the FST is a copy of the FST used in the previous call to this method
  // (useful to avoid unnecessary updates).
  template <class FST>
  void ReachInit(const FST &fst, bool reach_input, bool copy = false) {
    reach_fst_input_ = reach_input;
    if (!fst.Properties(reach_fst_input_ ? kILabelSorted : kOLabelSorted,
                        true)) {
      FSTERROR() << "LabelReachable::ReachInit: Fst is not sorted";
      error_ = true;
    }
    accumulator_->Init(fst, copy);
    if (accumulator_->Error()) error_ = true;
    lower_bound_.Init(fst, /*reach_input=*/reach_input, /*is_copy=*/copy);
  }

  // Can reach any arc iterator label between iterator positions
  // aiter_begin and aiter_end?
  // Arc iterator labels must be transformed by the Relabel methods
  // above. If compute_weight is true, user may call ReachWeight().
  template <class Iterator>
  bool Reach(Iterator *aiter, ssize_t aiter_begin, ssize_t aiter_end,
             bool compute_weight) {
    if (error_) return false;
    const auto &interval_set = data_->GetIntervalSet(s_);
    ++ncalls_;
    nintervals_ += interval_set.Size();
    reach_begin_ = -1;
    reach_end_ = -1;
    reach_weight_ = Weight::Zero();
    const auto flags = aiter->Flags();  // Save flags to restore them on exit.
    aiter->SetFlags(kArcNoCache, kArcNoCache);  // Makes caching optional.
    aiter->Seek(aiter_begin);
    if (2 * (aiter_end - aiter_begin) < interval_set.Size()) {
      // Checks each arc against intervals, setting arc iterator flags to only
      // compute the ilabel or olabel values, since they are the only values
      // required for most of the arcs processed.
      aiter->SetFlags(reach_fst_input_ ? kArcILabelValue : kArcOLabelValue,
                      kArcValueFlags);
      Label reach_label = kNoLabel;
      for (auto aiter_pos = aiter_begin; aiter_pos < aiter_end;
           aiter->Next(), ++aiter_pos) {
        const auto &arc = aiter->Value();
        const auto label = reach_fst_input_ ? arc.ilabel : arc.olabel;
        if (label == reach_label || Reach(label)) {
          reach_label = label;
          if (reach_begin_ < 0) reach_begin_ = aiter_pos;
          reach_end_ = aiter_pos + 1;
          if (compute_weight) {
            if (!(aiter->Flags() & kArcWeightValue)) {
              // If arc.weight wasn't computed by the call to aiter->Value()
              // above, we need to call aiter->Value() again after having set
              // the arc iterator flags to compute the arc weight value.
              aiter->SetFlags(kArcWeightValue, kArcValueFlags);
              const auto &arcb = aiter->Value();
              // Call the accumulator.
              reach_weight_ = accumulator_->Sum(reach_weight_, arcb.weight);
              // Only ilabel or olabel required to process the following arcs.
              aiter->SetFlags(
                  reach_fst_input_ ? kArcILabelValue : kArcOLabelValue,
                  kArcValueFlags);
            } else {
              // Calls the accumulator.
              reach_weight_ = accumulator_->Sum(reach_weight_, arc.weight);
            }
          }
        }
      }
    } else {
      // Checks each interval against arcs.
      auto begin_low = aiter_begin;
      auto end_low = aiter_begin;
      for (const auto &interval : interval_set) {
        begin_low = lower_bound_(aiter, end_low, aiter_end, interval.begin);
        end_low = lower_bound_(aiter, begin_low, aiter_end, interval.end);
        if (end_low - begin_low > 0) {
          if (reach_begin_ < 0) reach_begin_ = begin_low;
          reach_end_ = end_low;
          if (compute_weight) {
            aiter->SetFlags(kArcWeightValue, kArcValueFlags);
            reach_weight_ =
                accumulator_->Sum(reach_weight_, aiter, begin_low, end_low);
          }
        }
      }
    }
    aiter->SetFlags(flags, kArcFlags);  // Restores original flag values.
    return reach_begin_ >= 0;
  }

  // Returns iterator position of first matching arc.
  ssize_t ReachBegin() const { return reach_begin_; }

  // Returns iterator position one past last matching arc.
  ssize_t ReachEnd() const { return reach_end_; }

  // Return the sum of the weights for matching arcs. Valid only if
  // compute_weight was true in Reach() call.
  Weight ReachWeight() const { return reach_weight_; }

  // Access to the relabeling map. Excludes epsilon (0) label but
  // includes kNoLabel that is used internally for super-final
  // transitions.
  const std::unordered_map<Label, Label> &Label2Index() const {
    return *data_->Label2Index();
  }

  const Data *GetData() const { return data_.get(); }

  std::shared_ptr<Data> GetSharedData() const { return data_; }

  bool Error() const { return error_ || accumulator_->Error(); }

 private:
  // Redirects labeled arcs (input or output labels determined by ReachInput())
  // to new label-specific final states. Each original final state is
  // redirected via a transition labeled with kNoLabel to a new
  // kNoLabel-specific final state. Creates super-initial state for all states
  // with zero in-degree.
  void TransformFst() {
    auto ins = fst_->NumStates();
    auto ons = ins;
    std::vector<ssize_t> indeg(ins, 0);
    // Redirects labeled arcs to new final states.
    for (StateId s = 0; s < ins; ++s) {
      for (MutableArcIterator<VectorFst<Arc>> aiter(fst_.get(), s);
           !aiter.Done(); aiter.Next()) {
        auto arc = aiter.Value();
        const auto label = data_->ReachInput() ? arc.ilabel : arc.olabel;
        if (label) {
          if (auto insert_result = label2state_.emplace(label, ons);
              insert_result.second) {
            indeg.push_back(0);
            ++ons;
          }
          arc.nextstate = label2state_[label];
          aiter.SetValue(arc);
        }
        ++indeg[arc.nextstate];  // Finds in-degrees for next step.
      }
      // Redirects final weights to new final state.
      auto final_weight = fst_->Final(s);
      if (final_weight != Weight::Zero()) {
        if (auto insert_result = label2state_.emplace(kNoLabel, ons);
            insert_result.second) {
          indeg.push_back(0);
          ++ons;
        }
        const auto nextstate = label2state_[kNoLabel];
        fst_->EmplaceArc(s, kNoLabel, kNoLabel, std::move(final_weight),
                         nextstate);
        ++indeg[nextstate];  // Finds in-degrees for next step.
        fst_->SetFinal(s, Weight::Zero());
      }
    }
    // Adds new final states to the FST.
    while (fst_->NumStates() < ons) {
      StateId s = fst_->AddState();
      fst_->SetFinal(s);
    }
    // Creates a super-initial state for all states with zero in-degree.
    const auto start = fst_->AddState();
    fst_->SetStart(start);
    for (StateId s = 0; s < start; ++s) {
      if (indeg[s] == 0) fst_->EmplaceArc(start, 0, 0, s);
    }
  }

  void FindIntervals(StateId ins) {
    StateReachable<Arc, Label, LabelIntervalSet> state_reachable(*fst_);
    if (state_reachable.Error()) {
      error_ = true;
      return;
    }
    auto &state2index = state_reachable.State2Index();
    auto &interval_sets = *data_->MutableIntervalSets();
    interval_sets = state_reachable.IntervalSets();
    interval_sets.resize(ins);
    auto &label2index = *data_->MutableLabel2Index();
    for (const auto &kv : label2state_) {
      Label i = state2index[kv.second];
      label2index[kv.first] = i;
      if (kv.first == kNoLabel) data_->SetFinalLabel(i);
    }
    label2state_.clear();
    double nintervals = 0;
    ssize_t non_intervals = 0;
    for (StateId s = 0; s < ins; ++s) {
      nintervals += interval_sets[s].Size();
      if (interval_sets[s].Size() > 1) {
        ++non_intervals;
        VLOG(3) << "state: " << s
                << " # of intervals: " << interval_sets[s].Size();
      }
    }
    VLOG(2) << "# of states: " << ins;
    VLOG(2) << "# of intervals: " << nintervals;
    VLOG(2) << "# of intervals/state: " << nintervals / ins;
    VLOG(2) << "# of non-interval states: " << non_intervals;
  }

  std::unique_ptr<VectorFst<Arc>> fst_;
  // Current state
  StateId s_;
  // Finds final state for a label
  std::unordered_map<Label, StateId> label2state_;
  // Iterator position of first match.
  ssize_t reach_begin_;
  // Iterator position after last match.
  ssize_t reach_end_;
  // Gives weight sum of arc iterator arcs with reachable labels.
  Weight reach_weight_;
  // Shareable data between copies.
  std::shared_ptr<Data> data_;
  // Sums arc weights.
  std::unique_ptr<Accumulator> accumulator_;
  // Functor for computing LowerBound(Iterator*, begin, end, label).
  LowerBound lower_bound_;
  // Relabeling map for OOV labels.
  std::unordered_map<Label, Label> oov_label2index_;
  double ncalls_ = 0;
  double nintervals_ = 0;
  bool reach_fst_input_ = false;
  bool error_ = false;
};

}  // namespace fst

#endif  // FST_LABEL_REACHABLE_H_
