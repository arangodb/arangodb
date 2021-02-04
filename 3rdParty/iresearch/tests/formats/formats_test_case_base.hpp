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

#ifndef IRESEARCH_FORMAT_TEST_CASE_BASE
#define IRESEARCH_FORMAT_TEST_CASE_BASE

#include "tests_shared.hpp"

#include "search/cost.hpp"

#include "index/doc_generator.hpp"
#include "index/index_tests.hpp"
#include "iql/query_builder.hpp"

#include "analysis/token_attributes.hpp"
#include "store/memory_directory.hpp"
#include "utils/version_utils.hpp"

// ----------------------------------------------------------------------------
// --SECTION--                                                   Base test case
// ----------------------------------------------------------------------------

namespace tests {

class format_test_case : public index_test_base {
 public:  
  class postings;

  class position final : public irs::position {
   public:
    explicit position(const irs::flags& features) {
      if (features.check<irs::offset>()) {
        poffs_ = &offs_;
      }

      if (features.check<irs::payload>()) {
        ppay_ = &pay_;
      }
    }

    attribute* get_mutable(irs::type_info::type_id type) noexcept override {
      if (irs::type<irs::offset>::id() == type) {
        return poffs_;
      }

      if (irs::type<irs::payload>::id() == type) {
        return ppay_;
      }

      return nullptr;
    }

    bool next() override {
      if (value_ == end_) {
        value_ = irs::type_limits<irs::type_t::pos_t>::eof();

        return false;
      }

      ++value_;

      const auto written = sprintf(pay_data_, "%d", value_);
      pay_.value = irs::bytes_ref(reinterpret_cast<const irs::byte_type*>(pay_data_), written);

      offs_.start = value_;
      offs_.end = offs_.start + written;
      return true;
    }

    void clear() {
      pay_.value = irs::bytes_ref::NIL;
      offs_.clear();
    }

    virtual void reset() override {
      assert(false); // unsupported
    }

   private:
    friend class postings;

    uint32_t end_;
    irs::offset offs_;
    irs::payload pay_;
    irs::offset* poffs_{};
    irs::payload* ppay_{};
    char pay_data_[21]; // enough to hold numbers up to max of uint64_t
  };

  class postings: public irs::doc_iterator {
   public:
    typedef std::vector<irs::doc_id_t> docs_t;
    typedef std::vector<irs::cost::cost_t> costs_t;

    postings(
        const docs_t::const_iterator& begin,
        const docs_t::const_iterator& end,
        const irs::flags& features = irs::flags::empty_instance())
      : next_(begin), end_(end), pos_(features) {
      attrs_[irs::type<irs::attribute_provider_change>::id()] = &callback_;
      if (features.check<irs::frequency>()) {
        freq_.value = 10;
        attrs_[irs::type<irs::frequency>::id()] = &freq_;
        if (features.check<irs::position>()) {
          attrs_[irs::type<irs::position>::id()] = &pos_;
        }
      }
    }

    bool next() override {
      if (!irs::doc_limits::valid(doc_)) {
        callback_(*this);
      }

      if (next_ == end_) {
        doc_ = irs::doc_limits::eof();
        return false;
      }

      doc_ = *next_;
      pos_.value_ = doc_;
      pos_.end_ = pos_.value_ + 10;
      pos_.clear();
      ++next_;

      return true;
    }

    irs::doc_id_t value() const override {
      return doc_;
    }

    irs::doc_id_t seek(irs::doc_id_t target) override {
      irs::seek(*this, target);
      return value();
    }

    irs::attribute* get_mutable(irs::type_info::type_id type) noexcept override {
      const auto it = attrs_.find(type);
      return it == attrs_.end() ? nullptr : it->second;
    }

   private:
    std::map<irs::type_info::type_id, irs::attribute*> attrs_;
    docs_t::const_iterator next_;
    docs_t::const_iterator end_;
    irs::frequency freq_;
    irs::attribute_provider_change callback_;
    tests::format_test_case::position pos_;
    irs::doc_id_t doc_{ irs::doc_limits::invalid() };
  }; // postings 

  template<typename Iterator>
  class terms: public irs::term_iterator {
   public:
    terms(const Iterator& begin, const Iterator& end)
      : next_(begin), end_(end) {
      docs_.push_back((irs::type_limits<irs::type_t::doc_id_t>::min)());
    }

    terms(const Iterator& begin, const Iterator& end,
        std::vector<irs::doc_id_t>::const_iterator doc_begin,
        std::vector<irs::doc_id_t>::const_iterator doc_end)
      : docs_(doc_begin, doc_end), next_(begin), end_(end) {
    }

    bool next() {
      if (next_ == end_) {
        return false;
      }

      val_ = irs::ref_cast<irs::byte_type>(*next_);
      ++next_;
      return true;
    }

    const irs::bytes_ref& value() const {
      return val_;
    }

    irs::doc_iterator::ptr postings(const irs::flags& /*features*/) const {
      return irs::memory::make_managed<format_test_case::postings>(
        docs_.begin(), docs_.end());
    }

    void read() { }

    irs::attribute* get_mutable(irs::type_info::type_id) noexcept {
      return nullptr;
    }

   private:
    irs::bytes_ref val_;
    std::vector<irs::doc_id_t> docs_;
    Iterator next_;
    Iterator end_;
  }; // terms

  void assert_no_directory_artifacts(
      const iresearch::directory& dir,
      const iresearch::format& codec,
      const std::unordered_set<std::string>& expect_additional = {}) {
    std::vector<std::string> dir_files;
    auto visitor = [&dir_files] (std::string& file) {
      // ignore lock file present in fs_directory
      if (iresearch::index_writer::WRITE_LOCK_NAME != file) {
        dir_files.emplace_back(std::move(file));
      }      
      return true;
    };
    ASSERT_TRUE(dir.visit(visitor));

    iresearch::index_meta index_meta;
    std::string segment_file;

    auto reader = codec.get_index_meta_reader();
    std::unordered_set<std::string> index_files(expect_additional.begin(), expect_additional.end());
    const bool exists = reader->last_segments_file(dir, segment_file);

    if (exists) {
      reader->read(dir, index_meta, segment_file);

      index_meta.visit_files([&index_files] (const std::string& file) {
        index_files.emplace(file);
        return true;
      });

      index_files.insert(segment_file);
    }

    for (auto& file: dir_files) {
      ASSERT_TRUE(index_files.erase(file) == 1);
    }

    ASSERT_TRUE(index_files.empty());
  }
}; // format_test_case

} // tests

namespace iresearch {

// use base irs::position type for ancestors
template<>
struct type<tests::format_test_case::position> : type<irs::position> { };

}

#endif // IRESEARCH_FORMAT_TEST_CASE_BASE
