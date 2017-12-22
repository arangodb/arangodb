// lookahead-matcher.h

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
// Classes to add lookahead to FST matchers, useful e.g. for improving
// composition efficiency with certain inputs.

#ifndef FST_LIB_LOOKAHEAD_MATCHER_H__
#define FST_LIB_LOOKAHEAD_MATCHER_H__

#include <fst/add-on.h>
#include <fst/const-fst.h>
#include <fst/fst.h>
#include <fst/label-reachable.h>
#include <fst/matcher.h>


DECLARE_string(save_relabel_ipairs);
DECLARE_string(save_relabel_opairs);

namespace fst {

// LOOKAHEAD MATCHERS - these have the interface of Matchers (see
// matcher.h) and these additional methods:
//
// template <class F>
// class LookAheadMatcher {
//  public:
//   typedef F FST;
//   typedef F::Arc Arc;
//   typedef typename Arc::StateId StateId;
//   typedef typename Arc::Label Label;
//   typedef typename Arc::Weight Weight;
//
//  // Required constructors.
//  LookAheadMatcher(const F &fst, MatchType match_type);
//   // If safe=true, the copy is thread-safe (except the lookahead Fst is
//   // preserved). See Fst<>::Cop() for further doc.
//  LookAheadMatcher(const LookAheadMatcher &matcher, bool safe = false);
//
//  Below are methods for looking ahead for a match to a label and
//  more generally, to a rational set. Each returns false if there is
//  definitely not a match and returns true if there possibly is a
//  match.

//  // LABEL LOOKAHEAD: Can 'label' be read from the current matcher state
//  // after possibly following epsilon transitions?
//  bool LookAheadLabel(Label label) const;
//
//  // RATIONAL LOOKAHEAD: The next methods allow looking ahead for an
//  // arbitrary rational set of strings, specified by an FST and a state
//  // from which to begin the matching. If the lookahead FST is a
//  // transducer, this looks on the side different from the matcher
//  // 'match_type' (cf. composition).
//
//  // Are there paths P from 's' in the lookahead FST that can be read from
//  // the cur. matcher state?
//  bool LookAheadFst(const Fst<Arc>& fst, StateId s);
//
//  // Gives an estimate of the combined weight of the paths P in the
//  // lookahead and matcher FSTs for the last call to LookAheadFst.
//  // A trivial implementation returns Weight::One(). Non-trivial
//  // implementations are useful for weight-pushing in composition.
//  Weight LookAheadWeight() const;
//
//  // Is there is a single non-epsilon arc found in the lookahead FST
//  // that begins P (after possibly following any epsilons) in the last
//  // call LookAheadFst? If so, return true and copy it to '*arc', o.w.
//  // return false. A trivial implementation returns false. Non-trivial
//  // implementations are useful for label-pushing in composition.
//  bool LookAheadPrefix(Arc *arc);
//
//  // Optionally pre-specifies the lookahead FST that will be passed
//  // to LookAheadFst() for possible precomputation. If copy is true,
//  // then 'fst' is a copy of the FST used in the previous call to
//  // this method (useful to avoid unnecessary updates).
//  void InitLookAheadFst(const Fst<Arc>& fst, bool copy = false);
//
// };

//
// LOOK-AHEAD FLAGS (see also kMatcherFlags in matcher.h):
//
// Matcher is a lookahead matcher when 'match_type' is MATCH_INPUT.
const uint32 kInputLookAheadMatcher =     0x00000010;

// Matcher is a lookahead matcher when 'match_type' is MATCH_OUTPUT.
const uint32 kOutputLookAheadMatcher =    0x00000020;

// A non-trivial implementation of LookAheadWeight() method defined and
// should be used?
const uint32 kLookAheadWeight =           0x00000040;

// A non-trivial implementation of LookAheadPrefix() method defined and
// should be used?
const uint32 kLookAheadPrefix =           0x00000080;

// Look-ahead of matcher FST non-epsilon arcs?
const uint32 kLookAheadNonEpsilons =      0x00000100;

// Look-ahead of matcher FST epsilon arcs?
const uint32 kLookAheadEpsilons =         0x00000200;

// Ignore epsilon paths for the lookahead prefix? Note this gives
// correct results in composition only with an appropriate composition
// filter since it depends on the filter blocking the ignored paths.
const uint32 kLookAheadNonEpsilonPrefix = 0x00000400;

// For LabelLookAheadMatcher, save relabeling data to file
const uint32 kLookAheadKeepRelabelData =  0x00000800;

// Flags used for lookahead matchers.
const uint32 kLookAheadFlags =            0x00000ff0;

// LookAhead Matcher interface, templated on the Arc definition; used
// for lookahead matcher specializations that are returned by the
// InitMatcher() Fst method.
template <class A>
class LookAheadMatcherBase : public MatcherBase<A> {
 public:
  typedef A Arc;
  typedef typename A::StateId StateId;
  typedef typename A::Label Label;
  typedef typename A::Weight Weight;

