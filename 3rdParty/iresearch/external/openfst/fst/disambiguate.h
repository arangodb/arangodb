// disambiguate.h


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
// Functions and classes to disambiguate an FST.

#ifndef FST_LIB_DISAMBIGUATE_H__
#define FST_LIB_DISAMBIGUATE_H__

#include <algorithm>
#include <climits>
#include <map>
#include <set>
#include <string>
#include <vector>
using std::vector;

#include <fst/arcsort.h>
#include <fst/compose.h>
#include <fst/connect.h>
#include <fst/determinize.h>
#include <fst/dfs-visit.h>
#include <fst/project.h>
#include <fst/prune.h>
#include <fst/state-map.h>
#include <fst/state-table.h>
#include <fst/union-find.h>
#include <fst/verify.h>


namespace fst {

template <class Arc>
struct DisambiguateOptions : public DeterminizeOptions<Arc> {
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Weight Weight;
  typedef typename Arc::Label Label;

  explicit DisambiguateOptions(float d = kDelta, Weight w = Weight::Zero(),
                               StateId n = kNoStateId, Label l = 0)
      : DeterminizeOptions<Arc>(d, w, n, l, DETERMINIZE_FUNCTIONAL) {}
};

// A determinize filter based on a subset element relation.
// The relation is assumed to be reflexive and symmetric.
template <class Arc, class R>
class RelationDeterminizeFilter {
 public:
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Label Label;
  typedef typename Arc::Weight Weight;

  typedef IntegerFilterState<StateId> FilterState;
  typedef DeterminizeStateTuple<Arc, FilterState> StateTuple;
  typedef typename StateTuple::Subset Subset;
  typedef typename StateTuple::Element Element;
  typedef multimap<Label, DeterminizeArc<StateTuple> > LabelMap;

  // This is needed e.g. to go into the gallic domain for transducers.
  // No need to rebind the relation since its use here only depends
  // on the state Ids.
  template <class A>
  struct rebind { typedef RelationDeterminizeFilter<A, R> other; };

  RelationDeterminizeFilter(const Fst<Arc> &fst)
      : fst_(fst.Copy()),
        r_(new R()),
        s_(kNoStateId),
        head_(0) {
  }

  // Ownership of the relation is given to this class.
  RelationDeterminizeFilter(const Fst<Arc> &fst, R *r)
      : fst_(fst.Copy()),
        r_(r),
        s_(kNoStateId),
        head_(0) {
  }

  // Ownership of the relation is given to this class. Returns head states.
  RelationDeterminizeFilter(const Fst<Arc> &fst, R *r, vector<StateId> *head)
      : fst_(fst.Copy()),
        r_(r),
        s_(kNoStateId),
        head_(head) {
  }

  // This is needed e.g. to go into the gallic domain for transducers.
  // Ownership of the templated filter argument is given to this class.
  template <class F>
  RelationDeterminizeFilter(const Fst<Arc> &fst, F* filter)
      : fst_(fst.Copy()),
        r_(new R(filter->GetRelation())),
        s_(kNoStateId),
        head_(filter->GetHeadStates()) {
    delete filter;
  }

  // Copy ctr. The FST can be passed if it has been e.g. (deep) copied.
  explicit RelationDeterminizeFilter(
      const RelationDeterminizeFilter<Arc, R> &filter,
      const Fst<Arc> *fst = 0)
      : fst_(fst ? fst->Copy() : filter.fst_->Copy()),
        r_(new R(*filter.r_)),
        s_(kNoStateId),
        head_() {
  }

  ~RelationDeterminizeFilter() {
    delete fst_;
    delete r_;
  }

  FilterState Start() const { return FilterState(fst_->Start()); }

  void SetState(StateId s, const StateTuple &tuple) {
    if (s_ != s) {
      s_ = s;
      tuple_ = &tuple;
      StateId head = tuple.filter_state.GetState();
      is_final_ = fst_->Final(head) != Weight::Zero();
      if (head_) {
        if (head_->size() <= s)
          head_->resize(s + 1, kNoStateId);
        (*head_)[s] = head;
      }
    }
  }

  // Filters transition, possibly modifying label map. Returns
  // true if arc is added to label map.
  bool FilterArc(const Arc &arc,
                 const Element &src_element,
                 const Element &dest_element,
                 LabelMap *label_map) const;

  // Filters super-final transition, returning new final weight
  Weight FilterFinal(const Weight final_weight, const Element &element) const {
    return is_final_ ? final_weight : Weight::Zero();
  }

