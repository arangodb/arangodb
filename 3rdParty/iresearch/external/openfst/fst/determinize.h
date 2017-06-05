// determinize.h


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
// Functions and classes to determinize an FST.

#ifndef FST_LIB_DETERMINIZE_H__
#define FST_LIB_DETERMINIZE_H__

#include <algorithm>
#include <climits>
#include <forward_list>
using std::forward_list;
#include <unordered_map>
using std::unordered_map;
using std::unordered_multimap;
#include <map>
#include <string>
#include <vector>
using std::vector;

#include <fst/arc-map.h>
#include <fst/cache.h>
#include <fst/bi-table.h>
#include <fst/factor-weight.h>
#include <fst/filter-state.h>
#include <fst/prune.h>
#include <fst/test-properties.h>


namespace fst {

//
// COMMON DIVISORS - these are used in determinization to compute
// the transition weights. In the simplest case, it is just the same
// as the semiring Plus(). However, other choices permit more efficient
// determinization when the output contains strings.
//

// The default common divisor uses the semiring Plus.
template <class W>
class DefaultCommonDivisor {
 public:
  typedef W Weight;

  W operator()(const W &w1, const W &w2) const { return Plus(w1, w2); }
};


// The label common divisor for a (left) string semiring selects a
// single letter common prefix or the empty string. This is used in
// the determinization of output strings so that at most a single
// letter will appear in the output of a transtion.
template <typename L, StringType S>
class LabelCommonDivisor {
 public:
  typedef StringWeight<L, S> Weight;

  Weight operator()(const Weight &w1, const Weight &w2) const {
    StringWeightIterator<L, S> iter1(w1);
    StringWeightIterator<L, S> iter2(w2);

    if (!(StringWeight<L, S>::Properties() & kLeftSemiring)) {
      FSTERROR() << "LabelCommonDivisor: Weight needs to be left semiring";
      return Weight::NoWeight();
    } else if (w1.Size() == 0 || w2.Size() == 0) {
      return Weight::One();
    } else if (w1 == Weight::Zero()) {
      return Weight(iter2.Value());
    } else if (w2 == Weight::Zero()) {
      return Weight(iter1.Value());
    } else if (iter1.Value() == iter2.Value()) {
      return Weight(iter1.Value());
    } else {
      return Weight::One();
    }
  }
};


// The gallic common divisor uses the label common divisor on the
// string component and the template argument D common divisor on the
// weight component, which defaults to the default common divisor.
template <class L, class W, GallicType G, class D = DefaultCommonDivisor<W> >
class GallicCommonDivisor {
 public:
  typedef GallicWeight<L, W, G> Weight;

  Weight operator()(const Weight &w1, const Weight &w2) const {
    return Weight(label_common_divisor_(w1.Value1(), w2.Value1()),
                  weight_common_divisor_(w1.Value2(), w2.Value2()));
  }

 private:
  LabelCommonDivisor<L, GALLIC_STRING_TYPE(G)> label_common_divisor_;
  D weight_common_divisor_;
};

// Specialization for general GALLIC weight.
template <class L, class W, class D>
class GallicCommonDivisor<L, W, GALLIC, D> {
 public:
  typedef GallicWeight<L, W, GALLIC> Weight;
  typedef GallicWeight<L, W, GALLIC_RESTRICT> GRWeight;
  typedef UnionWeightIterator<GRWeight, GallicUnionWeightOptions<L, W> > Iter;

  Weight operator()(const Weight &w1, const Weight &w2) const {
    GRWeight w = GRWeight::Zero();
    for (Iter iter(w1); !iter.Done(); iter.Next())
      w = common_divisor_(w, iter.Value());
    for (Iter iter(w2); !iter.Done(); iter.Next())
      w = common_divisor_(w, iter.Value());
    return w == GRWeight::Zero() ? Weight::Zero() : Weight(w);
  }

 private:
  GallicCommonDivisor<L, W, GALLIC_RESTRICT, D> common_divisor_;
};

// Represents an element in a subset
template <class A>
struct DeterminizeElement {
  typedef typename A::StateId StateId;
  typedef typename A::Weight Weight;

  DeterminizeElement() {}

  DeterminizeElement(StateId s, Weight w) : state_id(s), weight(w) {}

  bool operator==(const DeterminizeElement<A> & element) const {
    return state_id == element.state_id && weight == element.weight;
  }

  bool operator!=(const DeterminizeElement<A> & element) const {
    return !(*this == element);
  }

  bool operator<(const DeterminizeElement<A> & element) const {
    return state_id < element.state_id;
  }

  StateId state_id;  // Input state Id
  Weight weight;     // Residual weight
};

// Represents a weighted subset and determinization filter state
template <typename A, typename F>
struct DeterminizeStateTuple {
  typedef A Arc;
  typedef F FilterState;
  typedef DeterminizeElement<Arc> Element;
  typedef std::forward_list<Element> Subset;

  DeterminizeStateTuple() : filter_state(FilterState::NoState()) { }

  bool operator==(const DeterminizeStateTuple<A, F> &tuple) const {
    return (tuple.filter_state == filter_state) && (tuple.subset == subset);
  }

