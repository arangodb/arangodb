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

#pragma once

#include "analysis/token_attributes.hpp"
#include "index/index_reader.hpp"
#include "index/iterators.hpp"
#include "search/cost.hpp"
#include "search/score.hpp"
#include "search/scorer.hpp"
#include "utils/attribute_helper.hpp"

namespace irs {

class AllIterator : public doc_iterator {
 public:
  AllIterator(const irs::SubReader& reader, const byte_type* query_stats,
              const irs::Scorers& order, uint64_t docs_count, score_t boost);

  attribute* get_mutable(irs::type_info::type_id id) noexcept final {
    return irs::get_mutable(attrs_, id);
  }

  bool next() noexcept final {
    auto& doc = std::get<document>(attrs_);

    if (doc.value >= max_doc_) {
      doc.value = doc_limits::eof();
      return false;
    } else {
      doc.value++;
      return true;
    }
  }

  irs::doc_id_t seek(irs::doc_id_t target) noexcept final {
    auto& doc = std::get<document>(attrs_);

    doc.value = target <= max_doc_ ? target : doc_limits::eof();

    return doc.value;
  }

  irs::doc_id_t value() const noexcept final {
    return std::get<document>(attrs_).value;
  }

 private:
  using attributes = std::tuple<document, cost, score>;

  doc_id_t max_doc_;  // largest valid doc_id
  attributes attrs_;
};

}  // namespace irs
