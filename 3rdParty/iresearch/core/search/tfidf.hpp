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

#ifndef IRESEARCH_TFIDF_H
#define IRESEARCH_TFIDF_H

#include "scorers.hpp"

namespace iresearch {

class tfidf_sort : public sort {
public:
  using score_t = float_t;

  static constexpr string_ref type_name() noexcept {
    return "tfidf";
  }

  static constexpr bool WITH_NORMS() noexcept {
    return false;
  }

  static constexpr bool BOOST_AS_SCORE() noexcept {
    return false;
  }

  // for use with irs::order::add<T>() and default args (static build)
  static sort::ptr make(
    bool normalize = WITH_NORMS(),
    bool boost_as_score = BOOST_AS_SCORE());

  explicit tfidf_sort(
    bool normalize = WITH_NORMS(),
    bool boost_as_score = BOOST_AS_SCORE()) noexcept;

  static void init(); // for trigering registration in a static build
  bool normalize() const noexcept { return normalize_; }
  void normalize(bool value) noexcept { normalize_ = value; }

  // use boost as score even if frequency is not set
  bool use_boost_as_score() const noexcept {
    return boost_as_score_;
  }
  void use_boost_as_score(bool use) noexcept {
    boost_as_score_ = use;
  }

  virtual sort::prepared::ptr prepare() const override;

 private:
  bool normalize_;
  bool boost_as_score_;
}; // tfidf_sort

}

#endif
