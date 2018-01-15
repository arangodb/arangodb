// lookahead-filter.h

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
// Composition filters to support lookahead matchers, useful for improving
// composition efficiency with certain inputs.

#ifndef FST_LIB_LOOKAHEAD_FILTER_H__
#define FST_LIB_LOOKAHEAD_FILTER_H__

#include <vector>
using std::vector;

#include <fst/filter-state.h>
#include <fst/fst.h>
#include <fst/lookahead-matcher.h>


namespace fst {

// Identifies and verifies the capabilities of the matcher to be used for
// lookahead with the composition filters below. This version is passed
// the matchers.
template <class M1, class M2>
MatchType LookAheadMatchType(const M1 &m1, const M2 &m2) {
  MatchType type1 = m1.Type(false);
  MatchType type2 = m2.Type(false);
  if (type1 == MATCH_OUTPUT &&
      m1.Flags() & kOutputLookAheadMatcher)
    return MATCH_OUTPUT;
  else if (type2 == MATCH_INPUT &&
           m2.Flags() & kInputLookAheadMatcher)
    return MATCH_INPUT;
  else if (m1.Flags() & kOutputLookAheadMatcher &&
           m1.Type(true) == MATCH_OUTPUT)
    return MATCH_OUTPUT;
  else if (m2.Flags() & kInputLookAheadMatcher &&
           m2.Type(true) == MATCH_INPUT)
    return MATCH_INPUT;
  else
    return MATCH_NONE;
}

// Identifies and verifies the capabilities of the matcher to be used for
// lookahead with the composition filters below. This version uses the
// Fst's default matchers.
template <class Arc>
MatchType LookAheadMatchType(const Fst<Arc> &fst1, const Fst<Arc> &fst2) {
  LookAheadMatcher< Fst <Arc> > matcher1(fst1, MATCH_OUTPUT);
  LookAheadMatcher< Fst <Arc> > matcher2(fst2, MATCH_INPUT);
  return LookAheadMatchType(matcher1, matcher2);
}

//
// LookAheadSelector - a helper class for selecting among possibly
// distinct FST and matcher types w/o using a common base class. This
// lets us avoid virtual function calls.
//

// Stores and returns the appropriate FST and matcher for lookahead.
// It is templated on the matcher types.  General case has no methods
// since not currently supported.
template <class M1, class M2, MatchType MT>
class LookAheadSelector {
};

// Stores and returns the appropriate FST and matcher for lookahead.
// Specialized for two matchers of same type with the (match) 'type'
// arg determining which is used for lookahead.
template <class M, MatchType MT>
class LookAheadSelector<M, M, MT> {
 public:
  typedef typename M::Arc Arc;
  typedef typename M::FST F;

  LookAheadSelector(M *lmatcher1, M *lmatcher2, MatchType type)
      : lmatcher1_(lmatcher1->Copy()),
        lmatcher2_(lmatcher2->Copy()),
        type_(type) {}

  LookAheadSelector(const LookAheadSelector<M, M, MT> &selector)
      : lmatcher1_(selector.lmatcher1_->Copy()),
        lmatcher2_(selector.lmatcher2_->Copy()),
        type_(selector.type_) {}

  ~LookAheadSelector() {
    delete lmatcher1_;
    delete lmatcher2_;
  }

  const F &GetFst() const {
    return type_ == MATCH_OUTPUT ? lmatcher2_->GetFst() :
        lmatcher1_->GetFst();
  }

  M *GetMatcher() const {
    return type_ == MATCH_OUTPUT ? lmatcher1_ : lmatcher2_;
  }

 private:
  M *lmatcher1_;
  M *lmatcher2_;
  MatchType type_;

  void operator=(const LookAheadSelector<M, M, MT> &);  // disallow
};

// Stores and returns the appropriate FST and matcher for lookahead.
// Specialized for lookahead on input labels.
template <class M1, class M2>
class LookAheadSelector<M1, M2, MATCH_INPUT> {
 public:
  typedef typename M1::FST F1;

