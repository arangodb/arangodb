// state-table.h

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
// Classes for representing the mapping between state tuples and state Ids.

#ifndef FST_LIB_STATE_TABLE_H__
#define FST_LIB_STATE_TABLE_H__

#include <deque>
using std::deque;
#include <utility>
using std::pair; using std::make_pair;
#include <vector>
using std::vector;

#include <fst/bi-table.h>
#include <fst/expanded-fst.h>


namespace fst {

// STATE TABLES - these determine the bijective mapping between state
// tuples (e.g. in composition triples of two FST states and a
// composition filter state) and their corresponding state IDs.
// They are classes, templated on state tuples, of the form:
//
// template <class T>
// class StateTable {
//  public:
//   typedef typename T StateTuple;
//
//   // Required constructors.
//   StateTable();
//   StateTable(const StateTable &);
//
//   // Lookup state ID by tuple. If it doesn't exist, then add it.
//   StateId FindState(const StateTuple &);
//   // Lookup state tuple by state ID.
//   const StateTuple<StateId> &Tuple(StateId) const;
//   // # of stored tuples.
//   StateId Size() const;
// };
//
// A state tuple has the form:
//
// template <class S>
// struct StateTuple {
//   typedef typename S StateId;
//
//   // Required constructors.
//   StateTuple();
//   StateTuple(const StateTuple &);
// };


// An implementation using a hash map for the tuple to state ID mapping.
// The state tuple T must have == defined. H is the hash function.
template <class T, class H>
class HashStateTable : public HashBiTable<typename T::StateId, T, H> {
 public:
  typedef T StateTuple;
  typedef typename StateTuple::StateId StateId;
  using HashBiTable<StateId, T, H>::FindId;
  using HashBiTable<StateId, T, H>::FindEntry;
  using HashBiTable<StateId, T, H>::Size;

  HashStateTable() : HashBiTable<StateId, T, H>() {}

  // Reserves space for table_size elements.
  explicit HashStateTable(size_t table_size)
      : HashBiTable<StateId, T, H>(table_size) {}

