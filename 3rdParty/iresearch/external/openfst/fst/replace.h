// replace.h

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
// Author: johans@google.com (Johan Schalkwyk)
//
// \file
// Functions and classes for the recursive replacement of Fsts.
//

#ifndef FST_LIB_REPLACE_H__
#define FST_LIB_REPLACE_H__

#include <unordered_map>
using std::unordered_map;
using std::unordered_multimap;
#include <set>
#include <string>
#include <utility>
using std::pair; using std::make_pair;
#include <vector>
using std::vector;

#include <fst/cache.h>
#include <fst/fst-decl.h>  // For optional argument declarations
#include <fst/expanded-fst.h>
#include <fst/fst.h>
#include <fst/matcher.h>
#include <fst/replace-util.h>
#include <fst/state-table.h>
#include <fst/test-properties.h>

namespace fst {

//
// REPLACE STATE TUPLES AND TABLES
//
// The replace state table has the form
//
// template <class A, class P>
// class ReplaceStateTable {
//  public:
//   typedef A Arc;
//   typedef P PrefixId;
//   typedef typename A::StateId StateId;
//   typedef ReplaceStateTuple<StateId, PrefixId> StateTuple;
//   typedef typename A::Label Label;
//   typedef ReplaceStackPrefix<Label, StateId> StackPrefix;
//
//   // Required constuctor
//   ReplaceStateTable(const vector<pair<Label, const Fst<A>*> > &fst_tuples,
//                     Label root);
//
//   // Required copy constructor that does not copy state
//   ReplaceStateTable(const ReplaceStateTable<A,P> &table);
//
//   // Lookup state ID by tuple. If it doesn't exist, then add it.
//   StateId FindState(const StateTuple &tuple);
//
//   // Lookup state tuple by ID.
//   const StateTuple &Tuple(StateId id) const;
//
//   // Lookup prefix ID by stack prefix. If it doesn't exist, then add it.
//   PrefixId FindPrefixId(const StackPrefix &stack_prefix);
//
//  // Look stack prefix by ID.
//  const StackPrefix &GetStackPrefix(PrefixId id) const;
// };

//
// Replace State Tuples
//

// \struct ReplaceStateTuple
// \brief Tuple of information that uniquely defines a state in replace
template <class S, class P>
struct ReplaceStateTuple {
  typedef S StateId;
  typedef P PrefixId;

  ReplaceStateTuple()
      : prefix_id(-1), fst_id(kNoStateId), fst_state(kNoStateId) {}

  ReplaceStateTuple(PrefixId p, StateId f, StateId s)
      : prefix_id(p), fst_id(f), fst_state(s) {}

  PrefixId prefix_id;  // index in prefix table
  StateId fst_id;      // current fst being walked
  StateId fst_state;   // current state in fst being walked, not to be
                       // confused with the state_id of the combined fst
};

// Equality of replace state tuples.
template <class S, class P>
inline bool operator==(const ReplaceStateTuple<S, P>& x,
                       const ReplaceStateTuple<S, P>& y) {
  return x.prefix_id == y.prefix_id &&
      x.fst_id == y.fst_id &&
      x.fst_state == y.fst_state;
}

// \class ReplaceRootSelector
// Functor returning true for tuples corresponding to states in the root FST
template <class S, class P>
class ReplaceRootSelector {
 public:
  bool operator()(const ReplaceStateTuple<S, P> &tuple) const {
    return tuple.prefix_id == 0;
  }
};

// \class ReplaceFingerprint
// Fingerprint for general replace state tuples.
template <class S, class P>
class ReplaceFingerprint {
 public:
  explicit ReplaceFingerprint(const vector<uint64> *size_array)
      : cumulative_size_array_(size_array) {}

  uint64 operator()(const ReplaceStateTuple<S, P> &tuple) const {
    return tuple.prefix_id * (cumulative_size_array_->back()) +
        cumulative_size_array_->at(tuple.fst_id - 1) +
        tuple.fst_state;
  }

 private:
  const vector<uint64> *cumulative_size_array_;
};

// \class ReplaceFstStateFingerprint
// Useful when the fst_state uniquely define the tuple.
template <class S, class P>
class ReplaceFstStateFingerprint {
 public:
  uint64 operator()(const ReplaceStateTuple<S, P>& tuple) const {
    return tuple.fst_state;
  }
};

// \class ReplaceHash
// A generic hash function for replace state tuples.
template <typename S, typename P>
class ReplaceHash {
 public:
  size_t operator()(const ReplaceStateTuple<S, P>& t) const {
    return t.prefix_id + t.fst_id * kPrime0 + t.fst_state * kPrime1;
  }
 private:
  static const size_t kPrime0;
  static const size_t kPrime1;
};

template <typename S, typename P>
const size_t ReplaceHash<S, P>::kPrime0 = 7853;

template <typename S, typename P>
const size_t ReplaceHash<S, P>::kPrime1 = 7867;


//
// Replace Stack Prefix
//

// \class ReplaceStackPrefix
// \brief Container for stack prefix.
template <class L, class S>
class ReplaceStackPrefix {
 public:
  typedef L Label;
  typedef S StateId;

  // \class PrefixTuple
  // \brief Tuple of fst_id and destination state (entry in stack prefix)
  struct PrefixTuple {
    PrefixTuple(Label f, StateId s) : fst_id(f), nextstate(s) {}
    PrefixTuple() : fst_id(kNoLabel), nextstate(kNoStateId) {}

    Label   fst_id;
    StateId nextstate;
  };

  ReplaceStackPrefix() {}

  // copy constructor
  ReplaceStackPrefix(const ReplaceStackPrefix& x) :
      prefix_(x.prefix_) {
  }

  void Push(StateId fst_id, StateId nextstate) {
    prefix_.push_back(PrefixTuple(fst_id, nextstate));
  }

  void Pop() {
    prefix_.pop_back();
  }

  const PrefixTuple& Top() const {
    return prefix_[prefix_.size()-1];
  }

  size_t Depth() const {
    return prefix_.size();
  }

 public:
  vector<PrefixTuple> prefix_;
};

// Equality stack prefix classes
template <class L, class S>
inline bool operator==(const ReplaceStackPrefix<L, S>& x,
                const ReplaceStackPrefix<L, S>& y) {
  if (x.prefix_.size() != y.prefix_.size()) return false;
  for (size_t i = 0; i < x.prefix_.size(); ++i) {
    if (x.prefix_[i].fst_id    != y.prefix_[i].fst_id ||
        x.prefix_[i].nextstate != y.prefix_[i].nextstate) return false;
  }
  return true;
}

//
// \class ReplaceStackPrefixHash
// \brief Hash function for stack prefix to prefix id
template <class L, class S>
class ReplaceStackPrefixHash {
 public:
  size_t operator()(const ReplaceStackPrefix<L, S>& x) const {
    size_t sum = 0;
    for (size_t i = 0; i < x.prefix_.size(); ++i) {
      sum += x.prefix_[i].fst_id + x.prefix_[i].nextstate*kPrime0;
    }
    return sum;
  }

 private:
  static const size_t kPrime0;
};

template <class L, class S>
const size_t ReplaceStackPrefixHash<L, S>::kPrime0 = 7853;

//
// Replace State Tables
//

// \class VectorHashReplaceStateTable
// A two-level state table for replace.
// Warning: calls CountStates to compute the number of states of each
// component Fst.
template <class A, class P = ssize_t>
class VectorHashReplaceStateTable {
 public:
  typedef A Arc;
  typedef typename A::StateId StateId;
  typedef typename A::Label Label;
  typedef P PrefixId;
  typedef ReplaceStateTuple<StateId, P> StateTuple;
  typedef VectorHashStateTable<ReplaceStateTuple<StateId, P>,
                               ReplaceRootSelector<StateId, P>,
                               ReplaceFstStateFingerprint<StateId, P>,
                               ReplaceFingerprint<StateId, P> > StateTable;
  typedef ReplaceStackPrefix<Label, StateId> StackPrefix;
  typedef CompactHashBiTable<
    PrefixId, StackPrefix,
    ReplaceStackPrefixHash<Label, StateId> > StackPrefixTable;