  LookAheadSelector(M1 *lmatcher1, M2 *lmatcher2, MatchType)
      : fst_(lmatcher1->GetFst().Copy()),
        lmatcher_(lmatcher2->Copy()) {}

  LookAheadSelector(const LookAheadSelector<M1, M2, MATCH_INPUT> &selector)
      : fst_(selector.fst_->Copy()),
        lmatcher_(selector.lmatcher_->Copy()) {}

  ~LookAheadSelector() {
    delete lmatcher_;
    delete fst_;
  }

  const F1 &GetFst() const { return *fst_; }

  M2 *GetMatcher() const { return lmatcher_; }

 private:
  const F1 *fst_;
  M2 *lmatcher_;

  void operator=(const LookAheadSelector<M1, M2, MATCH_INPUT> &);  // disallow
};


// Stores and returns the appropriate FST and matcher for lookahead.
// Specialized for lookahead on output labels.
template <class M1, class M2>
class LookAheadSelector<M1, M2, MATCH_OUTPUT> {
 public:
  typedef typename M2::FST F2;

  LookAheadSelector(M1 *lmatcher1, M2 *lmatcher2, MatchType)
      : fst_(lmatcher2->GetFst().Copy()),
        lmatcher_(lmatcher1->Copy()) {}

  LookAheadSelector(const LookAheadSelector<M1, M2, MATCH_OUTPUT> &selector)
      : fst_(selector.fst_->Copy()),
        lmatcher_(selector.lmatcher_->Copy()) {}

  ~LookAheadSelector() {
    delete lmatcher_;
    delete fst_;
  }

  const F2 &GetFst() const { return *fst_; }

  M1 *GetMatcher() const { return lmatcher_; }

 private:
  const F2 *fst_;
  M1 *lmatcher_;

  void operator=(const LookAheadSelector<M1, M2, MATCH_OUTPUT> &);  // disallow
};

// This filter uses a lookahead matcher in FilterArc(arc1, arc2) to
// examine the future of the composition state (arc1.nextstate,
// arc2.nextstate), blocking moving forward when its determined to be
// non-coaccessible. It is templated on an underlying filter,
// typically the epsilon filter.  Which matcher is the lookahead
// matcher is determined by the template argument MT unless it is
// MATCH_BOTH. In that case, both matcher arguments must be lookahead
// matchers of the same type and one will be selected by
// LookAheadMatchType() based on their capability.
template <class F,
          class M1 = LookAheadMatcher<typename F::FST1>,
          class M2 = M1,
          MatchType MT = MATCH_BOTH>
class LookAheadComposeFilter {
 public:
  typedef typename F::FST1 FST1;
  typedef typename F::FST2 FST2;
  typedef typename F::Arc Arc;
  typedef typename F::Matcher1 Matcher1;
  typedef typename F::Matcher2 Matcher2;
  typedef typename F::FilterState FilterState;
  typedef LookAheadComposeFilter<F, M1, M2, MT> Filter;

  typedef typename Arc::StateId StateId;
  typedef typename Arc::Label Label;
  typedef typename Arc::Weight Weight;

  LookAheadComposeFilter(const FST1 &fst1, const FST2 &fst2,
                         M1 *matcher1,  M2 *matcher2)
      : filter_(fst1, fst2, matcher1, matcher2),
        lookahead_type_(MT == MATCH_BOTH ?
                        LookAheadMatchType(*filter_.GetMatcher1(),
                                           *filter_.GetMatcher2()) : MT),
        selector_(filter_.GetMatcher1(), filter_.GetMatcher2(),
                  lookahead_type_),
        flags_(lookahead_type_ == MATCH_OUTPUT ?
               filter_.GetMatcher1()->Flags() :
               filter_.GetMatcher2()->Flags()) {
    if (lookahead_type_ == MATCH_NONE) {
      FSTERROR() << "LookAheadComposeFilter: 1st argument cannot "
                 << "match/look-ahead on output labels and 2nd argument "
                 << "cannot match/look-ahead on input labels.";
    }
    selector_.GetMatcher()->InitLookAheadFst(selector_.GetFst());
  }

