// rational.h

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
// An Fst implementation and base interface for delayed unions,
// concatenations and closures.

#ifndef FST_LIB_RATIONAL_H__
#define FST_LIB_RATIONAL_H__

#include <algorithm>
#include <string>
#include <vector>
using std::vector;

#include <fst/mutable-fst.h>
#include <fst/replace.h>
#include <fst/test-properties.h>


namespace fst {

typedef CacheOptions RationalFstOptions;

// This specifies whether to add the empty string.
enum ClosureType { CLOSURE_STAR = 0,    // T* -> add the empty string
                   CLOSURE_PLUS = 1 };  // T+ -> don't add the empty string

template <class A> class RationalFst;
template <class A> void Union(RationalFst<A> *fst1, const Fst<A> &fst2);
template <class A> void Concat(RationalFst<A> *fst1, const Fst<A> &fst2);
template <class A> void Concat(const Fst<A> &fst1, RationalFst<A> *fst2);
template <class A> void Closure(RationalFst<A> *fst, ClosureType closure_type);


// Implementation class for delayed unions, concatenations and closures.
template<class A>
class RationalFstImpl : public FstImpl<A> {
 public:
  using FstImpl<A>::SetType;
  using FstImpl<A>::SetProperties;
  using FstImpl<A>::WriteHeader;
  using FstImpl<A>::SetInputSymbols;
  using FstImpl<A>::SetOutputSymbols;

  typedef A Arc;
  typedef typename A::Weight Weight;
  typedef typename A::StateId StateId;
  typedef typename A::Label Label;

  explicit RationalFstImpl(const RationalFstOptions &opts)
      : nonterminals_(0),
        replace_(0),
        replace_options_(opts, 0) {
    SetType("rational");
    fst_tuples_.push_back(pair<Label, const Fst<A>*>(0, 0));
  }

  RationalFstImpl(const RationalFstImpl<A> &impl)
      : rfst_(impl.rfst_),
        nonterminals_(impl.nonterminals_),

        replace_(impl.replace_ ? impl.replace_->Copy(true) : 0),
        replace_options_(impl.replace_options_) {
    SetType("rational");
    fst_tuples_.reserve(impl.fst_tuples_.size());
    for (size_t i = 0; i < impl.fst_tuples_.size(); ++i)
      fst_tuples_.push_back(std::make_pair(
          impl.fst_tuples_[i].first,
          impl.fst_tuples_[i].second ? impl.fst_tuples_[i].second->Copy(true)
                                     : 0));
  }

  virtual ~RationalFstImpl() {
    for (size_t i = 0; i < fst_tuples_.size(); ++i)
      if (fst_tuples_[i].second)
        delete fst_tuples_[i].second;
    if (replace_)
      delete replace_;
  }

  StateId Start() { return Replace()->Start(); }

  Weight Final(StateId s) { return Replace()->Final(s); }

  size_t NumArcs(StateId s) { return Replace()->NumArcs(s); }

  size_t NumInputEpsilons(StateId s) {
    return Replace()->NumInputEpsilons(s);
  }

  size_t NumOutputEpsilons(StateId s) {
    return Replace()->NumOutputEpsilons(s);
  }

  uint64 Properties() const { return Properties(kFstProperties); }

  // Set error if found; return FST impl properties.
  uint64 Properties(uint64 mask) const {
    if ((mask & kError) && Replace()->Properties(kError, false))
      SetProperties(kError, kError);
    return FstImpl<Arc>::Properties(mask);
  }

  // Implementation of UnionFst(fst1,fst2)
  void InitUnion(const Fst<A> &fst1, const Fst<A> &fst2) {
    if (replace_)
      delete replace_;
    uint64 props1 = fst1.Properties(kFstProperties, false);
    uint64 props2 = fst2.Properties(kFstProperties, false);
    SetInputSymbols(fst1.InputSymbols());
    SetOutputSymbols(fst1.OutputSymbols());
    rfst_.AddState();
    rfst_.AddState();
    rfst_.SetStart(0);
    rfst_.SetFinal(1, Weight::One());
    rfst_.SetInputSymbols(fst1.InputSymbols());
    rfst_.SetOutputSymbols(fst1.OutputSymbols());
    nonterminals_ = 2;
    rfst_.AddArc(0, A(0, -1, Weight::One(), 1));
    rfst_.AddArc(0, A(0, -2, Weight::One(), 1));
    fst_tuples_.push_back(std::make_pair(-1, fst1.Copy()));
    fst_tuples_.push_back(std::make_pair(-2, fst2.Copy()));
    SetProperties(UnionProperties(props1, props2, true), kCopyProperties);
  }

