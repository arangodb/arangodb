// mutable-fst.h

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
// Expanded FST augmented with mutators - interface class definition
// and mutable arc iterator interface.
//

#ifndef FST_LIB_MUTABLE_FST_H__
#define FST_LIB_MUTABLE_FST_H__

#include <stddef.h>
#include <sys/types.h>
#include <string>
#include <vector>
using std::vector;

#include <fst/expanded-fst.h>


namespace fst {

template <class A> struct MutableArcIteratorData;

// An expanded FST plus mutators (use MutableArcIterator to modify arcs).
template <class A>
class MutableFst : public ExpandedFst<A> {
 public:
  typedef A Arc;
  typedef typename A::Weight Weight;
  typedef typename A::StateId StateId;

  virtual MutableFst<A> &operator=(const Fst<A> &fst) = 0;

  MutableFst<A> &operator=(const MutableFst<A> &fst) {
    return operator=(static_cast<const Fst<A> &>(fst));
  }

  virtual void SetStart(StateId) = 0;           // Set the initial state
  virtual void SetFinal(StateId, Weight) = 0;   // Set a state's final weight
  virtual void SetProperties(uint64 props,
                             uint64 mask) = 0;  // Set property bits wrt mask

  virtual StateId AddState() = 0;               // Add a state, return its ID
  virtual void AddArc(StateId, const A &arc) = 0;   // Add an arc to state

  virtual void DeleteStates(const vector<StateId>&) = 0;  // Delete some states
  virtual void DeleteStates() = 0;              // Delete all states
  virtual void DeleteArcs(StateId, size_t n) = 0;  // Delete some arcs at state
  virtual void DeleteArcs(StateId) = 0;         // Delete all arcs at state

  virtual void ReserveStates(StateId n) { }  // Optional, best effort only.
  virtual void ReserveArcs(StateId s, size_t n) { }  // Optional, Best effort.

  // Return input label symbol table; return NULL if not specified
  virtual const SymbolTable* InputSymbols() const = 0;
  // Return output label symbol table; return NULL if not specified
  virtual const SymbolTable* OutputSymbols() const = 0;

  // Return input label symbol table; return NULL if not specified
  virtual SymbolTable* MutableInputSymbols() = 0;
  // Return output label symbol table; return NULL if not specified
  virtual SymbolTable* MutableOutputSymbols() = 0;

  // Set input label symbol table; NULL signifies not unspecified
  virtual void SetInputSymbols(const SymbolTable* isyms) = 0;
  // Set output label symbol table; NULL signifies not unspecified
  virtual void SetOutputSymbols(const SymbolTable* osyms) = 0;

  // Get a copy of this MutableFst. See Fst<>::Copy() for further doc.
  virtual MutableFst<A> *Copy(bool safe = false) const = 0;

  // Read an MutableFst from an input stream; return NULL on error.
  static MutableFst<A> *Read(istream &strm, const FstReadOptions &opts) {
    FstReadOptions ropts(opts);
    FstHeader hdr;
    if (ropts.header)
      hdr = *opts.header;
    else {
      if (!hdr.Read(strm, opts.source))
        return 0;
      ropts.header = &hdr;
    }
    if (!(hdr.Properties() & kMutable)) {
      LOG(ERROR) << "MutableFst::Read: Not a MutableFst: " << ropts.source;
      return 0;
    }
    FstRegister<A> *registr = FstRegister<A>::GetRegister();
    const typename FstRegister<A>::Reader reader =
      registr->GetReader(hdr.FstType());
    if (!reader) {
      LOG(ERROR) << "MutableFst::Read: Unknown FST type \"" << hdr.FstType()
                 << "\" (arc type = \"" << A::Type()
                 << "\"): " << ropts.source;
      return 0;
    }
    Fst<A> *fst = reader(strm, ropts);
    if (!fst) return 0;
    return static_cast<MutableFst<A> *>(fst);
  }

