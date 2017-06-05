// add-on.h

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
// Fst implementation class to attach an arbitrary object with a
// read/write method to an FST and its file rep. The FST is given a
// new type name.

#ifndef FST_LIB_ADD_ON_H_
#define FST_LIB_ADD_ON_H_

#include <stddef.h>
#include <string>

#include <fst/fst.h>


namespace fst {

// Identifies stream data as an add-on fst.
static const int32 kAddOnMagicNumber = 446681434;


//
// Some useful add-on objects.
//

// Nothing to save.
class NullAddOn {
 public:
  NullAddOn() {}

  static NullAddOn *Read(istream &istrm) {
    return new NullAddOn();
  }

  bool Write(ostream &ostrm) const { return true; }

  int RefCount() const { return ref_count_.count(); }
  int IncrRefCount() { return ref_count_.Incr(); }
  int DecrRefCount() { return ref_count_.Decr(); }

 private:
  RefCounter ref_count_;

  DISALLOW_COPY_AND_ASSIGN(NullAddOn);
};


// Create a new add-on from a pair of add-ons.
template <class A1, class A2>
class AddOnPair {
 public:
  // Argument reference count incremented.
  AddOnPair(A1 *a1, A2 *a2)
      : a1_(a1), a2_(a2) {
    if (a1_)
      a1_->IncrRefCount();
    if (a2_)
      a2_->IncrRefCount();
  }

  ~AddOnPair() {
    if (a1_ && !a1_->DecrRefCount())
      delete a1_;
    if (a2_ && !a2_->DecrRefCount())
      delete a2_;
  }

  A1 *First() const { return a1_; }
  A2 *Second() const { return a2_; }

  static AddOnPair<A1, A2> *Read(istream &istrm) {
    A1 *a1 = 0;
    bool have_addon1 = false;
    ReadType(istrm, &have_addon1);
    if (have_addon1)
      a1 = A1::Read(istrm);

    A2 *a2 = 0;
    bool have_addon2 = false;
    ReadType(istrm, &have_addon2);
    if (have_addon2)
      a2 = A2::Read(istrm);

    AddOnPair<A1, A2> *a = new AddOnPair<A1, A2>(a1, a2);
    if (a1)
      a1->DecrRefCount();
    if (a2)
      a2->DecrRefCount();
    return a;
  }

  bool Write(ostream &ostrm) const {
    bool have_addon1 = a1_;
    WriteType(ostrm, have_addon1);
    if (have_addon1)
      a1_->Write(ostrm);
    bool have_addon2 = a2_;
    WriteType(ostrm, have_addon2);
    if (have_addon2)
      a2_->Write(ostrm);
    return true;
  }

  int RefCount() const { return ref_count_.count(); }

  int IncrRefCount() {
    return ref_count_.Incr();
  }

  int DecrRefCount() {
    return ref_count_.Decr();
  }

 private:
  A1 *a1_;
  A2 *a2_;
  RefCounter ref_count_;

  DISALLOW_COPY_AND_ASSIGN(AddOnPair);
};


// Add to an Fst F a type T object. T must have a 'T* Read(istream &)',
// a 'bool Write(ostream &)' method, and 'int RecCount(), 'int IncrRefCount()'
// and 'int DecrRefCount()' methods (e.g. 'MatcherData' in matcher-fst.h).
// The result is a new Fst implemenation with type name 'type'.
template<class F, class T>
class AddOnImpl : public FstImpl<typename F::Arc> {
 public:
  typedef typename F::Arc Arc;
  typedef typename Arc::Label Label;
  typedef typename Arc::Weight Weight;
  typedef typename Arc::StateId StateId;

  using FstImpl<Arc>::SetType;
  using FstImpl<Arc>::SetInputSymbols;
  using FstImpl<Arc>::SetOutputSymbols;
  using FstImpl<Arc>::SetProperties;
  using FstImpl<Arc>::WriteHeader;

  // If 't' is non-zero, its reference count is incremented.
  AddOnImpl(const F &fst, const string &type, T *t = 0)
      : fst_(fst), t_(t) {
    SetType(type);
    SetProperties(fst_.Properties(kFstProperties, false));
    SetInputSymbols(fst_.InputSymbols());
    SetOutputSymbols(fst_.OutputSymbols());
    if (t_)
      t_->IncrRefCount();
  }

  // If 't' is non-zero, its reference count is incremented.
  AddOnImpl(const Fst<Arc> &fst, const string &type, T *t = 0)
      : fst_(fst), t_(t) {
    SetType(type);
    SetProperties(fst_.Properties(kFstProperties, false));
    SetInputSymbols(fst_.InputSymbols());
    SetOutputSymbols(fst_.OutputSymbols());
    if (t_)
      t_->IncrRefCount();
  }

