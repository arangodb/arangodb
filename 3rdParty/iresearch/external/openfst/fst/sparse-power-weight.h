// sparse-power-weight.h

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
// Author: krr@google.com (Kasturi Rangan Raghavan)
// Inspiration: allauzen@google.com (Cyril Allauzen)
//
// \file
// Cartesian power weight semiring operation definitions.
// Uses SparseTupleWeight as underlying representation.

#ifndef FST_LIB_SPARSE_POWER_WEIGHT_H__
#define FST_LIB_SPARSE_POWER_WEIGHT_H__

#include<string>

#include <fst/sparse-tuple-weight.h>
#include <fst/weight.h>


namespace fst {

// Below SparseTupleWeight*Mapper are used in conjunction with
// SparseTupleWeightMap to compute the respective semiring operations
template<class W, class K>
struct SparseTupleWeightPlusMapper {
  W Map(const K& k, const W& v1, const W& v2) const {
    return Plus(v1, v2);
  }
};

template<class W, class K>
struct SparseTupleWeightTimesMapper {
  W Map(const K& k, const W& v1, const W& v2) const {
    return Times(v1, v2);
  }
};

template<class W, class K>
struct SparseTupleWeightDivideMapper {
  SparseTupleWeightDivideMapper(DivideType divide_type) {
    divide_type_ = divide_type;
  }
  W Map(const K& k, const W& v1, const W& v2) const {
    return Divide(v1, v2, divide_type_);
  }
  DivideType divide_type_;
};

template<class W, class K>
struct SparseTupleWeightApproxMapper {
  SparseTupleWeightApproxMapper(float delta) { delta_ = delta; }
  W Map(const K& k, const W& v1, const W& v2) const {
    return ApproxEqual(v1, v2, delta_) ? W::One() : W::Zero();
  }
  float delta_;
};

// Sparse cartesian power semiring: W ^ n
// Forms:
//  - a left semimodule when W is a left semiring,
//  - a right semimodule when W is a right semiring,
//  - a bisemimodule when W is a semiring,
//    the free semimodule of rank n over W
// The Times operation is overloaded to provide the
// left and right scalar products.
// K is the key value type. kNoKey(-1) is reserved for internal use
template <class W, class K = int>
class SparsePowerWeight : public SparseTupleWeight<W, K> {
 public:
  using SparseTupleWeight<W, K>::Zero;
  using SparseTupleWeight<W, K>::One;
  using SparseTupleWeight<W, K>::NoWeight;
  using SparseTupleWeight<W, K>::Quantize;
  using SparseTupleWeight<W, K>::Reverse;

  typedef SparsePowerWeight<typename W::ReverseWeight, K> ReverseWeight;

  SparsePowerWeight() {}

  SparsePowerWeight(const SparseTupleWeight<W, K> &w) :
    SparseTupleWeight<W, K>(w) { }

  template <class Iterator>
  SparsePowerWeight(Iterator begin, Iterator end) :
    SparseTupleWeight<W, K>(begin, end) { }

  SparsePowerWeight(const K &key, const W &w) :
    SparseTupleWeight<W, K>(key, w) { }

  static const SparsePowerWeight<W, K> &Zero() {
    static const SparsePowerWeight<W, K> zero(SparseTupleWeight<W, K>::Zero());
    return zero;
  }

  static const SparsePowerWeight<W, K> &One() {
    static const SparsePowerWeight<W, K> one(SparseTupleWeight<W, K>::One());
    return one;
  }

  static const SparsePowerWeight<W, K> &NoWeight() {
    static const SparsePowerWeight<W, K> no_weight(
        SparseTupleWeight<W, K>::NoWeight());
    return no_weight;
  }

  // Overide this: Overwrite the Type method to reflect the key type
  // if using non-default key type.
  static const string &Type() {
    static string type;
    if(type.empty()) {
      type = W::Type() + "_^n";
      if(sizeof(K) != sizeof(uint32)) {
        string size;
        Int64ToStr(8 * sizeof(K), &size);
        type += "_" + size;
      }
    }
    return type;
  }

  static uint64 Properties() {
    uint64 props = W::Properties();
    return props & (kLeftSemiring | kRightSemiring |
                    kCommutative | kIdempotent);
  }

  SparsePowerWeight<W, K> Quantize(float delta = kDelta) const {
    return SparseTupleWeight<W, K>::Quantize(delta);
  }

  ReverseWeight Reverse() const {
    return SparseTupleWeight<W, K>::Reverse();
  }
};

// Semimodule plus operation
template <class W, class K>
inline SparsePowerWeight<W, K> Plus(const SparsePowerWeight<W, K> &w1,
                                    const SparsePowerWeight<W, K> &w2) {
  SparsePowerWeight<W, K> ret;
  SparseTupleWeightPlusMapper<W, K> operator_mapper;
  SparseTupleWeightMap(&ret, w1, w2, operator_mapper);
  return ret;
}

// Semimodule times operation
template <class W, class K>
inline SparsePowerWeight<W, K> Times(const SparsePowerWeight<W, K> &w1,
                                     const SparsePowerWeight<W, K> &w2) {
  SparsePowerWeight<W, K> ret;
  SparseTupleWeightTimesMapper<W, K> operator_mapper;
  SparseTupleWeightMap(&ret, w1, w2, operator_mapper);
  return ret;
}

// Semimodule divide operation
template <class W, class K>
inline SparsePowerWeight<W, K> Divide(const SparsePowerWeight<W, K> &w1,
                                      const SparsePowerWeight<W, K> &w2,
                                      DivideType type = DIVIDE_ANY) {
  SparsePowerWeight<W, K> ret;
  SparseTupleWeightDivideMapper<W, K> operator_mapper(type);
  SparseTupleWeightMap(&ret, w1, w2, operator_mapper);
  return ret;
}

// Semimodule dot product
template <class W, class K>
inline const W& DotProduct(const SparsePowerWeight<W, K> &w1,
                    const SparsePowerWeight<W, K> &w2) {
  const SparsePowerWeight<W, K>& product = Times(w1, w2);
  W ret(W::Zero());
  for (SparseTupleWeightIterator<W, K> it(product); !it.Done(); it.Next()) {
    ret = Plus(ret, it.Value().second);
  }
  return ret;
}

template <class W, class K>
inline bool ApproxEqual(const SparsePowerWeight<W, K> &w1,
                        const SparsePowerWeight<W, K> &w2,
                        float delta = kDelta) {
  SparseTupleWeight<W, K> ret;
  SparseTupleWeightApproxMapper<W, K> operator_mapper(kDelta);
  SparseTupleWeightMap(&ret, w1, w2, operator_mapper);
  return ret == SparsePowerWeight<W, K>::One();
}

template <class W, class K>
inline SparsePowerWeight<W, K> Times(const W &k,
                                     const SparsePowerWeight<W, K> &w2) {
  SparsePowerWeight<W, K> w1(k);
  return Times(w1, w2);
}

template <class W, class K>
inline SparsePowerWeight<W, K> Times(const SparsePowerWeight<W, K> &w1,
                                     const W &k) {
  SparsePowerWeight<W, K> w2(k);
  return Times(w1, w2);
}

template <class W, class K>
inline SparsePowerWeight<W, K> Divide(const SparsePowerWeight<W, K> &w1,
                                      const W &k,
                                      DivideType divide_type = DIVIDE_ANY) {
  SparsePowerWeight<W, K> w2(k);
  return Divide(w1, w2, divide_type);
}

}  // namespace fst

#endif  // FST_LIB_SPARSE_POWER_WEIGHT_H__