  // Read a MutableFst from a file; return NULL on error.
  // Empty filename reads from standard input. If 'convert' is true,
  // convert to a mutable FST of type 'convert_type' if file is
  // a non-mutable FST.
  static MutableFst<A> *Read(const string &filename, bool convert = false,
                             const string &convert_type = "vector") {
    if (convert == false) {
      if (!filename.empty()) {
        ifstream strm(filename.c_str(),
                      std::ios_base::in | std::ios_base::binary);
        if (!strm) {
          LOG(ERROR) << "MutableFst::Read: Can't open file: " << filename;
          return 0;
        }
        return Read(strm, FstReadOptions(filename));
      } else {
        return Read(std::cin, FstReadOptions("standard input"));
      }
    } else {  // Converts to 'convert_type' if not mutable.
      Fst<A> *ifst = Fst<A>::Read(filename);
      if (!ifst) return 0;
      if (ifst->Properties(kMutable, false)) {
        return static_cast<MutableFst *>(ifst);
      } else {
        Fst<A> *ofst = Convert(*ifst, convert_type);
        delete ifst;
        if (!ofst) return 0;
        if (!ofst->Properties(kMutable, false))
          LOG(ERROR) << "MutableFst: bad convert type: " << convert_type;
        return static_cast<MutableFst *>(ofst);
      }
    }
  }

  // For generic mutuble arc iterator construction; not normally called
  // directly by users.
  virtual void InitMutableArcIterator(StateId s,
                                      MutableArcIteratorData<A> *) = 0;
};

// Mutable arc iterator interface, templated on the Arc definition; used
// for mutable Arc iterator specializations that are returned by
// the InitMutableArcIterator MutableFst method.
template <class A>
class MutableArcIteratorBase : public ArcIteratorBase<A> {
 public:
  typedef A Arc;

  void SetValue(const A &arc) { SetValue_(arc); }  // Set current arc's content

 private:
  virtual void SetValue_(const A &arc) = 0;
};

template <class A>
struct MutableArcIteratorData {
  MutableArcIteratorBase<A> *base;  // Specific iterator
};

// Generic mutable arc iterator, templated on the FST definition
// - a wrapper around pointer to specific one.
// Here is a typical use: \code
//   for (MutableArcIterator<StdFst> aiter(&fst, s);
//        !aiter.Done();
//         aiter.Next()) {
//     StdArc arc = aiter.Value();
//     arc.ilabel = 7;
//     aiter.SetValue(arc);
//     ...
//   } \endcode
// This version requires function calls.
template <class F>
class MutableArcIterator {
 public:
  typedef F FST;
  typedef typename F::Arc Arc;
  typedef typename Arc::StateId StateId;

  MutableArcIterator(F *fst, StateId s) {
    fst->InitMutableArcIterator(s, &data_);
  }
  ~MutableArcIterator() { delete data_.base; }

  bool Done() const { return data_.base->Done(); }
  const Arc& Value() const { return data_.base->Value(); }
  void Next() { data_.base->Next(); }
  size_t Position() const { return data_.base->Position(); }
  void Reset() { data_.base->Reset(); }
  void Seek(size_t a) { data_.base->Seek(a); }
  void SetValue(const Arc &a) { data_.base->SetValue(a); }
  uint32 Flags() const { return data_.base->Flags(); }
  void SetFlags(uint32 f, uint32 m) {
    return data_.base->SetFlags(f, m);
  }

 private:
  MutableArcIteratorData<Arc> data_;
  DISALLOW_COPY_AND_ASSIGN(MutableArcIterator);
};


namespace internal {

//  MutableFst<A> case - abstract methods.
template <class A> inline
typename A::Weight Final(const MutableFst<A> &fst, typename A::StateId s) {
  return fst.Final(s);
}

template <class A> inline
ssize_t NumArcs(const MutableFst<A> &fst, typename A::StateId s) {
  return fst.NumArcs(s);
}

template <class A> inline
ssize_t NumInputEpsilons(const MutableFst<A> &fst, typename A::StateId s) {
  return fst.NumInputEpsilons(s);
}

template <class A> inline
ssize_t NumOutputEpsilons(const MutableFst<A> &fst, typename A::StateId s) {
  return fst.NumOutputEpsilons(s);
}

}  // namespace internal


// A useful alias when using StdArc.
typedef MutableFst<StdArc> StdMutableFst;


// This is a helper class template useful for attaching a MutableFst
// interface to its implementation, handling reference counting and
// copy-on-write.
template <class I, class F = MutableFst<typename I::Arc> >
class ImplToMutableFst : public ImplToExpandedFst<I, F> {
 public:
  typedef typename I::Arc Arc;
  typedef typename Arc::Weight Weight;
  typedef typename Arc::StateId StateId;