  bool operator!=(const DeterminizeStateTuple<A, F> &tuple) const {
    return (tuple.filter_state != filter_state) || (tuple.subset != subset);
  }

  Subset subset;
  FilterState filter_state;
};

// Proto-transition for determinization
template <class S>
struct DeterminizeArc {
  typedef S StateTuple;
  typedef typename S::Arc Arc;
  typedef typename Arc::Label Label;
  typedef typename Arc::Weight Weight;

  DeterminizeArc()
    : label(kNoLabel), weight(Weight::Zero()), dest_tuple(0) {}

  explicit DeterminizeArc(const Arc &arc)
    : label(arc.ilabel),
      weight(Weight::Zero()),
      dest_tuple(new S) { }

  Label label;                // arc label
  Weight weight;              // arc weight
  StateTuple *dest_tuple;     // destination subset and filter state
};

//
// DETERMINIZE FILTERS - these are used in determinization to compute
// destination state tuples based on the source tuple, transition, and
// destination element or on similar super-final transition
// information. The filter operates on a map between a label and the
// corresponding destination state tuples. It must define the map type
// LabelMap. The default filter is used for weighted determinization.
//

// A determinize filter for implementing weighted determinization.
template <class Arc>
class DefaultDeterminizeFilter {
 public:
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Label Label;
  typedef typename Arc::Weight Weight;
  typedef CharFilterState FilterState;

  typedef DeterminizeElement<Arc> Element;
  typedef DeterminizeStateTuple<Arc, FilterState> StateTuple;
  typedef map<Label, DeterminizeArc<StateTuple> > LabelMap;

  // This is needed e.g. to go into the gallic domain for transducers.
  template <class A>
  struct rebind { typedef DefaultDeterminizeFilter<A> other; };

  DefaultDeterminizeFilter(const Fst<Arc> &fst)
      : fst_(fst.Copy()) { }

  // This is needed e.g. to go into the gallic domain for transducers.
  // Ownership of the templated filter argument is given to this class.
  template <class F>
  DefaultDeterminizeFilter(const Fst<Arc> &fst, F* filter)
      : fst_(fst.Copy()) {
    delete filter;
  }

  // Copy ctr. The FST can be passed if it has been e.g. (deep) copied.
  explicit DefaultDeterminizeFilter(
      const DefaultDeterminizeFilter<Arc> &filter,
      const Fst<Arc> *fst = 0)
      : fst_(fst ? fst->Copy() : filter.fst_->Copy()) { }

  ~DefaultDeterminizeFilter() { delete fst_; }

  FilterState Start() const { return FilterState(0); }

  void SetState(StateId s, const StateTuple &tuple) { }

  // Filters transition, possibly modifying label map. Returns
  // true if arc is added to label map.
  bool FilterArc(const Arc &arc,
                 const Element &src_element,
                 const Element &dest_element,
                 LabelMap *label_map) const {
    // Adds element to unique state tuple for arc label; create if necessary
    DeterminizeArc<StateTuple> &det_arc = (*label_map)[arc.ilabel];
    if (det_arc.label == kNoLabel) {
      det_arc = DeterminizeArc<StateTuple>(arc);
      det_arc.dest_tuple->filter_state = FilterState(0);
    }
    det_arc.dest_tuple->subset.push_front(dest_element);
    return true;
  }

  // Filters super-final transition, returning new final weight
  Weight FilterFinal(Weight final_weight, const Element &element) {
    return final_weight;
  }

  static uint64 Properties(uint64 props) { return props; }

 private:
  Fst<Arc> *fst_;
  void operator=(const DefaultDeterminizeFilter<Arc> &);  // disallow
};

//
// DETERMINIZATION STATE TABLES
//
// The determinization state table has the form:
//
// template <class A, class F>
// class DeterminizeStateTable {
//  public:
//   typename A Arc;
//   typename F FilterState;
//   typedef typename Arc::StateId StateId;
//   typedef DeterminizeStateTuple<Arc, FilterState> StateTuple;
//
//   // Required sub-class. This is needed e.g. to go into the gallic domain.
//   template <class B, class G>
//   struct rebind { typedef DeterminizeStateTable<B, G> other; };
//
//   // Required constuctor
//   DeterminizeStateTable();
//
//   // Required copy constructor that does not copy state
//   DeterminizeStateTable(const DeterminizeStateTable<A,F> &table);
//
//   // Lookup state ID by state tuple.
//   // If it doesn't exist, then add it. FindState takes ownership
//   // of the state tuple argument (so that it doesn't have to
//   // copy it if it creates a new state).
//   StateId FindState(StateTuple *tuple);
//
//   // Lookup state tuple by ID.
//   const StateTuple *Tuple(StateId id) const;
// };

// The default determinization state table based on the
// compact hash bi-table.
template <class A, class F>
class DefaultDeterminizeStateTable {
 public:
  typedef A Arc;
  typedef F FilterState;
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Label Label;
  typedef typename Arc::Weight Weight;
  typedef DeterminizeStateTuple<Arc, FilterState> StateTuple;
  typedef typename StateTuple::Subset Subset;
  typedef typename StateTuple::Element Element;

  template <class B, class G>
  struct rebind { typedef DefaultDeterminizeStateTable<B, G> other; };

