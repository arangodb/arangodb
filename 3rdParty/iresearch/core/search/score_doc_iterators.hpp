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
class IRESEARCH_API doc_iterator_base : public doc_iterator {
 public:
  virtual const attribute_view& attributes() const NOEXCEPT override final {
    return attrs_;
  }

 protected:
  doc_iterator_base() = default;

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
      const void* ctx,
      score::score_f func) {
    if (scr_.prepare(order, ctx, func)) {
      attrs_.emplace(scr_);
    }
  }

  attribute_view attrs_;
  irs::cost cost_;
  irs::score scr_;
}; // doc_iterator_base

////////////////////////////////////////////////////////////////////////////////
/// @class doc_iterator_base
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API basic_doc_iterator_base : public doc_iterator_base {
 public:
  basic_doc_iterator_base() = default;

 protected:
  // intenitonally hides doc_iterator_base::prepare_score(...)
  void prepare_score(
    const order::prepared& order,
    order::prepared::scorers&& scorers
  );

 private:
  order::prepared::scorers scorers_;
}; // basic_doc_iterator_base

////////////////////////////////////////////////////////////////////////////////
/// @class basic_doc_iterator
/// @brief basic implementation of scoring iterator for single term queries
////////////////////////////////////////////////////////////////////////////////
class basic_doc_iterator final : public basic_doc_iterator_base {
 public:
   basic_doc_iterator(
     const sub_reader& segment,
     const term_reader& field,
     const byte_type* stats,
     doc_iterator::ptr&& it,
     const order::prepared& ord,
     cost::cost_t estimation,
     boost_t boost) NOEXCEPT;

  virtual doc_id_t value() const NOEXCEPT override {
    // this function is executed very frequently, to avoid expensive virtual call
    // and optimize it, we directly access document attribute of wrapped iterator
    assert(doc_);
    return doc_->value;
  }

  virtual bool next() override {
    return it_->next();
  }

  virtual doc_id_t seek(doc_id_t target) override {
    return it_->seek(target);
  }

 private:
  doc_iterator::ptr it_;
  const irs::document* doc_{};
  const byte_type* stats_;
}; // basic_doc_iterator

NS_END // ROOT

#endif
