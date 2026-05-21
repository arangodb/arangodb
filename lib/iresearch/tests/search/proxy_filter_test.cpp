////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#include "search/proxy_filter.hpp"

#include "filter_test_case_base.hpp"
#include "index/index_writer.hpp"
#include "search/boolean_filter.hpp"
#include "search/term_filter.hpp"
#include "store/memory_directory.hpp"
#include "tests_shared.hpp"

namespace {

using namespace tests;
using namespace irs;

class doclist_test_iterator : public doc_iterator, private util::noncopyable {
 public:
  doclist_test_iterator(const std::vector<doc_id_t>& documents)
    : begin_(documents.begin()),
      end_(documents.end()),
      cost_(documents.size()) {
    reset();
  }

  bool next() final {
    if (resetted_) {
      resetted_ = false;
      current_ = begin_;
    }

    if (current_ != end_) {
      doc_.value = *current_;
      ++current_;
      return true;
    } else {
      doc_.value = doc_limits::eof();
    }
    return false;
  }

  doc_id_t seek(doc_id_t target) final {
    while (doc_.value < target && next()) {
    }
    return doc_.value;
  }

  doc_id_t value() const noexcept final { return doc_.value; }

  attribute* get_mutable(irs::type_info::type_id id) noexcept final {
    if (irs::type<irs::document>::id() == id) {
      return &doc_;
    }
    if (irs::type<irs::cost>::id() == id) {
      return &cost_;
    }
    return nullptr;
  }

  void reset() noexcept {
    current_ = end_;
    resetted_ = true;
    doc_.value = doc_limits::invalid();
  }

 private:
  std::vector<doc_id_t>::const_iterator begin_;
  std::vector<doc_id_t>::const_iterator current_;
  std::vector<doc_id_t>::const_iterator end_;
  irs::document doc_;
  cost cost_;
  bool resetted_;
};

class doclist_test_query : public filter::prepared {
 public:
  doclist_test_query(const std::vector<doc_id_t>& documents, score_t)
    : documents_(documents) {}

  doc_iterator::ptr execute(const ExecutionContext&) const final {
    ++executes_;
    return memory::make_managed<doclist_test_iterator>(documents_);
  }

  void visit(const SubReader&, PreparedStateVisitor&, score_t) const final {
    // No terms to visit
  }

  irs::score_t boost() const noexcept final { return kNoBoost; }

  static size_t get_execs() noexcept { return executes_; }

  static void reset_execs() noexcept { executes_ = 0; }

 private:
  const std::vector<doc_id_t>& documents_;
  static size_t executes_;
};

size_t doclist_test_query::executes_{0};

class doclist_test_filter : public filter {
 public:
  static size_t get_prepares() noexcept { return prepares_; }

  static void reset_prepares() noexcept { prepares_ = 0; }

  filter::prepared::ptr prepare(const PrepareContext& ctx) const final {
    ++prepares_;
    return memory::make_tracked<doclist_test_query>(ctx.memory, *documents_,
                                                    ctx.boost);
  }

  void set_expected(const std::vector<doc_id_t>& documents) {
    documents_ = &documents;
  }

  irs::type_info::type_id type() const noexcept final {
    return irs::type<doclist_test_filter>::id();
  }

 private:
  const std::vector<doc_id_t>* documents_;
  static size_t prepares_;
};

size_t doclist_test_filter::prepares_;

class proxy_filter_test_case : public ::testing::TestWithParam<size_t> {
 public:
  proxy_filter_test_case() {
    auto codec = irs::formats::get("1_0");
    auto writer = irs::IndexWriter::Make(dir_, codec, irs::OM_CREATE);
    {  // make dummy document so we could have non-empty index
      auto ctx = writer->GetBatch();
      for (size_t i = 0; i < GetParam(); ++i) {
        auto doc = ctx.Insert();
        auto field = std::make_shared<tests::string_field>("foo", "bar");
        doc.Insert<Action::INDEX>(*field);
      }
    }
    writer->Commit();
    index_ = irs::DirectoryReader(dir_, codec);
    AssertSnapshotEquality(writer->GetSnapshot(), index_);
  }

 protected:
  void SetUp() final {
    doclist_test_query::reset_execs();
    doclist_test_filter::reset_prepares();
  }