  explicit DefaultDeterminizeStateTable(size_t table_size = 0)
      : table_size_(table_size), tuples_(table_size_) { }

  DefaultDeterminizeStateTable(const DefaultDeterminizeStateTable<A, F> &table)
      : table_size_(table.table_size_), tuples_(table_size_) { }

  ~DefaultDeterminizeStateTable() {
    for (StateId s = 0; s < tuples_.Size(); ++s)
      delete tuples_.FindEntry(s);
  }

  // Finds the state corresponding to a state tuple. Only creates a new
  // state if the tuple is not found. FindState takes ownership of
  // the tuple argument (so that it doesn't have to copy it if it
  // creates a new state).
  StateId FindState(StateTuple *tuple) {
    StateId ns = tuples_.Size();
    StateId s = tuples_.FindId(tuple);

    if (s != ns) delete tuple;  // tuple found
    return s;
  }

  const StateTuple* Tuple(StateId s) { return tuples_.FindEntry(s); }

 private:
  // Comparison object for StateTuples.
  class StateTupleEqual {
   public:
    bool operator()(const StateTuple* tuple1, const StateTuple* tuple2) const {
      return *tuple1 == *tuple2;
    }
  };

  // Hash function for StateTuples.
  class StateTupleKey {
   public:
    size_t operator()(const StateTuple* tuple) const {
      size_t h = tuple->filter_state.Hash();
      for (typename Subset::const_iterator iter = tuple->subset.begin();
           iter != tuple->subset.end();
           ++iter) {
        const Element &element = *iter;
        size_t h1 = element.state_id;
        size_t h2 = element.weight.Hash();
        const int lshift = 5;
        const int rshift = CHAR_BIT * sizeof(size_t) - 5;
        h ^= h << 1 ^ h1 << lshift ^ h1 >> rshift ^ h2;
     }
      return h;
    }
  };

  size_t table_size_;

  typedef CompactHashBiTable<StateId, StateTuple *,
                             StateTupleKey, StateTupleEqual,
                             HS_STL>  StateTupleTable;

  StateTupleTable tuples_;

  void operator=(const DefaultDeterminizeStateTable<A, F> &);  // disallow
};


// Type of determinization
enum DeterminizeType {
  DETERMINIZE_FUNCTIONAL,     // Input transducer is functional (error if not)
  DETERMINIZE_NONFUNCTIONAL,  // Input transducer is not known to be functional
  DETERMINIZE_DISAMBIGUATE    // Input transducer is non-functional but only
  // keep the min of ambiguous outputs.
};

// Options for finite-state transducer determinization templated on
// the arc type, common divisor, the determinization filter and the
// state table.  DeterminizeFst takes ownership of the determinization
// filter and state table if provided.
template <class Arc,
          class D = DefaultCommonDivisor<typename Arc::Weight>,
          class F = DefaultDeterminizeFilter<Arc>,
          class T = DefaultDeterminizeStateTable<Arc,
                                                 typename F::FilterState> >
struct DeterminizeFstOptions : CacheOptions {
  typedef typename Arc::Label Label;
  float delta;                // Quantization delta for subset weights
  Label subsequential_label;  // Label used for residual final output
                              // when producing subsequential transducers.
  DeterminizeType type;       // Determinization type
  bool increment_subsequential_label;  // When creating several subsequential
                                       // arcs at a given state, make their
                                       // label distinct by incrementing.
  F *filter;                  // Determinization filter
  T *state_table;             // Determinization state table

  explicit DeterminizeFstOptions(const CacheOptions &opts,
                                 float del = kDelta, Label lab = 0,
                                 DeterminizeType typ = DETERMINIZE_FUNCTIONAL,
                                 bool inc_lab = false,
                                 F *filt = 0, T *table = 0)
      : CacheOptions(opts), delta(del), subsequential_label(lab),
        type(typ), increment_subsequential_label(inc_lab),
        filter(filt), state_table(table) {}

  explicit DeterminizeFstOptions(float del = kDelta, Label lab = 0,
                                 DeterminizeType typ = DETERMINIZE_FUNCTIONAL,
                                 bool inc_lab = false,
                                 F *filt = 0, T *table = 0)
      : delta(del), subsequential_label(lab), type(typ),
        increment_subsequential_label(inc_lab), filter(filt),
        state_table(table) {}
};

// Implementation of delayed DeterminizeFst. This base class is
// common to the variants that implement acceptor and transducer
// determinization.
template <class A>
class DeterminizeFstImplBase : public CacheImpl<A> {
 public:
  using FstImpl<A>::SetType;
  using FstImpl<A>::SetProperties;
  using FstImpl<A>::Properties;
  using FstImpl<A>::SetInputSymbols;
  using FstImpl<A>::SetOutputSymbols;

  using CacheBaseImpl< CacheState<A> >::HasStart;
  using CacheBaseImpl< CacheState<A> >::HasFinal;
  using CacheBaseImpl< CacheState<A> >::HasArcs;
  using CacheBaseImpl< CacheState<A> >::SetFinal;
  using CacheBaseImpl< CacheState<A> >::SetStart;

