// arc-map.h

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
// Class to map over/transform arcs e.g., change semirings or
// implement project/invert. Consider using when operation does
// not change the number of arcs (except possibly superfinal arcs).

#ifndef FST_LIB_ARC_MAP_H__
#define FST_LIB_ARC_MAP_H__

#include <unordered_map>
using std::unordered_map;
using std::unordered_multimap;
#include <string>
#include <utility>
using std::pair; using std::make_pair;

#include <fst/cache.h>
#include <fst/mutable-fst.h>


namespace fst {

// This determines how final weights are mapped.
enum MapFinalAction {
  // A final weight is mapped into a final weight. An error
  // is raised if this is not possible.
  MAP_NO_SUPERFINAL,

  // A final weight is mapped to an arc to the superfinal state
  // when the result cannot be represented as a final weight.
  // The superfinal state will be added only if it is needed.
  MAP_ALLOW_SUPERFINAL,

  // A final weight is mapped to an arc to the superfinal state
  // unless the result can be represented as a final weight of weight
  // Zero(). The superfinal state is always added (if the input is
  // not the empty Fst).
  MAP_REQUIRE_SUPERFINAL
};

// This determines how symbol tables are mapped.
enum MapSymbolsAction {
  // Symbols should be cleared in the result by the map.
  MAP_CLEAR_SYMBOLS,

  // Symbols should be copied from the input FST by the map.
  MAP_COPY_SYMBOLS,

