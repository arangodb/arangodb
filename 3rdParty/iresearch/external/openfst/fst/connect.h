// connect.h

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
// Classes and functions to remove unsuccessful paths from an Fst.

#ifndef FST_LIB_CONNECT_H__
#define FST_LIB_CONNECT_H__

#include <vector>
using std::vector;

#include <fst/dfs-visit.h>
#include <fst/union-find.h>
#include <fst/mutable-fst.h>


namespace fst {

// Finds and returns connected components. Use with Visit().
template <class A>
class CcVisitor {
 public:
  typedef A Arc;
  typedef typename Arc::Weight Weight;
  typedef typename A::StateId StateId;

  // cc[i]: connected component number for state i.
  CcVisitor(vector<StateId> *cc)
      : comps_(new UnionFind<StateId>(0, kNoStateId)),
        cc_(cc),
        nstates_(0) { }

  // comps: connected components equiv classes.
  CcVisitor(UnionFind<StateId> *comps)
      : comps_(comps),
        cc_(0),
        nstates_(0) { }

  ~CcVisitor() {
    if (cc_)  // own comps_?
      delete comps_;
  }

  void InitVisit(const Fst<A> &fst) { }

  bool InitState(StateId s, StateId root) {
    ++nstates_;
    if (comps_->FindSet(s) == kNoStateId)
      comps_->MakeSet(s);
    return true;
  }

  bool WhiteArc(StateId s, const A &arc) {
    comps_->MakeSet(arc.nextstate);
    comps_->Union(s, arc.nextstate);
    return true;
  }

  bool GreyArc(StateId s, const A &arc) {
    comps_->Union(s, arc.nextstate);
    return true;
  }

  bool BlackArc(StateId s, const A &arc) {
    comps_->Union(s, arc.nextstate);
    return true;
  }

  void FinishState(StateId s) { }

  void FinishVisit() {
    if (cc_)
      GetCcVector(cc_);
  }

  // cc[i]: connected component number for state i.
  // Returns number of components.
  int GetCcVector(vector<StateId> *cc) {
    cc->clear();
    cc->resize(nstates_, kNoStateId);
    StateId ncomp = 0;
    for (StateId i = 0; i < nstates_; ++i) {
      StateId rep = comps_->FindSet(i);
      StateId &comp = (*cc)[rep];
      if (comp == kNoStateId) {
        comp = ncomp;
        ++ncomp;
      }
      (*cc)[i] = comp;
    }
    return ncomp;
  }

 private:
  UnionFind<StateId> *comps_;   // Components
  vector<StateId> *cc_;         // State's cc number
  StateId nstates_;             // State count
};


// Finds and returns strongly-connected components, accessible and
// coaccessible states and related properties. Uses Tarjan's single
// DFS SCC algorithm (see Aho, et al, "Design and Analysis of Computer
// Algorithms", 189pp). Use with DfsVisit();
template <class A>
class SccVisitor {
 public:
  typedef A Arc;
  typedef typename A::Weight Weight;
  typedef typename A::StateId StateId;

  // scc[i]: strongly-connected component number for state i.
  //   SCC numbers will be in topological order for acyclic input.
  // access[i]: accessibility of state i.
  // coaccess[i]: coaccessibility of state i.
  // Any of above can be NULL.
  // props: related property bits (cyclicity, initial cyclicity,
  //   accessibility, coaccessibility) set/cleared (o.w. unchanged).
  SccVisitor(vector<StateId> *scc, vector<bool> *access,
             vector<bool> *coaccess, uint64 *props)
      : scc_(scc), access_(access), coaccess_(coaccess), props_(props) {}
  SccVisitor(uint64 *props)
      : scc_(0), access_(0), coaccess_(0), props_(props) {}

  void InitVisit(const Fst<A> &fst);

  bool InitState(StateId s, StateId root);

  bool TreeArc(StateId s, const A &arc) { return true; }

  bool BackArc(StateId s, const A &arc) {
    StateId t = arc.nextstate;
    if ((*dfnumber_)[t] < (*lowlink_)[s])
      (*lowlink_)[s] = (*dfnumber_)[t];
    if ((*coaccess_)[t])
      (*coaccess_)[s] = true;
    *props_ |= kCyclic;
    *props_ &= ~kAcyclic;
    if (arc.nextstate == start_) {
      *props_ |= kInitialCyclic;
      *props_ &= ~kInitialAcyclic;
    }
    return true;
  }

  bool ForwardOrCrossArc(StateId s, const A &arc) {
    StateId t = arc.nextstate;
    if ((*dfnumber_)[t] < (*dfnumber_)[s] /* cross edge */ &&
        (*onstack_)[t] && (*dfnumber_)[t] < (*lowlink_)[s])
      (*lowlink_)[s] = (*dfnumber_)[t];
    if ((*coaccess_)[t])
      (*coaccess_)[s] = true;
    return true;
  }

  void FinishState(StateId s, StateId p, const A *);

  void FinishVisit() {
    // Numbers SCC's in topological order when acyclic.
    if (scc_)
      for (StateId i = 0; i < scc_->size(); ++i)
        (*scc_)[i] = nscc_ - 1 - (*scc_)[i];
    if (coaccess_internal_)
      delete coaccess_;
    delete dfnumber_;
    delete lowlink_;
    delete onstack_;
    delete scc_stack_;
  }

