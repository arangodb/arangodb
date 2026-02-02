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
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "search/nested_filter.hpp"

#include "search/all_filter.hpp"
#include "search/bitset_doc_iterator.hpp"
#include "search/boolean_filter.hpp"
#include "search/column_existence_filter.hpp"
#include "search/filter_test_case_base.hpp"
#include "search/granular_range_filter.hpp"
#include "search/prev_doc.hpp"
#include "search/term_filter.hpp"
#include "tests_shared.hpp"

namespace {

struct ChildIterator : irs::doc_iterator {
 public:
  ChildIterator(irs::doc_iterator::ptr&& it, std::set<irs::doc_id_t> parents)
    : it_{std::move(it)}, parents_{std::move(parents)} {
    EXPECT_NE(nullptr, it_);
  }

  irs::doc_id_t value() const { return it_->value(); }

  bool next() {
    while (1) {
      if (!it_->next()) {
        return false;
      }

      if (!parents_.contains(it_->value())) {
        return true;
      }
    }
  }

  irs::doc_id_t seek(irs::doc_id_t target) {
    const auto doc = it_->seek(target);

    if (!parents_.contains(doc)) {
      return doc;
    }

    next();
    return value();
  }

  irs::attribute* get_mutable(irs::type_info::type_id id) noexcept {
    return it_->get_mutable(id);
  }

 private:
  irs::doc_iterator::ptr it_;
  std::set<irs::doc_id_t> parents_;
};

class PrevDocWrapper : public irs::doc_iterator {
 public:
  explicit PrevDocWrapper(doc_iterator::ptr&& it) noexcept
    : it_{std::move(it)} {
    // Actual implementation doesn't matter
    prev_doc_.reset([](const void*) { return irs::doc_limits::eof(); },
                    nullptr);
  }

  irs::doc_id_t value() const final { return it_->value(); }

  irs::doc_id_t seek(irs::doc_id_t target) final { return it_->seek(target); }

  bool next() final { return it_->next(); }

  irs::attribute* get_mutable(irs::type_info::type_id id) final {
    if (irs::type<irs::prev_doc>::id() == id) {
      return &prev_doc_;
    }

    return it_->get_mutable(id);
  }

 private:
  doc_iterator::ptr it_;
  irs::prev_doc prev_doc_;
};

struct DocIdScorer : irs::ScorerBase<void> {
  irs::IndexFeatures index_features() const final {
    return irs::IndexFeatures::NONE;
  }

