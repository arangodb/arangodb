//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

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