  StateId FindState(const StateTuple &tuple) { return FindId(tuple); }
  const StateTuple &Tuple(StateId s) const { return FindEntry(s); }
};


// An implementation using a hash map for the tuple to state ID mapping.
// The state tuple T must have == defined. H is the hash function.
template <class T, class H>
class CompactHashStateTable
    : public CompactHashBiTable<typename T::StateId, T, H> {
 public:
  typedef T StateTuple;
  typedef typename StateTuple::StateId StateId;
  using CompactHashBiTable<StateId, T, H>::FindId;
  using CompactHashBiTable<StateId, T, H>::FindEntry;
  using CompactHashBiTable<StateId, T, H>::Size;

  CompactHashStateTable() : CompactHashBiTable<StateId, T, H>() {}

  // Reserves space for 'table_size' elements.
  explicit CompactHashStateTable(size_t table_size)
      : CompactHashBiTable<StateId, T, H>(table_size) {}

  StateId FindState(const StateTuple &tuple) { return FindId(tuple); }
  const StateTuple &Tuple(StateId s) const { return FindEntry(s); }
};

// An implementation using a vector for the tuple to state mapping.
// It is passed a function object FP that should fingerprint tuples
// uniquely to an integer that can used as a vector index. Normally,
// VectorStateTable constructs the FP object.  The user can instead
// pass in this object; in that case, VectorStateTable takes its
// ownership.
template <class T, class FP>
class VectorStateTable
    : public VectorBiTable<typename T::StateId, T, FP> {
 public:
  typedef T StateTuple;
  typedef typename StateTuple::StateId StateId;
  using VectorBiTable<StateId, T, FP>::FindId;
  using VectorBiTable<StateId, T, FP>::FindEntry;
  using VectorBiTable<StateId, T, FP>::Size;
  using VectorBiTable<StateId, T, FP>::Fingerprint;

  // Reserves space for 'table_size' elements.
  explicit VectorStateTable(FP *fp = 0, size_t table_size = 0)
      : VectorBiTable<StateId, T, FP>(fp, table_size) {}

  StateId FindState(const StateTuple &tuple) { return FindId(tuple); }
  const StateTuple &Tuple(StateId s) const { return FindEntry(s); }
};


// An implementation using a vector and a compact hash table. The
// selecting functor S returns true for tuples to be hashed in the
// vector.  The fingerprinting functor FP returns a unique fingerprint
// for each tuple to be hashed in the vector (these need to be
// suitable for indexing in a vector).  The hash functor H is used when
// hashing tuple into the compact hash table.
template <class T, class S, class FP, class H>
class VectorHashStateTable
    : public VectorHashBiTable<typename T::StateId, T, S, FP, H> {
 public:
  typedef T StateTuple;
  typedef typename StateTuple::StateId StateId;
  using VectorHashBiTable<StateId, T, S, FP, H>::FindId;
  using VectorHashBiTable<StateId, T, S, FP, H>::FindEntry;
  using VectorHashBiTable<StateId, T, S, FP, H>::Size;
  using VectorHashBiTable<StateId, T, S, FP, H>::Selector;
  using VectorHashBiTable<StateId, T, S, FP, H>::Fingerprint;
  using VectorHashBiTable<StateId, T, S, FP, H>::Hash;

  VectorHashStateTable(S *s, FP *fp, H *h,
                       size_t vector_size = 0,
                       size_t tuple_size = 0)
      : VectorHashBiTable<StateId, T, S, FP, H>(
          s, fp, h, vector_size, tuple_size) {}

  StateId FindState(const StateTuple &tuple) { return FindId(tuple); }
  const StateTuple &Tuple(StateId s) const { return FindEntry(s); }
};


// An implementation using a hash map for the tuple to state ID
// mapping. This version permits erasing of states.  The state tuple T
// must have == defined and its default constructor must produce a
// tuple that will never be seen. F is the hash function.
template <class T, class F>
class ErasableStateTable : public ErasableBiTable<typename T::StateId, T, F> {
 public:
  typedef T StateTuple;
  typedef typename StateTuple::StateId StateId;
  using ErasableBiTable<StateId, T, F>::FindId;
  using ErasableBiTable<StateId, T, F>::FindEntry;
  using ErasableBiTable<StateId, T, F>::Size;
  using ErasableBiTable<StateId, T, F>::Erase;

  ErasableStateTable() : ErasableBiTable<StateId, T, F>() {}
  StateId FindState(const StateTuple &tuple) { return FindId(tuple); }
  const StateTuple &Tuple(StateId s) const { return FindEntry(s); }
};

//
// COMPOSITION STATE TUPLES AND TABLES
//
// The composition state table has the form:
//
// template <class A, class F>
// class ComposeStateTable {
//  public:
//   typedef A Arc;
//   typedef F FilterState;
//   typedef typename A::StateId StateId;
//   typedef ComposeStateTuple<StateId> StateTuple;
//
//   // Required constructors.
//   ComposeStateTable(const Fst<Arc> &fst1, const Fst<Arc> &fst2);
//   ComposeStateTable(const ComposeStateTable<A, F> &table);
//   // Lookup state ID by tuple. If it doesn't exist, then add it.
//   StateId FindState(const StateTuple &);
//   // Lookup state tuple by state ID.
//   const StateTuple<StateId> &Tuple(StateId) const;
//   // # of stored tuples.
//   StateId Size() const;
//   // Return true if error encountered
//   bool Error() const;
// };

// Represents the composition state.
//
// template <class S, class F>
// class StateTuple {
//  public:
//   typedef S StateId;
//   typedef F FilterState;
//   // Required constructors.
//   StateTuple();
//   StateTuple(StateId s1, StateId s2, const FilterState &f);
//   StateId StateId1() const;
//   StateId StateId2() const;
//   FilterState GetFilterState() const;
//   pair<StateId, StateId> StatePair() const;
//   size_t Hash() const;
//   friend bool operator==(const StateTuple& x, const StateTuple &y);
// }
//
template <typename S, typename F>
class DefaultComposeStateTuple {
 public:
  typedef S StateId;
  typedef F FilterState;