  // Symbols should not be modified in the result by the map itself.
  // (They may set by the mapper).
  MAP_NOOP_SYMBOLS
};

// ArcMapper Interface - class determinies how arcs and final weights
// are mapped. Useful for implementing operations that do not change
// the number of arcs (expect possibly superfinal arcs).
//
// class ArcMapper {
//  public:
//   typedef A FromArc;
//   typedef B ToArc;
//
//   // Maps an arc type A to arc type B.
//   B operator()(const A &arc);
//   // Specifies final action the mapper requires (see above).
//   // The mapper will be passed final weights as arcs of the
//   // form A(0, 0, weight, kNoStateId).
//   MapFinalAction FinalAction() const;
//   // Specifies input symbol table action the mapper requires (see above).
//   MapSymbolsAction InputSymbolsAction() const;
//   // Specifies output symbol table action the mapper requires (see above).
//   MapSymbolsAction OutputSymbolsAction() const;
//   // This specifies the known properties of an Fst mapped by this
//   // mapper. It takes as argument the input Fst's known properties.
//   uint64 Properties(uint64 props) const;
// };
//
// The ArcMap functions and classes below will use the FinalAction()
// method of the mapper to determine how to treat final weights,
// e.g. whether to add a superfinal state. They will use the Properties()
// method to set the result Fst properties.
//
// We include a various map versions below. One dimension of
// variation is whether the mapping mutates its input, writes to a
// new result Fst, or is an on-the-fly Fst. Another dimension is how
// we pass the mapper. We allow passing the mapper by pointer
// for cases that we need to change the state of the user's mapper.
// This is the case with the encode mapper, which is reused during
// decoding. We also include map versions that pass the mapper
// by value or const reference when this suffices.


// Maps an arc type A using a mapper function object C, passed
// by pointer.  This version modifies its Fst input.
template<class A, class C>
void ArcMap(MutableFst<A> *fst, C* mapper) {
  typedef typename A::StateId StateId;
  typedef typename A::Weight Weight;

  if (mapper->InputSymbolsAction() == MAP_CLEAR_SYMBOLS)
    fst->SetInputSymbols(0);

  if (mapper->OutputSymbolsAction() == MAP_CLEAR_SYMBOLS)
    fst->SetOutputSymbols(0);

  if (fst->Start() == kNoStateId)
    return;

  uint64 props = fst->Properties(kFstProperties, false);

  MapFinalAction final_action = mapper->FinalAction();
  StateId superfinal = kNoStateId;
  if (final_action == MAP_REQUIRE_SUPERFINAL) {
    superfinal = fst->AddState();
    fst->SetFinal(superfinal, Weight::One());
  }

  for (StateId s = 0; s < fst->NumStates(); ++s) {
    for (MutableArcIterator< MutableFst<A> > aiter(fst, s);
         !aiter.Done(); aiter.Next()) {
      const A &arc = aiter.Value();
      aiter.SetValue((*mapper)(arc));
    }

    switch (final_action) {
      case MAP_NO_SUPERFINAL:
      default: {
        A final_arc = (*mapper)(A(0, 0, fst->Final(s), kNoStateId));
        if (final_arc.ilabel != 0 || final_arc.olabel != 0) {
          FSTERROR() << "ArcMap: non-zero arc labels for superfinal arc";
          fst->SetProperties(kError, kError);
        }

        fst->SetFinal(s, final_arc.weight);
        break;
      }
      case MAP_ALLOW_SUPERFINAL: {
        if (s != superfinal) {
          A final_arc = (*mapper)(A(0, 0, fst->Final(s), kNoStateId));
          if (final_arc.ilabel != 0 || final_arc.olabel != 0) {
            // Add a superfinal state if not already done.
            if (superfinal == kNoStateId) {
              superfinal = fst->AddState();
              fst->SetFinal(superfinal, Weight::One());
            }
            final_arc.nextstate = superfinal;
            fst->AddArc(s, final_arc);
            fst->SetFinal(s, Weight::Zero());
          } else {
            fst->SetFinal(s, final_arc.weight);
          }
        }
        break;
      }
      case MAP_REQUIRE_SUPERFINAL: {
        if (s != superfinal) {
          A final_arc = (*mapper)(A(0, 0, fst->Final(s), kNoStateId));
          if (final_arc.ilabel != 0 || final_arc.olabel != 0 ||
              final_arc.weight != Weight::Zero())
            fst->AddArc(s, A(final_arc.ilabel, final_arc.olabel,
                             final_arc.weight, superfinal));
            fst->SetFinal(s, Weight::Zero());
        }
        break;
      }
    }
  }
  fst->SetProperties(mapper->Properties(props), kFstProperties);
}


// Maps an arc type A using a mapper function object C, passed
// by value.  This version modifies its Fst input.
template<class A, class C>
void ArcMap(MutableFst<A> *fst, C mapper) {
  ArcMap(fst, &mapper);
}


// Maps an arc type A to an arc type B using mapper function
// object C, passed by pointer. This version writes the mapped
// input Fst to an output MutableFst.
template<class A, class B, class C>
void ArcMap(const Fst<A> &ifst, MutableFst<B> *ofst, C* mapper) {
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

  MapFinalAction final_action = mapper->FinalAction();
  if (ifst.Properties(kExpanded, false)) {
    ofst->ReserveStates(CountStates(ifst) +
                        final_action == MAP_NO_SUPERFINAL ? 0 : 1);
  }

  // Add all states.
  for (StateIterator< Fst<A> > siter(ifst); !siter.Done(); siter.Next())
    ofst->AddState();

  StateId superfinal = kNoStateId;
  if (final_action == MAP_REQUIRE_SUPERFINAL) {
    superfinal = ofst->AddState();
    ofst->SetFinal(superfinal, B::Weight::One());
  }
  for (StateIterator< Fst<A> > siter(ifst); !siter.Done(); siter.Next()) {
    StateId s = siter.Value();
    if (s == ifst.Start())
      ofst->SetStart(s);

    ofst->ReserveArcs(s, ifst.NumArcs(s));
    for (ArcIterator< Fst<A> > aiter(ifst, s); !aiter.Done(); aiter.Next())
      ofst->AddArc(s, (*mapper)(aiter.Value()));

    switch (final_action) {
      case MAP_NO_SUPERFINAL:
      default: {
        B final_arc = (*mapper)(A(0, 0, ifst.Final(s), kNoStateId));
        if (final_arc.ilabel != 0 || final_arc.olabel != 0) {
          FSTERROR() << "ArcMap: non-zero arc labels for superfinal arc";
          ofst->SetProperties(kError, kError);
        }
        ofst->SetFinal(s, final_arc.weight);
        break;
      }
      case MAP_ALLOW_SUPERFINAL: {
        B final_arc = (*mapper)(A(0, 0, ifst.Final(s), kNoStateId));
        if (final_arc.ilabel != 0 || final_arc.olabel != 0) {
            // Add a superfinal state if not already done.
          if (superfinal == kNoStateId) {
            superfinal = ofst->AddState();
            ofst->SetFinal(superfinal, B::Weight::One());
          }
          final_arc.nextstate = superfinal;
          ofst->AddArc(s, final_arc);
          ofst->SetFinal(s, B::Weight::Zero());
        } else {
          ofst->SetFinal(s, final_arc.weight);
        }
        break;
      }
      case MAP_REQUIRE_SUPERFINAL: {
        B final_arc = (*mapper)(A(0, 0, ifst.Final(s), kNoStateId));
        if (final_arc.ilabel != 0 || final_arc.olabel != 0 ||
            final_arc.weight != B::Weight::Zero())
          ofst->AddArc(s, B(final_arc.ilabel, final_arc.olabel,
                            final_arc.weight, superfinal));
        ofst->SetFinal(s, B::Weight::Zero());
        break;
      }
    }
  }
  uint64 oprops = ofst->Properties(kFstProperties, false);
  ofst->SetProperties(mapper->Properties(iprops) | oprops, kFstProperties);
}

// Maps an arc type A to an arc type B using mapper function
// object C, passed by value. This version writes the mapped input
// Fst to an output MutableFst.
template<class A, class B, class C>
void ArcMap(const Fst<A> &ifst, MutableFst<B> *ofst, C mapper) {
  ArcMap(ifst, ofst, &mapper);
}


struct ArcMapFstOptions : public CacheOptions {
  // ArcMapFst default caching behaviour is to do no caching. Most
  // mappers are cheap and therefore we save memory by not doing
  // caching.
  ArcMapFstOptions() : CacheOptions(true, 0) {}
  ArcMapFstOptions(const CacheOptions& opts) : CacheOptions(opts) {}
};


template <class A, class B, class C> class ArcMapFst;

// Implementation of delayed ArcMapFst.
template <class A, class B, class C>
class ArcMapFstImpl : public CacheImpl<B> {
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

