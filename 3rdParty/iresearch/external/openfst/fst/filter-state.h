// filter-state.h

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
// Classes for storing filter state in various algorithms like composition.
//

#ifndef FST_LIB_FILTER_STATE_H_
#define FST_LIB_FILTER_STATE_H_

#include <forward_list>
using std::forward_list;

#include <fst/fst.h>
#include <fst/fst-decl.h>  // For optional argument declarations
#include <fst/matcher.h>


namespace fst {

// FILTER STATE - this represents the state of the algorithm
// (e.g. composition) filter. It has the form:
//
// class FilterState {
//  public:
//   // Required constructors
//   FilterState();
//   FilterState(const FilterState &f);
//   // An invalid filter state.
//   static const FilterState NoState();
//   // Maps state to integer for hashing.
//   size_t Hash() const;
//   // Equality of filter states.
//   bool operator==(const FilterState &f) const;
//   // Inequality of filter states.
//   bool operator!=(const FilterState &f) const;
//   // Assignment to filter states.
//   FilterState& operator=(const FilterState& f);
// };

// Filter state that is a signed integral type.
template <typename T>
class IntegerFilterState {
 public:
  IntegerFilterState() : state_(kNoStateId) {}
  explicit IntegerFilterState(T s) : state_(s) {}

  static const IntegerFilterState NoState() { return IntegerFilterState(); }

  size_t Hash() const { return static_cast<size_t>(state_); }

  bool operator==(const IntegerFilterState &f) const {
    return state_ == f.state_;
  }

  bool operator!=(const IntegerFilterState &f) const {
    return state_ != f.state_;
  }

  T GetState() const { return state_; }

  void SetState(T state) { state_ = state; }

private:
  T state_;
};

typedef IntegerFilterState<signed char> CharFilterState;
typedef IntegerFilterState<short> ShortFilterState;
typedef IntegerFilterState<int> IntFilterState;


// Filter state that is a weight (class).
template <class W>
class WeightFilterState {
 public:
  WeightFilterState() : weight_(W::Zero()) {}
  explicit WeightFilterState(W w) : weight_(w) {}

  static const WeightFilterState NoState() { return WeightFilterState(); }

  size_t Hash() const { return weight_.Hash(); }

  bool operator==(const WeightFilterState &f) const {
    return weight_ == f.weight_;
  }

  bool operator!=(const WeightFilterState &f) const {
    return weight_ != f.weight_;
  }

  W GetWeight() const { return weight_; }

  void SetWeight(W w) { weight_ = w; }

private:
  W weight_;
};


// Filter state is a list of signed integer types T. Order matters
// for equality.
template <typename T>
class ListFilterState {
 public:
  ListFilterState() {}

  explicit ListFilterState(T s) { list_.push_front(s); }

  static const ListFilterState NoState() { return ListFilterState(kNoStateId); }

  size_t Hash() const {
    size_t h = 0;
    for (typename std::forward_list<T>::const_iterator iter = list_.begin();
         iter != list_.end(); ++iter) {
      h ^= h << 1  ^ *iter;
    }
    return h;
  }

  bool operator==(const ListFilterState &f) const {
    return list_ == f.list_;
  }

  bool operator!=(const ListFilterState &f) const {
    return list_ != f.list_;
  }

  const std::forward_list<T> &GetState() const { return list_; }

  std::forward_list<T> *GetMutableState() { return &list_; }

  void SetState(const std::forward_list<T> &state) { list_ = state; }

 private:
  std::forward_list<T> list_;
};


// Filter state that is the combination of two filter states.
template <class F1, class F2>
class PairFilterState {
 public:
  PairFilterState() : f1_(F1::NoState()), f2_(F2::NoState()) {}

  PairFilterState(const F1 &f1, const F2 &f2) : f1_(f1), f2_(f2) {}

  static const PairFilterState NoState() { return PairFilterState(); }

  size_t Hash() const {
    size_t h1 = f1_.Hash();
    size_t h2 = f2_.Hash();
    const int lshift = 5;
    const int rshift = CHAR_BIT * sizeof(size_t) - 5;
    return h1 << lshift ^ h1 >> rshift ^ h2;
  }

  bool operator==(const PairFilterState &f) const {
    return f1_ == f.f1_ && f2_ == f.f2_;
  }

  bool operator!=(const PairFilterState &f) const {
    return f1_ != f.f1_ || f2_ != f.f2_;
  }

  const F1 &GetState1() const { return f1_; }
  const F2 &GetState2() const { return f2_; }

  void SetState(const F1 &f1, const F2 &f2) {
    f1_ = f1;
    f2_ = f2;
  }

private:
  F1 f1_;
  F2 f2_;
};

}  // namespace fst

#endif  // FST_LIB_FILTER_STATE_H_
