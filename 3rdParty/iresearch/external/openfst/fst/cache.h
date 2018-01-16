// cache.h

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
// An Fst implementation that caches FST elements of a delayed
// computation.

#ifndef FST_LIB_CACHE_H__
#define FST_LIB_CACHE_H__

#include <functional>
#include <list>
#include <unordered_map>
using std::unordered_map;
using std::unordered_multimap;
#include <vector>
using std::vector;


#include <fst/vector-fst.h>


DECLARE_bool(fst_default_cache_gc);
DECLARE_int64(fst_default_cache_gc_limit);

namespace fst {

// Options for controlling caching behavior; higher
// level than CacheImplOptions.
struct CacheOptions {
  bool gc;          // enable GC
  size_t gc_limit;  // # of bytes allowed before GC

  CacheOptions(bool g, size_t l) : gc(g), gc_limit(l) {}
  CacheOptions()
      : gc(FLAGS_fst_default_cache_gc),
        gc_limit(FLAGS_fst_default_cache_gc_limit) {}
};

// Options for controlling caching behavior; lower
// level than CacheOptions - templated on the
// cache store and allows passing the store.
template <class C>
struct CacheImplOptions {
  bool gc;          // enable GC
  size_t gc_limit;  // # of bytes allowed before GC
  C *store;         // cache store

  CacheImplOptions(bool g, size_t l, C s = 0)
      : gc(g), gc_limit(l), store(0) {}

 explicit CacheImplOptions(const CacheOptions &opts)
      : gc(opts.gc), gc_limit(opts.gc_limit), store(0) {}

  CacheImplOptions()
      : gc(FLAGS_fst_default_cache_gc),
        gc_limit(FLAGS_fst_default_cache_gc_limit),
        store(0) {}
};

// CACHE FLAGS

const uint32 kCacheFinal =    0x0001;    // Final weight has been cached
const uint32 kCacheArcs =     0x0002;    // Arcs have been cached
const uint32 kCacheInit =     0x0004;    // Initialized by GC
const uint32 kCacheRecent =   0x0008;    // Visited since GC
const uint32 kCacheFlags = kCacheFinal | kCacheArcs | kCacheInit |
    kCacheRecent;


// CACHE STATE - Arcs implemented by an STL vector per state.
template <class A, class M = PoolAllocator<A> >
class CacheState {
 public:
  typedef A Arc;
  typedef typename A::Weight Weight;
  typedef typename A::StateId StateId;
  typedef M ArcAllocator;
  typedef typename ArcAllocator::template
  rebind<CacheState<A, M> >::other StateAllocator;

  // Provide STL allocator for arcs
  explicit CacheState(const ArcAllocator &alloc)
    : final_(Weight::Zero()), niepsilons_(0), noepsilons_(0),
      arcs_(alloc), flags_(0), ref_count_(0) {}

  CacheState(const CacheState<A> &state, const ArcAllocator &alloc)
    : final_(state.Final()),
      niepsilons_(state.NumInputEpsilons()),
      noepsilons_(state.NumOutputEpsilons()),
      arcs_(state.arcs_.begin(), state.arcs_.end(), alloc),
      flags_(state.Flags()),
      ref_count_(0) { }

  void Reset() {
    final_ = Weight::Zero();
    niepsilons_ = 0;
    noepsilons_ = 0;
    ref_count_ = 0;
    flags_ = 0;
    arcs_.clear();
  }

  Weight Final() const { return final_; }
  size_t NumInputEpsilons() const { return niepsilons_; }
  size_t NumOutputEpsilons() const { return noepsilons_; }
  size_t NumArcs() const { return arcs_.size(); }
  const A &GetArc(size_t n) const { return arcs_[n]; }

  // Used by the ArcIterator<Fst<A>> efficient implementation.
  const A *Arcs() const { return arcs_.size() > 0 ? &arcs_[0] : 0; }

  // Accesses flags; used by the caller
  uint32 Flags() const { return flags_; }
  // Accesses ref count; used by the caller
  int RefCount() const { return ref_count_; }

  void SetFinal(Weight final) { final_ = final; }

  void ReserveArcs(size_t n) { arcs_.reserve(n); }

  // Adds one arc at a time with all needed book-keeping.
  // Can use PushArc's and SetArcs instead as more efficient alternative.
  void AddArc(const A &arc) {
    arcs_.push_back(arc);
    if (arc.ilabel == 0)
      ++niepsilons_;
    if (arc.olabel == 0)
      ++noepsilons_;
  }

  // Adds one arc at a time with delayed book-keeping; finalize with SetArcs()
  void PushArc(const A &arc) { arcs_.push_back(arc); }

  // Finalizes arcs book-keeping; call only once
  void SetArcs() {
    for (size_t a = 0; a < arcs_.size(); ++a) {
      const Arc &arc = arcs_[a];
      if (arc.ilabel == 0)
        ++niepsilons_;
      if (arc.olabel == 0)
        ++noepsilons_;
    }
  }

  // Modifies nth arc
  void SetArc(const A &arc, size_t n) {
    if (arcs_[n].ilabel == 0)
      --niepsilons_;
    if (arcs_[n].olabel == 0)
      --noepsilons_;
    if (arc.ilabel == 0)
      ++niepsilons_;
    if (arc.olabel == 0)
      ++noepsilons_;
    arcs_[n] = arc;
  }

  // Deletes all arcs
  void DeleteArcs() {
    niepsilons_ = 0;
    noepsilons_ = 0;
    arcs_.clear();
  }

