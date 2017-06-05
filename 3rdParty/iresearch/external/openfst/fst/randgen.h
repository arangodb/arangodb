// randgen.h

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
// Classes and functions to generate random paths through an FST.

#ifndef FST_LIB_RANDGEN_H__
#define FST_LIB_RANDGEN_H__

#include <cmath>
#include <cstdlib>
#include <ctime>
#include <map>

#include <fst/accumulator.h>
#include <fst/cache.h>
#include <fst/dfs-visit.h>
#include <fst/mutable-fst.h>

namespace fst {

//
// ARC SELECTORS - these function objects are used to select a random
// transition to take from an FST's state. They should return a number
// N s.t. 0 <= N <= NumArcs(). If N < NumArcs(), then the N-th
// transition is selected. If N == NumArcs(), then the final weight at
// that state is selected (i.e., the 'super-final' transition is selected).
// It can be assumed these will not be called unless either there
// are transitions leaving the state and/or the state is final.
//

// Randomly selects a transition using the uniform distribution.
template <class A>
struct UniformArcSelector {
  typedef typename A::StateId StateId;
  typedef typename A::Weight Weight;

  UniformArcSelector(int seed = time(0)) { srand(seed); }

  size_t operator()(const Fst<A> &fst, StateId s) const {
    double r = rand()/(RAND_MAX + 1.0);
    size_t n = fst.NumArcs(s);
    if (fst.Final(s) != Weight::Zero())
      ++n;
    return static_cast<size_t>(r * n);
  }
};


// Randomly selects a transition w.r.t. the weights treated as negative
// log probabilities after normalizing for the total weight leaving
// the state. Weight::zero transitions are disregarded.
// Assumes Weight::Value() accesses the floating point
// representation of the weight.
template <class A>
class LogProbArcSelector {
 public:
  typedef typename A::StateId StateId;
  typedef typename A::Weight Weight;

  LogProbArcSelector(int seed = time(0)) { srand(seed); }

  size_t operator()(const Fst<A> &fst, StateId s) const {
    // Find total weight leaving state
    double sum = 0.0;
    for (ArcIterator< Fst<A> > aiter(fst, s); !aiter.Done();
         aiter.Next()) {
      const A &arc = aiter.Value();
      sum += exp(-to_log_weight_(arc.weight).Value());
    }
    sum += exp(-to_log_weight_(fst.Final(s)).Value());

    double r = rand()/(RAND_MAX + 1.0);
    double p = 0.0;
    int n = 0;
    for (ArcIterator< Fst<A> > aiter(fst, s); !aiter.Done();
         aiter.Next(), ++n) {
      const A &arc = aiter.Value();
      p += exp(-to_log_weight_(arc.weight).Value());
      if (p > r * sum) return n;
    }
    return n;
  }

 private:
  WeightConvert<Weight, Log64Weight> to_log_weight_;
};

// Convenience definitions
typedef LogProbArcSelector<StdArc> StdArcSelector;
typedef LogProbArcSelector<LogArc> LogArcSelector;


// Same as LogProbArcSelector but use CacheLogAccumulator to cache
// the cummulative weight computations.
template <class A>
class FastLogProbArcSelector : public LogProbArcSelector<A> {
 public:
  typedef typename A::StateId StateId;
  typedef typename A::Weight Weight;
  using LogProbArcSelector<A>::operator();

  FastLogProbArcSelector(int seed = time(0))
      : LogProbArcSelector<A>(seed),
        seed_(seed) {}

  size_t operator()(const Fst<A> &fst, StateId s,
                    CacheLogAccumulator<A> *accumulator) const {
    accumulator->SetState(s);
    ArcIterator< Fst<A> > aiter(fst, s);
    // Find total weight leaving state
    double sum = to_log_weight_(accumulator->Sum(fst.Final(s), &aiter, 0,
                                                 fst.NumArcs(s))).Value();
    double r = -log(rand()/(RAND_MAX + 1.0));
    return accumulator->LowerBound(r + sum, &aiter);
  }