  static uint64 Properties(uint64 props) {
    return props & ~(kIDeterministic | kODeterministic);
  }

  const R &GetRelation() { return *r_; }
  vector<StateId> *GetHeadStates() { return head_; }

 private:
  // Pairs arc labels with state tuples with possible heads and
  // empty subsets.
  void InitLabelMap(LabelMap *label_map) const;

  Fst<Arc> *fst_;  // Input FST
  R *r_;           // Relation compatible with the inverse trans. function
  StateId s_;                    // Current state
  const StateTuple *tuple_;      // Current tuple
  bool is_final_;                // Is the current head state final?
  vector<StateId> *head_;        // Head state for a given state.

  void operator=(const RelationDeterminizeFilter<Arc, R> &filt);  // disallow
};

template <class Arc, class R> bool
RelationDeterminizeFilter<Arc, R>::FilterArc(const Arc &arc,
                                             const Element &src_element,
                                             const Element &dest_element,
                                             LabelMap *label_map) const {
  bool added = false;
  if (label_map->empty())
    InitLabelMap(label_map);

  // Adds element to state tuple if element state is related to tuple head.
  for (typename LabelMap::iterator liter = label_map->lower_bound(arc.ilabel);
       liter != label_map->end() && liter->first == arc.ilabel;
       ++liter) {
    StateTuple *dest_tuple = liter->second.dest_tuple;
    StateId dest_head = dest_tuple->filter_state.GetState();
    if ((*r_)(dest_element.state_id, dest_head)) {
      dest_tuple->subset.push_front(dest_element);
      added = true;
    }
  }
  return added;
}

template <class Arc, class R> void
RelationDeterminizeFilter<Arc, R>::InitLabelMap(LabelMap *label_map) const {
  StateId src_head = tuple_->filter_state.GetState();
  Label label = kNoLabel;
  StateId nextstate = kNoStateId;
  for (ArcIterator< Fst<Arc> > aiter(*fst_, src_head);
       !aiter.Done();
       aiter.Next()) {
    const Arc &arc = aiter.Value();
    if (arc.ilabel == label && arc.nextstate == nextstate)
      continue;  // multiarc
    DeterminizeArc<StateTuple> det_arc(arc);
    det_arc.dest_tuple->filter_state = FilterState(arc.nextstate);
    pair<Label, DeterminizeArc<StateTuple> > pr(arc.ilabel, det_arc);
    label_map->insert(pr);
    label = arc.ilabel;
    nextstate = arc.nextstate;
  }
}

// Helper class to disambiguate an FST via Disambiguate().
template <class Arc>
class Disambiguator {
 public:
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Weight Weight;
  typedef typename Arc::Label Label;
  // Ids arc with state id and arc position. Arc pos of -1 means
  // final (super-final transition).
  typedef pair<StateId, ssize_t> ArcId;

  Disambiguator() : candidates_(0), merge_(0), error_(false) { }

  void Disambiguate(const Fst<Arc> &ifst, MutableFst<Arc> *ofst,
                    const DisambiguateOptions<Arc> &opts =
                    DisambiguateOptions<Arc>()) {
    VectorFst<Arc> sfst(ifst);
    Connect(&sfst);
    ArcSort(&sfst, ArcCompare());
    PreDisambiguate(sfst, ofst, opts);
    ArcSort(ofst, ArcCompare());
    FindAmbiguities(*ofst);
    RemoveSplits(ofst);
    MarkAmbiguities();
    RemoveAmbiguities(ofst);
    if (error_) ofst->SetProperties(kError, kError);
  }

 private:
  // Compare class for comparing input labels and next states of arcs.
  // This sort order facilitates the predisambiguation.
  class ArcCompare {
   public:
    bool operator() (Arc arc1, Arc arc2) const {
      return arc1.ilabel < arc2.ilabel ||
          (arc1.ilabel == arc2.ilabel && arc1.nextstate < arc2.nextstate);
    }

    uint64 Properties(uint64 props) const {
      return (props & kArcSortProperties) | kILabelSorted |
          (props & kAcceptor ? kOLabelSorted : 0);
    }
  };

  // Compare class for comparing transitions represented by their arc ID.
  // This sort order facilitates ambiguity detection.
  class ArcIdCompare {
   public:
    explicit ArcIdCompare(const vector<StateId> &head)
        : head_(head) {
    }

