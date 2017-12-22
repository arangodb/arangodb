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
  explicit doc_iterator_base(const order::prepared& ord);

  void estimate(cost::cost_f&& func) {
    cost_.rule(std::move(func));
    attrs_.emplace(cost_);
  }

  void estimate(cost::cost_t value) {
    cost_.value(value);
    attrs_.emplace(cost_);
  }

  void prepare_score(score::score_f&& func) {
    if (scr_.prepare(*ord_, std::move(func))) {
      attrs_.emplace(scr_);
    }
  }

  attribute_view attrs_;
  irs::cost cost_;
  irs::score scr_;
  const order::prepared* ord_;
}; // doc_iterator_base

////////////////////////////////////////////////////////////////////////////////
/// @class basic_doc_iterator
/// @brief basic implementation of scoring iterator for single term queries
////////////////////////////////////////////////////////////////////////////////
class basic_doc_iterator final : public doc_iterator_base {
 public:
   basic_doc_iterator(
     const sub_reader& segment,
     const term_reader& field,
     const attribute_store& stats,
     doc_iterator::ptr&& it,
     const order::prepared& ord,
     cost::cost_t estimation) NOEXCEPT;

  virtual doc_id_t value() const override {
    return it_->value();
  }

  virtual bool next() override {
    return it_->next();
  }

  virtual doc_id_t seek(doc_id_t target) override {
    return it_->seek(target);
  }

 private:
  order::prepared::scorers scorers_;
  doc_iterator::ptr it_;
  const attribute_store* stats_;
}; // basic_doc_iterator

NS_END // ROOT

#endif