  LookAheadComposeFilter(const LookAheadComposeFilter<F, M1, M2, MT> &filter,
                         bool safe = false)
      : filter_(filter.filter_, safe),
        lookahead_type_(filter.lookahead_type_),
        selector_(filter_.GetMatcher1(), filter_.GetMatcher2(),
                  lookahead_type_),
        flags_(filter.flags_) {
    selector_.GetMatcher()->InitLookAheadFst(selector_.GetFst(), true);
  }

  FilterState Start() const {
    return filter_.Start();
  }

  void SetState(StateId s1, StateId s2, const FilterState &f) {
    filter_.SetState(s1, s2, f);
  }

  FilterState FilterArc(Arc *arc1, Arc *arc2) const {
    lookahead_arc_ = false;

    const FilterState &f = filter_.FilterArc(arc1, arc2);
    if (f == FilterState::NoState())
      return FilterState::NoState();

    return LookAheadOutput() ? LookAheadFilterArc(arc1, arc2, f) :
        LookAheadFilterArc(arc2, arc1, f);
  }

  void FilterFinal(Weight *weight1, Weight *weight2) const {
    filter_.FilterFinal(weight1, weight2);
  }

  // Return resp matchers. Ownership stays with filter.
  Matcher1 *GetMatcher1() { return filter_.GetMatcher1(); }
  Matcher2 *GetMatcher2() { return filter_.GetMatcher2(); }

  const LookAheadSelector<Matcher1, Matcher2, MT> &Selector() const {
    return selector_;
  }

  uint64 Properties(uint64 inprops) const {
    uint64 outprops = filter_.Properties(inprops);
    if (lookahead_type_ == MATCH_NONE)
      outprops |= kError;
    return outprops;
  }

  uint32 LookAheadFlags() const { return flags_; }

  bool LookAheadArc() const { return lookahead_arc_; }

  bool LookAheadOutput() const {
    if (MT == MATCH_OUTPUT)
      return true;
    else if (MT == MATCH_INPUT)
      return false;
    else if (lookahead_type_ == MATCH_OUTPUT)
      return true;
    else
      return false;
  }

 private:
  FilterState LookAheadFilterArc(Arc *arca, Arc *arcb,
                                 const FilterState &f) const {
    Label &labela = LookAheadOutput() ? arca->olabel : arca->ilabel;

    if (labela != 0 && !(flags_ & kLookAheadNonEpsilons))
      return f;
    if (labela == 0 && !(flags_ & kLookAheadEpsilons))
      return f;

    lookahead_arc_ = true;
    selector_.GetMatcher()->SetState(arca->nextstate);

    return selector_.GetMatcher()->LookAheadFst(selector_.GetFst(),
                                                arcb->nextstate) ? f :
                                                FilterState::NoState();
  }

  F filter_;                    // Underlying filter
  MatchType lookahead_type_;    // Lookahead match type
  LookAheadSelector<Matcher1, Matcher2, MT> selector_;
  uint32 flags_;                // Lookahead flags
  mutable bool lookahead_arc_;  // Look-ahead performed at last FilterArc()?

  void operator=(const LookAheadComposeFilter<F, M1, M2> &);  // disallow
};


// This filter adds weight-pushing to a lookahead composition filter
// using the LookAheadWeight() method of matcher argument. It is
// templated on an underlying lookahead filter, typically the basic
// lookahead filter. Weight-pushing in composition brings weights
// forward as much as possible based on the lookahead information.
template <class F,
          class M1 = LookAheadMatcher<typename F::FST1>,
          class M2 = M1,
          MatchType MT = MATCH_BOTH>
class PushWeightsComposeFilter {
 public:
  typedef typename F::FST1 FST1;
  typedef typename F::FST2 FST2;
  typedef typename F::Arc Arc;
  typedef typename F::Matcher1 Matcher1;
  typedef typename F::Matcher2 Matcher2;
  typedef typename F::FilterState FilterState1;
  typedef WeightFilterState<typename Arc::Weight> FilterState2;
  typedef PairFilterState<FilterState1, FilterState2> FilterState;

