////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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

#include "search/terms_filter.hpp"

#include "filter_test_case_base.hpp"
#include "index/doc_generator.hpp"
#include "search/boost_scorer.hpp"
#include "tests_shared.hpp"

namespace {

irs::by_terms make_filter(
  const std::string_view& field,
  const std::vector<std::pair<std::string_view, irs::score_t>>& terms,
  size_t min_match = 1) {
  irs::by_terms q;
  *q.mutable_field() = field;
  q.mutable_options()->min_match = min_match;
  for (auto& term : terms) {
    q.mutable_options()->terms.emplace(
      irs::ViewCast<irs::byte_type>(term.first), term.second);
  }
  return q;
}

}  // namespace

TEST(by_terms_test, options) {
  irs::by_terms_options opts;
  ASSERT_TRUE(opts.terms.empty());
}

TEST(by_terms_test, ctor) {
  irs::by_terms q;
  ASSERT_EQ(irs::type<irs::by_terms>::id(), q.type());
  ASSERT_EQ(irs::by_terms_options{}, q.options());
  ASSERT_TRUE(q.field().empty());
  ASSERT_EQ(irs::kNoBoost, q.boost());
}

TEST(by_terms_test, equal) {
  const irs::by_terms q0 =
    make_filter("field", {{"bar", 0.5f}, {"baz", 0.25f}});
  const irs::by_terms q1 =
    make_filter("field", {{"bar", 0.5f}, {"baz", 0.25f}});
  ASSERT_EQ(q0, q1);

  const irs::by_terms q2 =
    make_filter("field1", {{"bar", 0.5f}, {"baz", 0.25f}});
  ASSERT_NE(q0, q2);

  const irs::by_terms q3 =
    make_filter("field", {{"bar1", 0.5f}, {"baz", 0.25f}});
  ASSERT_NE(q0, q3);

  const irs::by_terms q4 = make_filter("field", {{"bar", 0.5f}, {"baz", 0.5f}});
  ASSERT_NE(q0, q4);

  irs::by_terms q5 = make_filter("field", {{"bar", 0.5f}, {"baz", 0.25f}});
  q5.mutable_options()->min_match = 2;
  ASSERT_NE(q0, q5);
}

class terms_filter_test_case : public tests::FilterTestCaseBase {};

