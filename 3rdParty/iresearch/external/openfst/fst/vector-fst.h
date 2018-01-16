// vector-fst.h

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
// Simple concrete, mutable FST whose states and arcs are stored in STL
// vectors.

#ifndef FST_LIB_VECTOR_FST_H__
#define FST_LIB_VECTOR_FST_H__

#include <string>
#include <vector>
using std::vector;

#include <fst/mutable-fst.h>
#include <fst/fst-decl.h>  // For optional argument declarations
#include <fst/test-properties.h>


namespace fst {

template <class A, class S> class VectorFst;
template <class F, class G> void Cast(const F &, G *);

// Arcs (of type A) implemented by an STL vector per state. M specifies Arc
// allocator (default declared in fst-decl.h).
template <class A, class M /* = std::allocator<A> */>
class VectorState {
 public:
  typedef A Arc;
  typedef typename A::Weight Weight;
  typedef typename A::StateId StateId;
  typedef M ArcAllocator;
  typedef typename ArcAllocator::template
  rebind<VectorState<A, M> >::other StateAllocator;

  // Provide STL allocator for arcs
  explicit VectorState(const ArcAllocator &alloc)
      : final_(Weight::Zero()), niepsilons_(0), noepsilons_(0),
        arcs_(alloc) {}

  VectorState(const VectorState<A, M> &state, const ArcAllocator &alloc)
      : final_(state.Final()),
        niepsilons_(state.NumInputEpsilons()),
        noepsilons_(state.NumOutputEpsilons()),
        arcs_(state.arcs_.begin(), state.arcs_.end(), alloc) { }

  void Reset() {
    final_ = Weight::Zero();
    niepsilons_ = 0;
    noepsilons_ = 0;
    arcs_.clear();
  }

  Weight Final() const { return final_; }
  size_t NumInputEpsilons() const { return niepsilons_; }
  size_t NumOutputEpsilons() const { return noepsilons_; }
  size_t NumArcs() const { return arcs_.size(); }
  const A &GetArc(size_t n) const { return arcs_[n]; }

  const A *Arcs() const { return arcs_.size() > 0 ? &arcs_[0] : 0; }
  A *MutableArcs() { return arcs_.size() > 0 ? &arcs_[0] : 0; }

  void ReserveArcs(size_t n) { arcs_.reserve(n); }

  void SetFinal(Weight final) { final_ =  final; }
  void SetNumInputEpsilons(size_t n) { niepsilons_ = n; }
  void SetNumOutputEpsilons(size_t n) { noepsilons_ = n; }

  void AddArc(const A &arc) {
    if (arc.ilabel == 0)
      ++niepsilons_;
    if (arc.olabel == 0)
      ++noepsilons_;
    arcs_.push_back(arc);
  }

  void SetArc(const A &arc, size_t n) {
    if (arcs_[n].ilabel == 0)
      --niepsilons_;
    if (arcs_[n].olabel == 0)
      --noepsilons_;
    if (arc.ilabel == 0)
      ++niepsilons_;
    if (arc.olabel == 0)
      ++noepsilons_;
    arcs_[n] = arc;
  }

  void DeleteArcs() {
    niepsilons_ = 0;
    noepsilons_ = 0;
    arcs_.clear();
  }

  void DeleteArcs(size_t n) {
    for (size_t i = 0; i < n; ++i) {
      if (arcs_.back().ilabel == 0)
        --niepsilons_;
      if (arcs_.back().olabel == 0)
        --noepsilons_;
      arcs_.pop_back();
    }
  }

  // For state class allocation
  void *operator new(size_t size, StateAllocator *alloc) {
    return alloc->allocate(1);
  }

  // For state destruction and memory freeing
  static void Destroy(VectorState<A, M> *state, StateAllocator *alloc) {
    if (state) {
      state->~VectorState<A, M>();
      alloc->deallocate(state, 1);
    }
  }