  void DeleteArcs(size_t n) {
    for (size_t i = 0; i < n; ++i) {
      if (arcs_.back().ilabel == 0)
        --niepsilons_;
      if (arcs_.back().olabel == 0)
        --noepsilons_;
      arcs_.pop_back();
    }
  }

  // Sets status flags; used by the caller
  void SetFlags(uint32 flags, uint32 mask) const {
    flags_ &= ~mask;
    flags_ |= flags;
  }

  // Mutates ref counts; used by the caller
  int IncrRefCount() const { return  ++ref_count_; }
  int DecrRefCount() const { return --ref_count_; }

  // Used by the ArcIterator<Fst<A>> efficient implementation.
  int *MutableRefCount() const { return &ref_count_; }

  // For state class allocation
  void *operator new(size_t size, StateAllocator *alloc) {
    return alloc->allocate(1);
  }

  // For state destruction and memory freeing
  static void Destroy(CacheState<A> *state, StateAllocator *alloc) {
    if (state) {
      state->~CacheState<A>();
      alloc->deallocate(state, 1);
    }
  }

 private:
  Weight final_;                   // Final weight
  size_t niepsilons_;              // # of input epsilons
  size_t noepsilons_;              // # of output epsilons
  vector<A, ArcAllocator> arcs_;   // Arcs represenation
  mutable uint32 flags_;
  mutable int ref_count_;          // if 0, avail. for GC; used by arc iterators

  DISALLOW_COPY_AND_ASSIGN(CacheState);
};


// CACHE STORE - these allocate and store states, provide a mapping
// from state IDs to cached states and an iterator over
// the states. The state template argument should be a CacheState
// as above (or have the same interface). The state for StateId s
// is constructed when requested by GetMutableState(s) if it not
// yet stored. Initially a state has RefCount() = 0; the user may increment
// and decrement to control the time of destruction. In particular, this state
// is destroyed when:
//    (1) this class is destroyed or
//    (2) Clear() is called or Delete() for it is called or
//    (3) possibly when:
//         (a) opts.gc = true and
//         (b) the cache store size exceeds opts.gc_limit bytes and
//         (c) the state's RefCount() is zero and
//         (d) the state is not the most recently requested state.
//        The actual GC behavior is up to the specific store.
//
// template <class S>
// class CacheStore {
//  public:
//   typedef S State;
//   typedef typename State::Arc Arc;
//   typedef typename Arc::StateId StateId;
//
//   // Required constructors/assignment operators
//   explicit CacheStore(const CacheOptions &opts);
//   CacheStore(const CacheStore &store);
//   CacheStore<State> &operator=(const CacheStore<State> &store);
//
//   // Returns 0 if state is not stored
//   const State *GetState(StateId s);
//
//   // Creates state if state is not stored
//   State *GetMutableState(StateId s);
//
//   // Similar to State::AddArc() but updates cache store book-keeping
//   void AddArc(StateId *state, const Arc &arc);
//
//   // Similar to State::SetArcs() but updates cache store book-keeping
//   // Call only once.
//   void SetArcs(StateId *state);
//
//   // Similar to State::DeleteArcs() but updates cache store book-keeping
//   void DeleteArcs(State *state);
//   void DeleteArcs(State *state, size_t n);
//
//   // Deletes all cached states
//   void Clear();
//
//
//   // Iterates over cached states (in an arbitrary order).
//   // Only needed if opts.gc is true
//   bool Done() const;      // End of iteration
//   StateId Value() const;  // Current state
//   void Next();            // Advances to next state (when !Done)
//   void Reset();           // Return to initial condition
//   void Delete();          // Deletes current state and advances to next
// };

//
// CONTAINER CACHE STORES - these simply hold states
//

// This class uses a vector of pointers to states to store cached states.
template <class S>
class VectorCacheStore {
 public:
  typedef S State;
  typedef typename State::Arc Arc;
  typedef typename Arc::StateId StateId;
  typedef list<StateId, PoolAllocator<StateId> > StateList;

  // Required constructors/assignment operators
  explicit VectorCacheStore(const CacheOptions &opts) : cache_gc_(opts.gc) {
    Clear();
    Reset();
  }

  VectorCacheStore(const VectorCacheStore<S> &store)
      : cache_gc_(store.cache_gc_) {
    CopyStates(store);
    Reset();
  }

  ~VectorCacheStore() {
    Clear();
  }

  VectorCacheStore<State> &operator=(const VectorCacheStore<State> &store) {
    if (this != &store) {
      CopyStates(store);
      Reset();
    }
    return *this;
  }

  // Returns 0 if state is not stored
  const State *GetState(StateId s) const {
    return s < state_vec_.size() ? state_vec_[s] : 0;
  }

  // Creates state if state is not stored
  State *GetMutableState(StateId s) {
    State *state = 0;
    if (s >= state_vec_.size()) {
      state_vec_.resize(s + 1, 0);
    } else {
      state = state_vec_[s];
    }

    if (state == 0) {
      state = new(&state_alloc_) State(arc_alloc_);
      state_vec_[s] = state;
      if (cache_gc_)
        state_list_.push_back(s);
    }
    return state;
  }

  // Similar to State::AddArc() but updates cache store book-keeping
  void AddArc(State *state, const Arc &arc) { state->AddArc(arc); }

  // Similar to State::SetArcs() but updates internal cache size.
  // Call only once.
  void SetArcs(State *state) { state->SetArcs(); }