  friend class StateIterator< ArcMapFst<A, B, C> >;

  typedef B Arc;
  typedef typename B::Weight Weight;
  typedef typename B::StateId StateId;

  ArcMapFstImpl(const Fst<A> &fst, const C &mapper,
                 const ArcMapFstOptions& opts)
      : CacheImpl<B>(opts),
        fst_(fst.Copy()),
        mapper_(new C(mapper)),
        own_mapper_(true),
        superfinal_(kNoStateId),
        nstates_(0) {
    Init();
  }

  ArcMapFstImpl(const Fst<A> &fst, C *mapper,
                 const ArcMapFstOptions& opts)
      : CacheImpl<B>(opts),
        fst_(fst.Copy()),
        mapper_(mapper),
        own_mapper_(false),
        superfinal_(kNoStateId),
        nstates_(0) {
    Init();
  }

  ArcMapFstImpl(const ArcMapFstImpl<A, B, C> &impl)
      : CacheImpl<B>(impl),
        fst_(impl.fst_->Copy(true)),
        mapper_(new C(*impl.mapper_)),
        own_mapper_(true),
        superfinal_(kNoStateId),
        nstates_(0) {
    Init();
  }

  ~ArcMapFstImpl() {
    delete fst_;
    if (own_mapper_) delete mapper_;
  }

  StateId Start() {
    if (!HasStart())
      SetStart(FindOState(fst_->Start()));
    return CacheImpl<B>::Start();
  }

  Weight Final(StateId s) {
    if (!HasFinal(s)) {
      switch (final_action_) {
        case MAP_NO_SUPERFINAL:
        default: {
          B final_arc = (*mapper_)(A(0, 0, fst_->Final(FindIState(s)),
                                        kNoStateId));
          if (final_arc.ilabel != 0 || final_arc.olabel != 0) {
            FSTERROR() << "ArcMapFst: non-zero arc labels for superfinal arc";
            SetProperties(kError, kError);
          }
          SetFinal(s, final_arc.weight);
          break;
        }
        case MAP_ALLOW_SUPERFINAL: {
          if (s == superfinal_) {
            SetFinal(s, Weight::One());
          } else {
            B final_arc = (*mapper_)(A(0, 0, fst_->Final(FindIState(s)),
                                          kNoStateId));
            if (final_arc.ilabel == 0 && final_arc.olabel == 0)
              SetFinal(s, final_arc.weight);
            else
              SetFinal(s, Weight::Zero());
          }
          break;
        }
        case MAP_REQUIRE_SUPERFINAL: {
          SetFinal(s, s == superfinal_ ? Weight::One() : Weight::Zero());
          break;
        }
      }
    }
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

  uint64 Properties() const { return Properties(kFstProperties); }

  // Set error if found; return FST impl properties.
  uint64 Properties(uint64 mask) const {
    if ((mask & kError) && (fst_->Properties(kError, false) ||
                            (mapper_->Properties(0) & kError)))
      SetProperties(kError, kError);
    return FstImpl<Arc>::Properties(mask);
  }

  void InitArcIterator(StateId s, ArcIteratorData<B> *data) {
    if (!HasArcs(s))
      Expand(s);
    CacheImpl<B>::InitArcIterator(s, data);
  }

  void Expand(StateId s) {
    // Add exiting arcs.
    if (s == superfinal_) { SetArcs(s); return; }

    for (ArcIterator< Fst<A> > aiter(*fst_, FindIState(s));
         !aiter.Done(); aiter.Next()) {
      A aarc(aiter.Value());
      aarc.nextstate = FindOState(aarc.nextstate);
      const B& barc = (*mapper_)(aarc);
      PushArc(s, barc);
    }

    // Check for superfinal arcs.
    if (!HasFinal(s) || Final(s) == Weight::Zero())
      switch (final_action_) {
        case MAP_NO_SUPERFINAL:
        default:
          break;
        case MAP_ALLOW_SUPERFINAL: {
          B final_arc = (*mapper_)(A(0, 0, fst_->Final(FindIState(s)),
                                        kNoStateId));
          if (final_arc.ilabel != 0 || final_arc.olabel != 0) {
            if (superfinal_ == kNoStateId)
              superfinal_ = nstates_++;
            final_arc.nextstate = superfinal_;
            PushArc(s, final_arc);
          }
          break;
        }
      case MAP_REQUIRE_SUPERFINAL: {
        B final_arc = (*mapper_)(A(0, 0, fst_->Final(FindIState(s)),
                                      kNoStateId));
        if (final_arc.ilabel != 0 || final_arc.olabel != 0 ||
            final_arc.weight != B::Weight::Zero())
          PushArc(s, B(final_arc.ilabel, final_arc.olabel,
                      final_arc.weight, superfinal_));
        break;
      }
    }
    SetArcs(s);
  }

 private:
  void Init() {
    SetType("map");

    if (mapper_->InputSymbolsAction() == MAP_COPY_SYMBOLS)
      SetInputSymbols(fst_->InputSymbols());
    else if (mapper_->InputSymbolsAction() == MAP_CLEAR_SYMBOLS)
      SetInputSymbols(0);

    if (mapper_->OutputSymbolsAction() == MAP_COPY_SYMBOLS)
      SetOutputSymbols(fst_->OutputSymbols());
    else if (mapper_->OutputSymbolsAction() == MAP_CLEAR_SYMBOLS)
      SetOutputSymbols(0);

    if (fst_->Start() == kNoStateId) {
      final_action_ = MAP_NO_SUPERFINAL;
      SetProperties(kNullProperties);
    } else {
      final_action_ = mapper_->FinalAction();
      uint64 props = fst_->Properties(kCopyProperties, false);
      SetProperties(mapper_->Properties(props));
      if (final_action_ == MAP_REQUIRE_SUPERFINAL)
        superfinal_ = 0;
    }
  }

  // Maps from output state to input state.
  StateId FindIState(StateId s) {
    if (superfinal_ == kNoStateId || s < superfinal_)
      return s;
    else
      return s - 1;
  }

  // Maps from input state to output state.
  StateId FindOState(StateId is) {
    StateId os;
    if (superfinal_ == kNoStateId || is < superfinal_)
      os = is;
    else
      os = is + 1;

    if (os >= nstates_)
      nstates_ = os + 1;

    return os;
  }


  const Fst<A> *fst_;
  C*   mapper_;
  bool own_mapper_;
  MapFinalAction final_action_;

  StateId superfinal_;
  StateId nstates_;

  void operator=(const ArcMapFstImpl<A, B, C> &);  // disallow
};


// Maps an arc type A to an arc type B using Mapper function object
// C. This version is a delayed Fst.
template <class A, class B, class C>
class ArcMapFst : public ImplToFst< ArcMapFstImpl<A, B, C> > {
 public:
  friend class ArcIterator< ArcMapFst<A, B, C> >;
  friend class StateIterator< ArcMapFst<A, B, C> >;

