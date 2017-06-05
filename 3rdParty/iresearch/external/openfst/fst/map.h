// map.h

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
// Compatability file for old-style Map() functions and MapFst class
// that have been renamed to ArcMap (cf. StateMap).

#ifndef FST_LIB_MAP_H__
#define FST_LIB_MAP_H__


#include <fst/arc-map.h>


namespace fst {

template<class A, class C>
void Map(MutableFst<A> *fst, C* mapper) {
  ArcMap(fst, mapper);
}

template<class A, class C>
void Map(MutableFst<A> *fst, C mapper) {
  ArcMap(fst, mapper);
}

template<class A, class B, class C>
void Map(const Fst<A> &ifst, MutableFst<B> *ofst, C* mapper) {
  ArcMap(ifst, ofst, mapper);
}

template<class A, class B, class C>
void Map(const Fst<A> &ifst, MutableFst<B> *ofst, C mapper) {
  ArcMap(ifst, ofst, mapper);
}

typedef ArcMapFstOptions MapFstOptions;

template <class A, class B, class C>
class MapFst : public ArcMapFst<A, B, C> {
 public:
  typedef B Arc;
  typedef typename B::Weight Weight;
  typedef typename B::StateId StateId;
  typedef CacheState<B> State;

  MapFst(const Fst<A> &fst, const C &mapper, const MapFstOptions& opts)
      : ArcMapFst<A, B, C>(fst, mapper, opts) {}

  MapFst(const Fst<A> &fst, C* mapper, const MapFstOptions& opts)
      : ArcMapFst<A, B, C>(fst, mapper, opts) {}

  MapFst(const Fst<A> &fst, const C &mapper)
      : ArcMapFst<A, B, C>(fst, mapper) {}

  MapFst(const Fst<A> &fst, C* mapper) : ArcMapFst<A, B, C>(fst, mapper) {}

  // See Fst<>::Copy() for doc.
  MapFst(const ArcMapFst<A, B, C> &fst, bool safe = false)
      : ArcMapFst<A, B, C>(fst, safe) {}

  // Get a copy of this MapFst. See Fst<>::Copy() for further doc.
virtual MapFst<A, B, C> *Copy(bool safe = false) const {
    return new MapFst(*this, safe);
  }
};


// Specialization for MapFst.
template <class A, class B, class C>
class StateIterator< MapFst<A, B, C> >
    : public StateIterator< ArcMapFst<A, B, C> > {
 public:
  explicit StateIterator(const ArcMapFst<A, B, C> &fst)
      : StateIterator< ArcMapFst<A, B, C> >(fst) {}
};


// Specialization for MapFst.
template <class A, class B, class C>
class ArcIterator< MapFst<A, B, C> >
    : public ArcIterator< ArcMapFst<A, B, C> > {
 public:
  ArcIterator(const ArcMapFst<A, B, C> &fst, typename A::StateId s)
      : ArcIterator< ArcMapFst<A, B, C> >(fst, s) {}
};


template <class A>
struct IdentityMapper {
  typedef A FromArc;
  typedef A ToArc;

  A operator()(const A &arc) const { return arc; }

  MapFinalAction FinalAction() const { return MAP_NO_SUPERFINAL; }

  MapSymbolsAction InputSymbolsAction() const { return MAP_COPY_SYMBOLS; }

  MapSymbolsAction OutputSymbolsAction() const { return MAP_COPY_SYMBOLS;}

  uint64 Properties(uint64 props) const { return props; }
};

}  // namespace fst

#endif  // FST_LIB_MAP_H__