  AddOnImpl(const AddOnImpl<F, T> &impl)
      : fst_(impl.fst_), t_(impl.t_) {
    SetType(impl.Type());
    SetProperties(fst_.Properties(kCopyProperties, false));
    SetInputSymbols(fst_.InputSymbols());
    SetOutputSymbols(fst_.OutputSymbols());
    if (t_)
      t_->IncrRefCount();
  }

  ~AddOnImpl() {
    if (t_ && !t_->DecrRefCount())
      delete t_;
  }

  StateId Start() const { return fst_.Start(); }
  Weight Final(StateId s) const { return fst_.Final(s); }
  size_t NumArcs(StateId s) const { return fst_.NumArcs(s); }

  size_t NumInputEpsilons(StateId s) const {
    return fst_.NumInputEpsilons(s);
  }

  size_t NumOutputEpsilons(StateId s) const {
    return fst_.NumOutputEpsilons(s);
  }

  size_t NumStates() const { return fst_.NumStates(); }

  static AddOnImpl<F, T> *Read(istream &strm, const FstReadOptions &opts) {
    FstReadOptions nopts(opts);
    FstHeader hdr;
    if (!nopts.header) {
      hdr.Read(strm, nopts.source);
      nopts.header = &hdr;
    }
    AddOnImpl<F, T> *impl = new AddOnImpl<F, T>(nopts.header->FstType());
    if (!impl->ReadHeader(strm, nopts, kMinFileVersion, &hdr))
      return 0;
    delete impl;       // Used here only for checking types.

    int32 magic_number = 0;
    ReadType(strm, &magic_number);   // Ensures this is an add-on Fst.
    if (magic_number != kAddOnMagicNumber) {
      LOG(ERROR) << "AddOnImpl::Read: Bad add-on header: " << nopts.source;
      return 0;
    }

    FstReadOptions fopts(opts);
    fopts.header = 0;  // Contained header was written out.
    F *fst = F::Read(strm, fopts);
    if (!fst)
      return 0;

    T *t = 0;
    bool have_addon = false;
    ReadType(strm, &have_addon);
    if (have_addon) {   // Read add-on object if present.
      t = T::Read(strm);
      if (!t)
        return 0;
    }
    impl = new AddOnImpl<F, T>(*fst, nopts.header->FstType(), t);
    delete fst;
    if (t)
      t->DecrRefCount();
    return impl;
  }

  bool Write(ostream &strm, const FstWriteOptions &opts) const {
    FstHeader hdr;
    FstWriteOptions nopts(opts);
    nopts.write_isymbols = false;  // Let contained FST hold any symbols.
    nopts.write_osymbols = false;
    WriteHeader(strm, nopts, kFileVersion, &hdr);
    WriteType(strm, kAddOnMagicNumber);  // Ensures this is an add-on Fst.
    FstWriteOptions fopts(opts);
    fopts.write_header = true;     // Force writing contained header.
    if (!fst_.Write(strm, fopts))
      return false;
    bool have_addon = t_;
    WriteType(strm, have_addon);
    if (have_addon)                // Write add-on object if present.
      t_->Write(strm);
    return true;
  }

  void InitStateIterator(StateIteratorData<Arc> *data) const {
    fst_.InitStateIterator(data);
  }

  void InitArcIterator(StateId s, ArcIteratorData<Arc> *data) const {
    fst_.InitArcIterator(s, data);
  }

  F &GetFst() { return fst_; }

  const F &GetFst() const { return fst_; }

  T *GetAddOn() const { return t_; }

  // If 't' is non-zero, its reference count is incremented.
  void SetAddOn(T *t) {
    if (t == t_)
      return;
    if (t_ && !t_->DecrRefCount())
      delete t_;
    t_ = t;
    if (t_)
      t_->IncrRefCount();
  }

 private:
  explicit AddOnImpl(const string &type) : t_(0) {
    SetType(type);
    SetProperties(kExpanded);
  }

  // Current file format version
  static const int kFileVersion = 1;
  // Minimum file format version supported
  static const int kMinFileVersion = 1;

  F fst_;
  T *t_;

  void operator=(const AddOnImpl<F, T> &fst);  // Disallow
};

template <class F, class T> const int AddOnImpl<F, T>::kFileVersion;
template <class F, class T> const int AddOnImpl<F, T>::kMinFileVersion;


}  // namespace fst

#endif  // FST_LIB_ADD_ON_H_
