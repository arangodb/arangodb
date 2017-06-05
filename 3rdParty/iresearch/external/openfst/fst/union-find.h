
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
// Author: wojciech@google.com (Wojciech Skut)
//
// \file Union-Find algorithm for dense sets of non-negative
// integers. Implemented using disjoint tree forests with rank
// heuristics and path compression.

#ifndef FST_LIB_UNION_FIND_H_
#define FST_LIB_UNION_FIND_H_

#include <stack>
#include <vector>
using std::vector;
#include <fst/types.h>

namespace fst {

// Union-Find algorithm for dense sets of non-negative integers
// (exact type: T).
template <class T>
class UnionFind {
 public:
  // Ctor: creates a disjoint set forest for the range [0;max).
  // 'fail' is a value indicating that an element hasn't been
  // initialized using MakeSet(...). The upper bound of the range
  // can be reset (increased) using MakeSet(...).
  UnionFind(T max, T fail)
      : parent_(max, fail), rank_(max), fail_(fail) { }

  // Finds the representative of the set 'item' belongs to.
  // Performs path compression if needed.
  T FindSet(T item) {
    if (item >= parent_.size()
        || item == fail_
        || parent_[item] == fail_) return fail_;

    T *p = &parent_[item];
    for (; *p != item; item = *p, p = &parent_[item]) {
      exec_stack_.push(p);
    }
    for (; ! exec_stack_.empty(); exec_stack_.pop()) {
      *exec_stack_.top() = *p;
    }
    return *p;
  }

  // Creates the (destructive) union of the sets x and y belong to.
  void Union(T x, T y) {
    Link(FindSet(x), FindSet(y));
  }

  // Initialization of an element: creates a singleton set containing
  // 'item'. The range [0;max) is reset if item >= max.
  T MakeSet(T item) {
    if (item >= parent_.size()) {
      // New value in parent_ should be initialized to fail_
      size_t nitem = item > 0 ? 2 * item : 2;
      parent_.resize(nitem, fail_);
      rank_.resize(nitem);
    }
    parent_[item] = item;
    return item;
  }

  // Initialization of all elements starting from 0 to max - 1 to distinct sets
  void MakeAllSet(T max) {
    parent_.resize(max);
    for (T item = 0; item < max; ++item) {
      parent_[item] = item;
    }
  }

 private:
  vector<T> parent_;      // Parent nodes.
  vector<int> rank_;      // Rank of an element = min. depth in tree.
  T fail_;                // Value indicating lookup failure.
  stack<T*> exec_stack_;  // Used for path compression.

  // Links trees rooted in 'x' and 'y'.
  void Link(T x, T y) {
    if (x == y) return;

    if (rank_[x] > rank_[y]) {
      parent_[y] = x;
    } else {
      parent_[x] = y;
      if (rank_[x] == rank_[y]) {
        ++rank_[y];
      }
    }
  }
  DISALLOW_COPY_AND_ASSIGN(UnionFind);
};

}  // namespace fst

#endif  // FST_LIB_UNION_FIND_H_
