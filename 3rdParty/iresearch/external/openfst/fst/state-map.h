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
// Class to map over/transform states e.g., sort transitions
// Consider using when operation does not change the number of states.

#ifndef FST_LIB_STATE_MAP_H__
#define FST_LIB_STATE_MAP_H__

#include <algorithm>
#include <unordered_map>
using std::unordered_map;
using std::unordered_multimap;
#include <string>
#include <utility>
using std::pair; using std::make_pair;

#include <fst/cache.h>
#include <fst/arc-map.h>
#include <fst/mutable-fst.h>


namespace fst {

// StateMapper Interface - class determinies how states are mapped.
// Useful for implementing operations that do not change the number of states.
//
// class StateMapper {
//  public:
//   typedef A FromArc;
//   typedef B ToArc;
//
//   // Typical constructor
//   StateMapper(const Fst<A> &fst);
//   // Required copy constructor that allows updating Fst argument;
//   // pass only if relevant and changed.
//   StateMapper(const StateMapper &mapper, const Fst<A> *fst = 0);
//
//   // Specifies initial state of result
//   B::StateId Start() const;
//   // Specifies state's final weight in result
//   B::Weight Final(B::StateId s) const;
//
//   // These methods iterate through a state's arcs in result
//   // Specifies state to iterate over
//   void SetState(B::StateId s);
//   // End of arcs?
//   bool Done() const;
//   // Current arc

//   const B &Value() const;
//   // Advance to next arc (when !Done)
//   void Next();
//
//   // Specifies input symbol table action the mapper requires (see above).
//   MapSymbolsAction InputSymbolsAction() const;
//   // Specifies output symbol table action the mapper requires (see above).
//   MapSymbolsAction OutputSymbolsAction() const;
//   // This specifies the known properties of an Fst mapped by this
//   // mapper. It takes as argument the input Fst's known properties.
//   uint64 Properties(uint64 props) const;
// };
//
// We include a various state map versions below. One dimension of
// variation is whether the mapping mutates its input, writes to a
// new result Fst, or is an on-the-fly Fst. Another dimension is how
// we pass the mapper. We allow passing the mapper by pointer
// for cases that we need to change the state of the user's mapper.
// We also include map versions that pass the mapper
// by value or const reference when this suffices.

// Maps an arc type A using a mapper function object C, passed
// by pointer.  This version modifies its Fst input.
template<class A, class C>
void StateMap(MutableFst<A> *fst, C* mapper) {
  typedef typename A::StateId StateId;
  typedef typename A::Weight Weight;

  if (mapper->InputSymbolsAction() == MAP_CLEAR_SYMBOLS)
    fst->SetInputSymbols(0);

  if (mapper->OutputSymbolsAction() == MAP_CLEAR_SYMBOLS)
    fst->SetOutputSymbols(0);

  if (fst->Start() == kNoStateId)
    return;

  uint64 props = fst->Properties(kFstProperties, false);

  fst->SetStart(mapper->Start());

  for (StateId s = 0; s < fst->NumStates(); ++s) {
    mapper->SetState(s);
    fst->DeleteArcs(s);
    for (; !mapper->Done(); mapper->Next())
      fst->AddArc(s, mapper->Value());
    fst->SetFinal(s, mapper->Final(s));
  }

  fst->SetProperties(mapper->Properties(props), kFstProperties);
}

// Maps an arc type A using a mapper function object C, passed
// by value.  This version modifies its Fst input.
template<class A, class C>
void StateMap(MutableFst<A> *fst, C mapper) {
  StateMap(fst, &mapper);
}


// Maps an arc type A to an arc type B using mapper function
// object C, passed by pointer. This version writes the mapped
// input Fst to an output MutableFst.
template<class A, class B, class C>
void StateMap(const Fst<A> &ifst, MutableFst<B> *ofst, C* mapper) {
  typedef typename A::StateId StateId;
  typedef typename A::Weight Weight;

  ofst->DeleteStates();

  if (mapper->InputSymbolsAction() == MAP_COPY_SYMBOLS)
    ofst->SetInputSymbols(ifst.InputSymbols());
  else if (mapper->InputSymbolsAction() == MAP_CLEAR_SYMBOLS)
    ofst->SetInputSymbols(0);

  if (mapper->OutputSymbolsAction() == MAP_COPY_SYMBOLS)
    ofst->SetOutputSymbols(ifst.OutputSymbols());
  else if (mapper->OutputSymbolsAction() == MAP_CLEAR_SYMBOLS)
    ofst->SetOutputSymbols(0);

  uint64 iprops = ifst.Properties(kCopyProperties, false);

  if (ifst.Start() == kNoStateId) {
    if (iprops & kError) ofst->SetProperties(kError, kError);
    return;
  }

  // Add all states.
  if (ifst.Properties(kExpanded, false))
    ofst->ReserveStates(CountStates(ifst));
  for (StateIterator< Fst<A> > siter(ifst); !siter.Done(); siter.Next())
    ofst->AddState();

  ofst->SetStart(mapper->Start());

  for (StateIterator< Fst<A> > siter(ifst); !siter.Done(); siter.Next()) {
    StateId s = siter.Value();
    mapper->SetState(s);
    for (; !mapper->Done(); mapper->Next())
      ofst->AddArc(s, mapper->Value());
    ofst->SetFinal(s, mapper->Final(s));
  }

  uint64 oprops = ofst->Properties(kFstProperties, false);
  ofst->SetProperties(mapper->Properties(iprops) | oprops, kFstProperties);
}

// Maps an arc type A to an arc type B using mapper function
// object C, passed by value. This version writes the mapped input
// Fst to an output MutableFst.
template<class A, class B, class C>
void StateMap(const Fst<A> &ifst, MutableFst<B> *ofst, C mapper) {
  StateMap(ifst, ofst, &mapper);
}

typedef CacheOptions StateMapFstOptions;

template <class A, class B, class C> class StateMapFst;

// Implementation of delayed StateMapFst.
template <class A, class B, class C>
class StateMapFstImpl : public CacheImpl<B> {
 public:
  using FstImpl<B>::SetType;
  using FstImpl<B>::SetProperties;
  using FstImpl<B>::SetInputSymbols;
  using FstImpl<B>::SetOutputSymbols;

