// synchronize.h

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
// Author: allauzen@google.com (Cyril Allauzen)
//
// \file
// Synchronize an FST with bounded delay.

#ifndef FST_LIB_SYNCHRONIZE_H__
#define FST_LIB_SYNCHRONIZE_H__

#include <algorithm>
#include <unordered_map>
using std::unordered_map;
using std::unordered_multimap;
#include <unordered_set>
using std::unordered_set;
using std::unordered_multiset;
#include <string>
#include <utility>
using std::pair; using std::make_pair;
#include <vector>
using std::vector;

#include <fst/cache.h>
#include <fst/test-properties.h>


namespace fst {

typedef CacheOptions SynchronizeFstOptions;


// Implementation class for SynchronizeFst
template <class A>
class SynchronizeFstImpl
    : public CacheImpl<A> {
 public:
  using FstImpl<A>::SetType;
  using FstImpl<A>::SetProperties;
  using FstImpl<A>::SetInputSymbols;
  using FstImpl<A>::SetOutputSymbols;

  using CacheBaseImpl< CacheState<A> >::PushArc;
  using CacheBaseImpl< CacheState<A> >::HasArcs;
  using CacheBaseImpl< CacheState<A> >::HasFinal;
  using CacheBaseImpl< CacheState<A> >::HasStart;
  using CacheBaseImpl< CacheState<A> >::SetArcs;
  using CacheBaseImpl< CacheState<A> >::SetFinal;
  using CacheBaseImpl< CacheState<A> >::SetStart;

  typedef A Arc;
  typedef typename A::Label Label;
  typedef typename A::Weight Weight;
  typedef typename A::StateId StateId;

  typedef basic_string<Label> String;

  struct Element {
    Element() {}

    Element(StateId s, const String *i, const String *o)
        : state(s), istring(i), ostring(o) {}

    StateId state;     // Input state Id
    const String *istring;     // Residual input labels
    const String *ostring;     // Residual output labels
    // Residual strings are represented by const pointers to
    // basic_string<Label> and are stored in a hash_set. The pointed
    // memory is owned by the hash_set string_set_.
  };

  SynchronizeFstImpl(const Fst<A> &fst, const SynchronizeFstOptions &opts)
      : CacheImpl<A>(opts), fst_(fst.Copy()) {
    SetType("synchronize");
    uint64 props = fst.Properties(kFstProperties, false);
    SetProperties(SynchronizeProperties(props), kCopyProperties);

    SetInputSymbols(fst.InputSymbols());
    SetOutputSymbols(fst.OutputSymbols());
  }

  SynchronizeFstImpl(const SynchronizeFstImpl &impl)
      : CacheImpl<A>(impl),
        fst_(impl.fst_->Copy(true)) {
    SetType("synchronize");
    SetProperties(impl.Properties(), kCopyProperties);
    SetInputSymbols(impl.InputSymbols());
    SetOutputSymbols(impl.OutputSymbols());
  }

  ~SynchronizeFstImpl() {
    delete fst_;
    // Extract pointers from the hash set
    vector<const String*> strings;
    typename StringSet::iterator it = string_set_.begin();
    for (; it != string_set_.end(); ++it)
      strings.push_back(*it);
    // Free the extracted pointers
    for (size_t i = 0; i < strings.size(); ++i)
      delete strings[i];
  }

  StateId Start() {
    if (!HasStart()) {
      StateId s = fst_->Start();
      if (s == kNoStateId)
        return kNoStateId;
      const String *empty = FindString(new String());
      StateId start = FindState(Element(fst_->Start(), empty, empty));
      SetStart(start);
    }
    return CacheImpl<A>::Start();
  }

  Weight Final(StateId s) {
    if (!HasFinal(s)) {
      const Element &e = elements_[s];
      Weight w = e.state == kNoStateId ? Weight::One() : fst_->Final(e.state);
      if ((w != Weight::Zero()) && (e.istring)->empty() && (e.ostring)->empty())
        SetFinal(s, w);
      else
        SetFinal(s, Weight::Zero());
    }
    return CacheImpl<A>::Final(s);
  }

  size_t NumArcs(StateId s) {
    if (!HasArcs(s))
      Expand(s);
    return CacheImpl<A>::NumArcs(s);
  }

  size_t NumInputEpsilons(StateId s) {
    if (!HasArcs(s))
      Expand(s);
    return CacheImpl<A>::NumInputEpsilons(s);
  }

  size_t NumOutputEpsilons(StateId s) {
    if (!HasArcs(s))
      Expand(s);
    return CacheImpl<A>::NumOutputEpsilons(s);
  }

  uint64 Properties() const { return Properties(kFstProperties); }

  // Set error if found; return FST impl properties.
  uint64 Properties(uint64 mask) const {
    if ((mask & kError) && fst_->Properties(kError, false))
      SetProperties(kError, kError);
    return FstImpl<Arc>::Properties(mask);
  }

  void InitArcIterator(StateId s, ArcIteratorData<A> *data) {
    if (!HasArcs(s))
      Expand(s);
    CacheImpl<A>::InitArcIterator(s, data);
  }

  // Returns the first character of the string obtained by
  // concatenating s and l.
  Label Car(const String *s, Label l = 0) const {
    if (!s->empty())
      return (*s)[0];
    else
      return l;
  }

  // Computes the residual string obtained by removing the first
  // character in the concatenation of s and l.
  const String *Cdr(const String *s, Label l = 0) {
    String *r = new String();
    for (int i = 1; i < s->size(); ++i)
      r->push_back((*s)[i]);
    if (l && !(s->empty())) r->push_back(l);
    return FindString(r);
  }

  // Computes the concatenation of s and l.
  const String *Concat(const String *s, Label l = 0) {
    String *r = new String();
    for (int i = 0; i < s->size(); ++i)
      r->push_back((*s)[i]);
    if (l) r->push_back(l);
    return FindString(r);
  }

  // Tests if the concatenation of s and l is empty
  bool Empty(const String *s, Label l = 0) const {
    if (s->empty())
      return l == 0;
    else
      return false;
  }

  // Finds the string pointed by s in the hash set. Transfers the
  // pointer ownership to the hash set.
  const String *FindString(const String *s) {
    typename StringSet::iterator it = string_set_.find(s);
    if (it != string_set_.end()) {
      delete s;
      return (*it);
    } else {
      string_set_.insert(s);
      return s;
    }
  }

  // Finds state corresponding to an element. Creates new state
  // if element not found.
  StateId FindState(const Element &e) {
    typename ElementMap::iterator eit = element_map_.find(e);
    if (eit != element_map_.end()) {
      return (*eit).second;
    } else {
      StateId s = elements_.size();
      elements_.push_back(e);
      element_map_.insert(pair<const Element, StateId>(e, s));
      return s;
    }
  }


  // Computes the outgoing transitions from a state, creating new destination
  // states as needed.
  void Expand(StateId s) {
    Element e = elements_[s];

    if (e.state != kNoStateId)
      for (ArcIterator< Fst<A> > ait(*fst_, e.state);
           !ait.Done();
           ait.Next()) {
        const A &arc = ait.Value();
        if (!Empty(e.istring, arc.ilabel)  && !Empty(e.ostring, arc.olabel)) {
          const String *istring = Cdr(e.istring, arc.ilabel);
          const String *ostring = Cdr(e.ostring, arc.olabel);
          StateId d = FindState(Element(arc.nextstate, istring, ostring));
          PushArc(s, Arc(Car(e.istring, arc.ilabel),
                        Car(e.ostring, arc.olabel), arc.weight, d));
        } else {
          const String *istring = Concat(e.istring, arc.ilabel);
          const String *ostring = Concat(e.ostring, arc.olabel);
          StateId d = FindState(Element(arc.nextstate, istring, ostring));
          PushArc(s, Arc(0 , 0, arc.weight, d));
        }
      }

    Weight w = e.state == kNoStateId ? Weight::One() : fst_->Final(e.state);
    if ((w != Weight::Zero()) &&
        ((e.istring)->size() + (e.ostring)->size() > 0)) {
      const String *istring = Cdr(e.istring);
      const String *ostring = Cdr(e.ostring);
      StateId d = FindState(Element(kNoStateId, istring, ostring));
      PushArc(s, Arc(Car(e.istring), Car(e.ostring), w, d));
    }
    SetArcs(s);
  }

 private:
  // Equality function for Elements, assume strings have been hashed.
  class ElementEqual {
   public:
    bool operator()(const Element &x, const Element &y) const {
      return x.state == y.state &&
              x.istring == y.istring &&
              x.ostring == y.ostring;
    }
  };

  // Hash function for Elements to Fst states.
  class ElementKey {
   public:
    size_t operator()(const Element &x) const {
      size_t key = x.state;
      key = (key << 1) ^ (x.istring)->size();
      for (size_t i = 0; i < (x.istring)->size(); ++i)
        key = (key << 1) ^ (*x.istring)[i];
      key = (key << 1) ^ (x.ostring)->size();
      for (size_t i = 0; i < (x.ostring)->size(); ++i)
        key = (key << 1) ^ (*x.ostring)[i];
      return key;
    }
  };

  // Equality function for strings
  class StringEqual {
   public:
    bool operator()(const String * const &x, const String * const &y) const {
      if (x->size() != y->size()) return false;
      for (size_t i = 0; i < x->size(); ++i)
        if ((*x)[i] != (*y)[i]) return false;
      return true;
    }
  };

  // Hash function for set of strings
  class StringKey{
   public:
    size_t operator()(const String * const & x) const {
      size_t key = x->size();
      for (size_t i = 0; i < x->size(); ++i)
        key = (key << 1) ^ (*x)[i];
      return key;
    }
  };


  typedef unordered_map<Element, StateId, ElementKey, ElementEqual> ElementMap;
  typedef unordered_set<const String*, StringKey, StringEqual> StringSet;

  const Fst<A> *fst_;
  vector<Element> elements_;  // mapping Fst state to Elements
  ElementMap element_map_;    // mapping Elements to Fst state
  StringSet string_set_;

  void operator=(const SynchronizeFstImpl<A> &);  // disallow
};


// Synchronizes a transducer. This version is a delayed Fst.  The
// result will be an equivalent FST that has the property that during
// the traversal of a path, the delay is either zero or strictly
// increasing, where the delay is the difference between the number of
// non-epsilon output labels and input labels along the path.
//
// For the algorithm to terminate, the input transducer must have
// bounded delay, i.e., the delay of every cycle must be zero.
//
// Complexity:
// - A has bounded delay: exponential
// - A does not have bounded delay: does not terminate
//
// References:
// - Mehryar Mohri. Edit-Distance of Weighted Automata: General
//   Definitions and Algorithms, International Journal of Computer
//   Science, 14(6): 957-982 (2003).
//
// This class attaches interface to implementation and handles
// reference counting, delegating most methods to ImplToFst.
template <class A>
class SynchronizeFst : public ImplToFst< SynchronizeFstImpl<A> > {
 public:
  friend class ArcIterator< SynchronizeFst<A> >;
  friend class StateIterator< SynchronizeFst<A> >;

