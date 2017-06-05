// random-weight.h

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
// Function objects to generate random weights in various semirings
// for testing purposes.

#ifndef FST_LIB_RANDOM_WEIGHT_H__
#define FST_LIB_RANDOM_WEIGHT_H__

#include <cstdlib>
#include <ctime>
#include <vector>
using std::vector;


#include <fst/float-weight.h>
#include <fst/product-weight.h>
#include <fst/string-weight.h>
#include <fst/lexicographic-weight.h>
#include <fst/power-weight.h>
#include <fst/signed-log-weight.h>
#include <fst/sparse-power-weight.h>
#include <fst/union-weight.h>


namespace fst {

// The boolean 'allow_zero' below determines whether Zero() and zero
// divisors should be returned in the random weight generation.

// This function object returns TropicalWeightTpl<T>'s that are random integers
// chosen from [0, kNumRandomWeights).
template <class T>
class TropicalWeightGenerator_ {
 public:
  typedef TropicalWeightTpl<T> Weight;

  explicit TropicalWeightGenerator_(int seed = time(0), bool allow_zero = true)
      : allow_zero_(allow_zero) {
    srand(seed);
  }

  Weight operator() () const {
    int n = rand() % (kNumRandomWeights + allow_zero_);
    if (allow_zero_ && n == kNumRandomWeights)
      return Weight::Zero();

    return Weight(static_cast<T>(n));
  }

 private:
  // The number of alternative random weights.
  static const int kNumRandomWeights = 5;

  bool allow_zero_;  // permit Zero() and zero divisors
};

template <class T> const int TropicalWeightGenerator_<T>::kNumRandomWeights;

typedef TropicalWeightGenerator_<float> TropicalWeightGenerator;


// This function object returns LogWeightTpl<T>'s that are random integers
// chosen from [0, kNumRandomWeights).
template <class T>
class LogWeightGenerator_ {
 public:
  typedef LogWeightTpl<T> Weight;

  explicit LogWeightGenerator_(int seed = time(0), bool allow_zero = true)
      : allow_zero_(allow_zero) {
    srand(seed);
  }

  Weight operator() () const {
    int n = rand() % (kNumRandomWeights + allow_zero_);
    if (allow_zero_ && n == kNumRandomWeights)
      return Weight::Zero();

    return Weight(static_cast<T>(n));
  }

 private:
  // Number of alternative random weights.
  static const int kNumRandomWeights = 5;

  bool allow_zero_;  // permit Zero() and zero divisors
};

template <class T> const int LogWeightGenerator_<T>::kNumRandomWeights;

typedef LogWeightGenerator_<float> LogWeightGenerator;


// This function object returns MinMaxWeightTpl<T>'s that are random integers
// chosen from (-kNumRandomWeights, kNumRandomWeights) in addition to
// One(), and Zero() if zero is allowed.
template <class T>
class MinMaxWeightGenerator_ {
 public:
  typedef MinMaxWeightTpl<T> Weight;

  explicit MinMaxWeightGenerator_(int seed = time(0), bool allow_zero = true)
      : allow_zero_(allow_zero) {
    srand(seed);
  }

  Weight operator() () const {
    int n = (rand() % (2*kNumRandomWeights + allow_zero_)) - kNumRandomWeights;
    if (allow_zero_ && n == kNumRandomWeights)
      return Weight::Zero();
    else if (n == -kNumRandomWeights)
      return Weight::One();

    return Weight(static_cast<T>(n));
  }

 private:
  // Parameters controlling the number of alternative random weights.
  static const int kNumRandomWeights = 5;

  bool allow_zero_;  // permit Zero() and zero divisors
};

template <class T> const int MinMaxWeightGenerator_<T>::kNumRandomWeights;

typedef MinMaxWeightGenerator_<float> MinMaxWeightGenerator;


// This function object returns StringWeights that are random integer
// strings chosen from {1,...,kAlphabetSize}^{0,kMaxStringLength} U { Zero }
template <typename L, StringType S = STRING_LEFT>
class StringWeightGenerator {
 public:
  typedef StringWeight<L, S> Weight;