  // Deletes all arcs
  void DeleteArcs(State *state) { state->DeleteArcs(); }

  // Deletes some arcs
  void DeleteArcs(State *state, size_t n) { state->DeleteArcs(n); }

  // Deletes all cached states
  void Clear() {
    for (StateId s = 0; s < state_vec_.size(); ++s)
      State::Destroy(state_vec_[s], &state_alloc_);
    state_vec_.clear();
    state_list_.clear();
  }

  // Iterates over cached states (in an arbitrary order).
  // Only works if GC is enabled (o.w. avoiding state_list_ overhead).
  bool Done() const { return iter_ == state_list_.end(); }
  StateId Value() const { return *iter_; }
  void Next() { ++iter_; }
  void Reset() { iter_ = state_list_.begin();  }

  // Deletes current state and advances to next.
  void Delete() {
    State::Destroy(state_vec_[*iter_], &state_alloc_);
    state_vec_[*iter_] = 0;
    state_list_.erase(iter_++);
  }

 private:
  void CopyStates(const VectorCacheStore<State> &store) {
    Clear();
    state_vec_.reserve(store.state_vec_.size());
    for (StateId s = 0; s < store.state_vec_.size(); ++s) {
      S *state = 0;
      const State *store_state = store.state_vec_[s];
      if (store_state) {
        state = new(&state_alloc_) State(*store_state, arc_alloc_);
        if (cache_gc_)
          state_list_.push_back(s);
      }
      state_vec_.push_back(state);
    }
  }

  bool cache_gc_;                          // supports iteration when true
  vector<State *> state_vec_;              // vector of states (NULL if empty)
  StateList state_list_;                   // list of states
  typename StateList::iterator iter_;      // state list iterator
  typename State::StateAllocator state_alloc_;  // for state allocation
  typename State::ArcAllocator arc_alloc_;      // for arc allocation
};


// This class uses a hash map from state Ids to pointers to states
// to store cached states.
template <class S>
class HashCacheStore {
 public:
  typedef S State;
  typedef typename State::Arc Arc;
  typedef typename Arc::StateId StateId;
  typedef unordered_map<StateId, State *, std::hash<StateId>, std::equal_to<StateId>,
                   PoolAllocator<pair<const StateId, State *> > > StateMap;

  // Required constructors/assignment operators
  explicit HashCacheStore(const CacheOptions &opts) {
    Clear();
    Reset();
  }

  HashCacheStore(const HashCacheStore<S> &store) {
    CopyStates(store);
    Reset();
  }

  ~HashCacheStore() { Clear(); }

  HashCacheStore<State> &operator=(const HashCacheStore<State> &store) {
    if (this != &store) {
      CopyStates(store);
      Reset();
    }
    return *this;
  }

  // Returns 0 if state is not stoed
  const State *GetState(StateId s) const {
    const typename StateMap::const_iterator it = state_map_.find(s);
    return it != state_map_.end() ? it->second : 0;
  }

  // Creates state if state is not stored
  State *GetMutableState(StateId s) {
    State* &state = state_map_[s];
    if (state == 0)
      state = new(&state_alloc_) State(arc_alloc_);
    return state;
  }

  // Similar to State::AddArc() but updates cache store book-keeping
  void AddArc(State *state, const Arc &arc) { state->AddArc(arc); }

  // Similar to State::SetArcs() but updates internal cache size.
  // Call only once.
  void SetArcs(State *state) { state->SetArcs(); }

  // Deletes all arcs
  void DeleteArcs(State *state) { state->DeleteArcs(); }

  // Deletes some arcs
  void DeleteArcs(State *state, size_t n) { state->DeleteArcs(n); }

  // Deletes all cached states
  void Clear() {
    for (typename StateMap::iterator it = state_map_.begin();
         it != state_map_.end();
         ++it) {
      State::Destroy(it->second, &state_alloc_);
    }
    state_map_.clear();
  }

  // Iterates over cached states (in an arbitrary order).
  bool Done() const {
    typename StateMap::const_iterator citer = iter_;
    return citer == state_map_.end();
  }

  StateId Value() const { return iter_->first; }
  void Next() { ++iter_; }
  void Reset() { iter_ = state_map_.begin();  }

  // Deletes current state and advances to next.
  void Delete() {
    State::Destroy(iter_->second, &state_alloc_);
    state_map_.erase(iter_++);
  }

 private:
  void CopyStates(const HashCacheStore<State> &store) {
    Clear();
    for (typename StateMap::const_iterator it = store.state_map_.begin();
         it != store.state_map_.end();
         ++it) {
      StateId s = it->first;
      const S *state = it->second;
      state_map_[s] = new(&state_alloc_) State(*state, arc_alloc_);
    }
  }

  StateMap state_map_;                     // map from State Id to state
  typename StateMap::iterator iter_;       // state map iterator
  typename State::StateAllocator state_alloc_;  // for state allocation
  typename State::ArcAllocator arc_alloc_;      // for arc allocation
};


//
// GARBAGE COLLECTION CACHE STORES - these garbage collect underlying
// container cache stores.
//

// This class implements a simple garbage collection scheme when
// 'opts.gc_limit' is 0. In particular, the first cached state is reused
// for each new state so long as the reference count is zero on
// the to-be-reused state. Otherwise, the full underlying store is used.
// The caller can increment the reference count to inhibit the
// GC of in-use states (e.g., in an ArcIterator).
//
// The typical use case for this optimization is when a single pass over
// a cached FST is performed with only one-state expanded at a time.
template <class C>
class FirstCacheStore {
 public:
  typedef typename C::State State;
  typedef typename State::Arc Arc;
  typedef typename Arc::StateId StateId;