  VectorHashReplaceStateTable(
      const vector<pair<Label, const Fst<A>*> > &fst_tuples,
      Label root) : root_size_(0) {
    cumulative_size_array_.push_back(0);
    for (size_t i = 0; i < fst_tuples.size(); ++i) {
      if (fst_tuples[i].first == root) {
        root_size_ = CountStates(*(fst_tuples[i].second));
        cumulative_size_array_.push_back(cumulative_size_array_.back());
      } else {
        cumulative_size_array_.push_back(cumulative_size_array_.back() +
                                         CountStates(*(fst_tuples[i].second)));
      }
    }
    state_table_ = new StateTable(
        new ReplaceRootSelector<StateId, P>,
        new ReplaceFstStateFingerprint<StateId, P>,
        new ReplaceFingerprint<StateId, P>(&cumulative_size_array_),
        root_size_,
        root_size_ + cumulative_size_array_.back());
  }

  VectorHashReplaceStateTable(const VectorHashReplaceStateTable<A, P> &table)
      : root_size_(table.root_size_),
        cumulative_size_array_(table.cumulative_size_array_),
        prefix_table_(table.prefix_table_) {
    state_table_ = new StateTable(
        new ReplaceRootSelector<StateId, P>,
        new ReplaceFstStateFingerprint<StateId, P>,
        new ReplaceFingerprint<StateId, P>(&cumulative_size_array_),
        root_size_,
        root_size_ + cumulative_size_array_.back());
  }

  ~VectorHashReplaceStateTable() {
    delete state_table_;
  }

  StateId FindState(const StateTuple &tuple) {
    return state_table_->FindState(tuple);
  }

  const StateTuple &Tuple(StateId id) const {
    return state_table_->Tuple(id);
  }

  PrefixId FindPrefixId(const StackPrefix &prefix) {
    return prefix_table_.FindId(prefix);
  }

  const StackPrefix &GetStackPrefix(PrefixId id) const {
    return prefix_table_.FindEntry(id);
  }

 private:
  StateId root_size_;
  vector<uint64> cumulative_size_array_;
  StateTable *state_table_;
  StackPrefixTable prefix_table_;
};


// \class DefaultReplaceStateTable
// Default replace state table
template <class A, class P /* = size_t */>
class DefaultReplaceStateTable : public CompactHashStateTable<
  ReplaceStateTuple<typename A::StateId, P>,
  ReplaceHash<typename A::StateId, P> > {
 public:
  typedef A Arc;
  typedef typename A::StateId StateId;
  typedef typename A::Label Label;
  typedef P PrefixId;
  typedef ReplaceStateTuple<StateId, P> StateTuple;
  typedef CompactHashStateTable<StateTuple,
                                ReplaceHash<StateId, PrefixId> > StateTable;
  typedef ReplaceStackPrefix<Label, StateId> StackPrefix;
  typedef CompactHashBiTable<
    PrefixId, StackPrefix,
    ReplaceStackPrefixHash<Label, StateId> > StackPrefixTable;

  using StateTable::FindState;
  using StateTable::Tuple;

  DefaultReplaceStateTable(
      const vector<pair<Label, const Fst<A>*> > &fst_tuples,
      Label root) {}

  DefaultReplaceStateTable(const DefaultReplaceStateTable<A, P> &table)
      : StateTable(), prefix_table_(table.prefix_table_) {}

  PrefixId FindPrefixId(const StackPrefix &prefix) {
    return prefix_table_.FindId(prefix);
  }

  const StackPrefix &GetStackPrefix(PrefixId id) const {
    return prefix_table_.FindEntry(id);
  }

 private:
  StackPrefixTable prefix_table_;
};

//
// REPLACE FST CLASS
//

// By default ReplaceFst will copy the input label of the 'replace arc'.
// The call_label_type and return_label_type options specify how to manage
// the labels of the call arc and the return arc of the replace FST
template <class A, class T = DefaultReplaceStateTable<A>,
          class C = DefaultCacheStore<A> >
struct ReplaceFstOptions : CacheImplOptions<C> {
  int64 root;    // root rule for expansion
  ReplaceLabelType call_label_type;  // how to label call arc
  ReplaceLabelType return_label_type;  // how to label return arc
  int64 call_output_label;  // specifies output label to put on call arc
                            // if kNoLabel, use existing label on call arc
                            // if 0, epsilon
                            // otherwise, use this field as the output label
  int64 return_label;  // specifies label to put on return arc
  bool  take_ownership;  // take ownership of input Fst(s)
  T*    state_table;

  ReplaceFstOptions(const CacheImplOptions<C> &opts, int64 r)
      : CacheImplOptions<C>(opts),
        root(r),
        call_label_type(REPLACE_LABEL_INPUT),
        return_label_type(REPLACE_LABEL_NEITHER),
        call_output_label(kNoLabel),
        return_label(0),
        take_ownership(false),
        state_table(0) {}

  ReplaceFstOptions(const CacheOptions &opts, int64 r)
      : CacheImplOptions<C>(opts),
        root(r),
        call_label_type(REPLACE_LABEL_INPUT),
        return_label_type(REPLACE_LABEL_NEITHER),
        call_output_label(kNoLabel),
        return_label(0),
        take_ownership(false),
        state_table(0) {}

  explicit ReplaceFstOptions(const fst::ReplaceUtilOptions &opts)
      : root(opts.root),
        call_label_type(opts.call_label_type),
        return_label_type(opts.return_label_type),
        call_output_label(kNoLabel),
        return_label(opts.return_label),
        take_ownership(false),
        state_table(0) {}

  explicit ReplaceFstOptions(int64 r)
      : root(r),
        call_label_type(REPLACE_LABEL_INPUT),
        return_label_type(REPLACE_LABEL_NEITHER),
        call_output_label(kNoLabel),
        return_label(0),
        take_ownership(false),
        state_table(0) {}

  ReplaceFstOptions(int64 r, ReplaceLabelType call_label_type,
                    ReplaceLabelType return_label_type, int64 return_label)
      : root(r),
        call_label_type(call_label_type),
        return_label_type(return_label_type),
        call_output_label(kNoLabel),
        return_label(return_label),
        take_ownership(false),
        state_table(0) {}

  ReplaceFstOptions(int64 r, ReplaceLabelType call_label_type,
                    ReplaceLabelType return_label_type, int64 call_output_label,
                    int64 return_label)
      : root(r),
        call_label_type(call_label_type),
        return_label_type(return_label_type),
        call_output_label(call_output_label),
        return_label(return_label),
        take_ownership(false),
        state_table(0) {}

  ReplaceFstOptions(int64 r, bool epsilon_replace_arc)  // b/w compatibility
      : root(r),
        call_label_type((epsilon_replace_arc) ? REPLACE_LABEL_NEITHER :
                        REPLACE_LABEL_INPUT),
        return_label_type(REPLACE_LABEL_NEITHER),
        call_output_label((epsilon_replace_arc) ? 0 : kNoLabel),
        return_label(0),
        take_ownership(false),
        state_table(0) {}