  typedef B Arc;
  typedef typename B::Weight Weight;
  typedef typename B::StateId StateId;
  typedef DefaultCacheStore<B> Store;
  typedef typename Store::State State;
  typedef ArcMapFstImpl<A, B, C> Impl;

  ArcMapFst(const Fst<A> &fst, const C &mapper, const ArcMapFstOptions& opts)
      : ImplToFst<Impl>(new Impl(fst, mapper, opts)) {}

  ArcMapFst(const Fst<A> &fst, C* mapper, const ArcMapFstOptions& opts)
      : ImplToFst<Impl>(new Impl(fst, mapper, opts)) {}

  ArcMapFst(const Fst<A> &fst, const C &mapper)
      : ImplToFst<Impl>(new Impl(fst, mapper, ArcMapFstOptions())) {}

  ArcMapFst(const Fst<A> &fst, C* mapper)
      : ImplToFst<Impl>(new Impl(fst, mapper, ArcMapFstOptions())) {}

  // See Fst<>::Copy() for doc.
  ArcMapFst(const ArcMapFst<A, B, C> &fst, bool safe = false)
    : ImplToFst<Impl>(fst, safe) {}

  // Get a copy of this ArcMapFst. See Fst<>::Copy() for further doc.
  virtual ArcMapFst<A, B, C> *Copy(bool safe = false) const {
    return new ArcMapFst<A, B, C>(*this, safe);
  }

  virtual inline void InitStateIterator(StateIteratorData<B> *data) const;

  virtual void InitArcIterator(StateId s, ArcIteratorData<B> *data) const {
    GetImpl()->InitArcIterator(s, data);
  }

 private:
  // Makes visible to friends.
  Impl *GetImpl() const { return ImplToFst<Impl>::GetImpl(); }

  void operator=(const ArcMapFst<A, B, C> &fst);  // disallow
};


// Specialization for ArcMapFst.
template<class A, class B, class C>
class StateIterator< ArcMapFst<A, B, C> > : public StateIteratorBase<B> {
 public:
  typedef typename B::StateId StateId;