  int Seed() const { return seed_; }
 private:
  int seed_;
  WeightConvert<Weight, Log64Weight> to_log_weight_;
};

// Random path state info maintained by RandGenFst and passed to samplers.
template <typename A>
struct RandState {
  typedef typename A::StateId StateId;

  StateId state_id;              // current input FST state
  size_t nsamples;               // # of samples to be sampled at this state
  size_t length;                 // length of path to this random state
  size_t select;                 // previous sample arc selection
  const RandState<A> *parent;    // previous random state on this path

  RandState(StateId s, size_t n, size_t l, size_t k, const RandState<A> *p)
      : state_id(s), nsamples(n), length(l), select(k), parent(p) {}

  RandState()
      : state_id(kNoStateId), nsamples(0), length(0), select(0), parent(0) {}
};

// This class, given an arc selector, samples, with raplacement,
// multiple random transitions from an FST's state. This is a generic
// version with a straight-forward use of the arc selector.
// Specializations may be defined for arc selectors for greater
// efficiency or special behavior.
template <class A, class S>
class ArcSampler {
 public:
  typedef typename A::StateId StateId;
  typedef typename A::Weight Weight;

  // The 'max_length' may be interpreted (including ignored) by a
  // sampler as it chooses. This generic version interprets this literally.
  ArcSampler(const Fst<A> &fst, const S &arc_selector,
             int max_length = INT_MAX)
      : fst_(fst),
        arc_selector_(arc_selector),
        max_length_(max_length) {}

  // Allow updating Fst argument; pass only if changed.
  ArcSampler(const ArcSampler<A, S> &sampler, const Fst<A> *fst = 0)
      : fst_(fst ? *fst : sampler.fst_),
        arc_selector_(sampler.arc_selector_),
        max_length_(sampler.max_length_) {
    Reset();
  }

  // Samples 'rstate.nsamples' from state 'state_id'. The 'rstate.length' is
  // the length of the path to 'rstate'. Returns true if samples were
  // collected.  No samples may be collected if either there are no (including
  // 'super-final') transitions leaving that state or if the
  // 'max_length' has been deemed reached. Use the iterator members to
  // read the samples. The samples will be in their original order.
  bool Sample(const RandState<A> &rstate) {
    sample_map_.clear();
    if ((fst_.NumArcs(rstate.state_id) == 0 &&
         fst_.Final(rstate.state_id) == Weight::Zero()) ||
        rstate.length == max_length_) {
      Reset();
      return false;
    }

    for (size_t i = 0; i < rstate.nsamples; ++i)
      ++sample_map_[arc_selector_(fst_, rstate.state_id)];
    Reset();
    return true;
  }

  // More samples?
  bool Done() const { return sample_iter_ == sample_map_.end(); }

  // Gets the next sample.
  void Next() { ++sample_iter_; }

  // Returns a pair (N, K) where 0 <= N <= NumArcs(s) and 0 < K <= nsamples.
  // If N < NumArcs(s), then the N-th transition is specified.
  // If N == NumArcs(s), then the final weight at that state is
  // specified (i.e., the 'super-final' transition is specified).
  // For the specified transition, K repetitions have been sampled.
  pair<size_t, size_t> Value() const { return *sample_iter_; }

  void Reset() { sample_iter_ = sample_map_.begin(); }

  bool Error() const { return false; }

 private:
  const Fst<A> &fst_;
  const S &arc_selector_;
  int max_length_;

  // Stores (N, K) as described for Value().
  map<size_t, size_t> sample_map_;
  map<size_t, size_t>::const_iterator sample_iter_;

  // disallow
  ArcSampler<A, S> & operator=(const ArcSampler<A, S> &s);
};


// Specialization for FastLogProbArcSelector.
template <class A>
class ArcSampler<A, FastLogProbArcSelector<A> > {
 public:
  typedef FastLogProbArcSelector<A> S;
  typedef typename A::StateId StateId;
  typedef typename A::Weight Weight;
  typedef CacheLogAccumulator<A> C;