  ReplaceFstOptions()
      : root(kNoLabel),
        call_label_type(REPLACE_LABEL_INPUT),
        return_label_type(REPLACE_LABEL_NEITHER),
        call_output_label(kNoLabel),
        return_label(0),
        take_ownership(false),
        state_table(0) {}
};

// Forward declaration
template <class A, class T, class C> class ReplaceFstMatcher;

// \class ReplaceFstImpl
// \brief Implementation class for replace class Fst
//
// The replace implementation class supports a dynamic
// expansion of a recursive transition network represented as Fst
// with dynamic replacable arcs.
//
template <class A, class T, class C>
class ReplaceFstImpl : public CacheBaseImpl<typename C::State, C> {
  friend class ReplaceFstMatcher<A, T, C>;

 public:
  typedef A Arc;
  typedef typename A::Label   Label;
  typedef typename A::Weight  Weight;
  typedef typename A::StateId StateId;
  typedef typename C::State State;
  typedef CacheBaseImpl<State, C> CImpl;
  typedef unordered_map<Label, Label> NonTerminalHash;
  typedef T StateTable;
  typedef typename T::PrefixId PrefixId;
  typedef ReplaceStateTuple<StateId, PrefixId> StateTuple;
  typedef ReplaceStackPrefix<Label, StateId> StackPrefix;

  using FstImpl<A>::SetType;
  using FstImpl<A>::SetProperties;
  using FstImpl<A>::WriteHeader;
  using FstImpl<A>::SetInputSymbols;
  using FstImpl<A>::SetOutputSymbols;
  using FstImpl<A>::InputSymbols;
  using FstImpl<A>::OutputSymbols;

  using CImpl::PushArc;
  using CImpl::HasArcs;
  using CImpl::HasFinal;
  using CImpl::HasStart;
  using CImpl::SetArcs;
  using CImpl::SetFinal;
  using CImpl::SetStart;

  // constructor for replace class implementation.
  // \param fst_tuples array of label/fst tuples, one for each non-terminal
  ReplaceFstImpl(const vector< pair<Label, const Fst<A>* > >& fst_tuples,
                 const ReplaceFstOptions<A, T, C> &opts)
      : CImpl(opts),
        call_label_type_(opts.call_label_type),
        return_label_type_(opts.return_label_type),
        call_output_label_(opts.call_output_label),
        return_label_(opts.return_label),
        state_table_(opts.state_table ? opts.state_table :
                     new StateTable(fst_tuples, opts.root)) {
    SetType("replace");

    // if the label is epsilon, then all REPLACE_LABEL_* options equivalent.
    // Set the label_type to NEITHER for ease of setting properties later
    if (call_output_label_ == 0)
      call_label_type_ = REPLACE_LABEL_NEITHER;
    if (return_label_ == 0)
      return_label_type_ = REPLACE_LABEL_NEITHER;

    if (fst_tuples.size() > 0) {
      SetInputSymbols(fst_tuples[0].second->InputSymbols());
      SetOutputSymbols(fst_tuples[0].second->OutputSymbols());
    }

    bool all_negative = true;  // all nonterminals are negative?
    bool dense_range = true;   // all nonterminals are positive
                               // and form a dense range containing 1?
    for (size_t i = 0; i < fst_tuples.size(); ++i) {
      Label nonterminal = fst_tuples[i].first;
      if (nonterminal >= 0)
        all_negative = false;
      if (nonterminal > fst_tuples.size() || nonterminal <= 0)
        dense_range = false;
    }

    vector<uint64> inprops;
    bool all_ilabel_sorted = true;
    bool all_olabel_sorted = true;
    bool all_non_empty = true;
    fst_array_.push_back(0);
    for (size_t i = 0; i < fst_tuples.size(); ++i) {
      Label label = fst_tuples[i].first;
      const Fst<A> *fst = fst_tuples[i].second;
      nonterminal_hash_[label] = fst_array_.size();
      nonterminal_set_.insert(label);
      fst_array_.push_back(opts.take_ownership ? fst : fst->Copy());
      if (fst->Start() == kNoStateId)
        all_non_empty = false;
      if (!fst->Properties(kILabelSorted, false))
        all_ilabel_sorted = false;
      if (!fst->Properties(kOLabelSorted, false))
        all_olabel_sorted = false;
      inprops.push_back(fst->Properties(kCopyProperties, false));
      if (i) {
        if (!CompatSymbols(InputSymbols(), fst->InputSymbols())) {
          FSTERROR() << "ReplaceFstImpl: input symbols of Fst " << i
                     << " does not match input symbols of base Fst (0'th fst)";
          SetProperties(kError, kError);
        }
        if (!CompatSymbols(OutputSymbols(), fst->OutputSymbols())) {
          FSTERROR() << "ReplaceFstImpl: output symbols of Fst " << i
                     << " does not match output symbols of base Fst "
                     << "(0'th fst)";
          SetProperties(kError, kError);
        }
      }
    }
    Label nonterminal = nonterminal_hash_[opts.root];
    if ((nonterminal == 0) && (fst_array_.size() > 1)) {
      FSTERROR() << "ReplaceFstImpl: no Fst corresponding to root label '"
                 << opts.root << "' in the input tuple vector";
      SetProperties(kError, kError);
    }
    root_ = (nonterminal > 0) ? nonterminal : 1;

    SetProperties(ReplaceProperties(inprops, root_ - 1,
                                    EpsilonOnInput(call_label_type_),
                                    EpsilonOnInput(return_label_type_),
                                    ReplaceTransducer(), all_non_empty));
    // We assume that all terminals are positive.  The resulting
    // ReplaceFst is known to be kILabelSorted when: (1) all sub-FSTs are
    // kILabelSorted, (2) the input label of the return arc is epsilon,
    // and (3) one of the 3 following conditions is satisfied:
    //  1. the input label of the call arc is not epsilon
    //  2. all non-terminals are negative, or
    //  3. all non-terninals are positive and form a dense range containing 1.
    if (all_ilabel_sorted && EpsilonOnInput(return_label_type_) &&
        (!EpsilonOnInput(call_label_type_) || all_negative || dense_range)) {
      SetProperties(kILabelSorted, kILabelSorted);
    }
    // Similarly, the resulting ReplaceFst is known to be
    // kOLabelSorted when: (1) all sub-FSTs are kOLabelSorted, (2) the output
    // label of the return arc is epsilon, and (3) one of the 3 following
    // conditions is satisfied:
    //  1. the output label of the call arc is not epsilon
    //  2. all non-terminals are negative, or
    //  3. all non-terninals are positive and form a dense range containing 1.
    if (all_olabel_sorted && EpsilonOnOutput(return_label_type_) &&
        (!EpsilonOnOutput(call_label_type_) || all_negative || dense_range))
      SetProperties(kOLabelSorted, kOLabelSorted);

    // Enable optional caching as long as sorted and all non empty.
    if (Properties(kILabelSorted | kOLabelSorted) && all_non_empty)
      always_cache_ = false;
    else
      always_cache_ = true;
    VLOG(2) << "ReplaceFstImpl::ReplaceFstImpl: always_cache = "
            << (always_cache_ ? "true" : "false");
  }

  ReplaceFstImpl(const ReplaceFstImpl& impl)
      : CImpl(impl),
        call_label_type_(impl.call_label_type_),
        return_label_type_(impl.return_label_type_),
        call_output_label_(impl.call_output_label_),
        return_label_(impl.return_label_),
        always_cache_(impl.always_cache_),
        state_table_(new StateTable(*(impl.state_table_))),
        nonterminal_set_(impl.nonterminal_set_),
        nonterminal_hash_(impl.nonterminal_hash_),
        root_(impl.root_) {
    SetType("replace");
    SetProperties(impl.Properties(), kCopyProperties);
    SetInputSymbols(impl.InputSymbols());
    SetOutputSymbols(impl.OutputSymbols());
    fst_array_.reserve(impl.fst_array_.size());
    fst_array_.push_back(0);
    for (size_t i = 1; i < impl.fst_array_.size(); ++i) {
      fst_array_.push_back(impl.fst_array_[i]->Copy(true));
    }
  }

