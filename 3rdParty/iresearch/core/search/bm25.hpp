////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2016 by EMC Corporation, All Rights Reserved
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is EMC Corporation
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_BM25_H
#define IRESEARCH_BM25_H

#include "scorers.hpp"

NS_ROOT

class bm25_sort : public sort {
 public:
  DECLARE_SORT_TYPE();

  static CONSTEXPR float_t K() NOEXCEPT {
    return 1.2f;
  }

  static CONSTEXPR float_t B() NOEXCEPT {
    return 0.75f;
  }

  static void init(); // for trigering registration in a static build

  // for use with irs::order::add<T>() and default args (static build)
  DECLARE_FACTORY_DEFAULT();

  typedef float_t score_t;

  bm25_sort(float_t k = K(), float_t b = B());

  float_t k() const { return k_; }
  void k(float_t k) { k_ = k; }

  float_t b() const { return b_; }
  void b(float_t b) { b_ = b; }

  // returns 'true' if current scorer is 'bm11'
  bool bm11() const NOEXCEPT {
    return b_ == 1.f;
  }

  // returns 'true' if current scorer is 'bm15'
  bool bm15() const NOEXCEPT {
    return b_ == 0.f;
  }

  virtual sort::prepared::ptr prepare() const override;

 private:
  float_t k_; // [1.2 .. 2.0]
  float_t b_; // 0.75
}; // bm25_sort

NS_END

#endif