  ArcSampler(const Fst<A> &fst, const S &arc_selector, int max_length = INT_MAX)
      : fst_(fst),
        arc_selector_(arc_selector),
        max_length_(max_length),
        accumulator_(new C()) {
    accumulator_->Init(fst);
  }

  ArcSampler(const ArcSampler<A, S> &sampler, const Fst<A> *fst = 0)
      : fst_(fst ? *fst : sampler.fst_),
        arc_selector_(sampler.arc_selector_),
        max_length_(sampler.max_length_) {
    if (fst) {
      accumulator_ = new C();
      accumulator_->Init(*fst);
    } else {  // shallow copy
      accumulator_ = new C(*sampler.accumulator_);
    }
  }

  ~ArcSampler() {
    delete accumulator_;
  }

  bool Sample(const RandState<A> &rstate) {
    sample_map_.clear();
    if ((fst_.NumArcs(rstate.state_id) == 0 &&
         fst_.Final(rstate.state_id) == Weight::Zero()) ||
        rstate.length == max_length_) {
      Reset();
      return false;
    }

    for (size_t i = 0; i < rstate.nsamples; ++i)
      ++sample_map_[arc_selector_(fst_, rstate.state_id, accumulator_)];
    Reset();
    return true;
  }

  bool Done() const { return sample_iter_ == sample_map_.end(); }
  void Next() { ++sample_iter_; }
  pair<size_t, size_t> Value() const { return *sample_iter_; }
  void Reset() { sample_iter_ = sample_map_.begin(); }

  bool Error() const { return accumulator_->Error(); }

 private:
  const Fst<A> &fst_;
  const S &arc_selector_;
  int max_length_;

  // Stores (N, K) as described for Value().
  map<size_t, size_t> sample_map_;
  map<size_t, size_t>::const_iterator sample_iter_;
  C *accumulator_;

  // disallow
  ArcSampler<A, S> & operator=(const ArcSampler<A, S> &s);
};


// Options for random path generation with RandGenFst. The template argument
// is an arc sampler, typically class 'ArcSampler' above.  Ownership of
// the sampler is taken by RandGenFst.
template <class S>
struct RandGenFstOptions : public CacheOptions {
  S *arc_sampler;            // How to sample transitions at a state
  size_t npath;              // # of paths to generate
  bool weighted;             // Output tree weighted by path count; o.w.
                             // output unweighted DAG
  bool remove_total_weight;  // Remove total weight when output is weighted.

  RandGenFstOptions(const CacheOptions &copts, S *samp,
                    size_t n = 1, bool w = true, bool rw = false)
      : CacheOptions(copts),
        arc_sampler(samp),
        npath(n),
        weighted(w),
        remove_total_weight(rw) {}
};


// Implementation of RandGenFst.
template <class A, class B, class S>
class RandGenFstImpl : public CacheImpl<B> {
 public:
  using FstImpl<B>::SetType;
  using FstImpl<B>::SetProperties;
  using FstImpl<B>::SetInputSymbols;
  using FstImpl<B>::SetOutputSymbols;

  using CacheBaseImpl< CacheState<B> >::PushArc;
  using CacheBaseImpl< CacheState<B> >::HasArcs;
  using CacheBaseImpl< CacheState<B> >::HasFinal;
  using CacheBaseImpl< CacheState<B> >::HasStart;
  using CacheBaseImpl< CacheState<B> >::SetArcs;
  using CacheBaseImpl< CacheState<B> >::SetFinal;
  using CacheBaseImpl< CacheState<B> >::SetStart;

  typedef B Arc;
  typedef typename A::Label Label;
  typedef typename A::Weight Weight;
  typedef typename A::StateId StateId;

  RandGenFstImpl(const Fst<A> &fst, const RandGenFstOptions<S> &opts)
      : CacheImpl<B>(opts),
        fst_(fst.Copy()),
        arc_sampler_(opts.arc_sampler),
        npath_(opts.npath),
        weighted_(opts.weighted),
        remove_total_weight_(opts.remove_total_weight),
        superfinal_(kNoLabel) {
    SetType("randgen");

    uint64 props = fst.Properties(kFstProperties, false);
    SetProperties(RandGenProperties(props, weighted_), kCopyProperties);

    SetInputSymbols(fst.InputSymbols());
    SetOutputSymbols(fst.OutputSymbols());
  }