  // Required constructors/assignment operators
  explicit FirstCacheStore(const CacheOptions &opts) :
      store_(opts),
      cache_gc_(opts.gc_limit == 0),     // opts.gc ignored historically
      cache_first_state_id_(kNoStateId),
      cache_first_state_(0) { }

  FirstCacheStore(const FirstCacheStore<C> &store) :
      store_(store.store_),
      cache_gc_(store.cache_gc_),
      cache_first_state_id_(store.cache_first_state_id_),
      cache_first_state_(store.cache_first_state_id_ != kNoStateId ?
                         store_.GetMutableState(0) : 0) { }

  FirstCacheStore<C> &operator=(const FirstCacheStore<C> &store) {
    if (this != &store) {
      store_ = store.store_;
      cache_gc_ = store.cache_gc_;
      cache_first_state_id_ = store.cache_first_state_id_;
      cache_first_state_ = store.cache_first_state_id_ != kNoStateId ?
          store_.GetMutableState(0) : 0;
    }
    return *this;
  }


  // Returns 0 if state is not stored
  const State *GetState(StateId s) const {
    // store_ state 0 may hold first cached state; rest shifted + 1
    return s == cache_first_state_id_ ?
        cache_first_state_ : store_.GetState(s + 1);
  }

  // Creates state if state is not stored
  State *GetMutableState(StateId s) {
    // store_ state 0 used to hold first cached state; rest shifted + 1
    if (cache_first_state_id_ == s)
      return cache_first_state_;          // request for first cached state

    if (cache_gc_) {
      if (cache_first_state_id_ == kNoStateId) {
        cache_first_state_id_ = s;        // sets first cached state
        cache_first_state_ = store_.GetMutableState(0);
        cache_first_state_->SetFlags(kCacheInit, kCacheInit);
        cache_first_state_->ReserveArcs(2 * kAllocSize);
        return cache_first_state_;
      } else if (cache_first_state_->RefCount() == 0) {
        cache_first_state_id_ = s;        // updates first cached state
        cache_first_state_->Reset();
        cache_first_state_->SetFlags(kCacheInit, kCacheInit);
        return cache_first_state_;
      } else {                            // keeps first cached state
        cache_first_state_->SetFlags(0, kCacheInit);  // clears initialized bit
        cache_gc_ = false;                // disable GC
      }
    }

    State *state = store_.GetMutableState(s + 1);
    return state;
  }

  // Similar to State::AddArc() but updates cache store book-keeping
  void AddArc(State *state, const Arc &arc) {
    store_.AddArc(state, arc);
  }

  // Similar to State::SetArcs() but updates internal cache size.
  // Call only once.
  void SetArcs(State *state) {
    store_.SetArcs(state);
  }

  // Deletes all arcs
  void DeleteArcs(State *state) {
    store_.DeleteArcs(state);
  }

  // Deletes some arcs
  void DeleteArcs(State *state, size_t n) {
    store_.DeleteArcs(state, n);
  }

  // Deletes all cached states
  void Clear() {
    store_.Clear();
    cache_first_state_id_ = kNoStateId;
    cache_first_state_ = 0;
  }

  // Iterates over cached states (in an arbitrary order).
  // Only needed if GC is enabled.
  bool Done() const { return store_.Done(); }

  StateId Value() const {
    // store_ state 0 may hold first cached state; rest shifted + 1
    StateId s = store_.Value();
    return s == 0 ? cache_first_state_id_ : s - 1;
  }

  void Next() { store_.Next(); }
  void Reset() { store_.Reset(); }

  // Deletes current state and advances to next.
  void Delete() {
    if (Value() == cache_first_state_id_) {
      cache_first_state_id_ = kNoStateId;
      cache_first_state_ = 0;
    }
    store_.Delete();
  }

 private:
  C store_;                            // Underlying store
  bool cache_gc_;                      // GC enabled
  StateId cache_first_state_id_;       // First cached state ID
  State *cache_first_state_;           // First cached state
};


// This class implements mark-sweep garbage collection on an
// underlying cache store.  If the 'gc' option is 'true', garbage
// collection of states is performed in a rough approximation of LRU
// order, when 'gc_limit' bytes is reached - controlling memory
// use. The caller can increment the reference count to inhibit the
// GC of in-use states (e.g., in an ArcIterator). With GC enabled,
// the 'gc_limit' parameter allows the caller to trade-off time vs space.
template <class C>
class GCCacheStore {
 public:
  typedef typename C::State State;
  typedef typename State::Arc Arc;
  typedef typename Arc::StateId StateId;

  // Required constructors/assignment operators
  explicit GCCacheStore(const CacheOptions &opts) :
      store_(opts),
      cache_gc_request_(opts.gc),
      cache_limit_(opts.gc_limit > kMinCacheLimit ?
                   opts.gc_limit : kMinCacheLimit),
      cache_gc_(false),
      cache_size_(0) { }

  // Returns 0 if state is not stored
  const State *GetState(StateId s) const { return store_.GetState(s); }

