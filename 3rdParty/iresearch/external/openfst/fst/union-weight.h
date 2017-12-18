// union-weight.h

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
// Union weight set and associated semiring operation definitions.
//
// TODO(riley): add in normalizer functor

#ifndef FST_LIB_UNION_WEIGHT_H_
#define FST_LIB_UNION_WEIGHT_H_

#include <stack>
#include <string>

#include <fst/weight.h>


namespace fst {


// Example UnionWeightOptions for UnionWeight template param below.
// The Merge operation is used to collapse elements of the set and the
// Compare function to efficiently implement the merge.  In the
// simplest case, the merge would just apply with equality of set
// elements so the result is a set (and not a multiset). More
// generally, this can be used to maintain the multiplicity or other
// such weight associated with the set elements (cf Gallic).

// template <class W>
// struct UnionWeightOptions {
//   // Used for example 'Merge' below
//   struct first {
//     W operator()(const W &w1, const W &w2) { return w1; }
//   };
//
//   // Comparison function C is a total order on W that is monotonic
//   // wrt to Times: for all a,b,c!=Zero(): C(a,b) => C(ca, cb) and
//   // is anti-monotonic wrt to Divide: C(a,b) => C(c/b,c/a).
//   // For all a,b: only one of C(a,b), C(b,a) or a~b must true where ~
//   // is an equivalence relation on W. Also we require a~b iff
//   // a.Reverse()~b.Reverse().
//   typedef NaturalLess<W> Compare;
//   //
//   // How to combine two weights if a~b as above. For all a,b:
//   // a~b => merge(a,b) ~ a. Merge must define a semiring endomorphism
//   // from the unmerged weight sets to the merged weight sets.
//   typedef first Merge;
//   //
//   // For ReverseWeight
//   typedef UnionWeightOptions<ReverseWeight> ReverseOptions;
// };

template <class W, class O>
class UnionWeight;

template <class W, class O>
class UnionWeightIterator;

template <class W, class O>
class UnionWeightReverseIterator;

template <class W, class O>
bool operator==(const UnionWeight<W, O> &,  const UnionWeight<W, O> &);

// Semiring that uses Times() and One() from W and union and the empty set
// for Plus() and Zero(), resp.  Template argument O specifies the
// union weight options as above.
template<class W, class O>
class UnionWeight {
 public:
  typedef W Weight;
  typedef typename O::Compare Compare;
  typedef typename O::Merge Merge;

  typedef UnionWeight<typename W::ReverseWeight,
                      typename O::ReverseOptions> ReverseWeight;

  friend class UnionWeightIterator<W, O>;
  friend class UnionWeightReverseIterator<W, O>;
  friend bool operator==<>(const UnionWeight<W, O> &,
                           const UnionWeight<W, O> &);

  // Set represented as first_ weight + rest_ weights.
  // Uses first_ as NoWeight() to indicate the union weight Zero() ask
  // the empty set.  Uses rest_ containing NoWeight() to indicate the
  // union weight NoWeight().
  UnionWeight() : first_(W::NoWeight()) { }

  explicit UnionWeight(W w) : first_(w) {
    if (w == W::NoWeight())
      rest_.push_back(w);
  }

  static const UnionWeight<W, O> &Zero() {
    static const UnionWeight<W, O> zero(W::NoWeight());
    return zero;
  }

  static const UnionWeight<W, O> &One() {
    static const UnionWeight<W, O> one(W::One());
    return one;
  }

  static const UnionWeight<W, O> &NoWeight() {
    static const UnionWeight<W, O> no_weight(W::Zero(), W::NoWeight());
    return no_weight;
  }

  static const string &Type() {
    static const string type = W::Type() + "_union";
    return type;
  }

  static uint64 Properties() {
    uint64 props = W::Properties();
    return props & (kLeftSemiring | kRightSemiring |
                    kCommutative | kIdempotent);
  }

  bool Member() const;

  istream &Read(istream &strm); // NOLINT

  ostream &Write(ostream &strm) const;  // NOLINT

  size_t Hash() const;

  UnionWeight<W, O> Quantize(float delta = kDelta) const;

  ReverseWeight Reverse() const;

  // These operations combined with the UnionWeightIterator
  // and UnionWeightReverseIterator provide the access and
  // mutation of the union weight internal elements.

  // Common initializer among constructors.
  // Clear existing UnionWeight.
  void Clear() { first_ = W::NoWeight(); rest_.clear(); }

  size_t Size() const {
    return first_.Member() ? rest_.size() + 1 : 0;
  }

  const W &Back() const {
    if (rest_.empty())
      return first_;
    else
      return rest_.back();
  }

  // When srt is true, assumes elements added sorted wrt Compare and
  // merging of weights performed as needed. Otherwise, just ensures
  // first_ is the least element wrt Compare.
  void PushBack(W w, bool srt);

  // Sorts the elements of the set.  Assumes that first_, if present,
  // is the least element.
  void Sort() {
    Compare cmp;
    rest_.sort(cmp);
  }

 private:
  W &Back() {
    if (rest_.empty())
      return first_;
    else
      return rest_.back();
  }