  using CacheImpl<B>::PushArc;
  using CacheImpl<B>::HasArcs;
  using CacheImpl<B>::HasFinal;
  using CacheImpl<B>::HasStart;
  using CacheImpl<B>::SetArcs;
  using CacheImpl<B>::SetFinal;
  using CacheImpl<B>::SetStart;

  friend class StateIterator< StateMapFst<A, B, C> >;

  typedef B Arc;
  typedef typename B::Weight Weight;
  typedef typename B::StateId StateId;

  StateMapFstImpl(const Fst<A> &fst, const C &mapper,
                 const StateMapFstOptions& opts)
      : CacheImpl<B>(opts),
        fst_(fst.Copy()),
        mapper_(new C(mapper, fst_)),
        own_mapper_(true) {
    Init();
  }

  StateMapFstImpl(const Fst<A> &fst, C *mapper,
                 const StateMapFstOptions& opts)
      : CacheImpl<B>(opts),
        fst_(fst.Copy()),
        mapper_(mapper),
        own_mapper_(false) {
    Init();
  }

  StateMapFstImpl(const StateMapFstImpl<A, B, C> &impl)
      : CacheImpl<B>(impl),
        fst_(impl.fst_->Copy(true)),
        mapper_(new C(*impl.mapper_, fst_)),
        own_mapper_(true) {
    Init();
  }

  ~StateMapFstImpl() {
    delete fst_;
    if (own_mapper_) delete mapper_;
  }

  StateId Start() {
    if (!HasStart())
      SetStart(mapper_->Start());
    return CacheImpl<B>::Start();
  }

  Weight Final(StateId s) {
    if (!HasFinal(s))
      SetFinal(s, mapper_->Final(s));
    return CacheImpl<B>::Final(s);
  }