 private:
  Weight final_;                  // Final weight
  size_t niepsilons_;             // # of input epsilons
  size_t noepsilons_;             // # of output epsilons
  vector<A, ArcAllocator> arcs_;  // Arcs represenation

  DISALLOW_COPY_AND_ASSIGN(VectorState);
};


// States are implemented by STL vectors, templated on the
// State definition. This does not manage the Fst properties.
template <class S>
class VectorFstBaseImpl : public FstImpl<typename S::Arc> {
 public:
  typedef S State;
  typedef typename State::Arc Arc;
  typedef typename Arc::Weight Weight;
  typedef typename Arc::StateId StateId;

  VectorFstBaseImpl() : start_(kNoStateId) {}

  ~VectorFstBaseImpl() {
    for (StateId s = 0; s < states_.size(); ++s)
      State::Destroy(states_[s], &state_alloc_);
  }

  StateId Start() const { return start_; }

  Weight Final(StateId s) const { return states_[s]->Final(); }

  StateId NumStates() const { return states_.size(); }

  size_t NumArcs(StateId s) const { return states_[s]->NumArcs(); }

  size_t NumInputEpsilons(StateId s) const {
    return GetState(s)->NumInputEpsilons();
  }

  size_t NumOutputEpsilons(StateId s) const {
    return GetState(s)->NumOutputEpsilons();
  }

  void SetStart(StateId s) { start_ = s; }

  void SetFinal(StateId s, Weight w) { states_[s]->SetFinal(w); }

  StateId AddState() {
    states_.push_back(new(&state_alloc_) State(arc_alloc_));
    return states_.size() - 1;
  }

  StateId AddState(State *state) {
    states_.push_back(state);
    return states_.size() - 1;
  }

  void AddArc(StateId s, const Arc &arc) {
    states_[s]->AddArc(arc);
  }

  void DeleteStates(const vector<StateId>& dstates) {
    vector<StateId> newid(states_.size(), 0);
    for (size_t i = 0; i < dstates.size(); ++i)
      newid[dstates[i]] = kNoStateId;
    StateId nstates = 0;
    for (StateId s = 0; s < states_.size(); ++s) {
      if (newid[s] != kNoStateId) {
        newid[s] = nstates;
        if (s != nstates)
          states_[nstates] = states_[s];
        ++nstates;
      } else {
        State::Destroy(states_[s], &state_alloc_);
      }
    }
    states_.resize(nstates);
    for (StateId s = 0; s < states_.size(); ++s) {
      Arc *arcs = states_[s]->MutableArcs();
      size_t narcs = 0;
      size_t nieps = states_[s]->NumInputEpsilons();
      size_t noeps = states_[s]->NumOutputEpsilons();
      for (size_t i = 0; i < states_[s]->NumArcs(); ++i) {
        StateId t = newid[arcs[i].nextstate];
        if (t != kNoStateId) {
          arcs[i].nextstate = t;
          if (i != narcs)
            arcs[narcs] = arcs[i];
          ++narcs;
        } else {
          if (arcs[i].ilabel == 0)
            --nieps;
          if (arcs[i].olabel == 0)
            --noeps;
        }
      }
      states_[s]->DeleteArcs(states_[s]->NumArcs() - narcs);
      states_[s]->SetNumInputEpsilons(nieps);
      states_[s]->SetNumOutputEpsilons(noeps);
    }
    if (Start() != kNoStateId)
      SetStart(newid[Start()]);
  }

  void DeleteStates() {
    for (StateId s = 0; s < states_.size(); ++s)
      State::Destroy(states_[s], &state_alloc_);
    states_.clear();
    SetStart(kNoStateId);
  }

  void DeleteArcs(StateId s, size_t n) {
    states_[s]->DeleteArcs(n);
  }

  void DeleteArcs(StateId s) { states_[s]->DeleteArcs(); }

  State *GetState(StateId s) { return states_[s]; }

