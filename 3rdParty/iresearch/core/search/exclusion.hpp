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

#ifndef IRESEARCH_EXCLUSION_H
#define IRESEARCH_EXCLUSION_H

#include "index/iterators.hpp"

NS_ROOT

////////////////////////////////////////////////////////////////////////////////
/// @class exclusion
////////////////////////////////////////////////////////////////////////////////
class exclusion final : public doc_iterator {
 public:
  exclusion(doc_iterator::ptr&& incl, doc_iterator::ptr&& excl)
    : incl_(std::move(incl)), excl_(std::move(excl)) {
    assert(incl_);
    assert(excl_);
  }

  virtual doc_id_t value() const override {
    return incl_->value();
  }

  virtual bool next() override {
    if (!incl_->next()) {
      return false;
    }

    return !type_limits<type_t::doc_id_t>::eof(next(incl_->value()));
  }

  virtual doc_id_t seek(doc_id_t target) override {
    if (!type_limits<type_t::doc_id_t>::valid(target)) {
      return incl_->value();
    }

    if (type_limits<type_t::doc_id_t>::eof(target = incl_->seek(target))) {
      return target;
    }

    return next(target);
  }

  virtual const attribute_view& attributes() const NOEXCEPT override {
    return incl_->attributes();
  }

 private:
  // moves iterator to next not excluded
  // document not less than "target"
  doc_id_t next(doc_id_t target) {
    auto excl = excl_->value();

    if (excl < target) {
      excl = excl_->seek(target);
    }

    for (; excl == target; ) {
      if (!incl_->next()) {
        return incl_->value();
      }

      target = incl_->value();

      if (excl < target) {
        excl = excl_->seek(target);
      }
    }

    return target;
  }

  doc_iterator::ptr incl_;
  doc_iterator::ptr excl_;
}; // exclusion

NS_END // ROOT

#endif // IRESEARCH_EXCLUSION_H
