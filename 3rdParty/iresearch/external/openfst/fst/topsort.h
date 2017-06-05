// topsort.h

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
// Topological sort of FSTs

#ifndef FST_LIB_TOPSORT_H__
#define FST_LIB_TOPSORT_H__

#include <algorithm>
#include <vector>
using std::vector;


#include <fst/dfs-visit.h>
#include <fst/fst.h>
#include <fst/statesort.h>


namespace fst {

// DFS visitor class to return topological ordering.
template <class A>
class TopOrderVisitor {
 public:
  typedef A Arc;
  typedef typename A::StateId StateId;

  // If acyclic, ORDER[i] gives the topological position of state Id i;
  // otherwise unchanged. ACYCLIC will be true iff the FST has
  // no cycles.
  TopOrderVisitor(vector<StateId> *order, bool *acyclic)
      : order_(order), acyclic_(acyclic) {}

  void InitVisit(const Fst<A> &fst) {
    finish_ = new vector<StateId>;
    *acyclic_ = true;
  }

  bool InitState(StateId s, StateId r) { return true; }

  bool TreeArc(StateId s, const A &arc) { return true; }

  bool BackArc(StateId s, const A &arc) { return (*acyclic_ = false); }

  bool ForwardOrCrossArc(StateId s, const A &arc) { return true; }

  void FinishState(StateId s, StateId p, const A *) { finish_->push_back(s); }

  void FinishVisit() {
    if (*acyclic_) {
      order_->clear();
      for (StateId s = 0; s < finish_->size(); ++s)
        order_->push_back(kNoStateId);
      for (StateId s = 0; s < finish_->size(); ++s)
        (*order_)[(*finish_)[finish_->size() - s - 1]] = s;
    }
    delete finish_;
  }

 private:
  vector<StateId> *order_;
  bool *acyclic_;
  vector<StateId> *finish_;  // states in finishing-time order
};


// Topologically sorts its input if acyclic, modifying it.  Otherwise,
// the input is unchanged.  When sorted, all transitions are from
// lower to higher state IDs.
//
// Complexity:
// - Time:  O(V + E)
// - Space: O(V + E)
// where V = # of states and E = # of arcs.
template <class Arc>
bool TopSort(MutableFst<Arc> *fst) {
  typedef typename Arc::StateId StateId;

  vector<StateId> order;
  bool acyclic;

  TopOrderVisitor<Arc> top_order_visitor(&order, &acyclic);
  DfsVisit(*fst, &top_order_visitor);

  if (acyclic) {
    StateSort(fst, order);
    fst->SetProperties(kAcyclic | kInitialAcyclic | kTopSorted,
                       kAcyclic | kInitialAcyclic | kTopSorted);
  } else {
    fst->SetProperties(kCyclic | kNotTopSorted, kCyclic | kNotTopSorted);
  }
  return acyclic;
}

}  // namespace fst

#endif  // FST_LIB_TOPSORT_H__
