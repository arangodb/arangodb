// shortest-path.h

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
// Functions to find shortest paths in an FST.

#ifndef FST_LIB_SHORTEST_PATH_H__
#define FST_LIB_SHORTEST_PATH_H__

#include <functional>
#include <utility>
using std::pair; using std::make_pair;
#include <vector>
using std::vector;

#include <fst/cache.h>
#include <fst/determinize.h>
#include <fst/queue.h>
#include <fst/shortest-distance.h>
#include <fst/test-properties.h>


namespace fst {

template <class Arc, class Queue, class ArcFilter>
struct ShortestPathOptions
    : public ShortestDistanceOptions<Arc, Queue, ArcFilter> {
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Weight Weight;
  size_t nshortest;   // return n-shortest paths
  bool unique;        // only return paths with distinct input strings
  bool has_distance;  // distance vector already contains the
                      // shortest distance from the initial state
  bool first_path;    // Single shortest path stops after finding the first
                      // path to a final state. That path is the shortest path
                      // only when using the ShortestFirstQueue and
                      // only when all the weights in the FST are between
                      // One() and Zero() according to NaturalLess.
  Weight weight_threshold;   // pruning weight threshold.
  StateId state_threshold;   // pruning state threshold.

  ShortestPathOptions(Queue *q, ArcFilter filt, size_t n = 1, bool u = false,
                      bool hasdist = false, float d = kDelta,
                      bool fp = false, Weight w = Weight::Zero(),
                      StateId s = kNoStateId)
      : ShortestDistanceOptions<Arc, Queue, ArcFilter>(q, filt, kNoStateId, d),
        nshortest(n), unique(u), has_distance(hasdist), first_path(fp),
        weight_threshold(w), state_threshold(s) {}
};

// Helper function for SingleShortestPath, builds the shortest path as
// a left to right machine backwards from the best final state.
// Not normally called by users, see ShortestPath.
// Takes the 'ifst' passed to SingleShortestPath and the 'parent' vector
// and 'f_parent' returned by that function.  Builds the result into the
// provided 'ofst'.
template<class Arc>
void SingleShortestPathBacktrace(
    const Fst<Arc> &ifst,
    MutableFst<Arc> *ofst,
    const vector<pair<typename Arc::StateId, size_t>>& parent,
    typename Arc::StateId f_parent) {
  ofst->DeleteStates();
  ofst->SetInputSymbols(ifst.InputSymbols());
  ofst->SetOutputSymbols(ifst.OutputSymbols());

  typedef typename Arc::StateId StateId;
  StateId s_p = kNoStateId, d_p = kNoStateId;
  for (StateId s = f_parent, d = kNoStateId;
       s != kNoStateId;
       d = s, s = parent[s].first) {
    d_p = s_p;
    s_p = ofst->AddState();
    if (d == kNoStateId) {
      ofst->SetFinal(s_p, ifst.Final(f_parent));
    } else {
      ArcIterator<Fst<Arc>> aiter(ifst, s);
      aiter.Seek(parent[d].second);
      Arc arc = aiter.Value();
      arc.nextstate = d_p;
      ofst->AddArc(s_p, arc);
    }
  }
  ofst->SetStart(s_p);
  if (ifst.Properties(kError, false)) ofst->SetProperties(kError, kError);
  ofst->SetProperties(
      ShortestPathProperties(ofst->Properties(kFstProperties, false)),
      kFstProperties);
}

// Helper function for SingleShortestPath, builds a tree of shortest paths
// to every final state in the input fst.  Takes the 'ifst' and 'parent'
// values computed by SingleShortestPath and builds into 'ofst' the subtree
// of 'ifst' that consists only of the best paths to all Final states.
template<class Arc>
void SingleShortestTree(
    const Fst<Arc> &ifst,
    MutableFst<Arc> *ofst,
    const vector<pair<typename Arc::StateId, size_t>>& parent) {
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Weight Weight;
  ofst->DeleteStates();
  ofst->SetInputSymbols(ifst.InputSymbols());
  ofst->SetOutputSymbols(ifst.OutputSymbols());
  ofst->SetStart(ifst.Start());
  for (StateIterator<Fst<Arc>> siter(ifst); !siter.Done(); siter.Next()) {
    ofst->AddState();
    ofst->SetFinal(siter.Value(), ifst.Final(siter.Value()));
  }
  typename vector<pair<StateId, size_t>>::const_iterator it;
  for (it = parent.begin(); it != parent.end(); ++it) {
    if (it->first != kNoStateId && it->second != -1) {
      ArcIterator<Fst<Arc>> aiter(ifst, it->first);
      aiter.Seek(it->second);
      ofst->AddArc(it->first, aiter.Value());
    }
  }
  if (ifst.Properties(kError, false)) ofst->SetProperties(kError, kError);
  ofst->SetProperties(
    ShortestPathProperties(ofst->Properties(kFstProperties, false), true),
    kFstProperties);
}

// Shortest-path algorithm: normally not called directly; prefer
// 'ShortestPath' below with n=1. 'ofst' contains the shortest path in
// 'ifst'. 'distance' returns the shortest distances from the source
// state to each state in 'ifst'. 'opts' is used to specify options
// such as the queue discipline, the arc filter and delta.
// The super_final argument is an output parameter indicating the
// final state, and the parent argument is used for the storage of
// the backtrace path for each state 1 to n, (i.e. the best previous
// state and the arc that transition to state n.)
//
// The shortest path is the lowest weight path w.r.t. the natural
// semiring order.
//
// The weights need to be right distributive and have the path (kPath)
// property.  Returns false if an error is encountered.
template<class Arc, class Queue, class ArcFilter>
bool SingleShortestPath(
    const Fst<Arc> &ifst,
    vector<typename Arc::Weight> *distance,
    ShortestPathOptions<Arc, Queue, ArcFilter> &opts,  // NO_LINT
    typename Arc::StateId *f_parent,
    vector<pair<typename Arc::StateId, size_t>> *parent) {
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Weight Weight;

  parent->clear();
  *f_parent = kNoStateId;

  if (ifst.Start() == kNoStateId) {
    return true;
  }

  vector<bool> enqueued;

  Queue *state_queue = opts.state_queue;
  StateId source = opts.source == kNoStateId ? ifst.Start() : opts.source;
  Weight f_distance = Weight::Zero();

  distance->clear();
  state_queue->Clear();
  if (opts.nshortest != 1) {
    FSTERROR() << "SingleShortestPath: for nshortest > 1, use ShortestPath"
               << " instead";
    return false;
  }
  if (opts.weight_threshold != Weight::Zero() ||
      opts.state_threshold != kNoStateId) {
    FSTERROR() <<
        "SingleShortestPath: weight and state thresholds not applicable";
    return false;
  }
  if ((Weight::Properties() & (kPath | kRightSemiring))
      != (kPath | kRightSemiring)) {
    FSTERROR() << "SingleShortestPath: Weight needs to have the path"
               << " property and be right distributive: " << Weight::Type();
    return false;
  }
  while (distance->size() < source) {
    distance->push_back(Weight::Zero());
    enqueued.push_back(false);
    parent->push_back(std::make_pair(kNoStateId, -1));
  }
  distance->push_back(Weight::One());
  parent->push_back(std::make_pair(kNoStateId, -1));
  state_queue->Enqueue(source);
  enqueued.push_back(true);

  while (!state_queue->Empty()) {
    StateId s = state_queue->Head();
    state_queue->Dequeue();
    enqueued[s] = false;
    Weight sd = (*distance)[s];
    if (ifst.Final(s) != Weight::Zero()) {
      Weight w = Times(sd, ifst.Final(s));
      if (f_distance != Plus(f_distance, w)) {
        f_distance = Plus(f_distance, w);
        *f_parent = s;
      }
      if (!f_distance.Member()) {
        return false;
      }
      if (opts.first_path)
        break;
    }
    for (ArcIterator< Fst<Arc> > aiter(ifst, s);
         !aiter.Done();
         aiter.Next()) {
      const Arc &arc = aiter.Value();
      while (distance->size() <= arc.nextstate) {
        distance->push_back(Weight::Zero());
        enqueued.push_back(false);
        parent->push_back(std::make_pair(kNoStateId, -1));
      }
      Weight &nd = (*distance)[arc.nextstate];
      Weight w = Times(sd, arc.weight);
      if (nd != Plus(nd, w)) {
        nd = Plus(nd, w);
        if (!nd.Member()) {
          return false;
        }
        (*parent)[arc.nextstate] = std::make_pair(s, aiter.Position());
        if (!enqueued[arc.nextstate]) {
          state_queue->Enqueue(arc.nextstate);
          enqueued[arc.nextstate] = true;
        } else {
          state_queue->Update(arc.nextstate);
        }
      }
    }
  }
  return true;
}


template <class S, class W>
class ShortestPathCompare {
 public:
  typedef S StateId;
  typedef W Weight;
  typedef pair<StateId, Weight> Pair;

