////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include "search/wildcard_filter.hpp"

#include "filter_test_case_base.hpp"
#include "search/all_filter.hpp"
#include "search/multiterm_query.hpp"
#include "search/prefix_filter.hpp"
#include "search/term_filter.hpp"
#include "tests_shared.hpp"

namespace {

template<typename Filter = irs::by_wildcard>
Filter make_filter(std::string_view field, std::string_view term) {
  Filter q;
  *q.mutable_field() = field;
  q.mutable_options()->term = irs::ViewCast<irs::byte_type>(term);
  return q;
}

}  // namespace

TEST(by_wildcard_test, options) {
  irs::by_wildcard_options opts;
  ASSERT_TRUE(opts.term.empty());
  ASSERT_EQ(1024, opts.scored_terms_limit);
}

TEST(by_wildcard_test, ctor) {
  irs::by_wildcard q;
  ASSERT_EQ(irs::type<irs::by_wildcard>::id(), q.type());
  ASSERT_EQ(irs::by_wildcard_options{}, q.options());
  ASSERT_TRUE(q.field().empty());
  ASSERT_EQ(irs::kNoBoost, q.boost());
}

TEST(by_wildcard_test, equal) {
  const irs::by_wildcard q = make_filter("field", "bar*");

  ASSERT_EQ(q, make_filter("field", "bar*"));
  ASSERT_NE(q, make_filter("field1", "bar*"));
  ASSERT_NE(q, make_filter("field", "bar"));

  irs::by_wildcard q1 = make_filter("field", "bar*");
  q1.mutable_options()->scored_terms_limit = 100;
  ASSERT_NE(q, q1);
}