  typedef typename A::Label Label;
  typedef typename A::Weight Weight;
  typedef typename A::StateId StateId;
  typedef DefaultCacheStore<A> Store;
  typedef typename Store::State State;

  template <class D, class F, class T>
  DeterminizeFstImplBase(const Fst<A> &fst,
                         const DeterminizeFstOptions<A, D, F, T> &opts)
      : CacheImpl<A>(opts), fst_(fst.Copy()) {
    SetType("determinize");
    uint64 iprops = fst.Properties(kFstProperties, false);
    uint64 dprops = DeterminizeProperties(
        iprops,
        opts.subsequential_label != 0,
        opts.type == DETERMINIZE_NONFUNCTIONAL ?
        opts.increment_subsequential_label : true);
    SetProperties(F::Properties(dprops), kCopyProperties);
    SetInputSymbols(fst.InputSymbols());
    SetOutputSymbols(fst.OutputSymbols());
  }

  DeterminizeFstImplBase(const DeterminizeFstImplBase<A> &impl)
      : CacheImpl<A>(impl),
        fst_(impl.fst_->Copy(true)) {
    SetType("determinize");
    SetProperties(impl.Properties(), kCopyProperties);
    SetInputSymbols(impl.InputSymbols());
    SetOutputSymbols(impl.OutputSymbols());
  }

  virtual ~DeterminizeFstImplBase() { delete fst_; }

  virtual DeterminizeFstImplBase<A> *Copy() = 0;

  StateId Start() {
    if (!HasStart()) {
      StateId start = ComputeStart();
      if (start != kNoStateId) {
        SetStart(start);
      }
    }
    return CacheImpl<A>::Start();
  }

  Weight Final(StateId s) {
    if (!HasFinal(s)) {
      Weight final = ComputeFinal(s);
      SetFinal(s, final);
    }
    return CacheImpl<A>::Final(s);
  }

  virtual void Expand(StateId s) = 0;

  size_t NumArcs(StateId s) {
    if (!HasArcs(s))
      Expand(s);
    return CacheImpl<A>::NumArcs(s);
  }

  size_t NumInputEpsilons(StateId s) {
    if (!HasArcs(s))
      Expand(s);
    return CacheImpl<A>::NumInputEpsilons(s);
  }

  size_t NumOutputEpsilons(StateId s) {
    if (!HasArcs(s))
      Expand(s);
    return CacheImpl<A>::NumOutputEpsilons(s);
  }

  void InitArcIterator(StateId s, ArcIteratorData<A> *data) {
    if (!HasArcs(s))
      Expand(s);
    CacheImpl<A>::InitArcIterator(s, data);
  }

  virtual StateId ComputeStart() = 0;

  virtual Weight ComputeFinal(StateId s) = 0;

  const Fst<A> &GetFst() const { return *fst_; }

 private:
  const Fst<A> *fst_;            // Input Fst

  void operator=(const DeterminizeFstImplBase<A> &);  // disallow
};


// Implementation of delayed determinization for weighted acceptors.
// It is templated on the arc type A and the common divisor D.
template <class A, class D, class F, class T>
class DeterminizeFsaImpl : public DeterminizeFstImplBase<A> {
 public:
  using FstImpl<A>::SetProperties;
  using DeterminizeFstImplBase<A>::GetFst;
  using DeterminizeFstImplBase<A>::SetArcs;

  typedef typename A::Label Label;
  typedef typename A::Weight Weight;
  typedef typename A::StateId StateId;
  typedef typename F::FilterState FilterState;
  typedef DeterminizeStateTuple<A, FilterState> StateTuple;
  typedef typename StateTuple::Element Element;
  typedef typename StateTuple::Subset Subset;
  typedef typename F::LabelMap LabelMap;

  DeterminizeFsaImpl(const Fst<A> &fst,
                     const vector<Weight> *in_dist, vector<Weight> *out_dist,
                     const DeterminizeFstOptions<A, D, F, T> &opts)
      : DeterminizeFstImplBase<A>(fst, opts),
        delta_(opts.delta),
        in_dist_(in_dist),
        out_dist_(out_dist),
        filter_(opts.filter ? opts.filter : new F(fst)),
        state_table_(opts.state_table ? opts.state_table : new T()) {
    if (!fst.Properties(kAcceptor, true)) {
      FSTERROR()  << "DeterminizeFst: argument not an acceptor";
      SetProperties(kError, kError);
    }
    if (!(Weight::Properties() & kLeftSemiring)) {
      FSTERROR() << "DeterminizeFst: Weight needs to be left distributive: "
                 << Weight::Type();
      SetProperties(kError, kError);
    }
    if (out_dist_)
      out_dist_->clear();
  }

  DeterminizeFsaImpl(const DeterminizeFsaImpl<A, D, F, T> &impl)
      : DeterminizeFstImplBase<A>(impl),
        delta_(impl.delta_),
        in_dist_(0),
        out_dist_(0),
        filter_(new F(*impl.filter_, &GetFst())),
        state_table_(new T(*impl.state_table_)) {
    if (impl.out_dist_) {
      FSTERROR() << "DeterminizeFsaImpl: cannot copy with out_dist vector";
      SetProperties(kError, kError);
    }
  }

  virtual ~DeterminizeFsaImpl() {
    delete filter_;
    delete state_table_;
  }