  explicit StateIterator(const ArcMapFst<A, B, C> &fst)
      : impl_(fst.GetImpl()), siter_(*impl_->fst_), s_(0),
        superfinal_(impl_->final_action_ == MAP_REQUIRE_SUPERFINAL)
  { CheckSuperfinal(); }

  bool Done() const { return siter_.Done() && !superfinal_; }

  StateId Value() const { return s_; }

  void Next() {
    ++s_;
    if (!siter_.Done()) {
      siter_.Next();
      CheckSuperfinal();
    }
    else if (superfinal_)
      superfinal_ = false;
  }

  void Reset() {
    s_ = 0;
    siter_.Reset();
    superfinal_ = impl_->final_action_ == MAP_REQUIRE_SUPERFINAL;
    CheckSuperfinal();
  }

 private:
  // This allows base-class virtual access to non-virtual derived-
  // class members of the same name. It makes the derived class more
  // efficient to use but unsafe to further derive.
  bool Done_() const { return Done(); }
  StateId Value_() const { return Value(); }
  void Next_() { Next(); }
  void Reset_() { Reset(); }

  void CheckSuperfinal() {
    if (impl_->final_action_ != MAP_ALLOW_SUPERFINAL || superfinal_)
      return;
    if (!siter_.Done()) {
      B final_arc = (*impl_->mapper_)(A(0, 0, impl_->fst_->Final(s_),
                                           kNoStateId));
      if (final_arc.ilabel != 0 || final_arc.olabel != 0)
        superfinal_ = true;
    }
  }

  const ArcMapFstImpl<A, B, C> *impl_;
  StateIterator< Fst<A> > siter_;
  StateId s_;
  bool superfinal_;    // true if there is a superfinal state and not done

  DISALLOW_COPY_AND_ASSIGN(StateIterator);
};


// Specialization for ArcMapFst.
template <class A, class B, class C>
class ArcIterator< ArcMapFst<A, B, C> >
    : public CacheArcIterator< ArcMapFst<A, B, C> > {
 public:
  typedef typename A::StateId StateId;

  ArcIterator(const ArcMapFst<A, B, C> &fst, StateId s)
      : CacheArcIterator< ArcMapFst<A, B, C> >(fst.GetImpl(), s) {
    if (!fst.GetImpl()->HasArcs(s))
      fst.GetImpl()->Expand(s);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcIterator);
};

template <class A, class B, class C> inline
void ArcMapFst<A, B, C>::InitStateIterator(StateIteratorData<B> *data)
    const {
  data->base = new StateIterator< ArcMapFst<A, B, C> >(*this);
}


//
// Utility Mappers
//

// Mapper that returns its input.
template <class A>
struct IdentityArcMapper {
  typedef A FromArc;
  typedef A ToArc;

  A operator()(const A &arc) const { return arc; }

  MapFinalAction FinalAction() const { return MAP_NO_SUPERFINAL; }

  MapSymbolsAction InputSymbolsAction() const { return MAP_COPY_SYMBOLS; }

  MapSymbolsAction OutputSymbolsAction() const { return MAP_COPY_SYMBOLS;}

  uint64 Properties(uint64 props) const { return props; }
};


// Mapper that returns its input with final states redirected to
// a single super-final state.
template <class A>
struct SuperFinalMapper {
  typedef A FromArc;
  typedef A ToArc;

  A operator()(const A &arc) const { return arc; }

  MapFinalAction FinalAction() const { return MAP_REQUIRE_SUPERFINAL; }

  MapSymbolsAction InputSymbolsAction() const { return MAP_COPY_SYMBOLS; }

  MapSymbolsAction OutputSymbolsAction() const { return MAP_COPY_SYMBOLS;}

  uint64 Properties(uint64 props) const {
    return props & kAddSuperFinalProperties;
  }
};


// Mapper that leaves labels and nextstate unchanged and constructs a new weight
// from the underlying value of the arc weight.  Requires that there is a
// WeightConvert class specialization that converts the weights.
template <class A, class B>
class WeightConvertMapper {
 public:
  typedef A FromArc;
  typedef B ToArc;
  typedef typename FromArc::Weight FromWeight;
  typedef typename ToArc::Weight ToWeight;

  ToArc operator()(const FromArc &arc) const {
    return ToArc(arc.ilabel, arc.olabel,
                 convert_weight_(arc.weight), arc.nextstate);
  }

  MapFinalAction FinalAction() const { return MAP_NO_SUPERFINAL; }

  MapSymbolsAction InputSymbolsAction() const { return MAP_COPY_SYMBOLS; }

  MapSymbolsAction OutputSymbolsAction() const { return MAP_COPY_SYMBOLS;}

  uint64 Properties(uint64 props) const { return props; }

