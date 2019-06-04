////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_ALL_ITERATOR_H
#define IRESEARCH_ALL_ITERATOR_H

#include "search/score_doc_iterators.hpp"

NS_ROOT

class all_iterator final : public irs::basic_doc_iterator_base {
 public:
  all_iterator(
    const irs::sub_reader& reader,
    const irs::attribute_store& prepared_filter_attrs,
    const irs::order::prepared& order,
    uint64_t docs_count
  );

  virtual bool next() override {
    return !doc_limits::eof(seek(doc_.value + 1));
  }

  virtual irs::doc_id_t seek(irs::doc_id_t target) override {
    doc_.value = target <= max_doc_
      ? target
      : doc_limits::eof();

    return doc_.value;
  }

  virtual irs::doc_id_t value() const NOEXCEPT override {
    return doc_.value;
  }

 private:
  irs::document doc_;
  irs::doc_id_t max_doc_; // largest valid doc_id
}; // all_iterator

NS_END // ROOT

#endif // IRESEARCH_ALL_ITERATOR_H
