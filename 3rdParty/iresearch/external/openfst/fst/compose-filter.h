// compose-filter.h

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
// Classes for filtering the composition matches, e.g. for correct epsilon
// handling.

#ifndef FST_LIB_COMPOSE_FILTER_H__
#define FST_LIB_COMPOSE_FILTER_H__

#include <fst/fst.h>
#include <fst/fst-decl.h>  // For optional argument declarations
#include <fst/filter-state.h>
#include <fst/matcher.h>


namespace fst {


// COMPOSITION FILTERS - these determine which matches are allowed to
// proceed. The filter's state is represented by the type
// ComposeFilter::FilterState. The basic filters handle correct
// epsilon matching.  Their interface is:
//
// template <class M1, class M2>
// class ComposeFilter {
//  public:
//   typedef typename M1::FST1 FST1;
//   typedef typename M1::FST2 FST2;
//   typedef typename FST1::Arc Arc;
//   typedef ... FilterState;
//   typedef ... Matcher1;
//   typedef ... Matcher2;
//
//   // Required constructors.
//   ComposeFilter(const FST1 &fst1, const FST2 &fst2,
//   //            M1 *matcher1 = 0, M2 *matcher2 = 0);
//   // If safe=true, the copy is thread-safe. See Fst<>::Copy()
//   // for further doc.
//   ComposeFilter(const ComposeFilter<M1, M2> &filter,
//   //            bool safe = false);
//   // Return start state of filter.
//   FilterState Start() const;
//   // Specifies current composition state.
//   void SetState(StateId s1, StateId s2, const FilterState &f);
//
//   // Apply filter at current composition state to these transitions.
//   // If an arc label to be matched is kNolabel, then that side
//   // does not consume a symbol. Returns the new filter state or,
//   // if disallowed, FilterState::NoState(). The filter is permitted to
//   // modify its inputs, e.g. for optimizations.
//   FilterState FilterArc(Arc *arc1, Arc *arc2) const;

//   // Apply filter at current composition state to these final weights
//   // (cf. superfinal transitions). The filter may modify its inputs,
//   // e.g. for optimizations.
//   void FilterFinal(Weight *final1, Weight *final2) const;
//
//   // Return resp matchers. Ownership stays with filter. These
//   // methods allow the filter to access and possibly modify
//   // the composition matchers (useful e.g. with lookahead).
//   Matcher1 *GetMatcher1();
//   Matcher2 *GetMatcher2();
//
//   // This specifies how the filter affects the composition result
//   // properties. It takes as argument the properties that would
//   // apply with a trivial composition fitler.
//   uint64 Properties(uint64 props) const;
// };


// This filter allows only exact matching of symbols from FST1 with on
// FST2; e.g. no special interpretation of epsilons.  (Template arg
// default declared in fst-decl.h.)
template <class M1, class M2 /* = M1 */>
class NullComposeFilter {
 public:
  typedef typename M1::FST FST1;
  typedef typename M2::FST FST2;
  typedef typename FST1::Arc Arc;
  typedef CharFilterState FilterState;
  typedef M1 Matcher1;
  typedef M2 Matcher2;

  typedef typename Arc::StateId StateId;
  typedef typename Arc::Label Label;
  typedef typename Arc::Weight Weight;

  NullComposeFilter(const FST1 &fst1, const FST2 &fst2,
                         M1 *matcher1 = 0, M2 *matcher2 = 0)
      : matcher1_(matcher1 ? matcher1 : new M1(fst1, MATCH_OUTPUT)),
        matcher2_(matcher2 ? matcher2 : new M2(fst2, MATCH_INPUT)),
        fst1_(matcher1_->GetFst()),
        fst2_(matcher2_->GetFst()) {}

  NullComposeFilter(const NullComposeFilter<M1, M2> &filter,
                         bool safe = false)
      : matcher1_(filter.matcher1_->Copy(safe)),
        matcher2_(filter.matcher2_->Copy(safe)),
        fst1_(matcher1_->GetFst()),
        fst2_(matcher2_->GetFst()) {}

  ~NullComposeFilter() {
    delete matcher1_;
    delete matcher2_;
  }

  FilterState Start() const { return FilterState(0); }

  void SetState(StateId, StateId, const FilterState &) { }

  FilterState FilterArc(Arc *arc1, Arc *arc2) const {
    return (arc1->olabel == kNoLabel || arc2->ilabel == kNoLabel) ?
        FilterState::NoState() : FilterState(0);
  }

  void FilterFinal(Weight *, Weight *) const {}

  // Return resp matchers. Ownership stays with filter.
  Matcher1 *GetMatcher1() { return matcher1_; }
  Matcher2 *GetMatcher2() { return matcher2_; }

