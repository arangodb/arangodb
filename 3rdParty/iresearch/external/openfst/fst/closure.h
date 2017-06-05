// closure.h

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
// Functions and classes to compute the concatenative closure of an Fst.

#ifndef FST_LIB_CLOSURE_H__
#define FST_LIB_CLOSURE_H__

#include <vector>
using std::vector;
#include <algorithm>

#include <fst/mutable-fst.h>
#include <fst/rational.h>


namespace fst {

// Computes the concatenative closure. This version modifies its
// MutableFst input. If FST transduces string x to y with weight a,
// then the closure transduces x to y with weight a, xx to yy with
// weight Times(a, a), xxx to yyy with with Times(Times(a, a), a),
// etc. If closure_type == CLOSURE_STAR, then the empty string is
// transduced to itself with weight Weight::One() as well.
//
// Complexity:
// - Time: O(V)
// - Space: O(V)
// where V = # of states.
template<class Arc>
void Closure(MutableFst<Arc> *fst, ClosureType closure_type) {
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Label Label;
  typedef typename Arc::Weight Weight;

  uint64 props = fst->Properties(kFstProperties, false);
  StateId start = fst->Start();
  for (StateIterator< MutableFst<Arc> > siter(*fst);
       !siter.Done();
       siter.Next()) {
    StateId s = siter.Value();
    Weight final = fst->Final(s);
    if (final != Weight::Zero())
      fst->AddArc(s, Arc(0, 0, final, start));
  }
  if (closure_type == CLOSURE_STAR) {
    fst->ReserveStates(fst->NumStates() + 1);
    StateId nstart = fst->AddState();
    fst->SetStart(nstart);
    fst->SetFinal(nstart, Weight::One());
    if (start != kNoLabel)
      fst->AddArc(nstart, Arc(0, 0, Weight::One(), start));
  }
  fst->SetProperties(ClosureProperties(props, closure_type == CLOSURE_STAR),
                     kFstProperties);
}

// Computes the concatenative closure. This version modifies its
// RationalFst input.
template<class Arc>
void Closure(RationalFst<Arc> *fst, ClosureType closure_type) {
  fst->GetImpl()->AddClosure(closure_type);
}


struct ClosureFstOptions : RationalFstOptions {
  ClosureType type;

  ClosureFstOptions(const RationalFstOptions &opts, ClosureType t)
      : RationalFstOptions(opts), type(t) {}
  explicit ClosureFstOptions(ClosureType t) : type(t) {}
  ClosureFstOptions() : type(CLOSURE_STAR) {}
};


// Computes the concatenative closure. This version is a delayed
// Fst. If FST transduces string x to y with weight a, then the
// closure transduces x to y with weight a, xx to yy with weight
// Times(a, a), xxx to yyy with weight Times(Times(a, a), a), etc. If
// closure_type == CLOSURE_STAR, then The empty string is transduced
// to itself with weight Weight::One() as well.
//
// Complexity:
// - Time: O(v)
// - Space: O(v)
// where v = # of states visited. Constant time and space to visit an
// input state or arc is assumed and exclusive of caching.
template <class A>
class ClosureFst : public RationalFst<A> {
 public:
  using ImplToFst< RationalFstImpl<A> >::GetImpl;

  typedef A Arc;

  ClosureFst(const Fst<A> &fst, ClosureType closure_type) {
    GetImpl()->InitClosure(fst, closure_type);
  }

  ClosureFst(const Fst<A> &fst, const ClosureFstOptions &opts)
      : RationalFst<A>(opts) {
    GetImpl()->InitClosure(fst, opts.type);
  }

  // See Fst<>::Copy() for doc.
  ClosureFst(const ClosureFst<A> &fst, bool safe = false)
      : RationalFst<A>(fst, safe) {}

  // Get a copy of this ClosureFst. See Fst<>::Copy() for further doc.
  virtual ClosureFst<A> *Copy(bool safe = false) const {
    return new ClosureFst<A>(*this, safe);
  }
};


// Specialization for ClosureFst.
template <class A>
class StateIterator< ClosureFst<A> > : public StateIterator< RationalFst<A> > {
 public:
  explicit StateIterator(const ClosureFst<A> &fst)
      : StateIterator< RationalFst<A> >(fst) {}
};


// Specialization for ClosureFst.
template <class A>
class ArcIterator< ClosureFst<A> > : public ArcIterator< RationalFst<A> > {
 public:
  typedef typename A::StateId StateId;

  ArcIterator(const ClosureFst<A> &fst, StateId s)
      : ArcIterator< RationalFst<A> >(fst, s) {}
};


// Useful alias when using StdArc.
typedef ClosureFst<StdArc> StdClosureFst;

}  // namespace fst

#endif  // FST_LIB_CLOSURE_H__
