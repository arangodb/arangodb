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
////////////////////////////////////////////////////////////////////////////////

#ifndef IRESEARCH_ALL_ITERATOR_H
#define IRESEARCH_ALL_ITERATOR_H

#include "analysis/token_attributes.hpp"
#include "index/iterators.hpp"
#include "index/index_reader.hpp"
#include "search/sort.hpp"
#include "search/cost.hpp"
#include "search/score.hpp"
#include "utils/frozen_attributes.hpp"

NS_ROOT

class all_iterator final
    : public frozen_attributes<3, doc_iterator> {
 public:
  all_iterator(
    const irs::sub_reader& reader,
    const byte_type* query_stats,
    const irs::order::prepared& order,
    uint64_t docs_count,
    boost_t boost);

  virtual bool next() noexcept override {
    return !doc_limits::eof(seek(doc_.value + 1));
  }

  virtual irs::doc_id_t seek(irs::doc_id_t target) noexcept override {
    doc_.value = target <= max_doc_
      ? target
      : doc_limits::eof();

    return doc_.value;
  }

  virtual irs::doc_id_t value() const noexcept override {
    return doc_.value;
  }

 private:
  document doc_;
  score score_;
  cost cost_;
  doc_id_t max_doc_; // largest valid doc_id
}; // all_iterator

NS_END // ROOT

#endif // IRESEARCH_ALL_ITERATOR_H