  uint64 Properties(uint64 props) const { return props; }

 private:
  Matcher1 *matcher1_;
  Matcher2 *matcher2_;
  const FST1 &fst1_;
  const FST2 &fst2_;

  void operator=(const NullComposeFilter<M1, M2> &);  // disallow
};


// This filter requires epsilons on FST1 to be read before epsilons on FST2.
// (Template arg default declared in fst-decl.h.)
template <class M1, class M2 /* = M1 */>
class SequenceComposeFilter {
 public:
  typedef typename M1::FST FST1;
  typedef typename M2::FST FST2;
  typedef typename FST1::Arc Arc;
  typedef CharFilterState FilterState;
  typedef M1 Matcher1;
  typedef M2 Matcher2;

  typedef typename Arc::StateId StateId;
  typedef typename Arc::Label Label;
  typedef typename Arc::Weight Weight;

  SequenceComposeFilter(const FST1 &fst1, const FST2 &fst2,
                        M1 *matcher1 = 0, M2 *matcher2 = 0)
      : matcher1_(matcher1 ? matcher1 : new M1(fst1, MATCH_OUTPUT)),
        matcher2_(matcher2 ? matcher2 : new M2(fst2, MATCH_INPUT)),
        fst1_(matcher1_->GetFst()),
        s1_(kNoStateId),
        s2_(kNoStateId),
        f_(kNoStateId) {}

  SequenceComposeFilter(const SequenceComposeFilter<M1, M2> &filter,
                        bool safe = false)
      : matcher1_(filter.matcher1_->Copy(safe)),
        matcher2_(filter.matcher2_->Copy(safe)),
        fst1_(matcher1_->GetFst()),
        s1_(kNoStateId),
        s2_(kNoStateId),
        f_(kNoStateId) {}

  ~SequenceComposeFilter() {
    delete matcher1_;
    delete matcher2_;
  }

  FilterState Start() const { return FilterState(0); }

  void SetState(StateId s1, StateId s2, const FilterState &f) {
    if (s1_ == s1 && s2_ == s2 && f == f_)
      return;
    s1_ = s1;
    s2_ = s2;
    f_ = f;
    size_t na1 = internal::NumArcs(fst1_, s1);
    size_t ne1 = internal::NumOutputEpsilons(fst1_, s1);
    bool fin1 = internal::Final(fst1_, s1) != Weight::Zero();
    alleps1_ = na1 == ne1 && !fin1;
    noeps1_ = ne1 == 0;
  }

  FilterState FilterArc(Arc *arc1, Arc *arc2) const {
    if (arc1->olabel == kNoLabel)
      return alleps1_ ? FilterState::NoState() :
        noeps1_ ? FilterState(0) : FilterState(1);
    else if (arc2->ilabel == kNoLabel)
      return f_ != FilterState(0) ? FilterState::NoState() : FilterState(0);
    else
      return arc1->olabel == 0 ? FilterState::NoState() : FilterState(0);
  }

  void FilterFinal(Weight *, Weight *) const {}

  // Return resp matchers. Ownership stays with filter.
  Matcher1 *GetMatcher1() { return matcher1_; }
  Matcher2 *GetMatcher2() { return matcher2_; }

  uint64 Properties(uint64 props) const { return props; }

 private:
  Matcher1 *matcher1_;
  Matcher2 *matcher2_;
  const FST1 &fst1_;
  StateId s1_;     // Current fst1_ state;
  StateId s2_;     // Current fst2_ state;
  FilterState f_;  // Current filter state
  bool alleps1_;   // Only epsilons (and non-final) leaving s1_?
  bool noeps1_;    // No epsilons leaving s1_?

  void operator=(const SequenceComposeFilter<M1, M2> &);  // disallow
};


// This filter requires epsilons on FST2 to be read before epsilons on FST1.
// (Template arg default declared in fst-decl.h.)
template <class M1, class M2 /* = M1 */>
class AltSequenceComposeFilter {
 public:
  typedef typename M1::FST FST1;
  typedef typename M2::FST FST2;
  typedef typename FST1::Arc Arc;
  typedef CharFilterState FilterState;
  typedef M1 Matcher1;
  typedef M2 Matcher2;

  typedef typename Arc::StateId StateId;
  typedef typename Arc::Label Label;
  typedef typename Arc::Weight Weight;

  AltSequenceComposeFilter(const FST1 &fst1, const FST2 &fst2,
                        M1 *matcher1 = 0, M2 *matcher2 = 0)
      : matcher1_(matcher1 ? matcher1 : new M1(fst1, MATCH_OUTPUT)),
        matcher2_(matcher2 ? matcher2 : new M2(fst2, MATCH_INPUT)),
        fst2_(matcher2_->GetFst()),
        s1_(kNoStateId),
        s2_(kNoStateId),
        f_(kNoStateId) {}

