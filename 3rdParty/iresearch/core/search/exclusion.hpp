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

#ifndef IRESEARCH_EXCLUSION_H
#define IRESEARCH_EXCLUSION_H

#include "analysis/token_attributes.hpp"
#include "index/iterators.hpp"

NS_ROOT

////////////////////////////////////////////////////////////////////////////////
/// @class exclusion
////////////////////////////////////////////////////////////////////////////////
class exclusion final : public doc_iterator {
 public:
  exclusion(doc_iterator::ptr&& incl, doc_iterator::ptr&& excl) NOEXCEPT
    : incl_(std::move(incl)), excl_(std::move(excl)) {
    assert(incl_);
    assert(excl_);
    incl_doc_ = incl_->attributes().get<document>().get();
    excl_doc_ = excl_->attributes().get<document>().get();
    assert(incl_doc_);
    assert(excl_doc_);
  }

  virtual doc_id_t value() const override {
    return incl_doc_->value;
  }

  virtual bool next() override {
    if (!incl_->next()) {
      return false;
    }

    return !doc_limits::eof(next(incl_doc_->value));
  }

  virtual doc_id_t seek(doc_id_t target) override {
    if (!doc_limits::valid(target)) {
      return incl_doc_->value;
    }

    if (doc_limits::eof(target = incl_->seek(target))) {
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
    auto excl = excl_doc_->value;

    if (excl < target) {
      excl = excl_->seek(target);
    }

    for (; excl == target; ) {
      if (!incl_->next()) {
        return incl_doc_->value;
      }

      target = incl_doc_->value;

      if (excl < target) {
        excl = excl_->seek(target);
      }
    }

    return target;
  }

  doc_iterator::ptr incl_;
  doc_iterator::ptr excl_;
  const document* incl_doc_;
  const document* excl_doc_;
}; // exclusion

NS_END // ROOT

#endif // IRESEARCH_EXCLUSION_H