  ~ReplaceFstImpl() {
    delete state_table_;
    for (size_t i = 1; i < fst_array_.size(); ++i) {
      delete fst_array_[i];
    }
  }

  // Computes the dependency graph of the replace class and returns
  // true if the dependencies are cyclic. Cyclic dependencies will result
  // in an un-expandable replace fst.
  bool CyclicDependencies() const {
    ReplaceUtil<A> replace_util(fst_array_, nonterminal_hash_,
                                fst::ReplaceUtilOptions(root_));
    return replace_util.CyclicDependencies();
  }

  // Returns or computes start state of replace fst.
  StateId Start() {
    if (!HasStart()) {
      if (fst_array_.size() == 1) {      // no fsts defined for replace
        SetStart(kNoStateId);
        return kNoStateId;
      } else {
        const Fst<A>* fst = fst_array_[root_];
        StateId fst_start = fst->Start();
        if (fst_start == kNoStateId)  // root Fst is empty
          return kNoStateId;

        PrefixId prefix = GetPrefixId(StackPrefix());
        StateId start = state_table_->FindState(
            StateTuple(prefix, root_, fst_start));
        SetStart(start);
        return start;
      }
    } else {
      return CImpl::Start();
    }
  }

  // Returns final weight of state (Weight::Zero() means state is not final).
  Weight Final(StateId s) {
    if (HasFinal(s)) {
      return CImpl::Final(s);
    } else {
      const StateTuple& tuple  = state_table_->Tuple(s);
      Weight final = Weight::Zero();

      if (tuple.prefix_id == 0) {
        const Fst<A>* fst = fst_array_[tuple.fst_id];
        StateId fst_state = tuple.fst_state;
        final = fst->Final(fst_state);
      }

      if (always_cache_ || HasArcs(s))
        SetFinal(s, final);
      return final;
    }
  }

  size_t NumArcs(StateId s) {
    if (HasArcs(s)) {  // If state cached, use the cached value.
      return CImpl::NumArcs(s);
    } else if (always_cache_) {  // If always caching, expand and cache state.
      Expand(s);
      return CImpl::NumArcs(s);
    } else {  // Otherwise compute the number of arcs without expanding.
      StateTuple tuple  = state_table_->Tuple(s);
      if (tuple.fst_state == kNoStateId)
        return 0;

      const Fst<A>* fst = fst_array_[tuple.fst_id];
      size_t num_arcs = fst->NumArcs(tuple.fst_state);
      if (ComputeFinalArc(tuple, 0))
        num_arcs++;

      return num_arcs;
    }
  }

  // Returns whether a given label is a non terminal
  bool IsNonTerminal(Label l) const {
    if (l < *nonterminal_set_.begin() || l > *nonterminal_set_.rbegin())
      return false;
    // TODO(allauzen): be smarter and take advantage of
    // all_dense or all_negative.
    // Use also in ComputeArc, this would require changes to replace
    // so that recursing into an empty fst lead to a non co-accessible
    // state instead of deleting the arc as done currently.
    // Current use correct, since i/olabel sorted iff all_non_empty.
    typename NonTerminalHash::const_iterator it =
        nonterminal_hash_.find(l);
    return it != nonterminal_hash_.end();
  }

  size_t NumInputEpsilons(StateId s) {
    if (HasArcs(s)) {
      // If state cached, use the cached value.
      return CImpl::NumInputEpsilons(s);
    } else if (always_cache_ || !Properties(kILabelSorted)) {
      // If always caching or if the number of input epsilons is too expensive
      // to compute without caching (i.e. not ilabel sorted),
      // then expand and cache state.
      Expand(s);
      return CImpl::NumInputEpsilons(s);
    } else {
      // Otherwise, compute the number of input epsilons without caching.
      StateTuple tuple  = state_table_->Tuple(s);
      if (tuple.fst_state == kNoStateId)
        return 0;
      const Fst<A>* fst = fst_array_[tuple.fst_id];
      size_t num  = 0;
      if (!EpsilonOnInput(call_label_type_)) {
        // If EpsilonOnInput(c) is false, all input epsilon arcs
        // are also input epsilons arcs in the underlying machine.
        num = fst->NumInputEpsilons(tuple.fst_state);
      } else {
        // Otherwise, one need to consider that all non-terminal arcs
        // in the underlying machine also become input epsilon arc.
        ArcIterator<Fst<A> > aiter(*fst, tuple.fst_state);
        for (; !aiter.Done() &&
                 ((aiter.Value().ilabel == 0) ||
                  IsNonTerminal(aiter.Value().olabel));
             aiter.Next())
          ++num;
      }
      if (EpsilonOnInput(return_label_type_) && ComputeFinalArc(tuple, 0))
        num++;
      return num;
    }
  }

  size_t NumOutputEpsilons(StateId s) {
    if (HasArcs(s)) {
      // If state cached, use the cached value.
      return CImpl::NumOutputEpsilons(s);
    } else if (always_cache_ || !Properties(kOLabelSorted)) {
      // If always caching or if the number of output epsilons is too expensive
      // to compute without caching (i.e. not olabel sorted),
      // then expand and cache state.
      Expand(s);
      return CImpl::NumOutputEpsilons(s);
    } else {
      // Otherwise, compute the number of output epsilons without caching.
      StateTuple tuple  = state_table_->Tuple(s);
      if (tuple.fst_state == kNoStateId)
        return 0;
      const Fst<A>* fst = fst_array_[tuple.fst_id];
      size_t num  = 0;
      if (!EpsilonOnOutput(call_label_type_)) {
        // If EpsilonOnOutput(c) is false, all output epsilon arcs
        // are also output epsilons arcs in the underlying machine.
        num = fst->NumOutputEpsilons(tuple.fst_state);
      } else {
        // Otherwise, one need to consider that all non-terminal arcs
        // in the underlying machine also become output epsilon arc.
        ArcIterator<Fst<A> > aiter(*fst, tuple.fst_state);
        for (; !aiter.Done() &&
                 ((aiter.Value().olabel == 0) ||
                  IsNonTerminal(aiter.Value().olabel));
             aiter.Next())
          ++num;
      }
      if (EpsilonOnOutput(return_label_type_) && ComputeFinalArc(tuple, 0))
        num++;
      return num;
    }
  }

  uint64 Properties() const { return Properties(kFstProperties); }

  // Set error if found; return FST impl properties.
  uint64 Properties(uint64 mask) const {
    if (mask & kError) {
      for (size_t i = 1; i < fst_array_.size(); ++i) {
        if (fst_array_[i]->Properties(kError, false))
          SetProperties(kError, kError);
      }
    }
    return FstImpl<Arc>::Properties(mask);
  }

  // return the base arc iterator, if arcs have not been computed yet,
  // extend/recurse for new arcs.
  void InitArcIterator(StateId s, ArcIteratorData<A> *data) {
    if (!HasArcs(s))
      Expand(s);
    CImpl::InitArcIterator(s, data);
    // TODO(allauzen): Set behaviour of generic iterator
    // Warning: ArcIterator<ReplaceFst<A> >::InitCache()
    // relies on current behaviour.
  }


  // Extend current state (walk arcs one level deep)
  void Expand(StateId s) {
    StateTuple tuple = state_table_->Tuple(s);

    // If local fst is empty
    if (tuple.fst_state == kNoStateId) {
      SetArcs(s);
      return;
    }

    ArcIterator< Fst<A> > aiter(
        *(fst_array_[tuple.fst_id]), tuple.fst_state);
    Arc arc;

    // Create a final arc when needed
    if (ComputeFinalArc(tuple, &arc))
      PushArc(s, arc);

    // Expand all arcs leaving the state
    for (; !aiter.Done(); aiter.Next()) {
      if (ComputeArc(tuple, aiter.Value(), &arc))
        PushArc(s, arc);
    }

    SetArcs(s);
  }