  explicit StringWeightGenerator(int seed = time(0), bool allow_zero = true)
      : allow_zero_(allow_zero) {
     srand(seed);
  }

  Weight operator() () const {
    int n = rand() % (kMaxStringLength + allow_zero_);
    if (allow_zero_ && n == kMaxStringLength)
      return Weight::Zero();

    vector<L> v;
    for (int i = 0; i < n; ++i)
      v.push_back(rand() % kAlphabetSize + 1);
    return Weight(v.begin(), v.end());
  }

 private:
  // Alphabet size for random weights.
  static const int kAlphabetSize = 5;
  // Number of alternative random weights.
  static const int kMaxStringLength = 5;

  bool allow_zero_;  // permit Zero() and zero
};

template <typename L, StringType S>
const int StringWeightGenerator<L, S>::kAlphabetSize;
template <typename L, StringType S>
const int StringWeightGenerator<L, S>::kMaxStringLength;


// This function object returns a weight generator over the union of the
// weights (by default) for the generators G. Class O is the options
// class for the union.
template <class G, class O>
class UnionWeightGenerator {
 public:
  typedef typename G::Weight W;;
  typedef UnionWeight<W, O> Weight;

  explicit UnionWeightGenerator(int seed = time(0), bool allow_zero = true)
      : generator_(seed, false), allow_zero_(allow_zero) {}

  Weight operator() () const {
    int n = rand() % (kNumRandomWeights + 1);
    if (allow_zero_ && n == kNumRandomWeights) {
      return Weight::Zero();
    } else if (n % 2 == 0) {
      W w = generator_();
      return Weight(w);
    } else {
      W w1 = generator_();
      W w2 = generator_();
      return Plus(Weight(w1), Weight(w2));
    }
  }

 private:
  // The number of alternative random weights.
  static const int kNumRandomWeights = 5;

  G generator_;
  bool allow_zero_;
};

template <class G, class O>
const int UnionWeightGenerator<G, O>::kNumRandomWeights;


// This function object returns a weight generator over the product of the
// weights (by default) for the generators G1 and G2.
template <class G1, class G2,
  class W = ProductWeight<typename G1::Weight, typename G2::Weight> >
class ProductWeightGenerator {
 public:
  typedef typename G1::Weight W1;
  typedef typename G2::Weight W2;
  typedef W Weight;

  explicit ProductWeightGenerator(int seed = time(0), bool allow_zero = true)
      : generator1_(seed, allow_zero), generator2_(seed, allow_zero) {}

  Weight operator() () const {
    W1 w1 = generator1_();
    W2 w2 = generator2_();
    return Weight(w1, w2);
  }

 private:
  G1 generator1_;
  G2 generator2_;
};


// This function object returns a weight generator for a lexicographic weight
// composed out of weights for the generators G1 and G2. For lexicographic
// weights, we cannot generate zeroes for the two subweights separately:
// weights are members iff both members are zero or both members are non-zero.
template <class G1, class G2>
class LexicographicWeightGenerator {
 public:
  typedef typename G1::Weight W1;
  typedef typename G2::Weight W2;
  typedef LexicographicWeight<W1, W2> Weight;

  explicit LexicographicWeightGenerator(int seed = time(0),
                                        bool allow_zero = true)
      : generator1_(seed, false), generator2_(seed, false),
        allow_zero_(allow_zero) {}

  Weight operator() () const {
    if (allow_zero_) {
      int n = rand() % (kNumRandomWeights + 1);
      if (n == kNumRandomWeights)
        return Weight(W1::Zero(), W2::Zero());
    }
    W1 w1 = generator1_();
    W2 w2 = generator2_();
    return Weight(w1, w2);
  }

 private:
  // The number of alternative random weights.
  static const int kNumRandomWeights = 5;

