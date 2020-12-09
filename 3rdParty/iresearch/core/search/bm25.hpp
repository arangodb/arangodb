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
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_BM25_H
#define IRESEARCH_BM25_H

#include "scorers.hpp"

namespace iresearch {

class bm25_sort : public sort {
 public:
  using score_t = float_t;

  static constexpr string_ref type_name() noexcept {
    return "bm25";
  }

  static constexpr float_t K() noexcept {
    return 1.2f;
  }

  static constexpr float_t B() noexcept {
    return 0.75f;
  }

  static constexpr bool BOOST_AS_SCORE() noexcept {
    return false;
  }

  static void init(); // for trigering registration in a static build

  static ptr make(
    float_t k = K(),
    float_t b = B(),
    bool boost_as_score = BOOST_AS_SCORE());

  explicit bm25_sort(
   float_t k = K(),
   float_t b = B(),
   bool boost_as_score = BOOST_AS_SCORE()) noexcept;

  float_t k() const noexcept { return k_; }
  void k(float_t k) noexcept { k_ = k; }

  float_t b() const noexcept { return b_; }
  void b(float_t b) noexcept { b_ = b; }

  // use boost as score even if frequency is not set
  bool use_boost_as_score() const noexcept {
    return boost_as_score_;
  }
  void use_boost_as_score(bool use) noexcept {
    boost_as_score_ = use;
  }

  // returns 'true' if current scorer is 'bm11'
  bool bm11() const noexcept {
    return b_ == 1.f;
  }

  // returns 'true' if current scorer is 'bm15'
  bool bm15() const noexcept {
    return b_ == 0.f;
  }

  virtual sort::prepared::ptr prepare() const override;

 private:
  float_t k_; // [1.2 .. 2.0]
  float_t b_; // 0.75
  bool boost_as_score_;
}; // bm25_sort

}

#endif