  typedef typename Arc::StateId StateId;
  typedef typename Arc::Label Label;
  typedef typename Arc::Weight Weight;

  PushWeightsComposeFilter(const FST1 &fst1, const FST2 &fst2,
                         M1 *matcher1,  M2 *matcher2)
      : filter_(fst1, fst2, matcher1, matcher2),
        f_(FilterState::NoState()) {}

  PushWeightsComposeFilter(const PushWeightsComposeFilter<F, M1, M2, MT>
                           &filter,
                           bool safe = false)
      : filter_(filter.filter_, safe),
        f_(FilterState::NoState()) {}

  FilterState Start() const {
    return FilterState(filter_.Start(), FilterState2(Weight::One()));
  }

  void SetState(StateId s1, StateId s2, const FilterState &f) {
    f_ = f;
    filter_.SetState(s1, s2, f.GetState1());
  }

  FilterState FilterArc(Arc *arc1, Arc *arc2) const {
    const FilterState1 &f1 = filter_.FilterArc(arc1, arc2);
    if (f1 == FilterState1::NoState())
      return FilterState::NoState();

    if (!(LookAheadFlags() & kLookAheadWeight))
      return FilterState(f1, FilterState2(Weight::One()));

    const Weight &lweight = filter_.LookAheadArc() ?
        Selector().GetMatcher()->LookAheadWeight() : Weight::One();
    const FilterState2 &f2 = f_.GetState2();
    const Weight &fweight = f2.GetWeight();

    if (lweight == Weight::Zero())  // disallows Zero() weight futures
      return FilterState::NoState();

    arc2->weight = Divide(Times(arc2->weight, lweight), fweight);
    return FilterState(f1, FilterState2(lweight.Quantize()));
  }

  void FilterFinal(Weight *weight1, Weight *weight2) const {
    filter_.FilterFinal(weight1, weight2);
    if (!(LookAheadFlags() & kLookAheadWeight) || *weight1 == Weight::Zero())
      return;

    const FilterState2 &f2 = f_.GetState2();
    const Weight &fweight = f2.GetWeight();
    *weight1 = Divide(*weight1, fweight);
  }
  // Return resp matchers. Ownership states with filter.
  Matcher1 *GetMatcher1() { return filter_.GetMatcher1(); }
  Matcher2 *GetMatcher2() { return filter_.GetMatcher2(); }

  const LookAheadSelector<Matcher1, Matcher2, MT> &Selector() const {
    return filter_.Selector();
  }

  uint32 LookAheadFlags() const { return filter_.LookAheadFlags(); }
  bool LookAheadArc() const { return filter_.LookAheadArc(); }
  bool LookAheadOutput() const { return filter_.LookAheadOutput(); }

  uint64 Properties(uint64 props) const {
    return filter_.Properties(props) & kWeightInvariantProperties;
  }

 private:
  F filter_;                       // Underlying filter
  FilterState f_;                  // Current filter state

  void operator=(const PushWeightsComposeFilter<F, M1, M2, MT> &);  // disallow
};

// This filter adds label-pushing to a lookahead composition filter
// using the LookAheadPrefix() method of the matcher argument. It is
// templated on an underlying filter, typically the basic lookahead
// or weight-pushing lookahead filter. Label-pushing in composition
// matches labels as early as possible based on the lookahead
// information.
template <class F,
          class M1 = LookAheadMatcher<typename F::FST1>,
          class M2 = M1,
          MatchType MT = MATCH_BOTH>
class PushLabelsComposeFilter {
 public:
  typedef typename F::FST1 FST1;
  typedef typename F::FST2 FST2;
  typedef typename F::Arc Arc;
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Label Label;
  typedef typename Arc::Weight Weight;

