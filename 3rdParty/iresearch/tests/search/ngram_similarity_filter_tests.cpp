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
/// @author Andrei Lobov
////////////////////////////////////////////////////////////////////////////////

#include "tests_shared.hpp"
#include "filter_test_case_base.hpp"
#include "search/ngram_similarity_filter.hpp"
#include "search/tfidf.hpp"
#include "search/bm25.hpp"
#include "utils/ngram_match_utils.hpp"

#include <functional>

namespace {

irs::by_ngram_similarity make_filter(const irs::string_ref& field,
                                     const std::vector<irs::string_ref>& ngrams,
                                     float_t threshold = 1.f) {
  irs::by_ngram_similarity filter;
  *filter.mutable_field() = field;
  auto* opts = filter.mutable_options();
  for (auto& ngram : ngrams) {
    opts->ngrams.emplace_back(irs::ref_cast<irs::byte_type>(ngram));
  }
  opts->threshold = threshold;
  return filter;
}

}

namespace tests {

// ----------------------------------------------------------------------------
// --SECTION--                            by_ngram_similarity filter base tests
// ----------------------------------------------------------------------------

TEST(ngram_similarity_base_test, options) {
  irs::by_ngram_similarity_options opts;
  ASSERT_TRUE(opts.ngrams.empty());
  ASSERT_EQ(1.f, opts.threshold);
}

TEST(ngram_similarity_base_test, ctor) {
  irs::by_ngram_similarity q;
  ASSERT_EQ(irs::type<irs::by_ngram_similarity>::id(), q.type());
  ASSERT_EQ(irs::by_ngram_similarity_options{}, q.options());
  ASSERT_EQ(irs::no_boost(), q.boost());
  ASSERT_EQ("", q.field());


  auto& features = irs::by_ngram_similarity::features();
  ASSERT_EQ(2, features.size());
  ASSERT_TRUE(features.check<irs::frequency>());
  ASSERT_TRUE(features.check<irs::position>());
}

TEST(ngram_similarity_base_test, boost) {
  // no boost
  {
    // no terms no field
    {
      irs::by_ngram_similarity q;

      auto prepared = q.prepare(irs::sub_reader::empty());
      ASSERT_EQ(irs::no_boost(), prepared->boost());
    }


    // simple disjunction
    {
      irs::by_ngram_similarity q = make_filter("field", {"1", "2"}, 0.5f);

      auto prepared = q.prepare(irs::sub_reader::empty());
      ASSERT_EQ(irs::no_boost(), prepared->boost());
    }

    // multiple terms
    {
      irs::by_ngram_similarity q = make_filter("field", {"1", "2", "3", "4"}, 0.5f);

      auto prepared = q.prepare(irs::sub_reader::empty());
      ASSERT_EQ(irs::no_boost(), prepared->boost());
    }
  }

  // with boost
  {
    iresearch::boost_t boost = 1.5f;

    // no terms, return empty query
    {
      irs::by_ngram_similarity q;
      q.boost(boost);

      auto prepared = q.prepare(irs::sub_reader::empty());
      ASSERT_EQ(irs::no_boost(), prepared->boost());
    }

    // simple disjunction
    {
      irs::by_ngram_similarity q = make_filter("field", {"1", "2"}, 0.5f);
      q.boost(boost);

      auto prepared = q.prepare(irs::sub_reader::empty());
      ASSERT_EQ(boost, prepared->boost());
    }

    // multiple terms
    {
      irs::by_ngram_similarity q = make_filter("field", {"1", "2", "3", "4"}, 0.5f);
      q.boost(boost);

      auto prepared = q.prepare(irs::sub_reader::empty());
      ASSERT_EQ(boost, prepared->boost());
    }
  }
}

TEST(ngram_similarity_base_test, equal) {
  ASSERT_EQ(irs::by_ngram_similarity(), irs::by_ngram_similarity());

  {
    irs::by_ngram_similarity q0 = make_filter("a", {"1", "2"}, 0.5f);
    irs::by_ngram_similarity q1 = make_filter("a", {"1", "2"}, 0.5f);
    ASSERT_EQ(q0, q1);

    // different threshold
    irs::by_ngram_similarity q2 = make_filter("a", {"1", "2"}, 0.25f);
    ASSERT_NE(q0, q2);

    // different terms
    irs::by_ngram_similarity q3 = make_filter("a", {"1", "3"}, 0.5f);
    ASSERT_NE(q0, q3);

    // different terms count (less)
    irs::by_ngram_similarity q4 = make_filter("a", {"1"}, 0.5f);
    ASSERT_NE(q0, q4);

    // different terms count (more)
    irs::by_ngram_similarity q5 = make_filter("a", {"1", "2", "2"}, 0.5f);
    ASSERT_NE(q0, q5);

    //different field
    irs::by_ngram_similarity q6 = make_filter("b", {"1", "2"}, 0.5f);
    ASSERT_NE(q0, q6);
  }
}

// ----------------------------------------------------------------------------
// --SECTION--                         by_ngram_similarity filter matcher tests
// ----------------------------------------------------------------------------
class ngram_similarity_filter_test_case : public tests::filter_test_case_base {
};


TEST_P(ngram_similarity_filter_test_case, check_matcher_1) {
  // sequence 1 3 4 ______ 2 -> longest is 134 not 12
  // add segment
  {
    tests::json_doc_generator gen(
      "[{ \"seq\" : 1, \"field\": [ \"1\", \"3\", \"4\", \"5\", \"6\", \"7\", \"2\"] }]",
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();
  irs::order order;
  order.add<tests::sort::custom_sort>(false);
  irs::by_ngram_similarity filter = make_filter("field", {"1", "2", "3", "4"}, 0.5f);

  auto prepared_order = order.prepare();
  auto prepared = filter.prepare(rdr, prepared_order);
  for (const auto& sub : rdr) {
    auto docs = prepared->execute(sub, prepared_order);
    auto* doc = irs::get<irs::document>(*docs);
    auto* boost = irs::get<irs::filter_boost>(*docs);
    auto* frequency = irs::get<irs::frequency>(*docs);
    ASSERT_TRUE(bool(doc)); // ensure all iterators contain "document" attribute
    ASSERT_TRUE(bool(boost));
    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::eof(doc->value));
    ASSERT_DOUBLE_EQ(0.75, boost->value);
    const irs::string_ref rhs = "134";
    const irs::string_ref lhs = "1234";
    ASSERT_DOUBLE_EQ(
      boost->value,
      (irs::ngram_similarity<char, true>(lhs.begin(), lhs.size(), rhs.begin(), rhs.size(), 1)));
    ASSERT_EQ(1, frequency->value);
    ASSERT_FALSE(docs->next());
  }
}

TEST_P(ngram_similarity_filter_test_case, check_matcher_2) {
  // sequence 1 1 2 2 3 3 4 4 -> longest is 1234  and freq should be 1 not 2 as
  // this sequence could not be built twice one after another but only
  // intereaved
  {
    tests::json_doc_generator gen(
      "[{ \"seq\" : 1, \"field\": [ \"1\", \"1\", \"2\", \"2\", \"3\", \"3\", \"4\", \"4\"] }]",
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();
  irs::order order;
  order.add<tests::sort::custom_sort>(false);
  irs::by_ngram_similarity filter = make_filter("field", {"1", "2", "3", "4"}, 0.5f);

  auto prepared_order = order.prepare();
  auto prepared = filter.prepare(rdr, prepared_order);
  for (const auto& sub : rdr) {
    auto docs = prepared->execute(sub, prepared_order);
    auto* doc = irs::get<irs::document>(*docs);
    auto* boost = irs::get<irs::filter_boost>(*docs);
    auto* frequency = irs::get<irs::frequency>(*docs);
    // ensure all iterators contain  attributes
    ASSERT_TRUE(bool(doc));
    ASSERT_TRUE(bool(boost));
    ASSERT_TRUE(bool(frequency));
    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::eof(doc->value));
    ASSERT_DOUBLE_EQ(1, boost->value);
    const irs::string_ref rhs = "11223344";
    const irs::string_ref lhs = "1234";
    ASSERT_DOUBLE_EQ(
      boost->value,
      (irs::ngram_similarity<char, true>(lhs.begin(), lhs.size(), rhs.begin(), rhs.size(), 1)));
    ASSERT_EQ(1, frequency->value);
    ASSERT_FALSE(docs->next());
  }
}

TEST_P(ngram_similarity_filter_test_case, check_matcher_3) {
  // sequence 1 2 1 1 3 4 -> longest is 1234  not 134!
  {
    tests::json_doc_generator gen(
      "[{ \"seq\" : 1, \"field\": [ \"1\", \"2\", \"1\", \"1\", \"3\", \"4\"] }]",
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();
  irs::order order;
  order.add<tests::sort::custom_sort>(false);

  irs::by_ngram_similarity filter = make_filter("field", {"1", "2", "3", "4"}, 0.5f);

  auto prepared_order = order.prepare();
  auto prepared = filter.prepare(rdr, prepared_order);
  for (const auto& sub : rdr) {
    auto docs = prepared->execute(sub, prepared_order);
    auto* doc = irs::get<irs::document>(*docs);
    auto* boost = irs::get<irs::filter_boost>(*docs);
    auto* frequency = irs::get<irs::frequency>(*docs);
    // ensure all iterators contain  attributes
    ASSERT_TRUE(bool(doc));
    ASSERT_TRUE(bool(boost));
    ASSERT_TRUE(bool(frequency));
    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::eof(doc->value));
    ASSERT_DOUBLE_EQ(1, boost->value);
    const irs::string_ref rhs = "121134";
    const irs::string_ref lhs = "1234";
    ASSERT_DOUBLE_EQ(
      boost->value,
      (irs::ngram_similarity<char, true>(lhs.begin(), lhs.size(), rhs.begin(), rhs.size(), 1)));
    ASSERT_EQ(1, frequency->value);
    ASSERT_FALSE(docs->next());
  }
}

TEST_P(ngram_similarity_filter_test_case, check_matcher_4) {
  // sequence 1 2 1 1 1 1 pattern 1 1 -> longest is 1 1 and frequency is 2
  {
    tests::json_doc_generator gen(
      "[{ \"seq\" : 1, \"field\": [ \"1\", \"2\", \"1\", \"1\", \"1\", \"1\"] }]",
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();
  irs::order order;
  order.add<tests::sort::custom_sort>(false);

  irs::by_ngram_similarity filter = make_filter("field", {"1", "1"}, 0.5f);

  auto prepared_order = order.prepare();
  auto prepared = filter.prepare(rdr, prepared_order);
  for (const auto& sub : rdr) {
    auto docs = prepared->execute(sub, prepared_order);
    auto* doc = irs::get<irs::document>(*docs);
    auto* boost = irs::get<irs::filter_boost>(*docs);
    auto* frequency = irs::get<irs::frequency>(*docs);
    // ensure all iterators contain  attributes
    ASSERT_TRUE(bool(doc));
    ASSERT_TRUE(bool(boost));
    ASSERT_TRUE(bool(frequency));
    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::eof(doc->value));
    ASSERT_DOUBLE_EQ(1, boost->value);
    const irs::string_ref rhs = "121111";
    const irs::string_ref lhs = "11";
    ASSERT_DOUBLE_EQ(
        boost->value,
        (irs::ngram_similarity<char, true>(lhs.begin(), lhs.size(), rhs.begin(), rhs.size(), 1)));
    ASSERT_EQ(2, frequency->value);
    ASSERT_FALSE(docs->next());
  }
}

TEST_P(ngram_similarity_filter_test_case, check_matcher_5) {
  // sequence 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 pattern 1 2 1 -> longest is 1 2 1 and frequency is 4
  {
    tests::json_doc_generator gen(
      "[{ \"seq\" : 1, \"field\": [ \"1\", \"2\", \"1\", \"2\", \"1\", "
      " \"2\", \"1\", \"2\", \"1\", \"2\", \"1\", \"2\", \"1\", \"2\", \"1\"] }]",
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();
  irs::order order;
  order.add<tests::sort::custom_sort>(false);

  irs::by_ngram_similarity filter = make_filter("field", {"1", "2", "1"}, 0.5f);

  auto prepared_order = order.prepare();
  auto prepared = filter.prepare(rdr, prepared_order);
  for (const auto& sub : rdr) {
    auto docs = prepared->execute(sub, prepared_order);
    auto* doc = irs::get<irs::document>(*docs);
    auto* boost = irs::get<irs::filter_boost>(*docs);
    auto* frequency = irs::get<irs::frequency>(*docs);
    // ensure all iterators contain  attributes
    ASSERT_TRUE(bool(doc));
    ASSERT_TRUE(bool(boost));
    ASSERT_TRUE(bool(frequency));
    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::eof(doc->value));
    ASSERT_DOUBLE_EQ(1, boost->value);
    const irs::string_ref rhs = "121212121212121";
    const irs::string_ref lhs = "121";
    ASSERT_DOUBLE_EQ(
      boost->value,
      (irs::ngram_similarity<char, true>(lhs.begin(), lhs.size(), rhs.begin(), rhs.size(), 1)));
    ASSERT_EQ(4, frequency->value);
    ASSERT_FALSE(docs->next());
  }
}

TEST_P(ngram_similarity_filter_test_case, check_matcher_6) {
  // sequence 1 1 pattern 1 1 -> longest is 1 1 and frequency is 1
  // checks seek for second term does not  skips it at all
  {
    tests::json_doc_generator gen(
      "[{ \"seq\" : 1, \"field\": [ \"1\", \"1\"] }]",
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();
  irs::order order;
  order.add<tests::sort::custom_sort>(false);

  irs::by_ngram_similarity filter = make_filter("field", {"1", "1"}, 1.0f);

  auto prepared_order = order.prepare();
  auto prepared = filter.prepare(rdr, prepared_order);
  for (const auto& sub : rdr) {
    auto docs = prepared->execute(sub, prepared_order);
    auto* doc = irs::get<irs::document>(*docs);
    auto* boost = irs::get<irs::filter_boost>(*docs);
    auto* frequency = irs::get<irs::frequency>(*docs);
    // ensure all iterators contain  attributes
    ASSERT_TRUE(bool(doc));
    ASSERT_TRUE(bool(boost));
    ASSERT_TRUE(bool(frequency));
    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::eof(doc->value));
    ASSERT_DOUBLE_EQ(1, boost->value);
    const irs::string_ref rhs = "11";
    const irs::string_ref lhs = "11";
    ASSERT_DOUBLE_EQ(
      boost->value,
      (irs::ngram_similarity<char, true>(lhs.begin(), lhs.size(), rhs.begin(), rhs.size(), 1)));
    ASSERT_EQ(1, frequency->value);
    ASSERT_FALSE(docs->next());
  }
}

TEST_P(ngram_similarity_filter_test_case, check_matcher_7) {
    // sequence 2 4 2 4 1 3 1 3 pattern 1 2 3 4-> longest is 1 3  and 2 4 but frequency is 2
    // as only first longest counted
    {
      tests::json_doc_generator gen(
        "[{ \"seq\" : 1, \"field\": [ \"2\", \"4\", \"2\", \"4\", \"1\", \"3\", \"1\", \"3\"] }]",
        &tests::generic_json_field_factory);
      add_segment(gen);
    }

    auto rdr = open_reader();
    irs::order order;
    order.add<tests::sort::custom_sort>(false);

    irs::by_ngram_similarity filter = make_filter("field", {"1", "2", "3", "4"}, 0.5f);

    auto prepared_order = order.prepare();
    auto prepared = filter.prepare(rdr, prepared_order);
    for (const auto& sub : rdr) {
        auto docs = prepared->execute(sub, prepared_order);
        auto* doc = irs::get<irs::document>(*docs);
        auto* boost = irs::get<irs::filter_boost>(*docs);
        auto* frequency = irs::get<irs::frequency>(*docs);
        // ensure all iterators contain  attributes
        ASSERT_TRUE(bool(doc));
        ASSERT_TRUE(bool(boost));
        ASSERT_TRUE(bool(frequency));
        ASSERT_TRUE(docs->next());
        ASSERT_EQ(docs->value(), doc->value);
        ASSERT_FALSE(irs::doc_limits::eof(doc->value));
        ASSERT_DOUBLE_EQ(0.5, boost->value);
        const irs::string_ref rhs = "24241313";
        const irs::string_ref lhs = "1234";
        ASSERT_DOUBLE_EQ(
          boost->value,
          (irs::ngram_similarity<char, true>(lhs.begin(), lhs.size(), rhs.begin(), rhs.size(), 1)));
        ASSERT_EQ(2, frequency->value);
        ASSERT_FALSE(docs->next());
    }
}

TEST_P(ngram_similarity_filter_test_case, check_matcher_8) {
  // sequence 1 2 3 4 pattern 1 5 6 2 -> longest is 1 2  and  boost 0.5
  // as only first longest counted
  {
    tests::json_doc_generator gen(
      "[{ \"seq\" : 1, \"field\": [ \"1\", \"2\", \"3\", \"4\"] }]",
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();
  irs::order order;
  order.add<tests::sort::custom_sort>(false);

  irs::by_ngram_similarity filter = make_filter("field", {"1", "5", "6", "2"}, 0.5f);

  auto prepared_order = order.prepare();
  auto prepared = filter.prepare(rdr, prepared_order);
  for (const auto& sub : rdr) {
    auto docs = prepared->execute(sub, prepared_order);
    auto* doc = irs::get<irs::document>(*docs);
    auto* boost = irs::get<irs::filter_boost>(*docs);
    auto* frequency = irs::get<irs::frequency>(*docs);
    // ensure all iterators contain  attributes
    ASSERT_TRUE(bool(doc));
    ASSERT_TRUE(bool(boost));
    ASSERT_TRUE(bool(frequency));
    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::eof(doc->value));
    ASSERT_DOUBLE_EQ(0.5, boost->value);
    const irs::string_ref lhs = "1234";
    const irs::string_ref rhs = "1562";
    ASSERT_DOUBLE_EQ(
      boost->value,
      (irs::ngram_similarity<char, true>(lhs.begin(), lhs.size(), rhs.begin(), rhs.size(), 1)));
    ASSERT_EQ(1, frequency->value);
    ASSERT_FALSE(docs->next());
  }
}

TEST_P(ngram_similarity_filter_test_case, check_matcher_9) {
  // bulk pos read check (for future optimization)
  // sequence 1 1 2 3 4 5 1  pattern 1 2 3 4 5 1 -> longest is 1 2 3 4 5 1   and  boost 1 and frequency 1
  {
    tests::json_doc_generator gen(
      "[{ \"seq\" : 1, \"field\": [ \"1\", \"1\", \"2\", \"3\", \"4\", \"5\", \"1\"] }]",
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();
  irs::order order;
  order.add<tests::sort::custom_sort>(false);

  irs::by_ngram_similarity filter = make_filter("field", {"1", "2", "3", "4", "5", "1"}, 0.5f);

  auto prepared_order = order.prepare();
  auto prepared = filter.prepare(rdr, prepared_order);
  for (const auto& sub : rdr) {
    auto docs = prepared->execute(sub, prepared_order);
    auto* doc = irs::get<irs::document>(*docs);
    auto* boost = irs::get<irs::filter_boost>(*docs);
    auto* frequency = irs::get<irs::frequency>(*docs);
    // ensure all iterators contain  attributes
    ASSERT_TRUE(bool(doc));
    ASSERT_TRUE(bool(boost));
    ASSERT_TRUE(bool(frequency));
    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::eof(doc->value));
    ASSERT_DOUBLE_EQ(1., boost->value);
    const irs::string_ref rhs = "1123451";
    const irs::string_ref lhs = "123451";
    ASSERT_DOUBLE_EQ(
      boost->value,
      (irs::ngram_similarity<char, true>(lhs.begin(), lhs.size(), rhs.begin(), rhs.size(), 1)));
    ASSERT_EQ(1, frequency->value);
    ASSERT_FALSE(docs->next());
  }
}

TEST_P(ngram_similarity_filter_test_case, check_matcher_10) {
  // bulk pos read check (for future optimization)
  // sequence '' pattern '' -> longest is ''   and  boost 1 and frequency 1
  {
    tests::json_doc_generator gen(
      "[{ \"seq\" : 1, \"field\": [ \"\"] }]",
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();
  irs::order order;
  order.add<tests::sort::custom_sort>(false);

  irs::by_ngram_similarity filter = make_filter("field", {""}, 0.5f);

  auto prepared_order = order.prepare();
  auto prepared = filter.prepare(rdr, prepared_order);
  for (const auto& sub : rdr) {
    auto docs = prepared->execute(sub, prepared_order);
    auto* doc = irs::get<irs::document>(*docs);
    auto* boost = irs::get<irs::filter_boost>(*docs);
    auto* frequency = irs::get<irs::frequency>(*docs);
    // ensure all iterators contain  attributes
    ASSERT_TRUE(bool(doc));
    ASSERT_TRUE(bool(boost));
    ASSERT_TRUE(bool(frequency));
    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::eof(doc->value));
    ASSERT_DOUBLE_EQ(1., boost->value);
    const irs::string_ref rhs = "";
    const irs::string_ref lhs = "";
    ASSERT_DOUBLE_EQ(
      boost->value,
      (irs::ngram_similarity<char, true>(lhs.begin(), lhs.size(), rhs.begin(), rhs.size(), 1)));
    ASSERT_EQ(1, frequency->value);
    ASSERT_FALSE(docs->next());
  }
}

TEST_P(ngram_similarity_filter_test_case, no_match_case) {
  {
    tests::json_doc_generator gen(
      resource("ngram_similarity.json"),
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  irs::by_ngram_similarity filter = make_filter("field", {"ee", "we", "qq", "rr", "ff", "never_match"}, 0.1f);

  auto prepared = filter.prepare(rdr, irs::order::prepared::unordered());
  for (const auto& sub : rdr) {
    auto docs = prepared->execute(sub);

    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc)); // ensure all iterators contain "document" attribute
    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(doc->value));
  }
}

TEST_P(ngram_similarity_filter_test_case, no_serial_match_case) {
  {
    tests::json_doc_generator gen(
      resource("ngram_similarity.json"),
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  irs::by_ngram_similarity filter = make_filter("field", {"ee", "ss", "pa", "rr" }, 0.5f);

  auto prepared = filter.prepare(rdr, irs::order::prepared::unordered());
  for (const auto& sub : rdr) {
    auto docs = prepared->execute(sub);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc)); // ensure all iterators contain "document" attribute
    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(doc->value));
  }
}

TEST_P(ngram_similarity_filter_test_case, one_match_case) {
  {
    tests::json_doc_generator gen(
      resource("ngram_similarity.json"),
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  irs::by_ngram_similarity filter = make_filter("field", {"ee", "ss", "qq", "rr", "ff", "never_match"}, 0.1f);

  docs_t expected{ 1, 3, 5, 6, 7, 8, 9, 10, 12};
  const size_t expected_size = expected.size();
  auto prepared = filter.prepare(rdr, irs::order::prepared::unordered());
  size_t count = 0;
  for (const auto& sub : rdr) {
    auto docs = prepared->execute(sub);

    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc)); // ensure all iterators contain "document" attribute
    while (docs->next()) {
      ASSERT_EQ(docs->value(), doc->value);
      expected.erase(std::remove(expected.begin(), expected.end(), docs->value()), expected.end());
      ++count;
    }
  }
  ASSERT_EQ(expected_size, count);
  ASSERT_EQ(0, expected.size());
}

TEST_P(ngram_similarity_filter_test_case, missed_last_test) {
  {
    tests::json_doc_generator gen(
      resource("ngram_similarity.json"),
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  irs::by_ngram_similarity filter = make_filter("field", {"at", "tl", "la", "as", "ll", "never_match"}, 0.5f);

  docs_t expected{ 1, 2, 5, 8, 11, 12, 13};
  const size_t expected_size = expected.size();
  auto prepared = filter.prepare(rdr, irs::order::prepared::unordered());
  size_t count = 0;
  for (const auto& sub : rdr) {
    auto docs = prepared->execute(sub);

    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc)); // ensure all iterators contain "document" attribute
    while (docs->next()) {
      ASSERT_EQ(docs->value(), doc->value);
      expected.erase(std::remove(expected.begin(), expected.end(), docs->value()), expected.end());
      ++count;
    }
  }
  ASSERT_EQ(expected_size, count);
  ASSERT_EQ(0, expected.size());
}

TEST_P(ngram_similarity_filter_test_case, missed_first_test) {
  {
    tests::json_doc_generator gen(
      resource("ngram_similarity.json"),
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  irs::by_ngram_similarity filter = make_filter("field", {"never_match", "at", "tl", "la", "as", "ll"}, 0.5f);

  docs_t expected{ 1, 2, 5, 8, 11, 12, 13};
  const size_t expected_size = expected.size();
  auto prepared = filter.prepare(rdr, irs::order::prepared::unordered());
  size_t count = 0;
  for (const auto& sub : rdr) {
    auto docs = prepared->execute(sub);

    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc)); // ensure all iterators contain "document" attribute
    while (docs->next()) {
      ASSERT_EQ(docs->value(), doc->value);
      expected.erase(std::remove(expected.begin(), expected.end(), docs->value()), expected.end());
      ++count;
    }
  }
  ASSERT_EQ(expected_size, count);
  ASSERT_EQ(0, expected.size());
}

TEST_P(ngram_similarity_filter_test_case, not_miss_match_for_tail) {
  {
    tests::json_doc_generator gen(
      resource("ngram_similarity.json"),
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  irs::by_ngram_similarity filter = make_filter("field", {"at", "tl", "la", "as", "ll", "never_match"}, 0.33f);

  docs_t expected{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
  const size_t expected_size = expected.size();
  auto prepared = filter.prepare(rdr, irs::order::prepared::unordered());
  size_t count = 0;
  for (const auto& sub : rdr) {
    auto docs = prepared->execute(sub);

    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc)); // ensure all iterators contain "document" attribute
    while (docs->next()) {
      ASSERT_EQ(docs->value(), doc->value);
      expected.erase(std::remove(expected.begin(), expected.end(), docs->value()), expected.end());
      ++count;
    }
  }
  ASSERT_EQ(expected_size, count);
  ASSERT_EQ(0, expected.size());
}


TEST_P(ngram_similarity_filter_test_case, missed_middle_test) {
  {
    tests::json_doc_generator gen(
      resource("ngram_similarity.json"),
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  irs::by_ngram_similarity filter = make_filter("field", {"at", "never_match", "la", "as", "ll"}, 0.333f);

  docs_t expected{ 1, 2, 3, 4, 5, 6, 7, 8, 11, 12, 13, 14};
  const size_t expected_size = expected.size();

  auto prepared = filter.prepare(rdr, irs::order::prepared::unordered());
  size_t count = 0;
  for (const auto& sub : rdr) {
    auto docs = prepared->execute(sub);

    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc)); // ensure all iterators contain "document" attribute
    while (docs->next()) {
      ASSERT_EQ(docs->value(), doc->value);
      expected.erase(std::remove(expected.begin(), expected.end(), docs->value()), expected.end());
      ++count;
    }
  }
  ASSERT_EQ(expected_size, count);
  ASSERT_EQ(0, expected.size());
}

TEST_P(ngram_similarity_filter_test_case, missed_middle2_test) {
  {
    tests::json_doc_generator gen(
      resource("ngram_similarity.json"),
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  irs::by_ngram_similarity filter = make_filter("field", {"at", "never_match", "never_match2", "la", "as", "ll"}, 0.5f);

  docs_t expected{ 1, 2, 5, 8, 11, 12, 13};
  const size_t expected_size = expected.size();

  auto prepared = filter.prepare(rdr, irs::order::prepared::unordered());
  size_t count = 0;
  for (const auto& sub : rdr) {
    auto docs = prepared->execute(sub);

    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc)); // ensure all iterators contain "document" attribute
    while (docs->next()) {
      ASSERT_EQ(docs->value(), doc->value);
      expected.erase(std::remove(expected.begin(), expected.end(), docs->value()), expected.end());
      ++count;
    }
  }
  ASSERT_EQ(expected_size, count);
  ASSERT_EQ(0, expected.size());
}

TEST_P(ngram_similarity_filter_test_case, missed_middle3_test) {
  {
    tests::json_doc_generator gen(
      resource("ngram_similarity.json"),
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  irs::by_ngram_similarity filter = make_filter("field", {"at", "never_match", "tl", "never_match2", "la", "as", "ll"}, 0.28f);

  docs_t expected{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
  const size_t expected_size = expected.size();

  auto prepared = filter.prepare(rdr, irs::order::prepared::unordered());
  size_t count = 0;
  for (const auto& sub : rdr) {
    auto docs = prepared->execute(sub);

    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc)); // ensure all iterators contain "document" attribute
    while (docs->next()) {
      ASSERT_EQ(docs->value(), doc->value);
      expected.erase(std::remove(expected.begin(), expected.end(), docs->value()), expected.end());
      ++count;
    }
  }
  ASSERT_EQ(expected_size, count);
  ASSERT_EQ(0, expected.size());
}

struct test_score_ctx : public irs::score_ctx {
  test_score_ctx(
      std::vector<size_t>* f,
      const irs::frequency* p,
      std::vector<irs::boost_t>* b,
      const irs::filter_boost* fb,
      irs::byte_type* score_buf) noexcept
    : freq(f),
      filter_boost(b),
      freq_from_filter(p),
      boost_from_filter(fb),
      score_buf(score_buf) {
  }

  std::vector<size_t>* freq;
  std::vector<irs::boost_t>* filter_boost;
  const irs::frequency* freq_from_filter;
  const irs::filter_boost* boost_from_filter;
  irs::byte_type* score_buf;
};

TEST_P(ngram_similarity_filter_test_case, missed_last_scored_test) {
  {
    tests::json_doc_generator gen(
      resource("ngram_similarity.json"),
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  irs::by_ngram_similarity filter = make_filter("field", {"at", "tl", "la", "as", "ll", "never_match"}, 0.5f);

  docs_t expected{ 1, 2, 5, 8, 11, 12, 13};
  irs::order order;
  size_t collect_field_count = 0;
  size_t collect_term_count = 0;
  size_t finish_count = 0;
  std::vector<size_t> frequency;
  std::vector<irs::boost_t> filter_boost;
  auto& scorer = order.add<tests::sort::custom_sort>(false);
  scorer.collector_collect_field = [&collect_field_count](
      const irs::sub_reader&, const irs::term_reader&)->void{
    ++collect_field_count;
  };
  scorer.collector_collect_term = [&collect_term_count](
      const irs::sub_reader&,
      const irs::term_reader&,
      const irs::attribute_provider&)->void{
    ++collect_term_count;
  };
  scorer.collectors_collect_ = [&finish_count](
      irs::byte_type*,
      const irs::index_reader&,
      const irs::sort::field_collector*,
      const irs::sort::term_collector*)->void {
    ++finish_count;
  };
  scorer.prepare_field_collector_ = [&scorer]()->irs::sort::field_collector::ptr {
    return irs::memory::make_unique<tests::sort::custom_sort::prepared::field_collector>(scorer);
  };
  scorer.prepare_term_collector_ = [&scorer]()->irs::sort::term_collector::ptr {
    return irs::memory::make_unique<tests::sort::custom_sort::prepared::term_collector>(scorer);
  };
  scorer.prepare_scorer = [&frequency, &filter_boost](
    const irs::sub_reader& segment,
    const irs::term_reader& term,
    const irs::byte_type* stats_buf,
    irs::byte_type* score_buf,
    const irs::attribute_provider& attr)->irs::score_function {
      auto* freq = irs::get<irs::frequency>(attr);
      auto* boost = irs::get<irs::filter_boost>(attr);
      return {
        irs::memory::make_unique<test_score_ctx>(&frequency, freq, &filter_boost, boost, score_buf),
        [](irs::score_ctx* ctx) noexcept -> const irs::byte_type* {
          const auto& freq = *reinterpret_cast<test_score_ctx*>(ctx);
          freq.freq->push_back(freq.freq_from_filter->value);
          freq.filter_boost->push_back(freq.boost_from_filter->value);

          return freq.score_buf;
        }
      };
  };
  std::vector<size_t> expectedFrequency{1, 1, 2, 1, 1, 1, 1};
  std::vector<irs::boost_t> expected_filter_boost{4.f/6.f, 4.f/6.f, 4.f/6.f, 4.f/6.f, 0.5, 0.5, 0.5};
  check_query(filter, order, expected, rdr);
  ASSERT_EQ(expectedFrequency, frequency);
  ASSERT_EQ(expected_filter_boost.size(), filter_boost.size());
  for (size_t i = 0; i < expected_filter_boost.size(); ++i) {
    SCOPED_TRACE(testing::Message("i=") << i);
    ASSERT_DOUBLE_EQ(expected_filter_boost[i], filter_boost[i]);
  }
  ASSERT_EQ(1, collect_field_count);
  ASSERT_EQ(5, collect_term_count);
  ASSERT_EQ(collect_field_count + collect_term_count, finish_count);
}

TEST_P(ngram_similarity_filter_test_case, missed_frequency_test) {
  {
    tests::json_doc_generator gen(
      resource("ngram_similarity.json"),
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  irs::by_ngram_similarity filter = make_filter("field", {"never_match", "at", "tl", "la", "as", "ll"}, 0.5f);

  docs_t expected{ 1, 2, 5, 8, 11, 12, 13};
  irs::order order;
  size_t collect_field_count = 0;
  size_t collect_term_count = 0;
  size_t finish_count = 0;
  std::vector<size_t> frequency;
  std::vector<irs::boost_t> filter_boost;
  auto& scorer = order.add<tests::sort::custom_sort>(false);
  scorer.collector_collect_field = [&collect_field_count](
      const irs::sub_reader&, const irs::term_reader&)->void{
    ++collect_field_count;
  };
  scorer.collector_collect_term = [&collect_term_count](
      const irs::sub_reader&,
      const irs::term_reader&,
      const irs::attribute_provider&)->void{
    ++collect_term_count;
  };
  scorer.collectors_collect_ = [&finish_count](
      irs::byte_type*,
      const irs::index_reader&,
      const irs::sort::field_collector*,
      const irs::sort::term_collector*)->void {
    ++finish_count;
  };
  scorer.prepare_field_collector_ = [&scorer]()->irs::sort::field_collector::ptr {
    return irs::memory::make_unique<tests::sort::custom_sort::prepared::field_collector>(scorer);
  };
  scorer.prepare_term_collector_ = [&scorer]()->irs::sort::term_collector::ptr {
    return irs::memory::make_unique<tests::sort::custom_sort::prepared::term_collector>(scorer);
  };
  scorer.prepare_scorer = [&frequency, &filter_boost](
      const irs::sub_reader& /*segment*/,
      const irs::term_reader& /*term*/,
      const irs::byte_type* /*stats_buf*/,
      irs::byte_type* score_buf,
      const irs::attribute_provider& attr)->irs::score_function {
    auto* freq = irs::get<irs::frequency>(attr);
    auto* boost = irs::get<irs::filter_boost>(attr);
    return {
        irs::memory::make_unique<test_score_ctx>(&frequency, freq, &filter_boost, boost, score_buf),
        [](irs::score_ctx* ctx) noexcept -> const irs::byte_type* {
          const auto& freq = *reinterpret_cast<test_score_ctx*>(ctx);
          freq.freq->push_back(freq.freq_from_filter->value);
          freq.filter_boost->push_back(freq.boost_from_filter->value);

          return freq.score_buf;
        }
    };
  };
  std::vector<size_t> expected_frequency{1, 1, 2, 1, 1, 1, 1};
  std::vector<irs::boost_t> expected_filter_boost{4.f/6.f, 4.f/6.f, 4.f/6.f, 4.f/6.f, 0.5, 0.5, 0.5};
  check_query(filter, order, expected, rdr);
  ASSERT_EQ(expected_frequency, frequency);
  ASSERT_EQ(expected_filter_boost.size(), filter_boost.size());
  for (size_t i = 0; i < expected_filter_boost.size(); ++i) {
    SCOPED_TRACE(testing::Message("i=") << i);
    ASSERT_DOUBLE_EQ(expected_filter_boost[i], filter_boost[i]);
  }
  ASSERT_EQ(1, collect_field_count);
  ASSERT_EQ(5, collect_term_count);
  ASSERT_EQ(collect_field_count + collect_term_count, finish_count);
}

#ifndef IRESEARCH_DLL

TEST_P(ngram_similarity_filter_test_case, missed_first_tfidf_norm_test) {
  {
    tests::json_doc_generator gen(
      resource("ngram_similarity.json"),
      &tests::normalized_string_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  irs::by_ngram_similarity filter = make_filter("field", {"never_match", "at", "tl", "la", "as", "ll"}, 0.5f);

  docs_t expected{ 11, 12, 8, 13, 5, 1, 2};
  irs::order order;
  order.add<irs::tfidf_sort>(false).normalize(true);
  check_query(filter, order, expected, rdr);
}

TEST_P(ngram_similarity_filter_test_case, missed_first_tfidf_test) {
  {
    tests::json_doc_generator gen(
      resource("ngram_similarity.json"),
      &tests::normalized_string_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  irs::by_ngram_similarity filter = make_filter("field", {"never_match", "at", "tl", "la", "as", "ll"}, 0.5f);

  docs_t expected{ 11, 12, 13, 1, 2, 8, 5};
  irs::order order;
  order.add<irs::tfidf_sort>(false).normalize(false);

  check_query(filter, order, expected, rdr);
}

TEST_P(ngram_similarity_filter_test_case, missed_first_bm25_test) {
  {
    tests::json_doc_generator gen(
      resource("ngram_similarity.json"),
      &tests::normalized_string_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  irs::by_ngram_similarity filter = make_filter("field", {"never_match", "at", "tl", "la", "as", "ll"}, 0.5f);

  docs_t expected{13, 11, 12, 2, 1, 8, 5};
  irs::order order;
  order.add<irs::bm25_sort>(false);

  check_query(filter, order, expected, rdr);
}

TEST_P(ngram_similarity_filter_test_case, missed_first_bm15_test) {
  {
    tests::json_doc_generator gen(
      resource("ngram_similarity.json"),
      &tests::normalized_string_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  irs::by_ngram_similarity filter = make_filter("field", {"never_match", "at", "tl", "la", "as", "ll"}, 0.5f);

  docs_t expected{ 13, 11, 12, 2, 1, 8, 5};
  irs::order order;
  order.add<irs::bm25_sort>(false).b(1); // set to BM15 mode

  check_query(filter, order, expected, rdr);
}

TEST_P(ngram_similarity_filter_test_case, seek_next) {
  {
    tests::json_doc_generator gen(
      resource("ngram_similarity.json"),
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  irs::by_ngram_similarity filter = make_filter("field", {"never_match", "at", "tl", "la", "as", "ll"}, 0.5f);
  docs_t expected{1, 2, 5, 8, 11, 12, 13};
  auto expected_it = std::begin(expected);
  irs::order order;
  auto prepared_order = order.prepare();
  auto prepared_filter = filter.prepare(rdr, prepared_order);
  for (const auto& sub : rdr) {
    auto docs = prepared_filter->execute(sub, prepared_order);
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc)); // ensure all iterators contain "document" attribute
    ASSERT_EQ(irs::doc_limits::invalid(), docs->value());
    while (docs->next()) {
      ASSERT_EQ(docs->value(),*expected_it);
      ASSERT_EQ(doc->value, docs->value());
      // seek same
      ASSERT_EQ(*expected_it, docs->seek(*expected_it));
      // seek backward
      ASSERT_EQ(*expected_it, docs->seek((*expected_it) - 1));
      ++expected_it;
      if (expected_it != std::end(expected)) {
        //seek forward
        ASSERT_EQ(*expected_it, docs->seek(*expected_it));
        ++expected_it;
      }
    }
    ASSERT_EQ(irs::doc_limits::eof(), docs->value());
    ASSERT_FALSE(docs->next());
    ASSERT_EQ(irs::doc_limits::eof(), docs->value());
  }
  ASSERT_EQ(expected_it, std::end(expected));
}

TEST_P(ngram_similarity_filter_test_case, seek) {
  {
    tests::json_doc_generator gen(
      resource("ngram_similarity.json"),
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  irs::by_ngram_similarity filter = make_filter("field", { "never_match", "at", "tl", "la", "as", "ll" }, 0.5f);
  docs_t seek_tagrets{ 2, 5, 8, 13 };
  auto seek_it = std::begin(seek_tagrets);
  irs::order order;
  auto prepared_order = order.prepare();
  auto prepared_filter = filter.prepare(rdr, prepared_order);
  for (const auto& sub : rdr) {
    while (std::end(seek_tagrets) != seek_it) {
      auto docs = prepared_filter->execute(sub, prepared_order);
      auto* doc = irs::get<irs::document>(*docs);
      ASSERT_TRUE(bool(doc)); // ensure all iterators contain "document" attribute
      ASSERT_EQ(irs::doc_limits::invalid(), docs->value());
      ASSERT_EQ(doc->value, docs->value());
      auto actual_seeked = docs->seek(*seek_it);
      ASSERT_EQ(doc->value, docs->value());
      if (actual_seeked == *seek_it) {
        ASSERT_EQ(docs->seek(*seek_it), *seek_it);
        ASSERT_EQ(docs->seek((*seek_it) - 1), *seek_it);
        ASSERT_EQ(doc->value, docs->value());
        ++seek_it;
      }
      if (actual_seeked == irs::doc_limits::eof()) {
        // go try next subreader
        break;
      }
    }
  }
  ASSERT_EQ(std::end(seek_tagrets), seek_it);
}

#endif

INSTANTIATE_TEST_CASE_P(
  ngram_similarity_test,
  ngram_similarity_filter_test_case,
  ::testing::Combine(
    ::testing::Values(
      &tests::memory_directory,
      &tests::fs_directory,
      &tests::mmap_directory),
    ::testing::Values(tests::format_info{"1_0"},
                      tests::format_info{"1_3", "1_0"})),
    tests::to_string);

} // tests