  virtual DeterminizeFsaImpl<A, D, F, T> *Copy() {
    return new DeterminizeFsaImpl<A, D, F, T>(*this);
  }

  uint64 Properties() const { return Properties(kFstProperties); }

  // Set error if found; return FST impl properties.
  uint64 Properties(uint64 mask) const {
    if ((mask & kError) && (GetFst().Properties(kError, false)))
      SetProperties(kError, kError);
    return FstImpl<A>::Properties(mask);
  }

  virtual StateId ComputeStart() {
    StateId s = GetFst().Start();
    if (s == kNoStateId)
      return kNoStateId;
    Element element(s, Weight::One());
    StateTuple *tuple = new StateTuple;
    tuple->subset.push_front(element);
    tuple->filter_state = filter_->Start();
    return FindState(tuple);
  }

  virtual Weight ComputeFinal(StateId s) {
    const StateTuple *tuple = state_table_->Tuple(s);
    filter_->SetState(s, *tuple);

    Weight final = Weight::Zero();
    for (typename Subset::const_iterator siter = tuple->subset.begin();
         siter != tuple->subset.end();
         ++siter) {
      const Element &element = *siter;
      final = Plus(final, Times(element.weight,
                                GetFst().Final(element.state_id)));
      final = filter_->FilterFinal(final, element);
      if (!final.Member())
        SetProperties(kError, kError);
    }
    return final;
  }

  StateId FindState(StateTuple *tuple) {
    StateId s = state_table_->FindState(tuple);
    if (in_dist_ && out_dist_->size() <= s)
      out_dist_->push_back(ComputeDistance(tuple->subset));
    return s;
  }

  // Compute distance from a state to the final states in the DFA
  // given the distances in the NFA.
  Weight ComputeDistance(const Subset &subset) {
    Weight outd = Weight::Zero();
    for (typename Subset::const_iterator siter = subset.begin();
         siter != subset.end(); ++siter) {
      const Element &element = *siter;
      Weight ind = element.state_id < in_dist_->size() ?
          (*in_dist_)[element.state_id] : Weight::Zero();
      outd = Plus(outd, Times(element.weight, ind));
    }
    return outd;
  }

  // Computes the outgoing transitions from a state, creating new destination
  // states as needed.
  virtual void Expand(StateId s) {
    LabelMap label_map;
    GetLabelMap(s, &label_map);

    for (typename LabelMap::iterator liter = label_map.begin();
         liter != label_map.end();
         ++liter) {
      AddArc(s, liter->second);
    }
    SetArcs(s);
  }

 private:
  // Constructs proto determinization transition, including
  // destination subset, per label.
  void GetLabelMap(StateId s, LabelMap *label_map) {
    const StateTuple *src_tuple = state_table_->Tuple(s);
    filter_->SetState(s, *src_tuple);
    for (typename Subset::const_iterator siter = src_tuple->subset.begin();
         siter != src_tuple->subset.end();
         ++siter) {
      const Element &src_element = *siter;
      for (ArcIterator< Fst<A> > aiter(GetFst(), src_element.state_id);
           !aiter.Done();
           aiter.Next()) {
        const A &arc = aiter.Value();
        Element dest_element(arc.nextstate,
                            Times(src_element.weight, arc.weight));
        filter_->FilterArc(arc, src_element, dest_element, label_map);
      }
    }

    for (typename LabelMap::iterator liter = label_map->begin();
         liter != label_map->end();
         ++liter) {
      NormArc(&liter->second);
    }
  }

  // Sorts subsets and removes duplicate elements.
  // Normalizes transition and subset weights.
  void NormArc(DeterminizeArc<StateTuple> *det_arc) {
    StateTuple *dest_tuple = det_arc->dest_tuple;
    dest_tuple->subset.sort();
    typename Subset::iterator piter = dest_tuple->subset.begin();
    for (typename Subset::iterator diter = dest_tuple->subset.begin();
         diter != dest_tuple->subset.end();) {
      Element &dest_element = *diter;
      Element &prev_element = *piter;
      // Computes arc weight.
      det_arc->weight = common_divisor_(det_arc->weight, dest_element.weight);

      if (piter != diter && dest_element.state_id == prev_element.state_id) {
        // Found duplicate state: sums state weight and deletes dup.
        prev_element.weight = Plus(prev_element.weight,
                                   dest_element.weight);
        if (!prev_element.weight.Member())
          SetProperties(kError, kError);
        ++diter;
        dest_tuple->subset.erase_after(piter);
      } else {
        piter = diter;
        ++diter;
      }
    }

    // Divides out label weight from destination subset elements.
    // Quantizes to ensure comparisons are effective.
    for (typename Subset::iterator diter = dest_tuple->subset.begin();
         diter != dest_tuple->subset.end();
         ++diter) {
      Element &dest_element = *diter;
      dest_element.weight = Divide(dest_element.weight, det_arc->weight,
                                   DIVIDE_LEFT);
      dest_element.weight = dest_element.weight.Quantize(delta_);
    }
  }