 private:
  WeightConvert<FromWeight, ToWeight> convert_weight_;
};

// Non-precision-changing weight conversions.
// Consider using more efficient Cast (fst.h) instead.
typedef WeightConvertMapper<StdArc, LogArc> StdToLogMapper;
typedef WeightConvertMapper<LogArc, StdArc> LogToStdMapper;

// Precision-changing weight conversions.
typedef WeightConvertMapper<StdArc, Log64Arc> StdToLog64Mapper;
typedef WeightConvertMapper<LogArc, Log64Arc> LogToLog64Mapper;
typedef WeightConvertMapper<Log64Arc, StdArc> Log64ToStdMapper;
typedef WeightConvertMapper<Log64Arc, LogArc> Log64ToLogMapper;

// Mapper from A to GallicArc<A>.
template <class A, GallicType G = GALLIC_LEFT>
struct ToGallicMapper {
  typedef A FromArc;
  typedef GallicArc<A, G> ToArc;

  typedef StringWeight<typename A::Label, GALLIC_STRING_TYPE(G)> SW;
  typedef typename A::Weight AW;
  typedef typename GallicArc<A, G>::Weight GW;

  ToArc operator()(const A &arc) const {
    // 'Super-final' arc.
    if (arc.nextstate == kNoStateId && arc.weight != AW::Zero())
      return ToArc(0, 0, GW(SW::One(), arc.weight), kNoStateId);
    // 'Super-non-final' arc.
    else if (arc.nextstate == kNoStateId)
      return ToArc(0, 0, GW::Zero(), kNoStateId);
    // Epsilon label.
    else if (arc.olabel == 0)
      return ToArc(arc.ilabel, arc.ilabel,
                   GW(SW::One(), arc.weight), arc.nextstate);
    // Regular label.
    else
      return ToArc(arc.ilabel, arc.ilabel,
                   GW(SW(arc.olabel), arc.weight), arc.nextstate);
  }

  MapFinalAction FinalAction() const { return MAP_NO_SUPERFINAL; }

  MapSymbolsAction InputSymbolsAction() const { return MAP_COPY_SYMBOLS; }

  MapSymbolsAction OutputSymbolsAction() const { return MAP_CLEAR_SYMBOLS;}

  uint64 Properties(uint64 props) const {
    return ProjectProperties(props, true) & kWeightInvariantProperties;
  }
};


// Mapper from GallicArc<A> to A.
template <class A, GallicType G = GALLIC_LEFT>
struct FromGallicMapper {
  typedef GallicArc<A, G> FromArc;
  typedef A ToArc;

  typedef typename A::Label Label;
  typedef typename A::Weight AW;
  typedef typename GallicArc<A, G>::Weight GW;

  FromGallicMapper(Label superfinal_label = 0)
      : superfinal_label_(superfinal_label), error_(false) {}

  A operator()(const FromArc &arc) const {
    // 'Super-non-final' arc.
    if (arc.nextstate == kNoStateId && arc.weight == GW::Zero())
      return A(arc.ilabel, 0, AW::Zero(), kNoStateId);

    Label l = kNoLabel;
    AW w;
    if (!Extract(arc.weight, &w, &l) || arc.ilabel != arc.olabel) {
      FSTERROR() << "FromGallicMapper: unrepresentable weight: " << arc.weight
                 << " for arc with ilabel = " << arc.ilabel
                 << ", olabel = " << arc.olabel
                 << ", nextstate = " << arc.nextstate;
      error_ = true;
    }

    if (arc.ilabel == 0 && l != 0 && arc.nextstate == kNoStateId)
      return A(superfinal_label_, l, w, arc.nextstate);
    else
      return A(arc.ilabel, l, w, arc.nextstate);
  }

  MapFinalAction FinalAction() const { return MAP_ALLOW_SUPERFINAL; }

  MapSymbolsAction InputSymbolsAction() const { return MAP_COPY_SYMBOLS; }

  MapSymbolsAction OutputSymbolsAction() const { return MAP_CLEAR_SYMBOLS;}

  uint64 Properties(uint64 inprops) const {
    uint64 outprops = inprops & kOLabelInvariantProperties &
        kWeightInvariantProperties & kAddSuperFinalProperties;
    if (error_)
      outprops |= kError;
    return outprops;
  }

 private:
  template <GallicType GT>
  static bool Extract(
      const GallicWeight<Label, AW, GT> &gallic_weight,
      typename A::Weight *weight, typename A::Label *label) {
    StringWeight<Label, GALLIC_STRING_TYPE(GT)> w1 = gallic_weight.Value1();
    AW w2 = gallic_weight.Value2();
    StringWeightIterator<Label, GALLIC_STRING_TYPE(GT)> iter1(w1);

    Label l = w1.Size() == 1 ? iter1.Value() : 0;
    if (l == kStringInfinity || l == kStringBad || w1.Size() > 1)
      return false;

    *label = l;
    *weight = w2;
    return true;
  }