  DefaultComposeStateTuple()
      : state_pair_(kNoStateId, kNoStateId),
        filter_state_(FilterState::NoState()) {}

  DefaultComposeStateTuple(StateId s1, StateId s2, const FilterState &f)
      : state_pair_(s1, s2), filter_state_(f) {}

  StateId StateId1() const { return state_pair_.first; }

  StateId StateId2() const { return state_pair_.second; }

  FilterState GetFilterState() const { return filter_state_; }

  const pair<StateId, StateId>& StatePair() const {
    return state_pair_;
  }

  friend bool operator==(const DefaultComposeStateTuple& x,
                         const DefaultComposeStateTuple& y) {
    return (&x == &y) || (x.state_pair_ == y.state_pair_ &&
                          x.filter_state_ == y.filter_state_);
  }

  size_t Hash() const {
    return StateId1() + StateId2() * 7853 + GetFilterState().Hash() * 7867;
  }

 private:
  pair<StateId, StateId> state_pair_;
  FilterState filter_state_;  // State of composition filter
};

// Hashing of composition state tuples.
template <typename T>
class ComposeHash {
 public:
  size_t operator()(const T& t) const {
    return t.Hash();
  }
};

// A HashStateTable over composition tuples.
template <typename A,
          typename FS,
          typename T = DefaultComposeStateTuple<typename A::StateId, FS>,
          typename H = CompactHashStateTable<T, ComposeHash<T> > >
class GenericComposeStateTable : public H {
 public:
  typedef A Arc;
  typedef FS FilterState;
  typedef typename A::StateId StateId;
  typedef T StateTuple;

  GenericComposeStateTable(const Fst<A> &fst1, const Fst<A> &fst2) {}

  // Reserves space for 'table_size' elements.
  GenericComposeStateTable(const Fst<A> &fst1, const Fst<A> &fst2,
                           size_t table_size) : H(table_size) {}

  bool Error() const { return false; }

 private:
  void operator=(
      const GenericComposeStateTable<A, FS, T, H> &table);  // disallow
};


//  Fingerprint for general composition tuples.
template <typename T>
class ComposeFingerprint {
 public:
  typedef T StateTuple;
  typedef typename StateTuple::StateId StateId;

  // Required but suboptimal constructor.
  ComposeFingerprint() : mult1_(8192), mult2_(8192) {
    LOG(WARNING) << "TupleFingerprint: # of FST states should be provided.";
  }

  // Constructor is provided the sizes of the input FSTs
  ComposeFingerprint(StateId nstates1, StateId nstates2)
      : mult1_(nstates1), mult2_(nstates1 * nstates2) { }

  size_t operator()(const StateTuple &tuple) {
    return tuple.StateId1() + tuple.StateId2() * mult1_ +
        tuple.GetFilterState().Hash() * mult2_;
  }

 private:
  ssize_t mult1_;
  ssize_t mult2_;
};


// Useful when the first composition state determines the tuple.
template <typename T>
class ComposeState1Fingerprint {
 public:
  typedef T StateTuple;

  size_t operator()(const StateTuple &tuple) { return tuple.StateId1(); }
};


// Useful when the second composition state determines the tuple.
template  <typename T>
class ComposeState2Fingerprint {
 public:
  typedef T StateTuple;