    bool operator()(const ArcId &a1, const ArcId &a2) const {
      StateId src1 = a1.first;
      StateId src2 = a2.first;
      StateId pos1 = a1.second;
      StateId pos2 = a2.second;
      StateId head1 = head_[src1];
      StateId head2 = head_[src2];

      // Sort first by src head state
      if (head1 < head2) return true;
      if (head2 < head1) return false;

      // ... then by src state
      if (src1 < src2) return true;
      if (src2 < src1) return false;

      // ... then by position
      return pos1 < pos2;
    }

   private:
    const vector<StateId> &head_;
  };

  // A relation that determines if two states share a common future.
  class CommonFuture {
   public:
    typedef GenericComposeStateTable<Arc, CharFilterState> T;
    typedef typename T::StateTuple StateTuple;

    // Needed for compilation with DeterminizeRelationFilter
    CommonFuture() {
      FSTERROR() << "Disambiguate::CommonFuture: FST not provided";
    }

    explicit CommonFuture(const Fst<Arc> &ifst) {
      typedef Matcher< Fst<Arc> > M;
      ComposeFstOptions<Arc, M, NullComposeFilter<M> > opts;

      // Ensures composition is between acceptors.
      bool trans = ifst.Properties(kNotAcceptor, true);
      const Fst<Arc> *fsa = trans ?
          new ProjectFst<Arc>(ifst, PROJECT_INPUT) : &ifst;

      opts.state_table = new T(*fsa, *fsa);
      ComposeFst<Arc> cfst(*fsa, *fsa, opts);
      vector<bool> coaccess;
      uint64 props = 0;
      SccVisitor<Arc> scc_visitor(0, 0, &coaccess, &props);
      DfsVisit(cfst, &scc_visitor);
      for (StateId s = 0; s < coaccess.size(); ++s) {
        if (coaccess[s]) {
          const StateTuple &tuple = opts.state_table->Tuple(s);
          related_.insert(tuple.StatePair());
        }
      }
      if (trans)
        delete fsa;
    }

    bool operator()(const StateId s1, StateId s2) const {
      pair<StateId, StateId> pr(s1, s2);
      return related_.count(pr) > 0;
    }

   private:
    // States s1 and s2 resp. are in this relation iff they there is a
    // path from s1 to a final state that has the same label as some
    // path from s2 to a final state.
    set< pair<StateId, StateId> > related_;
  };

  typedef multimap<ArcId, ArcId, ArcIdCompare> ArcIdMap;

  // Returns the arc corresponding to ArcId a
  static Arc GetArc(const Fst<Arc> &fst, ArcId a) {
    if (a.second == -1) {  // returns super-final transition
      Arc arc(kNoLabel, kNoLabel, fst.Final(a.first), kNoStateId);
      return arc;
    } else {
      ArcIterator< Fst<Arc> > aiter(fst, a.first);
      aiter.Seek(a.second);
      return aiter.Value();
    }
  }

  // Outputs an equivalent FST whose states are subsets of states
  // that have a future path in common.
  void PreDisambiguate(const ExpandedFst<Arc> &ifst, MutableFst<Arc> *ofst,
                       const DisambiguateOptions<Arc> &opts);

  // Finds transitions that are ambiguous candidates in the result of
  // PreDisambiguate.
  void FindAmbiguities(const ExpandedFst<Arc> &fst);

  // Finds transition pairs that are ambiguous candidates from two specified
  // source states.
  void FindAmbiguousPairs(const ExpandedFst<Arc> &fst, StateId s1, StateId s2);

  // Marks ambiguous transitions to be removed.
  void MarkAmbiguities();

  // Deletes spurious ambiguous transitions (due to quantization)
  void RemoveSplits(MutableFst<Arc> *ofst);

  // Deletes actual ambiguous transitions.
  void RemoveAmbiguities(MutableFst<Arc> *ofst);