  irs::ScoreFunction prepare_scorer(const irs::ColumnProvider&,
                                    const irs::feature_map_t&,
                                    const irs::byte_type*,
                                    const irs::attribute_provider& attrs,
                                    irs::score_t) const final {
    struct ScorerContext final : irs::score_ctx {
      explicit ScorerContext(const irs::document* doc) noexcept : doc{doc} {}

      const irs::document* doc;
    };

    auto* doc = irs::get<irs::document>(attrs);
    EXPECT_NE(nullptr, doc);

    return irs::ScoreFunction::Make<ScorerContext>(
      [](irs::score_ctx* ctx, irs::score_t* res) noexcept {
        ASSERT_NE(nullptr, res);
        ASSERT_NE(nullptr, ctx);
        const auto& state = *static_cast<ScorerContext*>(ctx);
        *res = state.doc->value;
      },
      irs::ScoreFunction::DefaultMin, doc);
  }
};

// exists(name)
auto MakeParentProvider(std::string_view name) {
  return [name](const irs::SubReader& segment) {
    const auto* col = segment.column(name);

    return col
             ? col->iterator(irs::ColumnHint::kMask | irs::ColumnHint::kPrevDoc)
             : nullptr;
  };
}

// name == value
auto MakeByTerm(std::string_view name, std::string_view value) {
  auto filter = std::make_unique<irs::by_term>();
  *filter->mutable_field() = name;
  filter->mutable_options()->term = irs::ViewCast<irs::byte_type>(value);
  return filter;
}

// name == value
auto MakeByNumericTerm(std::string_view name, int32_t value) {
  auto filter = std::make_unique<irs::by_term>();
  *filter->mutable_field() = name;

  irs::numeric_token_stream stream;
  const irs::term_attribute* token = irs::get<irs::term_attribute>(stream);
  stream.reset(value);
  stream.next();

  filter->mutable_options()->term = token->value;

  return filter;
}

auto MakeByColumnExistence(std::string_view name) {
  auto filter = std::make_unique<irs::by_column_existence>();
  *filter->mutable_field() = name;
  return filter;
}

// name == value && range_field <= upper_bound
auto MakeByTermAndRange(std::string_view name, std::string_view value,
                        std::string_view range_field, int32_t upper_bound) {
  auto root = std::make_unique<irs::And>();
  // name == value
  {
    auto& filter = root->add<irs::by_term>();
    *filter.mutable_field() = name;
    filter.mutable_options()->term = irs::ViewCast<irs::byte_type>(value);
  }
  // range_field <= upper_bound
  {
    auto& filter = root->add<irs::by_granular_range>();
    *filter.mutable_field() = range_field;

    irs::numeric_token_stream stream;
    auto& range = filter.mutable_options()->range;
    stream.reset(upper_bound);
    irs::set_granular_term(range.max, stream);
  }
  return root;
}

auto MakeOptions(std::string_view parent, std::string_view child,
                 std::string_view child_value,
                 irs::ScoreMergeType merge_type = irs::ScoreMergeType::kSum,
                 irs::Match match = irs::kMatchAny) {
  irs::ByNestedOptions opts;
  opts.match = match;
  opts.merge_type = merge_type;
  opts.parent = MakeParentProvider(parent);
  opts.child = std::make_unique<irs::by_term>();
  auto& child_filter = static_cast<irs::by_term&>(*opts.child);
  *child_filter.mutable_field() = child;
  child_filter.mutable_options()->term =
    irs::ViewCast<irs::byte_type>(child_value);

  return opts;
}

TEST(NestedFilterTest, CheckMatch) {
  static_assert(irs::Match{0, 0} == irs::kMatchNone);
  static_assert(irs::Match{1, irs::doc_limits::eof()} == irs::kMatchAny &&
                irs::kMatchAny.IsMinMatch());
}

TEST(NestedFilterTest, CheckOptions) {
  {
    irs::ByNestedOptions opts;
    ASSERT_EQ(nullptr, opts.parent);
    ASSERT_EQ(nullptr, opts.child);
    ASSERT_EQ(irs::ScoreMergeType::kSum, opts.merge_type);
    ASSERT_NE(nullptr, std::get_if<irs::Match>(&opts.match));
    ASSERT_EQ(irs::kMatchAny, std::get<irs::Match>(opts.match));
    ASSERT_EQ(opts, irs::ByNestedOptions{});
  }

  {
    const auto opts0 = MakeOptions("parent", "child", "442");
    const auto opts1 = MakeOptions("parent", "child", "442");
    ASSERT_EQ(opts0, opts1);

    // We discount parent providers from equality comparison
    const auto opts2 = MakeOptions("parent42", "child", "442");
    ASSERT_EQ(opts0, opts2);

    ASSERT_NE(opts0, MakeOptions("parent", "child", "443"));
    ASSERT_NE(opts0,
              MakeOptions("parent", "child", "442", irs::ScoreMergeType::kMax));
    ASSERT_NE(opts0, MakeOptions("parent", "child", "442",
                                 irs::ScoreMergeType::kSum, irs::kMatchNone));
  }
}

TEST(NestedFilterTest, ConstructFilter) {
  irs::ByNestedFilter filter;
  ASSERT_EQ(irs::ByNestedOptions{}, filter.options());
  ASSERT_EQ(irs::kNoBoost, filter.boost());
}

class NestedFilterTestCase : public tests::FilterTestCaseBase {
 protected:
  struct Item {
    std::string name;
    int32_t price;
    int32_t count;
  };

  struct Order {
    std::string customer;
    std::string date;
    std::vector<Item> items;
  };

  static constexpr auto kIndexAndStore =
    irs::Action::INDEX | irs::Action::STORE;

