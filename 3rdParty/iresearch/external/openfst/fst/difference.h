// difference.h

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
// Class to compute the difference between two FSAs

#ifndef FST_LIB_DIFFERENCE_H__
#define FST_LIB_DIFFERENCE_H__

#include <vector>
using std::vector;
#include <algorithm>

#include <fst/cache.h>
#include <fst/compose.h>
#include <fst/complement.h>


namespace fst {

template <class A,
          class M = Matcher<Fst<A> >,
          class F = SequenceComposeFilter<M>,
          class T = GenericComposeStateTable<A, typename F::FilterState> >
struct DifferenceFstOptions : public ComposeFstOptions<A, M, F, T> {
  explicit DifferenceFstOptions(const CacheOptions &opts,
                                M *mat1 = 0, M *mat2 = 0,
                                F *filt = 0, T *sttable= 0)
      : ComposeFstOptions<A, M, F, T>(mat1, mat2, filt, sttable) { }

  DifferenceFstOptions() {}
};

// Computes the difference between two FSAs. This version is a delayed
// Fst. Only strings that are in the first automaton but not in second
// are retained in the result.
//
// The first argument must be an acceptor; the second argument must be
// an unweighted, epsilon-free, deterministic acceptor. One of the
// arguments must be label-sorted.
//
// Complexity: same as ComposeFst.
//
// Caveats: same as ComposeFst.
template <class A>
class DifferenceFst : public ComposeFst<A> {
 public:
  using ImplToFst< ComposeFstImplBase<A> >::SetImpl;
  using ImplToFst< ComposeFstImplBase<A> >::GetImpl;

  using ComposeFst<A>::CreateBase1;

  typedef A Arc;
  typedef typename A::Weight Weight;
  typedef typename A::StateId StateId;

  // A - B = A ^ B'.
  DifferenceFst(const Fst<A> &fst1, const Fst<A> &fst2,
                const CacheOptions &opts = CacheOptions()) {
    typedef RhoMatcher< Matcher<Fst<A> > > R;

    ComplementFst<A> cfst(fst2);
    ComposeFstOptions<A, R> copts(CacheOptions(),
                                  new R(fst1, MATCH_NONE),
                                  new R(cfst, MATCH_INPUT,
                                        ComplementFst<A>::kRhoLabel));
    SetImpl(CreateBase1(fst1, cfst, copts));

    if (!fst1.Properties(kAcceptor, true)) {
      FSTERROR() << "DifferenceFst: 1st argument not an acceptor";
      GetImpl()->SetProperties(kError, kError);
    }
 }

  template <class M, class F, class T>
  DifferenceFst(const Fst<A> &fst1, const Fst<A> &fst2,
                const DifferenceFstOptions<A, M, F, T> &opts) {
    typedef RhoMatcher<M> R;

    ComplementFst<A> cfst(fst2);
    ComposeFstOptions<A, R> copts(opts);
    copts.matcher1 = new R(fst1, MATCH_NONE, kNoLabel, MATCHER_REWRITE_ALWAYS,
                           opts.matcher1);
    copts.matcher2 = new R(cfst, MATCH_INPUT, ComplementFst<A>::kRhoLabel,
                           MATCHER_REWRITE_ALWAYS, opts.matcher2);

    SetImpl(CreateBase1(fst1, cfst, copts));

    if (!fst1.Properties(kAcceptor, true)) {
      FSTERROR() << "DifferenceFst: 1st argument not an acceptor";
      GetImpl()->SetProperties(kError, kError);
    }
  }

  // See Fst<>::Copy() for doc.
  DifferenceFst(const DifferenceFst<A> &fst, bool safe = false)
      : ComposeFst<A>(fst, safe) {}

  // Get a copy of this DifferenceFst. See Fst<>::Copy() for further doc.
  virtual DifferenceFst<A> *Copy(bool safe = false) const {
    return new DifferenceFst<A>(*this, safe);
  }
};


// Specialization for DifferenceFst.
template <class A>
class StateIterator< DifferenceFst<A> >
    : public StateIterator< ComposeFst<A> > {
 public:
  explicit StateIterator(const DifferenceFst<A> &fst)
      : StateIterator< ComposeFst<A> >(fst) {}
};


// Specialization for DifferenceFst.
template <class A>
class ArcIterator< DifferenceFst<A> >
    : public ArcIterator< ComposeFst<A> > {
 public:
  typedef typename A::StateId StateId;

  ArcIterator(const DifferenceFst<A> &fst, StateId s)
      : ArcIterator< ComposeFst<A> >(fst, s) {}
};

// Useful alias when using StdArc.
typedef DifferenceFst<StdArc> StdDifferenceFst;


typedef ComposeOptions DifferenceOptions;


// Computes the difference between two FSAs. This version is writes
// the difference to an output MutableFst. Only strings that are in
// the first automaton but not in second are retained in the result.
//
// The first argument must be an acceptor; the second argument must be
// an unweighted, epsilon-free, deterministic acceptor.  One of the
// arguments must be label-sorted.
//
// Complexity: same as Compose.
//
// Caveats: same as Compose.
template<class Arc>
void Difference(const Fst<Arc> &ifst1, const Fst<Arc> &ifst2,
             MutableFst<Arc> *ofst,
             const DifferenceOptions &opts = DifferenceOptions()) {
  typedef Matcher< Fst<Arc> > M;

  if (opts.filter_type == AUTO_FILTER) {
    CacheOptions nopts;
    nopts.gc_limit = 0;  // Cache only the last state for fastest copy.
    *ofst = DifferenceFst<Arc>(ifst1, ifst2, nopts);
  } else if (opts.filter_type == SEQUENCE_FILTER) {
    DifferenceFstOptions<Arc> dopts;
    dopts.gc_limit = 0;  // Cache only the last state for fastest copy.
    *ofst = DifferenceFst<Arc>(ifst1, ifst2, dopts);
  } else if (opts.filter_type == ALT_SEQUENCE_FILTER) {
    DifferenceFstOptions<Arc, M, AltSequenceComposeFilter<M> > dopts;
    dopts.gc_limit = 0;  // Cache only the last state for fastest copy.
    *ofst = DifferenceFst<Arc>(ifst1, ifst2, dopts);
  } else if (opts.filter_type == MATCH_FILTER) {
    DifferenceFstOptions<Arc, M, MatchComposeFilter<M> > dopts;
    dopts.gc_limit = 0;  // Cache only the last state for fastest copy.
    *ofst = DifferenceFst<Arc>(ifst1, ifst2, dopts);
  }

  if (opts.connect)
    Connect(ofst);
}

}  // namespace fst

#endif  // FST_LIB_DIFFERENCE_H__