  LookAheadMatcherBase()
  : weight_(Weight::One()),
    prefix_arc_(kNoLabel, kNoLabel, Weight::One(), kNoStateId) {}

  virtual ~LookAheadMatcherBase() {}

  bool LookAheadLabel(Label label) const { return LookAheadLabel_(label); }

  bool LookAheadFst(const Fst<Arc> &fst, StateId s) {
    return LookAheadFst_(fst, s);
  }

  Weight LookAheadWeight() const { return weight_; }

  bool LookAheadPrefix(Arc *arc) const {
    if (prefix_arc_.nextstate != kNoStateId) {
      *arc = prefix_arc_;
      return true;
    } else {
      return false;
    }
  }

  virtual void InitLookAheadFst(const Fst<Arc>& fst, bool copy = false) = 0;

 protected:
  void SetLookAheadWeight(const Weight &w) { weight_ = w; }

  void SetLookAheadPrefix(const Arc &arc) { prefix_arc_ = arc; }

  void ClearLookAheadPrefix() { prefix_arc_.nextstate = kNoStateId; }

 private:
  virtual bool LookAheadLabel_(Label label) const = 0;
  virtual bool LookAheadFst_(const Fst<Arc> &fst,
                             StateId s) = 0;  // This must set l.a. weight and
                                              // prefix if non-trivial.
  Weight weight_;                             // Look-ahead weight
  Arc prefix_arc_;                            // Look-ahead prefix arc
};


// Don't really lookahead, just declare future looks good regardless.
template <class M>
class TrivialLookAheadMatcher
    : public LookAheadMatcherBase<typename M::FST::Arc> {
 public:
  typedef typename M::FST FST;
  typedef typename M::Arc Arc;
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Label Label;
  typedef typename Arc::Weight Weight;

  TrivialLookAheadMatcher(const FST &fst, MatchType match_type)
      : matcher_(fst, match_type) {}

  TrivialLookAheadMatcher(const TrivialLookAheadMatcher<M> &lmatcher,
                          bool safe = false)
      : matcher_(lmatcher.matcher_, safe) {}

  // General matcher methods
  TrivialLookAheadMatcher<M> *Copy(bool safe = false) const {
    return new TrivialLookAheadMatcher<M>(*this, safe);
  }

  MatchType Type(bool test) const { return matcher_.Type(test); }
  void SetState(StateId s) { return matcher_.SetState(s); }
  bool Find(Label label) { return matcher_.Find(label); }
  bool Done() const { return matcher_.Done(); }
  const Arc& Value() const { return matcher_.Value(); }
  void Next() { matcher_.Next(); }
  Weight Final(StateId s) const { return matcher_.Final(s); }
  ssize_t Priority(StateId s) { return matcher_.Priority(s); }
  virtual const FST &GetFst() const { return matcher_.GetFst(); }
  uint64 Properties(uint64 props) const { return matcher_.Properties(props); }
  uint32 Flags() const {
    return matcher_.Flags() | kInputLookAheadMatcher | kOutputLookAheadMatcher;
  }

  // Look-ahead methods.
  bool LookAheadLabel(Label label) const { return true;  }
  bool LookAheadFst(const Fst<Arc> &fst, StateId s) {return true; }
  Weight LookAheadWeight() const { return Weight::One(); }
  bool LookAheadPrefix(Arc *arc) const { return false; }
  void InitLookAheadFst(const Fst<Arc>& fst, bool copy = false) {}

 private:
  // This allows base class virtual access to non-virtual derived-
  // class members of the same name. It makes the derived class more
  // efficient to use but unsafe to further derive.
  virtual void SetState_(StateId s) { SetState(s); }
  virtual bool Find_(Label label) { return Find(label); }
  virtual bool Done_() const { return Done(); }
  virtual const Arc& Value_() const { return Value(); }
  virtual void Next_() { Next(); }
  virtual Weight Final_(StateId s) const { return Final(s); }
  virtual ssize_t Priority_(StateId s) { return Priority(s); }

  bool LookAheadLabel_(Label l) const { return LookAheadLabel(l); }

  bool LookAheadFst_(const Fst<Arc> &fst, StateId s) {
    return LookAheadFst(fst, s);
  }

  Weight LookAheadWeight_() const { return LookAheadWeight(); }
  bool LookAheadPrefix_(Arc *arc) const { return LookAheadPrefix(arc); }

  M matcher_;
};

// Look-ahead of one transition. Template argument F accepts flags to
// control behavior.
template <class M, uint32 F = kLookAheadNonEpsilons | kLookAheadEpsilons |
          kLookAheadWeight | kLookAheadPrefix>
class ArcLookAheadMatcher
    : public LookAheadMatcherBase<typename M::FST::Arc> {
 public:
  typedef typename M::FST FST;
  typedef typename M::Arc Arc;
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Label Label;
  typedef typename Arc::Weight Weight;
  typedef NullAddOn MatcherData;

  using LookAheadMatcherBase<Arc>::LookAheadWeight;
  using LookAheadMatcherBase<Arc>::SetLookAheadPrefix;
  using LookAheadMatcherBase<Arc>::SetLookAheadWeight;
  using LookAheadMatcherBase<Arc>::ClearLookAheadPrefix;

  ArcLookAheadMatcher(const FST &fst, MatchType match_type,
                      MatcherData *data = 0)
      : matcher_(fst, match_type),
        fst_(matcher_.GetFst()),
        lfst_(0),
        s_(kNoStateId) {}

  ArcLookAheadMatcher(const ArcLookAheadMatcher<M, F> &lmatcher,
                      bool safe = false)
      : matcher_(lmatcher.matcher_, safe),
        fst_(matcher_.GetFst()),
        lfst_(lmatcher.lfst_),
        s_(kNoStateId) {}

  // General matcher methods
  ArcLookAheadMatcher<M, F> *Copy(bool safe = false) const {
    return new ArcLookAheadMatcher<M, F>(*this, safe);
  }

  MatchType Type(bool test) const { return matcher_.Type(test); }

  void SetState(StateId s) {
    s_ = s;
    matcher_.SetState(s);
  }

  bool Find(Label label) { return matcher_.Find(label); }
  bool Done() const { return matcher_.Done(); }
  const Arc& Value() const { return matcher_.Value(); }
  void Next() { matcher_.Next(); }
  Weight Final(StateId s) const { return matcher_.Final(s); }
  ssize_t Priority(StateId s) { return matcher_.Priority(s); }
  const FST &GetFst() const { return fst_; }
  uint64 Properties(uint64 props) const { return matcher_.Properties(props); }
  uint32 Flags() const {
    return matcher_.Flags() | kInputLookAheadMatcher |
        kOutputLookAheadMatcher | F;
  }

  // Writable matcher methods
  MatcherData *GetData() const { return 0; }

  // Look-ahead methods.
  bool LookAheadLabel(Label label) const { return matcher_.Find(label); }

  // Checks if there is a matching (possibly super-final) transition
  // at (s_, s).
  bool LookAheadFst(const Fst<Arc> &fst, StateId s);

  void InitLookAheadFst(const Fst<Arc>& fst, bool copy = false) {
    lfst_ = &fst;
  }

 private:
  // This allows base class virtual access to non-virtual derived-
  // class members of the same name. It makes the derived class more
  // efficient to use but unsafe to further derive.
  virtual void SetState_(StateId s) { SetState(s); }
  virtual bool Find_(Label label) { return Find(label); }
  virtual bool Done_() const { return Done(); }
  virtual const Arc& Value_() const { return Value(); }
  virtual void Next_() { Next(); }
  virtual Weight Final_(StateId s) const { return Final(s); }
  virtual ssize_t Priority_(StateId s) { return Priority(s); }

  bool LookAheadLabel_(Label l) const { return LookAheadLabel(l); }
  bool LookAheadFst_(const Fst<Arc> &fst, StateId s) {
    return LookAheadFst(fst, s);
  }

  mutable M matcher_;
  const FST &fst_;         // Matcher FST
  const Fst<Arc> *lfst_;   // Look-ahead FST
  StateId s_;              // Matcher state
};

template <class M, uint32 F>
bool ArcLookAheadMatcher<M, F>::LookAheadFst(const Fst<Arc> &fst, StateId s) {
  if (&fst != lfst_)
    InitLookAheadFst(fst);

  bool ret = false;
  ssize_t nprefix = 0;
  if (F & kLookAheadWeight)
    SetLookAheadWeight(Weight::Zero());
  if (F & kLookAheadPrefix)
    ClearLookAheadPrefix();
  if (fst_.Final(s_) != Weight::Zero() &&
      lfst_->Final(s) != Weight::Zero()) {
    if (!(F & (kLookAheadWeight | kLookAheadPrefix)))
      return true;
    ++nprefix;
    if (F & kLookAheadWeight)
      SetLookAheadWeight(Plus(LookAheadWeight(),
                              Times(fst_.Final(s_), lfst_->Final(s))));
    ret = true;
  }
  if (matcher_.Find(kNoLabel)) {
    if (!(F & (kLookAheadWeight | kLookAheadPrefix)))
      return true;
    ++nprefix;
    if (F & kLookAheadWeight)
      for (; !matcher_.Done(); matcher_.Next())
        SetLookAheadWeight(Plus(LookAheadWeight(), matcher_.Value().weight));
    ret = true;
  }
  for (ArcIterator< Fst<Arc> > aiter(*lfst_, s);
       !aiter.Done();
       aiter.Next()) {
    const Arc &arc = aiter.Value();
    Label label = kNoLabel;
    switch (matcher_.Type(false)) {
      case MATCH_INPUT:
        label = arc.olabel;
        break;
      case MATCH_OUTPUT:
        label = arc.ilabel;
        break;
      default:
        FSTERROR() << "ArcLookAheadMatcher::LookAheadFst: bad match type";
        return true;
    }
    if (label == 0) {
      if (!(F & (kLookAheadWeight | kLookAheadPrefix)))
        return true;
      if (!(F & kLookAheadNonEpsilonPrefix))
        ++nprefix;
      if (F & kLookAheadWeight)
        SetLookAheadWeight(Plus(LookAheadWeight(), arc.weight));
      ret = true;
    } else if (matcher_.Find(label)) {
      if (!(F & (kLookAheadWeight | kLookAheadPrefix)))
        return true;
      for (; !matcher_.Done(); matcher_.Next()) {
        ++nprefix;
        if (F & kLookAheadWeight)
          SetLookAheadWeight(Plus(LookAheadWeight(),
                                  Times(arc.weight,
                                        matcher_.Value().weight)));
        if ((F & kLookAheadPrefix) && nprefix == 1)
          SetLookAheadPrefix(arc);
      }
      ret = true;
    }
  }
  if (F & kLookAheadPrefix) {
    if (nprefix == 1)
      SetLookAheadWeight(Weight::One());  // Avoids double counting.
    else
      ClearLookAheadPrefix();
  }
  return ret;
}


// Template argument F accepts flags to control behavior.
// It must include precisely one of KInputLookAheadMatcher or
// KOutputLookAheadMatcher.
template <class M, uint32 F = kLookAheadEpsilons | kLookAheadWeight |
          kLookAheadPrefix | kLookAheadNonEpsilonPrefix |
          kLookAheadKeepRelabelData,
          class S = DefaultAccumulator<typename M::Arc>,
          class R = LabelReachable<typename M::Arc, S> >
class LabelLookAheadMatcher
    : public LookAheadMatcherBase<typename M::FST::Arc> {
 public:
  typedef typename M::FST FST;
  typedef typename M::Arc Arc;
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Label Label;
  typedef typename Arc::Weight Weight;
  typedef typename R::Data MatcherData;

  using LookAheadMatcherBase<Arc>::LookAheadWeight;
  using LookAheadMatcherBase<Arc>::SetLookAheadPrefix;
  using LookAheadMatcherBase<Arc>::SetLookAheadWeight;
  using LookAheadMatcherBase<Arc>::ClearLookAheadPrefix;

  LabelLookAheadMatcher(const FST &fst, MatchType match_type,
                        MatcherData *data = 0, S *s = 0)
      : matcher_(fst, match_type),
        lfst_(0),
        label_reachable_(0),
        s_(kNoStateId),
        error_(false) {
    if (!(F & (kInputLookAheadMatcher | kOutputLookAheadMatcher))) {
      FSTERROR() << "LabelLookaheadMatcher: bad matcher flags: " << F;
      error_ = true;
    }
    bool reach_input = match_type == MATCH_INPUT;
    if (data) {
      if (reach_input == data->ReachInput())
        label_reachable_ = new R(data, s);
    } else if ((reach_input && (F & kInputLookAheadMatcher)) ||
               (!reach_input && (F & kOutputLookAheadMatcher))) {
      label_reachable_ = new R(
          fst, reach_input, s, F & kLookAheadKeepRelabelData);
    }
  }

  LabelLookAheadMatcher(const LabelLookAheadMatcher<M, F, S, R> &lmatcher,
                        bool safe = false)
      : matcher_(lmatcher.matcher_, safe),
        lfst_(lmatcher.lfst_),
        label_reachable_(
            lmatcher.label_reachable_ ?
            new R(*lmatcher.label_reachable_, safe) : 0),
        s_(kNoStateId),
        error_(lmatcher.error_) {}

  ~LabelLookAheadMatcher() {
    delete label_reachable_;
  }

  // General matcher methods
  LabelLookAheadMatcher<M, F, S, R> *Copy(bool safe = false) const {
    return new LabelLookAheadMatcher<M, F, S, R>(*this, safe);
  }

  MatchType Type(bool test) const { return matcher_.Type(test); }

  void SetState(StateId s) {
    if (s_ == s)
      return;
    s_ = s;
    match_set_state_ = false;
    reach_set_state_ = false;
  }

  bool Find(Label label) {
    if (!match_set_state_) {
      matcher_.SetState(s_);
      match_set_state_ = true;
    }
    return matcher_.Find(label);
  }

  bool Done() const { return matcher_.Done(); }
  const Arc& Value() const { return matcher_.Value(); }
  void Next() { matcher_.Next(); }
  ssize_t Priority(StateId s) { return matcher_.Priority(s); }
  Weight Final(StateId s) const { return matcher_.Final(s); }
  const FST &GetFst() const { return matcher_.GetFst(); }

  uint64 Properties(uint64 inprops) const {
    uint64 outprops = matcher_.Properties(inprops);
    if (error_ || (label_reachable_ && label_reachable_->Error()))
      outprops |= kError;
    return outprops;
  }

  uint32 Flags() const {
    if (label_reachable_ && label_reachable_->GetData()->ReachInput())
      return matcher_.Flags() | F | kInputLookAheadMatcher;
    else if (label_reachable_ && !label_reachable_->GetData()->ReachInput())
      return matcher_.Flags() | F | kOutputLookAheadMatcher;
    else
      return matcher_.Flags();
  }

  // Writable matcher methods
  MatcherData *GetData() const {
    return label_reachable_ ? label_reachable_->GetData() : 0;
  };

  // Look-ahead methods.
  bool LookAheadLabel(Label label) const {
    if (label == 0)
      return true;

    if (label_reachable_) {
      if (!reach_set_state_) {
        label_reachable_->SetState(s_);
        reach_set_state_ = true;
      }
      return label_reachable_->Reach(label);
    } else {
      return true;
    }
  }

  // Checks if there is a matching (possibly super-final) transition
  // at (s_, s).
  template <class L>
  bool LookAheadFst(const L &fst, StateId s);

  void InitLookAheadFst(const Fst<Arc>& fst, bool copy = false) {
    lfst_ = &fst;
    if (label_reachable_) {
      bool reach_input = Type(false) == MATCH_OUTPUT;
      label_reachable_->ReachInit(fst, reach_input, copy);
    }
  }

  template <class L>
  void InitLookAheadFst(const L& fst, bool copy = false) {
    lfst_ = static_cast<const Fst<Arc> *>(&fst);
    if (label_reachable_) {
      bool reach_input = Type(false) == MATCH_OUTPUT;
      label_reachable_->ReachInit(fst, reach_input, copy);
    }
  }

 private:
  // This allows base class virtual access to non-virtual derived-
  // class members of the same name. It makes the derived class more
  // efficient to use but unsafe to further derive.
  virtual void SetState_(StateId s) { SetState(s); }
  virtual bool Find_(Label label) { return Find(label); }
  virtual bool Done_() const { return Done(); }
  virtual const Arc& Value_() const { return Value(); }
  virtual void Next_() { Next(); }
  virtual Weight Final_(StateId s) const { return Final(s); }
  virtual ssize_t Priority_(StateId s) { return Priority(s); }

  bool LookAheadLabel_(Label l) const { return LookAheadLabel(l); }
  bool LookAheadFst_(const Fst<Arc> &fst, StateId s) {
    return LookAheadFst(fst, s);
  }

  mutable M matcher_;
  const Fst<Arc> *lfst_;                     // Look-ahead FST
  R *label_reachable_;                       // Label reachability info
  StateId s_;                                // Matcher state
  bool match_set_state_;                     // matcher_.SetState called?
  mutable bool reach_set_state_;             // reachable_.SetState called?
  bool error_;
};

template <class M, uint32 F, class S, class R>
template <class L> inline
bool LabelLookAheadMatcher<M, F, S, R>::LookAheadFst(const L &fst, StateId s) {
  if (static_cast<const Fst<Arc> *>(&fst) != lfst_)
    InitLookAheadFst(fst);

  SetLookAheadWeight(Weight::One());
  ClearLookAheadPrefix();

  if (!label_reachable_)
    return true;

  label_reachable_->SetState(s_, s);
  reach_set_state_ = true;

  bool compute_weight = F & kLookAheadWeight;
  bool compute_prefix = F & kLookAheadPrefix;

  ArcIterator<L> aiter(fst, s);
  aiter.SetFlags(kArcNoCache, kArcNoCache);  // make caching optional;
  bool reach_arc = label_reachable_->Reach(&aiter, 0,
                                           internal::NumArcs(*lfst_, s),
                                           compute_weight);
  Weight lfinal = internal::Final(*lfst_, s);
  bool reach_final = lfinal != Weight::Zero() && label_reachable_->ReachFinal();
  if (reach_arc) {
    ssize_t begin = label_reachable_->ReachBegin();
    ssize_t end = label_reachable_->ReachEnd();
    if (compute_prefix && end - begin == 1 && !reach_final) {
      aiter.Seek(begin);
      SetLookAheadPrefix(aiter.Value());
      compute_weight = false;
    } else if (compute_weight) {
      SetLookAheadWeight(label_reachable_->ReachWeight());
    }
  }
  if (reach_final && compute_weight)
    SetLookAheadWeight(reach_arc ?
                       Plus(LookAheadWeight(), lfinal) : lfinal);

  return reach_arc || reach_final;
}


// Label-lookahead relabeling class.
template <class A, class R = LabelReachableData<typename A::Label> >
class LabelLookAheadRelabeler {
 public:
  typedef typename A::Label Label;
  typedef R MatcherData;
  typedef AddOnPair<MatcherData, MatcherData> D;
  typedef LabelReachable<A, DefaultAccumulator<A>, R> Reachable;

