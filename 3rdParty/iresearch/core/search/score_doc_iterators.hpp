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
#include "filter.hpp"
#include "cost.hpp"

NS_ROOT

////////////////////////////////////////////////////////////////////////////////
/// @class score_doc_iterator_base
////////////////////////////////////////////////////////////////////////////////
class IRESEARCH_API score_doc_iterator_base: public score_doc_iterator {
 public:
  virtual const attribute_store& attributes() const NOEXCEPT override final {
    return attrs_;
  }

 protected:
  score_doc_iterator_base(const order::prepared& ord);

  attribute_store attrs_;
  iresearch::score* scr_;
  const order::prepared* ord_;
}; // score_doc_iterator_base

////////////////////////////////////////////////////////////////////////////////
/// @class basic_score_iterator 
/// @brief basic implementation of scoring iterator for single term queries
////////////////////////////////////////////////////////////////////////////////
class basic_score_iterator final : public score_doc_iterator_base {
 public:
   basic_score_iterator(
     const sub_reader& segment,
     const term_reader& field,
     const attribute_store& stats,
     doc_iterator::ptr&& it,
     const order::prepared& ord,
     cost::cost_t estimation) NOEXCEPT;

  virtual void score() override {
    scr_->clear();
    scorers_.score(*ord_, scr_->leak());
  }

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
}; // basic_score_iterator    

NS_END // ROOT

#endif