TEST_P(terms_filter_test_case, boost) {
  MaxMemoryCounter counter;

  // no boost
  {
    irs::by_terms q = make_filter("field", {{"bar", 0.5f}, {"baz", 0.25f}});

    auto prepared = q.prepare({
      .index = irs::SubReader::empty(),
      .memory = counter,
    });
    ASSERT_EQ(irs::kNoBoost, prepared->boost());
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  // with boost
  {
    irs::score_t boost = 1.5f;

    irs::by_terms q = make_filter("field", {{"bar", 0.5f}, {"baz", 0.25f}});
    q.boost(boost);

    auto prepared = q.prepare({
      .index = irs::SubReader::empty(),
      .memory = counter,
    });
    ASSERT_EQ(irs::kNoBoost,
              prepared->boost());  // no boost because index is empty
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  // with boost
  {
    tests::json_doc_generator gen(resource("simple_sequential.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);

    auto rdr = open_reader();
    ASSERT_EQ(1, rdr.size());

    irs::score_t boost = 1.5f;

    irs::by_terms q =
      make_filter("duplicated", {{"abcd", 0.5f}, {"vczc", 0.25f}});
    q.boost(boost);

    auto prepared = q.prepare({
      .index = *rdr,
      .memory = counter,
    });
    ASSERT_EQ(boost, prepared->boost());
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
}

TEST_P(terms_filter_test_case, simple_sequential_order) {
  // add segment
  {
    tests::json_doc_generator gen(resource("simple_sequential.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();
  ASSERT_EQ(1, rdr.size());

  // empty prefix test collector call count for field/term/finish
  {
    const Docs docs{1, 21, 31, 32};
    Costs costs{docs.size()};

    size_t collect_field_count = 0;
    size_t collect_term_count = 0;
    size_t finish_count = 0;

    irs::Scorer::ptr impl{std::make_unique<tests::sort::custom_sort>()};
    auto* scorer = static_cast<tests::sort::custom_sort*>(impl.get());

    scorer->collector_collect_field = [&collect_field_count](
                                        const irs::SubReader&,
                                        const irs::term_reader&) -> void {
      ++collect_field_count;
    };
    scorer->collector_collect_term =
      [&collect_term_count](const irs::SubReader&, const irs::term_reader&,
                            const irs::attribute_provider&) -> void {
      ++collect_term_count;
    };
    scorer->collectors_collect_ =
      [&finish_count](irs::byte_type*, const irs::FieldCollector*,
                      const irs::TermCollector*) -> void { ++finish_count; };
    scorer->prepare_field_collector_ = [&scorer]() -> irs::FieldCollector::ptr {
      return std::make_unique<tests::sort::custom_sort::field_collector>(
        *scorer);
    };
    scorer->prepare_term_collector_ = [&scorer]() -> irs::TermCollector::ptr {
      return std::make_unique<tests::sort::custom_sort::term_collector>(
        *scorer);
    };

    const auto filter = make_filter(
      "prefix", {{"abcd", 1.f}, {"abcd", 1.f}, {"abc", 1.f}, {"abcy", 1.f}});

    CheckQuery(filter, std::span{&impl, 1}, docs, rdr);
    ASSERT_EQ(1, collect_field_count);  // 1 fields in 1 segment
    ASSERT_EQ(3, collect_term_count);   // 3 different terms
    ASSERT_EQ(3, finish_count);         // 3 unque terms
  }

  // check boost
  {
    const Docs docs{21, 31, 32, 1};
    const Costs costs{docs.size()};
    const auto filter = make_filter(
      "prefix", {{"abcd", 0.5f}, {"abcd", 1.f}, {"abc", 1.f}, {"abcy", 1.f}});

    irs::Scorer::ptr impl{std::make_unique<irs::BoostScore>()};
    CheckQuery(filter, std::span{&impl, 1}, docs, rdr, true, true);
  }

  // check negative boost
  {
    const Docs docs{21, 31, 32, 1};
    const Costs costs{docs.size()};

    const auto filter = make_filter(
      "prefix",
      {{"abcd", -1.f}, {"abcd", 0.5f}, {"abc", 0.65}, {"abcy", 0.5f}});

    irs::Scorer::ptr impl{std::make_unique<irs::BoostScore>()};
    CheckQuery(filter, std::span{&impl, 1}, docs, rdr, true, true);
  }
}

TEST_P(terms_filter_test_case, simple_sequential) {
  // add segment
  {
    tests::json_doc_generator gen(resource("simple_sequential_utf8.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();
  ASSERT_EQ(1, rdr.size());
  auto& segment = rdr[0];

  // empty query
  CheckQuery(irs::by_terms(), Docs{}, Costs{0}, rdr);

  // empty field
  CheckQuery(make_filter("", {{"xyz", 0.5f}}), Docs{}, Costs{0}, rdr);

  // invalid field
  CheckQuery(make_filter("same1", {{"xyz", 0.5f}}), Docs{}, Costs{0}, rdr);

  // invalid term
  CheckQuery(make_filter("same", {{"invalid_term", 0.5f}}), Docs{}, Costs{0},
             rdr);

  // no value requested to match
  CheckQuery(make_filter("duplicated", {}), Docs{}, Costs{0}, rdr);

  // match all
  {
    Docs result(32);
    std::iota(std::begin(result), std::end(result), irs::doc_limits::min());
    Costs costs{result.size()};
    const auto filter = make_filter("same", {{"xyz", 1.f}});
    CheckQuery(filter, result, costs, rdr);

    // test visit
    tests::empty_filter_visitor visitor;
    const auto* reader = segment.field("same");
    ASSERT_NE(nullptr, reader);
    irs::by_terms::visit(segment, *reader, filter.options().terms, visitor);
    ASSERT_EQ(1, visitor.prepare_calls_counter());
    ASSERT_EQ(1, visitor.visit_calls_counter());
    ASSERT_EQ(
      (std::vector<std::pair<std::string_view, irs::score_t>>{{"xyz", 1.f}}),
      visitor.term_refs<char>());
  }

  // match all
  {
    Docs result(32);
    std::iota(std::begin(result), std::end(result), irs::doc_limits::min());
    Costs costs{result.size()};
    const auto filter = make_filter("same", {{"invalid", 1.f}}, 0);
    CheckQuery(filter, result, costs, rdr);

    // test visit
    tests::empty_filter_visitor visitor;
    const auto* reader = segment.field("same");
    ASSERT_NE(nullptr, reader);
    irs::by_terms::visit(segment, *reader, filter.options().terms, visitor);
    ASSERT_EQ(1, visitor.prepare_calls_counter());
    ASSERT_EQ(0, visitor.visit_calls_counter());
    ASSERT_EQ((std::vector<std::pair<std::string_view, irs::score_t>>{}),
              visitor.term_refs<char>());
  }

  // match all
  {
    Docs result(32);
    std::iota(std::begin(result), std::end(result), irs::doc_limits::min());
    Costs costs{result.size()};
    const auto filter =
      make_filter("same", {{"xyz", 1.f}, {"invalid_term", 0.5f}});
    CheckQuery(filter, result, costs, rdr);

    // test visit
    tests::empty_filter_visitor visitor;
    const auto* reader = segment.field("same");
    ASSERT_NE(nullptr, reader);
    irs::by_terms::visit(segment, *reader, filter.options().terms, visitor);
    ASSERT_EQ(1, visitor.prepare_calls_counter());
    ASSERT_EQ(1, visitor.visit_calls_counter());
    ASSERT_EQ(
      (std::vector<std::pair<std::string_view, irs::score_t>>{{"xyz", 1.f}}),
      visitor.term_refs<char>());
  }

  // match something
  {
    const Docs result{1, 21, 31, 32};
    const Costs costs{result.size()};
    const auto filter =
      make_filter("prefix", {{"abcd", 1.f}, {"abc", 0.5f}, {"abcy", 0.5f}});
    CheckQuery(filter, result, costs, rdr);

    // test visit
    tests::empty_filter_visitor visitor;
    const auto* reader = segment.field("prefix");
    ASSERT_NE(nullptr, reader);
    irs::by_terms::visit(segment, *reader, filter.options().terms, visitor);
    ASSERT_EQ(1, visitor.prepare_calls_counter());
    ASSERT_EQ(3, visitor.visit_calls_counter());
    ASSERT_EQ((std::vector<std::pair<std::string_view, irs::score_t>>{
                {"abc", 0.5f}, {"abcd", 1.f}, {"abcy", 0.5f}}),
              visitor.term_refs<char>());
  }

  // duplicate terms are not allowed
  {
    const Docs result{1, 21, 31, 32};
    const Costs costs{result.size()};
    const auto filter = make_filter(
      "prefix", {{"abcd", 1.f}, {"abcd", 0.f}, {"abc", 0.5f}, {"abcy", 0.5f}});
    CheckQuery(filter, result, costs, rdr);

    // test visit
    tests::empty_filter_visitor visitor;
    const auto* reader = segment.field("prefix");
    ASSERT_NE(nullptr, reader);
    irs::by_terms::visit(segment, *reader, filter.options().terms, visitor);
    ASSERT_EQ(1, visitor.prepare_calls_counter());
    ASSERT_EQ(3, visitor.visit_calls_counter());
    ASSERT_EQ((std::vector<std::pair<std::string_view, irs::score_t>>{
                {"abc", 0.5f}, {"abcd", 1.f}, {"abcy", 0.5f}}),
              visitor.term_refs<char>());
  }

  // test non existing term
  {
    const Docs result{1, 21, 31, 32};
    const Costs costs{result.size()};
    const auto filter = make_filter(
      "prefix",
      {{"abcd", 1.f}, {"invalid_term", 0.f}, {"abc", 0.5f}, {"abcy", 0.5f}});
    CheckQuery(filter, result, costs, rdr);

    // test visit
    tests::empty_filter_visitor visitor;
    const auto* reader = segment.field("prefix");
    ASSERT_NE(nullptr, reader);
    irs::by_terms::visit(segment, *reader, filter.options().terms, visitor);
    ASSERT_EQ(1, visitor.prepare_calls_counter());
    ASSERT_EQ(3, visitor.visit_calls_counter());
    ASSERT_EQ((std::vector<std::pair<std::string_view, irs::score_t>>{
                {"abc", 0.5f}, {"abcd", 1.f}, {"abcy", 0.5f}}),
              visitor.term_refs<char>());
  }
}

TEST_P(terms_filter_test_case, min_match) {
  // write segments
  auto writer = open_writer(irs::OM_CREATE);

  {
    tests::json_doc_generator gen{resource("AdventureWorks2014.json"),
                                  &tests::generic_json_field_factory};
    add_segment(*writer, gen);
  }
  {
    tests::json_doc_generator gen{resource("AdventureWorks2014Edges.json"),
                                  &tests::generic_json_field_factory};
    add_segment(*writer, gen);
  }
  {
    tests::json_doc_generator gen{resource("Northwnd.json"),
                                  &tests::generic_json_field_factory};
    add_segment(*writer, gen);
  }
  {
    tests::json_doc_generator gen{resource("NorthwndEdges.json"),
                                  &tests::generic_json_field_factory};
    add_segment(*writer, gen);
  }

  auto rdr = open_reader();
  ASSERT_EQ(4, rdr.size());

  {
    const auto& segment = rdr[0];
    tests::empty_filter_visitor visitor;
    const auto* reader = segment.field("Fields");
    ASSERT_NE(nullptr, reader);
    const auto filter =
      make_filter("Fields", {{"BusinessEntityID", 1.f}, {"StartDate", 1.f}}, 1);
    irs::by_terms::visit(segment, *reader, filter.options().terms, visitor);
    ASSERT_EQ(1, visitor.prepare_calls_counter());
    ASSERT_EQ(2, visitor.visit_calls_counter());
    ASSERT_EQ((std::vector<std::pair<std::string_view, irs::score_t>>{
                {"BusinessEntityID", 1.f}, {"StartDate", 1.f}}),
              visitor.term_refs<char>());
  }

  {
    const Docs result{4,  5,  6,  7,  19, 20, 21, 22, 25, 27, 28, 29,
                      30, 34, 38, 46, 52, 53, 57, 62, 65, 69, 70};
    const Costs costs{25, 0, 0, 0};
    const auto filter =
      make_filter("Fields", {{"BusinessEntityID", 1.f}, {"StartDate", 1.f}}, 1);
    CheckQuery(filter, result, costs, rdr);
  }

  {
    const Docs result{21, 57};
    // FIXME(gnusi): fix estimation, it's not accurate
    const Costs costs{7, 0, 0, 0};
    const auto filter =
      make_filter("Fields", {{"BusinessEntityID", 1.f}, {"StartDate", 1.f}}, 2);
    CheckQuery(filter, result, costs, rdr);
  }

  {
    const Docs result{21, 57};
    // FIXME(gnusi): fix estimation, it's not accurate
    const Costs costs{7, 0, 0, 0};
    const auto filter = make_filter(
      "Fields",
      {{"BusinessEntityID", 1.f}, {"StartDate", 1.f}, {"InvalidValue", 1.f}},
      2);
    CheckQuery(filter, result, costs, rdr);
  }

  {
    const Docs result{};
    const Costs costs{0, 0, 0, 0};
    const auto filter =
      make_filter("Fields", {{"BusinessEntityID", 1.f}, {"StartDate", 1.f}}, 3);
    CheckQuery(filter, result, costs, rdr);
  }

  {
    const Docs result{};
    const Costs costs{0, 0, 0, 0};
    const auto filter = make_filter("Fields",
                                    {{"BusinessEntityID", 1.f},
                                     {"StartDate", 1.f},
                                     {"InvalidValue0", 1.f},
                                     {"InvalidValue0", 1.f}},
                                    3);
    CheckQuery(filter, result, costs, rdr);
  }

  // empty prefix test collector call count for field/term/finish
  {
    ScoredDocs result(71);
    for (irs::doc_id_t i = 0; auto& [doc, scores] : result) {
      doc = ++i;
      scores = {0.f};
    }
    for (const auto doc : {4,  5,  6,  7,  19, 20, 21, 22, 25, 27, 28, 29,
                           30, 34, 38, 46, 52, 53, 57, 62, 65, 69, 70}) {
      result[doc - 1].second = {static_cast<float_t>(doc)};
    }
    for (const auto doc : {21, 57}) {
      result[doc - 1].second = {static_cast<float_t>(2 * doc)};
    }

    const Costs costs{25, 0, 0, 0};

    size_t collect_field_count = 0;
    size_t collect_term_count = 0;
    size_t finish_count = 0;

    irs::Scorer::ptr impl{std::make_unique<tests::sort::custom_sort>()};
    auto* scorer = static_cast<tests::sort::custom_sort*>(impl.get());

    scorer->collector_collect_field = [&collect_field_count](
                                        const irs::SubReader&,
                                        const irs::term_reader&) -> void {
      ++collect_field_count;
    };
    scorer->collector_collect_term =
      [&collect_term_count](const irs::SubReader&, const irs::term_reader&,
                            const irs::attribute_provider&) -> void {
      ++collect_term_count;
    };
    scorer->collectors_collect_ =
      [&finish_count](irs::byte_type*, const irs::FieldCollector*,
                      const irs::TermCollector*) -> void { ++finish_count; };
    scorer->prepare_field_collector_ = [&scorer]() -> irs::FieldCollector::ptr {
      return std::make_unique<tests::sort::custom_sort::field_collector>(
        *scorer);
    };
    scorer->prepare_term_collector_ = [&scorer]() -> irs::TermCollector::ptr {
      return std::make_unique<tests::sort::custom_sort::term_collector>(
        *scorer);
    };
    scorer->prepare_scorer_ =
      [](const irs::ColumnProvider&, const irs::feature_map_t&,
         const irs::byte_type*, const irs::attribute_provider& attrs,
         irs::score_t boost) -> irs::ScoreFunction {
      struct ScoreCtx final : public irs::score_ctx {
        ScoreCtx(const irs::document* doc, irs::score_t boost) noexcept
          : doc{doc}, boost{boost} {}
        const irs::document* doc;
        irs::score_t boost;
      };

      auto* doc = irs::get<irs::document>(attrs);
      EXPECT_NE(nullptr, doc);

      return irs::ScoreFunction::Make<ScoreCtx>(
        [](irs::score_ctx* ctx, irs::score_t* res) noexcept {
          auto* state = static_cast<ScoreCtx*>(ctx);
          *res = static_cast<irs::score_t>(state->doc->value) * state->boost;
        },
        irs::ScoreFunction::DefaultMin, doc, boost);
    };

    const auto filter =
      make_filter("Fields", {{"BusinessEntityID", 1.f}, {"StartDate", 1.f}}, 0);

    CheckQuery(filter, std::span{&impl, 1}, result, rdr[0]);
    ASSERT_EQ(1, collect_field_count);  // 1 fields in 1 segment
    ASSERT_EQ(2, collect_term_count);   // 2 different terms
    ASSERT_EQ(3, finish_count);         // 3 unque terms
  }
}

static constexpr auto kTestDirs = tests::getDirectories<tests::kTypesDefault>();

INSTANTIATE_TEST_SUITE_P(
  terms_filter_test, terms_filter_test_case,
  ::testing::Combine(::testing::ValuesIn(kTestDirs),
                     ::testing::Values(tests::format_info{"1_0"},
                                       tests::format_info{"1_1", "1_0"},
                                       tests::format_info{"1_2", "1_0"},
                                       tests::format_info{"1_3", "1_0"})),
  terms_filter_test_case::to_string);
