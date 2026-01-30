////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2023 ArangoDB GmbH, Cologne, Germany
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
#include "index/buffered_column.hpp"
#include "search/cost.hpp"
#include "utils/attribute_helper.hpp"

namespace irs {

class BufferedColumnIterator : public doc_iterator {
 public:
  BufferedColumnIterator(std::span<const BufferedValue> values,
                         bytes_view data) noexcept
    : begin_{values.data()},
      next_{begin_},
      end_{begin_ + values.size()},
      data_{data} {
    std::get<cost>(attrs_).reset(values.size());
  }

  attribute* get_mutable(irs::type_info::type_id type) noexcept final {
    return irs::get_mutable(attrs_, type);
  }

  doc_id_t value() const noexcept final {
    return std::get<document>(attrs_).value;
  }

  doc_id_t seek(doc_id_t target) noexcept final {
    // Currently the iterator is only used for access during the segment
    // flushing. We intentionally allow iterator to seek backwards.
    // We expect a lot of dense ranges.
    const auto* curr = next_ == end_ ? begin_ : next_;
    curr = (curr + target) - curr->key;

    if (IRS_UNLIKELY(curr < begin_ || end_ <= curr || curr->key != target)) {
      curr = std::lower_bound(
        begin_, end_, target,
        [](const BufferedValue& value, doc_id_t target) noexcept {
          return value.key < target;
        });
    }

    next_ = curr;
    next();
    return value();
  }

  bool next() noexcept final {
    auto& doc = std::get<document>(attrs_);

    if (IRS_UNLIKELY(next_ == end_)) {
      doc.value = doc_limits::eof();
      return false;
    }

    auto& payload = std::get<irs::payload>(attrs_);

    doc.value = next_->key;
    payload.value = {data_.data() + next_->begin, next_->size};

    ++next_;

    return true;
  }

 private:
  using attributes = std::tuple<document, cost, irs::payload>;

  attributes attrs_;
  const BufferedValue* begin_;
  const BufferedValue* next_;
  const BufferedValue* end_;
  bytes_view data_;
};

}  // namespace irs