  ShortestPathCompare(const vector<Pair> &pairs,
                      const vector<Weight> &distance,
                      StateId sfinal, float d)
      : pairs_(pairs), distance_(distance), superfinal_(sfinal), delta_(d)  {}

  bool operator()(const StateId x, const StateId y) const {
    const Pair &px = pairs_[x];
    const Pair &py = pairs_[y];
    Weight dx = px.first == superfinal_ ? Weight::One() :
        px.first < distance_.size() ? distance_[px.first] : Weight::Zero();
    Weight dy = py.first == superfinal_ ? Weight::One() :
        py.first < distance_.size() ? distance_[py.first] : Weight::Zero();
    Weight wx = Times(dx, px.second);
    Weight wy = Times(dy, py.second);
    // Penalize complete paths to ensure correct results with inexact weights.
    // This forms a strict weak order so long as ApproxEqual(a, b) =>
    // ApproxEqual(a, c) for all c s.t. less_(a, c) && less_(c, b).
    if (px.first == superfinal_ && py.first != superfinal_) {
      return less_(wy, wx) || ApproxEqual(wx, wy, delta_);
    } else if (py.first == superfinal_ && px.first != superfinal_) {
      return less_(wy, wx) && !ApproxEqual(wx, wy, delta_);
    } else {
      return less_(wy, wx);
    }
  }

