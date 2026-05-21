// Copyright 2005-2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the 'License');
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an 'AS IS' BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// See www.openfst.org for extensive documentation on this weighted
// finite-state transducer library.
//
// Depth-first search visitation. See visit.h for more general search queue
// disciplines.

#ifndef FST_DFS_VISIT_H_
#define FST_DFS_VISIT_H_

#include <cstdint>
#include <stack>
#include <vector>

#include <fst/arcfilter.h>
#include <fst/fst.h>


namespace fst {

// Visitor Interface: class determining actions taken during a depth-first
// search-style visit. If any of the boolean member functions return false, the
// DFS is aborted by first calling FinishState() on all currently grey states
// and then calling FinishVisit().
//
// This is similar to the more general visitor interface in visit.h, except
// that FinishState returns additional information appropriate only for a DFS
// and some methods names here are better suited to a DFS.
//
// template <class Arc>
// class Visitor {
//  public:
//   using StateId = typename Arc::StateId;
//
//   Visitor(T *return_data);
//
//   // Invoked before DFS visit.
//   void InitVisit(const Fst<Arc> &fst);
//
//   // Invoked when state discovered (2nd arg is DFS tree root).
//   bool InitState(StateId s, StateId root);
//
//   // Invoked when tree arc to white/undiscovered state examined.
//   bool TreeArc(StateId s, const Arc &arc);
//
//   // Invoked when back arc to grey/unfinished state examined.
//   bool BackArc(StateId s, const Arc &arc);
//
//   // Invoked when forward or cross arc to black/finished state examined.
//   bool ForwardOrCrossArc(StateId s, const Arc &arc);
//
//   // Invoked when state finished ('s' is tree root, 'parent' is kNoStateId,
//   // and 'arc' is nullptr).
//   void FinishState(StateId s, StateId parent, const Arc *arc);
//
//   // Invoked after DFS visit.
//   void FinishVisit();
// };

namespace internal {

// An FST state's DFS stack state.
template <class FST>
struct DfsState {
  using Arc = typename FST::Arc;
  using StateId = typename Arc::StateId;

  DfsState(const FST &fst, StateId s) : state_id(s), arc_iter(fst, s) {}

  void *operator new(size_t size, MemoryPool<DfsState<FST>> *pool) {
    return pool->Allocate();
  }

  static void Destroy(DfsState<FST> *dfs_state,
                      MemoryPool<DfsState<FST>> *pool) {
    if (dfs_state) {
      dfs_state->~DfsState<FST>();
      pool->Free(dfs_state);
    }
  }

  StateId state_id;           // FST state.
  ArcIterator<FST> arc_iter;  // The corresponding arcs.
};

}  // namespace internal

// Performs depth-first visitation. Visitor class argument determines actions
// and contains any return data. ArcFilter determines arcs that are considered.
// If 'access_only' is true, performs visitation only to states accessible from
// the initial state.
//
// Note this is similar to Visit() in visit.h called with a LIFO queue, except
// this version has a Visitor class specialized and augmented for a DFS.
template <class FST, class Visitor, class ArcFilter>
void DfsVisit(const FST &fst, Visitor *visitor, ArcFilter filter,
              bool access_only = false) {
  visitor->InitVisit(fst);
  const auto start = fst.Start();
  if (start == kNoStateId) {
    visitor->FinishVisit();
    return;
  }
  // An FST state's DFS status
  enum class StateColor : uint8_t {
    kWhite = 0,  // Undiscovered.
    kGrey = 1,   // Discovered but unfinished.
    kBlack = 2,  // Finished.
  };
  std::vector<StateColor> state_color;
  std::stack<internal::DfsState<FST> *> state_stack;  // DFS execution stack.
  MemoryPool<internal::DfsState<FST>> state_pool;     // Pool for DFSStates.
  auto nstates = start + 1;  // Number of known states in general case.
  bool expanded = false;
  if (fst.Properties(kExpanded, false)) {  // Tests if expanded case, then
    nstates = CountStates(fst);            // uses ExpandedFst::NumStates().
    expanded = true;
  }
  state_color.resize(nstates, StateColor::kWhite);
  StateIterator<FST> siter(fst);
  // Continue DFS while true.
  bool dfs = true;
  // Iterate over trees in DFS forest.
  for (auto root = start; dfs && root < nstates;) {
    state_color[root] = StateColor::kGrey;
    state_stack.push(new (&state_pool) internal::DfsState<FST>(fst, root));
    dfs = visitor->InitState(root, root);
    while (!state_stack.empty()) {
      auto *dfs_state = state_stack.top();
      const auto s = dfs_state->state_id;
      if (s >= static_cast<decltype(s)>(state_color.size())) {
        nstates = s + 1;
        state_color.resize(nstates, StateColor::kWhite);
      }
      ArcIterator<FST> &aiter = dfs_state->arc_iter;
      if (!dfs || aiter.Done()) {
        state_color[s] = StateColor::kBlack;
        internal::DfsState<FST>::Destroy(dfs_state, &state_pool);
        state_stack.pop();
        if (!state_stack.empty()) {
          auto *parent_state = state_stack.top();
          auto &piter = parent_state->arc_iter;
          visitor->FinishState(s, parent_state->state_id, &piter.Value());
          piter.Next();
        } else {
          visitor->FinishState(s, kNoStateId, nullptr);
        }
        continue;
      }
      const auto &arc = aiter.Value();
      if (arc.nextstate >=
          static_cast<decltype(arc.nextstate)>(state_color.size())) {
        nstates = arc.nextstate + 1;
        state_color.resize(nstates, StateColor::kWhite);
      }
      if (!filter(arc)) {
        aiter.Next();
        continue;
      }
      const auto next_color = state_color[arc.nextstate];
      switch (next_color) {
        case StateColor::kWhite:
          dfs = visitor->TreeArc(s, arc);
          if (!dfs) break;
          state_color[arc.nextstate] = StateColor::kGrey;
          state_stack.push(new (&state_pool)
                               internal::DfsState<FST>(fst, arc.nextstate));
          dfs = visitor->InitState(arc.nextstate, root);
          break;
        case StateColor::kGrey:
          dfs = visitor->BackArc(s, arc);
          aiter.Next();
          break;
        case StateColor::kBlack:
          dfs = visitor->ForwardOrCrossArc(s, arc);
          aiter.Next();
          break;
      }
    }
    if (access_only) break;
    // Finds next tree root.
    for (root = root == start ? 0 : root + 1;
         root < nstates && state_color[root] != StateColor::kWhite; ++root) {
    }
    // Checks for a state beyond the largest known state.
    if (!expanded && root == nstates) {
      for (; !siter.Done(); siter.Next()) {
        if (siter.Value() == nstates) {
          ++nstates;
          state_color.push_back(StateColor::kWhite);
          break;
        }
      }
    }
  }
  visitor->FinishVisit();
}

template <class Arc, class Visitor>
void DfsVisit(const Fst<Arc> &fst, Visitor *visitor) {
  DfsVisit(fst, visitor, AnyArcFilter<Arc>());
}

}  // namespace fst

#endif  // FST_DFS_VISIT_H_