  size_t NumArcs(StateId s) {
    if (!HasArcs(s))
      Expand(s);
    return CacheImpl<B>::NumArcs(s);
  }

  size_t NumInputEpsilons(StateId s) {
    if (!HasArcs(s))
      Expand(s);
    return CacheImpl<B>::NumInputEpsilons(s);
  }

  size_t NumOutputEpsilons(StateId s) {
    if (!HasArcs(s))
      Expand(s);
    return CacheImpl<B>::NumOutputEpsilons(s);
  }

  void InitStateIterator(StateIteratorData<A> *data) const {
    fst_->InitStateIterator(data);
  }

  void InitArcIterator(StateId s, ArcIteratorData<B> *data) {
    if (!HasArcs(s))
      Expand(s);
    CacheImpl<B>::InitArcIterator(s, data);
  }

  uint64 Properties() const { return Properties(kFstProperties); }

  // Set error if found; return FST impl properties.
  uint64 Properties(uint64 mask) const {
    if ((mask & kError) && (fst_->Properties(kError, false) ||
                            (mapper_->Properties(0) & kError)))
      SetProperties(kError, kError);
    return FstImpl<Arc>::Properties(mask);
  }

  void Expand(StateId s) {
    // Add exiting arcs.
    for (mapper_->SetState(s); !mapper_->Done(); mapper_->Next())
      PushArc(s, mapper_->Value());
    SetArcs(s);
  }

  const Fst<A> &GetFst() const {
    return *fst_;
  }

 private:
  void Init() {
    SetType("statemap");

    if (mapper_->InputSymbolsAction() == MAP_COPY_SYMBOLS)
      SetInputSymbols(fst_->InputSymbols());
    else if (mapper_->InputSymbolsAction() == MAP_CLEAR_SYMBOLS)
      SetInputSymbols(0);

    if (mapper_->OutputSymbolsAction() == MAP_COPY_SYMBOLS)
      SetOutputSymbols(fst_->OutputSymbols());
    else if (mapper_->OutputSymbolsAction() == MAP_CLEAR_SYMBOLS)
      SetOutputSymbols(0);

    uint64 props = fst_->Properties(kCopyProperties, false);
    SetProperties(mapper_->Properties(props));
  }

  const Fst<A> *fst_;
  C*  mapper_;
  bool own_mapper_;

  void operator=(const StateMapFstImpl<A, B, C> &);  // disallow
};


// Maps an arc type A to an arc type B using Mapper function object
// C. This version is a delayed Fst.
template <class A, class B, class C>
class StateMapFst : public ImplToFst< StateMapFstImpl<A, B, C> > {
 public:
  friend class ArcIterator< StateMapFst<A, B, C> >;

  typedef B Arc;
  typedef typename B::Weight Weight;
  typedef typename B::StateId StateId;
  typedef DefaultCacheStore<B> Store;
  typedef typename Store::State State;
  typedef StateMapFstImpl<A, B, C> Impl;

  StateMapFst(const Fst<A> &fst, const C &mapper,
              const StateMapFstOptions& opts)
      : ImplToFst<Impl>(new Impl(fst, mapper, opts)) {}

  StateMapFst(const Fst<A> &fst, C* mapper, const StateMapFstOptions& opts)
      : ImplToFst<Impl>(new Impl(fst, mapper, opts)) {}

  StateMapFst(const Fst<A> &fst, const C &mapper)
      : ImplToFst<Impl>(new Impl(fst, mapper, StateMapFstOptions())) {}

  StateMapFst(const Fst<A> &fst, C* mapper)
      : ImplToFst<Impl>(new Impl(fst, mapper, StateMapFstOptions())) {}

  // See Fst<>::Copy() for doc.
  StateMapFst(const StateMapFst<A, B, C> &fst, bool safe = false)
    : ImplToFst<Impl>(fst, safe) {}

  // Get a copy of this StateMapFst. See Fst<>::Copy() for further doc.
  virtual StateMapFst<A, B, C> *Copy(bool safe = false) const {
    return new StateMapFst<A, B, C>(*this, safe);
  }