  const State *GetState(StateId s) const { return states_[s]; }

  void SetState(StateId s, State *state) { states_[s] = state; }

  void ReserveStates(StateId n) { states_.reserve(n); }

  void ReserveArcs(StateId s, size_t n) { states_[s]->ReserveArcs(n); }

  // Provide information needed for generic state iterator
  void InitStateIterator(StateIteratorData<Arc> *data) const {
    data->base = 0;
    data->nstates = states_.size();
  }

  // Provide information needed for generic arc iterator
  void InitArcIterator(StateId s, ArcIteratorData<Arc> *data) const {
    data->base = 0;
    data->narcs = states_[s]->NumArcs();
    data->arcs = states_[s]->Arcs();
    data->ref_count = 0;
  }

 private:
  vector<State *> states_;      // States represenation.
  StateId start_;               // initial state
  typename State::StateAllocator state_alloc_;  // for state allocation
  typename State::ArcAllocator arc_alloc_;      // for arc allocation

  DISALLOW_COPY_AND_ASSIGN(VectorFstBaseImpl);
};

// This is a VectorFstBaseImpl container that holds VectorState's.  It
// manages Fst properties.
template <class S>
class VectorFstImpl : public VectorFstBaseImpl<S> {
 public:
  typedef S State;
  typedef typename S::Arc A;
  using FstImpl<A>::SetInputSymbols;
  using FstImpl<A>::SetOutputSymbols;
  using FstImpl<A>::SetType;
  using FstImpl<A>::SetProperties;
  using FstImpl<A>::Properties;

  using VectorFstBaseImpl<S>::Start;
  using VectorFstBaseImpl<S>::NumStates;
  using VectorFstBaseImpl<S>::GetState;
  using VectorFstBaseImpl<S>::ReserveArcs;

  friend class MutableArcIterator< VectorFst<A, S> >;

  typedef VectorFstBaseImpl<S> BaseImpl;
  typedef typename A::Weight Weight;
  typedef typename A::StateId StateId;

  VectorFstImpl() {
    SetType("vector");
    SetProperties(kNullProperties | kStaticProperties);
  }
  explicit VectorFstImpl(const Fst<A> &fst);

  static VectorFstImpl<S> *Read(istream &strm, const FstReadOptions &opts);

  void SetStart(StateId s) {
    BaseImpl::SetStart(s);
    SetProperties(SetStartProperties(Properties()));
  }

  void SetFinal(StateId s, Weight w) {
    Weight ow = BaseImpl::Final(s);
    BaseImpl::SetFinal(s, w);
    SetProperties(SetFinalProperties(Properties(), ow, w));
  }

  StateId AddState() {
    StateId s = BaseImpl::AddState();
    SetProperties(AddStateProperties(Properties()));
    return s;
  }

  void AddArc(StateId s, const A &arc) {
    State *state = GetState(s);
    const A *parc = state->NumArcs() == 0 ? 0 :
        &(state->GetArc(state->NumArcs() - 1));
    SetProperties(AddArcProperties(Properties(), s, arc, parc));
    BaseImpl::AddArc(s, arc);
  }

  void DeleteStates(const vector<StateId> &dstates) {
    BaseImpl::DeleteStates(dstates);
    SetProperties(DeleteStatesProperties(Properties()));
  }

  void DeleteStates() {
    BaseImpl::DeleteStates();
    SetProperties(DeleteAllStatesProperties(Properties(), kStaticProperties));
  }

  void DeleteArcs(StateId s, size_t n) {
    BaseImpl::DeleteArcs(s, n);
    SetProperties(DeleteArcsProperties(Properties()));
  }

  void DeleteArcs(StateId s) {
    BaseImpl::DeleteArcs(s);
    SetProperties(DeleteArcsProperties(Properties()));
  }

  // Properties always true of this Fst class
  static const uint64 kStaticProperties = kExpanded | kMutable;