  UnionWeight(W w1, W w2) : first_(w1), rest_(1, w2) {}

  W first_;         // first weight in set
  list<W> rest_;    // remaining weights in set
};


template<class W, class O>
void UnionWeight<W, O>::PushBack(W w, bool srt) {
  Compare cmp;
  Merge merge;

  if (!w.Member()) {
    rest_.push_back(w);
  } else if (!first_.Member()) {
    first_ = w;
  } else if (srt) {
    W &back = Back();
    if (cmp(back, w)) {
      rest_.push_back(w);
    } else {
      back = merge(back, w);
    }
  } else {
    if (cmp(first_, w)) {
      rest_.push_back(w);
    } else {
      rest_.push_back(first_);
      first_ = w;
    }
  }
}

// Traverses union weight in the forward direction.
template <class W, class O>
class UnionWeightIterator {
 public:
  explicit UnionWeightIterator(const UnionWeight<W, O>& w)
      : first_(w.first_), rest_(w.rest_), init_(true),
        iter_(rest_.begin()) {}

  bool Done() const {
    if (init_)
      return !first_.Member();
    else
      return iter_ == rest_.end();
  }

  const W& Value() const { return init_ ? first_ : *iter_; }

  void Next() {
    if (init_) init_ = false;
    else  ++iter_;
  }

  void Reset() {
    init_ = true;
    iter_ = rest_.begin();
  }

 private:
  const W &first_;
  const list<W> &rest_;
  bool init_;   // in the initialized state?
  typename list<W>::const_iterator iter_;

  DISALLOW_COPY_AND_ASSIGN(UnionWeightIterator);
};


// Traverses union weight in backward direction.
template <typename L, class O>
class UnionWeightReverseIterator {
 public:
  explicit UnionWeightReverseIterator(const UnionWeight<L, O>& w)
      : first_(w.first_), rest_(w.rest_), fin_(!first_.Member()),
        iter_(rest_.rbegin()) {}

  bool Done() const { return fin_; }

  const L& Value() const { return iter_ == rest_.rend() ? first_ : *iter_; }

  void Next() {
    if (iter_ == rest_.rend()) fin_ = true;
    else  ++iter_;
  }

  void Reset() {
    fin_ = !first_.Member();
    iter_ = rest_.rbegin();
  }

 private:
  const L &first_;
  const list<L> &rest_;
  bool fin_;   // in the final state?
  typename list<L>::const_reverse_iterator iter_;