TEST(by_wildcard_test, boost) {
  MaxMemoryCounter counter;

  // no boost
  {
    irs::by_wildcard q = make_filter("field", "bar*");

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

    irs::by_wildcard q = make_filter("field", "bar*");
    q.boost(boost);

    auto prepared = q.prepare({
      .index = irs::SubReader::empty(),
      .memory = counter,
    });
    ASSERT_EQ(boost, prepared->boost());
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
}

#ifdef __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpotentially-evaluated-expression"
#endif  // __clang__

TEST(by_wildcard_test, test_type_of_prepared_query) {
  MaxMemoryCounter counter;

  // term query
  {
    auto lhs = make_filter<irs::by_term>("foo", "bar")
                 .prepare({
                   .index = irs::SubReader::empty(),
                   .memory = counter,
                 });
    auto rhs = make_filter("foo", "bar")
                 .prepare({
                   .index = irs::SubReader::empty(),
                   .memory = counter,
                 });
    ASSERT_EQ(typeid(*lhs), typeid(*rhs));
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  // term query
  {
    auto lhs = make_filter<irs::by_term>("foo", "").prepare({
      .index = irs::SubReader::empty(),
      .memory = counter,
    });
    auto rhs = make_filter("foo", "").prepare({
      .index = irs::SubReader::empty(),
      .memory = counter,
    });
    ASSERT_EQ(typeid(*lhs), typeid(*rhs));
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  // term query
  {
    auto lhs = make_filter<irs::by_term>("foo", "foo%")
                 .prepare({
                   .index = irs::SubReader::empty(),
                   .memory = counter,
                 });
    auto rhs = make_filter("foo", "foo\\%")
                 .prepare({
                   .index = irs::SubReader::empty(),
                   .memory = counter,
                 });
    ASSERT_EQ(typeid(*lhs), typeid(*rhs));
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  // prefix query
  {
    auto lhs = make_filter<irs::by_prefix>("foo", "bar")
                 .prepare({
                   .index = irs::SubReader::empty(),
                   .memory = counter,
                 });
    auto rhs = make_filter("foo", "bar%")
                 .prepare({
                   .index = irs::SubReader::empty(),
                   .memory = counter,
                 });
    ASSERT_EQ(typeid(*lhs), typeid(*rhs));
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  // prefix query
  {
    auto lhs = make_filter<irs::by_prefix>("foo", "bar")
                 .prepare({
                   .index = irs::SubReader::empty(),
                   .memory = counter,
                 });
    auto rhs = make_filter("foo", "bar%%")
                 .prepare({
                   .index = irs::SubReader::empty(),
                   .memory = counter,
                 });
    ASSERT_EQ(typeid(*lhs), typeid(*rhs));
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  // term query
  {
    auto lhs = make_filter<irs::by_term>("foo", "bar%")
                 .prepare({
                   .index = irs::SubReader::empty(),
                   .memory = counter,
                 });
    auto rhs = make_filter("foo", "bar\\%")
                 .prepare({
                   .index = irs::SubReader::empty(),
                   .memory = counter,
                 });
    ASSERT_EQ(typeid(*lhs), typeid(*rhs));
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  // all query
  {
    auto lhs = make_filter<irs::by_prefix>("foo", "").prepare({
      .index = irs::SubReader::empty(),
      .memory = counter,
    });
    auto rhs = make_filter("foo", "%")
                 .prepare({
                   .index = irs::SubReader::empty(),
                   .memory = counter,
                 });
    ASSERT_EQ(typeid(*lhs), typeid(*rhs));
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  // all query
  {
    auto lhs = make_filter<irs::by_prefix>("foo", "").prepare({
      .index = irs::SubReader::empty(),
      .memory = counter,
    });
    auto rhs = make_filter("foo", "%%")
                 .prepare({
                   .index = irs::SubReader::empty(),
                   .memory = counter,
                 });
    ASSERT_EQ(typeid(*lhs), typeid(*rhs));
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  // term query
  {
    auto lhs = make_filter<irs::by_term>("foo", "%")
                 .prepare({
                   .index = irs::SubReader::empty(),
                   .memory = counter,
                 });
    auto rhs = make_filter("foo", "\\%")
                 .prepare({
                   .index = irs::SubReader::empty(),
                   .memory = counter,
                 });
    ASSERT_EQ(typeid(*lhs), typeid(*rhs));
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
}

#ifdef __clang__
#pragma GCC diagnostic pop
#endif  // __clang__

class wildcard_filter_test_case : public tests::FilterTestCaseBase {};

TEST_P(wildcard_filter_test_case, simple_sequential_order) {
  // add segment
  {
    tests::json_doc_generator gen(resource("simple_sequential.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  // empty query
  CheckQuery(irs::by_wildcard(), Docs{}, Costs{0}, rdr);

  // empty prefix test collector call count for field/term/finish
  {
    Docs docs{1, 4, 9, 16, 21, 24, 26, 29, 31, 32};
    Costs costs{docs.size()};

    size_t collect_field_count = 0;
    size_t collect_term_count = 0;
    size_t finish_count = 0;

    std::array<irs::Scorer::ptr, 1> order{
      std::make_unique<tests::sort::custom_sort>()};
    auto& scorer = static_cast<tests::sort::custom_sort&>(*order.front());

    scorer.collector_collect_field = [&collect_field_count](
                                       const irs::SubReader&,
                                       const irs::term_reader&) -> void {
      ++collect_field_count;
    };
    scorer.collector_collect_term =
      [&collect_term_count](const irs::SubReader&, const irs::term_reader&,
                            const irs::attribute_provider&) -> void {
      ++collect_term_count;
    };
    scorer.collectors_collect_ =
      [&finish_count](irs::byte_type*, const irs::FieldCollector*,
                      const irs::TermCollector*) -> void { ++finish_count; };
    scorer.prepare_field_collector_ = [&scorer]() -> irs::FieldCollector::ptr {
      return std::make_unique<tests::sort::custom_sort::field_collector>(
        scorer);
    };
    scorer.prepare_term_collector_ = [&scorer]() -> irs::TermCollector::ptr {
      return std::make_unique<tests::sort::custom_sort::term_collector>(scorer);
    };
    CheckQuery(make_filter("prefix", "%"), order, docs, rdr);
    ASSERT_EQ(9, collect_field_count);  // 9 fields (1 per term since treated as
                                        // a disjunction) in 1 segment
    ASSERT_EQ(9, collect_term_count);   // 9 different terms
    ASSERT_EQ(9, finish_count);         // 9 unque terms
  }

  // match all
  {
    Docs docs{31, 32, 1, 4, 9, 16, 21, 24, 26, 29};
    Costs costs{docs.size()};

    std::array<irs::Scorer::ptr, 1> order{
      std::make_unique<tests::sort::frequency_sort>()};

    CheckQuery(make_filter("prefix", "%"), order, docs, rdr);
  }

  // prefix
  {
    Docs docs{31, 32, 1, 4, 16, 21, 26, 29};
    Costs costs{docs.size()};

    std::array<irs::Scorer::ptr, 1> order{
      std::make_unique<tests::sort::frequency_sort>()};

    CheckQuery(make_filter("prefix", "a%"), order, docs, rdr);
  }
}

TEST_P(wildcard_filter_test_case, simple_sequential) {
  // add segment
  {
    tests::json_doc_generator gen(resource("simple_sequential_utf8.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  // empty query
  CheckQuery(irs::by_wildcard(), Docs{}, Costs{0}, rdr);

  // empty field
  CheckQuery(make_filter("", "xyz%"), Docs{}, Costs{0}, rdr);

  // invalid field
  CheckQuery(make_filter("same1", "xyz%"), Docs{}, Costs{0}, rdr);

  // invalid prefix
  CheckQuery(make_filter("same", "xyz_invalid%"), Docs{}, Costs{0}, rdr);

  // empty pattern - no match
  CheckQuery(make_filter("duplicated", ""), Docs{}, Costs{0}, rdr);

  // match all
  {
    Docs result;
    for (size_t i = 0; i < 32; ++i) {
      result.push_back(irs::doc_id_t((irs::doc_limits::min)() + i));
    }

    Costs costs{result.size()};

    CheckQuery(make_filter("same", "%"), result, costs, rdr);
    CheckQuery(make_filter("same", "___"), result, costs, rdr);
    CheckQuery(make_filter("same", "%_"), result, costs, rdr);
    CheckQuery(make_filter("same", "_%"), result, costs, rdr);
    CheckQuery(make_filter("same", "x_%"), result, costs, rdr);
    CheckQuery(make_filter("same", "__z"), result, costs, rdr);
    CheckQuery(make_filter("same", "%_z"), result, costs, rdr);
    CheckQuery(make_filter("same", "x%_"), result, costs, rdr);
    CheckQuery(make_filter("same", "x_%"), result, costs, rdr);
    CheckQuery(make_filter("same", "x_z"), result, costs, rdr);
    CheckQuery(make_filter("same", "x%z"), result, costs, rdr);
    CheckQuery(make_filter("same", "_yz"), result, costs, rdr);
    CheckQuery(make_filter("same", "%yz"), result, costs, rdr);
    CheckQuery(make_filter("same", "xyz"), result, costs, rdr);
  }

  // match nothing
  CheckQuery(make_filter("prefix", "ab\\%"), Docs{}, Costs{0}, rdr);
  CheckQuery(make_filter("same", "x\\_z"), Docs{}, Costs{0}, rdr);
  CheckQuery(make_filter("same", "x\\%z"), Docs{}, Costs{0}, rdr);
  CheckQuery(make_filter("same", "_"), Docs{}, Costs{0}, rdr);

  // escaped prefix
  {
    Docs result{10, 11};
    Costs costs{result.size()};

    CheckQuery(make_filter("prefix", "ab\\\\%"), result, costs, rdr);
  }

  // escaped term
  {
    Docs result{10};
    Costs costs{result.size()};

    CheckQuery(make_filter("prefix", "ab\\\\\\%"), result, costs, rdr);
  }

  // escaped term
  {
    Docs result{11};
    Costs costs{result.size()};

    CheckQuery(make_filter("prefix", "ab\\\\\\\\%"), result, costs, rdr);
  }

  // valid prefix
  {
    Docs result;
    for (size_t i = 0; i < 32; ++i) {
      result.push_back(irs::doc_id_t((irs::doc_limits::min)() + i));
    }

    Costs costs{result.size()};

    CheckQuery(make_filter("same", "xyz%"), result, costs, rdr);
  }

  // pattern
  {
    Docs docs{2, 3, 8, 14, 17, 19, 24};
    Costs costs{docs.size()};

    CheckQuery(make_filter("duplicated", "v_z%"), docs, costs, rdr);
    CheckQuery(make_filter("duplicated", "v%c"), docs, costs, rdr);
    CheckQuery(make_filter("duplicated", "v%%%%%c"), docs, costs, rdr);
    CheckQuery(make_filter("duplicated", "%c"), docs, costs, rdr);
    CheckQuery(make_filter("duplicated", "%_c"), docs, costs, rdr);
  }

  // pattern
  {
    Docs docs{1, 4, 9, 21, 26, 31, 32};
    Costs costs{docs.size()};

    CheckQuery(make_filter("prefix", "%c%"), docs, costs, rdr);
    CheckQuery(make_filter("prefix", "%c%%"), docs, costs, rdr);
    CheckQuery(make_filter("prefix", "%%%%c%%"), docs, costs, rdr);
    CheckQuery(make_filter("prefix", "%%c%"), docs, costs, rdr);
    CheckQuery(make_filter("prefix", "%%c%%"), docs, costs, rdr);
  }

  // single digit prefix
  {
    Docs docs{1, 5, 11, 21, 27, 31};
    Costs costs{docs.size()};

    CheckQuery(make_filter("duplicated", "a%"), docs, costs, rdr);
  }

  CheckQuery(make_filter("name", "!%"), Docs{28}, Costs{1}, rdr);
  CheckQuery(make_filter("prefix", "b%"), Docs{9, 24}, Costs{2}, rdr);

  // multiple digit prefix
  {
    Docs docs{2, 3, 8, 14, 17, 19, 24};
    Costs costs{docs.size()};

    CheckQuery(make_filter("duplicated", "vcz%"), docs, costs, rdr);
    CheckQuery(make_filter("duplicated", "vcz%%%%%"), docs, costs, rdr);
  }

  {
    Docs docs{1, 4, 21, 26, 31, 32};
    Costs costs{docs.size()};
    CheckQuery(make_filter("prefix", "abc%"), docs, costs, rdr);
  }

  {
    Docs docs{1, 4, 21, 26, 31, 32};
    Costs costs{docs.size()};

    CheckQuery(make_filter("prefix", "abc%"), docs, costs, rdr);
    CheckQuery(make_filter("prefix", "abc%%"), docs, costs, rdr);
  }

  {
    Docs docs{1, 4, 16, 26};
    Costs costs{docs.size()};

    CheckQuery(make_filter("prefix", "a%d%"), docs, costs, rdr);
    CheckQuery(make_filter("prefix", "a%d%%"), docs, costs, rdr);
  }

  {
    Docs docs{1, 26};
    Costs costs{docs.size()};

    CheckQuery(make_filter("utf8", "\x25\xD0\xB9"), docs, costs, rdr);
    CheckQuery(make_filter("utf8", "\x25\x25\xD0\xB9"), docs, costs, rdr);
  }

  {
    Docs docs{26};
    Costs costs{docs.size()};

    CheckQuery(make_filter("utf8", "\xD0\xB2\x25\xD0\xB9"), docs, costs, rdr);
    CheckQuery(make_filter("utf8", "\xD0\xB2\x25\x25\xD0\xB9"), docs, costs,
               rdr);
  }

  {
    Docs docs{1, 3};
    Costs costs{docs.size()};

    CheckQuery(make_filter("utf8", "\xD0\xBF\x25"), docs, costs, rdr);
    CheckQuery(make_filter("utf8", "\xD0\xBF\x25\x25"), docs, costs, rdr);
  }

  // whole word
  CheckQuery(make_filter("prefix", "bateradsfsfasdf"), Docs{24}, Costs{1}, rdr);
}

TEST_P(wildcard_filter_test_case, visit) {
  // add segment
  {
    tests::json_doc_generator gen(resource("simple_sequential.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  std::string fld = "prefix";
  std::string_view field = std::string_view(fld);

  // read segment
  auto index = open_reader();
  ASSERT_EQ(1, index.size());
  auto& segment = index[0];
  // get term dictionary for field
  const auto* reader = segment.field(field);
  ASSERT_NE(nullptr, reader);

  {
    auto term = irs::ViewCast<irs::byte_type>(std::string_view("abc"));
    tests::empty_filter_visitor visitor;
    auto field_visitor = irs::by_wildcard::visitor(term);
    ASSERT_TRUE(field_visitor);
    field_visitor(segment, *reader, visitor);
    ASSERT_EQ(1, visitor.prepare_calls_counter());
    ASSERT_EQ(1, visitor.visit_calls_counter());
    ASSERT_EQ((std::vector<std::pair<std::string_view, irs::score_t>>{
                {"abc", irs::kNoBoost},
              }),
              visitor.term_refs<char>());

    visitor.reset();
  }

  {
    auto prefix = irs::ViewCast<irs::byte_type>(std::string_view("ab%"));
    tests::empty_filter_visitor visitor;
    auto field_visitor = irs::by_wildcard::visitor(prefix);
    ASSERT_TRUE(field_visitor);
    field_visitor(segment, *reader, visitor);
    ASSERT_EQ(1, visitor.prepare_calls_counter());
    ASSERT_EQ(6, visitor.visit_calls_counter());
    ASSERT_EQ((std::vector<std::pair<std::string_view, irs::score_t>>{
                {"abc", irs::kNoBoost},
                {"abcd", irs::kNoBoost},
                {"abcde", irs::kNoBoost},
                {"abcdrer", irs::kNoBoost},
                {"abcy", irs::kNoBoost},
                {"abde", irs::kNoBoost}}),
              visitor.term_refs<char>());

    visitor.reset();
  }

  {
    auto wildcard = irs::ViewCast<irs::byte_type>(std::string_view("a_c%"));
    tests::empty_filter_visitor visitor;
    auto field_visitor = irs::by_wildcard::visitor(wildcard);
    ASSERT_TRUE(field_visitor);
    field_visitor(segment, *reader, visitor);
    ASSERT_EQ(1, visitor.prepare_calls_counter());
    ASSERT_EQ(5, visitor.visit_calls_counter());
    ASSERT_EQ((std::vector<std::pair<std::string_view, irs::score_t>>{
                {"abc", irs::kNoBoost},
                {"abcd", irs::kNoBoost},
                {"abcde", irs::kNoBoost},
                {"abcdrer", irs::kNoBoost},
                {"abcy", irs::kNoBoost},
              }),
              visitor.term_refs<char>());

    visitor.reset();
  }
}

static constexpr auto kTestDirs = tests::getDirectories<tests::kTypesDefault>();

INSTANTIATE_TEST_SUITE_P(
  wildcard_filter_test, wildcard_filter_test_case,
  ::testing::Combine(::testing::ValuesIn(kTestDirs),
                     ::testing::Values(tests::format_info{"1_0"},
                                       tests::format_info{"1_1", "1_0"},
                                       tests::format_info{"1_2", "1_0"},
                                       tests::format_info{"1_3", "1_0"})),
  wildcard_filter_test_case::to_string);