  // Creates state if state is not stored
  State *GetMutableState(StateId s) {
    State *state = store_.GetMutableState(s);

    if (cache_gc_request_ && !(state->Flags() & kCacheInit)) {
      state->SetFlags(kCacheInit, kCacheInit);
      cache_size_ += sizeof(State) + state->NumArcs() * sizeof(Arc);
      // GC is enabled once an uninited state (from underlying store) is seen
      cache_gc_ = true;
      if (cache_size_ > cache_limit_)
        GC(state, false);
    }
    return state;
  }

  // Similar to State::AddArc() but updates cache store book-keeping
  void AddArc(State *state, const Arc &arc) {
    store_.AddArc(state, arc);
    if (cache_gc_ && (state->Flags() & kCacheInit)) {
      cache_size_ += sizeof(Arc);
      if (cache_size_ > cache_limit_)
        GC(state, false);
    }
  }

  // Similar to State::SetArcs() but updates internal cache size.
  // Call only once.
  void SetArcs(State *state) {
    store_.SetArcs(state);
    if (cache_gc_ && (state->Flags() & kCacheInit)) {
      cache_size_ += state->NumArcs() * sizeof(Arc);
      if (cache_size_ > cache_limit_)
        GC(state, false);
    }
  }

  // Deletes all arcs
  void DeleteArcs(State *state) {
    if (cache_gc_ && (state->Flags() & kCacheInit))
      cache_size_ -= state->NumArcs() * sizeof(Arc);
    store_.DeleteArcs(state);
  }

  // Deletes some arcs
  void DeleteArcs(State *state, size_t n) {
    if (cache_gc_ && (state->Flags() & kCacheInit))
      cache_size_ -= n * sizeof(Arc);
    store_.DeleteArcs(state, n);
  }

  // Deletes all cached states
  void Clear() {
    store_.Clear();
    cache_size_ = 0;
  }

  // Iterates over cached states (in an arbitrary order).
  // Only needed if GC is enabled.
  bool Done() const { return store_.Done(); }

  StateId Value() const { return store_.Value(); }
  void Next() { store_.Next(); }
  void Reset() { store_.Reset(); }

  // Deletes current state and advances to next.
  void Delete() {
    if (cache_gc_) {
      const State *state = store_.GetState(Value());
      if (state->Flags() & kCacheInit)
        cache_size_ -= sizeof(State) + state->NumArcs() * sizeof(Arc);
    }
    store_.Delete();
  }

  // Removes from the cache store (not referenced-counted and not the
  // current) states that have not been accessed since the last GC
  // until at most cache_fraction * cache_limit_ bytes are cached.  If
  // that fails to free enough, recurs uncaching recently visited
  // states as well. If still unable to free enough memory, then
  // widens cache_limit_ to fulfill condition.
  void GC(const State *current, bool free_recent,
          float cache_fraction = 0.666);

 private:
  static const size_t kMinCacheLimit = 8096;   // Min. cache limit

  C store_;                 // Underlying store
  bool cache_gc_request_;   // GC requested but possibly not yet enabled
  size_t cache_limit_;      // # of bytes allowed before GC
  bool cache_gc_;           // GC enabled
  size_t cache_size_;       // # of bytes cached
};


// Removes from the cache store (not referenced-counted and not the
// current) states that have not been accessed since the last GC until
// at most cache_fraction * cache_limit_ bytes are cached.  If that
// fails to free enough, recurs uncaching recently visited states as
// well. If still unable to free enough memory, then widens
// cache_limit_ to fulfill condition.
template <class C>
void GCCacheStore<C>::GC(const State *current,
                         bool free_recent, float cache_fraction) {
  if (!cache_gc_)
    return;

  VLOG(2) << "GCCacheStore: Enter GC: object = " << "(" << this
          << "), free recently cached = " << free_recent
          << ", cache size = " << cache_size_
          << ", cache frac = " << cache_fraction
          << ", cache limit = " << cache_limit_ << "\n";

  size_t cache_target = cache_fraction * cache_limit_;
  store_.Reset();
  while (!store_.Done()) {
    State* state = store_.GetMutableState(store_.Value());
    if (cache_size_ > cache_target && state->RefCount() == 0 &&
        (free_recent || !(state->Flags() & kCacheRecent)) &&
        state != current) {
      if (state->Flags() & kCacheInit) {
        size_t size = sizeof(State) + state->NumArcs() * sizeof(Arc);
        CHECK_LE(size, cache_size_);
        cache_size_ -= size;
      }
      store_.Delete();
    } else {
      state->SetFlags(0, kCacheRecent);
      store_.Next();
    }
  }

  if (!free_recent && cache_size_ > cache_target) {   // recurses on recent
    GC(current, true, cache_fraction);
  } else if (cache_target > 0) {                      // widens cache limit
    while (cache_size_ > cache_target) {
      cache_limit_ *= 2;
      cache_target *= 2;
    }
  } else if (cache_size_ > 0) {
    FSTERROR() << "GCCacheStore:GC: Unable to free all cached states";
  }
  VLOG(2) << "GCCacheStore: Exit GC: object = " << "(" << this
          << "), free recently cached = " << free_recent
          << ", cache size = " << cache_size_
          << ", cache frac = " << cache_fraction
          << ", cache limit = " << cache_limit_ << "\n";
}

template <class C> const size_t GCCacheStore<C>::kMinCacheLimit;


// This class is the default cache state and store used by CacheBaseImpl.
// It uses VectorCacheStore for storage decorated by FirstCacheStore
// and GCCacheStore to do (optional) garbage collection.
template <class A>
class DefaultCacheStore
    : public GCCacheStore<FirstCacheStore<
                            VectorCacheStore<CacheState<A> > > > {
 public:
  explicit DefaultCacheStore(const CacheOptions &opts)
      : GCCacheStore<FirstCacheStore<
                       VectorCacheStore<CacheState<A> > > >(opts) { }
};


// This class is used to cache FST elements stored in states of type S
// (see CacheState) with the flags used to indicate what has been
// cached. Use HasStart() HasFinal(), and HasArcs() to determine if
// cached and SetStart(), SetFinal(), AddArc(), (or PushArc() and
// SetArcs()) to cache. Note you must set the final weight even if the
// state is non-final to mark it as cached. The state storage method
// and any garbage collection policy are determined by the cache store C.
// If the store is passed in with the options, CacheBaseImpl takes ownership.
template <class S, class C = DefaultCacheStore<typename S::Arc> >
class CacheBaseImpl : public FstImpl<typename S::Arc> {
 public:
  typedef S State;
  typedef C Store;

