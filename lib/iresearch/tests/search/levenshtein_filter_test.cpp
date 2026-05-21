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

#include "search/levenshtein_filter.hpp"

#include "filter_test_case_base.hpp"
#include "index/norm.hpp"
#include "search/prefix_filter.hpp"
#include "search/term_filter.hpp"
#include "tests_shared.hpp"
#include "utils/levenshtein_default_pdp.hpp"
#include "utils/misc.hpp"

namespace {

irs::by_term make_term_filter(const std::string_view& field,
                              const std::string_view term) {
  irs::by_term q;
  *q.mutable_field() = field;
  q.mutable_options()->term = irs::ViewCast<irs::byte_type>(term);
  return q;
}

irs::by_edit_distance make_filter(const std::string_view& field,
                                  const std::string_view term,
                                  irs::byte_type max_distance = 0,
                                  size_t max_terms = 0,
                                  bool with_transpositions = false,
                                  const std::string_view prefix = "") {
  irs::by_edit_distance q;
  *q.mutable_field() = field;
  q.mutable_options()->term = irs::ViewCast<irs::byte_type>(term);
  q.mutable_options()->max_distance = max_distance;
  q.mutable_options()->max_terms = max_terms;
  q.mutable_options()->with_transpositions = with_transpositions;
  q.mutable_options()->prefix = irs::ViewCast<irs::byte_type>(prefix);
  return q;
}

}  // namespace

class levenshtein_filter_test_case : public tests::FilterTestCaseBase {};

TEST(by_edit_distance_test, options) {
  irs::by_edit_distance_options opts;
  ASSERT_EQ(0, opts.max_distance);
  ASSERT_EQ(0, opts.max_terms);
  ASSERT_FALSE(opts.with_transpositions);
  ASSERT_TRUE(opts.term.empty());
}

TEST(by_edit_distance_test, ctor) {
  irs::by_edit_distance q;
  ASSERT_EQ(irs::type<irs::by_edit_distance>::id(), q.type());
  ASSERT_EQ(irs::by_edit_distance_options{}, q.options());
  ASSERT_TRUE(q.field().empty());
  ASSERT_EQ(irs::kNoBoost, q.boost());
}

TEST(by_edit_distance_test, equal) {
  const irs::by_edit_distance q = make_filter("field", "bar", 1, 0, true);

  ASSERT_EQ(q, make_filter("field", "bar", 1, 0, true));
  ASSERT_NE(q, make_filter("field1", "bar", 1, 0, true));
  ASSERT_NE(q, make_filter("field", "baz", 1, 0, true));
  ASSERT_NE(q, make_filter("field", "bar", 2, 0, true));
  ASSERT_NE(q, make_filter("field", "bar", 1, 1024, true));
  ASSERT_NE(q, make_filter("field", "bar", 1, 0, false));
  {
    irs::by_prefix rhs;
    *rhs.mutable_field() = "field";
    rhs.mutable_options()->term =
      irs::ViewCast<irs::byte_type>(std::string_view("bar"));
    ASSERT_NE(q, rhs);
  }
}