  static void InsertItemDocument(irs::IndexWriter::Transaction& trx,
                                 std::string_view item, int32_t price,
                                 int32_t count) {
    auto doc = trx.Insert();
    ASSERT_TRUE(doc.Insert<kIndexAndStore>(tests::string_field{"item", item}));
    ASSERT_TRUE(doc.Insert<kIndexAndStore>(tests::int_field{
      "price", price, irs::type<irs::granularity_prefix>::id()}));
    ASSERT_TRUE(doc.Insert<kIndexAndStore>(tests::int_field{
      "count", count, irs::type<irs::granularity_prefix>::id()}));
    ASSERT_TRUE(doc);
  }

  static void InsertOrderDocument(irs::IndexWriter::Transaction& trx,
                                  std::string_view customer,
                                  std::string_view date) {
    auto doc = trx.Insert();
    if (!customer.empty()) {
      ASSERT_TRUE(
        doc.Insert<kIndexAndStore>(tests::string_field{"customer", customer}));
    }
    ASSERT_TRUE(doc.Insert<kIndexAndStore>(tests::string_field{"date", date}));
    ASSERT_TRUE(doc);
  }

  static void InsertOrder(irs::IndexWriter& writer, const Order& order) {
    auto trx = writer.GetBatch();
    for (const auto& [item, price, count] : order.items) {
      InsertItemDocument(trx, item, price, count);
    }
    InsertOrderDocument(trx, order.customer, order.date);
  }