 private:
  // Current file format version
  static const int kFileVersion = 2;
  // Minimum file format version supported
  static const int kMinFileVersion = 1;

  DISALLOW_COPY_AND_ASSIGN(VectorFstImpl);
};

template <class S> const uint64 VectorFstImpl<S>::kStaticProperties;
template <class S> const int VectorFstImpl<S>::kFileVersion;
template <class S> const int VectorFstImpl<S>::kMinFileVersion;

template <class S>
VectorFstImpl<S>::VectorFstImpl(const Fst<A> &fst) {
  SetType("vector");
  SetInputSymbols(fst.InputSymbols());
  SetOutputSymbols(fst.OutputSymbols());
  BaseImpl::SetStart(fst.Start());
  if (fst.Properties(kExpanded, false))
    BaseImpl::ReserveStates(CountStates(fst));

  for (StateIterator< Fst<A> > siter(fst);
       !siter.Done();
       siter.Next()) {
    StateId s = siter.Value();
    BaseImpl::AddState();
    BaseImpl::SetFinal(s, fst.Final(s));
    ReserveArcs(s, fst.NumArcs(s));
    for (ArcIterator< Fst<A> > aiter(fst, s);
         !aiter.Done();
         aiter.Next()) {
      const A &arc = aiter.Value();
      BaseImpl::AddArc(s, arc);
    }
  }
  SetProperties(fst.Properties(kCopyProperties, false) | kStaticProperties);
}

template <class S>
VectorFstImpl<S> *VectorFstImpl<S>::Read(istream &strm,
                                                 const FstReadOptions &opts) {
  VectorFstImpl<S> *impl = new VectorFstImpl;
  FstHeader hdr;
  if (!impl->ReadHeader(strm, opts, kMinFileVersion, &hdr)) {
    delete impl;
    return 0;
  }
  impl->BaseImpl::SetStart(hdr.Start());
  if (hdr.NumStates() != kNoStateId) {
    impl->ReserveStates(hdr.NumStates());
  }

  StateId s = 0;
  for (;hdr.NumStates() == kNoStateId || s < hdr.NumStates(); ++s) {
    typename A::Weight final;
    if (!final.Read(strm)) break;
    impl->BaseImpl::AddState();
    State *state = impl->GetState(s);
    state->SetFinal(final);
    int64 narcs;
    ReadType(strm, &narcs);
    if (!strm) {
      LOG(ERROR) << "VectorFst::Read: read failed: " << opts.source;
      delete impl;
      return 0;
    }
    impl->ReserveArcs(s, narcs);
    for (size_t j = 0; j < narcs; ++j) {
      A arc;
      ReadType(strm, &arc.ilabel);
      ReadType(strm, &arc.olabel);
      arc.weight.Read(strm);
      ReadType(strm, &arc.nextstate);
      if (!strm) {
        LOG(ERROR) << "VectorFst::Read: read failed: " << opts.source;
        delete impl;
        return 0;
      }
      impl->BaseImpl::AddArc(s, arc);
    }
  }
  if (hdr.NumStates() != kNoStateId && s != hdr.NumStates()) {
    LOG(ERROR) << "VectorFst::Read: unexpected end of file: " << opts.source;
    delete impl;
    return 0;
  }
  return impl;
}

// Converts a string into a weight.
template <class W> class WeightFromString {
 public:
  W operator()(const string &s);
};

// Generic case fails.
template <class W> inline
W WeightFromString<W>::operator()(const string &s) {
  FSTERROR() << "VectorFst::Read: Obsolete file format";
  return W::NoWeight();
}

// TropicalWeight version.
template <> inline
TropicalWeight WeightFromString<TropicalWeight>::operator()(const string &s) {
  float f;
  memcpy(&f, s.data(), sizeof(f));
  return TropicalWeight(f);
}

// LogWeight version.
template <> inline
LogWeight WeightFromString<LogWeight>::operator()(const string &s) {
  float f;
  memcpy(&f, s.data(), sizeof(f));
  return LogWeight(f);
}

// Simple concrete, mutable FST. This class attaches interface to
// implementation and handles reference counting, delegating most
// methods to ImplToMutableFst. Supports additional operations:
// ReserveStates and ReserveArcs (cf. STL vectors). The second
// optional template argument gives the State definition (default
// declared in fst-decl.h).
template <class A, class S /* = VectorState<A> */>
class VectorFst : public ImplToMutableFst<VectorFstImpl<S> > {
 public:
  friend class StateIterator< VectorFst<A, S> >;
  friend class ArcIterator< VectorFst<A, S> >;
  friend class MutableArcIterator< VectorFst<A, S> >;
  template <class F, class G> friend void Cast(const F &, G *);