  virtual void InitStateIterator(StateIteratorData<A> *data) const {
    GetImpl()->InitStateIterator(data);
  }

  virtual void InitArcIterator(StateId s, ArcIteratorData<B> *data) const {
    GetImpl()->InitArcIterator(s, data);
  }

 protected:
  Impl *GetImpl() const { return ImplToFst<Impl>::GetImpl(); }

 private:
  void operator=(const StateMapFst<A, B, C> &fst);  // disallow
};


// Specialization for StateMapFst.
template <class A, class B, class C>
class ArcIterator< StateMapFst<A, B, C> >
    : public CacheArcIterator< StateMapFst<A, B, C> > {
 public:
  typedef typename A::StateId StateId;

  ArcIterator(const StateMapFst<A, B, C> &fst, StateId s)
      : CacheArcIterator< StateMapFst<A, B, C> >(fst.GetImpl(), s) {
    if (!fst.GetImpl()->HasArcs(s))
      fst.GetImpl()->Expand(s);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcIterator);
};

//
// Utility Mappers
//

// Mapper that returns its input.
template <class A>
class IdentityStateMapper {
 public:
  typedef A FromArc;
  typedef A ToArc;

  typedef typename A::StateId StateId;
  typedef typename A::Weight Weight;

  explicit IdentityStateMapper(const Fst<A> &fst) : fst_(fst), aiter_(0) {}

  // Allows updating Fst argument; pass only if changed.
  IdentityStateMapper(const IdentityStateMapper<A> &mapper,
                      const Fst<A> *fst = 0)
      : fst_(fst ? *fst : mapper.fst_), aiter_(0) {}

  ~IdentityStateMapper() { delete aiter_; }

  StateId Start() const { return fst_.Start(); }

  Weight Final(StateId s) const { return fst_.Final(s); }

  void SetState(StateId s) {
    if (aiter_) delete aiter_;
    aiter_ = new ArcIterator< Fst<A> >(fst_, s);
  }

  bool Done() const { return aiter_->Done(); }
  const A &Value() const { return aiter_->Value(); }
  void Next() { aiter_->Next(); }

  MapSymbolsAction InputSymbolsAction() const { return MAP_COPY_SYMBOLS; }
  MapSymbolsAction OutputSymbolsAction() const { return MAP_COPY_SYMBOLS;}

  uint64 Properties(uint64 props) const { return props; }

 private:
  const Fst<A> &fst_;
  ArcIterator< Fst<A> > *aiter_;
};

template <class A>
class ArcSumMapper {
 public:
  typedef A FromArc;
  typedef A ToArc;

  typedef typename A::StateId StateId;
  typedef typename A::Weight Weight;

  explicit ArcSumMapper(const Fst<A> &fst) : fst_(fst), i_(0) {}

  // Allows updating Fst argument; pass only if changed.
  ArcSumMapper(const ArcSumMapper<A> &mapper,
               const Fst<A> *fst = 0)
      : fst_(fst ? *fst : mapper.fst_), i_(0) {}

  StateId Start() const { return fst_.Start(); }
  Weight Final(StateId s) const { return fst_.Final(s); }

  void SetState(StateId s) {
    i_ = 0;
    arcs_.clear();
    arcs_.reserve(fst_.NumArcs(s));
    for (ArcIterator<Fst<A> > aiter(fst_, s); !aiter.Done(); aiter.Next())
      arcs_.push_back(aiter.Value());

    // First sorts the exiting arcs by input label, output label
    // and destination state and then sums weights of arcs with
    // the same input label, output label, and destination state.
    std::sort(arcs_.begin(), arcs_.end(), comp_);
    size_t narcs = 0;
    for (size_t i = 0; i < arcs_.size(); ++i) {
      if (narcs > 0 && equal_(arcs_[i], arcs_[narcs - 1])) {
        arcs_[narcs - 1].weight = Plus(arcs_[narcs - 1].weight,
                                       arcs_[i].weight);
      } else {
        arcs_[narcs++] = arcs_[i];
      }
    }
    arcs_.resize(narcs);
  }