 private:
  vector<StateId> *scc_;        // State's scc number
  vector<bool> *access_;        // State's accessibility
  vector<bool> *coaccess_;      // State's coaccessibility
  uint64 *props_;
  const Fst<A> *fst_;
  StateId start_;
  StateId nstates_;             // State count
  StateId nscc_;                // SCC count
  bool coaccess_internal_;
  vector<StateId> *dfnumber_;   // state discovery times
  vector<StateId> *lowlink_;    // lowlink[s] == dfnumber[s] => SCC root
  vector<bool> *onstack_;       // is a state on the SCC stack
  vector<StateId> *scc_stack_;  // SCC stack (w/ random access)
};

template <class A> inline
void SccVisitor<A>::InitVisit(const Fst<A> &fst) {
  if (scc_)
    scc_->clear();
  if (access_)
    access_->clear();
  if (coaccess_) {
    coaccess_->clear();
    coaccess_internal_ = false;
  } else {
    coaccess_ = new vector<bool>;
    coaccess_internal_ = true;
  }
  *props_ |= kAcyclic | kInitialAcyclic | kAccessible | kCoAccessible;
  *props_ &= ~(kCyclic | kInitialCyclic | kNotAccessible | kNotCoAccessible);
  fst_ = &fst;
  start_ = fst.Start();
  nstates_ = 0;
  nscc_ = 0;
  dfnumber_ = new vector<StateId>;
  lowlink_ = new vector<StateId>;
  onstack_ = new vector<bool>;
  scc_stack_ = new vector<StateId>;
}

template <class A> inline
bool SccVisitor<A>::InitState(StateId s, StateId root) {
  scc_stack_->push_back(s);
  while (dfnumber_->size() <= s) {
    if (scc_)
      scc_->push_back(-1);
    if (access_)
      access_->push_back(false);
    coaccess_->push_back(false);
    dfnumber_->push_back(-1);
    lowlink_->push_back(-1);
    onstack_->push_back(false);
  }
  (*dfnumber_)[s] = nstates_;
  (*lowlink_)[s] = nstates_;
  (*onstack_)[s] = true;
  if (root == start_) {
    if (access_)
      (*access_)[s] = true;
  } else {
    if (access_)
      (*access_)[s] = false;
    *props_ |= kNotAccessible;
    *props_ &= ~kAccessible;
  }
  ++nstates_;
  return true;
}

template <class A> inline
void SccVisitor<A>::FinishState(StateId s, StateId p, const A *) {
  if (fst_->Final(s) != Weight::Zero())
    (*coaccess_)[s] = true;
  if ((*dfnumber_)[s] == (*lowlink_)[s]) {  // root of new SCC
    bool scc_coaccess = false;
    size_t i = scc_stack_->size();
    StateId t;
    do {
      t = (*scc_stack_)[--i];
      if ((*coaccess_)[t])
        scc_coaccess = true;
    } while (s != t);
    do {
      t = scc_stack_->back();
      if (scc_)
        (*scc_)[t] = nscc_;
      if (scc_coaccess)
        (*coaccess_)[t] = true;
      (*onstack_)[t] = false;
      scc_stack_->pop_back();
    } while (s != t);
    if (!scc_coaccess) {
      *props_ |= kNotCoAccessible;
      *props_ &= ~kCoAccessible;
    }
    ++nscc_;
  }
  if (p != kNoStateId) {
    if ((*coaccess_)[s])
      (*coaccess_)[p] = true;
    if ((*lowlink_)[s] < (*lowlink_)[p])
      (*lowlink_)[p] = (*lowlink_)[s];
  }
}


// Trims an FST, removing states and arcs that are not on successful
// paths. This version modifies its input.
//
// Complexity:
// - Time:  O(V + E)
// - Space: O(V + E)
// where V = # of states and E = # of arcs.
template<class Arc>
void Connect(MutableFst<Arc> *fst) {
  typedef typename Arc::StateId StateId;

  vector<bool> access;
  vector<bool> coaccess;
  uint64 props = 0;
  SccVisitor<Arc> scc_visitor(0, &access, &coaccess, &props);
  DfsVisit(*fst, &scc_visitor);
  vector<StateId> dstates;
  for (StateId s = 0; s < access.size(); ++s)
    if (!access[s] || !coaccess[s])
      dstates.push_back(s);
  fst->DeleteStates(dstates);
  fst->SetProperties(kAccessible | kCoAccessible, kAccessible | kCoAccessible);
}


// Returns an acyclic FST where each SCC in the input FST has been
// condensed to a single state with transitions between SCCs retained
// and within SCCs dropped.  Also returns the mapping from an input
// state 's' to an output state 'scc[s]'.
template<class Arc>
void Condense(const Fst<Arc> &ifst, MutableFst<Arc> *ofst,
              vector<typename Arc::StateId> *scc) {
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Weight Weight;

  ofst->DeleteStates();
  uint64 props = 0;
  SccVisitor<Arc> scc_visitor(scc, 0, 0, &props);
  DfsVisit(ifst, &scc_visitor);
  for (StateId s = 0; s < scc->size(); ++s) {
    StateId c = (*scc)[s];
    while (c >= ofst->NumStates())
      ofst->AddState();
    if (s == ifst.Start())
      ofst->SetStart(c);
    Weight final = ifst.Final(s);
    if (final != Weight::Zero())
      ofst->SetFinal(c, Plus(ofst->Final(c), final));
    for (ArcIterator< Fst<Arc> > aiter(ifst, s);
         !aiter.Done();
         aiter.Next()) {
      Arc arc = aiter.Value();
      StateId nextc = (*scc)[arc.nextstate];
      if (nextc != c) {
        while (nextc >= ofst->NumStates())
          ofst->AddState();
        arc.nextstate = nextc;
        ofst->AddArc(c, arc);
      }
    }
  }
  ofst->SetProperties(kAcyclic | kInitialAcyclic, kAcyclic | kInitialAcyclic);
}

}  // namespace fst

#endif  // FST_LIB_CONNECT_H__