  using ImplToFst<I, F>::GetImpl;
  using ImplToFst<I, F>::SetImpl;

  virtual void SetStart(StateId s) {
    MutateCheck();
    GetImpl()->SetStart(s);
  }

  virtual void SetFinal(StateId s, Weight w) {
    MutateCheck();
    GetImpl()->SetFinal(s, w);
  }

  virtual void SetProperties(uint64 props, uint64 mask) {
    // Can skip mutate check if extrinsic properties don't change,
    // since it is then safe to update all (shallow) copies
    uint64 exprops = kExtrinsicProperties & mask;
    if (GetImpl()->Properties(exprops) != (props & exprops))
      MutateCheck();
    GetImpl()->SetProperties(props, mask);
  }

  virtual StateId AddState() {
    MutateCheck();
    return GetImpl()->AddState();
  }

  virtual void AddArc(StateId s, const Arc &arc) {
    MutateCheck();
    GetImpl()->AddArc(s, arc);
  }

  virtual void DeleteStates(const vector<StateId> &dstates) {
    MutateCheck();
    GetImpl()->DeleteStates(dstates);
  }

  virtual void DeleteStates() {
    MutateCheck();
    GetImpl()->DeleteStates();
  }

  virtual void DeleteArcs(StateId s, size_t n) {
    MutateCheck();
    GetImpl()->DeleteArcs(s, n);
  }

  virtual void DeleteArcs(StateId s) {
    MutateCheck();
    GetImpl()->DeleteArcs(s);
  }

  virtual void ReserveStates(StateId s) {
    MutateCheck();
    GetImpl()->ReserveStates(s);
  }

  virtual void ReserveArcs(StateId s, size_t n) {
    MutateCheck();
    GetImpl()->ReserveArcs(s, n);
  }

  virtual const SymbolTable* InputSymbols() const {
    return GetImpl()->InputSymbols();
  }

  virtual const SymbolTable* OutputSymbols() const {
    return GetImpl()->OutputSymbols();
  }

  virtual SymbolTable* MutableInputSymbols() {
    MutateCheck();
    return GetImpl()->InputSymbols();
  }

  virtual SymbolTable* MutableOutputSymbols() {
    MutateCheck();
    return GetImpl()->OutputSymbols();
  }

  virtual void SetInputSymbols(const SymbolTable* isyms) {
    MutateCheck();
    GetImpl()->SetInputSymbols(isyms);
  }

  virtual void SetOutputSymbols(const SymbolTable* osyms) {
    MutateCheck();
    GetImpl()->SetOutputSymbols(osyms);
  }

 protected:
  ImplToMutableFst() : ImplToExpandedFst<I, F>() {}

  ImplToMutableFst(I *impl) : ImplToExpandedFst<I, F>(impl) {}


  ImplToMutableFst(const ImplToMutableFst<I, F> &fst)
      : ImplToExpandedFst<I, F>(fst) {}

  ImplToMutableFst(const ImplToMutableFst<I, F> &fst, bool safe)
      : ImplToExpandedFst<I, F>(fst, safe) {}

  void MutateCheck() {
    // Copy on write
    if (GetImpl()->RefCount() > 1)
      SetImpl(new I(*this));
  }

 private:
  // Disallow
  ImplToMutableFst<I, F>  &operator=(const ImplToMutableFst<I, F> &fst);

  ImplToMutableFst<I, F> &operator=(const Fst<Arc> &fst) {
    FSTERROR() << "ImplToMutableFst: Assignment operator disallowed";
    GetImpl()->SetProperties(kError, kError);
    return *this;
  }
};


}  // namespace fst

#endif  // FST_LIB_MUTABLE_FST_H__