  // States s1 and s2 are in this relation iff there is a path
  // from the initial state to s1 that has the same label as some path
  // from the initial state to s2.  We store only state pairs s1, s2
  // such that s1 <= s2.
  set< pair<StateId, StateId> > coreachable_;
  // Queue of disambiguation-related states to be processed. We store
  // only state pairs s1, s2 such that s1 <= s2.
  list < pair<StateId, StateId> > queue_;
  // Head state in the pre-disambiguation for a given state.
  vector<StateId> head_;
  // Maps from a candidate ambiguous arc A to each ambiguous candidate arc
  // B with the same label and destination state as A, whose source state s'
  // is coreachable with the source state s of A, and for which
  // head(s') < head(s).
  ArcIdMap *candidates_;
  // Set of ambiguous transitions to be removed.
  set<ArcId> ambiguous_;
  // States to merge due to quantization issues.
  UnionFind<StateId> *merge_;
  // Marks error condition.
  bool error_;
  DISALLOW_COPY_AND_ASSIGN(Disambiguator);
};

template <class Arc> void
Disambiguator<Arc>::PreDisambiguate(const ExpandedFst<Arc> &ifst,
                                    MutableFst<Arc> *ofst,
                                    const DisambiguateOptions<Arc> &opts) {
  typedef DefaultCommonDivisor<Weight> Div;
  typedef RelationDeterminizeFilter<Arc, CommonFuture> Filt;

  // Subset elements with states s1 and s2 resp. are in this relation
  // iff they there is a path from s1 to a final state that has the
  // same label as some path from s2 to a final state.
  CommonFuture *common_future = new CommonFuture(ifst);

  DeterminizeFstOptions<Arc, Div, Filt> nopts;
  nopts.delta = opts.delta;
  nopts.subsequential_label = opts.subsequential_label;
  nopts.filter = new Filt(ifst, common_future, &head_);
  nopts.gc_limit = 0;  // Cache only the last state for fastest copy.
  if (opts.weight_threshold != Weight::Zero() ||
      opts.state_threshold != kNoStateId) {
    /* TODO(riley): fails regression test; understand why
    if (ifst.Properties(kAcceptor, true)) {
      vector<Weight> idistance, odistance;
      ShortestDistance(ifst, &idistance, true);
      DeterminizeFst<Arc> dfst(ifst, &idistance, &odistance, nopts);
      PruneOptions< Arc, AnyArcFilter<Arc> > popts(opts.weight_threshold,
                                                   opts.state_threshold,
                                                   AnyArcFilter<Arc>(),
                                                   &odistance);
      Prune(dfst, ofst, popts);
      } else */ {
      *ofst = DeterminizeFst<Arc>(ifst, nopts);
      Prune(ofst, opts.weight_threshold, opts.state_threshold);
    }
  } else {
    *ofst = DeterminizeFst<Arc>(ifst, nopts);
  }
  head_.resize(ofst->NumStates(), kNoStateId);
}

template <class Arc> void
Disambiguator<Arc>::FindAmbiguities(const ExpandedFst<Arc> &fst) {
  if (fst.Start() == kNoStateId)
    return;

  candidates_ = new ArcIdMap(ArcIdCompare(head_));

  pair<StateId, StateId> start_pr(fst.Start(), fst.Start());
  coreachable_.insert(start_pr);
  queue_.push_back(start_pr);
  while (!queue_.empty()) {
    const pair<StateId, StateId>  &pr = queue_.front();
    StateId s1 = pr.first;
    StateId s2 = pr.second;
    queue_.pop_front();
    FindAmbiguousPairs(fst, s1, s2);
  }
}

template <class Arc>
void Disambiguator<Arc>::FindAmbiguousPairs(const ExpandedFst<Arc> &fst,
                                            StateId s1, StateId s2) {
  if (fst.NumArcs(s2) > fst.NumArcs(s1))
    FindAmbiguousPairs(fst, s2, s1);

  SortedMatcher < Fst<Arc> > matcher(fst, MATCH_INPUT);
  matcher.SetState(s2);
  for (ArcIterator< Fst<Arc> > aiter(fst, s1);
       !aiter.Done();
       aiter.Next()) {
    const Arc &arc1 = aiter.Value();
    ArcId a1(s1, aiter.Position());
    if (matcher.Find(arc1.ilabel)) {
      for (; !matcher.Done(); matcher.Next()) {
        const Arc &arc2 = matcher.Value();
        if (arc2.ilabel == kNoLabel)  // implicit epsilon match
          continue;
        ArcId a2(s2, matcher.Position());

        // Actual transition is ambiguous
        if (s1 != s2 && arc1.nextstate == arc2.nextstate) {
          pair<ArcId, ArcId> apr;
          if (head_[s1] > head_[s2]) {
            apr = std::make_pair(a1, a2);
          } else {
            apr = std::make_pair(a2, a1);
          }
          candidates_->insert(apr);
        }

        pair<StateId, StateId> spr;
        if (arc1.nextstate <= arc2.nextstate) {
          spr = std::make_pair(arc1.nextstate, arc2.nextstate);
        } else {
          spr = std::make_pair(arc2.nextstate, arc1.nextstate);
        }
        // Not already marked as coreachable?
        if (coreachable_.find(spr) == coreachable_.end()) {
          coreachable_.insert(spr);
          // Only possible if state split by quantization issues
          if (spr.first != spr.second &&
              head_[spr.first] == head_[spr.second]) {
            if (!merge_) {
              merge_ = new UnionFind<StateId>(fst.NumStates(), kNoStateId);
              merge_->MakeAllSet(fst.NumStates());
            }
            merge_->Union(spr.first, spr.second);
          } else {
            queue_.push_back(spr);
          }
        }
      }
    }
  }

  // Super-final transition is ambiguous
  if (s1 != s2 &&
      fst.Final(s1) != Weight::Zero() && fst.Final(s2) != Weight::Zero()) {
    ArcId a1(s1, -1);
    ArcId a2(s2, -1);
    pair<ArcId, ArcId> apr;
    if (head_[s1] > head_[s2]) {
      apr = std::make_pair(a1, a2);
    } else {
      apr = std::make_pair(a2, a1);
    }
    candidates_->insert(apr);
  }
}

template <class Arc>
void Disambiguator<Arc>::MarkAmbiguities() {
  if (!candidates_)
    return;

  for (typename ArcIdMap::iterator it = candidates_->begin();
       it != candidates_->end(); ++it) {
    ArcId a = it->first;
    ArcId b = it->second;
    if (ambiguous_.count(b) == 0)  // if b is not to be removed
      ambiguous_.insert(a);        // ... then a is.
  }
  coreachable_.clear();
  delete candidates_;
  candidates_ = 0;
}

template <class Arc>
void Disambiguator<Arc>::RemoveSplits(MutableFst<Arc> *ofst) {
  if (!merge_)
    return;

  // 'Merges' split states to remove spurious ambiguities
  for (StateId s = 0; s < ofst->NumStates(); ++s) {
    for (MutableArcIterator< MutableFst<Arc> > aiter(ofst, s);
         !aiter.Done();
         aiter.Next()) {
      Arc arc = aiter.Value();
      StateId nextstate = merge_->FindSet(arc.nextstate);
      if (nextstate != arc.nextstate) {
        arc.nextstate = nextstate;
        aiter.SetValue(arc);
      }
    }
  }

  // Repeats search for actual ambiguities on modified FST
  coreachable_.clear();
  delete merge_;
  merge_ = 0;
  delete candidates_;
  candidates_ = 0;
  FindAmbiguities(*ofst);
  if (merge_) {  // shouldn't get here; sanity test
    FSTERROR() << "Disambiguate: unable to remove spurious ambiguities";
    error_ = true;
    return;
  }
}

template <class Arc>
void Disambiguator<Arc>::RemoveAmbiguities(MutableFst<Arc> *ofst) {
  if (ambiguous_.empty())
    return;
  // Adds dead state to redirect ambiguous transitions to be removed
  StateId dead = ofst->AddState();
  for (typename set<ArcId>::iterator it = ambiguous_.begin();
       it != ambiguous_.end();
       ++it) {
    StateId s = it->first;
    ssize_t pos = it->second;
    if (pos >= 0) {  // actual transition
      MutableArcIterator< MutableFst<Arc> > aiter(ofst, s);
      aiter.Seek(pos);
      Arc arc = aiter.Value();
      arc.nextstate = dead;
      aiter.SetValue(arc);
    } else {        // super-final transition
      ofst->SetFinal(s, Weight::Zero());
    }
  }
  Connect(ofst);
  ambiguous_.clear();
}

// Disambiguates a weighted FST. This version writes the
// disambiguated FST to an output MutableFst. The result will
// be an equivalent FST that has the property that there are not
// two distinct paths from the initial state to a final state
// with the same input labeling.
//
// The weights must be (weakly) left divisible (valid for Tropical
// and LogWeight).
//
// Complexity:
// - Disambiguable: exponential (polynomial in the size of the output)
// - Non-disambiguable: does not terminate
//
// The disambiguable transducers include all automata and functional
// transducers that are unweighted or that are acyclic or that are unambiguous.
//
// References:
// - Mehryar Mohri and Michael Riley, "On the Disambiguation
//   of Weighted Automata," ArXiv e-prints, cs-FL/1405.0500, 2014,
//   http://arxiv.org/abs/1405.0500.
template <class Arc>
void Disambiguate(const Fst<Arc> &ifst, MutableFst<Arc> *ofst,
                  const DisambiguateOptions<Arc> &opts
                 = DisambiguateOptions<Arc>()) {
  Disambiguator<Arc> disamb;
  disamb.Disambiguate(ifst, ofst, opts);
}

}  // namespace fst

#endif  // FST_LIB_DISAMBIGUATE_H__