  // Adds an arc from state S to the destination state associated
  // with state tuple in DET_ARC (as created by GetLabelMap).
  void AddArc(StateId s, const DeterminizeArc<StateTuple> &det_arc) {
    A arc;
    arc.ilabel = det_arc.label;
    arc.olabel = det_arc.label;
    arc.weight = det_arc.weight;
    arc.nextstate = FindState(det_arc.dest_tuple);
    CacheImpl<A>::PushArc(s, arc);
  }

  float delta_;                    // Quantization delta for subset weights
  const vector<Weight> *in_dist_;  // Distance to final NFA states
  vector<Weight> *out_dist_;       // Distance to final DFA states

  D common_divisor_;
  F *filter_;
  T *state_table_;

  void operator=(const DeterminizeFsaImpl<A, D, F, T> &);  // disallow
};


// Implementation of delayed determinization for transducers.
// Transducer determinization is implemented by mapping the input to
// the Gallic semiring as an acceptor whose weights contain the output
// strings and using acceptor determinization above to determinize
// that acceptor.
template <class A, GallicType G, class D, class F, class T>
class DeterminizeFstImpl : public DeterminizeFstImplBase<A> {
 public:
  using FstImpl<A>::SetProperties;
  using DeterminizeFstImplBase<A>::GetFst;
  using CacheBaseImpl< CacheState<A> >::GetCacheGc;
  using CacheBaseImpl< CacheState<A> >::GetCacheLimit;

  typedef typename A::Label Label;
  typedef typename A::Weight Weight;
  typedef typename A::StateId StateId;

  typedef ToGallicMapper<A, G> ToMapper;
  typedef FromGallicMapper<A, G> FromMapper;

  typedef typename ToMapper::ToArc ToArc;
  typedef ArcMapFst<A, ToArc, ToMapper> ToFst;
  typedef ArcMapFst<ToArc, A, FromMapper> FromFst;

  typedef GallicCommonDivisor<Label, Weight, G, D> ToD;
  typedef typename F::template rebind<ToArc>::other ToF;
  typedef typename ToF::FilterState ToFilterState;
  typedef typename T::template rebind<ToArc, ToFilterState>::other ToT;
  typedef GallicFactor<Label, Weight, G> FactorIterator;

  DeterminizeFstImpl(const Fst<A> &fst,
                     const DeterminizeFstOptions<A, D, F, T> &opts)
      : DeterminizeFstImplBase<A>(fst, opts),
        delta_(opts.delta),
        subsequential_label_(opts.subsequential_label),
        increment_subsequential_label_(opts.increment_subsequential_label) {
    if (opts.state_table) {
      FSTERROR() << "DeterminizeFst: "
                 << "a state table can not be passed with transducer input";
      SetProperties(kError, kError);
      return;
    }
    Init(GetFst(), opts.filter);
  }

  DeterminizeFstImpl(const DeterminizeFstImpl<A, G, D, F, T> &impl)
      : DeterminizeFstImplBase<A>(impl),
        delta_(impl.delta_),
        subsequential_label_(impl.subsequential_label_),
        increment_subsequential_label_(impl.increment_subsequential_label_) {
    Init(GetFst(), 0);
  }

  ~DeterminizeFstImpl() { delete from_fst_; }

  virtual DeterminizeFstImpl<A, G, D, F, T> *Copy() {
    return new DeterminizeFstImpl<A, G, D, F, T>(*this);
  }

  uint64 Properties() const { return Properties(kFstProperties); }

  // Set error if found; return FST impl properties.
  uint64 Properties(uint64 mask) const {
    if ((mask & kError) && (GetFst().Properties(kError, false) ||
                            from_fst_->Properties(kError, false)))
      SetProperties(kError, kError);
    return FstImpl<A>::Properties(mask);
  }

  virtual StateId ComputeStart() { return from_fst_->Start(); }

  virtual Weight ComputeFinal(StateId s) { return from_fst_->Final(s); }

  virtual void Expand(StateId s) {
    for (ArcIterator<FromFst> aiter(*from_fst_, s);
         !aiter.Done();
         aiter.Next())
      CacheImpl<A>::PushArc(s, aiter.Value());
    CacheImpl<A>::SetArcs(s);
  }

 private:
  // Initialization of transducer determinization implementation, which
  // is defined after DeterminizeFst since it calls it.
  void Init(const Fst<A> &fst, F *filter);

  float delta_;
  Label subsequential_label_;
  bool increment_subsequential_label_;
  FromFst *from_fst_;

  void operator=(const DeterminizeFstImpl<A, G, D, F, T> &);  // disallow
};


// Determinizes a weighted transducer. This version is a delayed
// Fst. The result will be an equivalent FST that has the property
// that no state has two transitions with the same input label.
// For this algorithm, epsilon transitions are treated as regular
// symbols (cf. RmEpsilon).
//
// The transducer must be functional. The weights must be (weakly)
// left divisible (valid for TropicalWeight and LogWeight for instance)
// and be zero-sum-free if for all a,b: (Plus(a, b) = 0 => a = b = 0.
//
// Complexity:
// - Determinizable: exponential (polynomial in the size of the output)
// - Non-determinizable) does not terminate
//
// The determinizable automata include all unweighted and all acyclic input.
//
// References:
// - Mehryar Mohri, "Finite-State Transducers in Language and Speech
//   Processing". Computational Linguistics, 23:2, 1997.
//
// This class attaches interface to implementation and handles
// reference counting, delegating most methods to ImplToFst.
template <class A>
class DeterminizeFst : public ImplToFst< DeterminizeFstImplBase<A> >  {
 public:
  friend class ArcIterator< DeterminizeFst<A> >;
  friend class StateIterator< DeterminizeFst<A> >;
  template <class B, GallicType G, class D, class F, class T>
  friend class DeterminizeFstImpl;