  // Implementation of ConcatFst(fst1,fst2)
  void InitConcat(const Fst<A> &fst1, const Fst<A> &fst2) {
    if (replace_)
      delete replace_;
    uint64 props1 = fst1.Properties(kFstProperties, false);
    uint64 props2 = fst2.Properties(kFstProperties, false);
    SetInputSymbols(fst1.InputSymbols());
    SetOutputSymbols(fst1.OutputSymbols());
    rfst_.AddState();
    rfst_.AddState();
    rfst_.AddState();
    rfst_.SetStart(0);
    rfst_.SetFinal(2, Weight::One());
    rfst_.SetInputSymbols(fst1.InputSymbols());
    rfst_.SetOutputSymbols(fst1.OutputSymbols());
    nonterminals_ = 2;
    rfst_.AddArc(0, A(0, -1, Weight::One(), 1));
    rfst_.AddArc(1, A(0, -2, Weight::One(), 2));
    fst_tuples_.push_back(std::make_pair(-1, fst1.Copy()));
    fst_tuples_.push_back(std::make_pair(-2, fst2.Copy()));
    SetProperties(ConcatProperties(props1, props2, true), kCopyProperties);
  }

  // Implementation of ClosureFst(fst, closure_type)
  void InitClosure(const Fst<A> &fst, ClosureType closure_type) {
    if (replace_)
      delete replace_;
    uint64 props = fst.Properties(kFstProperties, false);
    SetInputSymbols(fst.InputSymbols());
    SetOutputSymbols(fst.OutputSymbols());
    if (closure_type == CLOSURE_STAR) {
      rfst_.AddState();
      rfst_.SetStart(0);
      rfst_.SetFinal(0, Weight::One());
      rfst_.AddArc(0, A(0, -1, Weight::One(), 0));
    } else {
      rfst_.AddState();
      rfst_.AddState();
      rfst_.SetStart(0);
      rfst_.SetFinal(1, Weight::One());
      rfst_.AddArc(0, A(0, -1, Weight::One(), 1));
      rfst_.AddArc(1, A(0, 0, Weight::One(), 0));
    }
    rfst_.SetInputSymbols(fst.InputSymbols());
    rfst_.SetOutputSymbols(fst.OutputSymbols());
    fst_tuples_.push_back(std::make_pair(-1, fst.Copy()));
    nonterminals_ = 1;
    SetProperties(ClosureProperties(props, closure_type == CLOSURE_STAR, true),
                  kCopyProperties);
  }

  // Implementation of Union(Fst &, RationalFst *)
  void AddUnion(const Fst<A> &fst) {
    if (replace_)
      delete replace_;
    uint64 props1 = FstImpl<A>::Properties();
    uint64 props2 = fst.Properties(kFstProperties, false);
    VectorFst<A> afst;
    afst.AddState();
    afst.AddState();
    afst.SetStart(0);
    afst.SetFinal(1, Weight::One());
    ++nonterminals_;
    afst.AddArc(0, A(0, -nonterminals_, Weight::One(), 1));
    Union(&rfst_, afst);
    fst_tuples_.push_back(std::make_pair(-nonterminals_, fst.Copy()));
    SetProperties(UnionProperties(props1, props2, true), kCopyProperties);
  }

  // Implementation of Concat(Fst &, RationalFst *)
  void AddConcat(const Fst<A> &fst, bool append) {
    if (replace_)
      delete replace_;
    uint64 props1 = FstImpl<A>::Properties();
    uint64 props2 = fst.Properties(kFstProperties, false);
    VectorFst<A> afst;
    afst.AddState();
    afst.AddState();
    afst.SetStart(0);
    afst.SetFinal(1, Weight::One());
    ++nonterminals_;
    afst.AddArc(0, A(0, -nonterminals_, Weight::One(), 1));
    if (append)
      Concat(&rfst_, afst);
    else
      Concat(afst, &rfst_);
    fst_tuples_.push_back(std::make_pair(-nonterminals_, fst.Copy()));
    SetProperties(ConcatProperties(props1, props2, true), kCopyProperties);
  }