TEST(by_edit_distance_test, boost) {
  MaxMemoryCounter counter;

  // no boost
  {
    irs::by_edit_distance q;
    *q.mutable_field() = "field";
    q.mutable_options()->term =
      irs::ViewCast<irs::byte_type>(std::string_view("bar*"));

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

    irs::by_edit_distance q;
    *q.mutable_field() = "field";
    q.mutable_options()->term =
      irs::ViewCast<irs::byte_type>(std::string_view("bar*"));
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

TEST(by_edit_distance_test, test_type_of_prepared_query) {
  MaxMemoryCounter counter;
  // term query
  {
    auto lhs = make_term_filter("foo", "bar")
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
}

#ifdef __clang__
#pragma GCC diagnostic pop
#endif  // __clang__

class by_edit_distance_test_case : public tests::FilterTestCaseBase {};

TEST_P(by_edit_distance_test_case, test_order) {
  // add segment
  {
    tests::json_doc_generator gen(resource("levenshtein_sequential.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  // empty query
  CheckQuery(irs::by_edit_distance(), Docs{}, Costs{0}, rdr);

  {
    Docs docs{28, 29};
    Costs costs{docs.size()};

    size_t term_collectors_count = 0;
    size_t field_collectors_count = 0;
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
    scorer.prepare_field_collector_ =
      [&scorer, &field_collectors_count]() -> irs::FieldCollector::ptr {
      ++field_collectors_count;
      return std::make_unique<tests::sort::custom_sort::field_collector>(
        scorer);
    };
    scorer.prepare_term_collector_ =
      [&scorer, &term_collectors_count]() -> irs::TermCollector::ptr {
      ++term_collectors_count;
      return std::make_unique<tests::sort::custom_sort::term_collector>(scorer);
    };

    CheckQuery(make_filter("title", "", 1, 0, false), order, docs, rdr);
    ASSERT_EQ(1, field_collectors_count);  // 1 field, 1 field collector
    ASSERT_EQ(1, term_collectors_count);  // need only 1 term collector since we
                                          // distribute stats across terms
    ASSERT_EQ(1, collect_field_count);    // 1 fields
    ASSERT_EQ(2, collect_term_count);     // 2 different terms
    ASSERT_EQ(1, finish_count);  // we distribute idf across all matched terms
  }

  {
    Docs docs{28, 29};
    Costs costs{docs.size()};

    size_t term_collectors_count = 0;
    size_t field_collectors_count = 0;
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
    scorer.prepare_field_collector_ =
      [&scorer, &field_collectors_count]() -> irs::FieldCollector::ptr {
      ++field_collectors_count;
      return std::make_unique<tests::sort::custom_sort::field_collector>(
        scorer);
    };
    scorer.prepare_term_collector_ =
      [&scorer, &term_collectors_count]() -> irs::TermCollector::ptr {
      ++term_collectors_count;
      return std::make_unique<tests::sort::custom_sort::term_collector>(scorer);
    };

    CheckQuery(make_filter("title", "", 1, 10, false), order, docs, rdr);
    ASSERT_EQ(1, field_collectors_count);  // 1 field, 1 field collector
    ASSERT_EQ(1, term_collectors_count);  // need only 1 term collector since we
                                          // distribute stats across terms
    ASSERT_EQ(1, collect_field_count);    // 1 fields
    ASSERT_EQ(2, collect_term_count);     // 2 different terms
    ASSERT_EQ(1, finish_count);  // we distribute idf across all matched terms
  }

  {
    Docs docs{29};
    Costs costs{docs.size()};

    size_t term_collectors_count = 0;
    size_t field_collectors_count = 0;
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
    scorer.prepare_field_collector_ =
      [&scorer, &field_collectors_count]() -> irs::FieldCollector::ptr {
      ++field_collectors_count;
      return std::make_unique<tests::sort::custom_sort::field_collector>(
        scorer);
    };
    scorer.prepare_term_collector_ =
      [&scorer, &term_collectors_count]() -> irs::TermCollector::ptr {
      ++term_collectors_count;
      return std::make_unique<tests::sort::custom_sort::term_collector>(scorer);
    };

    CheckQuery(make_filter("title", "", 1, 1, false), order, docs, rdr);
    ASSERT_EQ(1, field_collectors_count);  // 1 field, 1 field collector
    ASSERT_EQ(1, term_collectors_count);  // need only 1 term collector since we
                                          // distribute stats across terms
    ASSERT_EQ(1, collect_field_count);    // 1 fields
    ASSERT_EQ(1, collect_term_count);     // 1 term
    ASSERT_EQ(1, finish_count);  // we distribute idf across all matched terms
  }
}

TEST_P(by_edit_distance_test_case, test_filter) {
  // add data
  {
    tests::json_doc_generator gen(resource("levenshtein_sequential.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  auto rdr = open_reader();

  // empty query
  CheckQuery(irs::by_edit_distance(), Docs{}, Costs{0}, rdr);
  CheckQuery(make_filter("title", "", 0, 0, false), Docs{}, Costs{0}, rdr);

  //////////////////////////////////////////////////////////////////////////////
  /// Levenshtein and Damerau-Levenshtein with prefix
  //////////////////////////////////////////////////////////////////////////////
  // distance 0 (term query)
  CheckQuery(make_filter("title", "", 0, 1024, false, "aaaw"), Docs{32},
             Costs{1}, rdr);
  CheckQuery(make_filter("title", "w", 0, 1024, false, "aaa"), Docs{32},
             Costs{1}, rdr);
  CheckQuery(make_filter("title", "w", 0, 1024, true, "aaa"), Docs{32},
             Costs{1}, rdr);
  CheckQuery(make_filter("title", "", 0, 1024, false, ""), Docs{}, Costs{0},
             rdr);
  // distance 1
  CheckQuery(make_filter("title", "aa", 1, 1024, false, "aaabbba"), Docs{9, 10},
             Costs{2}, rdr);
  CheckQuery(make_filter("title", "", 1, 1024, false, ""), Docs{28, 29},
             Costs{2}, rdr);
  // distance 2
  CheckQuery(make_filter("title", "ca", 2, 1024, false, "b"), Docs{29, 30},
             Costs{2}, rdr);
  CheckQuery(make_filter("title", "aa", 2, 1024, false, "aa"),
             Docs{5, 7, 13, 16, 19, 27, 32}, Costs{7}, rdr);
  // distance 3
  CheckQuery(make_filter("title", "", 3, 1024, false, "aaa"),
             Docs{5, 7, 13, 16, 19, 32}, Costs{6}, rdr);
  CheckQuery(make_filter("title", "", 3, 1024, true, "aaa"),
             Docs{5, 7, 13, 16, 19, 32}, Costs{6}, rdr);

  //////////////////////////////////////////////////////////////////////////////
  /// Levenshtein
  //////////////////////////////////////////////////////////////////////////////

  // distance 0 (term query)
  CheckQuery(make_filter("title", "aa", 0, 1024), Docs{27}, Costs{1}, rdr);
  CheckQuery(make_filter("title", "aa", 0, 0), Docs{27}, Costs{1}, rdr);
  CheckQuery(make_filter("title", "aa", 0, 10), Docs{27}, Costs{1}, rdr);
  CheckQuery(make_filter("title", "aa", 0, 0), Docs{27}, Costs{1}, rdr);
  CheckQuery(make_filter("title", "ababab", 0, 10), Docs{17}, Costs{1}, rdr);
  CheckQuery(make_filter("title", "ababab", 0, 0), Docs{17}, Costs{1}, rdr);

  // distance 1
  CheckQuery(make_filter("title", "", 1, 1024), Docs{28, 29}, Costs{2}, rdr);
  CheckQuery(make_filter("title", "", 1, 0), Docs{28, 29}, Costs{2}, rdr);
  CheckQuery(make_filter("title", "", 1, 1), Docs{29}, Costs{1}, rdr);
  CheckQuery(make_filter("title", "aa", 1, 1024), Docs{27, 28}, Costs{2}, rdr);
  CheckQuery(make_filter("title", "aa", 1, 0), Docs{27, 28}, Costs{2}, rdr);
  CheckQuery(make_filter("title", "ababab", 1, 1024), Docs{17}, Costs{1}, rdr);
  CheckQuery(make_filter("title", "ababab", 0, 1024), Docs{17}, Costs{1}, rdr);

  // distance 2
  CheckQuery(make_filter("title", "", 2, 1024), Docs{27, 28, 29}, Costs{3},
             rdr);
  CheckQuery(make_filter("title", "", 2, 0), Docs{27, 28, 29}, Costs{3}, rdr);
  CheckQuery(make_filter("title", "", 2, 1), Docs{29}, Costs{1}, rdr);
  CheckQuery(make_filter("title", "", 2, 2), Docs{28, 29}, Costs{2}, rdr);
  CheckQuery(make_filter("title", "aa", 2, 1024), Docs{27, 28, 29, 30, 32},
             Costs{5}, rdr);
  CheckQuery(make_filter("title", "aa", 2, 0), Docs{27, 28, 29, 30, 32},
             Costs{5}, rdr);
  CheckQuery(make_filter("title", "ababab", 2, 1024), Docs{17}, Costs{1}, rdr);
  CheckQuery(make_filter("title", "ababab", 2, 0), Docs{17}, Costs{1}, rdr);

  // distance 3
  CheckQuery(make_filter("title", "", 3, 1024), Docs{27, 28, 29, 30, 31},
             Costs{5}, rdr);
  CheckQuery(make_filter("title", "", 3, 0), Docs{27, 28, 29, 30, 31}, Costs{5},
             rdr);
  CheckQuery(make_filter("title", "aaaa", 3, 10),
             Docs{
               5,
               7,
               13,
               16,
               17,
               18,
               19,
               21,
               27,
               28,
               30,
               32,
             },
             Costs{12}, rdr);
  CheckQuery(make_filter("title", "aaaa", 3, 0),
             Docs{
               5,
               7,
               13,
               16,
               17,
               18,
               19,
               21,
               27,
               28,
               30,
               32,
             },
             Costs{12}, rdr);
  CheckQuery(make_filter("title", "ababab", 3, 1024),
             Docs{3, 5, 7, 13, 14, 15, 16, 17, 32}, Costs{9}, rdr);
  CheckQuery(make_filter("title", "ababab", 3, 0),
             Docs{3, 5, 7, 13, 14, 15, 16, 17, 32}, Costs{9}, rdr);

  // distance 4
  CheckQuery(make_filter("title", "", 4, 1024), Docs{27, 28, 29, 30, 31, 32},
             Costs{6}, rdr);
  CheckQuery(make_filter("title", "", 4, 0), Docs{27, 28, 29, 30, 31, 32},
             Costs{6}, rdr);
  CheckQuery(
    make_filter("title", "ababab", 4, 1024),
    Docs{3, 4, 5, 6, 7, 10, 13, 14, 15, 16, 17, 18, 19, 21, 27, 30, 32, 34},
    Costs{18}, rdr);
  CheckQuery(
    make_filter("title", "ababab", 4, 0),
    Docs{3, 4, 5, 6, 7, 10, 13, 14, 15, 16, 17, 18, 19, 21, 27, 30, 32, 34},
    Costs{18}, rdr);

  // default provider doesn't support Levenshtein distances > 4
  CheckQuery(make_filter("title", "", 5, 1024), Docs{}, Costs{0}, rdr);
  CheckQuery(make_filter("title", "", 5, 0), Docs{}, Costs{0}, rdr);
  CheckQuery(make_filter("title", "", 6, 1024), Docs{}, Costs{0}, rdr);
  CheckQuery(make_filter("title", "", 6, 0), Docs{}, Costs{0}, rdr);

  //////////////////////////////////////////////////////////////////////////////
  /// Damerau-Levenshtein
  //////////////////////////////////////////////////////////////////////////////

  // distance 0 (term query)
  CheckQuery(make_filter("title", "aa", 0, 1024, true), Docs{27}, Costs{1},
             rdr);
  CheckQuery(make_filter("title", "aa", 0, 0, true), Docs{27}, Costs{1}, rdr);
  CheckQuery(make_filter("title", "ababab", 0, 1024, true), Docs{17}, Costs{1},
             rdr);
  CheckQuery(make_filter("title", "ababab", 0, 0, true), Docs{17}, Costs{1},
             rdr);

  // distance 1
  CheckQuery(make_filter("title", "", 1, 1024, true), Docs{28, 29}, Costs{2},
             rdr);
  CheckQuery(make_filter("title", "", 1, 0, true), Docs{28, 29}, Costs{2}, rdr);
  CheckQuery(make_filter("title", "aa", 1, 1024, true), Docs{27, 28}, Costs{2},
             rdr);
  CheckQuery(make_filter("title", "aa", 1, 0, true), Docs{27, 28}, Costs{2},
             rdr);
  CheckQuery(make_filter("title", "ababab", 1, 1024, true), Docs{17}, Costs{1},
             rdr);
  CheckQuery(make_filter("title", "ababab", 1, 0, true), Docs{17}, Costs{1},
             rdr);

  // distance 2
  CheckQuery(make_filter("title", "aa", 2, 1024, true),
             Docs{27, 28, 29, 30, 32}, Costs{5}, rdr);
  CheckQuery(make_filter("title", "aa", 2, 0, true), Docs{27, 28, 29, 30, 32},
             Costs{5}, rdr);
  CheckQuery(make_filter("title", "ababab", 2, 1024, true), Docs{17, 18},
             Costs{2}, rdr);
  CheckQuery(make_filter("title", "ababab", 2, 0, true), Docs{17, 18}, Costs{2},
             rdr);

  // distance 3
  CheckQuery(make_filter("title", "", 3, 1024, true), Docs{27, 28, 29, 30, 31},
             Costs{5}, rdr);
  CheckQuery(make_filter("title", "", 3, 0, true), Docs{27, 28, 29, 30, 31},
             Costs{5}, rdr);
  CheckQuery(make_filter("title", "ababab", 3, 1024, true),
             Docs{3, 5, 7, 13, 14, 15, 16, 17, 18, 32}, Costs{10}, rdr);
  CheckQuery(make_filter("title", "ababab", 3, 0, true),
             Docs{3, 5, 7, 13, 14, 15, 16, 17, 18, 32}, Costs{10}, rdr);

  // default provider doesn't support Damerau-Levenshtein distances > 3
  CheckQuery(make_filter("title", "", 4, 1024, true), Docs{}, Costs{0}, rdr);
  CheckQuery(make_filter("title", "", 4, 0, true), Docs{}, Costs{0}, rdr);
  CheckQuery(make_filter("title", "", 5, 1024, true), Docs{}, Costs{0}, rdr);
  CheckQuery(make_filter("title", "", 5, 0, true), Docs{}, Costs{0}, rdr);
}

TEST_P(by_edit_distance_test_case, bm25) {
  using tests::field_base;
  using tests::json_doc_generator;

  auto analyzer = irs::analysis::analyzers::get(
    "text", irs::type<irs::text_format::json>::get(),
    R"({"locale":"en.UTF-8", "stem":false, "accent":false, "case":"lower", "stopwords":[]})");
  ASSERT_NE(nullptr, analyzer);

  struct text_field : field_base {
   public:
    text_field(irs::analysis::analyzer& analyzer, std::string value)
      : value_(std::move(value)), analyzer_(&analyzer) {
      this->name("id");
      this->index_features_ = irs::IndexFeatures::FREQ;
      this->features_.emplace_back(irs::type<irs::Norm>::id());
    }

    bool write(irs::data_output&) const noexcept { return true; }

    irs::token_stream& get_tokens() const {
      const bool res = analyzer_->reset(value_);
      EXPECT_TRUE(res);
      return *analyzer_;
    }

   private:
    std::string value_;
    irs::analysis::analyzer* analyzer_;
  };

  {
    json_doc_generator gen(
      resource("v_DSS_Entity_id.json"),
      [&analyzer](tests::document& doc, const std::string& name,
                  const json_doc_generator::json_value& data) {
        if (json_doc_generator::ValueType::STRING == data.vt && name == "id") {
          auto field = std::make_shared<text_field>(
            *analyzer, std::string{data.str.data, data.str.size});
          doc.insert(field);
        }
      });

    irs::IndexWriterOptions opts;
    opts.features = [](irs::type_info::type_id id) {
      const irs::ColumnInfo info{
        irs::type<irs::compression::lz4>::get(), {}, false};

      if (irs::type<irs::Norm>::id() == id) {
        return std::make_pair(info, &irs::Norm::MakeWriter);
      }

      return std::make_pair(info, irs::FeatureWriterFactory{});
    };

    add_segment(gen, irs::OM_CREATE, opts);
  }

  std::array<irs::Scorer::ptr, 1> order{irs::scorers::get(
    "bm25", irs::type<irs::text_format::json>::get(), std::string_view{})};
  ASSERT_NE(nullptr, order.front());

  auto prepared_order = irs::Scorers::Prepare(order);

  auto index = open_reader();
  ASSERT_NE(nullptr, index);
  ASSERT_EQ(1, index->size());

  MaxMemoryCounter counter;

  {
    irs::by_edit_distance filter;
    *filter.mutable_field() = "id";
    auto& opts = *filter.mutable_options();
    opts.term = irs::ViewCast<irs::byte_type>(std::string_view("end202"));
    opts.max_distance = 2;
    opts.provider = irs::default_pdp;
    opts.with_transpositions = true;

    auto prepared = filter.prepare({
      .index = *index,
      .memory = counter,
      .scorers = prepared_order,
    });
    ASSERT_NE(nullptr, prepared);

    auto docs =
      prepared->execute({.segment = index[0], .scorers = prepared_order});
    ASSERT_NE(nullptr, docs);

    auto* score = irs::get<irs::score>(*docs);
    ASSERT_NE(nullptr, score);
    ASSERT_FALSE(score->Func() == &irs::ScoreFunction::DefaultScore);

    constexpr std::pair<float_t, irs::doc_id_t> expected_docs[]{
      {6.21361256f, 261},
      {9.32042027f, 272},
      {7.76701689f, 273},
      {6.21361256f, 289},
    };

    auto expected_doc = std::begin(expected_docs);
    while (docs->next()) {
      irs::score_t value;
      (*score)(&value);
      ASSERT_FLOAT_EQ(expected_doc->first, value);
      ASSERT_EQ(expected_doc->second, docs->value());
      ++expected_doc;
    }

    ASSERT_FALSE(docs->next());
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  {
    irs::by_edit_distance filter;
    *filter.mutable_field() = "id";
    auto& opts = *filter.mutable_options();
    opts.term = irs::ViewCast<irs::byte_type>(std::string_view("end202"));
    opts.max_distance = 1;
    opts.provider = irs::default_pdp;
    opts.with_transpositions = true;

    auto prepared = filter.prepare({
      .index = *index,
      .memory = counter,
      .scorers = prepared_order,
    });
    ASSERT_NE(nullptr, prepared);

    auto docs =
      prepared->execute({.segment = index[0], .scorers = prepared_order});
    ASSERT_NE(nullptr, docs);

    auto* score = irs::get<irs::score>(*docs);
    ASSERT_NE(nullptr, score);
    ASSERT_FALSE(score->Func() == &irs::ScoreFunction::DefaultScore);

    constexpr std::pair<float_t, irs::doc_id_t> expected_docs[]{
      {9.9112005f, 272},
      {8.2593336f, 273},
    };

    auto expected_doc = std::begin(expected_docs);
    while (docs->next()) {
      irs::score_t value;
      (*score)(&value);
      ASSERT_FLOAT_EQ(expected_doc->first, value);
      ASSERT_EQ(expected_doc->second, docs->value());
      ++expected_doc;
    }
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  // with prefix
  {
    irs::by_edit_distance filter;
    *filter.mutable_field() = "id";
    auto& opts = *filter.mutable_options();
    opts.prefix = irs::ViewCast<irs::byte_type>(std::string_view("end"));
    opts.term = irs::ViewCast<irs::byte_type>(std::string_view("202"));
    opts.max_distance = 1;
    opts.provider = irs::default_pdp;
    opts.with_transpositions = true;

    auto prepared = filter.prepare({
      .index = *index,
      .memory = counter,
      .scorers = prepared_order,
    });
    ASSERT_NE(nullptr, prepared);

    auto docs =
      prepared->execute({.segment = index[0], .scorers = prepared_order});
    ASSERT_NE(nullptr, docs);

    auto* score = irs::get<irs::score>(*docs);
    ASSERT_NE(nullptr, score);
    ASSERT_FALSE(score->Func() == &irs::ScoreFunction::DefaultScore);

    constexpr std::pair<float_t, irs::doc_id_t> expected_docs[]{
      {9.9112005f, 272},
      {8.2593336f, 273},
    };

    auto expected_doc = std::begin(expected_docs);
    while (docs->next()) {
      irs::score_t value;
      (*score)(&value);

      ASSERT_FLOAT_EQ(expected_doc->first, value);
      ASSERT_EQ(expected_doc->second, docs->value());
      ++expected_doc;
    }

    ASSERT_FALSE(docs->next());
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  {
    irs::by_edit_distance filter;
    *filter.mutable_field() = "id";
    auto& opts = *filter.mutable_options();
    opts.term = irs::ViewCast<irs::byte_type>(std::string_view("asm212"));
    opts.max_distance = 2;
    opts.provider = irs::default_pdp;
    opts.with_transpositions = true;

    auto prepared = filter.prepare({
      .index = *index,
      .memory = counter,
      .scorers = prepared_order,
    });
    ASSERT_NE(nullptr, prepared);

    auto docs =
      prepared->execute({.segment = index[0], .scorers = prepared_order});
    ASSERT_NE(nullptr, docs);

    auto* score = irs::get<irs::score>(*docs);
    ASSERT_NE(nullptr, score);
    ASSERT_FALSE(score->Func() == &irs::ScoreFunction::DefaultScore);

    constexpr std::pair<float_t, irs::doc_id_t> expected_docs[]{
      {8.1443901f, 265},   {6.9717164f, 46355}, {6.9717164f, 46356},
      {6.9717164f, 46357}, {6.7869916f, 264},   {6.7869916f, 3054},
      {6.7869916f, 3069},  {5.8097634f, 46353}, {5.8097634f, 46354},
      {5.4295926f, 263},   {5.4295926f, 3062},  {4.6478105f, 46350},
      {4.6478105f, 46351}, {4.6478105f, 46352},
    };

    std::vector<std::pair<float_t, irs::doc_id_t>> actual_docs;
    while (docs->next()) {
      irs::score_t value;
      (*score)(&value);
      actual_docs.emplace_back(value, docs->value());
    }
    ASSERT_FALSE(docs->next());
    ASSERT_EQ(std::size(expected_docs), actual_docs.size());

    std::sort(std::begin(actual_docs), std::end(actual_docs),
              [](const auto& lhs, const auto& rhs) {
                if (lhs.first < rhs.first) {
                  return false;
                }

                if (lhs.first > rhs.first) {
                  return true;
                }

                return lhs.second < rhs.second;
              });

    auto expected_doc = std::begin(expected_docs);
    for (auto& actual_doc : actual_docs) {
      ASSERT_FLOAT_EQ(expected_doc->first, actual_doc.first);
      ASSERT_EQ(expected_doc->second, actual_doc.second);
      ++expected_doc;
    }
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  {
    irs::by_edit_distance filter;
    *filter.mutable_field() = "id";
    auto& opts = *filter.mutable_options();
    opts.term = irs::ViewCast<irs::byte_type>(std::string_view("et038-pm"));
    opts.max_distance = 3;
    opts.provider = irs::default_pdp;
    opts.with_transpositions = true;

    auto prepared = filter.prepare({
      .index = *index,
      .memory = counter,
      .scorers = prepared_order,
    });
    ASSERT_NE(nullptr, prepared);

    auto docs =
      prepared->execute({.segment = index[0], .scorers = prepared_order});
    ASSERT_NE(nullptr, docs);

    auto* score = irs::get<irs::score>(*docs);
    ASSERT_NE(nullptr, score);
    ASSERT_FALSE(score->Func() == &irs::ScoreFunction::DefaultScore);

    constexpr std::pair<float_t, irs::doc_id_t> expected_docs[]{
      {3.8292058f, 275},
      {3.2778559f, 46376},
      {3.2778559f, 46377},
    };

    std::vector<std::pair<float_t, irs::doc_id_t>> actual_docs;
    while (docs->next()) {
      irs::score_t value;
      (*score)(&value);
      actual_docs.emplace_back(value, docs->value());
    }

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(std::size(expected_docs), actual_docs.size());

    std::sort(std::begin(actual_docs), std::end(actual_docs),
              [](const auto& lhs, const auto& rhs) {
                if (lhs.first < rhs.first) {
                  return false;
                }

                if (lhs.first > rhs.first) {
                  return true;
                }

                return lhs.second < rhs.second;
              });

    auto expected_doc = std::begin(expected_docs);
    for (auto& actual_doc : actual_docs) {
      ASSERT_EQ(expected_doc->second, actual_doc.second);
      ASSERT_FLOAT_EQ(expected_doc->first, actual_doc.first);
      ++expected_doc;
    }
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  // with prefix
  {
    irs::by_edit_distance filter;
    *filter.mutable_field() = "id";
    auto& opts = *filter.mutable_options();
    opts.prefix = irs::ViewCast<irs::byte_type>(std::string_view("et038"));
    opts.term = irs::ViewCast<irs::byte_type>(std::string_view("-pm"));
    opts.max_distance = 3;
    opts.provider = irs::default_pdp;
    opts.with_transpositions = true;

    auto prepared = filter.prepare({
      .index = *index,
      .memory = counter,
      .scorers = prepared_order,
    });
    ASSERT_NE(nullptr, prepared);

    auto docs =
      prepared->execute({.segment = index[0], .scorers = prepared_order});
    ASSERT_NE(nullptr, docs);

    auto* score = irs::get<irs::score>(*docs);
    ASSERT_NE(nullptr, score);
    ASSERT_FALSE(score->Func() == &irs::ScoreFunction::DefaultScore);

    constexpr std::pair<float_t, irs::doc_id_t> expected_docs[]{
      {3.8292058f, 275},
      {3.2778559f, 46376},
      {3.2778559f, 46377},
    };

    std::vector<std::pair<float_t, irs::doc_id_t>> actual_docs;
    while (docs->next()) {
      irs::score_t value;
      (*score)(&value);
      actual_docs.emplace_back(value, docs->value());
    }

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(std::size(expected_docs), actual_docs.size());

    std::sort(std::begin(actual_docs), std::end(actual_docs),
              [](const auto& lhs, const auto& rhs) {
                if (lhs.first < rhs.first) {
                  return false;
                }

                if (lhs.first > rhs.first) {
                  return true;
                }

                return lhs.second < rhs.second;
              });

    auto expected_doc = std::begin(expected_docs);
    for (auto& actual_doc : actual_docs) {
      ASSERT_EQ(expected_doc->second, actual_doc.second);
      ASSERT_FLOAT_EQ(expected_doc->first, actual_doc.first);
      ++expected_doc;
    }
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
}

TEST_P(by_edit_distance_test_case, visit) {
  // add segment
  {
    tests::json_doc_generator gen(resource("simple_sequential.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }
  const std::string_view field = "prefix";
  const auto term = irs::ViewCast<irs::byte_type>(std::string_view("abc"));
  // read segment
  auto index = open_reader();
  ASSERT_EQ(1, index.size());
  auto& segment = index[0];
  // get term dictionary for field
  const auto* reader = segment.field(field);
  ASSERT_NE(nullptr, reader);

  {
    irs::by_edit_distance_options opts;
    opts.term = term;
    opts.max_distance = 0;
    opts.provider = nullptr;
    opts.with_transpositions = false;

    tests::empty_filter_visitor visitor;
    auto field_visitor = irs::by_edit_distance::visitor(opts);
    ASSERT_TRUE(field_visitor);
    field_visitor(segment, *reader, visitor);
    ASSERT_EQ(1, visitor.prepare_calls_counter());
    ASSERT_EQ(1, visitor.visit_calls_counter());
    ASSERT_EQ((std::vector<std::pair<std::string_view, irs::score_t>>{
                {"abc", irs::kNoBoost}}),
              visitor.term_refs<char>());
    visitor.reset();
  }

  {
    irs::by_edit_distance_options opts;
    opts.term = term;
    opts.max_distance = 1;
    opts.provider = irs::default_pdp;
    opts.with_transpositions = false;

    tests::empty_filter_visitor visitor;
    auto field_visitor = irs::by_edit_distance::visitor(opts);
    ASSERT_TRUE(field_visitor);
    field_visitor(segment, *reader, visitor);
    ASSERT_EQ(1, visitor.prepare_calls_counter());
    ASSERT_EQ(3, visitor.visit_calls_counter());

    const auto actual_terms = visitor.term_refs<char>();
    std::vector<std::pair<std::string_view, irs::score_t>> expected_terms{
      {"abc", irs::kNoBoost},
      {"abcd", 2.f / 3},
      {"abcy", 2.f / 3},
    };
    ASSERT_EQ(expected_terms.size(), actual_terms.size());

    auto actual_term = actual_terms.begin();
    for (auto& expected_term : expected_terms) {
      ASSERT_EQ(expected_term.first, actual_term->first);
      ASSERT_FLOAT_EQ(expected_term.second, actual_term->second);
      ++actual_term;
    }

    visitor.reset();
  }

  // with prefix
  {
    irs::by_edit_distance_options opts;
    opts.term = irs::ViewCast<irs::byte_type>(std::string_view("c"));
    opts.max_distance = 2;
    opts.provider = irs::default_pdp;
    opts.with_transpositions = false;
    opts.prefix = irs::ViewCast<irs::byte_type>(std::string_view("ab"));

    tests::empty_filter_visitor visitor;
    auto field_visitor = irs::by_edit_distance::visitor(opts);
    ASSERT_TRUE(field_visitor);
    field_visitor(segment, *reader, visitor);
    ASSERT_EQ(1, visitor.prepare_calls_counter());
    ASSERT_EQ(5, visitor.visit_calls_counter());

    const auto actual_terms = visitor.term_refs<char>();
    std::vector<std::pair<std::string_view, irs::score_t>> expected_terms{
      {"abc", irs::kNoBoost}, {"abcd", 2.f / 3}, {"abcde", 1.f / 3},
      {"abcy", 2.f / 3},      {"abde", 1.f / 3},
    };
    ASSERT_EQ(expected_terms.size(), actual_terms.size());

    auto actual_term = actual_terms.begin();
    for (auto& expected_term : expected_terms) {
      ASSERT_EQ(expected_term.first, actual_term->first);
      ASSERT_FLOAT_EQ(expected_term.second, actual_term->second);
      ++actual_term;
    }

    visitor.reset();
  }
}

static constexpr auto kTestDirs = tests::getDirectories<tests::kTypesDefault>();

INSTANTIATE_TEST_SUITE_P(
  by_edit_distance_test, by_edit_distance_test_case,
  ::testing::Combine(::testing::ValuesIn(kTestDirs),
                     ::testing::Values(tests::format_info{"1_0"},
                                       tests::format_info{"1_1", "1_0"},
                                       tests::format_info{"1_2", "1_0"})),
  by_edit_distance_test_case::to_string);