  void Expand(StateId s, const StateTuple &tuple,
              const ArcIteratorData<A> &data) {
     // If local fst is empty
    if (tuple.fst_state == kNoStateId) {
      SetArcs(s);
      return;
    }

    ArcIterator< Fst<A> > aiter(data);
    Arc arc;

    // Create a final arc when needed
    if (ComputeFinalArc(tuple, &arc))
      AddArc(s, arc);

    // Expand all arcs leaving the state
    for (; !aiter.Done(); aiter.Next()) {
      if (ComputeArc(tuple, aiter.Value(), &arc))
        AddArc(s, arc);
    }

    SetArcs(s);
  }

  // If arcp == 0, only returns if a final arc is required, does not
  // actually compute it.
  bool ComputeFinalArc(const StateTuple &tuple, A* arcp,
                       uint32 flags = kArcValueFlags) {
    const Fst<A>* fst = fst_array_[tuple.fst_id];
    StateId fst_state = tuple.fst_state;
    if (fst_state == kNoStateId)
      return false;

    // if state is final, pop up stack
    if (fst->Final(fst_state) != Weight::Zero() && tuple.prefix_id) {
      if (arcp) {
        arcp->ilabel = (EpsilonOnInput(return_label_type_)) ? 0 : return_label_;
        arcp->olabel =
            (EpsilonOnOutput(return_label_type_)) ? 0 : return_label_;
        if (flags & kArcNextStateValue) {
          const StackPrefix& stack = state_table_->GetStackPrefix(
              tuple.prefix_id);
          PrefixId prefix_id = PopPrefix(stack);
          const typename StackPrefix::PrefixTuple& top = stack.Top();
          arcp->nextstate = state_table_->FindState(
              StateTuple(prefix_id, top.fst_id, top.nextstate));
        }
        if (flags & kArcWeightValue)
          arcp->weight = fst->Final(fst_state);
      }
      return true;
    } else {
      return false;
    }
  }

  // Compute the arc in the replace fst corresponding to a given
  // in the underlying machine. Returns false if the underlying arc
  // corresponds to no arc in the replace.
  bool ComputeArc(const StateTuple &tuple, const A &arc, A* arcp,
                  uint32 flags = kArcValueFlags) {
    if (!EpsilonOnInput(call_label_type_) &&
        (flags == (flags & (kArcILabelValue | kArcWeightValue)))) {
      *arcp = arc;
      return true;
    }

    if (arc.olabel == 0 ||
        arc.olabel < *nonterminal_set_.begin() ||
        arc.olabel > *nonterminal_set_.rbegin()) {  // expand local fst
      StateId nextstate = flags & kArcNextStateValue
          ? state_table_->FindState(
              StateTuple(tuple.prefix_id, tuple.fst_id, arc.nextstate))
          : kNoStateId;
      *arcp = A(arc.ilabel, arc.olabel, arc.weight, nextstate);
    } else {
      // check for non terminal
      typename NonTerminalHash::const_iterator it =
          nonterminal_hash_.find(arc.olabel);
      if (it != nonterminal_hash_.end()) {  // recurse into non terminal
        Label nonterminal = it->second;
        const Fst<A>* nt_fst = fst_array_[nonterminal];
        PrefixId nt_prefix = PushPrefix(
            state_table_->GetStackPrefix(tuple.prefix_id),
            tuple.fst_id, arc.nextstate);

        // if start state is valid replace, else arc is implicitly
        // deleted
        StateId nt_start = nt_fst->Start();
        if (nt_start != kNoStateId) {
          StateId nt_nextstate =  flags & kArcNextStateValue
              ? state_table_->FindState(
                  StateTuple(nt_prefix, nonterminal, nt_start))
              : kNoStateId;
          Label ilabel = (EpsilonOnInput(call_label_type_)) ? 0 : arc.ilabel;
          Label olabel = (EpsilonOnOutput(call_label_type_)) ?
              0 : ((call_output_label_ == kNoLabel) ?
                   arc.olabel : call_output_label_);
          *arcp = A(ilabel, olabel, arc.weight, nt_nextstate);
        } else {
          return false;
        }
      } else {
        StateId nextstate = flags & kArcNextStateValue
            ? state_table_->FindState(
                StateTuple(tuple.prefix_id, tuple.fst_id, arc.nextstate))
            : kNoStateId;
        *arcp = A(arc.ilabel, arc.olabel, arc.weight, nextstate);
      }
    }
    return true;
  }

  // Returns the arc iterator flags supported by this Fst.
  uint32 ArcIteratorFlags() const {
    uint32 flags = kArcValueFlags;
    if (!always_cache_)
      flags |= kArcNoCache;
    return flags;
  }

  T* GetStateTable() const {
    return state_table_;
  }

  const Fst<A>* GetFst(Label fst_id) const {
    return fst_array_[fst_id];
  }

  Label GetFstId(Label nonterminal) const {
    typename NonTerminalHash::const_iterator it =
        nonterminal_hash_.find(nonterminal);
    if (it == nonterminal_hash_.end()) {
      FSTERROR() << "ReplaceFstImpl::GetFstId: nonterminal not found: "
                 << nonterminal;
    }
    return it->second;
  }

  // returns true if label type on call arc results in epsilon input label
  bool EpsilonOnCallInput() {
    return EpsilonOnInput(call_label_type_);
  }

  // private methods
 private:
  // hash stack prefix (return unique index into stackprefix table)
  PrefixId GetPrefixId(const StackPrefix& prefix) {
    return state_table_->FindPrefixId(prefix);
  }

  // prefix id after a stack pop
  PrefixId PopPrefix(StackPrefix prefix) {
    prefix.Pop();
    return GetPrefixId(prefix);
  }

  // prefix id after a stack push
  PrefixId PushPrefix(StackPrefix prefix, Label fst_id, StateId nextstate) {
    prefix.Push(fst_id, nextstate);
    return GetPrefixId(prefix);
  }

  // returns true if label type on arc results in epsilon input label
  bool EpsilonOnInput(ReplaceLabelType label_type) {
    if (label_type == REPLACE_LABEL_NEITHER ||
        label_type == REPLACE_LABEL_OUTPUT) return true;
    return false;
  }

  // returns true if label type on arc results in epsilon input label
  bool EpsilonOnOutput(ReplaceLabelType label_type) {
    if (label_type == REPLACE_LABEL_NEITHER ||
        label_type == REPLACE_LABEL_INPUT) return true;
    return false;
  }

  // returns true if for either the call or return arc ilabel != olabel
  bool ReplaceTransducer() {
    if (call_label_type_ == REPLACE_LABEL_INPUT ||
        call_label_type_ == REPLACE_LABEL_OUTPUT ||
        (call_label_type_ == REPLACE_LABEL_BOTH &&
            call_output_label_ != kNoLabel) ||
        return_label_type_ == REPLACE_LABEL_INPUT ||
        return_label_type_ == REPLACE_LABEL_OUTPUT) return true;
    return false;
  }

  // private data
 private:
  // runtime options
  ReplaceLabelType call_label_type_;  // how to label call arc
  ReplaceLabelType return_label_type_;  // how to label return arc
  int64 call_output_label_;  // specifies output label to put on call arc
  int64 return_label_;  // specifies label to put on return arc
  bool always_cache_;  // Optionally caching arc iterator disabled when true

  // state table
  StateTable *state_table_;