  typedef typename State::Arc Arc;
  typedef typename Arc::Weight Weight;
  typedef typename Arc::StateId StateId;

  using FstImpl<Arc>::Type;
  using FstImpl<Arc>::Properties;

  CacheBaseImpl()
      : has_start_(false), cache_start_(kNoStateId), nknown_states_(0),
        min_unexpanded_state_id_(0),
        max_expanded_state_id_(-1),
        cache_gc_(FLAGS_fst_default_cache_gc),
        cache_limit_(FLAGS_fst_default_cache_gc_limit),
        cache_store_(new C(CacheOptions())),
        new_cache_store_(true) { }

  explicit CacheBaseImpl(const CacheOptions &opts)
      : has_start_(false), cache_start_(kNoStateId), nknown_states_(0),
        min_unexpanded_state_id_(0),  max_expanded_state_id_(-1),
        cache_gc_(opts.gc), cache_limit_(opts.gc_limit),
        cache_store_(new C(opts)),
        new_cache_store_(true) { }

  explicit CacheBaseImpl(const CacheImplOptions<C> &opts)
      : has_start_(false), cache_start_(kNoStateId), nknown_states_(0),
        min_unexpanded_state_id_(0),  max_expanded_state_id_(-1),
        cache_gc_(opts.gc), cache_limit_(opts.gc_limit),
        cache_store_(opts.store ? opts.store :
                     new C(CacheOptions(opts.gc, opts.gc_limit))),
        new_cache_store_(!opts.store) { }

  // Preserve gc parameters. If preserve_cache true, also preserves
  // cache data.
  CacheBaseImpl(const CacheBaseImpl<S, C> &impl, bool preserve_cache = false)
      : FstImpl<Arc>(),
        has_start_(false), cache_start_(kNoStateId),
        nknown_states_(0), min_unexpanded_state_id_(0),
        max_expanded_state_id_(-1), cache_gc_(impl.cache_gc_),
        cache_limit_(impl.cache_limit_),
        cache_store_(new C(CacheOptions(cache_gc_, cache_limit_))),
        new_cache_store_(impl.new_cache_store_ || !preserve_cache) {
    if (preserve_cache) {
      *cache_store_ = *impl.cache_store_;
      has_start_ = impl.has_start_;
      cache_start_ = impl.cache_start_;
      nknown_states_ = impl.nknown_states_;
      expanded_states_ = impl.expanded_states_;
      min_unexpanded_state_id_ = impl.min_unexpanded_state_id_;
      max_expanded_state_id_ = impl.max_expanded_state_id_;
    }
  }

  ~CacheBaseImpl() {
    delete cache_store_;
  }

  void SetStart(StateId s) {
    cache_start_ = s;
    has_start_ = true;
    if (s >= nknown_states_)
      nknown_states_ = s + 1;
  }

  void SetFinal(StateId s, Weight w) {
    S *state = cache_store_->GetMutableState(s);
    state->SetFinal(w);
    int32 flags = kCacheFinal | kCacheRecent;
    state->SetFlags(flags, flags);
  }

  // Disabled to ensure PushArc not AddArc is used in existing code
  // TODO(sorenj): re-enable for backing store
#if 0
  // AddArc adds a single arc to state s and does incremental cache
  // book-keeping.  For efficiency, prefer PushArc and SetArcs below
  // when possible.
  void AddArc(StateId s, const Arc &arc) {
    S *state = cache_store_->GetMutableState(s);
    cache_store_->AddArc(state, arc);
    if (arc.nextstate >= nknown_states_)
      nknown_states_ = arc.nextstate + 1;
    SetExpandedState(s);
    int32 flags = kCacheArcs | kCacheRecent;
    state->SetFlags(flags, flags);
  }
#endif

  // Adds a single arc to state s but delays cache book-keeping.
  // SetArcs must be called when all PushArc calls at a state are
  // complete.  Do not mix with calls to AddArc.
  void PushArc(StateId s, const Arc &arc) {
    S *state = cache_store_->GetMutableState(s);
    state->PushArc(arc);
  }

  // Marks arcs of state s as cached and does cache book-keeping after all
  // calls to PushArc have been completed.  Do not mix with calls to AddArc.
  void SetArcs(StateId s) {
    S *state = cache_store_->GetMutableState(s);
    cache_store_->SetArcs(state);
    size_t narcs = state->NumArcs();
    for (size_t a = 0; a < narcs; ++a) {
      const Arc &arc = state->GetArc(a);
      if (arc.nextstate >= nknown_states_)
        nknown_states_ = arc.nextstate + 1;
    }
    SetExpandedState(s);
    int32 flags = kCacheArcs | kCacheRecent;
    state->SetFlags(flags, flags);
  }