  // Relabels matcher Fst - initialization function object.
  template <typename I>
  LabelLookAheadRelabeler(I **impl);

  // Relabels arbitrary Fst. Class L should be a label-lookahead Fst.
  template <class L>
  static void Relabel(MutableFst<A> *fst, const L &mfst,
                      bool relabel_input) {
    typename L::Impl *impl = mfst.GetImpl();
    D *data = impl->GetAddOn();
    Reachable reachable(data->First() ? data->First() : data->Second());
    reachable.Relabel(fst, relabel_input);
  }

  // Returns relabeling pairs (cf. relabel.h::Relabel()).
  // Class L should be a label-lookahead Fst.
  // If 'avoid_collisions' is true, extra pairs are added to
  // ensure no collisions when relabeling automata that have
  // labels unseen here.
  template <class L>
  static void RelabelPairs(const L &mfst, vector<pair<Label, Label> > *pairs,
                           bool avoid_collisions = false) {
    typename L::Impl *impl = mfst.GetImpl();
    D *data = impl->GetAddOn();
    Reachable reachable(data->First() ? data->First() : data->Second());
    reachable.RelabelPairs(pairs, avoid_collisions);
  }
};

template <class A, class R>
template <typename I> inline
LabelLookAheadRelabeler<A, R>::LabelLookAheadRelabeler(I **impl) {
  Fst<A> &fst = (*impl)->GetFst();
  D *data = (*impl)->GetAddOn();
  const string name = (*impl)->Type();
  bool is_mutable = fst.Properties(kMutable, false);
  MutableFst<A> *mfst = 0;
  if (is_mutable) {
    mfst = static_cast<MutableFst<A> *>(&fst);
  } else {
    mfst = new VectorFst<A>(fst);
    data->IncrRefCount();
    delete *impl;
  }
  if (data->First()) {  // reach_input
    Reachable reachable(data->First());
    reachable.Relabel(mfst, true);
    if (!FLAGS_save_relabel_ipairs.empty()) {
      vector<pair<Label, Label> > pairs;
      reachable.RelabelPairs(&pairs, true);
      WriteLabelPairs(FLAGS_save_relabel_ipairs, pairs);
    }
  } else {
    Reachable reachable(data->Second());
    reachable.Relabel(mfst, false);
    if (!FLAGS_save_relabel_opairs.empty()) {
      vector<pair<Label, Label> > pairs;
      reachable.RelabelPairs(&pairs, true);
      WriteLabelPairs(FLAGS_save_relabel_opairs, pairs);
    }
  }
  if (!is_mutable) {
    *impl = new I(*mfst, name);
    (*impl)->SetAddOn(data);
    delete mfst;
    data->DecrRefCount();
  }
}


// Generic lookahead matcher, templated on the FST definition
// - a wrapper around pointer to specific one.
template <class F>
class LookAheadMatcher {
 public:
  typedef F FST;
  typedef typename F::Arc Arc;
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Label Label;
  typedef typename Arc::Weight Weight;
  typedef LookAheadMatcherBase<Arc> LBase;