  AltSequenceComposeFilter(const AltSequenceComposeFilter<M1, M2> &filter,
                           bool safe = false)
      : matcher1_(filter.matcher1_->Copy(safe)),
        matcher2_(filter.matcher2_->Copy(safe)),
        fst2_(matcher2_->GetFst()),
        s1_(kNoStateId),
        s2_(kNoStateId),
        f_(kNoStateId) {}

  ~AltSequenceComposeFilter() {
    delete matcher1_;
    delete matcher2_;
  }

  FilterState Start() const { return FilterState(0); }

  void SetState(StateId s1, StateId s2, const FilterState &f) {
    if (s1_ == s1 && s2_ == s2 && f == f_)
      return;
    s1_ = s1;
    s2_ = s2;
    f_ = f;
    size_t na2 = internal::NumArcs(fst2_, s2);
    size_t ne2 = internal::NumInputEpsilons(fst2_, s2);
    bool fin2 = internal::Final(fst2_, s2) != Weight::Zero();
    alleps2_ = na2 == ne2 && !fin2;
    noeps2_ = ne2 == 0;
  }

  FilterState FilterArc(Arc *arc1, Arc *arc2) const {
    if (arc2->ilabel == kNoLabel)
      return alleps2_ ? FilterState::NoState() :
        noeps2_ ? FilterState(0) : FilterState(1);
    else if (arc1->olabel == kNoLabel)
      return f_ == FilterState(1) ? FilterState::NoState() : FilterState(0);
    else
      return arc1->olabel == 0 ? FilterState::NoState() : FilterState(0);
  }

  void FilterFinal(Weight *, Weight *) const {}

  // Return resp matchers. Ownership stays with filter.
  Matcher1 *GetMatcher1() { return matcher1_; }
  Matcher2 *GetMatcher2() { return matcher2_; }

  uint64 Properties(uint64 props) const { return props; }

 private:
  Matcher1 *matcher1_;
  Matcher2 *matcher2_;
  const FST2 &fst2_;
  StateId s1_;     // Current fst1_ state;
  StateId s2_;     // Current fst2_ state;
  FilterState f_;  // Current filter state
  bool alleps2_;   // Only epsilons (and non-final) leaving s2_?
  bool noeps2_;    // No epsilons leaving s2_?

void operator=(const AltSequenceComposeFilter<M1, M2> &);  // disallow
};


// This filter requires epsilons on FST1 to be matched with epsilons on FST2
// whenever possible. (Template arg default declared in fst-decl.h.)
template <class M1, class M2 /* = M1 */>
class MatchComposeFilter {
 public:
  typedef typename M1::FST FST1;
  typedef typename M2::FST FST2;
  typedef typename FST1::Arc Arc;
  typedef CharFilterState FilterState;
  typedef M1 Matcher1;
  typedef M2 Matcher2;

  typedef typename Arc::StateId StateId;
  typedef typename Arc::Label Label;
  typedef typename Arc::Weight Weight;

  MatchComposeFilter(const FST1 &fst1, const FST2 &fst2,
                     M1 *matcher1 = 0, M2 *matcher2 = 0)
      : matcher1_(matcher1 ? matcher1 : new M1(fst1, MATCH_OUTPUT)),
        matcher2_(matcher2 ? matcher2 : new M2(fst2, MATCH_INPUT)),
        fst1_(matcher1_->GetFst()),
        fst2_(matcher2_->GetFst()),
        s1_(kNoStateId),
        s2_(kNoStateId),
        f_(kNoStateId) {}

  MatchComposeFilter(const MatchComposeFilter<M1, M2> &filter,
                     bool safe = false)
      : matcher1_(filter.matcher1_->Copy(safe)),
        matcher2_(filter.matcher2_->Copy(safe)),
        fst1_(matcher1_->GetFst()),
        fst2_(matcher2_->GetFst()),
        s1_(kNoStateId),
        s2_(kNoStateId),
        f_(kNoStateId) {}

  ~MatchComposeFilter() {
    delete matcher1_;
    delete matcher2_;
  }

  FilterState Start() const { return FilterState(0); }

  void SetState(StateId s1, StateId s2, const FilterState &f) {
    if (s1_ == s1 && s2_ == s2 && f == f_)
      return;
    s1_ = s1;
    s2_ = s2;
    f_ = f;
    size_t na1 = internal::NumArcs(fst1_, s1);
    size_t ne1 = internal::NumOutputEpsilons(fst1_, s1);
    bool f1 = internal::Final(fst1_, s1) != Weight::Zero();
    alleps1_ = na1 == ne1 && !f1;
    noeps1_ = ne1 == 0;
    size_t na2 = internal::NumArcs(fst2_, s2);
    size_t ne2 = internal::NumInputEpsilons(fst2_, s2);
    bool f2 = internal::Final(fst2_, s2) != Weight::Zero();
    alleps2_ = na2 == ne2 && !f2;
    noeps2_ = ne2 == 0;
  }

