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

#ifndef IRESEARCH_INDEX_TESTS_H
#define IRESEARCH_INDEX_TESTS_H

#include "tests_shared.hpp"
#include "tests_param.hpp"
#include "assert_format.hpp"
#include "analysis/analyzers.hpp"
#include "analysis/token_streams.hpp"
#include "analysis/token_attributes.hpp"
#include "index/directory_reader.hpp"
#include "index/index_writer.hpp"
#include "doc_generator.hpp"
#include "utils/timer_utils.hpp"

using namespace std::chrono_literals;

namespace iresearch {

struct term_attribute;

}

namespace tests {

class directory_mock: public irs::directory {
 public:
  directory_mock(irs::directory& impl): impl_(impl) {}

  using directory::attributes;

  virtual irs::directory_attributes& attributes() noexcept override {
    return impl_.attributes();
  }

  virtual irs::index_output::ptr create(
      const std::string& name) noexcept override {
    return impl_.create(name);
  }

  virtual bool exists(
      bool& result, const std::string& name) const noexcept override {
    return impl_.exists(result, name);
  }

  virtual bool length(
      uint64_t& result, const std::string& name) const noexcept override {
    return impl_.length(result, name);
  }

  virtual irs::index_lock::ptr make_lock(
      const std::string& name) noexcept override {
    return impl_.make_lock(name);
  }

  virtual bool mtime(
      std::time_t& result, const std::string& name) const noexcept override {
    return impl_.mtime(result, name);
  }

  virtual irs::index_input::ptr open(
      const std::string& name,
      irs::IOAdvice advice) const noexcept override {
    return impl_.open(name, advice);
  }

  virtual bool remove(const std::string& name) noexcept override {
    return impl_.remove(name);
  }

  virtual bool rename(
      const std::string& src, const std::string& dst) noexcept override {
    return impl_.rename(src, dst);
  }

  virtual bool sync(const std::string& name) noexcept override {
    return impl_.sync(name);
  }

  virtual bool visit(const irs::directory::visitor_f& visitor) const override {
    return impl_.visit(visitor);
  }

 private:
  irs::directory& impl_;
}; // directory_mock

struct blocking_directory : directory_mock {
  explicit blocking_directory(irs::directory& impl, const std::string& blocker)
    : tests::directory_mock(impl), blocker(blocker) {
  }

  irs::index_output::ptr create(const std::string& name) noexcept {
    auto stream = tests::directory_mock::create(name);

    if (name == blocker) {
      {
        auto guard = irs::make_unique_lock(policy_lock);
        policy_applied.notify_all();
      }

      // wait for intermediate commits to be applied
      auto guard = irs::make_unique_lock(intermediate_commits_lock);
    }

    return stream;
  }

  void wait_for_blocker() {
    bool has = false;
    exists(has, blocker);

    while (!has) {
      exists(has, blocker);

      auto policy_guard = irs::make_unique_lock(policy_lock);
      policy_applied.wait_for(policy_guard, 1000ms);
    }
  }

  std::string blocker;
  std::mutex policy_lock;
  std::condition_variable policy_applied;
  std::mutex intermediate_commits_lock;
}; // blocking_directory

struct callback_directory : directory_mock {
  typedef std::function<void()> AfterCallback;

  explicit callback_directory(irs::directory& impl, AfterCallback&& p)
    : tests::directory_mock(impl), after(p) {
  }

  irs::index_output::ptr create(const std::string& name) noexcept override {
    auto stream = tests::directory_mock::create(name);
    after();
    return stream;
  }

  AfterCallback after;
}; // callback_directory

struct format_info {
  constexpr format_info(
      const char* codec = nullptr,
      const char* module = nullptr) noexcept
    : codec(codec),
      module(module) {
  }

  const char* codec;
  const char* module;
};

typedef std::tuple<tests::dir_param_f, format_info> index_test_context;

class index_test_base : public virtual test_param_base<index_test_context> {
 public:
  static std::string to_string(const testing::TestParamInfo<index_test_context>& info);

 protected:
  std::shared_ptr<irs::directory> get_directory(const test_base& ctx) const;

  irs::format::ptr get_codec() const;

  irs::directory& dir() const { return *dir_; }
  irs::format::ptr codec() const { return codec_; }
  const index_t& index() const { return index_; }
  index_t& index() { return index_; }

  void sort(const irs::comparer& comparator) {
    for (auto& segment : index_) {
      segment.sort(comparator);
    }
  }

  irs::index_writer::ptr open_writer(
      irs::directory& dir,
      irs::OpenMode mode = irs::OM_CREATE,
      const irs::index_writer::init_options& options = {}) const {
    return irs::index_writer::make(dir, codec_, mode, options);
  }

  irs::index_writer::ptr open_writer(
      irs::OpenMode mode = irs::OM_CREATE,
      const irs::index_writer::init_options& options = {}) const {
    return irs::index_writer::make(*dir_, codec_, mode, options);
  }

  irs::directory_reader open_reader() const {
    return irs::directory_reader::open(*dir_, codec_);
  }

  void assert_index(irs::IndexFeatures features,
                    size_t skip = 0,
                    irs::automaton_table_matcher* matcher = nullptr) const {
    tests::assert_index(static_cast<irs::index_reader::ptr>(open_reader()),
                        index(), features, skip, matcher);
  }

  void assert_columnstore(size_t skip = 0) const {
    tests::assert_columnstore(static_cast<irs::index_reader::ptr>(open_reader()),
                              index(), skip);
  }

  virtual void SetUp() override {
    test_base::SetUp();

    // set directory
    dir_ = get_directory(*this);
    ASSERT_NE(nullptr, dir_);

    // set codec
    codec_ = get_codec();
    ASSERT_NE(nullptr, codec_);
  }

  virtual void TearDown() override {
    dir_ = nullptr;
    codec_ = nullptr;
    test_base::TearDown();
    irs::timer_utils::init_stats(); // disable profile state tracking
  }

  void write_segment(
    irs::index_writer& writer,
    tests::index_segment& segment,
    tests::doc_generator_base& gen);

  void add_segment(
    irs::index_writer& writer,
    tests::doc_generator_base& gen);

  void add_segments(
    irs::index_writer& writer,
    std::vector<doc_generator_base::ptr>& gens);

  void add_segment(
    tests::doc_generator_base& gen,
    irs::OpenMode mode = irs::OM_CREATE,
    const irs::index_writer::init_options& opts = {});

 private:
  index_t index_;
  std::shared_ptr<irs::directory> dir_;
  irs::format::ptr codec_;
}; // index_test_base

} // tests

#endif // IRESEARCH_INDEX_TESTS_H