  static bool Extract(
      const GallicWeight<Label, AW, GALLIC> &gallic_weight,
      typename A::Weight *weight, typename A::Label *label) {
    if (gallic_weight.Size() > 1) return false;
    if (gallic_weight.Size() == 0) {
      *label = 0;
      *weight = A::Weight::Zero();
      return true;
    }
    return Extract<GALLIC_RESTRICT>(gallic_weight.Back(), weight, label);
  }

 private:
  Label superfinal_label_;
  mutable bool error_;
};

// Mapper from GallicArc<A> to A.
template <class A, GallicType G = GALLIC_LEFT>
struct GallicToNewSymbolsMapper {
  typedef GallicArc<A, G> FromArc;
  typedef A ToArc;

  typedef typename A::StateId StateId;
  typedef typename A::Label Label;
  typedef StringWeight<Label, GALLIC_STRING_TYPE(G)> SW;
  typedef typename A::Weight AW;
  typedef typename GallicArc<A, G>::Weight GW;

  GallicToNewSymbolsMapper(MutableFst<ToArc> *fst)
      : fst_(fst), lmax_(0), osymbols_(fst->OutputSymbols()),
        isymbols_(0), error_(false) {
    fst_->DeleteStates();
    state_ = fst_->AddState();
    fst_->SetStart(state_);
    fst_->SetFinal(state_, AW::One());
    if (osymbols_) {
      string name = osymbols_->Name() + "_from_gallic";
      fst_->SetInputSymbols(new SymbolTable(name));
      isymbols_ = fst_->MutableInputSymbols();
      isymbols_->AddSymbol(osymbols_->Find((int64) 0), 0);
    } else {
      fst_->SetInputSymbols(0);
    }
  }

  A operator()(const FromArc &arc) {
    // 'Super-non-final' arc.
    if (arc.nextstate == kNoStateId && arc.weight == GW::Zero())
      return A(arc.ilabel, 0, AW::Zero(), kNoStateId);

    SW w1 = arc.weight.Value1();
    AW w2 = arc.weight.Value2();
    Label l;

    if (w1.Size() == 0) {
      l = 0;
    } else {
      typename Map::iterator miter = map_.find(w1);
      if (miter != map_.end()) {
        l = (*miter).second;
      } else {
        l = ++lmax_;
        map_.insert(pair<const SW, Label>(w1, l));
        StringWeightIterator<Label, GALLIC_STRING_TYPE(G)> iter1(w1);
        StateId n;
        string s;
        for(size_t i = 0, p = state_;
            i < w1.Size();
            ++i, iter1.Next(), p = n) {
          n = i == w1.Size() - 1 ? state_ : fst_->AddState();
          fst_->AddArc(p, ToArc(i ? 0 : l, iter1.Value(), AW::One(), n));
          if (isymbols_) {
            if (i) s = s + "_";
            s = s + osymbols_->Find(iter1.Value());
          }
        }
        if (isymbols_)
          isymbols_->AddSymbol(s, l);
      }
    }

    if (l == kStringInfinity || l == kStringBad || arc.ilabel != arc.olabel) {
      FSTERROR() << "GallicToNewSymbolMapper: unrepresentable weight: " << l;
      error_ = true;
    }

    return A(arc.ilabel, l, w2, arc.nextstate);
  }

  MapFinalAction FinalAction() const { return MAP_ALLOW_SUPERFINAL; }

  MapSymbolsAction InputSymbolsAction() const { return MAP_COPY_SYMBOLS; }

  MapSymbolsAction OutputSymbolsAction() const { return MAP_CLEAR_SYMBOLS; }

  uint64 Properties(uint64 inprops) const {
    uint64 outprops = inprops & kOLabelInvariantProperties &
        kWeightInvariantProperties & kAddSuperFinalProperties;
    if (error_)
      outprops |= kError;
    return outprops;
  }

 private:
  class StringKey {
   public:
    size_t operator()(const SW &x) const {
      return x.Hash();
    }
  };

  typedef unordered_map<SW, Label, StringKey> Map;

  MutableFst<ToArc> *fst_;
  Map map_;
  Label lmax_;
  StateId state_;
  const SymbolTable *osymbols_;
  SymbolTable *isymbols_;
  mutable bool error_;

  DISALLOW_COPY_AND_ASSIGN(GallicToNewSymbolsMapper);
};


// Mapper to add a constant to all weights.
template <class A>
struct PlusMapper {
  typedef A FromArc;
  typedef A ToArc;
  typedef typename A::Weight Weight;

  explicit PlusMapper(Weight w) : weight_(w) {}

  A operator()(const A &arc) const {
    if (arc.weight == Weight::Zero())
      return arc;
    Weight w = Plus(arc.weight, weight_);
    return A(arc.ilabel, arc.olabel, w, arc.nextstate);
  }

  MapFinalAction FinalAction() const { return MAP_NO_SUPERFINAL; }

  MapSymbolsAction InputSymbolsAction() const { return MAP_COPY_SYMBOLS; }