  LookAheadMatcher(const F &fst, MatchType match_type) {
    base_ = fst.InitMatcher(match_type);
    if (!base_)
      base_ = new SortedMatcher<F>(fst, match_type);
    lookahead_ = false;
  }

  LookAheadMatcher(const LookAheadMatcher<F> &matcher, bool safe = false) {
    base_ = matcher.base_->Copy(safe);
    lookahead_ = matcher.lookahead_;
  }

  ~LookAheadMatcher() { delete base_; }

  // General matcher methods
  LookAheadMatcher<F> *Copy(bool safe = false) const {
      return new LookAheadMatcher<F>(*this, safe);
  }

  MatchType Type(bool test) const { return base_->Type(test); }
  void SetState(StateId s) { base_->SetState(s); }
  bool Find(Label label) { return base_->Find(label); }
  bool Done() const { return base_->Done(); }
  const Arc& Value() const { return base_->Value(); }
  void Next() { base_->Next(); }
  Weight Final(StateId s) const { return base_->Final(s); }
  ssize_t Priority(StateId s) { return base_->Priority(s); }
  const F &GetFst() const { return static_cast<const F &>(base_->GetFst()); }

  uint64 Properties(uint64 props) const { return base_->Properties(props); }

  uint32 Flags() const { return base_->Flags(); }