  typedef A Arc;
  typedef typename A::Weight Weight;
  typedef typename A::StateId StateId;
  typedef typename A::Label Label;
  typedef DefaultCacheStore<A> Store;
  typedef typename Store::State State;
  typedef DeterminizeFstImplBase<A> Impl;
  using ImplToFst<Impl>::SetImpl;

  explicit DeterminizeFst(const Fst<A> &fst) {
    typedef DefaultCommonDivisor<Weight> D;
    typedef DefaultDeterminizeFilter<A> F;
    typedef typename F::FilterState FilterState;
    typedef DefaultDeterminizeStateTable<A, FilterState> T;
    DeterminizeFstOptions<A, D, F, T> opts;
    Init(fst, opts);
  }

  template <class D, class F, class T>
  DeterminizeFst(const Fst<A> &fst,
                 const DeterminizeFstOptions<A, D, F, T> &opts) {
    Init(fst, opts);
  }

  // This acceptor-only version additionally computes the distance to
  // final states in the output if provided with those distances for the
  // input. Useful for e.g. unique N-shortest paths.
  template <class D, class F, class T>
  DeterminizeFst(const Fst<A> &fst,
                 const vector<Weight> *in_dist, vector<Weight> *out_dist,
                 const DeterminizeFstOptions<A, D, F, T> &opts) {
    if (!fst.Properties(kAcceptor, true)) {
      FSTERROR() << "DeterminizeFst:"
                 << " distance to final states computed for acceptors only";
      GetImpl()->SetProperties(kError, kError);
    }
    SetImpl(new DeterminizeFsaImpl<A, D, F, T>(fst, in_dist, out_dist, opts));
  }

  // See Fst<>::Copy() for doc.
  DeterminizeFst(const DeterminizeFst<A> &fst, bool safe = false)
      : ImplToFst<Impl>() {
    if (safe)
      SetImpl(fst.GetImpl()->Copy());
    else
      SetImpl(fst.GetImpl(), false);
  }

  // Get a copy of this DeterminizeFst. See Fst<>::Copy() for further doc.
  virtual DeterminizeFst<A> *Copy(bool safe = false) const {
    return new DeterminizeFst<A>(*this, safe);
  }

  virtual inline void InitStateIterator(StateIteratorData<A> *data) const;

  virtual void InitArcIterator(StateId s, ArcIteratorData<A> *data) const {
    GetImpl()->InitArcIterator(s, data);
  }

 private:
  template <class D, class F, class T>
  void Init(const Fst<Arc> &fst,
            const DeterminizeFstOptions<A, D, F, T> &opts) {
    if (fst.Properties(kAcceptor, true)) {
      // Calls implementation for acceptors.
      SetImpl(new DeterminizeFsaImpl<A, D, F, T>(fst, 0, 0, opts));
    } else if (opts.type == DETERMINIZE_DISAMBIGUATE) {
      if (!(Weight::Properties() & kPath)) {
        FSTERROR() << "DeterminizeFst: Weight needs to have the "
                   << "path property to disambiguate output: "
                   << Weight::Type();
        GetImpl()->SetProperties(kError, kError);
      }
      // Calls disambiguating implementation for non-functional transducers.
      SetImpl(new
              DeterminizeFstImpl<A, GALLIC_MIN, D, F, T>(fst, opts));
    } else if (opts.type == DETERMINIZE_FUNCTIONAL) {
      // Calls implementation for functional transducers.
      SetImpl(new
              DeterminizeFstImpl<A, GALLIC_RESTRICT, D, F, T>(fst, opts));
    } else {  // opts.type == DETERMINIZE_NONFUNCTIONAL
      // Calls implementation for non functional transducers;
      SetImpl(new
              DeterminizeFstImpl<A, GALLIC, D, F, T>(fst, opts));
    }
  }

  // Makes visible to friends.
  Impl *GetImpl() const { return ImplToFst<Impl>::GetImpl(); }

