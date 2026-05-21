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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "index/index_reader.hpp"
#include "index/iterators.hpp"
#include "store/data_input.hpp"
#include "utils/attribute_provider.hpp"
#include "utils/attributes.hpp"
#include "utils/iterator.hpp"
#include "utils/string.hpp"
#include "utils/type_limits.hpp"

namespace irs {

// Represents token offset in a stream
struct offset final : attribute {
  static constexpr std::string_view type_name() noexcept { return "offset"; }

  void clear() noexcept {
    start = 0;
    end = 0;
  }

  uint32_t start{0};
  uint32_t end{0};
};

// Represents token increment in a stream
struct increment final : attribute {
  static constexpr std::string_view type_name() noexcept { return "increment"; }

  uint32_t value{1};
};

// Represents term value in a stream
struct term_attribute final : attribute {
  static constexpr std::string_view type_name() noexcept {
    return "term_attribute";
  }

  bytes_view value;
};

// Represents an arbitrary byte sequence associated with
// the particular term position in a field
struct payload final : attribute {
  // DO NOT CHANGE NAME
  static constexpr std::string_view type_name() noexcept { return "payload"; }

  bytes_view value;
};

// Contains a document identifier
struct document : attribute {
  // DO NOT CHANGE NAME
  static constexpr std::string_view type_name() noexcept { return "document"; }

  explicit document(irs::doc_id_t doc = irs::doc_limits::invalid()) noexcept
    : value(doc) {}

  doc_id_t value;
};

// Number of occurences of a term in a document
struct frequency final : attribute {
  // DO NOT CHANGE NAME
  static constexpr std::string_view type_name() noexcept { return "frequency"; }

  uint32_t value{0};
};

// Indexed tokens are prefixed with one byte indicating granularity
// this is marker attribute only used in field::features and by_range
// exact values are prefixed with 0
// the less precise the token the greater its granularity prefix value
struct granularity_prefix final {
  // DO NOT CHANGE NAME
  static constexpr std::string_view type_name() noexcept {
    return "iresearch::granularity_prefix";
  }
};

// Iterator representing term positions in a document
class position : public attribute, public attribute_provider {
 public:
  using value_t = uint32_t;
  using ref = std::reference_wrapper<position>;

  // DO NOT CHANGE NAME
  static constexpr std::string_view type_name() noexcept { return "position"; }

  static position& empty() noexcept;

  template<typename Provider>
  static position& get_mutable(Provider& attrs) {
    auto* pos = irs::get_mutable<position>(&attrs);
    return pos ? *pos : empty();
  }

  value_t value() const noexcept { return value_; }

  virtual bool next() = 0;

  virtual value_t seek(value_t /*target*/) { return pos_limits::invalid(); }

  virtual void reset() {}

 protected:
  value_t value_{pos_limits::invalid()};
};

// Subscription for attribute provider change
class attribute_provider_change final : public attribute {
 public:
  using callback_f = std::function<void(attribute_provider&)>;

  static constexpr std::string_view type_name() noexcept {
    return "attribute_provider_change";
  }

  void subscribe(callback_f&& callback) const {
    callback_ = std::move(callback);

    if (IRS_UNLIKELY(!callback_)) {
      callback_ = &noop;
    }
  }

  void operator()(attribute_provider& attrs) const {
    IRS_ASSERT(callback_);
    callback_(attrs);
  }

 private:
  static void noop(attribute_provider&) noexcept {}

  mutable callback_f callback_{&noop};
};

}  // namespace irs