  // Look-ahead methods
  bool LookAheadLabel(Label label) const {
    if (LookAheadCheck()) {
      LBase *lbase = static_cast<LBase *>(base_);
      return lbase->LookAheadLabel(label);
    } else {
      return true;
    }
  }

  bool LookAheadFst(const Fst<Arc> &fst, StateId s) {
    if (LookAheadCheck()) {
      LBase *lbase = static_cast<LBase *>(base_);
      return lbase->LookAheadFst(fst, s);
    } else {
      return true;
    }
  }

  Weight LookAheadWeight() const {
    if (LookAheadCheck()) {
      LBase *lbase = static_cast<LBase *>(base_);
      return lbase->LookAheadWeight();
    } else {
      return Weight::One();
    }
  }

  bool LookAheadPrefix(Arc *arc) const {
    if (LookAheadCheck()) {
      LBase *lbase = static_cast<LBase *>(base_);
      return lbase->LookAheadPrefix(arc);
    } else {
      return false;
    }
  }

  void InitLookAheadFst(const Fst<Arc>& fst, bool copy = false) {
    if (LookAheadCheck()) {
      LBase *lbase = static_cast<LBase *>(base_);
      lbase->InitLookAheadFst(fst, copy);
    }
  }

 private:
  bool LookAheadCheck() const {
    if (!lookahead_) {
      lookahead_ = base_->Flags() &
          (kInputLookAheadMatcher | kOutputLookAheadMatcher);
      if (!lookahead_) {
        FSTERROR() << "LookAheadMatcher: No look-ahead matcher defined";
      }
    }
    return lookahead_;
  }

  MatcherBase<Arc> *base_;
  mutable bool lookahead_;

  void operator=(const LookAheadMatcher<Arc> &);  // disallow
};

}  // namespace fst

#endif  // FST_LIB_LOOKAHEAD_MATCHER_H__