  bool Done() const { return i_ >= arcs_.size(); }
  const A &Value() const { return arcs_[i_]; }
  void Next() { ++i_; }

  MapSymbolsAction InputSymbolsAction() const { return MAP_COPY_SYMBOLS; }
  MapSymbolsAction OutputSymbolsAction() const { return MAP_COPY_SYMBOLS; }

  uint64 Properties(uint64 props) const {
    return props & kArcSortProperties &
        kDeleteArcsProperties & kWeightInvariantProperties;
  }

 private:
  struct Compare {
    bool operator()(const A &x, const A &y) const {
      if (x.ilabel < y.ilabel) return true;
      if (x.ilabel > y.ilabel) return false;
      if (x.olabel < y.olabel) return true;
      if (x.olabel > y.olabel) return false;
      if (x.nextstate < y.nextstate) return true;
      if (x.nextstate > y.nextstate) return false;
      return false;
    }
  };

  struct Equal {
    bool operator()(const A& x, const A& y) {
      return (x.ilabel == y.ilabel &&
              x.olabel == y.olabel &&
              x.nextstate == y.nextstate);
    }
  };

  const Fst<A> &fst_;
  Compare comp_;
  Equal equal_;
  vector<A> arcs_;
  ssize_t i_;               // current arc position

  void operator=(const ArcSumMapper<A> &);  // disallow
};

template <class A>
class ArcUniqueMapper {
 public:
  typedef A FromArc;
  typedef A ToArc;

  typedef typename A::StateId StateId;
  typedef typename A::Weight Weight;

  explicit ArcUniqueMapper(const Fst<A> &fst) : fst_(fst), i_(0) {}

  // Allows updating Fst argument; pass only if changed.
  ArcUniqueMapper(const ArcUniqueMapper<A> &mapper,
                  const Fst<A> *fst = 0)
      : fst_(fst ? *fst : mapper.fst_), i_(0) {}

  StateId Start() const { return fst_.Start(); }
  Weight Final(StateId s) const { return fst_.Final(s); }

  void SetState(StateId s) {
    i_ = 0;
    arcs_.clear();
    arcs_.reserve(fst_.NumArcs(s));
    for (ArcIterator<Fst<A> > aiter(fst_, s); !aiter.Done(); aiter.Next())
      arcs_.push_back(aiter.Value());

    // First sorts the exiting arcs by input label, output label
    // and destination state and then uniques identical arcs
    std::sort(arcs_.begin(), arcs_.end(), comp_);
    typename vector<A>::iterator unique_end =
        std::unique(arcs_.begin(), arcs_.end(), equal_);
    arcs_.resize(unique_end - arcs_.begin());
  }

  bool Done() const { return i_ >= arcs_.size(); }
  const A &Value() const { return arcs_[i_]; }
  void Next() { ++i_; }

  MapSymbolsAction InputSymbolsAction() const { return MAP_COPY_SYMBOLS; }
  MapSymbolsAction OutputSymbolsAction() const { return MAP_COPY_SYMBOLS; }

  uint64 Properties(uint64 props) const {
    return props & kArcSortProperties & kDeleteArcsProperties;
  }

 private:
  struct Compare {
    bool operator()(const A &x, const A &y) const {
      if (x.ilabel < y.ilabel) return true;
      if (x.ilabel > y.ilabel) return false;
      if (x.olabel < y.olabel) return true;
      if (x.olabel > y.olabel) return false;
      if (x.nextstate < y.nextstate) return true;
      if (x.nextstate > y.nextstate) return false;
      return false;
    }
  };

  struct Equal {
    bool operator()(const A &x, const A &y) const {
      return (x.ilabel == y.ilabel &&
              x.olabel == y.olabel &&
              x.nextstate == y.nextstate &&
              x.weight == y.weight);
    }
  };

  const Fst<A> &fst_;
  Compare comp_;
  Equal equal_;
  vector<A> arcs_;
  ssize_t i_;               // current arc position

  void operator=(const ArcUniqueMapper<A> &);  // disallow
};


}  // namespace fst

#endif  // FST_LIB_STATE_MAP_H__