  typedef A Arc;
  typedef typename A::StateId StateId;
  typedef S State;
  typedef VectorFstImpl<State> Impl;

  VectorFst() : ImplToMutableFst<Impl>(new Impl) {}

  explicit VectorFst(const Fst<A> &fst)
      : ImplToMutableFst<Impl>(new Impl(fst)) {}

  VectorFst(const VectorFst<A, S> &fst) : ImplToMutableFst<Impl>(fst) {}

  // Get a copy of this VectorFst. See Fst<>::Copy() for further doc.
  virtual VectorFst<A, S> *Copy(bool safe = false) const {
    return new VectorFst<A, S>(*this);
  }

  VectorFst<A, S> &operator=(const VectorFst<A, S> &fst) {
    SetImpl(fst.GetImpl(), false);
    return *this;
  }

  virtual VectorFst<A, S> &operator=(const Fst<A> &fst) {
    if (this != &fst) SetImpl(new Impl(fst));
    return *this;
  }

  // Read a VectorFst from an input stream; return NULL on error
  static VectorFst<A, S> *Read(istream &strm, const FstReadOptions &opts) {
    Impl* impl = Impl::Read(strm, opts);
    return impl ? new VectorFst<A, S>(impl) : 0;
  }

  // Read a VectorFst from a file; return NULL on error
  // Empty filename reads from standard input
  static VectorFst<A, S> *Read(const string &filename) {
    Impl* impl = ImplToExpandedFst<Impl, MutableFst<A> >::Read(filename);
    return impl ? new VectorFst<A, S>(impl) : 0;
  }

  virtual bool Write(ostream &strm, const FstWriteOptions &opts) const {
    return WriteFst(*this, strm, opts);
  }

  virtual bool Write(const string &filename) const {
    return Fst<A>::WriteFile(filename);
  }

  template <class F>
  static bool WriteFst(const F &fst, ostream &strm,
                       const FstWriteOptions &opts);

  void ReserveStates(StateId n) {
    MutateCheck();
    GetImpl()->ReserveStates(n);
  }

  void ReserveArcs(StateId s, size_t n) {
    MutateCheck();
    GetImpl()->ReserveArcs(s, n);
  }

  virtual void InitStateIterator(StateIteratorData<Arc> *data) const {
    GetImpl()->InitStateIterator(data);
  }

  virtual void InitArcIterator(StateId s, ArcIteratorData<Arc> *data) const {
    GetImpl()->InitArcIterator(s, data);
  }

  virtual inline
  void InitMutableArcIterator(StateId s, MutableArcIteratorData<A> *);

 private:
  explicit VectorFst(Impl *impl) : ImplToMutableFst<Impl>(impl) {}

  // Makes visible to friends.
  Impl *GetImpl() const { return ImplToFst< Impl, MutableFst<A> >::GetImpl(); }

  void SetImpl(Impl *impl, bool own_impl = true) {
    ImplToFst< Impl, MutableFst<A> >::SetImpl(impl, own_impl);
  }