 private:
  const vector<Pair> &pairs_;
  const vector<Weight> &distance_;
  StateId superfinal_;
  float delta_;
  NaturalLess<Weight> less_;
};


// N-Shortest-path algorithm: implements the core n-shortest path
// algorithm. The output is built REVERSED. See below for versions with
// more options and not reversed.
//
// 'ofst' contains the REVERSE of 'n'-shortest paths in 'ifst'.
// 'distance' must contain the shortest distance from each state to a final
// state in 'ifst'. 'delta' is the convergence delta.
//
// The n-shortest paths are the n-lowest weight paths w.r.t. the
// natural semiring order. The single path that can be read from the
// ith of at most n transitions leaving the initial state of 'ofst' is
// the ith shortest path. Disregarding the initial state and initial
// transitions, the n-shortest paths, in fact, form a tree rooted at
// the single final state.
//
// The weights need to be left and right distributive (kSemiring) and
// have the path (kPath) property.
//
// Arc weights must satisfy the property that the sum of the weights of one or
// more paths from some state S to T is never Zero(). In particular, arc weights
// are never Zero().
//
// The algorithm is from Mohri and Riley, "An Efficient Algorithm for
// the n-best-strings problem", ICSLP 2002. The algorithm relies on
// the shortest-distance algorithm. There are some issues with the
// pseudo-code as written in the paper (viz., line 11).
//
// IMPLEMENTATION NOTE: The input fst 'ifst' can be a delayed fst and
// and at any state in its expansion the values of distance vector need only
// be defined at that time for the states that are known to exist.
template<class Arc, class RevArc>
void NShortestPath(const Fst<RevArc> &ifst,
                   MutableFst<Arc> *ofst,
                   const vector<typename Arc::Weight> &distance,
                   size_t n,
                   float delta = kDelta,
                   typename Arc::Weight weight_threshold = Arc::Weight::Zero(),
                   typename Arc::StateId state_threshold = kNoStateId) {
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Weight Weight;
  typedef pair<StateId, Weight> Pair;
  typedef typename RevArc::Weight RevWeight;

  if (n <= 0) return;
  if ((Weight::Properties() & (kPath | kSemiring)) != (kPath | kSemiring)) {
    FSTERROR() << "NShortestPath: Weight needs to have the "
                 << "path property and be distributive: "
                 << Weight::Type();
    ofst->SetProperties(kError, kError);
    return;
  }
  ofst->DeleteStates();
  ofst->SetInputSymbols(ifst.InputSymbols());
  ofst->SetOutputSymbols(ifst.OutputSymbols());
  // Each state in 'ofst' corresponds to a path with weight w from the
  // initial state of 'ifst' to a state s in 'ifst', that can be
  // characterized by a pair (s,w).  The vector 'pairs' maps each
  // state in 'ofst' to the corresponding pair maps states in OFST to
  // the corresponding pair (s,w).
  vector<Pair> pairs;
  // The supefinal state is denoted by -1, 'compare' knows that the
  // distance from 'superfinal' to the final state is 'Weight::One()',
  // hence 'distance[superfinal]' is not needed.
  StateId superfinal = -1;
  ShortestPathCompare<StateId, Weight>
    compare(pairs, distance, superfinal, delta);
  vector<StateId> heap;
  // 'r[s + 1]', 's' state in 'fst', is the number of states in 'ofst'
  // which corresponding pair contains 's' ,i.e. , it is number of
  // paths computed so far to 's'. Valid for 's == -1' (superfinal).
  vector<int> r;
  NaturalLess<Weight> less;
  if (ifst.Start() == kNoStateId ||
      distance.size() <= ifst.Start() ||
      distance[ifst.Start()] == Weight::Zero() ||
      less(weight_threshold, Weight::One()) ||
      state_threshold == 0) {
    if (ifst.Properties(kError, false)) ofst->SetProperties(kError, kError);
    return;
  }
  ofst->SetStart(ofst->AddState());
  StateId final = ofst->AddState();
  ofst->SetFinal(final, Weight::One());
  while (pairs.size() <= final)
    pairs.push_back(Pair(kNoStateId, Weight::Zero()));
  pairs[final] = Pair(ifst.Start(), Weight::One());
  heap.push_back(final);
  Weight limit = Times(distance[ifst.Start()], weight_threshold);

  while (!heap.empty()) {
    std::pop_heap(heap.begin(), heap.end(), compare);
    StateId state = heap.back();
    Pair p = pairs[state];
    heap.pop_back();
    Weight d = p.first == superfinal ? Weight::One() :
        p.first < distance.size() ? distance[p.first] : Weight::Zero();

    if (less(limit, Times(d, p.second)) ||
        (state_threshold != kNoStateId &&
         ofst->NumStates() >= state_threshold))
      continue;

    while (r.size() <= p.first + 1) r.push_back(0);
    ++r[p.first + 1];
    if (p.first == superfinal)
      ofst->AddArc(ofst->Start(), Arc(0, 0, Weight::One(), state));
    if ((p.first == superfinal) && (r[p.first + 1] == n)) break;
    if (r[p.first + 1] > n) continue;
    if (p.first == superfinal) continue;

    for (ArcIterator< Fst<RevArc> > aiter(ifst, p.first);
         !aiter.Done();
         aiter.Next()) {
      const RevArc &rarc = aiter.Value();
      Arc arc(rarc.ilabel, rarc.olabel, rarc.weight.Reverse(), rarc.nextstate);
      Weight w = Times(p.second, arc.weight);
      StateId next = ofst->AddState();
      pairs.push_back(Pair(arc.nextstate, w));
      arc.nextstate = state;
      ofst->AddArc(next, arc);
      heap.push_back(next);
      std::push_heap(heap.begin(), heap.end(), compare);
    }

    Weight finalw = ifst.Final(p.first).Reverse();
    if (finalw != Weight::Zero()) {
      Weight w = Times(p.second, finalw);
      StateId next = ofst->AddState();
      pairs.push_back(Pair(superfinal, w));
      ofst->AddArc(next, Arc(0, 0, finalw, state));
      heap.push_back(next);
      std::push_heap(heap.begin(), heap.end(), compare);
    }
  }
  Connect(ofst);
  if (ifst.Properties(kError, false)) ofst->SetProperties(kError, kError);
  ofst->SetProperties(
      ShortestPathProperties(ofst->Properties(kFstProperties, false)),
      kFstProperties);
}


// N-Shortest-path algorithm:  this version allow fine control
// via the options argument. See below for a simpler interface.
//
// 'ofst' contains the n-shortest paths in 'ifst'. 'distance' returns
// the shortest distances from the source state to each state in
// 'ifst'. 'opts' is used to specify options such as the number of
// paths to return, whether they need to have distinct input
// strings, the queue discipline, the arc filter and the convergence
// delta.
//
// The n-shortest paths are the n-lowest weight paths w.r.t. the
// natural semiring order. The single path that can be read from the
// ith of at most n transitions leaving the initial state of 'ofst' is
// the ith shortest path. Disregarding the initial state and initial
// transitions, The n-shortest paths, in fact, form a tree rooted at
// the single final state.

// The weights need to be right distributive and have the path (kPath)
// property. They need to be left distributive as well for nshortest
// > 1.
//
// The algorithm is from Mohri and Riley, "An Efficient Algorithm for
// the n-best-strings problem", ICSLP 2002. The algorithm relies on
// the shortest-distance algorithm. There are some issues with the
// pseudo-code as written in the paper (viz., line 11).
template<class Arc, class Queue, class ArcFilter>
void ShortestPath(
    const Fst<Arc> &ifst, MutableFst<Arc> *ofst,
    vector<typename Arc::Weight> *distance,
    ShortestPathOptions<Arc, Queue, ArcFilter> &opts) {  // NO_LINT
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Weight Weight;
  typedef ReverseArc<Arc> ReverseArc;

  size_t n = opts.nshortest;
  if (n == 1) {
    vector<pair<StateId, size_t>> parent;
    StateId f_parent;
    if (SingleShortestPath(ifst, distance, opts, &f_parent, &parent)) {
      SingleShortestPathBacktrace(ifst, ofst, parent, f_parent);
    } else {
      ofst->SetProperties(kError, kError);
    }
    return;
  }
  if (n <= 0) return;
  if ((Weight::Properties() & (kPath | kSemiring)) != (kPath | kSemiring)) {
    FSTERROR() << "ShortestPath: n-shortest: Weight needs to have the "
               << "path property and be distributive: "
               << Weight::Type();
    ofst->SetProperties(kError, kError);
    return;
  }
  if (!opts.has_distance) {
    ShortestDistance(ifst, distance, opts);
    if (distance->size() == 1 && !(*distance)[0].Member()) {
      ofst->SetProperties(kError, kError);
      return;
    }
  }
  // Algorithm works on the reverse of 'fst' : 'rfst', 'distance' is
  // the distance to the final state in 'rfst', 'ofst' is built as the
  // reverse of the tree of n-shortest path in 'rfst'.
  VectorFst<ReverseArc> rfst;
  Reverse(ifst, &rfst);
  Weight d = Weight::Zero();
  for (ArcIterator< VectorFst<ReverseArc> > aiter(rfst, 0);
       !aiter.Done(); aiter.Next()) {
    const ReverseArc &arc = aiter.Value();
    StateId s = arc.nextstate - 1;
    if (s < distance->size())
      d = Plus(d, Times(arc.weight.Reverse(), (*distance)[s]));
  }
  distance->insert(distance->begin(), d);

  if (!opts.unique) {
    NShortestPath(rfst, ofst, *distance, n, opts.delta,
                  opts.weight_threshold, opts.state_threshold);
  } else {
    vector<Weight> ddistance;
    DeterminizeFstOptions<ReverseArc> dopts(opts.delta);
    DeterminizeFst<ReverseArc> dfst(rfst, distance, &ddistance, dopts);
    NShortestPath(dfst, ofst, ddistance, n, opts.delta,
                  opts.weight_threshold, opts.state_threshold);
  }
  distance->erase(distance->begin());
}


// Shortest-path algorithm: simplified interface. See above for a
// version that allows finer control.
//
// 'ofst' contains the 'n'-shortest paths in 'ifst'. The queue
// discipline is automatically selected. When 'unique' == true, only
// paths with distinct input labels are returned.
//
// The n-shortest paths are the n-lowest weight paths w.r.t. the
// natural semiring order. The single path that can be read from the
// ith of at most n transitions leaving the initial state of 'ofst' is
// the ith best path.
//
// The weights need to be right distributive and have the path
// (kPath) property.
template<class Arc>
void ShortestPath(const Fst<Arc> &ifst, MutableFst<Arc> *ofst,
                  size_t n = 1, bool unique = false,
                  bool first_path = false,
                  typename Arc::Weight weight_threshold = Arc::Weight::Zero(),
                  typename Arc::StateId state_threshold = kNoStateId) {
  vector<typename Arc::Weight> distance;
  AnyArcFilter<Arc> arc_filter;
  AutoQueue<typename Arc::StateId> state_queue(ifst, &distance, arc_filter);
  ShortestPathOptions< Arc, AutoQueue<typename Arc::StateId>,
      AnyArcFilter<Arc> > opts(&state_queue, arc_filter, n, unique, false,
                               kDelta, first_path, weight_threshold,
                               state_threshold);
  ShortestPath(ifst, ofst, &distance, opts);
}

}  // namespace fst

#endif  // FST_LIB_SHORTEST_PATH_H__
