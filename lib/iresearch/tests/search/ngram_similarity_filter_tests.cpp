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

#include <functional>

#include "filter_test_case_base.hpp"
#include "index/norm.hpp"
#include "search/bm25.hpp"
#include "search/ngram_similarity_filter.hpp"
#include "search/ngram_similarity_query.hpp"
#include "search/tfidf.hpp"
#include "tests_shared.hpp"
#include "utils/ngram_match_utils.hpp"

namespace tests {
namespace {

irs::by_ngram_similarity make_filter(
  const std::string_view& field, const std::vector<std::string_view>& ngrams,
  float_t threshold = 1.f, bool allow_phrase = true) {
  irs::by_ngram_similarity filter;
  *filter.mutable_field() = field;
  auto* opts = filter.mutable_options();
  for (auto& ngram : ngrams) {
    opts->ngrams.emplace_back(irs::ViewCast<irs::byte_type>(ngram));
  }
  opts->threshold = threshold;
  opts->allow_phrase = allow_phrase;
  return filter;
}

class CustomNgramScorer : public sort::custom_sort {
 public:
  using custom_sort::custom_sort;

  irs::IndexFeatures index_features() const final {
    return irs::IndexFeatures::FREQ;
  }
};

irs::score_t GetFilterBoost(const irs::doc_iterator::ptr& doc) {
  const auto* filter_boost = irs::get<irs::filter_boost>(*doc);
  return filter_boost != nullptr ? filter_boost->value : 1.F;
}

}  // namespace

TEST(ngram_similarity_base_test, options) {
  irs::by_ngram_similarity_options opts;
  ASSERT_TRUE(opts.ngrams.empty());
  ASSERT_EQ(1.f, opts.threshold);
}

TEST(ngram_similarity_base_test, ctor) {
  irs::by_ngram_similarity q;
  ASSERT_EQ(irs::type<irs::by_ngram_similarity>::id(), q.type());
  ASSERT_EQ(irs::by_ngram_similarity_options{}, q.options());
  ASSERT_EQ(irs::kNoBoost, q.boost());
  ASSERT_EQ("", q.field());

  static_assert((irs::IndexFeatures::FREQ | irs::IndexFeatures::POS) ==
                irs::NGramSimilarityQuery::kRequiredFeatures);
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

    // different field
    irs::by_ngram_similarity q6 = make_filter("b", {"1", "2"}, 0.5f);
    ASSERT_NE(q0, q6);
  }
}

// ----------------------------------------------------------------------------
// --SECTION--                         by_ngram_similarity filter matcher tests
// ----------------------------------------------------------------------------

class ngram_similarity_filter_test_case : public tests::FilterTestCaseBase {
 protected:
  static irs::FeatureInfoProvider features_with_norms() {
    return [](irs::type_info::type_id id) {
      const irs::ColumnInfo info{
        irs::type<irs::compression::lz4>::get(), {}, false};

      if (irs::type<irs::Norm>::id() == id) {
        return std::make_pair(info, &irs::Norm::MakeWriter);
      }

      return std::make_pair(info, irs::FeatureWriterFactory{});
    };
  }
};