  typedef A Arc;
  typedef typename A::Weight Weight;
  typedef typename A::StateId StateId;
  typedef DefaultCacheStore<A> Store;
  typedef typename Store::State State;
  typedef SynchronizeFstImpl<A> Impl;

  SynchronizeFst(const Fst<A> &fst)
      : ImplToFst<Impl>(new Impl(fst, SynchronizeFstOptions())) {}

  SynchronizeFst(const Fst<A> &fst,  const SynchronizeFstOptions &opts)
      : ImplToFst<Impl>(new Impl(fst, opts)) {}

  // See Fst<>::Copy() for doc.
  SynchronizeFst(const SynchronizeFst<A> &fst, bool safe = false)
      : ImplToFst<Impl>(fst, safe) {}

  // Get a copy of this SynchronizeFst. See Fst<>::Copy() for further doc.
  virtual SynchronizeFst<A> *Copy(bool safe = false) const {
    return new SynchronizeFst<A>(*this, safe);
  }

  virtual inline void InitStateIterator(StateIteratorData<A> *data) const;

  virtual void InitArcIterator(StateId s, ArcIteratorData<A> *data) const {
    GetImpl()->InitArcIterator(s, data);
  }

 private:
  // Makes visible to friends.
  Impl *GetImpl() const { return ImplToFst<Impl>::GetImpl(); }