  FilterState FilterArc(Arc *arc1, Arc *arc2) const {
    if (arc2->ilabel == kNoLabel)  // Epsilon on Fst1
      return f_ == FilterState(0) ?
          (noeps2_ ? FilterState(0) :
           (alleps2_ ? FilterState::NoState(): FilterState(1))) :
          (f_ == FilterState(1) ? FilterState(1) : FilterState::NoState());
    else if (arc1->olabel == kNoLabel)  // Epsilon on Fst2
      return f_ == FilterState(0) ?
          (noeps1_ ? FilterState(0) :
           (alleps1_ ? FilterState::NoState() : FilterState(2))) :
          (f_ == FilterState(2) ? FilterState(2) : FilterState::NoState());
    else if (arc1->olabel == 0)  // Epsilon on both
      return f_ == FilterState(0) ? FilterState(0) : FilterState::NoState();
    else  // Both are non-epsilons
      return FilterState(0);
  }

  void FilterFinal(Weight *, Weight *) const {}

  // Return resp matchers. Ownership stays with filter.
  Matcher1 *GetMatcher1() { return matcher1_; }
  Matcher2 *GetMatcher2() { return matcher2_; }

  uint64 Properties(uint64 props) const { return props; }

 private:
  Matcher1 *matcher1_;
  Matcher2 *matcher2_;
  const FST1 &fst1_;
  const FST2 &fst2_;
  StateId s1_;              // Current fst1_ state;
  StateId s2_;              // Current fst2_ state;
  FilterState f_;           // Current filter state ID
  bool alleps1_, alleps2_;  // Only epsilons (and non-final) leaving s1, s2?
  bool noeps1_, noeps2_;    // No epsilons leaving s1, s2?

  void operator=(const MatchComposeFilter<M1, M2> &);  // disallow
};


// This filter works with the MultiEpsMatcher to determine if
// 'multi-epsilons' are preserved in the composition output
// (rather than rewritten as 0) and ensures correct properties.
template <class F>
class MultiEpsFilter {
 public:
  typedef typename F::FST1 FST1;
  typedef typename F::FST2 FST2;
  typedef typename F::Arc Arc;
  typedef typename F::Matcher1 Matcher1;
  typedef typename F::Matcher2 Matcher2;
  typedef typename F::FilterState FilterState;
  typedef MultiEpsFilter<F> Filter;

  typedef typename Arc::StateId StateId;
  typedef typename Arc::Label Label;
  typedef typename Arc::Weight Weight;

  MultiEpsFilter(const FST1 &fst1, const FST2 &fst2,
                 Matcher1 *matcher1 = 0,  Matcher2 *matcher2 = 0,
                 bool keep_multi_eps = false)
      : filter_(fst1, fst2, matcher1, matcher2),
        keep_multi_eps_(keep_multi_eps) {}

  MultiEpsFilter(const Filter &filter, bool safe = false)
      : filter_(filter.filter_, safe),
        keep_multi_eps_(filter.keep_multi_eps_) {}

  FilterState Start() const { return filter_.Start(); }

  void SetState(StateId s1, StateId s2, const FilterState &f) {
    return filter_.SetState(s1, s2, f);
  }

  FilterState FilterArc(Arc *arc1, Arc *arc2) const {
    FilterState f = filter_.FilterArc(arc1, arc2);
    if (keep_multi_eps_) {
      if (arc1->olabel == kNoLabel)
        arc1->ilabel = arc2->ilabel;
      if (arc2->ilabel == kNoLabel)
        arc2->olabel = arc1->olabel;
    }
    return f;
  }

  void FilterFinal(Weight *w1, Weight *w2) const {
    return filter_.FilterFinal(w1, w2);
  }

  // Return resp matchers. Ownership stays with filter.
  Matcher1 *GetMatcher1() { return filter_.GetMatcher1(); }
  Matcher2 *GetMatcher2() { return filter_.GetMatcher2(); }

  uint64 Properties(uint64 iprops) const {
    uint64 oprops = filter_.Properties(iprops);
    return oprops & kILabelInvariantProperties & kOLabelInvariantProperties;
  }

 private:
  F filter_;
  bool keep_multi_eps_;
};

}  // namespace fst


#endif  // FST_LIB_COMPOSE_FILTER_H__
