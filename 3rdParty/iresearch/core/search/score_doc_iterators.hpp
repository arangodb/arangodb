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

#ifndef IRESEARCH_SCORE_DOC_ITERATORS_H
#define IRESEARCH_SCORE_DOC_ITERATORS_H

#include "index/iterators.hpp"
#include "analysis/token_attributes.hpp"
#include "score.hpp"
#include "cost.hpp"

NS_ROOT

////////////////////////////////////////////////////////////////////////////////
/// @class doc_iterator_base
////////////////////////////////////////////////////////////////////////////////
template<typename T>
class IRESEARCH_API doc_iterator_base : public T {
 public:
  virtual const attribute_view& attributes() const noexcept {
    return attrs_;
  }

 protected:
  void estimate(cost::cost_f&& func) {
    cost_.rule(std::move(func));
    attrs_.emplace(cost_);
  }

  void estimate(cost::cost_t value) {
    cost_.value(value);
    attrs_.emplace(cost_);
  }

  void prepare_score(
      const order::prepared& order,
      const score_ctx* ctx,
      score_f func) {
    if (scr_.prepare(order, ctx, func)) {
      attrs_.emplace(scr_);
    }
  }

  void prepare_score(
      const order::prepared& order,
      order::prepared::scorers&& scorers) {
    if (scr_.prepare(order, std::move(scorers))) {
      attrs_.emplace(scr_);
    }
  }

  attribute_view attrs_;
  irs::cost cost_;
  irs::score scr_;
}; // doc_iterator_base

NS_END // ROOT

#endif
