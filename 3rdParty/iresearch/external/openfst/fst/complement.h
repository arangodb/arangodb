// complement.h

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
// Class to complement an Fst.

#ifndef FST_LIB_COMPLEMENT_H__
#define FST_LIB_COMPLEMENT_H__

#include <algorithm>
#include <string>
#include <vector>
using std::vector;

#include <fst/fst.h>
#include <fst/test-properties.h>


namespace fst {

template <class A> class ComplementFst;

// Implementation of delayed ComplementFst. The algorithm used
// completes the (deterministic) FSA and then exchanges final and
// non-final states.  Completion, i.e. ensuring that all labels can be
// read from every state, is accomplished by using RHO labels, which
// match all labels that are otherwise not found leaving a state. The
// first state in the output is reserved to be a new state that is the
// destination of all RHO labels. Each remaining output state s
// corresponds to input state s - 1. The first arc in the output at
// these states is the rho label, the remaining arcs correspond to the
// input arcs.
template <class A>
class ComplementFstImpl : public FstImpl<A> {
 public:
  using FstImpl<A>::SetType;
  using FstImpl<A>::SetProperties;
  using FstImpl<A>::SetInputSymbols;
  using FstImpl<A>::SetOutputSymbols;

  friend class StateIterator< ComplementFst<A> >;
  friend class ArcIterator< ComplementFst<A> >;

  typedef A Arc;
  typedef typename A::Label Label;
  typedef typename A::Weight Weight;
  typedef typename A::StateId StateId;

  explicit ComplementFstImpl(const Fst<A> &fst) : fst_(fst.Copy()) {
    SetType("complement");
    uint64 props = fst.Properties(kILabelSorted, false);
    SetProperties(ComplementProperties(props), kCopyProperties);
    SetInputSymbols(fst.InputSymbols());
    SetOutputSymbols(fst.OutputSymbols());
  }

  ComplementFstImpl(const ComplementFstImpl<A> &impl)
      : fst_(impl.fst_->Copy()) {
    SetType("complement");
    SetProperties(impl.Properties(), kCopyProperties);
    SetInputSymbols(impl.InputSymbols());
    SetOutputSymbols(impl.OutputSymbols());
  }

  ~ComplementFstImpl() { delete fst_; }

  StateId Start() const {
    if (Properties(kError))
      return kNoStateId;

    StateId start = fst_->Start();
    if (start != kNoStateId)
      return start + 1;
    else
      return 0;
  }

  // Exchange final and non-final states; make rho destination state final.
  Weight Final(StateId s) const {
    if (s == 0 || fst_->Final(s - 1) == Weight::Zero())
      return Weight::One();
    else
      return Weight::Zero();
  }

  size_t NumArcs(StateId s) const {
    if (s == 0)
      return 1;
    else
      return fst_->NumArcs(s - 1) + 1;
  }

  size_t NumInputEpsilons(StateId s) const {
    return s == 0 ? 0 : fst_->NumInputEpsilons(s - 1);
  }

  size_t NumOutputEpsilons(StateId s) const {
    return s == 0 ? 0 : fst_->NumOutputEpsilons(s - 1);
  }


  uint64 Properties() const { return Properties(kFstProperties); }

  // Set error if found; return FST impl properties.
  uint64 Properties(uint64 mask) const {
    if ((mask & kError) && fst_->Properties(kError, false))
      SetProperties(kError, kError);
    return FstImpl<Arc>::Properties(mask);
  }


 private:
  const Fst<A> *fst_;

  void operator=(const ComplementFstImpl<A> &fst);  // Disallow
};


// Complements an automaton. This is a library-internal operation that
// introduces a (negative) 'rho' label; use Difference/DifferenceFst in
// user code, which will not see this label. This version is a delayed Fst.
//
// This class attaches interface to implementation and handles
// reference counting, delegating most methods to ImplToFst.
template <class A>
class ComplementFst : public ImplToFst< ComplementFstImpl<A> > {
 public:
  friend class StateIterator< ComplementFst<A> >;
  friend class ArcIterator< ComplementFst<A> >;

  using ImplToFst< ComplementFstImpl<A> >::GetImpl;

  typedef A Arc;
  typedef typename A::StateId StateId;
  typedef typename A::Label Label;
  typedef ComplementFstImpl<A> Impl;

  explicit ComplementFst(const Fst<A> &fst)
      : ImplToFst<Impl>(new Impl(fst)) {
    uint64 props = kUnweighted | kNoEpsilons | kIDeterministic | kAcceptor;
    if (fst.Properties(props, true) != props) {
      FSTERROR() << "ComplementFst: argument not an unweighted "
                 << "epsilon-free deterministic acceptor";
      GetImpl()->SetProperties(kError, kError);
    }
  }

  // See Fst<>::Copy() for doc.
  ComplementFst(const ComplementFst<A> &fst, bool safe = false)
      : ImplToFst<Impl>(fst, safe) {}