  RandGenFstImpl(const RandGenFstImpl &impl)
    : CacheImpl<B>(impl),
      fst_(impl.fst_->Copy(true)),
      arc_sampler_(new S(*impl.arc_sampler_, fst_)),
      npath_(impl.npath_),
      weighted_(impl.weighted_),
      superfinal_(kNoLabel) {
    SetType("randgen");
    SetProperties(impl.Properties(), kCopyProperties);
    SetInputSymbols(impl.InputSymbols());
    SetOutputSymbols(impl.OutputSymbols());
  }

  ~RandGenFstImpl() {
    for (int i = 0; i < state_table_.size(); ++i)
      delete state_table_[i];
    delete fst_;
    delete arc_sampler_;
  }

  StateId Start() {
    if (!HasStart()) {
      StateId s = fst_->Start();
      if (s == kNoStateId)
        return kNoStateId;
      StateId start = state_table_.size();
      SetStart(start);
      RandState<A> *rstate = new RandState<A>(s, npath_, 0, 0, 0);
      state_table_.push_back(rstate);
    }
    return CacheImpl<B>::Start();
  }

  Weight Final(StateId s) {
    if (!HasFinal(s)) {
      Expand(s);
    }
    return CacheImpl<B>::Final(s);
  }

  size_t NumArcs(StateId s) {
    if (!HasArcs(s)) {
      Expand(s);
    }
    return CacheImpl<B>::NumArcs(s);
  }

  size_t NumInputEpsilons(StateId s) {
    if (!HasArcs(s))
      Expand(s);
    return CacheImpl<B>::NumInputEpsilons(s);
  }

  size_t NumOutputEpsilons(StateId s) {
    if (!HasArcs(s))
      Expand(s);
    return CacheImpl<B>::NumOutputEpsilons(s);
  }

  uint64 Properties() const { return Properties(kFstProperties); }

  // Set error if found; return FST impl properties.
  uint64 Properties(uint64 mask) const {
    if ((mask & kError) &&
        (fst_->Properties(kError, false) || arc_sampler_->Error())) {
      SetProperties(kError, kError);
    }
    return FstImpl<Arc>::Properties(mask);
  }

  void InitArcIterator(StateId s, ArcIteratorData<B> *data) {
    if (!HasArcs(s))
      Expand(s);
    CacheImpl<B>::InitArcIterator(s, data);
  }

  // Computes the outgoing transitions from a state, creating new destination
  // states as needed.
  void Expand(StateId s) {
    if (s == superfinal_) {
      SetFinal(s, Weight::One());
      SetArcs(s);
      return;
    }

    SetFinal(s, Weight::Zero());
    const RandState<A> &rstate = *state_table_[s];
    arc_sampler_->Sample(rstate);
    ArcIterator< Fst<A> > aiter(*fst_, rstate.state_id);
    size_t narcs = fst_->NumArcs(rstate.state_id);
    for (;!arc_sampler_->Done(); arc_sampler_->Next()) {
      const pair<size_t, size_t> &sample_pair = arc_sampler_->Value();
      size_t pos = sample_pair.first;
      size_t count = sample_pair.second;
      double prob = static_cast<double>(count)/rstate.nsamples;
      if (pos < narcs) {  // regular transition
        aiter.Seek(sample_pair.first);
        const A &aarc = aiter.Value();
        Weight weight = weighted_ ? to_weight_(-log(prob)) : Weight::One();
        B barc(aarc.ilabel, aarc.olabel, weight, state_table_.size());
        PushArc(s, barc);
        RandState<A> *nrstate =
            new RandState<A>(aarc.nextstate, count, rstate.length + 1,
                             pos, &rstate);
        state_table_.push_back(nrstate);
      } else {            // super-final transition
        if (weighted_) {
          Weight weight = remove_total_weight_ ?
              to_weight_(-log(prob)) : to_weight_(-log(prob * npath_));
          SetFinal(s, weight);
        } else {
          if (superfinal_ == kNoLabel) {
            superfinal_ = state_table_.size();
            RandState<A> *nrstate = new RandState<A>(kNoStateId, 0, 0, 0, 0);
            state_table_.push_back(nrstate);
          }
          for (size_t n = 0; n < count; ++n) {
            B barc(0, 0, Weight::One(), superfinal_);
            PushArc(s, barc);
          }
        }
      }
    }
    SetArcs(s);
  }