  void operator=(const SynchronizeFst<A> &fst);  // Disallow
};


// Specialization for SynchronizeFst.
template<class A>
class StateIterator< SynchronizeFst<A> >
    : public CacheStateIterator< SynchronizeFst<A> > {
 public:
  explicit StateIterator(const SynchronizeFst<A> &fst)
      : CacheStateIterator< SynchronizeFst<A> >(fst, fst.GetImpl()) {}
};


// Specialization for SynchronizeFst.
template <class A>
class ArcIterator< SynchronizeFst<A> >
    : public CacheArcIterator< SynchronizeFst<A> > {
 public:
  typedef typename A::StateId StateId;

  ArcIterator(const SynchronizeFst<A> &fst, StateId s)
      : CacheArcIterator< SynchronizeFst<A> >(fst.GetImpl(), s) {
    if (!fst.GetImpl()->HasArcs(s))
      fst.GetImpl()->Expand(s);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcIterator);
};


template <class A> inline
void SynchronizeFst<A>::InitStateIterator(StateIteratorData<A> *data) const
{
  data->base = new StateIterator< SynchronizeFst<A> >(*this);
}



// Synchronizes a transducer. This version writes the synchronized
// result to a MutableFst.  The result will be an equivalent FST that
// has the property that during the traversal of a path, the delay is
// either zero or strictly increasing, where the delay is the
// difference between the number of non-epsilon output labels and
// input labels along the path.
//
// For the algorithm to terminate, the input transducer must have
// bounded delay, i.e., the delay of every cycle must be zero.
//
// Complexity:
// - A has bounded delay: exponential
// - A does not have bounded delay: does not terminate
//
// References:
// - Mehryar Mohri. Edit-Distance of Weighted Automata: General
//   Definitions and Algorithms, International Journal of Computer
//   Science, 14(6): 957-982 (2003).
template<class Arc>
void Synchronize(const Fst<Arc> &ifst, MutableFst<Arc> *ofst) {
  SynchronizeFstOptions opts;
  opts.gc_limit = 0;  // Cache only the last state for fastest copy.
  *ofst = SynchronizeFst<Arc>(ifst, opts);
}

}  // namespace fst

#endif // FST_LIB_SYNCHRONIZE_H__