  // Implementation of Closure(RationalFst *, closure_type)
  void AddClosure(ClosureType closure_type) {
    if (replace_)
      delete replace_;
    uint64 props = FstImpl<A>::Properties();
    Closure(&rfst_, closure_type);
    SetProperties(ClosureProperties(props, closure_type == CLOSURE_STAR, true),
                  kCopyProperties);
  }

  // Returns the underlying ReplaceFst.
  ReplaceFst<A> *Replace() const {
    if (!replace_) {
      fst_tuples_[0].second = rfst_.Copy();
      replace_ = new ReplaceFst<A>(fst_tuples_, replace_options_);
    }
    return replace_;
  }

 private:
  VectorFst<A> rfst_;   // rational topology machine; uses neg. nonterminals
  Label nonterminals_;  // # of nonterminals used
  // Contains the nonterminals and their corresponding FSTs.
  mutable vector<pair<Label, const Fst<A>*> > fst_tuples_;
  mutable ReplaceFst<A> *replace_;        // Underlying ReplaceFst
  ReplaceFstOptions<A> replace_options_;  // Options for creating 'replace_'

  void operator=(const RationalFstImpl<A> &impl);    // disallow
};

// Parent class for the delayed rational operations - delayed union,
// concatenation, and closure.
//
// This class attaches interface to implementation and handles
// reference counting, delegating most methods to ImplToFst.
template <class A>
class RationalFst : public ImplToFst< RationalFstImpl<A> > {
 public:
  friend class StateIterator< RationalFst<A> >;
  friend class ArcIterator< RationalFst<A> >;
  friend void Union<>(RationalFst<A> *fst1, const Fst<A> &fst2);
  friend void Concat<>(RationalFst<A> *fst1, const Fst<A> &fst2);
  friend void Concat<>(const Fst<A> &fst1, RationalFst<A> *fst2);
  friend void Closure<>(RationalFst<A> *fst, ClosureType closure_type);

  typedef A Arc;
  typedef typename A::StateId StateId;
  typedef RationalFstImpl<A> Impl;

  virtual void InitStateIterator(StateIteratorData<A> *data) const {
    GetImpl()->Replace()->InitStateIterator(data);
  }

  virtual void InitArcIterator(StateId s, ArcIteratorData<A> *data) const {
    GetImpl()->Replace()->InitArcIterator(s, data);
  }

 protected:
  RationalFst()
      : ImplToFst<Impl>(new Impl(RationalFstOptions())) {}

  explicit RationalFst(const RationalFstOptions &opts)
      : ImplToFst<Impl>(new Impl(opts)) {}

  // See Fst<>::Copy() for doc.
  RationalFst(const RationalFst<A> &fst , bool safe = false)
      : ImplToFst<Impl>(fst, safe) {}

 private:
  // Makes visible to friends.
  Impl *GetImpl() const { return ImplToFst<Impl>::GetImpl(); }

  void operator=(const RationalFst<A> &fst);  // disallow
};


// Specialization for RationalFst.
template <class A>
class StateIterator< RationalFst<A> >
    : public StateIterator< ReplaceFst<A> > {
 public:
  explicit StateIterator(const RationalFst<A> &fst)
      : StateIterator< ReplaceFst<A> >(*(fst.GetImpl()->Replace())) {}
};


// Specialization for RationalFst.
template <class A>
class ArcIterator< RationalFst<A> >
    : public CacheArcIterator< ReplaceFst<A> > {
 public:
  typedef typename A::StateId StateId;

  ArcIterator(const RationalFst<A> &fst, StateId s)
      : ArcIterator< ReplaceFst<A> >(*(fst.GetImpl()->Replace()), s) {}
};

}  // namespace fst

#endif  // FST_LIB_RATIONAL_H__