 private:
  Fst<A> *fst_;
  S *arc_sampler_;
  size_t npath_;
  vector<RandState<A> *> state_table_;
  bool weighted_;
  bool remove_total_weight_;
  StateId superfinal_;
  WeightConvert<Log64Weight, Weight> to_weight_;

  void operator=(const RandGenFstImpl<A, B, S> &);  // disallow
};


// Fst class to randomly generate paths through an FST; details controlled
// by RandGenOptionsFst. Output format is a tree weighted by the
// path count.
template <class A, class B, class S>
class RandGenFst : public ImplToFst< RandGenFstImpl<A, B, S> > {
 public:
  friend class ArcIterator< RandGenFst<A, B, S> >;
  friend class StateIterator< RandGenFst<A, B, S> >;
  typedef B Arc;
  typedef S Sampler;
  typedef typename A::Label Label;
  typedef typename A::Weight Weight;
  typedef typename A::StateId StateId;
  typedef DefaultCacheStore<A> Store;
  typedef typename Store::State State;
  typedef RandGenFstImpl<A, B, S> Impl;

  RandGenFst(const Fst<A> &fst, const RandGenFstOptions<S> &opts)
    : ImplToFst<Impl>(new Impl(fst, opts)) {}

  // See Fst<>::Copy() for doc.
 RandGenFst(const RandGenFst<A, B, S> &fst, bool safe = false)
    : ImplToFst<Impl>(fst, safe) {}

  // Get a copy of this RandGenFst. See Fst<>::Copy() for further doc.
  virtual RandGenFst<A, B, S> *Copy(bool safe = false) const {
    return new RandGenFst<A, B, S>(*this, safe);
  }

  virtual inline void InitStateIterator(StateIteratorData<B> *data) const;

  virtual void InitArcIterator(StateId s, ArcIteratorData<B> *data) const {
    GetImpl()->InitArcIterator(s, data);
  }

 private:
  // Makes visible to friends.
  Impl *GetImpl() const { return ImplToFst<Impl>::GetImpl(); }

  void operator=(const RandGenFst<A, B, S> &fst);  // Disallow
};



// Specialization for RandGenFst.
template <class A, class B, class S>
class StateIterator< RandGenFst<A, B, S> >
    : public CacheStateIterator< RandGenFst<A, B, S> > {
 public:
  explicit StateIterator(const RandGenFst<A, B, S> &fst)
    : CacheStateIterator< RandGenFst<A, B, S> >(fst, fst.GetImpl()) {}

 private:
  DISALLOW_COPY_AND_ASSIGN(StateIterator);
};


// Specialization for RandGenFst.
template <class A, class B, class S>
class ArcIterator< RandGenFst<A, B, S> >
    : public CacheArcIterator< RandGenFst<A, B, S> > {
 public:
  typedef typename A::StateId StateId;

  ArcIterator(const RandGenFst<A, B, S> &fst, StateId s)
      : CacheArcIterator< RandGenFst<A, B, S> >(fst.GetImpl(), s) {
    if (!fst.GetImpl()->HasArcs(s))
      fst.GetImpl()->Expand(s);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcIterator);
};


template <class A, class B, class S> inline
void RandGenFst<A, B, S>::InitStateIterator(StateIteratorData<B> *data) const
{
  data->base = new StateIterator< RandGenFst<A, B, S> >(*this);
}

// Options for random path generation.
template <class S>
struct RandGenOptions {
  const S &arc_selector;     // How an arc is selected at a state
  int max_length;            // Maximum path length
  size_t npath;              // # of paths to generate
  bool weighted;             // Output is tree weighted by path count; o.w.
                             // output unweighted union of paths.
  bool remove_total_weight;  // Remove total weight when output is weighted.