  // replace components
  set<Label> nonterminal_set_;
  NonTerminalHash nonterminal_hash_;
  vector<const Fst<A>*> fst_array_;
  Label root_;

  void operator=(const ReplaceFstImpl<A, T, C> &);  // disallow
};


//
// \class ReplaceFst
// \brief Recursively replaces arcs in the root Fst with other Fsts.
// This version is a delayed Fst.
//
// ReplaceFst supports dynamic replacement of arcs in one Fst with
// another Fst. This replacement is recursive.  ReplaceFst can be used
// to support a variety of delayed constructions such as recursive
// transition networks, union, or closure.  It is constructed with an
// array of Fst(s). One Fst represents the root (or topology)
// machine. The root Fst refers to other Fsts by recursively replacing
// arcs labeled as non-terminals with the matching non-terminal
// Fst. Currently the ReplaceFst uses the output symbols of the arcs
// to determine whether the arc is a non-terminal arc or not. A
// non-terminal can be any label that is not a non-zero terminal label
// in the output alphabet.
//
// Note that the constructor uses a vector of pair<>. These correspond
// to the tuple of non-terminal Label and corresponding Fst. For example
// to implement the closure operation we need 2 Fsts. The first root
// Fst is a single Arc on the start State that self loops, it references
// the particular machine for which we are performing the closure operation.
//
// The ReplaceFst class supports an optionally caching arc iterator:
//    ArcIterator< ReplaceFst<A> >
// The ReplaceFst need to be built such that it is known to be ilabel
// or olabel sorted (see usage below).
//
// Observe that Matcher<Fst<A> > will use the optionally caching arc
// iterator when available (Fst is ilabel sorted and matching on the
// input, or Fst is olabel sorted and matching on the output).
// In order to obtain the most efficient behaviour, it is recommended
// to set call_label_type to REPLACE_LABEL_INPUT or REPLACE_LABEL_BOTH
// and return_label_type to REPLACE_LABEL_OUTPUT or REPLACE_LABEL_NEITHER
// (this means that the call arc does not have epsilon on the input side
// and the return arc has epsilon on the input side) and matching on the
// input side.
//
// This class attaches interface to implementation and handles
// reference counting, delegating most methods to ImplToFst.
template <class A, class T /* = DefaultReplaceStateTable<A> */,
          class C /* = DefaultCacheStore<A> */ >
class ReplaceFst : public ImplToFst< ReplaceFstImpl<A, T, C> > {
 public:
  friend class ArcIterator< ReplaceFst<A, T, C> >;
  friend class StateIterator< ReplaceFst<A, T, C> >;
  friend class ReplaceFstMatcher<A, T, C>;

  typedef A Arc;
  typedef typename A::Label   Label;
  typedef typename A::Weight  Weight;
  typedef typename A::StateId StateId;
  typedef T StateTable;
  typedef C Store;
  typedef typename C::State State;
  typedef CacheBaseImpl<State, C> CImpl;
  typedef ReplaceFstImpl<A, T, C> Impl;

  using ImplToFst<Impl>::Properties;

  ReplaceFst(const vector<pair<Label, const Fst<A>* > >& fst_array,
             Label root)
      : ImplToFst<Impl>(
            new Impl(fst_array, ReplaceFstOptions<A, T, C>(root))) {}

  ReplaceFst(const vector<pair<Label, const Fst<A>* > >& fst_array,
             const ReplaceFstOptions<A, T, C> &opts)
      : ImplToFst<Impl>(new Impl(fst_array, opts)) {}

  // See Fst<>::Copy() for doc.
  ReplaceFst(const ReplaceFst<A, T, C>& fst, bool safe = false)
      : ImplToFst<Impl>(fst, safe) {}

  // Get a copy of this ReplaceFst. See Fst<>::Copy() for further doc.
  virtual ReplaceFst<A, T, C> *Copy(bool safe = false) const {
    return new ReplaceFst<A, T, C>(*this, safe);
  }

  virtual inline void InitStateIterator(StateIteratorData<A> *data) const;

  virtual void InitArcIterator(StateId s, ArcIteratorData<A> *data) const {
    GetImpl()->InitArcIterator(s, data);
  }

  virtual MatcherBase<A> *InitMatcher(MatchType match_type) const {
    if ((GetImpl()->ArcIteratorFlags() & kArcNoCache) &&
        ((match_type == MATCH_INPUT && Properties(kILabelSorted, false)) ||
         (match_type == MATCH_OUTPUT && Properties(kOLabelSorted, false)))) {
      return new ReplaceFstMatcher<A, T, C>(*this, match_type);
    } else {
      VLOG(2) << "Not using replace matcher";
      return 0;
    }
  }

  bool CyclicDependencies() const {
    return GetImpl()->CyclicDependencies();
  }

  const StateTable& GetStateTable() const {
    return *GetImpl()->GetStateTable();
  }

  const Fst<A> &GetFst(Label nonterminal) const {
    return *GetImpl()->GetFst(GetImpl()->GetFstId(nonterminal));
  }

 private:
  // Makes visible to friends.
  Impl *GetImpl() const { return ImplToFst<Impl>::GetImpl(); }

  void operator=(const ReplaceFst<A, T, C> &fst);  // disallow
};


// Specialization for ReplaceFst.
template<class A, class T, class C>
class StateIterator< ReplaceFst<A, T, C> >
    : public CacheStateIterator< ReplaceFst<A, T, C> > {
 public:
  explicit StateIterator(const ReplaceFst<A, T, C> &fst)
      : CacheStateIterator< ReplaceFst<A, T, C> >(fst, fst.GetImpl()) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(StateIterator);
};


// Specialization for ReplaceFst.
// Implements optional caching. It can be used as follows:
//
//   ReplaceFst<A> replace;
//   ArcIterator< ReplaceFst<A> > aiter(replace, s);
//   // Note: ArcIterator< Fst<A> > is always a caching arc iterator.
//   aiter.SetFlags(kArcNoCache, kArcNoCache);
//   // Use the arc iterator, no arc will be cached, no state will be expanded.
//   // The varied 'kArcValueFlags' can be used to decide which part
//   // of arc values needs to be computed.
//   aiter.SetFlags(kArcILabelValue, kArcValueFlags);
//   // Only want the ilabel for this arc
//   aiter.Value();  // Does not compute the destination state.
//   aiter.Next();
//   aiter.SetFlags(kArcNextStateValue, kArcNextStateValue);
//   // Want both ilabel and nextstate for that arc
//   aiter.Value();  // Does compute the destination state and inserts it
//                   // in the replace state table.
//   // No Arc has been cached at that point.
//
template <class A, class T, class C>
class ArcIterator< ReplaceFst<A, T, C> > {
 public:
  typedef A Arc;
  typedef typename A::StateId StateId;