  void InitDataSet();
};

void NestedFilterTestCase::InitDataSet() {
  irs::IndexWriterOptions opts;
  opts.column_info = [](std::string_view name) {
    return irs::ColumnInfo{
      .compression = irs::type<irs::compression::none>::get(),
      .options = {},
      .encryption = false,
      .track_prev_doc = (name == "customer")};
  };
  auto writer = open_writer(irs::OM_CREATE, opts);
  ASSERT_NE(nullptr, writer);

  // Parent document: 6
  InsertOrder(*writer, {"ArangoDB",
                        "May",
                        {{"Keyboard", 100, 1},
                         {"Mouse", 50, 2},
                         {"Display", 1000, 2},
                         {"CPU", 5000, 1},
                         {"RAM", 5000, 1}}});

  // Parent document: 8
  InsertOrder(*writer, {"Quest", "June", {{"CPU", 1000, 3}}});

  // Parent document: 13
  InsertOrder(*writer, {"Dell",
                        "April",
                        {{"Mouse", 10, 2},
                         {"Display", 1000, 2},
                         {"CPU", 1000, 2},
                         {"RAM", 5000, 2}}});

  // Parent document: 15, missing "customer" field
  // 'Mouse' is treated as a part of the next order
  InsertOrder(*writer, {"", "April", {{"Mouse", 10, 2}}});

  // Parent document: 20
  InsertOrder(*writer, {"BAE",
                        "March",
                        {{"Stand", 10, 2},
                         {"Display", 1000, 2},
                         {"CPU", 1000, 2},
                         {"RAM", 5000, 2}}});

  ASSERT_TRUE(writer->Commit());
  AssertSnapshotEquality(*writer);

  auto reader = open_reader();
  ASSERT_NE(nullptr, reader);
  ASSERT_EQ(1, reader.size());
}

TEST_P(NestedFilterTestCase, EmptyFilter) {
  InitDataSet();
  auto reader = open_reader();

  {
    irs::ByNestedFilter filter;
    CheckQuery(filter, Docs{}, Costs{0}, reader, SOURCE_LOCATION);
  }

  {
    irs::ByNestedFilter filter;
    auto& opts = *filter.mutable_options();
    opts.child = std::make_unique<irs::all>();
    CheckQuery(filter, Docs{}, Costs{0}, reader, SOURCE_LOCATION);
  }

  {
    irs::ByNestedFilter filter;
    auto& opts = *filter.mutable_options();
    opts.parent = MakeParentProvider("customer");
    CheckQuery(filter, Docs{}, Costs{0}, reader, SOURCE_LOCATION);
  }
}

TEST_P(NestedFilterTestCase, JoinAny0) {
  InitDataSet();
  auto reader = open_reader();

  irs::ByNestedFilter filter;
  auto& opts = *filter.mutable_options();
  opts.child = MakeByTerm("item", "Keyboard");
  opts.parent = MakeParentProvider("customer");

  CheckQuery(filter, Docs{6}, Costs{1}, reader, SOURCE_LOCATION);
}

TEST_P(NestedFilterTestCase, JoinAny1) {
  InitDataSet();
  auto reader = open_reader();

  irs::ByNestedFilter filter;
  auto& opts = *filter.mutable_options();
  opts.child = MakeByTerm("item", "Mouse");
  opts.parent = MakeParentProvider("customer");

  CheckQuery(filter, Docs{6, 13, 20}, Costs{3}, reader, SOURCE_LOCATION);

  {
    const Tests tests = {
      {Seek{6}, 6}, {Seek{7}, 13}, {Seek{7}, 13}, {Seek{16}, 20}};

    CheckQuery(filter, {}, {tests}, reader, SOURCE_LOCATION);
  }

  {
    std::array<irs::Scorer::ptr, 1> scorers{std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Seek{6}, 6, {2.f}}, {Seek{7}, 13, {9.f}},
      // FIXME(gnusi): should be 9, currently
      // fails due to we don't cache score
      /*{Seek{6}, 13, {25.f}}*/
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }

  {
    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 6, {2.f, 2.f}},
      {Next{}, 13, {9.f, 9.f}},
      {Next{}, 20, {14.f, 14.f}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }

  {
    const Tests tests = {{Seek{21}, irs::doc_limits::eof()},
                         {Next{}, irs::doc_limits::eof()},
                         {Next{}, irs::doc_limits::eof()}};
    CheckQuery(filter, {}, {tests}, reader, SOURCE_LOCATION);
  }

  {
    const Tests tests = {
      // Seek to doc_limits::invalid() is implementation specific
      {Seek{irs::doc_limits::invalid()}, irs::doc_limits::invalid()},
      {Seek{2}, 6},
      {Next{}, 13},
      {Next{}, 20},
      {Next{}, irs::doc_limits::eof()},
      {Seek{2}, irs::doc_limits::eof()},
      {Next{}, irs::doc_limits::eof()}};
    CheckQuery(filter, {}, {tests}, reader, SOURCE_LOCATION);
  }

  {
    const Tests tests = {{Seek{6}, 6},
                         {Next{}, 13},
                         {Next{}, 20},
                         {Next{}, irs::doc_limits::eof()}};

    CheckQuery(filter, {}, {tests}, reader, SOURCE_LOCATION);
  }
}

TEST_P(NestedFilterTestCase, JoinAny2) {
  InitDataSet();
  auto reader = open_reader();

  irs::ByNestedFilter filter;
  auto& opts = *filter.mutable_options();
  opts.child = MakeByTermAndRange("item", "Mouse", "price", 11);
  opts.parent = MakeParentProvider("customer");

  CheckQuery(filter, Docs{13, 20}, Costs{3}, reader, SOURCE_LOCATION);
}

TEST_P(NestedFilterTestCase, JoinAny3) {
  InitDataSet();
  auto reader = open_reader();

  irs::ByNestedFilter filter;
  auto& opts = *filter.mutable_options();
  opts.child = MakeByNumericTerm("count", 2);
  opts.parent = MakeParentProvider("customer");

  CheckQuery(filter, Docs{6, 13, 20}, Costs{11}, reader, SOURCE_LOCATION);

  {
    opts.merge_type = irs::ScoreMergeType::kMax;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 6, {3.f, 3.f}},
      {Next{}, 13, {12.f, 12.f}},
      {Next{}, 20, {19.f, 19.f}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }

  {
    opts.merge_type = irs::ScoreMergeType::kMin;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 6, {2.f, 2.f}},
      {Next{}, 13, {9.f, 9.f}},
      {Next{}, 20, {14.f, 14.f}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }

  {
    opts.merge_type = irs::ScoreMergeType::kNoop;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 6, {}},
      {Next{}, 13, {}},
      {Next{}, 20, {}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }
}

TEST_P(NestedFilterTestCase, JoinAll0) {
  InitDataSet();
  auto reader = open_reader();

  MaxMemoryCounter counter;

  irs::ByNestedFilter filter;
  auto& opts = *filter.mutable_options();
  opts.child = MakeByNumericTerm("count", 2);
  opts.parent = MakeParentProvider("customer");
  opts.match = [&](const irs::SubReader& segment) -> irs::doc_iterator::ptr {
    return irs::memory::make_managed<ChildIterator>(
      irs::all()
        .prepare({
          .index = segment,
          .memory = counter,
        })
        ->execute({.segment = segment}),
      std::set{6U, 13U, 15U, 20U});
  };

  CheckQuery(filter, Docs{13, 20}, Costs{11}, reader, SOURCE_LOCATION);
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  {
    opts.merge_type = irs::ScoreMergeType::kMax;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 13, {12.f, 12.f}},
      {Next{}, 20, {19.f, 19.f}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  {
    opts.merge_type = irs::ScoreMergeType::kMin;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 13, {9.f, 9.f}},
      {Next{}, 20, {14.f, 14.f}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  {
    opts.merge_type = irs::ScoreMergeType::kNoop;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 13, {}},
      {Next{}, 20, {}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
}

TEST_P(NestedFilterTestCase, JoinMin0) {
  InitDataSet();
  auto reader = open_reader();

  irs::ByNestedFilter filter;
  auto& opts = *filter.mutable_options();
  opts.child = MakeByNumericTerm("count", 2);
  opts.parent = MakeParentProvider("customer");
  opts.match = irs::Match{3};

  CheckQuery(filter, Docs{13, 20}, Costs{11}, reader, SOURCE_LOCATION);

  {
    opts.merge_type = irs::ScoreMergeType::kMax;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 13, {12.f, 12.f}},
      {Next{}, 20, {19.f, 19.f}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }

  {
    opts.merge_type = irs::ScoreMergeType::kMin;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 13, {9.f, 9.f}},
      {Next{}, 20, {14.f, 14.f}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }

  {
    opts.merge_type = irs::ScoreMergeType::kNoop;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 13, {}},
      {Next{}, 20, {}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }
}

TEST_P(NestedFilterTestCase, JoinMin1) {
  InitDataSet();
  auto reader = open_reader();

  irs::ByNestedFilter filter;
  auto& opts = *filter.mutable_options();
  opts.child = MakeByNumericTerm("count", 1);
  opts.parent = MakeParentProvider("customer");
  opts.match = irs::Match{3};

  CheckQuery(filter, Docs{6}, Costs{3}, reader, SOURCE_LOCATION);

  {
    opts.merge_type = irs::ScoreMergeType::kMax;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 6, {5.f, 5.f}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }

  {
    opts.merge_type = irs::ScoreMergeType::kMin;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 6, {1.f, 1.f}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }

  {
    opts.merge_type = irs::ScoreMergeType::kNoop;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 6, {}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }
}

TEST_P(NestedFilterTestCase, JoinMin2) {
  InitDataSet();
  auto reader = open_reader();

  irs::ByNestedFilter filter;
  auto& opts = *filter.mutable_options();
  opts.child = MakeByNumericTerm("count", 1);
  opts.parent = MakeParentProvider("customer");
  opts.match = irs::Match{0};  // Match all parents

  CheckQuery(filter, Docs{6, 8, 13, 20}, Costs{3}, reader, SOURCE_LOCATION);

  {
    opts.merge_type = irs::ScoreMergeType::kMax;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 6, {5.f, 5.f}},          {Next{}, 8, {0.f, 0.f}},
      {Next{}, 13, {0.f, 0.f}},         {Next{}, 20, {0.f, 0.f}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }

  {
    opts.merge_type = irs::ScoreMergeType::kMin;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 6, {0.f, 0.f}},          {Next{}, 8, {0.f, 0.f}},
      {Next{}, 13, {0.f, 0.f}},         {Next{}, 20, {0.f, 0.f}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }

  {
    opts.merge_type = irs::ScoreMergeType::kNoop;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 6, {}},
      {Next{}, 8, {}},
      {Next{}, 13, {}},
      {Next{}, 20, {}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }
}

TEST_P(NestedFilterTestCase, JoinMin3) {
  InitDataSet();
  auto reader = open_reader();

  irs::ByNestedFilter filter;
  auto& opts = *filter.mutable_options();
  opts.child = MakeByNumericTerm("count", 42);  // Empty child filter
  opts.parent = MakeParentProvider("customer");
  opts.match = irs::Match{0};  // Match all parents

  CheckQuery(filter, Docs{6, 8, 13, 20}, Costs{4}, reader, SOURCE_LOCATION);

  {
    opts.merge_type = irs::ScoreMergeType::kMax;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 6, {0.f, 0.f}},          {Next{}, 8, {0.f, 0.f}},
      {Next{}, 13, {0.f, 0.f}},         {Next{}, 20, {0.f, 0.f}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }

  {
    opts.merge_type = irs::ScoreMergeType::kMin;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 6, {0.f, 0.f}},          {Next{}, 8, {0.f, 0.f}},
      {Next{}, 13, {0.f, 0.f}},         {Next{}, 20, {0.f, 0.f}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }

  {
    opts.merge_type = irs::ScoreMergeType::kNoop;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 6, {}},
      {Next{}, 8, {}},
      {Next{}, 13, {}},
      {Next{}, 20, {}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }
}

TEST_P(NestedFilterTestCase, JoinRange0) {
  InitDataSet();
  auto reader = open_reader();

  irs::ByNestedFilter filter;
  auto& opts = *filter.mutable_options();
  opts.child = MakeByNumericTerm("count", 2);
  opts.parent = MakeParentProvider("customer");
  opts.match = irs::Match{3, 5};

  CheckQuery(filter, Docs{13, 20}, Costs{11}, reader, SOURCE_LOCATION);

  {
    opts.merge_type = irs::ScoreMergeType::kMax;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 13, {12.f, 12.f}},
      {Next{}, 20, {19.f, 19.f}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }

  {
    opts.merge_type = irs::ScoreMergeType::kMin;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 13, {9.f, 9.f}},
      {Next{}, 20, {14.f, 14.f}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }

  {
    opts.merge_type = irs::ScoreMergeType::kNoop;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 13, {}},
      {Next{}, 20, {}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }
}

TEST_P(NestedFilterTestCase, JoinRange1) {
  InitDataSet();
  auto reader = open_reader();

  irs::ByNestedFilter filter;
  auto& opts = *filter.mutable_options();
  opts.child = MakeByNumericTerm("count", 1);
  opts.parent = MakeParentProvider("customer");
  opts.match = irs::Match{3, 3};

  CheckQuery(filter, Docs{6}, Costs{3}, reader, SOURCE_LOCATION);

  {
    opts.merge_type = irs::ScoreMergeType::kMax;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 6, {5.f, 5.f}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }

  {
    opts.merge_type = irs::ScoreMergeType::kMin;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 6, {1.f, 1.f}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }

  {
    opts.merge_type = irs::ScoreMergeType::kNoop;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 6, {}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }
}

TEST_P(NestedFilterTestCase, JoinRange2) {
  InitDataSet();
  auto reader = open_reader();

  irs::ByNestedFilter filter;
  auto& opts = *filter.mutable_options();
  opts.child = MakeByNumericTerm("count", 2);
  opts.parent = MakeParentProvider("customer");
  opts.match = irs::Match{0, 5};

  CheckQuery(filter, Docs{6, 8, 13, 20}, Costs{11}, reader, SOURCE_LOCATION);

  {
    opts.merge_type = irs::ScoreMergeType::kMax;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 6, {3.f, 3.f}},          {Next{}, 8, {0.f, 0.f}},
      {Next{}, 13, {12.f, 12.f}},       {Next{}, 20, {19.f, 19.f}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }

  {
    opts.merge_type = irs::ScoreMergeType::kMin;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 6, {2.f, 2.f}},          {Next{}, 8, {0.f, 0.f}},
      {Next{}, 13, {9.f, 9.f}},         {Next{}, 20, {14.f, 14.f}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }

  {
    opts.merge_type = irs::ScoreMergeType::kNoop;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 6, {}},
      {Next{}, 8, {}},
      {Next{}, 13, {}},
      {Next{}, 20, {}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }
}

TEST_P(NestedFilterTestCase, JoinNone0) {
  InitDataSet();
  auto reader = open_reader();

  irs::ByNestedFilter filter;
  auto& opts = *filter.mutable_options();
  opts.child = MakeByTerm("item", "Mouse");
  opts.parent = MakeParentProvider("customer");
  opts.match = irs::kMatchNone;

  CheckQuery(filter, Docs{8}, Costs{3}, reader, SOURCE_LOCATION);

  {
    opts.merge_type = irs::ScoreMergeType::kMax;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 8, {1.f, 1.f}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }

  {
    opts.merge_type = irs::ScoreMergeType::kMin;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 8, {1.f, 1.f}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }

  {
    opts.merge_type = irs::ScoreMergeType::kNoop;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 8, {}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }
}

TEST_P(NestedFilterTestCase, JoinNone1) {
  InitDataSet();
  auto reader = open_reader();

  irs::ByNestedFilter filter;
  auto& opts = *filter.mutable_options();
  opts.child = MakeByTerm("item", "Mouse");
  opts.parent = MakeParentProvider("customer");
  opts.match = irs::kMatchNone;
  filter.boost(0.5f);

  CheckQuery(filter, Docs{8}, Costs{3}, reader, SOURCE_LOCATION);

  {
    opts.merge_type = irs::ScoreMergeType::kMax;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 8, {0.5f, 0.5f}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }

  {
    opts.merge_type = irs::ScoreMergeType::kMin;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 8, {0.5f, 0.5f}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }

  {
    opts.merge_type = irs::ScoreMergeType::kNoop;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 8, {}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }
}

TEST_P(NestedFilterTestCase, JoinNone2) {
  InitDataSet();
  auto reader = open_reader();

  irs::ByNestedFilter filter;
  auto& opts = *filter.mutable_options();
  opts.child = std::make_unique<irs::Empty>();
  opts.parent = MakeParentProvider("customer");
  opts.match = irs::kMatchNone;
  filter.boost(1.f);

  CheckQuery(filter, Docs{6, 8, 13, 20}, Costs{4}, reader, SOURCE_LOCATION);

  {
    opts.merge_type = irs::ScoreMergeType::kMax;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 6, {1.f, 1.f}},          {Next{}, 8, {1.f, 1.f}},
      {Next{}, 13, {1.f, 1.f}},         {Next{}, 20, {1.f, 1.f}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }

  {
    opts.merge_type = irs::ScoreMergeType::kMin;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 6, {1.f, 1.f}},          {Next{}, 8, {1.f, 1.f}},
      {Next{}, 13, {1.f, 1.f}},         {Next{}, 20, {1.f, 1.f}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }

  {
    opts.merge_type = irs::ScoreMergeType::kNoop;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 6, {}},
      {Next{}, 8, {}},
      {Next{}, 13, {}},
      {Next{}, 20, {}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }
}

TEST_P(NestedFilterTestCase, JoinNone3) {
  InitDataSet();
  auto reader = open_reader();

  irs::ByNestedFilter filter;
  auto& opts = *filter.mutable_options();
  opts.child = std::make_unique<irs::Empty>();

  // Bitset iterator doesn't provide score, check that wrapper works correctly
  opts.parent = [word = irs::bitset::word_t{}](
                  const irs::SubReader&) mutable -> irs::doc_iterator::ptr {
    irs::set_bit<6>(word);
    irs::set_bit<8>(word);
    irs::set_bit<13>(word);
    irs::set_bit<20>(word);
    return irs::memory::make_managed<PrevDocWrapper>(
      irs::memory::make_managed<irs::bitset_doc_iterator>(&word, &word + 1));
  };

  MakeParentProvider("customer");
  opts.match = irs::kMatchNone;
  filter.boost(0.5f);

  CheckQuery(filter, Docs{6, 8, 13, 20}, Costs{4}, reader, SOURCE_LOCATION);

  {
    opts.merge_type = irs::ScoreMergeType::kMax;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 6, {0.5f, 0.5f}},        {Next{}, 8, {0.5f, 0.5f}},
      {Next{}, 13, {0.5f, 0.5f}},       {Next{}, 20, {0.5f, 0.5f}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }

  {
    opts.merge_type = irs::ScoreMergeType::kMin;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 6, {0.5f, 0.5f}},        {Next{}, 8, {0.5f, 0.5f}},
      {Next{}, 13, {0.5f, 0.5f}},       {Next{}, 20, {0.5f, 0.5f}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }

  {
    opts.merge_type = irs::ScoreMergeType::kNoop;

    std::array<irs::Scorer::ptr, 2> scorers{std::make_unique<DocIdScorer>(),
                                            std::make_unique<DocIdScorer>()};

    const Tests tests = {
      {Next{}, 6, {}},
      {Next{}, 8, {}},
      {Next{}, 13, {}},
      {Next{}, 20, {}},
      {Next{}, irs::doc_limits::eof()},
    };

    CheckQuery(filter, scorers, {tests}, reader, SOURCE_LOCATION);
  }
}

static constexpr auto kTestDirs = tests::getDirectories<tests::kTypesDefault>();

static const auto kDirectories = ::testing::ValuesIn(kTestDirs);

INSTANTIATE_TEST_SUITE_P(
  NestedFilterTest, NestedFilterTestCase,
  ::testing::Combine(kDirectories,
                     ::testing::Values(tests::format_info{"1_4", "1_0"},
                                       tests::format_info{"1_5", "1_0"})),
  NestedFilterTestCase::to_string);

class NestedFilterFormatsTestCase : public NestedFilterTestCase {
 protected:
  bool HasPrevDocSupport() noexcept {
    // old formats don't support columnstore headers
    constexpr std::string_view kOldFormats[]{"1_0", "1_1", "1_2", "1_3",
                                             "1_3simd"};

    return std::end(kOldFormats) == std::find(std::begin(kOldFormats),
                                              std::end(kOldFormats),
                                              codec()->type()().name());
  }
};

TEST_P(NestedFilterFormatsTestCase, JoinAny0) {
  InitDataSet();
  auto reader = open_reader();

  irs::ByNestedFilter filter;
  auto& opts = *filter.mutable_options();
  opts.child = MakeByTerm("item", "Mouse");
  opts.parent = MakeParentProvider("customer");

  const auto expected = HasPrevDocSupport() ? Docs{6, 13, 20} : Docs{};
  CheckQuery(filter, expected, Costs{expected.size()}, reader, SOURCE_LOCATION);
}

TEST_P(NestedFilterFormatsTestCase, JoinAnyParent) {
  InitDataSet();
  auto reader = open_reader();

  irs::ByNestedFilter filter;
  auto& opts = *filter.mutable_options();
  opts.child = MakeByColumnExistence("customer");
  opts.parent = MakeParentProvider("customer");

  // Filter must return no docs, as child query returns only parents
  const auto expected = Docs{};
  CheckQuery(filter, expected, Costs{(HasPrevDocSupport() ? 4U : 0U)}, reader,
             SOURCE_LOCATION);
}

TEST_P(NestedFilterFormatsTestCase, JoinAnyAll) {
  InitDataSet();
  auto reader = open_reader();

  irs::ByNestedFilter filter;
  auto& opts = *filter.mutable_options();
  opts.child = std::make_unique<irs::all>();
  opts.parent = MakeParentProvider("customer");

  const auto expected = HasPrevDocSupport() ? Docs{6, 8, 13, 20} : Docs{};
  CheckQuery(filter, expected, Costs{(HasPrevDocSupport() ? 20U : 0U)}, reader,
             SOURCE_LOCATION);
}

INSTANTIATE_TEST_SUITE_P(
  NestedFilterFormatsTest, NestedFilterFormatsTestCase,
  ::testing::Combine(kDirectories,
                     ::testing::Values(tests::format_info{"1_0"},
                                       tests::format_info{"1_1", "1_0"},
                                       tests::format_info{"1_2", "1_0"},
                                       tests::format_info{"1_3", "1_0"},
                                       tests::format_info{"1_4", "1_0"},
                                       tests::format_info{"1_5", "1_0"})),
  NestedFilterFormatsTestCase::to_string);

}  // namespace