  void verify_filter(const std::vector<doc_id_t>& expected, size_t line) {
    SCOPED_TRACE(::testing::Message("Failed on line: ") << line);
    MaxMemoryCounter prepare_counter;
    MaxMemoryCounter execute_counter;

    irs::proxy_filter::cache_ptr cache;
    for (size_t i = 0; i < 3; ++i) {
      proxy_filter proxy;
      if (i == 0) {
        auto res =
          proxy.set_filter<doclist_test_filter>(irs::IResourceManager::kNoop);
        cache = res.second;
        res.first.set_expected(expected);
      } else {
        proxy.set_cache(cache);
      }
      auto prepared_proxy = proxy.prepare({
        .index = index_,
        .memory = prepare_counter,
      });
      auto docs = prepared_proxy->execute({
        .segment = index_[0],
        .memory = execute_counter,
      });
      auto costs = irs::get<irs::cost>(*docs);
      EXPECT_TRUE(costs);
      EXPECT_EQ(costs->estimate(), expected.size());
      auto expected_doc = expected.begin();
      while (docs->next() && expected_doc != expected.end()) {
        EXPECT_EQ(docs->value(), *expected_doc);
        ++expected_doc;
      }
      EXPECT_FALSE(docs->next());
      EXPECT_EQ(expected_doc, expected.end());
    }
    // Real filter should be exectued just once
    EXPECT_EQ(doclist_test_query::get_execs(), 1);
    EXPECT_EQ(doclist_test_filter::get_prepares(), 1);

    cache.reset();

    EXPECT_EQ(prepare_counter.current, 0);
    EXPECT_GT(prepare_counter.max, 0);
    prepare_counter.Reset();

    EXPECT_EQ(execute_counter.current, 0);
    EXPECT_GT(execute_counter.max, 0);
    execute_counter.Reset();
  }

  irs::memory_directory dir_;
  irs::DirectoryReader index_;
};

TEST_P(proxy_filter_test_case, test_1first_bit) {
  std::vector<doc_id_t> documents{1};
  verify_filter(documents, __LINE__);
}

TEST_P(proxy_filter_test_case, test_last_bit) {
  std::vector<doc_id_t> documents{63};
  verify_filter(documents, __LINE__);
}

TEST_P(proxy_filter_test_case, test_2first_bit) {
  if (GetParam() >= 64) {
    std::vector<doc_id_t> documents{64};
    verify_filter(documents, __LINE__);
  }
}

TEST_P(proxy_filter_test_case, test_2last_bit) {
  std::vector<doc_id_t> documents{static_cast<doc_id_t>(GetParam()) - 1};
  verify_filter(documents, __LINE__);
}

TEST_P(proxy_filter_test_case, test_1last_2first_bit) {
  if (GetParam() >= 64) {
    std::vector<doc_id_t> documents{63, 64};
    verify_filter(documents, __LINE__);
  }
}

TEST_P(proxy_filter_test_case, test_1first_2last_bit) {
  std::vector<doc_id_t> documents{1, static_cast<doc_id_t>(GetParam()) - 1};
  verify_filter(documents, __LINE__);
}

TEST_P(proxy_filter_test_case, test_full_dense) {
  std::vector<doc_id_t> documents(GetParam());
  std::iota(documents.begin(), documents.end(), irs::doc_limits::min());
  verify_filter(documents, __LINE__);
}

INSTANTIATE_TEST_SUITE_P(proxy_filter_test_case, proxy_filter_test_case,
                         ::testing::Values(10, 15, 64, 100, 128));

class proxy_filter_real_filter : public tests::FilterTestCaseBase {
 public:
  void init_index() {
    auto writer = open_writer(irs::OM_CREATE);

    std::vector<doc_generator_base::ptr> gens;
    gens.emplace_back(new tests::json_doc_generator(
      resource("simple_sequential.json"), &tests::generic_json_field_factory));
    gens.emplace_back(new tests::json_doc_generator(
      resource("simple_sequential_common_prefix.json"),
      &tests::generic_json_field_factory));
    add_segments(*writer, gens);
  }
};

TEST_P(proxy_filter_real_filter, with_terms_filter) {
  init_index();
  auto rdr = open_reader();
  proxy_filter proxy;
  auto [q, cache] = proxy.set_filter<by_term>(irs::IResourceManager::kNoop);
  *q.mutable_field() = "name";
  q.mutable_options()->term =
    irs::ViewCast<irs::byte_type>(std::string_view("A"));
  CheckQuery(proxy, Docs{1, 33}, rdr);
}

TEST_P(proxy_filter_real_filter, with_disjunction_filter) {
  init_index();
  auto rdr = open_reader();
  proxy_filter proxy;
  auto [root, cache] = proxy.set_filter<irs::Or>(irs::IResourceManager::kNoop);
  auto& q = root.add<by_term>();
  *q.mutable_field() = "name";
  q.mutable_options()->term =
    irs::ViewCast<irs::byte_type>(std::string_view("A"));
  auto& q1 = root.add<by_term>();
  *q1.mutable_field() = "name";
  q1.mutable_options()->term =
    irs::ViewCast<irs::byte_type>(std::string_view("B"));
  CheckQuery(proxy, Docs{1, 2, 33, 34}, rdr);
}

static constexpr auto kTestDirs = tests::getDirectories<tests::kTypesDefault>();

INSTANTIATE_TEST_SUITE_P(
  proxy_filter_real_filter, proxy_filter_real_filter,
  ::testing::Combine(::testing::ValuesIn(kTestDirs),
                     ::testing::Values(tests::format_info{"1_0"},
                                       tests::format_info{"1_1", "1_0"},
                                       tests::format_info{"1_2", "1_0"},
                                       tests::format_info{"1_3", "1_0"},
                                       tests::format_info{"1_4", "1_4simd"})));
}  // namespace
