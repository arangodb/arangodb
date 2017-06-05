// matcher-fst.h

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
// Class to add a matcher to an FST.

#ifndef FST_LIB_MATCHER_FST_H_
#define FST_LIB_MATCHER_FST_H_

#include <fst/add-on.h>
#include <fst/const-fst.h>
#include <fst/lookahead-matcher.h>


namespace fst {

// WRITABLE MATCHERS - these have the interface of Matchers (see
// matcher.h) and these additional methods:
//
// template <class F>
// class Matcher {
//  public:
//   typedef ... MatcherData;  // Initialization data
//  ...
//   // Constructor with additional argument for external initialization
//   // data; matcher increments its reference count on construction and
//   // decrements the reference count, and if 0 deletes, on destruction.
//   Matcher(const F &fst, MatchType type, MatcherData *data);
//
//   // Returns pointer to initialization data that can be
//   // passed to a Matcher constructor.
//   MatcherData *GetData() const;
// };

// The matcher initialization data class must have the form:
// class MatcherData {
// public:
//   // Required copy constructor.
//   MatcherData(const MatcherData &);
//   //
//   // Required I/O methods.
//   static MatcherData *Read(istream &istrm);
//   bool Write(ostream &ostrm);
//
//   // Required reference counting.
//   int RefCount() const;
//   int IncrRefCount();
//   int DecrRefCount();
// };

// Default MatcherFst initializer - does nothing.
template <class M>
class NullMatcherFstInit {
 public:
  typedef AddOnPair<typename M::MatcherData, typename M::MatcherData> D;
  typedef AddOnImpl<typename M::FST, D> Impl;
  NullMatcherFstInit(Impl **) {}
};

// Class to add a matcher M to an Fst F. Creates a new Fst of type name N.
// Optional function object I can be used to initialize the Fst.
// Parameter A allows defining the kind of add-on to use.
template <class F, class M, const char* N,
          class I = NullMatcherFstInit<M>,
          class A = AddOnPair<typename M::MatcherData,
                              typename M::MatcherData> >
class MatcherFst
    : public ImplToExpandedFst<AddOnImpl<F, A> > {
 public:
  friend class StateIterator< MatcherFst<F, M, N, I, A> >;
  friend class ArcIterator< MatcherFst<F, M, N, I, A> >;

  typedef F FST;
  typedef M FstMatcher;
  typedef typename F::Arc Arc;
  typedef typename Arc::StateId StateId;
  typedef A D;
  typedef AddOnImpl<F, D> Impl;

  MatcherFst() : ImplToExpandedFst<Impl>(new Impl(F(), N)) {}

  explicit MatcherFst(const F &fst)
      : ImplToExpandedFst<Impl>(CreateImpl(fst, N)) {}

  explicit MatcherFst(const Fst<Arc> &fst)
      : ImplToExpandedFst<Impl>(CreateImpl(fst, N)) {}

  // See Fst<>::Copy() for doc.
  MatcherFst(const MatcherFst<F, M, N, I, A> &fst, bool safe = false)
      : ImplToExpandedFst<Impl>(fst, safe) {}

  // Get a copy of this MatcherFst. See Fst<>::Copy() for further doc.
  virtual MatcherFst<F, M, N, I, A> *Copy(bool safe = false) const {
    return new MatcherFst<F, M, N, I, A>(*this, safe);
  }

  // Read a MatcherFst from an input stream; return NULL on error
  static MatcherFst<F, M, N, I, A> *Read(istream &strm,
                                      const FstReadOptions &opts) {
    Impl *impl = Impl::Read(strm, opts);
    return impl ? new MatcherFst<F, M, N, I, A>(impl) : 0;
  }

  // Read a MatcherFst from a file; return NULL on error
  // Empty filename reads from standard input
  static MatcherFst<F, M, N, I, A> *Read(const string &filename) {
    Impl *impl = ImplToExpandedFst<Impl>::Read(filename);
    return impl ? new MatcherFst<F, M, N, I, A>(impl) : 0;
  }

  virtual bool Write(ostream &strm, const FstWriteOptions &opts) const {
    return GetImpl()->Write(strm, opts);
  }

  virtual bool Write(const string &filename) const {
    return Fst<Arc>::WriteFile(filename);
  }

  virtual void InitStateIterator(StateIteratorData<Arc> *data) const {
    return GetImpl()->InitStateIterator(data);
  }

  virtual void InitArcIterator(StateId s, ArcIteratorData<Arc> *data) const {
    return GetImpl()->InitArcIterator(s, data);
  }

  virtual M *InitMatcher(MatchType match_type) const {
    return new M(GetFst(), match_type, GetData(match_type));
  }

  // Allows access to MatcherFst components.
  Impl *GetImpl() const {
    return ImplToFst<Impl, ExpandedFst<Arc> >::GetImpl();
  }

  F& GetFst() const { return GetImpl()->GetFst(); }

  typename M::MatcherData *GetData(MatchType match_type) const {
    D *data = GetImpl()->GetAddOn();
    return match_type == MATCH_INPUT ? data->First() : data->Second();
  }

 private:
  static Impl *CreateImpl(const F &fst, const string &name) {
    M imatcher(fst, MATCH_INPUT);
    M omatcher(fst, MATCH_OUTPUT);
    D *data = new D(imatcher.GetData(), omatcher.GetData());
    Impl *impl = new Impl(fst, name);
    impl->SetAddOn(data);
    I init(&impl);
    data->DecrRefCount();
    return impl;
  }

  static Impl *CreateImpl(const Fst<Arc> &fst, const string &name) {
    F ffst(fst);
    return CreateImpl(ffst, name);
  }

  explicit MatcherFst(Impl *impl) : ImplToExpandedFst<Impl>(impl) {}

  // Makes visible to friends.
  void SetImpl(Impl *impl, bool own_impl = true) {
    ImplToFst< Impl, ExpandedFst<Arc> >::SetImpl(impl, own_impl);
  }

  void operator=(const MatcherFst<F, M, N, I, A> &fst);  // disallow
};


// Specialization fo MatcherFst.
template <class F, class M, const char* N, class I>
class StateIterator< MatcherFst<F, M, N, I> > : public StateIterator<F> {
 public:
  explicit StateIterator(const MatcherFst<F, M, N, I> &fst) :
      StateIterator<F>(fst.GetImpl()->GetFst()) {}
};


// Specialization for MatcherFst.
template <class F, class M, const char* N, class I>
class ArcIterator< MatcherFst<F, M, N, I> > : public ArcIterator<F> {
 public:
  ArcIterator(const MatcherFst<F, M, N, I> &fst, typename F::Arc::StateId s)
      : ArcIterator<F>(fst.GetImpl()->GetFst(), s) {}
};


// Specialization for MatcherFst
template <class F, class M, const char* N, class I>
class Matcher< MatcherFst<F, M, N, I> > {
 public:
  typedef MatcherFst<F, M, N, I> FST;
  typedef typename F::Arc Arc;
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Label Label;

