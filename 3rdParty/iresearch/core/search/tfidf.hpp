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

#ifndef IRESEARCH_TFIDF_H
#define IRESEARCH_TFIDF_H

#include "scorers.hpp"

NS_ROOT

class tfidf_sort : public sort {
public:
  DECLARE_SORT_TYPE();

  static CONSTEXPR bool WITH_NORMS() NOEXCEPT {
    return false;
  }

  // for use with irs::order::add<T>() and default args (static build)
  DECLARE_FACTORY();

  typedef float_t score_t;

  explicit tfidf_sort(bool normalize = WITH_NORMS());

  static void init(); // for trigering registration in a static build
  bool normalize() const { return normalize_; }
  void normalize(bool value) { normalize_ = value; }

  virtual sort::prepared::ptr prepare() const;

private:
  bool normalize_;
}; // tfidf_sort

NS_END

#endif