TEST_P(ngram_similarity_filter_test_case, boost) {
  // no boost
  {
    tests::json_doc_generator gen(
      R"([{ "field": [ "1", "3", "4", "5", "6", "7", "2"] }])",
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();
  ASSERT_EQ(1, rdr.size());
  auto& segment = rdr[0];

  {
    MaxMemoryCounter counter;

    // no terms no field
    {
      irs::by_ngram_similarity q;

      auto prepared = q.prepare({
        .index = segment,
        .memory = counter,
      });
      ASSERT_EQ(irs::kNoBoost, prepared->boost());
    }
    EXPECT_EQ(counter.current, 0);
    EXPECT_EQ(counter.max, 0);
    counter.Reset();

    // simple disjunction
    {
      irs::by_ngram_similarity q = make_filter("field", {"1", "2"}, 0.5f);

      auto prepared = q.prepare({
        .index = segment,
        .memory = counter,
      });
      ASSERT_EQ(irs::kNoBoost, prepared->boost());
    }
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // multiple terms
    {
      irs::by_ngram_similarity q =
        make_filter("field", {"1", "2", "3", "4"}, 0.5f);

      auto prepared = q.prepare({
        .index = segment,
        .memory = counter,
      });
      ASSERT_EQ(irs::kNoBoost, prepared->boost());
    }
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();
  }

  // with boost
  {
    MaxMemoryCounter counter;

    irs::score_t boost = 1.5f;

    // no terms, return empty query
    {
      irs::by_ngram_similarity q;
      q.boost(boost);

      auto prepared = q.prepare({
        .index = segment,
        .memory = counter,
      });
      ASSERT_EQ(irs::kNoBoost, prepared->boost());
    }
    EXPECT_EQ(counter.current, 0);
    EXPECT_EQ(counter.max, 0);
    counter.Reset();

    // simple disjunction
    {
      irs::by_ngram_similarity q = make_filter("field", {"1", "2"}, 0.5f);
      q.boost(boost);

      auto prepared = q.prepare({
        .index = segment,
        .memory = counter,
      });
      ASSERT_EQ(boost, prepared->boost());
    }
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // multiple terms
    {
      irs::by_ngram_similarity q =
        make_filter("field", {"1", "2", "3", "4"}, 0.5f);
      q.boost(boost);

      auto prepared = q.prepare({
        .index = segment,
        .memory = counter,
      });
      ASSERT_EQ(boost, prepared->boost());
    }
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();
  }
}

TEST_P(ngram_similarity_filter_test_case, check_matcher_1) {
  // sequence 1 3 4 ______ 2 -> longest is 134 not 12
  // add segment
  {
    tests::json_doc_generator gen(
      "[{ \"seq\" : 1, \"field\": [ \"1\", \"3\", \"4\", \"5\", \"6\", "
      "\"7\", \"2\"] }]",
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  MaxMemoryCounter counter;

  irs::by_ngram_similarity filter =
    make_filter("field", {"1", "2", "3", "4"}, 0.5f);

  CustomNgramScorer sort;
  auto prepared_order = irs::Scorers::Prepare(sort);
  auto prepared = filter.prepare({
    .index = rdr,
    .memory = counter,
    .scorers = prepared_order,
  });
  for (const auto& sub : rdr) {
    auto docs = prepared->execute({
      .segment = sub,
      .scorers = prepared_order,
    });
    auto* doc = irs::get<irs::document>(*docs);
    auto* frequency = irs::get<irs::frequency>(*docs);
    ASSERT_TRUE(
      bool(doc));  // ensure all iterators contain "document" attribute
    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::eof(doc->value));
    ASSERT_DOUBLE_EQ(0.75, GetFilterBoost(docs));
    const std::string_view rhs = "134";
    const std::string_view lhs = "1234";
    ASSERT_DOUBLE_EQ(GetFilterBoost(docs),
                     (irs::ngram_similarity<char, true>(
                       lhs.data(), lhs.size(), rhs.data(), rhs.size(), 1)));
    ASSERT_EQ(1, frequency->value);
    ASSERT_FALSE(docs->next());
  }
  prepared.reset();
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
}

TEST_P(ngram_similarity_filter_test_case, check_matcher_2) {
  // sequence 1 1 2 2 3 3 4 4 -> longest is 1234  and freq should be 1 not 2 as
  // this sequence could not be built twice one after another but only
  // intereaved
  {
    tests::json_doc_generator gen(
      "[{ \"seq\" : 1, \"field\": [ \"1\", \"1\", \"2\", \"2\", \"3\", "
      "\"3\", \"4\", \"4\"] }]",
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  MaxMemoryCounter counter;

  irs::by_ngram_similarity filter =
    make_filter("field", {"1", "2", "3", "4"}, 0.5f);

  CustomNgramScorer sort;
  auto prepared_order = irs::Scorers::Prepare(sort);
  auto prepared = filter.prepare({
    .index = rdr,
    .memory = counter,
    .scorers = prepared_order,
  });
  for (const auto& sub : rdr) {
    auto docs = prepared->execute({
      .segment = sub,
      .scorers = prepared_order,
    });
    auto* doc = irs::get<irs::document>(*docs);
    auto* frequency = irs::get<irs::frequency>(*docs);
    // ensure all iterators contain  attributes
    ASSERT_TRUE(bool(doc));
    ASSERT_TRUE(bool(frequency));
    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::eof(doc->value));
    ASSERT_DOUBLE_EQ(1, GetFilterBoost(docs));
    const std::string_view rhs = "11223344";
    const std::string_view lhs = "1234";
    ASSERT_DOUBLE_EQ(GetFilterBoost(docs),
                     (irs::ngram_similarity<char, true>(
                       lhs.data(), lhs.size(), rhs.data(), rhs.size(), 1)));
    ASSERT_EQ(1, frequency->value);
    ASSERT_FALSE(docs->next());
  }
  prepared.reset();
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
}

TEST_P(ngram_similarity_filter_test_case, check_matcher_3) {
  // sequence 1 2 1 1 3 4 -> longest is 1234  not 134!
  {
    tests::json_doc_generator gen(
      "[{ \"seq\" : 1, \"field\": [ \"1\", \"2\", \"1\", \"1\", \"3\", "
      "\"4\"] }]",
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  MaxMemoryCounter counter;

  irs::by_ngram_similarity filter =
    make_filter("field", {"1", "2", "3", "4"}, 0.5f);

  CustomNgramScorer sort;
  auto prepared_order = irs::Scorers::Prepare(sort);
  auto prepared = filter.prepare({
    .index = rdr,
    .memory = counter,
    .scorers = prepared_order,
  });
  for (const auto& sub : rdr) {
    auto docs = prepared->execute({
      .segment = sub,
      .scorers = prepared_order,
    });
    auto* doc = irs::get<irs::document>(*docs);
    auto* frequency = irs::get<irs::frequency>(*docs);
    // ensure all iterators contain  attributes
    ASSERT_TRUE(bool(doc));
    ASSERT_TRUE(bool(frequency));
    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::eof(doc->value));
    ASSERT_DOUBLE_EQ(1, GetFilterBoost(docs));
    const std::string_view rhs = "121134";
    const std::string_view lhs = "1234";
    ASSERT_DOUBLE_EQ(GetFilterBoost(docs),
                     (irs::ngram_similarity<char, true>(
                       lhs.data(), lhs.size(), rhs.data(), rhs.size(), 1)));
    ASSERT_EQ(1, frequency->value);
    ASSERT_FALSE(docs->next());
  }
  prepared.reset();
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
}

TEST_P(ngram_similarity_filter_test_case, check_matcher_4) {
  // sequence 1 2 1 1 1 1 pattern 1 1 -> longest is 1 1 and frequency is 2
  {
    tests::json_doc_generator gen(
      "[{ \"seq\" : 1, \"field\": [ \"1\", \"2\", \"1\", \"1\", \"1\", "
      "\"1\"] }]",
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  MaxMemoryCounter counter;

  irs::by_ngram_similarity filter = make_filter("field", {"1", "1"}, 0.5f);

  CustomNgramScorer sort;
  auto prepared_order = irs::Scorers::Prepare(sort);
  auto prepared = filter.prepare({
    .index = rdr,
    .memory = counter,
    .scorers = prepared_order,
  });
  for (const auto& sub : rdr) {
    auto docs = prepared->execute({
      .segment = sub,
      .scorers = prepared_order,
    });
    auto* doc = irs::get<irs::document>(*docs);
    auto* frequency = irs::get<irs::frequency>(*docs);
    // ensure all iterators contain  attributes
    ASSERT_TRUE(bool(doc));
    ASSERT_TRUE(bool(frequency));
    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::eof(doc->value));
    ASSERT_DOUBLE_EQ(1, GetFilterBoost(docs));
    const std::string_view rhs = "121111";
    const std::string_view lhs = "11";
    ASSERT_DOUBLE_EQ(GetFilterBoost(docs),
                     (irs::ngram_similarity<char, true>(
                       lhs.data(), lhs.size(), rhs.data(), rhs.size(), 1)));
    ASSERT_EQ(2, frequency->value);
    ASSERT_FALSE(docs->next());
  }
  prepared.reset();
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
}

TEST_P(ngram_similarity_filter_test_case, check_matcher_5) {
  // sequence 1 2 1 2 1 2 1 2 1 2 1 2 1 2 1 pattern 1 2 1 -> longest is 1 2 1
  // and frequency is 4
  {
    tests::json_doc_generator gen(
      "[{ \"seq\" : 1, \"field\": [ \"1\", \"2\", \"1\", \"2\", \"1\", "
      " \"2\", \"1\", \"2\", \"1\", \"2\", \"1\", \"2\", \"1\", \"2\", "
      "\"1\"] }]",
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  MaxMemoryCounter counter;

  irs::by_ngram_similarity filter = make_filter("field", {"1", "2", "1"}, 0.5f);

  CustomNgramScorer sort;
  auto prepared_order = irs::Scorers::Prepare(sort);
  auto prepared = filter.prepare({
    .index = rdr,
    .memory = counter,
    .scorers = prepared_order,
  });
  for (const auto& sub : rdr) {
    auto docs = prepared->execute({
      .segment = sub,
      .scorers = prepared_order,
    });
    auto* doc = irs::get<irs::document>(*docs);
    auto* frequency = irs::get<irs::frequency>(*docs);
    // ensure all iterators contain  attributes
    ASSERT_TRUE(bool(doc));
    ASSERT_TRUE(bool(frequency));
    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::eof(doc->value));
    ASSERT_DOUBLE_EQ(1, GetFilterBoost(docs));
    const std::string_view rhs = "121212121212121";
    const std::string_view lhs = "121";
    ASSERT_DOUBLE_EQ(GetFilterBoost(docs),
                     (irs::ngram_similarity<char, true>(
                       lhs.data(), lhs.size(), rhs.data(), rhs.size(), 1)));
    ASSERT_EQ(4, frequency->value);
    ASSERT_FALSE(docs->next());
  }
  prepared.reset();
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
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

  MaxMemoryCounter counter;

  irs::by_ngram_similarity filter = make_filter("field", {"1", "1"}, 1.0f);

  CustomNgramScorer sort;
  auto prepared_order = irs::Scorers::Prepare(sort);
  auto prepared = filter.prepare({
    .index = rdr,
    .memory = counter,
    .scorers = prepared_order,
  });
  for (const auto& sub : rdr) {
    auto docs = prepared->execute({
      .segment = sub,
      .scorers = prepared_order,
    });
    auto* doc = irs::get<irs::document>(*docs);
    auto* frequency = irs::get<irs::frequency>(*docs);
    // ensure all iterators contain  attributes
    ASSERT_TRUE(bool(doc));
    ASSERT_TRUE(bool(frequency));
    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::eof(doc->value));
    ASSERT_DOUBLE_EQ(1, GetFilterBoost(docs));
    const std::string_view rhs = "11";
    const std::string_view lhs = "11";
    ASSERT_DOUBLE_EQ(GetFilterBoost(docs),
                     (irs::ngram_similarity<char, true>(
                       lhs.data(), lhs.size(), rhs.data(), rhs.size(), 1)));
    ASSERT_EQ(1, frequency->value);
    ASSERT_FALSE(docs->next());
  }
  prepared.reset();
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
}

TEST_P(ngram_similarity_filter_test_case, check_matcher_7) {
  // sequence 2 4 2 4 1 3 1 3 pattern 1 2 3 4-> longest is 1 3  and 2 4 but
  // frequency is 2 as only first longest counted
  {
    tests::json_doc_generator gen(
      "[{ \"seq\" : 1, \"field\": [ \"2\", \"4\", \"2\", \"4\", \"1\", "
      "\"3\", \"1\", \"3\"] }]",
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  MaxMemoryCounter counter;

  irs::by_ngram_similarity filter =
    make_filter("field", {"1", "2", "3", "4"}, 0.5f);

  CustomNgramScorer sort;
  auto prepared_order = irs::Scorers::Prepare(sort);
  auto prepared = filter.prepare({
    .index = rdr,
    .memory = counter,
    .scorers = prepared_order,
  });
  for (const auto& sub : rdr) {
    auto docs = prepared->execute({
      .segment = sub,
      .scorers = prepared_order,
    });
    auto* doc = irs::get<irs::document>(*docs);
    auto* frequency = irs::get<irs::frequency>(*docs);
    // ensure all iterators contain  attributes
    ASSERT_TRUE(bool(doc));
    ASSERT_TRUE(bool(frequency));
    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::eof(doc->value));
    ASSERT_DOUBLE_EQ(0.5, GetFilterBoost(docs));
    const std::string_view rhs = "24241313";
    const std::string_view lhs = "1234";
    ASSERT_DOUBLE_EQ(GetFilterBoost(docs),
                     (irs::ngram_similarity<char, true>(
                       lhs.data(), lhs.size(), rhs.data(), rhs.size(), 1)));
    ASSERT_EQ(2, frequency->value);
    ASSERT_FALSE(docs->next());
  }
  prepared.reset();
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
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

  MaxMemoryCounter counter;

  irs::by_ngram_similarity filter =
    make_filter("field", {"1", "5", "6", "2"}, 0.5f);

  CustomNgramScorer sort;
  auto prepared_order = irs::Scorers::Prepare(sort);
  auto prepared = filter.prepare({
    .index = rdr,
    .memory = counter,
    .scorers = prepared_order,
  });
  for (const auto& sub : rdr) {
    auto docs = prepared->execute({
      .segment = sub,
      .scorers = prepared_order,
    });
    auto* doc = irs::get<irs::document>(*docs);
    auto* frequency = irs::get<irs::frequency>(*docs);
    // ensure all iterators contain  attributes
    ASSERT_TRUE(bool(doc));
    ASSERT_TRUE(bool(frequency));
    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::eof(doc->value));
    ASSERT_DOUBLE_EQ(0.5, GetFilterBoost(docs));
    const std::string_view lhs = "1234";
    const std::string_view rhs = "1562";
    ASSERT_DOUBLE_EQ(GetFilterBoost(docs),
                     (irs::ngram_similarity<char, true>(
                       lhs.data(), lhs.size(), rhs.data(), rhs.size(), 1)));
    ASSERT_EQ(1, frequency->value);
    ASSERT_FALSE(docs->next());
  }
  prepared.reset();
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
}

TEST_P(ngram_similarity_filter_test_case, check_matcher_9) {
  // bulk pos read check (for future optimization)
  // sequence 1 1 2 3 4 5 1  pattern 1 2 3 4 5 1 -> longest is 1 2 3 4 5 1   and
  // boost 1 and frequency 1
  {
    tests::json_doc_generator gen(
      "[{ \"seq\" : 1, \"field\": [ \"1\", \"1\", \"2\", \"3\", \"4\", "
      "\"5\", \"1\"] }]",
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  MaxMemoryCounter counter;

  irs::by_ngram_similarity filter =
    make_filter("field", {"1", "2", "3", "4", "5", "1"}, 0.5f);

  CustomNgramScorer sort;
  auto prepared_order = irs::Scorers::Prepare(sort);
  auto prepared = filter.prepare({
    .index = rdr,
    .memory = counter,
    .scorers = prepared_order,
  });
  for (const auto& sub : rdr) {
    auto docs = prepared->execute({
      .segment = sub,
      .scorers = prepared_order,
    });
    auto* doc = irs::get<irs::document>(*docs);
    auto* frequency = irs::get<irs::frequency>(*docs);
    // ensure all iterators contain  attributes
    ASSERT_TRUE(bool(doc));
    ASSERT_TRUE(bool(frequency));
    ASSERT_TRUE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_FALSE(irs::doc_limits::eof(doc->value));
    ASSERT_DOUBLE_EQ(1., GetFilterBoost(docs));
    const std::string_view rhs = "1123451";
    const std::string_view lhs = "123451";
    ASSERT_DOUBLE_EQ(GetFilterBoost(docs),
                     (irs::ngram_similarity<char, true>(
                       lhs.data(), lhs.size(), rhs.data(), rhs.size(), 1)));
    ASSERT_EQ(1, frequency->value);
    ASSERT_FALSE(docs->next());
  }
  prepared.reset();
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
}

TEST_P(ngram_similarity_filter_test_case, check_matcher_10) {
  // bulk pos read check (for future optimization)
  // sequence '' pattern '' -> longest is ''   and  boost 1 and frequency 1
  {
    tests::json_doc_generator gen("[{ \"seq\" : 1, \"field\": [ \"\"] }]",
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  MaxMemoryCounter counter;

  irs::by_ngram_similarity filter = make_filter("field", {""}, 0.5f);

  CustomNgramScorer sort;
  auto prepared_order = irs::Scorers::Prepare(sort);
  auto prepared = filter.prepare({
    .index = rdr,
    .memory = counter,
    .scorers = prepared_order,
  });
  for (const auto& sub : rdr) {
    auto docs = prepared->execute({
      .segment = sub,
      .scorers = prepared_order,
    });
    auto* doc = irs::get<irs::document>(*docs);
    auto* frequency = irs::get<irs::frequency>(*docs);
    // ensure all iterators contain  attributes
    EXPECT_TRUE(bool(doc));
    EXPECT_TRUE(bool(frequency));
    EXPECT_TRUE(docs->next());
    EXPECT_EQ(docs->value(), doc->value);
    EXPECT_FALSE(irs::doc_limits::eof(doc->value));
    EXPECT_DOUBLE_EQ(1., GetFilterBoost(docs));
    const std::string_view rhs = "";
    const std::string_view lhs = "";
    EXPECT_DOUBLE_EQ(GetFilterBoost(docs),
                     (irs::ngram_similarity<char, true>(
                       lhs.data(), lhs.size(), rhs.data(), rhs.size(), 1)));
    EXPECT_EQ(1, frequency->value);
    EXPECT_FALSE(docs->next());
  }
  prepared.reset();
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
}

TEST_P(ngram_similarity_filter_test_case, no_match_case) {
  {
    tests::json_doc_generator gen(resource("ngram_similarity.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  MaxMemoryCounter counter;

  irs::by_ngram_similarity filter =
    make_filter("field", {"ee", "we", "qq", "rr", "ff", "never_match"}, 0.1f);

  auto prepared = filter.prepare({
    .index = rdr,
    .memory = counter,
  });
  for (const auto& sub : rdr) {
    auto docs = prepared->execute({.segment = sub});

    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(
      bool(doc));  // ensure all iterators contain "document" attribute
    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(doc->value));
  }
  prepared.reset();
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
}

TEST_P(ngram_similarity_filter_test_case, no_serial_match_case) {
  {
    tests::json_doc_generator gen(resource("ngram_similarity.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  MaxMemoryCounter counter;

  irs::by_ngram_similarity filter =
    make_filter("field", {"ee", "ss", "pa", "rr"}, 0.5f);

  auto prepared = filter.prepare({
    .index = rdr,
    .memory = counter,
  });
  for (const auto& sub : rdr) {
    auto docs = prepared->execute({.segment = sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(
      bool(doc));  // ensure all iterators contain "document" attribute
    ASSERT_FALSE(docs->next());
    ASSERT_EQ(docs->value(), doc->value);
    ASSERT_TRUE(irs::doc_limits::eof(doc->value));
  }
  prepared.reset();
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
}

TEST_P(ngram_similarity_filter_test_case, one_match_case) {
  {
    tests::json_doc_generator gen(resource("ngram_similarity.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  MaxMemoryCounter counter;

  irs::by_ngram_similarity filter =
    make_filter("field", {"ee", "ss", "qq", "rr", "ff", "never_match"}, 0.1f);

  Docs expected{1, 3, 5, 6, 7, 8, 9, 10, 12};
  const size_t expected_size = expected.size();
  auto prepared = filter.prepare({
    .index = rdr,
    .memory = counter,
  });
  size_t count = 0;
  for (const auto& sub : rdr) {
    auto docs = prepared->execute({.segment = sub});

    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(
      bool(doc));  // ensure all iterators contain "document" attribute
    while (docs->next()) {
      ASSERT_EQ(docs->value(), doc->value);
      expected.erase(
        std::remove(expected.begin(), expected.end(), docs->value()),
        expected.end());
      ++count;
    }
  }
  ASSERT_EQ(expected_size, count);
  ASSERT_EQ(0, expected.size());
  prepared.reset();
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
}

TEST_P(ngram_similarity_filter_test_case, missed_last_test) {
  {
    tests::json_doc_generator gen(resource("ngram_similarity.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  MaxMemoryCounter counter;

  irs::by_ngram_similarity filter =
    make_filter("field", {"at", "tl", "la", "as", "ll", "never_match"}, 0.5f);

  Docs expected{1, 2, 5, 8, 11, 12, 13};
  const size_t expected_size = expected.size();
  auto prepared = filter.prepare({
    .index = rdr,
    .memory = counter,
  });
  size_t count = 0;
  for (const auto& sub : rdr) {
    auto docs = prepared->execute({.segment = sub});

    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(
      bool(doc));  // ensure all iterators contain "document" attribute
    while (docs->next()) {
      ASSERT_EQ(docs->value(), doc->value);
      expected.erase(
        std::remove(expected.begin(), expected.end(), docs->value()),
        expected.end());
      ++count;
    }
  }
  ASSERT_EQ(expected_size, count);
  ASSERT_EQ(0, expected.size());
  prepared.reset();
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
}

TEST_P(ngram_similarity_filter_test_case, missed_first_test) {
  {
    tests::json_doc_generator gen(resource("ngram_similarity.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  MaxMemoryCounter counter;

  irs::by_ngram_similarity filter =
    make_filter("field", {"never_match", "at", "tl", "la", "as", "ll"}, 0.5f);

  Docs expected{1, 2, 5, 8, 11, 12, 13};
  const size_t expected_size = expected.size();
  auto prepared = filter.prepare({
    .index = rdr,
    .memory = counter,
  });
  size_t count = 0;
  for (const auto& sub : rdr) {
    auto docs = prepared->execute({.segment = sub});

    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(
      bool(doc));  // ensure all iterators contain "document" attribute
    while (docs->next()) {
      ASSERT_EQ(docs->value(), doc->value);
      expected.erase(
        std::remove(expected.begin(), expected.end(), docs->value()),
        expected.end());
      ++count;
    }
  }
  ASSERT_EQ(expected_size, count);
  ASSERT_EQ(0, expected.size());
  prepared.reset();
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
}

TEST_P(ngram_similarity_filter_test_case, not_miss_match_for_tail) {
  {
    tests::json_doc_generator gen(resource("ngram_similarity.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  MaxMemoryCounter counter;

  irs::by_ngram_similarity filter =
    make_filter("field", {"at", "tl", "la", "as", "ll", "never_match"}, 0.33f);

  Docs expected{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
  const size_t expected_size = expected.size();
  auto prepared = filter.prepare({
    .index = rdr,
    .memory = counter,
  });
  size_t count = 0;
  for (const auto& sub : rdr) {
    auto docs = prepared->execute({.segment = sub});

    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(
      bool(doc));  // ensure all iterators contain "document" attribute
    while (docs->next()) {
      ASSERT_EQ(docs->value(), doc->value);
      expected.erase(
        std::remove(expected.begin(), expected.end(), docs->value()),
        expected.end());
      ++count;
    }
  }
  ASSERT_EQ(expected_size, count);
  ASSERT_EQ(0, expected.size());
  prepared.reset();
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
}

TEST_P(ngram_similarity_filter_test_case, missed_middle_test) {
  {
    tests::json_doc_generator gen(resource("ngram_similarity.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  MaxMemoryCounter counter;

  irs::by_ngram_similarity filter =
    make_filter("field", {"at", "never_match", "la", "as", "ll"}, 0.333f);

  Docs expected{1, 2, 3, 4, 5, 6, 7, 8, 11, 12, 13, 14};
  const size_t expected_size = expected.size();

  auto prepared = filter.prepare({
    .index = rdr,
    .memory = counter,
  });
  size_t count = 0;
  for (const auto& sub : rdr) {
    auto docs = prepared->execute({.segment = sub});

    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(
      bool(doc));  // ensure all iterators contain "document" attribute
    while (docs->next()) {
      ASSERT_EQ(docs->value(), doc->value);
      expected.erase(
        std::remove(expected.begin(), expected.end(), docs->value()),
        expected.end());
      ++count;
    }
  }
  ASSERT_EQ(expected_size, count);
  ASSERT_EQ(0, expected.size());
  prepared.reset();
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
}

TEST_P(ngram_similarity_filter_test_case, missed_middle2_test) {
  {
    tests::json_doc_generator gen(resource("ngram_similarity.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  MaxMemoryCounter counter;

  irs::by_ngram_similarity filter = make_filter(
    "field", {"at", "never_match", "never_match2", "la", "as", "ll"}, 0.5f);

  Docs expected{1, 2, 5, 8, 11, 12, 13};
  const size_t expected_size = expected.size();

  auto prepared = filter.prepare({
    .index = rdr,
    .memory = counter,
  });
  size_t count = 0;
  for (const auto& sub : rdr) {
    auto docs = prepared->execute({.segment = sub});

    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(
      bool(doc));  // ensure all iterators contain "document" attribute
    while (docs->next()) {
      ASSERT_EQ(docs->value(), doc->value);
      expected.erase(
        std::remove(expected.begin(), expected.end(), docs->value()),
        expected.end());
      ++count;
    }
  }
  ASSERT_EQ(expected_size, count);
  ASSERT_EQ(0, expected.size());
  prepared.reset();
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
}

TEST_P(ngram_similarity_filter_test_case, missed_middle3_test) {
  {
    tests::json_doc_generator gen(resource("ngram_similarity.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  MaxMemoryCounter counter;

  irs::by_ngram_similarity filter = make_filter(
    "field", {"at", "never_match", "tl", "never_match2", "la", "as", "ll"},
    0.28f);

  Docs expected{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14};
  const size_t expected_size = expected.size();

  auto prepared = filter.prepare({
    .index = rdr,
    .memory = counter,
  });
  size_t count = 0;
  for (const auto& sub : rdr) {
    auto docs = prepared->execute({.segment = sub});

    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(
      bool(doc));  // ensure all iterators contain "document" attribute
    while (docs->next()) {
      ASSERT_EQ(docs->value(), doc->value);
      expected.erase(
        std::remove(expected.begin(), expected.end(), docs->value()),
        expected.end());
      ++count;
    }
  }
  ASSERT_EQ(expected_size, count);
  ASSERT_EQ(0, expected.size());
  prepared.reset();
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
}

struct test_score_ctx final : public irs::score_ctx {
  test_score_ctx(std::vector<size_t>* f, const irs::frequency* p,
                 std::vector<irs::score_t>* b,
                 const irs::filter_boost* fb) noexcept
    : freq(f), filter_boost(b), freq_from_filter(p), boost_from_filter(fb) {}

  std::vector<size_t>* freq;
  std::vector<irs::score_t>* filter_boost;
  const irs::frequency* freq_from_filter;
  const irs::filter_boost* boost_from_filter;
};

TEST_P(ngram_similarity_filter_test_case, missed_last_scored_test) {
  {
    tests::json_doc_generator gen(resource("ngram_similarity.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  irs::by_ngram_similarity filter =
    make_filter("field", {"at", "tl", "la", "as", "ll", "never_match"}, 0.5f);

  Docs expected{1, 2, 5, 8, 11, 12, 13};
  size_t collect_field_count = 0;
  size_t collect_term_count = 0;
  size_t finish_count = 0;
  std::vector<size_t> frequency;
  std::vector<irs::score_t> filter_boost;

  irs::Scorer::ptr order{std::make_unique<CustomNgramScorer>()};
  auto& scorer = static_cast<CustomNgramScorer&>(*order);

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
    return std::make_unique<CustomNgramScorer::field_collector>(scorer);
  };
  scorer.prepare_term_collector_ = [&scorer]() -> irs::TermCollector::ptr {
    return std::make_unique<CustomNgramScorer::term_collector>(scorer);
  };
  scorer.prepare_scorer_ =
    [&frequency, &filter_boost](
      const irs::ColumnProvider& /*segment*/,
      const irs::feature_map_t& /*term*/, const irs::byte_type* /*stats_buf*/,
      const irs::attribute_provider& attr, irs::score_t) -> irs::ScoreFunction {
    auto* freq = irs::get<irs::frequency>(attr);
    auto* boost = irs::get<irs::filter_boost>(attr);
    return irs::ScoreFunction::Make<test_score_ctx>(
      [](irs::score_ctx* ctx, irs::score_t* res) noexcept {
        const auto& freq = *reinterpret_cast<test_score_ctx*>(ctx);
        freq.freq->push_back(freq.freq_from_filter->value);
        freq.filter_boost->push_back(freq.boost_from_filter->value);
        *res = {};
      },
      irs::ScoreFunction::DefaultMin, &frequency, freq, &filter_boost, boost);
  };
  std::vector<size_t> expectedFrequency{1, 1, 2, 1, 1, 1, 1};
  std::vector<irs::score_t> expected_filter_boost{
    4.f / 6.f, 4.f / 6.f, 4.f / 6.f, 4.f / 6.f, 0.5, 0.5, 0.5};
  CheckQuery(filter, std::span{&order, 1}, expected, rdr);
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
    tests::json_doc_generator gen(resource("ngram_similarity.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  irs::by_ngram_similarity filter =
    make_filter("field", {"never_match", "at", "tl", "la", "as", "ll"}, 0.5f);

  Docs expected{1, 2, 5, 8, 11, 12, 13};
  size_t collect_field_count = 0;
  size_t collect_term_count = 0;
  size_t finish_count = 0;
  std::vector<size_t> frequency;
  std::vector<irs::score_t> filter_boost;

  irs::Scorer::ptr order{std::make_unique<CustomNgramScorer>()};
  auto& scorer = static_cast<CustomNgramScorer&>(*order);

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
    return std::make_unique<CustomNgramScorer::field_collector>(scorer);
  };
  scorer.prepare_term_collector_ = [&scorer]() -> irs::TermCollector::ptr {
    return std::make_unique<CustomNgramScorer::term_collector>(scorer);
  };
  scorer.prepare_scorer_ =
    [&frequency, &filter_boost](
      const irs::ColumnProvider& /*segment*/,
      const irs::feature_map_t& /*term*/, const irs::byte_type* /*stats_buf*/,
      const irs::attribute_provider& attr, irs::score_t) -> irs::ScoreFunction {
    auto* freq = irs::get<irs::frequency>(attr);
    auto* boost = irs::get<irs::filter_boost>(attr);
    return irs::ScoreFunction::Make<test_score_ctx>(
      [](irs::score_ctx* ctx, irs::score_t* res) noexcept {
        const auto& freq = *static_cast<test_score_ctx*>(ctx);
        freq.freq->push_back(freq.freq_from_filter->value);
        freq.filter_boost->push_back(freq.boost_from_filter->value);
        *res = {};
      },
      irs::ScoreFunction::DefaultMin, &frequency, freq, &filter_boost, boost);
  };
  std::vector<size_t> expected_frequency{1, 1, 2, 1, 1, 1, 1};
  std::vector<irs::score_t> expected_filter_boost{
    4.f / 6.f, 4.f / 6.f, 4.f / 6.f, 4.f / 6.f, 0.5, 0.5, 0.5};
  CheckQuery(filter, std::span{&order, 1}, expected, rdr);
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

TEST_P(ngram_similarity_filter_test_case, missed_first_tfidf_norm_test) {
  {
    irs::IndexWriterOptions opts;
    opts.features = features_with_norms();

    tests::json_doc_generator gen(resource("ngram_similarity.json"),
                                  &tests::normalized_string_json_field_factory);

    add_segment(gen, irs::OM_CREATE, opts);
  }

  auto rdr = open_reader();

  irs::by_ngram_similarity filter =
    make_filter("field", {"never_match", "at", "tl", "la", "as", "ll"}, 0.5f);

  Docs expected{11, 12, 8, 13, 5, 1, 2};

  irs::Scorer::ptr scorer{std::make_unique<irs::TFIDF>(true)};

  CheckQuery(filter, std::span{&scorer, 1}, expected, rdr);
}

TEST_P(ngram_similarity_filter_test_case, all_match_ngram_score_test) {
  {
    irs::IndexWriterOptions opts;
    opts.features = features_with_norms();

    tests::json_doc_generator gen(resource("ngram_similarity.json"),
                                  &tests::normalized_string_json_field_factory);

    add_segment(gen, irs::OM_CREATE, opts);
  }

  auto rdr = open_reader();

  irs::Scorer::ptr scorers[] = {
    std::make_unique<irs::TFIDF>(false),
    std::make_unique<irs::TFIDF>(true),
    std::make_unique<irs::BM25>(),
    std::make_unique<irs::BM25>(0.F),                  // BM1
    std::make_unique<irs::BM25>(irs::BM25::K(), 1.F),  // BM11
    std::make_unique<irs::BM25>(irs::BM25::K(), 0.F),  // BM15
  };

  std::vector<irs::doc_id_t> ngram;
  std::vector<irs::doc_id_t> phrase;
  for (auto& scorer : scorers) {
    irs::by_ngram_similarity ngram_filter =
      make_filter("field", {"at", "tl", "la", "as"}, 1.F, false);
    irs::by_ngram_similarity phrase_filter =
      make_filter("field", {"at", "tl", "la", "as"}, 1.F, true);

    MakeResult(ngram_filter, std::span{&scorer, 1}, rdr, ngram);
    MakeResult(phrase_filter, std::span{&scorer, 1}, rdr, phrase);
    EXPECT_EQ(ngram, phrase);
  }
}

TEST_P(ngram_similarity_filter_test_case, missed_first_tfidf_test) {
  {
    irs::IndexWriterOptions opts;
    opts.features = features_with_norms();

    tests::json_doc_generator gen(resource("ngram_similarity.json"),
                                  &tests::normalized_string_json_field_factory);

    add_segment(gen, irs::OM_CREATE, opts);
  }

  auto rdr = open_reader();

  irs::by_ngram_similarity filter =
    make_filter("field", {"never_match", "at", "tl", "la", "as", "ll"}, 0.5f);

  Docs expected{11, 12, 13, 1, 2, 8, 5};

  irs::Scorer::ptr scorer{std::make_unique<irs::TFIDF>(false)};

  CheckQuery(filter, std::span{&scorer, 1}, expected, rdr);
}

TEST_P(ngram_similarity_filter_test_case, missed_first_bm25_test) {
  {
    irs::IndexWriterOptions opts;
    opts.features = features_with_norms();

    tests::json_doc_generator gen(resource("ngram_similarity.json"),
                                  &tests::normalized_string_json_field_factory);

    add_segment(gen, irs::OM_CREATE, opts);
  }

  auto rdr = open_reader();

  irs::by_ngram_similarity filter =
    make_filter("field", {"never_match", "at", "tl", "la", "as", "ll"}, 0.5f);

  Docs expected{11, 12, 13, 8, 1, 5, 2};

  irs::Scorer::ptr scorer{std::make_unique<irs::BM25>()};

  CheckQuery(filter, std::span{&scorer, 1}, expected, rdr);
}

TEST_P(ngram_similarity_filter_test_case, missed_first_bm15_test) {
  {
    irs::IndexWriterOptions opts;
    opts.features = features_with_norms();

    tests::json_doc_generator gen(resource("ngram_similarity.json"),
                                  &tests::normalized_string_json_field_factory);

    add_segment(gen, irs::OM_CREATE, opts);
  }

  auto rdr = open_reader();

  irs::by_ngram_similarity filter =
    make_filter("field", {"never_match", "at", "tl", "la", "as", "ll"}, 0.5f);

  Docs expected{11, 12, 13, 1, 2, 8, 5};

  irs::Scorer::ptr bm15{std::make_unique<irs::BM25>(irs::BM25::K(), 0.f)};

  CheckQuery(filter, std::span{&bm15, 1}, expected, rdr);
}

TEST_P(ngram_similarity_filter_test_case, seek_next) {
  {
    tests::json_doc_generator gen(resource("ngram_similarity.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  MaxMemoryCounter counter;

  irs::by_ngram_similarity filter =
    make_filter("field", {"never_match", "at", "tl", "la", "as", "ll"}, 0.5f);
  Docs expected{1, 2, 5, 8, 11, 12, 13};
  auto expected_it = std::begin(expected);
  auto prepared_filter = filter.prepare({
    .index = rdr,
    .memory = counter,
  });
  for (const auto& sub : rdr) {
    auto docs = prepared_filter->execute({.segment = sub});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(
      bool(doc));  // ensure all iterators contain "document" attribute
    ASSERT_EQ(irs::doc_limits::invalid(), docs->value());
    while (docs->next()) {
      ASSERT_EQ(docs->value(), *expected_it);
      ASSERT_EQ(doc->value, docs->value());
      // seek same
      ASSERT_EQ(*expected_it, docs->seek(*expected_it));
      // seek backward
      ASSERT_EQ(*expected_it, docs->seek((*expected_it) - 1));
      ++expected_it;
      if (expected_it != std::end(expected)) {
        // seek forward
        ASSERT_EQ(*expected_it, docs->seek(*expected_it));
        ++expected_it;
      }
    }
    ASSERT_EQ(irs::doc_limits::eof(), docs->value());
    ASSERT_FALSE(docs->next());
    ASSERT_EQ(irs::doc_limits::eof(), docs->value());
  }
  ASSERT_EQ(expected_it, std::end(expected));
  prepared_filter.reset();
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
}

TEST_P(ngram_similarity_filter_test_case, seek) {
  {
    tests::json_doc_generator gen(resource("ngram_similarity.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  MaxMemoryCounter counter;

  irs::by_ngram_similarity filter =
    make_filter("field", {"never_match", "at", "tl", "la", "as", "ll"}, 0.5f);
  Docs seek_tagrets{2, 5, 8, 13};
  auto seek_it = std::begin(seek_tagrets);
  auto& prepared_order = irs::Scorers::kUnordered;
  auto prepared_filter = filter.prepare({
    .index = rdr,
    .memory = counter,
    .scorers = prepared_order,
  });
  for (const auto& sub : rdr) {
    while (std::end(seek_tagrets) != seek_it) {
      auto docs = prepared_filter->execute({
        .segment = sub,
        .scorers = prepared_order,
      });
      auto* doc = irs::get<irs::document>(*docs);
      ASSERT_TRUE(
        bool(doc));  // ensure all iterators contain "document" attribute
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
  prepared_filter.reset();
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
}

static constexpr auto kTestDirs = tests::getDirectories<tests::kTypesDefault>();

INSTANTIATE_TEST_SUITE_P(
  ngram_similarity_test, ngram_similarity_filter_test_case,
  ::testing::Combine(::testing::ValuesIn(kTestDirs),
                     ::testing::Values(tests::format_info{"1_0"},
                                       tests::format_info{"1_3", "1_0"})),
  ngram_similarity_filter_test_case::to_string);

}  // namespace tests