  void MutateCheck() { return ImplToMutableFst<Impl>::MutateCheck(); }
};

// Specialization for VectorFst; see generic version in fst.h
// for sample usage (but use the VectorFst type!). This version
// should inline.
template <class A, class S>
class StateIterator< VectorFst<A, S> > {
 public:
  typedef typename A::StateId StateId;

  explicit StateIterator(const VectorFst<A, S> &fst)
      : nstates_(fst.GetImpl()->NumStates()), s_(0) {}

  bool Done() const { return s_ >= nstates_; }

  StateId Value() const { return s_; }

  void Next() { ++s_; }

  void Reset() { s_ = 0; }

 private:
  StateId nstates_;
  StateId s_;

  DISALLOW_COPY_AND_ASSIGN(StateIterator);
};

// Writes Fst to file, will call CountStates so may involve two passes if
// called from an Fst that is not derived from Expanded.
template <class A, class S>
template <class F>
bool VectorFst<A, S>::WriteFst(const F &fst, ostream &strm,
                            const FstWriteOptions &opts) {
  static const int kFileVersion = 2;
  bool update_header = true;
  FstHeader hdr;
  hdr.SetStart(fst.Start());
  hdr.SetNumStates(kNoStateId);
  size_t start_offset = 0;
  if (fst.Properties(kExpanded, false) || (start_offset = strm.tellp()) != -1) {
    hdr.SetNumStates(CountStates(fst));
    update_header = false;
  }
  uint64 properties = fst.Properties(kCopyProperties, false) |
      Impl::kStaticProperties;
  FstImpl<A>::WriteFstHeader(fst, strm, opts, kFileVersion, "vector",
                             properties, &hdr);
  StateId num_states = 0;
  for (StateIterator<F> siter(fst); !siter.Done(); siter.Next()) {
    typename A::StateId s = siter.Value();
    fst.Final(s).Write(strm);
    int64 narcs = fst.NumArcs(s);
    WriteType(strm, narcs);
    for (ArcIterator<F> aiter(fst, s); !aiter.Done(); aiter.Next()) {
      const A &arc = aiter.Value();
      WriteType(strm, arc.ilabel);
      WriteType(strm, arc.olabel);
      arc.weight.Write(strm);
      WriteType(strm, arc.nextstate);
    }
    num_states++;
  }
  strm.flush();
  if (!strm) {
    LOG(ERROR) << "VectorFst::Write: write failed: " << opts.source;
    return false;
  }
  if (update_header) {
    hdr.SetNumStates(num_states);
    return FstImpl<A>::UpdateFstHeader(fst, strm, opts, kFileVersion, "vector",
                                       properties, &hdr, start_offset);
  } else {
    if (num_states != hdr.NumStates()) {
      LOG(ERROR) << "Inconsistent number of states observed during write";
      return false;
    }
  }
  return true;
}

// Specialization for VectorFst; see generic version in fst.h
// for sample usage (but use the VectorFst type!). This version
// should inline.
template <class A, class S>
class ArcIterator< VectorFst<A, S> > {
 public:
  typedef typename A::StateId StateId;

  ArcIterator(const VectorFst<A, S> &fst, StateId s)
      : arcs_(fst.GetImpl()->GetState(s)->Arcs()),
        narcs_(fst.GetImpl()->GetState(s)->NumArcs()),
        i_(0) {}

  bool Done() const { return i_ >= narcs_; }

  const A& Value() const { return arcs_[i_]; }

  void Next() { ++i_; }

  void Reset() { i_ = 0; }

  void Seek(size_t a) { i_ = a; }

  size_t Position() const { return i_; }

  uint32 Flags() const {
    return kArcValueFlags;
  }

  void SetFlags(uint32 f, uint32 m) {}

 private:
  const A *arcs_;
  size_t narcs_;
  size_t i_;