  size_t operator()(const StateTuple &tuple) { return tuple.StateId2(); }
};


// A VectorStateTable over composition tuples.  This can be used when
// the product of number of states in FST1 and FST2 (and the
// composition filter state hash) is manageable. If the FSTs are not
// expanded Fsts, they will first have their states counted.
template  <typename A, typename T>
class ProductComposeStateTable
    : public VectorStateTable<T, ComposeFingerprint<T> > {
 public:
  typedef A Arc;
  typedef typename A::StateId StateId;
  typedef T StateTuple;
  typedef VectorStateTable<
      StateTuple,
      ComposeFingerprint<StateTuple> > StateTable;

  // Reserves space for 'table_size' elements.
  ProductComposeStateTable(const Fst<A> &fst1, const Fst<A> &fst2,
                           size_t table_size = 0)
      : StateTable(
          new ComposeFingerprint<StateTuple>(CountStates(fst1),
                                                         CountStates(fst2)),
                   table_size) {}

  ProductComposeStateTable(const ProductComposeStateTable<A, T> &table)
      : StateTable(
          new ComposeFingerprint<StateTuple>(table.Fingerprint())) {
  }

  bool Error() const { return false; }

 private:
  void operator=(
      const ProductComposeStateTable<A, T> &table);  // disallow
};

// A VectorStateTable over composition tuples.  This can be used when
// FST1 is a string (satisfies kStringProperties) and FST2 is
// epsilon-free and deterministic. It should be used with a
// composition filter that creates at most one filter state per tuple
// under these conditions (e.g. SequenceComposeFilter or
// MatchComposeFilter).
template <typename A, typename T>
class StringDetComposeStateTable
    : public VectorStateTable<T, ComposeState1Fingerprint<T> > {
 public:
  typedef A Arc;
  typedef typename A::StateId StateId;
  typedef T StateTuple;
  typedef VectorStateTable<StateTuple,
                           ComposeState1Fingerprint<StateTuple> > StateTable;

  StringDetComposeStateTable(const Fst<A> &fst1, const Fst<A> &fst2)
      : error_(false) {
    uint64 props1 = kString;
    uint64 props2 = kIDeterministic | kNoIEpsilons;
    if (fst1.Properties(props1, true) != props1 ||
        fst2.Properties(props2, true) != props2) {
      FSTERROR() << "StringDetComposeStateTable: fst1 not a string or"
                 << " fst2 not input deterministic and epsilon-free";
      error_ = true;
    }
  }

  StringDetComposeStateTable(const StringDetComposeStateTable<A, T> &table)
      : StateTable(table), error_(table.error_) {}

  bool Error() const { return error_; }

 private:
  bool error_;

  void operator=(
      const StringDetComposeStateTable<A, T> &table);  // disallow
};


// A VectorStateTable over composition tuples.  This can be used when
// FST2 is a string (satisfies kStringProperties) and FST1 is
// epsilon-free and deterministic. It should be used with a
// composition filter that creates at most one filter state per tuple
// under these conditions (e.g. SequenceComposeFilter or
// MatchComposeFilter).
template <typename A, typename T>
class DetStringComposeStateTable
    : public VectorStateTable<T, ComposeState2Fingerprint<T> > {
 public:
  typedef A Arc;
  typedef typename A::StateId StateId;
  typedef T StateTuple;
  typedef VectorStateTable<StateTuple,
                           ComposeState2Fingerprint<StateTuple> > StateTable;

  DetStringComposeStateTable(const Fst<A> &fst1, const Fst<A> &fst2)
      :error_(false) {
    uint64 props1 = kODeterministic | kNoOEpsilons;
    uint64 props2 = kString;
    if (fst1.Properties(props1, true) != props1 ||
        fst2.Properties(props2, true) != props2) {
      FSTERROR() << "StringDetComposeStateTable: fst2 not a string or"
                 << " fst1 not output deterministic and epsilon-free";
      error_ = true;
    }
  }

  DetStringComposeStateTable(const DetStringComposeStateTable<A, T> &table)
      : StateTable(table), error_(table.error_) {}

  bool Error() const { return error_; }

 private:
  bool error_;

  void operator=(const DetStringComposeStateTable<A, T> &table);  // disallow
};


// An ErasableStateTable over composition tuples. The Erase(StateId) method
// can be called if the user either is sure that composition will never return
// to that tuple or doesn't care that if it does, it is assigned a new
// state ID.
template <typename A, typename T>
class ErasableComposeStateTable
    : public ErasableStateTable<T, ComposeHash<T> > {
 public:
  typedef A Arc;
  typedef typename A::StateId StateId;
  typedef T StateTuple;

  ErasableComposeStateTable(const Fst<A> &fst1, const Fst<A> &fst2) {}

  bool Error() const { return false; }

 private:
  void operator=(const ErasableComposeStateTable<A, T> &table);  // disallow
};

}  // namespace fst

#endif  // FST_LIB_STATE_TABLE_H__