  // Get a copy of this ComplementFst. See Fst<>::Copy() for further doc.
  virtual ComplementFst<A> *Copy(bool safe = false) const {
    return new ComplementFst<A>(*this, safe);
  }

  virtual inline void InitStateIterator(StateIteratorData<A> *data) const;

  virtual inline void InitArcIterator(StateId s,
                                      ArcIteratorData<A> *data) const;

  // Label that represents the rho transition.
  // We use a negative value, which is thus private to the library and
  // which will preserve FST label sort order.
  static const Label kRhoLabel = -2;
 private:
  // Makes visible to friends.
  Impl *GetImpl() const { return ImplToFst<Impl>::GetImpl(); }

  void operator=(const ComplementFst<A> &fst);  // disallow
};

template <class A> const typename A::Label ComplementFst<A>::kRhoLabel;


// Specialization for ComplementFst.
template <class A>
class StateIterator< ComplementFst<A> > : public StateIteratorBase<A> {
 public:
  typedef typename A::StateId StateId;
  typedef typename A::Label Label;

  explicit StateIterator(const ComplementFst<A> &fst)
      : siter_(*fst.GetImpl()->fst_), s_(0) {
  }

  bool Done() const { return s_ > 0 && siter_.Done(); }

  StateId Value() const { return s_; }

  void Next() {
    if (s_ != 0)
      siter_.Next();
    ++s_;
  }

  void Reset() {
    siter_.Reset();
    s_ = 0;
  }

 private:
  // This allows base class virtual access to non-virtual derived-
  // class members of the same name. It makes the derived class more
  // efficient to use but unsafe to further derive.
  virtual bool Done_() const { return Done(); }
  virtual StateId Value_() const { return Value(); }
  virtual void Next_() { Next(); }
  virtual void Reset_() { Reset(); }

  StateIterator< Fst<A> > siter_;
  StateId s_;

  DISALLOW_COPY_AND_ASSIGN(StateIterator);
};


// Specialization for ComplementFst.
template <class A>
class ArcIterator< ComplementFst<A> > : public ArcIteratorBase<A> {
 public:
  typedef typename A::StateId StateId;
  typedef typename A::Label Label;
  typedef typename A::Weight Weight;

  ArcIterator(const ComplementFst<A> &fst, StateId s)
      : aiter_(0), s_(s), pos_(0) {
    if (s_ != 0)
      aiter_ = new ArcIterator< Fst<A> >(*fst.GetImpl()->fst_, s - 1);
  }

  virtual ~ArcIterator() { delete aiter_; }

  bool Done() const {
    if (s_ != 0)
      return pos_ > 0 && aiter_->Done();
    else
      return pos_ > 0;
  }

  // Adds the rho label to the rho destination state.
  const A& Value() const {
    if (pos_ == 0) {
      arc_.ilabel = arc_.olabel = ComplementFst<A>::kRhoLabel;
      arc_.weight = Weight::One();
      arc_.nextstate = 0;
    } else {
      arc_ = aiter_->Value();
      ++arc_.nextstate;
    }
    return arc_;
  }

  void Next() {
    if (s_ != 0 && pos_ > 0)
      aiter_->Next();
    ++pos_;
  }

  size_t Position() const {
    return pos_;
  }

  void Reset() {
    if (s_ != 0)
      aiter_->Reset();
    pos_ = 0;
  }

  void Seek(size_t a) {
    if (s_ != 0) {
      if (a == 0) {
        aiter_->Reset();
      } else {
        aiter_->Seek(a - 1);
      }
    }
    pos_ = a;
  }

  uint32 Flags() const {
    return kArcValueFlags;
  }

  void SetFlags(uint32 f, uint32 m) {}

 private:
  // This allows base class virtual access to non-virtual derived-
  // class members of the same name. It makes the derived class more
  // efficient to use but unsafe to further derive.
  virtual bool Done_() const { return Done(); }
  virtual const A& Value_() const { return Value(); }
  virtual void Next_() { Next(); }
  virtual size_t Position_() const { return Position(); }
  virtual void Reset_() { Reset(); }
  virtual void Seek_(size_t a) { Seek(a); }
  uint32 Flags_() const { return Flags(); }
  void SetFlags_(uint32 f, uint32 m) { SetFlags(f, m); }

  ArcIterator< Fst<A> > *aiter_;
  StateId s_;
  size_t pos_;
  mutable A arc_;
  DISALLOW_COPY_AND_ASSIGN(ArcIterator);
};


template <class A> inline void
ComplementFst<A>::InitStateIterator(StateIteratorData<A> *data) const {
  data->base = new StateIterator< ComplementFst<A> >(*this);
}

template <class A> inline void
ComplementFst<A>::InitArcIterator(StateId s, ArcIteratorData<A> *data) const {
  data->base = new ArcIterator< ComplementFst<A> >(*this, s);
}


// Useful alias when using StdArc.
typedef ComplementFst<StdArc> StdComplementFst;

}  // namespace fst

#endif  // FST_LIB_COMPLEMENT_H__