  MapSymbolsAction OutputSymbolsAction() const { return MAP_COPY_SYMBOLS;}

  uint64 Properties(uint64 props) const {
    return props & kWeightInvariantProperties;
  }

 private:



  Weight weight_;
};


// Mapper to (right) multiply a constant to all weights.
template <class A>
struct TimesMapper {
  typedef A FromArc;
  typedef A ToArc;
  typedef typename A::Weight Weight;

  explicit TimesMapper(Weight w) : weight_(w) {}

  A operator()(const A &arc) const {
    if (arc.weight == Weight::Zero())
      return arc;
    Weight w = Times(arc.weight, weight_);
    return A(arc.ilabel, arc.olabel, w, arc.nextstate);
  }

  MapFinalAction FinalAction() const { return MAP_NO_SUPERFINAL; }

  MapSymbolsAction InputSymbolsAction() const { return MAP_COPY_SYMBOLS; }

  MapSymbolsAction OutputSymbolsAction() const { return MAP_COPY_SYMBOLS;}

  uint64 Properties(uint64 props) const {
    return props & kWeightInvariantProperties;
  }

 private:
  Weight weight_;
};


// Mapper to reciprocate all non-Zero() weights.
template <class A>
struct InvertWeightMapper {
  typedef A FromArc;
  typedef A ToArc;
  typedef typename A::Weight Weight;

  A operator()(const A &arc) const {
    if (arc.weight == Weight::Zero())
      return arc;
    Weight w = Divide(Weight::One(), arc.weight);
    return A(arc.ilabel, arc.olabel, w, arc.nextstate);
  }

  MapFinalAction FinalAction() const { return MAP_NO_SUPERFINAL; }

  MapSymbolsAction InputSymbolsAction() const { return MAP_COPY_SYMBOLS; }

  MapSymbolsAction OutputSymbolsAction() const { return MAP_COPY_SYMBOLS;}

  uint64 Properties(uint64 props) const {
    return props & kWeightInvariantProperties;
  }
};


// Mapper to map all non-Zero() weights to One().
template <class A, class B = A>
struct RmWeightMapper {
  typedef A FromArc;
  typedef B ToArc;
  typedef typename FromArc::Weight FromWeight;
  typedef typename ToArc::Weight ToWeight;

  B operator()(const A &arc) const {
    ToWeight w = arc.weight != FromWeight::Zero() ?
                   ToWeight::One() : ToWeight::Zero();
    return B(arc.ilabel, arc.olabel, w, arc.nextstate);
  }

  MapFinalAction FinalAction() const { return MAP_NO_SUPERFINAL; }

  MapSymbolsAction InputSymbolsAction() const { return MAP_COPY_SYMBOLS; }

  MapSymbolsAction OutputSymbolsAction() const { return MAP_COPY_SYMBOLS;}

  uint64 Properties(uint64 props) const {
    return (props & kWeightInvariantProperties) | kUnweighted;
  }
};


// Mapper to quantize all weights.
template <class A, class B = A>
struct QuantizeMapper {
  typedef A FromArc;
  typedef B ToArc;
  typedef typename FromArc::Weight FromWeight;
  typedef typename ToArc::Weight ToWeight;

  QuantizeMapper() : delta_(kDelta) {}

  explicit QuantizeMapper(float d) : delta_(d) {}

  B operator()(const A &arc) const {
    ToWeight w = arc.weight.Quantize(delta_);
    return B(arc.ilabel, arc.olabel, w, arc.nextstate);
  }

  MapFinalAction FinalAction() const { return MAP_NO_SUPERFINAL; }

  MapSymbolsAction InputSymbolsAction() const { return MAP_COPY_SYMBOLS; }

  MapSymbolsAction OutputSymbolsAction() const { return MAP_COPY_SYMBOLS;}

  uint64 Properties(uint64 props) const {
    return props & kWeightInvariantProperties;
  }

 private:
  float delta_;
};


// Mapper from A to B under the assumption:
//    B::Weight = A::Weight::ReverseWeight
//    B::Label == A::Label
//    B::StateId == A::StateId
// The weight is reversed, while the label and nextstate preserved
// in the mapping.
template <class A, class B>
struct ReverseWeightMapper {
  typedef A FromArc;
  typedef B ToArc;

  B operator()(const A &arc) const {
    return B(arc.ilabel, arc.olabel, arc.weight.Reverse(), arc.nextstate);
  }

  MapFinalAction FinalAction() const { return MAP_NO_SUPERFINAL; }

  MapSymbolsAction InputSymbolsAction() const { return MAP_COPY_SYMBOLS; }

  MapSymbolsAction OutputSymbolsAction() const { return MAP_COPY_SYMBOLS;}

  uint64 Properties(uint64 props) const { return props; }
};

}  // namespace fst

#endif  // FST_LIB_ARC_MAP_H__