  typedef MultiEpsMatcher<typename F::Matcher1> Matcher1;
  typedef MultiEpsMatcher<typename F::Matcher2> Matcher2;
  typedef typename F::FilterState FilterState1;
  typedef IntegerFilterState<typename Arc::Label> FilterState2;
  typedef PairFilterState<FilterState1, FilterState2> FilterState;

  PushLabelsComposeFilter(const FST1 &fst1, const FST2 &fst2,
                         M1 *matcher1,  M2 *matcher2)
      : filter_(fst1, fst2, matcher1, matcher2),
        f_(FilterState::NoState()),
        fst1_(filter_.GetMatcher1()->GetFst()),
        fst2_(filter_.GetMatcher2()->GetFst()),
        matcher1_(fst1_, MATCH_OUTPUT,
                  filter_.LookAheadOutput() ? kMultiEpsList : kMultiEpsLoop,
                  filter_.GetMatcher1(),
                  false),
        matcher2_(fst2_, MATCH_INPUT,
                  filter_.LookAheadOutput() ? kMultiEpsLoop : kMultiEpsList,
                  filter_.GetMatcher2(),
                  false) {}

  PushLabelsComposeFilter(const PushLabelsComposeFilter<F, M1, M2, MT> &filter,
                          bool safe = false)
      : filter_(filter.filter_, safe),
        f_(FilterState::NoState()),
        fst1_(filter_.GetMatcher1()->GetFst()),
        fst2_(filter_.GetMatcher2()->GetFst()),
        matcher1_(fst1_,  MATCH_OUTPUT,
                  filter_.LookAheadOutput() ? kMultiEpsList : kMultiEpsLoop,
                  filter_.GetMatcher1(),
                  false),
        matcher2_(fst2_, MATCH_INPUT,
                  filter_.LookAheadOutput() ? kMultiEpsLoop : kMultiEpsList,
                  filter_.GetMatcher2(),
                  false) {
  }

  FilterState Start() const {
    return FilterState(filter_.Start(), FilterState2(kNoLabel));
  }

  void SetState(StateId s1, StateId s2, const FilterState &f) {
    f_ = f;
    filter_.SetState(s1, s2, f.GetState1());
    if (!(LookAheadFlags() & kLookAheadPrefix))
      return;

    narcsa_ = LookAheadOutput() ? internal::NumArcs(fst1_, s1)
        : internal::NumArcs(fst2_, s2);

    const FilterState2 &f2 = f_.GetState2();
    const Label &flabel = f2.GetState();

    GetMatcher1()->ClearMultiEpsLabels();
    GetMatcher2()->ClearMultiEpsLabels();
    if (flabel != kNoLabel) {                   // Have a lookahead label?
      GetMatcher1()->AddMultiEpsLabel(flabel);  // Yes, make it a multi-epsilon
      GetMatcher2()->AddMultiEpsLabel(flabel);  // label so that it matches the
    }                                           // implicit epsilon arc to be
  }                                             // modified below when pushing.

  FilterState FilterArc(Arc *arc1, Arc *arc2) const {
    if (!(LookAheadFlags() & kLookAheadPrefix))
      return FilterState(filter_.FilterArc(arc1, arc2),
                         FilterState2(kNoLabel));

    const FilterState2 &f2 = f_.GetState2();
    const Label &flabel = f2.GetState();
    if (flabel != kNoLabel)             // Have a lookahead label?
      return LookAheadOutput() ? PushedLabelFilterArc(arc1, arc2, flabel) :
          PushedLabelFilterArc(arc2, arc1, flabel);

    const FilterState1 &f1 = filter_.FilterArc(arc1, arc2);
    if (f1 == FilterState1::NoState())
      return FilterState::NoState();

    if (!filter_.LookAheadArc())
      return FilterState(f1, FilterState2(kNoLabel));

    return LookAheadOutput() ? PushLabelFilterArc(arc1, arc2, f1) :
        PushLabelFilterArc(arc2, arc1, f1);
  }