  Matcher(const FST &fst, MatchType match_type) {
    matcher_ = fst.InitMatcher(match_type);
  }

  Matcher(const Matcher<FST> &matcher) {
    matcher_ = matcher.matcher_->Copy();
  }

  ~Matcher() { delete matcher_; }

  Matcher<FST> *Copy() const {
    return new Matcher<FST>(*this);
  }

  MatchType Type(bool test) const { return matcher_->Type(test); }
  void SetState(StateId s) { matcher_->SetState(s); }
  bool Find(Label label) { return matcher_->Find(label); }
  bool Done() const { return matcher_->Done(); }
  const Arc& Value() const { return matcher_->Value(); }
  void Next() { matcher_->Next(); }
  uint64 Properties(uint64 props) const { return matcher_->Properties(props); }
  uint32 Flags() const { return matcher_->Flags(); }

 private:
  M *matcher_;

  void operator=(const Matcher<Arc> &);  // disallow
};


// Specialization for MatcherFst
template <class F, class M, const char* N, class I>
class LookAheadMatcher< MatcherFst<F, M, N, I> > {
 public:
  typedef MatcherFst<F, M, N, I> FST;
  typedef typename F::Arc Arc;
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Label Label;
  typedef typename Arc::Weight Weight;

  LookAheadMatcher(const FST &fst, MatchType match_type) {
    matcher_ = fst.InitMatcher(match_type);
  }

  LookAheadMatcher(const LookAheadMatcher<FST> &matcher, bool safe = false) {
    matcher_ = matcher.matcher_->Copy(safe);
  }