  void ReserveArcs(StateId s, size_t n) {
    S *state = cache_store_->GetMutableState(s);
    state->ReserveArcs(n);
  }

  void DeleteArcs(StateId s) {
    S *state = cache_store_->GetMutableState(s);
    cache_store_->DeleteArcs(state);
  }

  void DeleteArcs(StateId s, size_t n) {
    S *state = cache_store_->GetMutableState(s);
    cache_store_->DeleteArcs(state, n);
  }

  void Clear() {
    nknown_states_ = 0;
    min_unexpanded_state_id_ = 0;
    max_expanded_state_id_ = -1;
    has_start_ = false;
    cache_start_ = kNoStateId;
    cache_store_->Clear();
  }

  // Is the start state cached?
  bool HasStart() const {
    if (!has_start_ && Properties(kError))
      has_start_ = true;
    return has_start_;
  }

  // Is the final weight of state s cached?
  bool HasFinal(StateId s) const {
    const S *state = cache_store_->GetState(s);
    if (state && state->Flags() & kCacheFinal) {
      state->SetFlags(kCacheRecent, kCacheRecent);
      return true;
    } else {
      return false;
    }
  }

  // Are arcs of state s cached?
  bool HasArcs(StateId s) const {
    const S *state = cache_store_->GetState(s);
    if (state && state->Flags() & kCacheArcs) {
      state->SetFlags(kCacheRecent, kCacheRecent);
      return true;
    } else {
      return false;
    }
  }

  StateId Start() const {
    return cache_start_;
  }

  Weight Final(StateId s) const {
    const S *state = cache_store_->GetState(s);
    return state->Final();
  }

  size_t NumArcs(StateId s) const {
    const S *state = cache_store_->GetState(s);
    return state->NumArcs();
  }

  size_t NumInputEpsilons(StateId s) const {
    const S *state = cache_store_->GetState(s);
    return state->NumInputEpsilons();
  }

  size_t NumOutputEpsilons(StateId s) const {
    const S *state = cache_store_->GetState(s);
    return state->NumOutputEpsilons();
  }

  // Provides information needed for generic arc iterator.
  void InitArcIterator(StateId s, ArcIteratorData<Arc> *data) const {
    const S *state = cache_store_->GetState(s);
    data->base = 0;
    data->narcs = state->NumArcs();
    data->arcs = state->Arcs();
    data->ref_count = state->MutableRefCount();
    state->IncrRefCount();
  }

  // Number of known states.
  StateId NumKnownStates() const { return nknown_states_; }

  // Update number of known states taking in account the existence of state s.
  void UpdateNumKnownStates(StateId s) {
    if (s >= nknown_states_)
      nknown_states_ = s + 1;
  }

  // Finds the mininum never-expanded state Id
  StateId MinUnexpandedState() const {
    while (min_unexpanded_state_id_ <= max_expanded_state_id_ &&
           ExpandedState(min_unexpanded_state_id_))
      ++min_unexpanded_state_id_;
    return min_unexpanded_state_id_;
  }

  // Returns maxinum ever-expanded state Id
  StateId MaxExpandedState() const {
    return max_expanded_state_id_;
  }

  void SetExpandedState(StateId s) {
    if (s > max_expanded_state_id_)
      max_expanded_state_id_ = s;
    if (s < min_unexpanded_state_id_)
      return;
    if (s == min_unexpanded_state_id_)
      ++min_unexpanded_state_id_;
    if (cache_gc_ || cache_limit_ == 0) {
      while (expanded_states_.size() <= s)
        expanded_states_.push_back(false);
      expanded_states_[s] = true;
    }
  }

  bool ExpandedState(StateId s) const {
    if (cache_gc_ || cache_limit_ == 0) {
      return expanded_states_[s];
    } else if (new_cache_store_) {
      return cache_store_->GetState(s) != 0;
    } else {
      // If the cache was not created by this class (but provided as opt),
      // then the cached state needs to be inspected to update nknown_states_.
      return false;
    }
  }

  const C *GetCacheStore() const { return cache_store_; }
  C *GetCacheStore() { return cache_store_; }

  // Caching on/off switch, limit and size accessors.
  bool GetCacheGc() const { return cache_gc_; }
  size_t GetCacheLimit() const { return cache_limit_; }

 private:
  mutable bool has_start_;                   // Is the start state cached?
  StateId cache_start_;                      // State Id of start state
  StateId nknown_states_;                    // # of known states
  vector<bool> expanded_states_;             // states that have been expanded
  mutable StateId min_unexpanded_state_id_;  // minimum never-expanded state Id
  mutable StateId max_expanded_state_id_;    // maximum ever-expanded state Id
  bool cache_gc_;                            // GC enabled
  size_t cache_limit_;                       // # of bytes allowed before GC
  Store *cache_store_;                       // store of cached states
  bool new_cache_store_;                     // store was created by class

  void operator=(const CacheBaseImpl<S, C> &impl);    // disallow
};

// A CacheBaseImpl with the default cache state type.
template <class A>
class CacheImpl : public CacheBaseImpl< CacheState<A> > {
 public:
  typedef CacheState<A> State;

  CacheImpl() {}