  void FilterFinal(Weight *weight1, Weight *weight2) const {
    filter_.FilterFinal(weight1, weight2);
    if (!(LookAheadFlags() & kLookAheadPrefix) ||
        *weight1 == Weight::Zero())
      return;

    const FilterState2 &f2 = f_.GetState2();
    const Label &flabel = f2.GetState();
    if (flabel != kNoLabel)
      *weight1 = Weight::Zero();
  }

  // Return resp matchers. Ownership states with filter.
  Matcher1 *GetMatcher1() { return &matcher1_; }
  Matcher2 *GetMatcher2() { return &matcher2_; }

  uint64 Properties(uint64 iprops) const {
    uint64 oprops = filter_.Properties(iprops);
    if (LookAheadOutput())
      return oprops & kOLabelInvariantProperties;
    else
      return oprops & kILabelInvariantProperties;
  }

 private:
  const LookAheadSelector<typename F::Matcher1, typename F::Matcher2, MT>
  &Selector() const {
    return filter_.Selector();
  }

  // Consumes an already pushed label.
  FilterState PushedLabelFilterArc(Arc *arca, Arc *arcb,
                                   Label flabel) const {
    Label &labela = LookAheadOutput() ? arca->olabel : arca->ilabel;
    const Label &labelb = LookAheadOutput() ? arcb->ilabel : arcb->olabel;

    if (labelb != kNoLabel) {
      return FilterState::NoState();    // Block non- (multi-) epsilon label
    } else if (labela == flabel) {
      labela = 0;                       // Convert match to multi-eps to eps
      return Start();
    } else if (labela == 0) {
      if (narcsa_ == 1)
        return f_;                      // Take eps; keep state w/ label
      Selector().GetMatcher()->SetState(arca->nextstate);
      if (Selector().GetMatcher()->LookAheadLabel(flabel))
        return f_;                      // Take eps; keep state w/ label
      else
        return FilterState::NoState();  // Block non-coaccessible path
    } else {
      return FilterState::NoState();    // Block mismatch to multi-eps label
    }
  }

  // Pushes a label forward when possible.
  FilterState PushLabelFilterArc(Arc *arca, Arc *arcb,
                                 const FilterState1 &f1) const {
    Label &labela = LookAheadOutput() ? arca->olabel : arca->ilabel;
    const Label &labelb = LookAheadOutput() ? arcb->olabel : arcb->ilabel;

    if (labelb != 0)                   // No place to push.
      return FilterState(f1, FilterState2(kNoLabel));
    if (labela != 0 &&                 // Wrong lookahead prefix type?
        LookAheadFlags() & kLookAheadNonEpsilonPrefix)
      return FilterState(f1, FilterState2(kNoLabel));

    Arc larc(kNoLabel, kNoLabel, Weight::Zero(), kNoStateId);

    if (Selector().GetMatcher()->LookAheadPrefix(&larc)) {  // Have prefix arc?
      labela = LookAheadOutput() ? larc.ilabel : larc.olabel;
      arcb->ilabel = larc.ilabel;      // Yes, go forward on that arc,
      arcb->olabel = larc.olabel;      // thus pushing the label.
      arcb->weight = Times(arcb->weight, larc.weight);
      arcb->nextstate = larc.nextstate;
      return FilterState(f1, FilterState2(labela));
    } else {
      return FilterState(f1, FilterState2(kNoLabel));
    }
  }

  uint32 LookAheadFlags() const { return filter_.LookAheadFlags(); }
  bool LookAheadArc() const { return filter_.LookAheadArc(); }
  bool LookAheadOutput() const { return filter_.LookAheadOutput(); }

