// arcsort.h

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
// Functions and classes to sort arcs in an FST.

#ifndef FST_LIB_ARCSORT_H__
#define FST_LIB_ARCSORT_H__

#include <algorithm>
#include <string>
#include <vector>
using std::vector;

#include <fst/cache.h>
#include <fst/state-map.h>
#include <fst/test-properties.h>


namespace fst {

template <class Arc, class Compare>
class ArcSortMapper {
 public:
  typedef Arc FromArc;
  typedef Arc ToArc;

  typedef typename Arc::StateId StateId;
  typedef typename Arc::Weight Weight;

  ArcSortMapper(const Fst<Arc> &fst, const Compare &comp)
      : fst_(fst), comp_(comp), i_(0) {}

  // Allows updating Fst argument; pass only if changed.
  ArcSortMapper(const ArcSortMapper<Arc, Compare> &mapper,
                const Fst<Arc> *fst = 0)
      : fst_(fst ? *fst : mapper.fst_), comp_(mapper.comp_), i_(0) {}

  StateId Start() { return fst_.Start(); }
  Weight Final(StateId s) const { return fst_.Final(s); }

  void SetState(StateId s) {
    i_ = 0;
    arcs_.clear();
    arcs_.reserve(fst_.NumArcs(s));
    for (ArcIterator< Fst<Arc> > aiter(fst_, s); !aiter.Done(); aiter.Next())
      arcs_.push_back(aiter.Value());
    std::sort(arcs_.begin(), arcs_.end(), comp_);
  }

  bool Done() const { return i_ >= arcs_.size(); }
  const Arc &Value() const { return arcs_[i_]; }
  void Next() { ++i_; }

  MapSymbolsAction InputSymbolsAction() const { return MAP_COPY_SYMBOLS; }
  MapSymbolsAction OutputSymbolsAction() const { return MAP_COPY_SYMBOLS; }
  uint64 Properties(uint64 props) const { return comp_.Properties(props); }

 private:
  const Fst<Arc> &fst_;
  const Compare &comp_;
  vector<Arc> arcs_;
  ssize_t i_;               // current arc position

  void operator=(const ArcSortMapper<Arc, Compare> &);  // disallow
};


// Sorts the arcs in an FST according to function object 'comp' of
// type Compare. This version modifies its input.  Comparison function
// objects ILabelCompare and OLabelCompare are provived by the
// library. In general, Compare must meet the requirements for an STL
// sort comparision function object. It must also have a member
// Properties(uint64) that specifies the known properties of the
// sorted FST; it takes as argument the input FST's known properties
// before the sort.
//
// Complexity:
// - Time: O(V D log D)
// - Space: O(D)
// where V = # of states and D = maximum out-degree.
template<class Arc, class Compare>
void ArcSort(MutableFst<Arc> *fst, Compare comp) {
  ArcSortMapper<Arc, Compare> mapper(*fst, comp);
  StateMap(fst, mapper);
}

typedef CacheOptions ArcSortFstOptions;

// Sorts the arcs in an FST according to function object 'comp' of
// type Compare. This version is a delayed Fst.  Comparsion function
// objects ILabelCompare and OLabelCompare are provided by the
// library. In general, Compare must meet the requirements for an STL
// comparision function object (e.g. as used for STL sort). It must
// also have a member Properties(uint64) that specifies the known
// properties of the sorted FST; it takes as argument the input FST's
// known properties.
//
// Complexity:
// - Time: O(v d log d)
// - Space: O(d)
// where v = # of states visited, d = maximum out-degree of states
// visited. Constant time and space to visit an input state is assumed
// and exclusive of caching.
template <class A, class C>
class ArcSortFst : public StateMapFst<A, A, ArcSortMapper<A, C> > {
  using StateMapFst<A, A, ArcSortMapper<A, C> >::GetImpl;
 public:
  typedef A Arc;
  typedef typename Arc::StateId StateId;
  typedef ArcSortMapper<A, C> M;

  ArcSortFst(const Fst<A> &fst, const C &comp)
      : StateMapFst<A, A, M>(fst, ArcSortMapper<A, C>(fst, comp)) {}

  ArcSortFst(const Fst<A> &fst, const C &comp, const ArcSortFstOptions &opts)
      : StateMapFst<A, A, M>(fst, ArcSortMapper<A, C>(fst, comp), opts) {}

  // See Fst<>::Copy() for doc.
  ArcSortFst(const ArcSortFst<A, C> &fst, bool safe = false)
      : StateMapFst<A, A, M>(fst, safe) {}

  // Get a copy of this ArcSortFst. See Fst<>::Copy() for further doc.
  virtual ArcSortFst<A, C> *Copy(bool safe = false) const {
    return new ArcSortFst(*this, safe);
  }

  virtual size_t NumArcs(StateId s) const {
    return GetImpl()->GetFst().NumArcs(s);
  }

  virtual size_t NumInputEpsilons(StateId s) const {
    return GetImpl()->GetFst().NumInputEpsilons(s);
  }

  virtual size_t NumOutputEpsilons(StateId s) const {
    return GetImpl()->GetFst().NumOutputEpsilons(s);
  }
};


// Specialization for ArcSortFst.
template <class A, class C>
class StateIterator< ArcSortFst<A, C> >
    : public StateIterator< StateMapFst<A, A,  ArcSortMapper<A, C> > > {
 public:
  explicit StateIterator(const ArcSortFst<A, C> &fst)
      : StateIterator< StateMapFst<A, A,  ArcSortMapper<A, C> > >(fst) {}
};


// Specialization for ArcSortFst.
template <class A, class C>
class ArcIterator< ArcSortFst<A, C> >
    : public ArcIterator< StateMapFst<A, A,  ArcSortMapper<A, C> > > {
 public:
  ArcIterator(const ArcSortFst<A, C> &fst, typename A::StateId s)
      : ArcIterator< StateMapFst<A, A,  ArcSortMapper<A, C> > >(fst, s) {}
};


// Compare class for comparing input labels of arcs.
template<class A> class ILabelCompare {
 public:
  bool operator() (A arc1, A arc2) const {
    return arc1.ilabel < arc2.ilabel;
  }

  uint64 Properties(uint64 props) const {
    return (props & kArcSortProperties) | kILabelSorted |
        (props & kAcceptor ? kOLabelSorted : 0);
  }
};


// Compare class for comparing output labels of arcs.
template<class A> class OLabelCompare {
 public:
  bool operator() (const A &arc1, const A &arc2) const {
    return arc1.olabel < arc2.olabel;
  }

  uint64 Properties(uint64 props) const {
    return (props & kArcSortProperties) | kOLabelSorted |
        (props & kAcceptor ? kILabelSorted : 0);
  }
};


// Useful aliases when using StdArc.
template<class C> class StdArcSortFst : public ArcSortFst<StdArc, C> {
 public:
  typedef StdArc Arc;
  typedef C Compare;
};

typedef ILabelCompare<StdArc> StdILabelCompare;

typedef OLabelCompare<StdArc> StdOLabelCompare;

}  // namespace fst

#endif  // FST_LIB_ARCSORT_H__
