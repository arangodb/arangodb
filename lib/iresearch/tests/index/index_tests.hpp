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

#include "analysis/analyzers.hpp"
#include "analysis/token_attributes.hpp"
#include "analysis/token_streams.hpp"
#include "assert_format.hpp"
#include "doc_generator.hpp"
#include "index/directory_reader.hpp"
#include "index/directory_reader_impl.hpp"
#include "index/index_writer.hpp"
#include "tests_param.hpp"
#include "tests_shared.hpp"
#include "utils/timer_utils.hpp"

using namespace std::chrono_literals;

namespace irs {

struct term_attribute;

}

namespace tests {

class directory_mock : public irs::directory {
 public:
  directory_mock(irs::directory& impl) : impl_(impl) {}

  using directory::attributes;

  irs::directory_attributes& attributes() noexcept final {
    return impl_.attributes();
  }

  irs::index_output::ptr create(std::string_view name) noexcept override {
    return impl_.create(name);
  }

  bool exists(bool& result, std::string_view name) const noexcept override {
    return impl_.exists(result, name);
  }

  bool length(uint64_t& result, std::string_view name) const noexcept override {
    return impl_.length(result, name);
  }

  irs::index_lock::ptr make_lock(std::string_view name) noexcept override {
    return impl_.make_lock(name);
  }

  bool mtime(std::time_t& result,
             std::string_view name) const noexcept override {
    return impl_.mtime(result, name);
  }

  irs::index_input::ptr open(std::string_view name,
                             irs::IOAdvice advice) const noexcept override {
    return impl_.open(name, advice);
  }

  bool remove(std::string_view name) noexcept override {
    return impl_.remove(name);
  }

  bool rename(std::string_view src, std::string_view dst) noexcept override {
    return impl_.rename(src, dst);
  }

  bool sync(std::span<const std::string_view> files) noexcept override {
    return impl_.sync(files);
  }

  bool visit(const irs::directory::visitor_f& visitor) const final {
    return impl_.visit(visitor);
  }

 private:
  irs::directory& impl_;
};

struct blocking_directory : directory_mock {
  explicit blocking_directory(irs::directory& impl, const std::string& blocker)
    : tests::directory_mock(impl), blocker(blocker) {}

  irs::index_output::ptr create(std::string_view name) noexcept final {
    auto stream = tests::directory_mock::create(name);

    if (name == blocker) {
      {
        auto guard = std::unique_lock(policy_lock);
        policy_applied.notify_all();
      }

      // wait for intermediate commits to be applied
      auto guard = std::unique_lock(intermediate_commits_lock);
    }

    return stream;
  }

  void wait_for_blocker() {
    bool has = false;
    exists(has, blocker);

    while (!has) {
      exists(has, blocker);

      auto policy_guard = std::unique_lock(policy_lock);
      policy_applied.wait_for(policy_guard, 1000ms);
    }
  }

  std::string blocker;
  std::mutex policy_lock;
  std::condition_variable policy_applied;
  std::mutex intermediate_commits_lock;
};

struct callback_directory : directory_mock {
  typedef std::function<void()> AfterCallback;

  explicit callback_directory(irs::directory& impl, AfterCallback&& p)
    : tests::directory_mock(impl), after(p) {}

  irs::index_output::ptr create(std::string_view name) noexcept final {
    auto stream = tests::directory_mock::create(name);
    after();
    return stream;
  }

  AfterCallback after;
};

struct format_info {
  constexpr format_info(const char* codec = "",
                        const char* module = "") noexcept
    : codec(codec), module(module) {}

  const char* codec;
  const char* module;
};

typedef std::tuple<tests::dir_param_f, format_info> index_test_context;

void AssertSnapshotEquality(irs::DirectoryReader lhs, irs::DirectoryReader rhs);

class index_test_base : public virtual test_param_base<index_test_context> {
 public:
  static std::string to_string(
    const testing::TestParamInfo<index_test_context>& info);

 protected:
  std::shared_ptr<irs::directory> get_directory(const test_base& ctx) const;

  irs::format::ptr get_codec() const;

  irs::directory& dir() const { return *dir_; }
  irs::format::ptr codec() const { return codec_; }
  const index_t& index() const { return index_; }
  index_t& index() { return index_; }

  void sort(const irs::Comparer& comparator) {
    for (auto& segment : index_) {
      segment.sort(comparator);
    }
  }

  irs::IndexWriter::ptr open_writer(
    irs::directory& dir, irs::OpenMode mode = irs::OM_CREATE,
    const irs::IndexWriterOptions& options = {}) const {
    return irs::IndexWriter::Make(dir, codec_, mode, options);
  }

  irs::IndexWriter::ptr open_writer(
    irs::OpenMode mode = irs::OM_CREATE,
    const irs::IndexWriterOptions& options = {}) const {
    return irs::IndexWriter::Make(*dir_, codec_, mode, options);
  }

  irs::DirectoryReader open_reader(
    const irs::IndexReaderOptions& options = {}) const {
    return irs::DirectoryReader{*dir_, codec_, options};
  }

  void AssertSnapshotEquality(const irs::IndexWriter& writer);

  void assert_index(irs::IndexFeatures features, size_t skip = 0,
                    irs::automaton_table_matcher* matcher = nullptr) const {
    tests::assert_index(open_reader().GetImpl(), index(), features, skip,
                        matcher);
  }

  void assert_columnstore(size_t skip = 0) const {
    tests::assert_columnstore(open_reader().GetImpl(), index(), skip);
  }

  void SetUp() final {
    test_base::SetUp();

    // set directory
    dir_ = get_directory(*this);
    ASSERT_NE(nullptr, dir_);

    // set codec
    codec_ = get_codec();
    ASSERT_NE(nullptr, codec_);
  }

  void TearDown() final {
    dir_ = nullptr;
    codec_ = nullptr;
    test_base::TearDown();
    irs::timer_utils::init_stats();  // disable profile state tracking
  }

  void write_segment(irs::IndexWriter& writer, tests::index_segment& segment,
                     tests::doc_generator_base& gen);

  void add_segment(irs::IndexWriter& writer, tests::doc_generator_base& gen);

  void add_segments(irs::IndexWriter& writer,
                    std::vector<doc_generator_base::ptr>& gens);

  void add_segment(tests::doc_generator_base& gen,
                   irs::OpenMode mode = irs::OM_CREATE,
                   const irs::IndexWriterOptions& opts = {});

 private:
  index_t index_;
  std::shared_ptr<irs::directory> dir_;
  irs::format::ptr codec_;
};

}  // namespace tests