  DISALLOW_COPY_AND_ASSIGN(UnionWeightReverseIterator);
};


// UnionWeight member functions follow that require
// UnionWeightIterator
template <class W, class O>
inline istream &UnionWeight<W, O>::Read(istream &strm) {  // NOLINT
  Clear();
  int32 size;
  ReadType(strm, &size);
  for (int i = 0; i < size; ++i) {
    W w;
    ReadType(strm, &w);
    PushBack(w, true);
  }
  return strm;
}

template <class W, class O>
inline ostream &UnionWeight<W, O>::Write(ostream &strm) const {  // NOLINT
  int32 size =  Size();
  WriteType(strm, size);
  for (UnionWeightIterator<W, O> iter(*this); !iter.Done(); iter.Next()) {
    const W &w = iter.Value();
    WriteType(strm, w);
  }
  return strm;
}

template <class W, class O>
inline bool UnionWeight<W, O>::Member() const {
  if (Size() <= 1)
    return true;
  for (UnionWeightIterator<W, O> iter(*this); !iter.Done(); iter.Next()) {
    const W &w = iter.Value();
    if (!w.Member())
      return false;
  }
  return true;
}

template <class W, class O>
inline UnionWeight<W, O> UnionWeight<W, O>::Quantize(float delta) const {
  UnionWeight<W, O> w;
  for (UnionWeightIterator<W, O> iter(*this); !iter.Done(); iter.Next())
    w.PushBack(iter.Value().Quantize(delta), true);
  return w;
}

template <class W, class O>
inline typename UnionWeight<W, O>::ReverseWeight
UnionWeight<W, O>::Reverse() const {
  ReverseWeight rw;
  for (UnionWeightIterator<W, O> iter(*this);
       !iter.Done();
       iter.Next()) {
    rw.PushBack(iter.Value().Reverse(), false);
  }
  rw.Sort();
  return rw;
}

template <class W, class O>
inline size_t UnionWeight<W, O>::Hash() const {
  size_t h = 0;
  for (UnionWeightIterator<W, O> iter(*this); !iter.Done(); iter.Next())
    h ^= h << 1 ^ iter.Value().Hash();
  return h;
}

// Requires union weight has been canonicalized.
template <class W, class O>
inline bool operator==(const UnionWeight<W, O> &w1,
                       const UnionWeight<W, O> &w2) {
  if (w1.Size() != w2.Size())
    return false;

  UnionWeightIterator<W, O> iter1(w1);
  UnionWeightIterator<W, O> iter2(w2);

  for (; !iter1.Done() ; iter1.Next(), iter2.Next())
    if (iter1.Value() != iter2.Value())
      return false;

  return true;
}

// Requires union weight has been canonicalized.
template <class W, class O>
inline bool operator!=(const UnionWeight<W, O> &w1,
                       const UnionWeight<W, O> &w2) {
  return !(w1 == w2);
}

// Requires union weight has been canonicalized.
template <class W, class O>
inline bool ApproxEqual(const UnionWeight<W, O> &w1,
                        const UnionWeight<W, O> &w2,
                        float delta = kDelta) {
  if (w1.Size() != w2.Size())
    return false;

  UnionWeightIterator<W, O> iter1(w1);
  UnionWeightIterator<W, O> iter2(w2);

  for (; !iter1.Done() ; iter1.Next(), iter2.Next())
    if (!ApproxEqual(iter1.Value(), iter2.Value(), delta))
      return false;

  return true;
}

template <class W, class O>
inline ostream &operator<<(ostream &strm, const UnionWeight<W, O> &w) {
  UnionWeightIterator<W, O> iter(w);
  if (iter.Done()) {
    return strm << "EmptySet";
  } else if (!w.Member()) {
    return strm << "BadSet";
  } else {
    CompositeWeightWriter writer(strm);
    writer.WriteBegin();
    for (; !iter.Done(); iter.Next())
      writer.WriteElement(iter.Value());
    writer.WriteEnd();
  }
  return strm;
}

template <class W, class O>
inline istream &operator>>(istream &strm, UnionWeight<W, O> &w) {
  string s;
  strm >> s;
  if (s == "EmptySet") {
    w = UnionWeight<W, O>::Zero();
  } else if (s == "BadSet") {
    w = UnionWeight<W, O>::NoWeight();
  } else {
    w = UnionWeight<W, O>::Zero();
    istringstream sstrm(s);
    CompositeWeightReader reader(sstrm);
    reader.ReadBegin();
    bool more = true;
    while (more) {
      W v;
      more = reader.ReadElement(&v);
      w.PushBack(v, true);
    }
    reader.ReadEnd();
  }
  return strm;
}

template <class W, class O>
inline UnionWeight<W, O> Plus(const UnionWeight<W, O> &w1,
                              const UnionWeight<W, O> &w2) {
    if (!w1.Member() || !w2.Member())
      return UnionWeight<W, O>::NoWeight();
    if (w1 == UnionWeight<W, O>::Zero())
      return w2;
    if (w2 == UnionWeight<W, O>::Zero())
      return w1;

    UnionWeightIterator<W, O> iter1(w1);
    UnionWeightIterator<W, O> iter2(w2);
    UnionWeight<W, O> sum;

    typename O::Compare cmp;
    while (!iter1.Done() && !iter2.Done()) {
      W v1  = iter1.Value();
      W v2  = iter2.Value();
      if (cmp(v1, v2)) {
        sum.PushBack(v1, true);
        iter1.Next();
      } else {
        sum.PushBack(v2, true);
        iter2.Next();
      }
    }
    for (; !iter1.Done(); iter1.Next())
      sum.PushBack(iter1.Value(), true);
    for (; !iter2.Done(); iter2.Next())
      sum.PushBack(iter2.Value(), true);
    return sum;
}

template <class W, class O>
inline UnionWeight<W, O> Times(const UnionWeight<W, O> &w1,
                             const UnionWeight<W, O> &w2) {
  if (!w1.Member() || !w2.Member())
    return UnionWeight<W, O>::NoWeight();
  if (w1 == UnionWeight<W, O>::Zero() || w2 == UnionWeight<W, O>::Zero())
    return UnionWeight<W, O>::Zero();

  UnionWeightIterator<W, O> iter1(w1);
  UnionWeightIterator<W, O> iter2(w2);

  UnionWeight<W, O> prod1;
  for (; !iter1.Done() ; iter1.Next()) {
    UnionWeight<W, O> prod2;
    for (; !iter2.Done() ; iter2.Next())
      prod2.PushBack(Times(iter1.Value(), iter2.Value()), true);
    prod1 = Plus(prod1, prod2);
    iter2.Reset();
  }
  return prod1;
}

template <class W, class O>
inline UnionWeight<W, O> Divide(const UnionWeight<W, O> &w1,
                                const UnionWeight<W, O> &w2,
                                DivideType typ) {
  if (!w1.Member() || !w2.Member())
    return UnionWeight<W, O>::NoWeight();
  if (w1 == UnionWeight<W, O>::Zero() || w2 == UnionWeight<W, O>::Zero())
    return UnionWeight<W, O>::Zero();

  UnionWeightIterator<W, O> iter1(w1);
  UnionWeightReverseIterator<W, O> iter2(w2);
  UnionWeight<W, O> quot;

  if (w1.Size() == 1) {
    for (; !iter2.Done(); iter2.Next())
      quot.PushBack(Divide(iter1.Value(), iter2.Value(), typ), true);
  } else if (w2.Size() == 1) {
    for (; !iter1.Done() ; iter1.Next())
      quot.PushBack(Divide(iter1.Value(), iter2.Value(), typ), true);
  } else {
    quot = UnionWeight<W, O>::NoWeight();
  }
  return quot;
}

}  // namespace fst

#endif  // FST_LIB_UNION_WEIGHT_H_