  DISALLOW_COPY_AND_ASSIGN(ArcIterator);
};

// Specialization for VectorFst; see generic version in fst.h
// for sample usage (but use the VectorFst type!). This version
// should inline.
template <class A, class S>
class MutableArcIterator< VectorFst<A, S> >
  : public MutableArcIteratorBase<A> {
 public:
  typedef S State;
  typedef typename A::StateId StateId;
  typedef typename A::Weight Weight;

  MutableArcIterator(VectorFst<A, S> *fst, StateId s) : i_(0) {
    fst->MutateCheck();
    state_ = fst->GetImpl()->GetState(s);
    properties_ = &fst->GetImpl()->properties_;
  }

  bool Done() const { return i_ >= state_->NumArcs(); }

  const A& Value() const { return state_->GetArc(i_); }

  void Next() { ++i_; }

  size_t Position() const { return i_; }

  void Reset() { i_ = 0; }

  void Seek(size_t a) { i_ = a; }

  void SetValue(const A &arc) {
    const A& oarc = state_->GetArc(i_);
    if (oarc.ilabel != oarc.olabel)
      *properties_ &= ~kNotAcceptor;
    if (oarc.ilabel == 0) {
      *properties_ &= ~kIEpsilons;
      if (oarc.olabel == 0)
        *properties_ &= ~kEpsilons;
    }
    if (oarc.olabel == 0) {
      *properties_ &= ~kOEpsilons;
    }
    if (oarc.weight != Weight::Zero() && oarc.weight != Weight::One())
      *properties_ &= ~kWeighted;

    state_->SetArc(arc, i_);

    if (arc.ilabel != arc.olabel) {
      *properties_ |= kNotAcceptor;
      *properties_ &= ~kAcceptor;
    }
    if (arc.ilabel == 0) {
      *properties_ |= kIEpsilons;
      *properties_ &= ~kNoIEpsilons;
      if (arc.olabel == 0) {
        *properties_ |= kEpsilons;
        *properties_ &= ~kNoEpsilons;
      }
    }
    if (arc.olabel == 0) {
      *properties_ |= kOEpsilons;
      *properties_ &= ~kNoOEpsilons;
    }
    if (arc.weight != Weight::Zero() && arc.weight != Weight::One()) {
      *properties_ |= kWeighted;
      *properties_ &= ~kUnweighted;
    }
    *properties_ &= kSetArcProperties | kAcceptor | kNotAcceptor |
        kEpsilons | kNoEpsilons | kIEpsilons | kNoIEpsilons |
        kOEpsilons | kNoOEpsilons | kWeighted | kUnweighted;
  }

  uint32 Flags() const {
    return kArcValueFlags;
  }

  void SetFlags(uint32 f, uint32 m) {}

 private:
  // This allows base-class virtual access to non-virtual derived-
  // class members of the same name. It makes the derived class more
  // efficient to use but unsafe to further derive.
  virtual bool Done_() const { return Done(); }
  virtual const A& Value_() const { return Value(); }
  virtual void Next_() { Next(); }
  virtual size_t Position_() const { return Position(); }
  virtual void Reset_() { Reset(); }
  virtual void Seek_(size_t a) { Seek(a); }
  virtual void SetValue_(const A &a) { SetValue(a); }
  uint32 Flags_() const { return Flags(); }
  void SetFlags_(uint32 f, uint32 m) { SetFlags(f, m); }

  State *state_;
  uint64 *properties_;
  size_t i_;

  DISALLOW_COPY_AND_ASSIGN(MutableArcIterator);
};

// Provide information needed for the generic mutable arc iterator
template <class A, class S> inline
void VectorFst<A, S>::InitMutableArcIterator(
    StateId s, MutableArcIteratorData<A> *data) {
  data->base = new MutableArcIterator< VectorFst<A, S> >(this, s);
}

// A useful alias when using StdArc.
typedef VectorFst<StdArc> StdVectorFst;

}  // namespace fst

#endif  // FST_LIB_VECTOR_FST_H__