  ArcIterator(const ReplaceFst<A, T, C> &fst, StateId s)
      : fst_(fst), state_(s), pos_(0), offset_(0), flags_(kArcValueFlags),
        arcs_(0), data_flags_(0), final_flags_(0) {
    cache_data_.ref_count = 0;
    local_data_.ref_count = 0;

    // If FST does not support optional caching, force caching.
    if (!(fst_.GetImpl()->ArcIteratorFlags() & kArcNoCache) &&
        !(fst_.GetImpl()->HasArcs(state_)))
      fst_.GetImpl()->Expand(state_);

    // If state is already cached, use cached arcs array.
    if (fst_.GetImpl()->HasArcs(state_)) {
      (fst_.GetImpl())
          ->template CacheBaseImpl<typename C::State, C>::InitArcIterator(
              state_, &cache_data_);
      num_arcs_ = cache_data_.narcs;
      arcs_ = cache_data_.arcs;      // 'arcs_' is a ptr to the cached arcs.
      data_flags_ = kArcValueFlags;  // All the arc member values are valid.
    } else {  // Otherwise delay decision until Value() is called.
      tuple_ = fst_.GetImpl()->GetStateTable()->Tuple(state_);
      if (tuple_.fst_state == kNoStateId) {
        num_arcs_ = 0;
      } else {
        // The decision to cache or not to cache has been defered
        // until Value() or SetFlags() is called. However, the arc
        // iterator is set up now to be ready for non-caching in order
        // to keep the Value() method simple and efficient.
        const Fst<A>* rfst = fst_.GetImpl()->GetFst(tuple_.fst_id);
        rfst->InitArcIterator(tuple_.fst_state, &local_data_);
        // 'arcs_' is a pointer to the arcs in the underlying machine.
        arcs_ = local_data_.arcs;
        // Compute the final arc (but not its destination state)
        // if a final arc is required.
        bool has_final_arc = fst_.GetImpl()->ComputeFinalArc(
            tuple_,
            &final_arc_,
            kArcValueFlags & ~kArcNextStateValue);
        // Set the arc value flags that hold for 'final_arc_'.
        final_flags_ = kArcValueFlags & ~kArcNextStateValue;
        // Compute the number of arcs.
        num_arcs_ = local_data_.narcs;
        if (has_final_arc)
          ++num_arcs_;
        // Set the offset between the underlying arc positions and
        // the positions in the arc iterator.
        offset_ = num_arcs_ - local_data_.narcs;
        // Defers the decision to cache or not until Value() or
        // SetFlags() is called.
        data_flags_ = 0;
      }
    }
  }

  ~ArcIterator() {
    if (cache_data_.ref_count)
      --(*cache_data_.ref_count);
    if (local_data_.ref_count)
      --(*local_data_.ref_count);
  }

  void ExpandAndCache() const   {
    // TODO(allauzen): revisit this
    // fst_.GetImpl()->Expand(state_, tuple_, local_data_);
    // (fst_.GetImpl())->CacheImpl<A>*>::InitArcIterator(state_,
    //                                               &cache_data_);
    //
    fst_.InitArcIterator(state_, &cache_data_);  // Expand and cache state.
    arcs_ = cache_data_.arcs;  // 'arcs_' is a pointer to the cached arcs.
    data_flags_ = kArcValueFlags;  // All the arc member values are valid.
    offset_ = 0;  // No offset
  }

  void Init() {
    if (flags_ & kArcNoCache) {  // If caching is disabled
      // 'arcs_' is a pointer to the arcs in the underlying machine.
      arcs_ = local_data_.arcs;
      // Set the arcs value flags that hold for 'arcs_'.
      data_flags_ = kArcWeightValue;
      if (!fst_.GetImpl()->EpsilonOnCallInput())
          data_flags_ |= kArcILabelValue;
      // Set the offset between the underlying arc positions and
      // the positions in the arc iterator.
      offset_ = num_arcs_ - local_data_.narcs;
    } else {  // Otherwise, expand and cache
      ExpandAndCache();
    }
  }

  bool Done() const { return pos_ >= num_arcs_; }

  const A& Value() const {
    // If 'data_flags_' was set to 0, non-caching was not requested
    if (!data_flags_) {
      // TODO(allauzen): revisit this.
      if (flags_ & kArcNoCache) {
        // Should never happen.
        FSTERROR() << "ReplaceFst: inconsistent arc iterator flags";
      }
      ExpandAndCache();  // Expand and cache.
    }

    if (pos_ - offset_ >= 0) {  // The requested arc is not the 'final' arc.
      const A& arc = arcs_[pos_ - offset_];
      if ((data_flags_ & flags_) == (flags_ & kArcValueFlags)) {
        // If the value flags for 'arc' match the recquired value flags
        // then return 'arc'.
        return arc;
      } else {
        // Otherwise, compute the corresponding arc on-the-fly.
        fst_.GetImpl()->ComputeArc(tuple_, arc, &arc_, flags_ & kArcValueFlags);
        return arc_;
      }
    } else {  // The requested arc is the 'final' arc.
      if ((final_flags_ & flags_) != (flags_ & kArcValueFlags)) {
        // If the arc value flags that hold for the final arc
        // do not match the requested value flags, then
        // 'final_arc_' needs to be updated.
        fst_.GetImpl()->ComputeFinalArc(tuple_, &final_arc_,
                                    flags_ & kArcValueFlags);
        final_flags_ = flags_ & kArcValueFlags;
      }
      return final_arc_;
    }
  }

  void Next() { ++pos_; }

  size_t Position() const { return pos_; }

  void Reset() { pos_ = 0;  }

  void Seek(size_t pos) { pos_ = pos; }

  uint32 Flags() const { return flags_; }

  void SetFlags(uint32 f, uint32 mask) {
    // Update the flags taking into account what flags are supported
    // by the Fst.
    flags_ &= ~mask;
    flags_ |= (f & fst_.GetImpl()->ArcIteratorFlags());
    // If non-caching is not requested (and caching has not already
    // been performed), then flush 'data_flags_' to request caching
    // during the next call to Value().
    if (!(flags_ & kArcNoCache) && data_flags_ != kArcValueFlags) {
      if (!fst_.GetImpl()->HasArcs(state_))
         data_flags_ = 0;
    }
    // If 'data_flags_' has been flushed but non-caching is requested
    // before calling Value(), then set up the iterator for non-caching.
    if ((f & kArcNoCache) && (!data_flags_))
      Init();
  }

 private:
  const ReplaceFst<A, T, C> &fst_;           // Reference to the FST
  StateId state_;                         // State in the FST
  mutable typename T::StateTuple tuple_;  // Tuple corresponding to state_

  ssize_t pos_;             // Current position
  mutable ssize_t offset_;  // Offset between position in iterator and in arcs_
  ssize_t num_arcs_;        // Number of arcs at state_
  uint32 flags_;            // Behavorial flags for the arc iterator
  mutable Arc arc_;         // Memory to temporarily store computed arcs

  mutable ArcIteratorData<Arc> cache_data_;  // Arc iterator data in cache
  mutable ArcIteratorData<Arc> local_data_;  // Arc iterator data in local fst

  mutable const A* arcs_;       // Array of arcs
  mutable uint32 data_flags_;   // Arc value flags valid for data in arcs_
  mutable Arc final_arc_;       // Final arc (when required)
  mutable uint32 final_flags_;  // Arc value flags valid for final_arc_

  DISALLOW_COPY_AND_ASSIGN(ArcIterator);
};


template <class A, class T, class C>
class ReplaceFstMatcher : public MatcherBase<A> {
 public:
  typedef ReplaceFst<A, T, C> FST;
  typedef A Arc;
  typedef typename A::StateId StateId;
  typedef typename A::Label Label;
  typedef MultiEpsMatcher<Matcher<Fst<A> > > LocalMatcher;

  ReplaceFstMatcher(const ReplaceFst<A, T, C> &fst,
                    fst::MatchType match_type)
      : fst_(fst),
        impl_(fst_.GetImpl()),
        s_(fst::kNoStateId),
        match_type_(match_type),
        current_loop_(false),
        final_arc_(false),
        loop_(fst::kNoLabel, 0, A::Weight::One(), fst::kNoStateId) {
    if (match_type_ == fst::MATCH_OUTPUT)
      swap(loop_.ilabel, loop_.olabel);
    InitMatchers();
  }

  ReplaceFstMatcher(const ReplaceFstMatcher<A, T, C> &matcher,
                    bool safe = false)
      : fst_(matcher.fst_),
        impl_(fst_.GetImpl()),
        s_(fst::kNoStateId),
        match_type_(matcher.match_type_),
        current_loop_(false),
        final_arc_(false),
        loop_(fst::kNoLabel, 0, A::Weight::One(), fst::kNoStateId) {
    if (match_type_ == fst::MATCH_OUTPUT)
      swap(loop_.ilabel, loop_.olabel);
    InitMatchers();
  }