  void operator=(const DeterminizeFst<A> &fst);  // Disallow
};


// Initialization of transducer determinization implementation, which
// is defined after DeterminizeFst since it calls it.
template <class A, GallicType G, class D, class F, class T>
void DeterminizeFstImpl<A, G, D, F, T>::Init(const Fst<A> &fst,  F *filter) {
  // Mapper to an acceptor.
  ToFst to_fst(fst, ToMapper());
  ToF *to_filter = filter ? new ToF(to_fst, filter) : 0;

  // Determinizes acceptor.
  // This recursive call terminates since it is to a (non-recursive)
  // different constructor.
  CacheOptions copts(GetCacheGc(), GetCacheLimit());
  DeterminizeFstOptions<ToArc, ToD, ToF, ToT>
      dopts(copts, delta_, 0, DETERMINIZE_FUNCTIONAL, false, to_filter);
  // Uses acceptor-only constructor to avoid template recursion
  DeterminizeFst<ToArc> det_fsa(to_fst, 0, 0, dopts);

  // Mapper back to transducer.
  FactorWeightOptions<ToArc> fopts(CacheOptions(true, 0), delta_,
                                   kFactorFinalWeights,
                                   subsequential_label_,
                                   subsequential_label_,
                                   increment_subsequential_label_,
                                   increment_subsequential_label_);
  FactorWeightFst<ToArc, FactorIterator> factored_fst(det_fsa, fopts);
  from_fst_ = new FromFst(factored_fst, FromMapper(subsequential_label_));
}


// Specialization for DeterminizeFst.
template <class A>
class StateIterator< DeterminizeFst<A> >
    : public CacheStateIterator< DeterminizeFst<A> > {
 public:
  explicit StateIterator(const DeterminizeFst<A> &fst)
      : CacheStateIterator< DeterminizeFst<A> >(fst, fst.GetImpl()) {}
};


// Specialization for DeterminizeFst.
template <class A>
class ArcIterator< DeterminizeFst<A> >
    : public CacheArcIterator< DeterminizeFst<A> > {
 public:
  typedef typename A::StateId StateId;

  ArcIterator(const DeterminizeFst<A> &fst, StateId s)
      : CacheArcIterator< DeterminizeFst<A> >(fst.GetImpl(), s) {
    if (!fst.GetImpl()->HasArcs(s))
      fst.GetImpl()->Expand(s);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ArcIterator);
};


template <class A> inline
void DeterminizeFst<A>::InitStateIterator(StateIteratorData<A> *data) const
{
  data->base = new StateIterator< DeterminizeFst<A> >(*this);
}


// Useful aliases when using StdArc.
typedef DeterminizeFst<StdArc> StdDeterminizeFst;


template <class Arc>
struct DeterminizeOptions {
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Weight Weight;
  typedef typename Arc::Label Label;

  float delta;                // Quantization delta for subset weights.
  Weight weight_threshold;    // Pruning weight threshold.
  StateId state_threshold;    // Pruning state threshold.
  Label subsequential_label;  // Label used for residual final output
  // when producing subsequential transducers.
  DeterminizeType type;       // functional, nonfunctional, disambiguate?
  bool increment_subsequential_label;  // When creating several subsequential
  // arcs at a given state, make their label distinct by incrementing.


  explicit DeterminizeOptions(float d = kDelta, Weight w = Weight::Zero(),
                              StateId n = kNoStateId, Label l = 0,
                              DeterminizeType t = DETERMINIZE_FUNCTIONAL,
                              bool isl = false)
      : delta(d), weight_threshold(w), state_threshold(n),
        subsequential_label(l), type(t), increment_subsequential_label(isl) {}
};


// Determinizes a weighted transducer.  This version writes the
// determinized Fst to an output MutableFst.  The result will be an
// equivalent FST that has the property that no state has two
// transitions with the same input label.  For this algorithm, epsilon
// transitions are treated as regular symbols (cf. RmEpsilon).
//
// The transducer must be functional. The weights must be (weakly)
// left divisible (valid for TropicalWeight and LogWeight).
//
// Complexity:
// - Determinizable: exponential (polynomial in the size of the output)
// - Non-determinizable: does not terminate
//
// The determinizable automata include all unweighted and all acyclic input.
//
// References:
// - Mehryar Mohri, "Finite-State Transducers in Language and Speech
//   Processing". Computational Linguistics, 23:2, 1997.
template <class Arc>
void Determinize(const Fst<Arc> &ifst, MutableFst<Arc> *ofst,
             const DeterminizeOptions<Arc> &opts
                 = DeterminizeOptions<Arc>()) {
  typedef typename Arc::StateId StateId;
  typedef typename Arc::Weight Weight;

  DeterminizeFstOptions<Arc> nopts;
  nopts.delta = opts.delta;
  nopts.subsequential_label = opts.subsequential_label;
  nopts.type = opts.type;
  nopts.increment_subsequential_label = opts.increment_subsequential_label;

  nopts.gc_limit = 0;  // Cache only the last state for fastest copy.

  if (opts.weight_threshold != Weight::Zero() ||
      opts.state_threshold != kNoStateId) {
    if (ifst.Properties(kAcceptor, false)) {
      vector<Weight> idistance, odistance;
      ShortestDistance(ifst, &idistance, true);
      DeterminizeFst<Arc> dfst(ifst, &idistance, &odistance, nopts);
      PruneOptions< Arc, AnyArcFilter<Arc> > popts(opts.weight_threshold,
                                                   opts.state_threshold,
                                                   AnyArcFilter<Arc>(),
                                                   &odistance);
      Prune(dfst, ofst, popts);
    } else {
      *ofst = DeterminizeFst<Arc>(ifst, nopts);
      Prune(ofst, opts.weight_threshold, opts.state_threshold);
    }
  } else {
    *ofst = DeterminizeFst<Arc>(ifst, nopts);
  }
}

}  // namespace fst

#endif  // FST_LIB_DETERMINIZE_H__