  F filter_;                  // Underlying filter
  FilterState f_ ;            // Current filter state
  const FST1 &fst1_;
  const FST2 &fst2_;
  Matcher1 matcher1_;         // Multi-epsilon matcher for fst1
  Matcher2 matcher2_;         // Multi-epsilon matcher for fst2
  ssize_t narcsa_;            // Number of arcs leaving look-ahead match FST

  void operator=(const PushLabelsComposeFilter<F, M1, M2, MT> &);  // disallow
};

//
// CONVENIENCE CLASS useful for setting up composition with a default
// look-ahead matcher and filter.
//

template <class A, MatchType type>  // MATCH_NONE
class DefaultLookAhead {
 public:
  typedef Matcher< Fst<A> > M;
  typedef SequenceComposeFilter<M> ComposeFilter;
  typedef M FstMatcher;
};

// Specializes for MATCH_INPUT to allow lookahead.
template <class A>
class DefaultLookAhead<A, MATCH_INPUT> {
 public:
  typedef LookAheadMatcher< Fst<A> > M;
  typedef SequenceComposeFilter<M> SF;
  typedef LookAheadComposeFilter<SF, M> ComposeFilter;
  typedef M FstMatcher;
};

// Specializes for MATCH_OUTPUT to allow lookahead.
template <class A>
class DefaultLookAhead<A, MATCH_OUTPUT> {
 public:
  typedef LookAheadMatcher< Fst<A> > M;
  typedef AltSequenceComposeFilter<M> SF;
  typedef LookAheadComposeFilter<SF, M> ComposeFilter;
  typedef M FstMatcher;
};

// Specializes for StdArc to allow weight and label pushing.
template <>
class DefaultLookAhead<StdArc, MATCH_INPUT> {
 public:
  typedef StdArc A;
  typedef LookAheadMatcher< Fst<A> > M;
  typedef SequenceComposeFilter<M> SF;
  typedef LookAheadComposeFilter<SF, M> LF;
  typedef PushWeightsComposeFilter<LF, M> WF;
  typedef PushLabelsComposeFilter<WF, M> ComposeFilter;
  typedef M FstMatcher;
};

// Specializes for StdArc to allow weight and label pushing.
template <>
class DefaultLookAhead<StdArc, MATCH_OUTPUT> {
 public:
  typedef StdArc A;
  typedef LookAheadMatcher< Fst<A> > M;
  typedef AltSequenceComposeFilter<M> SF;
  typedef LookAheadComposeFilter<SF, M>  LF;
  typedef PushWeightsComposeFilter<LF, M> WF;
  typedef PushLabelsComposeFilter<WF, M> ComposeFilter;
  typedef M FstMatcher;
};

// Specializes for LogArc to allow weight and label pushing.
template <>
class DefaultLookAhead<LogArc, MATCH_INPUT> {
 public:
  typedef LogArc A;
  typedef LookAheadMatcher< Fst<A> > M;
  typedef SequenceComposeFilter<M> SF;
  typedef LookAheadComposeFilter<SF, M> LF;
  typedef PushWeightsComposeFilter<LF, M> WF;
  typedef PushLabelsComposeFilter<WF, M> ComposeFilter;
  typedef M FstMatcher;
};

// Specializes for LogArc to allow weight and label pushing.
template <>
class DefaultLookAhead<LogArc, MATCH_OUTPUT> {
 public:
  typedef LogArc A;
  typedef LookAheadMatcher< Fst<A> > M;
  typedef AltSequenceComposeFilter<M> SF;
  typedef LookAheadComposeFilter<SF, M> LF;
  typedef PushWeightsComposeFilter<LF, M> WF;
  typedef PushLabelsComposeFilter<WF, M> ComposeFilter;
  typedef M FstMatcher;
};

}  // namespace fst

#endif  // FST_LIB_LOOKAHEAD_FILTER_H__
