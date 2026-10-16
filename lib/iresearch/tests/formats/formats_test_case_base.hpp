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

#pragma once

#include <algorithm>
#include <unordered_set>

#include "analysis/token_attributes.hpp"
#include "index/index_tests.hpp"

namespace tests {

class MockTermReader final : public irs::basic_term_reader {
 public:
  explicit MockTermReader(irs::term_iterator& it, irs::field_meta meta,
                          irs::bytes_view min_term, irs::bytes_view max_term)
    : it_{it},
      meta_{std::move(meta)},
      min_term_{min_term},
      max_term_(max_term) {}

 private:
  irs::attribute* get_mutable(irs::type_info::type_id /*type*/) final {
    return nullptr;
  }
  irs::term_iterator::ptr iterator() const final {
    return irs::memory::to_managed<irs::term_iterator>(it_);
  }
  const irs::field_meta& meta() const final { return meta_; }
  irs::bytes_view min() const final { return min_term_; }
  irs::bytes_view max() const final { return max_term_; }

  irs::term_iterator& it_;
  irs::field_meta meta_;
  irs::bytes_view min_term_;
  irs::bytes_view max_term_;
};

class format_test_case : public index_test_base {
 public:
  class postings;

  class position final : public irs::position {
   public:
    explicit position(irs::IndexFeatures features) {
      if (irs::IndexFeatures::NONE != (features & irs::IndexFeatures::OFFS)) {
        poffs_ = &offs_;
      }

      if (irs::IndexFeatures::NONE != (features & irs::IndexFeatures::PAY)) {
        ppay_ = &pay_;
      }
    }

    attribute* get_mutable(irs::type_info::type_id type) noexcept final {
      if (irs::type<irs::offset>::id() == type) {
        return poffs_;
      }

      if (irs::type<irs::payload>::id() == type) {
        return ppay_;
      }

      return nullptr;
    }

    bool next() final {
      if (value_ == end_) {
        value_ = end_ = irs::pos_limits::eof();

        return false;
      }

      ++value_;
      EXPECT_TRUE(irs::pos_limits::valid(value_));

      const auto written = sprintf(pay_data_, "%d", value_);
      pay_.value = irs::bytes_view(
        reinterpret_cast<const irs::byte_type*>(pay_data_), written);

      offs_.start = value_;
      offs_.end = offs_.start + written;
      return true;
    }

    void clear() {
      pay_.value = {};
      offs_.clear();
    }

    void reset() final {
      IRS_ASSERT(false);  // unsupported
    }

   private:
    friend class postings;

    uint32_t end_;
    irs::offset offs_;
    irs::payload pay_;
    irs::offset* poffs_{};
    irs::payload* ppay_{};
    char pay_data_[21];  // enough to hold numbers up to max of uint64_t
  };

  class postings : public irs::doc_iterator {
   public:
    // DocId + Freq
    using docs_t = std::span<const std::pair<irs::doc_id_t, uint32_t>>;

    postings(std::span<const std::pair<irs::doc_id_t, uint32_t>> docs,
             irs::IndexFeatures features = irs::IndexFeatures::NONE)
      : next_(std::begin(docs)), end_(std::end(docs)), pos_(features) {
      attrs_[irs::type<irs::document>::id()] = &doc_;
      attrs_[irs::type<irs::attribute_provider_change>::id()] = &callback_;
      if (irs::IndexFeatures::NONE != (features & irs::IndexFeatures::FREQ)) {
        freq_.value = 0;
        attrs_[irs::type<irs::frequency>::id()] = &freq_;
        if (irs::IndexFeatures::NONE != (features & irs::IndexFeatures::POS)) {
          attrs_[irs::type<irs::position>::id()] = &pos_;
        }
      }
    }

    bool next() final {
      if (!irs::doc_limits::valid(doc_.value)) {
        callback_(*this);
      }

      if (next_ == end_) {
        doc_.value = irs::doc_limits::eof();
        return false;
      }

      std::tie(doc_.value, freq_.value) = *next_;

      EXPECT_TRUE(irs::doc_limits::valid(doc_.value));
      pos_.value_ = doc_.value;
      EXPECT_TRUE(irs::pos_limits::valid(pos_.value_));
      pos_.end_ = pos_.value_ + freq_.value;
      pos_.clear();
      ++next_;

      return true;
    }

    irs::doc_id_t value() const final { return doc_.value; }

    irs::doc_id_t seek(irs::doc_id_t target) final {
      irs::seek(*this, target);
      return value();
    }

    irs::attribute* get_mutable(irs::type_info::type_id type) noexcept final {
      const auto it = attrs_.find(type);
      return it == attrs_.end() ? nullptr : it->second;
    }

   private:
    std::map<irs::type_info::type_id, irs::attribute*> attrs_;
    docs_t::iterator next_;
    docs_t::iterator end_;
    irs::frequency freq_;
    irs::attribute_provider_change callback_;
    format_test_case::position pos_;
    irs::document doc_;
  };

  irs::ColumnInfo lz4_column_info() const noexcept;
  irs::ColumnInfo none_column_info() const noexcept;

  bool supports_encryption() const noexcept {
    // old formats don't support columnstore headers
    constexpr std::string_view kOldFormats[]{"1_0"};

    const auto it = std::find(std::begin(kOldFormats), std::end(kOldFormats),
                              codec()->type()().name());
    return std::end(kOldFormats) == it;
  }

  bool supports_columnstore_headers() const noexcept {
    // old formats don't support columnstore headers
    constexpr std::string_view kOldFormats[]{"1_0", "1_1", "1_2", "1_3",
                                             "1_3simd"};

    const auto it = std::find(std::begin(kOldFormats), std::end(kOldFormats),
                              codec()->type()().name());
    return std::end(kOldFormats) == it;
  }

  template<typename Iterator>
  class terms : public irs::term_iterator {
   public:
    using docs_type = std::vector<std::pair<irs::doc_id_t, uint32_t>>;

    terms(const Iterator& begin, const Iterator& end)
      : next_(begin), end_(end) {
      IRS_ASSERT(std::is_sorted(begin, end));
      docs_.emplace_back((irs::doc_limits::min)(), 0);
    }

    terms(const Iterator& begin, const Iterator& end,
          docs_type::const_iterator doc_begin,
          docs_type::const_iterator doc_end)
      : docs_(doc_begin, doc_end), next_(begin), end_(end) {
      IRS_ASSERT(std::is_sorted(begin, end));
    }

    bool next() final {
      if (next_ == end_) {
        return false;
      }

      val_ = *next_;
      ++next_;
      return true;
    }

    irs::bytes_view value() const final { return val_; }

    irs::doc_iterator::ptr postings(
      irs::IndexFeatures /*features*/) const final {
      return irs::memory::make_managed<format_test_case::postings>(docs_);
    }

    void read() final {}

    irs::attribute* get_mutable(irs::type_info::type_id) noexcept final {
      return nullptr;
    }

   private:
    irs::bytes_view val_;
    docs_type docs_;
    Iterator next_;
    Iterator end_;
  };

  void AssertFrequencyAndPositions(irs::doc_iterator& expected,
                                   irs::doc_iterator& actual);

  void AssertNoDirectoryArtifacts(
    const irs::directory& dir, const irs::format& codec,
    const std::unordered_set<std::string>& expect_additional = {});
};

class format_test_case_with_encryption : public format_test_case {};

}  // namespace tests

namespace irs {

// use base irs::position type for ancestors
template<>
struct type<tests::format_test_case::position> : type<irs::position> {};

}  // namespace irs