  ~LookAheadMatcher() { delete matcher_; }

  // General matcher methods
  LookAheadMatcher<FST> *Copy(bool safe = false) const {
    return new LookAheadMatcher<FST>(*this, safe);
  }

  MatchType Type(bool test) const { return matcher_->Type(test); }
  void SetState(StateId s) { matcher_->SetState(s); }
  bool Find(Label label) { return matcher_->Find(label); }
  bool Done() const { return matcher_->Done(); }
  const Arc& Value() const { return matcher_->Value(); }
  void Next() { matcher_->Next(); }
  const FST &GetFst() const { return matcher_->GetFst(); }
  uint64 Properties(uint64 props) const { return matcher_->Properties(props); }
  uint32 Flags() const { return matcher_->Flags(); }

  // Look-ahead methods
  bool LookAheadLabel(Label label) const {
    return matcher_->LookAheadLabel(label);
  }

  bool LookAheadFst(const Fst<Arc> &fst, StateId s) {
    return matcher_->LookAheadFst(fst, s);
  }

  Weight LookAheadWeight() const { return matcher_->LookAheadWeight(); }

  bool LookAheadPrefix(Arc *arc) const {
    return matcher_->LookAheadPrefix(arc);
  }

  void InitLookAheadFst(const Fst<Arc>& fst, bool copy = false) {
    matcher_->InitLookAheadFst(fst, copy);
  }

 private:
  M *matcher_;

  void operator=(const LookAheadMatcher<FST> &);  // disallow
};

//
// Useful aliases when using StdArc and LogArc.
//

// Arc look-ahead matchers
extern const char arc_lookahead_fst_type[];

typedef MatcherFst<ConstFst<StdArc>,
                   ArcLookAheadMatcher<SortedMatcher<ConstFst<StdArc> > >,
                   arc_lookahead_fst_type> StdArcLookAheadFst;

typedef MatcherFst<ConstFst<LogArc>,
                   ArcLookAheadMatcher<SortedMatcher<ConstFst<LogArc> > >,
                   arc_lookahead_fst_type> LogArcLookAheadFst;


// Label look-ahead matchers
extern const char ilabel_lookahead_fst_type[];
extern const char olabel_lookahead_fst_type[];

static const uint32 ilabel_lookahead_flags = kInputLookAheadMatcher |
    kLookAheadWeight | kLookAheadPrefix |
    kLookAheadEpsilons | kLookAheadNonEpsilonPrefix;
static const uint32 olabel_lookahead_flags = kOutputLookAheadMatcher |
    kLookAheadWeight | kLookAheadPrefix |
    kLookAheadEpsilons | kLookAheadNonEpsilonPrefix;

typedef MatcherFst<ConstFst<StdArc>,
                   LabelLookAheadMatcher<SortedMatcher<ConstFst<StdArc> >,
                                         ilabel_lookahead_flags,
                                         FastLogAccumulator<StdArc> >,
                   ilabel_lookahead_fst_type,
                   LabelLookAheadRelabeler<StdArc> > StdILabelLookAheadFst;

typedef MatcherFst<ConstFst<LogArc>,
                   LabelLookAheadMatcher<SortedMatcher<ConstFst<LogArc> >,
                                         ilabel_lookahead_flags,
                                         FastLogAccumulator<LogArc> >,
                   ilabel_lookahead_fst_type,
                   LabelLookAheadRelabeler<LogArc> > LogILabelLookAheadFst;

typedef MatcherFst<ConstFst<StdArc>,
                   LabelLookAheadMatcher<SortedMatcher<ConstFst<StdArc> >,
                                         olabel_lookahead_flags,
                                         FastLogAccumulator<StdArc> >,
                   olabel_lookahead_fst_type,
                   LabelLookAheadRelabeler<StdArc> > StdOLabelLookAheadFst;

typedef MatcherFst<ConstFst<LogArc>,
                   LabelLookAheadMatcher<SortedMatcher<ConstFst<LogArc> >,
                                         olabel_lookahead_flags,
                                         FastLogAccumulator<LogArc> >,
                   olabel_lookahead_fst_type,
                   LabelLookAheadRelabeler<LogArc> > LogOLabelLookAheadFst;

}  // namespace fst

#endif  // FST_LIB_MATCHER_FST_H_