  RandGenOptions(const S &sel, int len = INT_MAX, size_t n = 1,
                 bool w = false, bool rw = false)
      : arc_selector(sel),
        max_length(len),
        npath(n),
        weighted(w),
        remove_total_weight(rw) {}
};


template <class IArc, class OArc>
class RandGenVisitor {
 public:
  typedef typename IArc::Weight Weight;
  typedef typename IArc::StateId StateId;

  RandGenVisitor(MutableFst<OArc> *ofst) : ofst_(ofst) {}

  void InitVisit(const Fst<IArc> &ifst) {
    ifst_ = &ifst;

    ofst_->DeleteStates();
    ofst_->SetInputSymbols(ifst.InputSymbols());
    ofst_->SetOutputSymbols(ifst.OutputSymbols());
    if (ifst.Properties(kError, false))
      ofst_->SetProperties(kError, kError);
    path_.clear();
  }

  bool InitState(StateId s, StateId root) { return true; }

  bool TreeArc(StateId s, const IArc &arc) {
    if (ifst_->Final(arc.nextstate) == Weight::Zero()) {
      path_.push_back(arc);
    } else {
      OutputPath();
    }
    return true;
  }

  bool BackArc(StateId s, const IArc &arc) {
    FSTERROR() << "RandGenVisitor: cyclic input";
    ofst_->SetProperties(kError, kError);
    return false;
  }

  bool ForwardOrCrossArc(StateId s, const IArc &arc) {
    OutputPath();
    return true;
  }

  void FinishState(StateId s, StateId p, const IArc *) {
    if (p != kNoStateId && ifst_->Final(s) == Weight::Zero())
      path_.pop_back();
  }

  void FinishVisit() {}

 private:
  void OutputPath() {
    if (ofst_->Start() == kNoStateId) {
      StateId start = ofst_->AddState();
      ofst_->SetStart(start);
    }

    StateId src = ofst_->Start();
    for (size_t i = 0; i < path_.size(); ++i) {
      StateId dest = ofst_->AddState();
      OArc arc(path_[i].ilabel, path_[i].olabel, Weight::One(), dest);
      ofst_->AddArc(src, arc);
      src = dest;
    }
    ofst_->SetFinal(src, Weight::One());
  }

  const Fst<IArc> *ifst_;
  MutableFst<OArc> *ofst_;
  vector<OArc> path_;

  DISALLOW_COPY_AND_ASSIGN(RandGenVisitor);
};


// Randomly generate paths through an FST; details controlled by
// RandGenOptions.
template<class IArc, class OArc, class Selector>
void RandGen(const Fst<IArc> &ifst, MutableFst<OArc> *ofst,
             const RandGenOptions<Selector> &opts) {
  typedef ArcSampler<IArc, Selector> Sampler;
  typedef RandGenFst<IArc, OArc, Sampler> RandFst;
  typedef typename OArc::StateId StateId;
  typedef typename OArc::Weight Weight;

  Sampler* arc_sampler = new Sampler(ifst, opts.arc_selector, opts.max_length);
  RandGenFstOptions<Sampler> fopts(CacheOptions(true, 0), arc_sampler,
                                   opts.npath, opts.weighted,
                                   opts.remove_total_weight);
  RandFst rfst(ifst, fopts);
  if (opts.weighted) {
    *ofst = rfst;
  } else {
    RandGenVisitor<IArc, OArc> rand_visitor(ofst);
    DfsVisit(rfst, &rand_visitor);
  }
}

// Randomly generate a path through an FST with the uniform distribution
// over the transitions.
template<class IArc, class OArc>
void RandGen(const Fst<IArc> &ifst, MutableFst<OArc> *ofst) {
  UniformArcSelector<IArc> uniform_selector;
  RandGenOptions< UniformArcSelector<IArc> > opts(uniform_selector);
  RandGen(ifst, ofst, opts);
}

}  // namespace fst

#endif  // FST_LIB_RANDGEN_H__
