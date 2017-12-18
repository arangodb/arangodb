// dfs-visit.h

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
// Depth-first search visitation. See visit.h for more general
// search queue disciplines.

#ifndef FST_LIB_DFS_VISIT_H__
#define FST_LIB_DFS_VISIT_H__

#include <stack>
#include <vector>
using std::vector;

#include <fst/arcfilter.h>
#include <fst/fst.h>


namespace fst {

// Visitor Interface - class determines actions taken during a Dfs.
// If any of the boolean member functions return false, the DFS is
// aborted by first calling FinishState() on all currently grey states
// and then calling FinishVisit().
//
// Note this is similar to the more general visitor interface in visit.h
// except that FinishState returns additional information appropriate only for
// a DFS and some methods names here are better suited to a DFS.
//
// template <class Arc>
// class Visitor {
//  public:
//   typedef typename Arc::StateId StateId;
//
//   Visitor(T *return_data);
//   // Invoked before DFS visit
//   void InitVisit(const Fst<Arc> &fst);
//   // Invoked when state discovered (2nd arg is DFS tree root)
//   bool InitState(StateId s, StateId root);
//   // Invoked when tree arc examined (to white/undiscovered state)
//   bool TreeArc(StateId s, const Arc &a);
//   // Invoked when back arc examined (to grey/unfinished state)
//   bool BackArc(StateId s, const Arc &a);
//   // Invoked when forward or cross arc examined (to black/finished state)
//   bool ForwardOrCrossArc(StateId s, const Arc &a);
//   // Invoked when state finished (PARENT is kNoStateID and ARC == NULL
//   // when S is tree root)
//   void FinishState(StateId s, StateId parent, const Arc *parent_arc);
//   // Invoked after DFS visit
//   void FinishVisit();
// };

// An Fst state's DFS status
const int kDfsWhite = 0;   // Undiscovered
const int kDfsGrey =  1;   // Discovered & unfinished
const int kDfsBlack = 2;   // Finished

// An Fst state's DFS stack state
template <class Arc>
struct DfsState {
  typedef typename Arc::StateId StateId;

  DfsState(const Fst<Arc> &fst, StateId s): state_id(s), arc_iter(fst, s) {}

  void *operator new(size_t size, MemoryPool< DfsState<Arc> > *pool) {
    return pool->Allocate();
  }

  static void Destroy(DfsState<Arc> *dfs_state,
                      MemoryPool< DfsState<Arc> > *pool) {
    if (dfs_state) {
      dfs_state->~DfsState<Arc>();
      pool->Free(dfs_state);
    }
  }

  StateId state_id;       // Fst state ...
  ArcIterator< Fst<Arc> > arc_iter;  // and its corresponding arcs
};

// Performs depth-first visitation. Visitor class argument determines
// actions and contains any return data. ArcFilter determines arcs
// that are considered.  If 'access_only' is true, performs visitation
// only to states accessible from the initial state.
//
// Note this is similar to Visit() in visit.h called with a LIFO
// queue except this version has a Visitor class specialized and
// augmented for a DFS.
template <class Arc, class V, class ArcFilter>
void DfsVisit(const Fst<Arc> &fst, V *visitor, ArcFilter filter,
              bool access_only = false) {
  typedef typename Arc::StateId StateId;

  visitor->InitVisit(fst);

  StateId start = fst.Start();
  if (start == kNoStateId) {
    visitor->FinishVisit();
    return;
  }

  vector<char> state_color;                // Fst state DFS status
  stack<DfsState<Arc> *> state_stack;      // DFS execution stack
  MemoryPool< DfsState<Arc> > state_pool;  // Pool for DFSStates

  StateId nstates = start + 1;             // # of known states in general case
  bool expanded = false;
  if (fst.Properties(kExpanded, false)) {  // tests if expanded case, then
    nstates = CountStates(fst);            // uses ExpandedFst::NumStates().
    expanded = true;
  }

  state_color.resize(nstates, kDfsWhite);
  StateIterator< Fst<Arc> > siter(fst);

  // Continue DFS while true
  bool dfs = true;

  // Iterate over trees in DFS forest.
  for (StateId root = start; dfs && root < nstates;) {
    state_color[root] = kDfsGrey;
    state_stack.push(new(&state_pool) DfsState<Arc>(fst, root));
    dfs = visitor->InitState(root, root);
    while (!state_stack.empty()) {
      DfsState<Arc> *dfs_state = state_stack.top();
      StateId s = dfs_state->state_id;
      if (s >= state_color.size()) {
        nstates = s + 1;
        state_color.resize(nstates, kDfsWhite);
      }
      ArcIterator< Fst<Arc> > &aiter = dfs_state->arc_iter;
      if (!dfs || aiter.Done()) {
        state_color[s] = kDfsBlack;
        DfsState<Arc>::Destroy(dfs_state,  &state_pool);
        state_stack.pop();
        if (!state_stack.empty()) {
          DfsState<Arc> *parent_state = state_stack.top();
          StateId p = parent_state->state_id;
          ArcIterator< Fst<Arc> > &piter = parent_state->arc_iter;
          visitor->FinishState(s, p, &piter.Value());
          piter.Next();
        } else {
          visitor->FinishState(s, kNoStateId, 0);
        }
        continue;
      }
      const Arc &arc = aiter.Value();
      if (arc.nextstate >= state_color.size()) {
        nstates = arc.nextstate + 1;
        state_color.resize(nstates, kDfsWhite);
      }
      if (!filter(arc)) {
        aiter.Next();
        continue;
      }
      int next_color = state_color[arc.nextstate];
      switch (next_color) {
        default:
        case kDfsWhite:
          dfs = visitor->TreeArc(s, arc);
          if (!dfs) break;
          state_color[arc.nextstate] = kDfsGrey;
          state_stack.push(new(&state_pool) DfsState<Arc>(fst, arc.nextstate));
          dfs = visitor->InitState(arc.nextstate, root);
          break;
        case kDfsGrey:
          dfs = visitor->BackArc(s, arc);
          aiter.Next();
          break;
        case kDfsBlack:
          dfs = visitor->ForwardOrCrossArc(s, arc);
          aiter.Next();
          break;
      }
    }

    if (access_only)
      break;

    // Find next tree root
    for (root = root == start ? 0 : root + 1;
         root < nstates && state_color[root] != kDfsWhite;
         ++root) {
    }

    // Check for a state beyond the largest known state
    if (!expanded && root == nstates) {
      for (; !siter.Done(); siter.Next()) {
        if (siter.Value() == nstates) {
          ++nstates;
          state_color.push_back(kDfsWhite);
          break;
        }
      }
    }
  }
  visitor->FinishVisit();
}


template <class Arc, class V>
void DfsVisit(const Fst<Arc> &fst, V *visitor) {
  DfsVisit(fst, visitor, AnyArcFilter<Arc>());
}

}  // namespace fst

#endif  // FST_LIB_DFS_VISIT_H__