  G1 generator1_;
  G2 generator2_;
  bool allow_zero_;
};

template <class G1, class G2>
const int LexicographicWeightGenerator<G1, G2>::kNumRandomWeights;


// Product generator of a string weight generator and an
// arbitrary weight generator.
template <class L, class G, GallicType T = GALLIC_LEFT>
class GallicWeightGenerator
    : public ProductWeightGenerator<
               StringWeightGenerator<L, GALLIC_STRING_TYPE(T)>, G> {
 public:
  typedef ProductWeightGenerator<
               StringWeightGenerator<L, GALLIC_STRING_TYPE(T)>, G> PG;
  typedef typename G::Weight W;
  typedef GallicWeight<L, W, T> Weight;

  explicit GallicWeightGenerator(int seed = time(0), bool allow_zero = true)
      : PG(seed, allow_zero) {}

  explicit GallicWeightGenerator(const PG &pg) : PG(pg) {}
};

// Specialization for (general) gallic weight.
template <class L, class G>
class GallicWeightGenerator<L, G, GALLIC>
    : public UnionWeightGenerator<
                GallicWeightGenerator<L, G, GALLIC_RESTRICT>,
                GallicUnionWeightOptions<L, typename G::Weight> > {
 public:
  typedef UnionWeightGenerator<
                GallicWeightGenerator<L, G, GALLIC_RESTRICT>,
                GallicUnionWeightOptions<L, typename G::Weight> > UG;
  typedef typename G::Weight W;

  explicit GallicWeightGenerator(int seed = time(0), bool allow_zero = true)
      : UG(seed, allow_zero) {}

  explicit GallicWeightGenerator(const UG &ug) : UG(ug) {}
};


// This function object returms a weight generator over the catersian power
// of rank n of the weights for the generator G.
template <class G, unsigned int n>
class PowerWeightGenerator {
 public:
  typedef typename G::Weight W;
  typedef PowerWeight<W, n> Weight;

  explicit PowerWeightGenerator(int seed = time(0), bool allow_zero = true)
      : generator_(seed, allow_zero) {}

  Weight operator()() const {
    Weight w;
    for (size_t i = 0; i < n; ++i) {
      W r = generator_();
      w.SetValue(i, r);
    }
    return w;
  }

 private:
  G generator_;
};

// This function object returns SignedLogWeightTpl<T>'s that are
// random integers chosen from [0, kNumRandomWeights).
// The sign is randomly chosen as well.
template <class T>
class SignedLogWeightGenerator_ {
 public:
  typedef SignedLogWeightTpl<T> Weight;

  explicit SignedLogWeightGenerator_(int seed = time(0),
                                     bool allow_zero = true)
  : allow_zero_(allow_zero) {
    srand(seed);
  }

  Weight operator() () const {
    int m = rand() % 2;
    int n = rand() % (kNumRandomWeights + allow_zero_);

    return SignedLogWeightTpl<T>(
      (m == 0) ?
        TropicalWeight(-1.0) :
        TropicalWeight(1.0),
      (allow_zero_ && n == kNumRandomWeights) ?
        LogWeightTpl<T>::Zero() :
        LogWeightTpl<T>(static_cast<T>(n)));
  }

 private:
  // Number of alternative random weights.
  static const int kNumRandomWeights = 5;
  bool allow_zero_;  // permit Zero() and zero divisors
};

template <class T> const int SignedLogWeightGenerator_<T>::kNumRandomWeights;

typedef SignedLogWeightGenerator_<float> SignedLogWeightGenerator;

// This function object returms a weight generator over the catersian power
// of rank n of the weights for the generator G.
template <class G, class K, unsigned int n>
class SparsePowerWeightGenerator {
 public:
  typedef typename G::Weight W;
  typedef SparsePowerWeight<W, K> Weight;

  explicit SparsePowerWeightGenerator(int seed = time(0),
                                      bool allow_zero = true)
      : generator_(seed, allow_zero) {}

  Weight operator()() const {
    Weight w;
    for (size_t i = 1; i <= n; ++i) {
      W r = generator_();
      K p = i;
      w.Push(p, r, true);
    }
    return w;
  }

 private:
  G generator_;
};

}  // namespace fst

#endif  // FST_LIB_RANDOM_WEIGHT_H__