  // Create a local matcher for each component Fst of replace.
  // LocalMatcher is a multi epsilon wrapper matcher. MultiEpsilonMatcher
  // is used to match each non-terminal arc, since these non-terminal
  // turn into epsilons on recursion.
  void InitMatchers() {
    const vector<const Fst<A>*>& fst_array = impl_->fst_array_;
    matcher_.resize(fst_array.size(), 0);
    for (size_t i = 0; i < fst_array.size(); ++i) {
      if (fst_array[i]) {
        matcher_[i] =
            new LocalMatcher(*fst_array[i], match_type_, kMultiEpsList);

        typename set<Label>::iterator it = impl_->nonterminal_set_.begin();
        for (; it != impl_->nonterminal_set_.end(); ++it) {
          matcher_[i]->AddMultiEpsLabel(*it);
        }
      }
    }
  }

  virtual ReplaceFstMatcher<A, T, C> *Copy(bool safe = false) const {
    return new ReplaceFstMatcher<A, T, C>(*this, safe);
  }

  virtual ~ReplaceFstMatcher() {
    for (size_t i = 0; i < matcher_.size(); ++i)
      delete matcher_[i];
  }

  virtual MatchType Type(bool test) const {
    if (match_type_ == MATCH_NONE)
      return match_type_;

    uint64 true_prop =  match_type_ == MATCH_INPUT ?
        kILabelSorted : kOLabelSorted;
    uint64 false_prop = match_type_ == MATCH_INPUT ?
        kNotILabelSorted : kNotOLabelSorted;
    uint64 props = fst_.Properties(true_prop | false_prop, test);

    if (props & true_prop)
      return match_type_;
    else if (props & false_prop)
      return MATCH_NONE;
    else
      return MATCH_UNKNOWN;
  }

  virtual const Fst<A> &GetFst() const {
    return fst_;
  }

  virtual uint64 Properties(uint64 props) const {
    return props;
  }

 private:
  // Set the sate from which our matching happens.
  virtual void SetState_(StateId s) {
    if (s_ == s) return;

    s_ = s;
    tuple_ = impl_->GetStateTable()->Tuple(s_);
    if (tuple_.fst_state == kNoStateId) {
      done_ = true;
      return;
    }
    // Get current matcher. Used for non epsilon matching
    current_matcher_ = matcher_[tuple_.fst_id];
    current_matcher_->SetState(tuple_.fst_state);
    loop_.nextstate = s_;

    final_arc_ = false;
  }

  // Search for label, from previous set state. If label == 0, first
  // hallucinate and epsilon loop, else use the underlying matcher to
  // search for the label or epsilons.
  // - Note since the ReplaceFST recursion on non-terminal arcs causes
  //   epsilon transitions to be created we use the MultiEpsilonMatcher
  //   to search for possible matches of non terminals.
  // - If the component Fst reaches a final state we also need to add
  //   the exiting final arc.
  virtual bool Find_(Label label) {
    bool found = false;
    label_ = label;
    if (label_ == 0 || label_ == kNoLabel) {
      // Compute loop directly, saving Replace::ComputeArc
      if (label_ == 0) {
        current_loop_ = true;
        found = true;
      }
      // Search for matching multi epsilons
      final_arc_ = impl_->ComputeFinalArc(tuple_, 0);
      found = current_matcher_->Find(kNoLabel) || final_arc_ || found;
    } else {
      // Search on sub machine directly using sub machine matcher.
      found = current_matcher_->Find(label_);
    }
    return found;
  }

  virtual bool Done_() const {
    return !current_loop_ && !final_arc_ && current_matcher_->Done();
  }

  virtual const Arc& Value_() const {
    if (current_loop_) {
      return loop_;
    }
    if (final_arc_) {
      impl_->ComputeFinalArc(tuple_, &arc_);
      return arc_;
    }
    const Arc& component_arc = current_matcher_->Value();
    impl_->ComputeArc(tuple_, component_arc, &arc_);
    return arc_;
  }

  virtual void Next_() {
    if (current_loop_) {
      current_loop_ = false;
      return;
    }
    if (final_arc_) {
      final_arc_ = false;
      return;
    }
    current_matcher_->Next();
  }

  virtual ssize_t Priority_(StateId s) { return fst_.NumArcs(s); }

  const ReplaceFst<A, T, C>& fst_;
  ReplaceFstImpl<A, T, C> *impl_;
  LocalMatcher* current_matcher_;
  vector<LocalMatcher*> matcher_;

  StateId s_;                        // Current state
  Label label_;                      // Current label

  MatchType match_type_;             // Supplied by caller
  mutable bool done_;
  mutable bool current_loop_;        // Current arc is the implicit loop
  mutable bool final_arc_;           // Current arc for exiting recursion
  mutable typename T::StateTuple tuple_;  // Tuple corresponding to state_
  mutable Arc arc_;
  Arc loop_;

  void operator=(const ReplaceFstMatcher<A, T, C> &matcher);  // disallow
};

template <class A, class T, class C> inline
void ReplaceFst<A, T, C>::InitStateIterator(StateIteratorData<A> *data) const {
  data->base = new StateIterator< ReplaceFst<A, T, C> >(*this);
}

typedef ReplaceFst<StdArc> StdReplaceFst;

// // Recursivively replaces arcs in the root Fst with other Fsts.
// This version writes the result of replacement to an output MutableFst.
//
// Replace supports replacement of arcs in one Fst with another
// Fst. This replacement is recursive.  Replace takes an array of
// Fst(s). One Fst represents the root (or topology) machine. The root
// Fst refers to other Fsts by recursively replacing arcs labeled as
// non-terminals with the matching non-terminal Fst. Currently Replace
// uses the output symbols of the arcs to determine whether the arc is
// a non-terminal arc or not. A non-terminal can be any label that is
// not a non-zero terminal label in the output alphabet.  Note that
// input argument is a vector of pair<>. These correspond to the tuple
// of non-terminal Label and corresponding Fst.
template<class Arc>
void Replace(const vector<pair<typename Arc::Label,
             const Fst<Arc>* > >& ifst_array,
             MutableFst<Arc> *ofst, ReplaceFstOptions<Arc> opts =
             ReplaceFstOptions<Arc>()) {
  opts.gc = true;
  opts.gc_limit = 0;  // Cache only the last state for fastest copy.
  *ofst = ReplaceFst<Arc>(ifst_array, opts);
}

template<class Arc>
void Replace(const vector<pair<typename Arc::Label,
             const Fst<Arc>* > >& ifst_array,
             MutableFst<Arc> *ofst, fst::ReplaceUtilOptions opts) {
  Replace(ifst_array, ofst, ReplaceFstOptions<Arc>(opts));
}

// Included for backward compatibility with 'epsilon_on_replace' arguments
template<class Arc>
void Replace(const vector<pair<typename Arc::Label,
             const Fst<Arc>* > >& ifst_array,
             MutableFst<Arc> *ofst, typename Arc::Label root,
             bool epsilon_on_replace) {
  Replace(ifst_array, ofst, ReplaceFstOptions<Arc>(root, epsilon_on_replace));
}

template<class Arc>
void Replace(const vector<pair<typename Arc::Label,
             const Fst<Arc>* > >& ifst_array,
             MutableFst<Arc> *ofst, typename Arc::Label root) {
  Replace(ifst_array, ofst, ReplaceFstOptions<Arc>(root));
}

}  // namespace fst

#endif  // FST_LIB_REPLACE_H__