  explicit CacheImpl(const CacheOptions &opts)
      : CacheBaseImpl< CacheState<A> >(opts) {}

  CacheImpl(const CacheImpl<A> &impl, bool preserve_cache = false)
      : CacheBaseImpl<State>(impl, preserve_cache) {}

 private:
  void operator=(const CacheImpl<State> &impl);    // disallow
};


// Use this to make a state iterator for a CacheBaseImpl-derived Fst,
// which must have types 'Arc' and 'Store' defined.  Note this iterator only
// returns those states reachable from the initial state, so consider
// implementing a class-specific one.
template <class F>
class CacheStateIterator : public StateIteratorBase<typename F::Arc> {
 public:
  typedef typename F::Arc Arc;
  typedef typename F::Store Store;
  typedef typename Arc::StateId StateId;
  typedef typename Store::State State;
  typedef CacheBaseImpl<State, Store> Impl;

  CacheStateIterator(const F &fst, Impl *impl)
      : fst_(fst), impl_(impl), s_(0) {
        fst_.Start();  // force start state
      }

  bool Done() const {
    if (s_ < impl_->NumKnownStates())
      return false;
    for (StateId u = impl_->MinUnexpandedState();
         u < impl_->NumKnownStates();
         u = impl_->MinUnexpandedState()) {
      // force state expansion
      ArcIterator<F> aiter(fst_, u);
      aiter.SetFlags(kArcValueFlags, kArcValueFlags | kArcNoCache);
      for (; !aiter.Done(); aiter.Next())
        impl_->UpdateNumKnownStates(aiter.Value().nextstate);
      impl_->SetExpandedState(u);
      if (s_ < impl_->NumKnownStates())
        return false;
    }
    return true;
  }

  StateId Value() const { return s_; }

  void Next() { ++s_; }

  void Reset() { s_ = 0; }

 private:
  // This allows base class virtual access to non-virtual derived-
  // class members of the same name. It makes the derived class more
  // efficient to use but unsafe to further derive.
  virtual bool Done_() const { return Done(); }
  virtual StateId Value_() const { return Value(); }
  virtual void Next_() { Next(); }
  virtual void Reset_() { Reset(); }

  const F &fst_;
  Impl *impl_;
  StateId s_;
};


// Use this to make an arc iterator for a CacheBaseImpl-derived Fst,
// which must have types 'Arc' and 'State' defined.
template <class F>
class CacheArcIterator {
 public:
  typedef typename F::Arc Arc;
  typedef typename F::Store Store;
  typedef typename Arc::StateId StateId;
  typedef typename Store::State State;
  typedef CacheBaseImpl<State, Store> Impl;

  CacheArcIterator(Impl *impl, StateId s) : i_(0) {
    state_ = impl->GetCacheStore()->GetMutableState(s);
    state_->IncrRefCount();
  }

  ~CacheArcIterator() { state_->DecrRefCount();  }

  bool Done() const { return i_ >= state_->NumArcs(); }

  const Arc& Value() const { return state_->GetArc(i_); }

  void Next() { ++i_; }

  size_t Position() const { return i_; }

  void Reset() { i_ = 0; }

  void Seek(size_t a) { i_ = a; }

  uint32 Flags() const {
    return kArcValueFlags;
  }

  void SetFlags(uint32 flags, uint32 mask) {}

 private:
  const State *state_;
  size_t i_;

  DISALLOW_COPY_AND_ASSIGN(CacheArcIterator);
};

// Use this to make a mutable arc iterator for a CacheBaseImpl-derived Fst,
// which must have types 'Arc' and 'Store' defined.
template <class F>
class CacheMutableArcIterator
    : public MutableArcIteratorBase<typename F::Arc> {
 public:
  typedef typename F::Arc Arc;
  typedef typename F::Store Store;
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Weight Weight;
  typedef typename Store::State State;
  typedef CacheBaseImpl<State, Store> Impl;

  // You will need to call MutateCheck() in the constructor.
  CacheMutableArcIterator(Impl *impl, StateId s) : i_(0), s_(s), impl_(impl) {
    state_ = impl_->GetCacheStore()->GetMutableState(s_);
    state_->IncrRefCount();
  }

  ~CacheMutableArcIterator() {
    state_->DecrRefCount();
  }

  bool Done() const { return i_ >= state_->NumArcs(); }

  const Arc& Value() const { return state_->GetArc(i_); }

  void Next() { ++i_; }

  size_t Position() const { return i_; }

  void Reset() { i_ = 0; }

  void Seek(size_t a) { i_ = a; }

  void SetValue(const Arc& arc) { state_->SetArc(arc, i_); }

  uint32 Flags() const {
    return kArcValueFlags;
  }

  void SetFlags(uint32 f, uint32 m) {}

 private:
  virtual bool Done_() const { return Done(); }
  virtual const Arc& Value_() const { return Value(); }
  virtual void Next_() { Next(); }
  virtual size_t Position_() const { return Position(); }
  virtual void Reset_() { Reset(); }
  virtual void Seek_(size_t a) { Seek(a); }
  virtual void SetValue_(const Arc &a) { SetValue(a); }
  uint32 Flags_() const { return Flags(); }
  void SetFlags_(uint32 f, uint32 m) { SetFlags(f, m); }

  size_t i_;
  StateId s_;
  Impl *impl_;
  State *state_;

  DISALLOW_COPY_AND_ASSIGN(CacheMutableArcIterator);
};

}  // namespace fst

#endif  // FST_LIB_CACHE_H__
