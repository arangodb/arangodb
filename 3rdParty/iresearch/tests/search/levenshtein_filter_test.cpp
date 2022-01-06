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

#include "tests_shared.hpp"
#include "filter_test_case_base.hpp"
#include "index/norm.hpp"
#include "search/levenshtein_filter.hpp"
#include "search/prefix_filter.hpp"
#include "search/term_filter.hpp"
#include "utils/levenshtein_default_pdp.hpp"
#include "utils/misc.hpp"

namespace {

irs::by_term make_term_filter(
    const irs::string_ref& field,
    const irs::string_ref term) {
  irs::by_term q;
  *q.mutable_field() = field;
  q.mutable_options()->term = irs::ref_cast<irs::byte_type>(term);
  return q;
}

irs::by_edit_distance make_filter(
    const irs::string_ref& field,
    const irs::string_ref term,
    irs::byte_type max_distance = 0,
    size_t max_terms = 0,
    bool with_transpositions = false,
    const irs::string_ref prefix = irs::string_ref::EMPTY) {
  irs::by_edit_distance q;
  *q.mutable_field() = field;
  q.mutable_options()->term = irs::ref_cast<irs::byte_type>(term);
  q.mutable_options()->max_distance = max_distance;
  q.mutable_options()->max_terms = max_terms;
  q.mutable_options()->with_transpositions = with_transpositions;
  q.mutable_options()->prefix = irs::ref_cast<irs::byte_type>(prefix);
  return q;
}

}

class levenshtein_filter_test_case : public tests::filter_test_case_base { };

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
  ASSERT_EQ(irs::no_boost(), q.boost());
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
    rhs.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("bar"));
    ASSERT_NE(q, rhs);
  }
}

TEST(by_edit_distance_test, boost) {
  // no boost
  {
    irs::by_edit_distance q;
    *q.mutable_field() = "field";
    q.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("bar*"));

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(irs::no_boost(), prepared->boost());
  }

  // with boost
  {
    irs::boost_t boost = 1.5f;

    irs::by_edit_distance q;
    *q.mutable_field() = "field";
    q.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("bar*"));
    q.boost(boost);

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(boost, prepared->boost());
  }
}

#ifndef IRESEARCH_DLL

#ifdef __clang__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpotentially-evaluated-expression"
#endif // __clang__

TEST(by_edit_distance_test, test_type_of_prepared_query) {
  // term query
  {
    auto lhs = make_term_filter("foo", "bar").prepare(irs::sub_reader::empty());
    auto rhs = make_filter("foo", "bar").prepare(irs::sub_reader::empty());
    ASSERT_EQ(typeid(*lhs), typeid(*rhs));
  }
}

#ifdef __clang__
#pragma GCC diagnostic pop
#endif // __clang__

#endif

class by_edit_distance_test_case : public tests::filter_test_case_base { };

TEST_P(by_edit_distance_test_case, test_order) {
  // add segment
  {
    tests::json_doc_generator gen(
      resource("levenshtein_sequential.json"),
      &tests::generic_json_field_factory
    );
    add_segment(gen);
  }

  auto rdr = open_reader();

  // empty query
  check_query(irs::by_edit_distance(), docs_t{}, costs_t{0}, rdr);

  {
    docs_t docs{28, 29};
    costs_t costs{ docs.size() };
    irs::order order;

    size_t term_collectors_count = 0;
    size_t field_collectors_count = 0;
    size_t collect_field_count = 0;
    size_t collect_term_count = 0;
    size_t finish_count = 0;
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
    scorer.prepare_field_collector_ = [&scorer, &field_collectors_count]()->irs::sort::field_collector::ptr {
      ++field_collectors_count;
      return irs::memory::make_unique<tests::sort::custom_sort::prepared::field_collector>(scorer);
    };
    scorer.prepare_term_collector_ = [&scorer, &term_collectors_count]()->irs::sort::term_collector::ptr {
      ++term_collectors_count;
      return irs::memory::make_unique<tests::sort::custom_sort::prepared::term_collector>(scorer);
    };

    check_query(make_filter("title", "", 1, 0, false), order, docs, rdr);
    ASSERT_EQ(1, field_collectors_count); // 1 field, 1 field collector
    ASSERT_EQ(1, term_collectors_count); // need only 1 term collector since we distribute stats across terms
    ASSERT_EQ(1, collect_field_count); // 1 fields
    ASSERT_EQ(2, collect_term_count); // 2 different terms
    ASSERT_EQ(1, finish_count); // we distribute idf across all matched terms
  }

  {
    docs_t docs{28, 29};
    costs_t costs{ docs.size() };
    irs::order order;

    size_t term_collectors_count = 0;
    size_t field_collectors_count = 0;
    size_t collect_field_count = 0;
    size_t collect_term_count = 0;
    size_t finish_count = 0;
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
    scorer.prepare_field_collector_ = [&scorer, &field_collectors_count]()->irs::sort::field_collector::ptr {
      ++field_collectors_count;
      return irs::memory::make_unique<tests::sort::custom_sort::prepared::field_collector>(scorer);
    };
    scorer.prepare_term_collector_ = [&scorer, &term_collectors_count]()->irs::sort::term_collector::ptr {
      ++term_collectors_count;
      return irs::memory::make_unique<tests::sort::custom_sort::prepared::term_collector>(scorer);
    };

    check_query(make_filter("title", "", 1, 10, false), order, docs, rdr);
    ASSERT_EQ(1, field_collectors_count); // 1 field, 1 field collector
    ASSERT_EQ(1, term_collectors_count); // need only 1 term collector since we distribute stats across terms
    ASSERT_EQ(1, collect_field_count); // 1 fields
    ASSERT_EQ(2, collect_term_count); // 2 different terms
    ASSERT_EQ(1, finish_count); // we distribute idf across all matched terms
  }

  {
    docs_t docs{29};
    costs_t costs{ docs.size() };
    irs::order order;

    size_t term_collectors_count = 0;
    size_t field_collectors_count = 0;
    size_t collect_field_count = 0;
    size_t collect_term_count = 0;
    size_t finish_count = 0;
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
    scorer.prepare_field_collector_ = [&scorer, &field_collectors_count]()->irs::sort::field_collector::ptr {
      ++field_collectors_count;
      return irs::memory::make_unique<tests::sort::custom_sort::prepared::field_collector>(scorer);
    };
    scorer.prepare_term_collector_ = [&scorer, &term_collectors_count]()->irs::sort::term_collector::ptr {
      ++term_collectors_count;
      return irs::memory::make_unique<tests::sort::custom_sort::prepared::term_collector>(scorer);
    };

    check_query(make_filter("title", "", 1, 1, false), order, docs, rdr);
    ASSERT_EQ(1, field_collectors_count); // 1 field, 1 field collector
    ASSERT_EQ(1, term_collectors_count); // need only 1 term collector since we distribute stats across terms
    ASSERT_EQ(1, collect_field_count); // 1 fields
    ASSERT_EQ(1, collect_term_count); // 1 term
    ASSERT_EQ(1, finish_count); // we distribute idf across all matched terms
  }
}

TEST_P(by_edit_distance_test_case, test_filter) {
  // add data
  {
    tests::json_doc_generator gen(
      resource("levenshtein_sequential.json"),
      &tests::generic_json_field_factory
    );
    add_segment(gen);
  }

  auto rdr = open_reader();

  // empty query
  check_query(irs::by_edit_distance(), docs_t{}, costs_t{0}, rdr);
  check_query(make_filter("title", "", 0, 0, false), docs_t{}, costs_t{0}, rdr);

  //////////////////////////////////////////////////////////////////////////////
  /// Levenshtein and Damerau-Levenshtein with prefix
  //////////////////////////////////////////////////////////////////////////////
  // distance 0 (term query)
  check_query(make_filter("title", "", 0, 1024, false, "aaaw"), docs_t{32}, costs_t{1}, rdr);
  check_query(make_filter("title", "w", 0, 1024, false, "aaa"), docs_t{32}, costs_t{1}, rdr);
  check_query(make_filter("title", "w", 0, 1024, true, "aaa"), docs_t{32}, costs_t{1}, rdr);
  check_query(make_filter("title", "", 0, 1024, false, ""), docs_t{}, costs_t{0}, rdr);
  // distance 1
  check_query(make_filter("title", "aa", 1, 1024, false, "aaabbba"), docs_t{9, 10}, costs_t{2}, rdr);
  check_query(make_filter("title", "", 1, 1024, false, ""), docs_t{28,29}, costs_t{2}, rdr);
  // distance 2
  check_query(make_filter("title", "ca", 2, 1024, false, "b"), docs_t{29,30}, costs_t{2}, rdr);
  check_query(make_filter("title", "aa", 2, 1024, false, "aa"), docs_t{5,7,13,16,19,27,32}, costs_t{7}, rdr);
  // distance 3
  check_query(make_filter("title", "", 3, 1024, false, "aaa"), docs_t{5,7,13,16,19,32}, costs_t{6}, rdr);
  check_query(make_filter("title", "", 3, 1024, true, "aaa"), docs_t{5,7,13,16,19,32}, costs_t{6}, rdr);

  //////////////////////////////////////////////////////////////////////////////
  /// Levenshtein
  //////////////////////////////////////////////////////////////////////////////

  // distance 0 (term query)
  check_query(make_filter("title", "aa", 0, 1024), docs_t{27}, costs_t{1}, rdr);
  check_query(make_filter("title", "aa", 0, 0), docs_t{27}, costs_t{1}, rdr);
  check_query(make_filter("title", "aa", 0, 10), docs_t{27}, costs_t{1}, rdr);
  check_query(make_filter("title", "aa", 0, 0), docs_t{27}, costs_t{1}, rdr);
  check_query(make_filter("title", "ababab", 0, 10), docs_t{17}, costs_t{1}, rdr);
  check_query(make_filter("title", "ababab", 0, 0), docs_t{17}, costs_t{1}, rdr);

  // distance 1
  check_query(make_filter("title", "", 1, 1024), docs_t{28, 29}, costs_t{2}, rdr);
  check_query(make_filter("title", "", 1, 0), docs_t{28, 29}, costs_t{2}, rdr);
  check_query(make_filter("title", "", 1, 1), docs_t{29}, costs_t{1}, rdr);
  check_query(make_filter("title", "aa", 1, 1024), docs_t{27, 28}, costs_t{2}, rdr);
  check_query(make_filter("title", "aa", 1, 0), docs_t{27, 28}, costs_t{2}, rdr);
  check_query(make_filter("title", "ababab", 1, 1024), docs_t{17}, costs_t{1}, rdr);
  check_query(make_filter("title", "ababab", 0, 1024), docs_t{17}, costs_t{1}, rdr);

  // distance 2
  check_query(make_filter("title", "", 2, 1024), docs_t{27, 28, 29}, costs_t{3}, rdr);
  check_query(make_filter("title", "", 2, 0), docs_t{27, 28, 29}, costs_t{3}, rdr);
  check_query(make_filter("title", "", 2, 1), docs_t{29}, costs_t{1}, rdr);
  check_query(make_filter("title", "", 2, 2), docs_t{28, 29}, costs_t{2}, rdr);
  check_query(make_filter("title", "aa", 2, 1024), docs_t{27, 28, 29, 30, 32}, costs_t{5}, rdr);
  check_query(make_filter("title", "aa", 2, 0), docs_t{27, 28, 29, 30, 32}, costs_t{5}, rdr);
  check_query(make_filter("title", "ababab", 2, 1024), docs_t{17}, costs_t{1}, rdr);
  check_query(make_filter("title", "ababab", 2, 0), docs_t{17}, costs_t{1}, rdr);

  // distance 3
  check_query(make_filter("title", "", 3, 1024), docs_t{27, 28, 29, 30, 31}, costs_t{5}, rdr);
  check_query(make_filter("title", "", 3, 0), docs_t{27, 28, 29, 30, 31}, costs_t{5}, rdr);
  check_query(make_filter("title", "aaaa", 3, 10), docs_t{5, 7, 13, 16, 17, 18, 19, 21, 27, 28, 30, 32, }, costs_t{12}, rdr);
  check_query(make_filter("title", "aaaa", 3, 0), docs_t{5, 7, 13, 16, 17, 18, 19, 21, 27, 28, 30, 32, }, costs_t{12}, rdr);
  check_query(make_filter("title", "ababab", 3, 1024), docs_t{3, 5, 7, 13, 14, 15, 16, 17, 32}, costs_t{9}, rdr);
  check_query(make_filter("title", "ababab", 3, 0), docs_t{3, 5, 7, 13, 14, 15, 16, 17, 32}, costs_t{9}, rdr);

  // distance 4
  check_query(make_filter("title", "", 4, 1024), docs_t{27, 28, 29, 30, 31, 32}, costs_t{6}, rdr);
  check_query(make_filter("title", "", 4, 0), docs_t{27, 28, 29, 30, 31, 32}, costs_t{6}, rdr);
  check_query(make_filter("title", "ababab", 4, 1024), docs_t{3, 4, 5, 6, 7, 10, 13, 14, 15, 16, 17, 18, 19, 21, 27, 30, 32, 34}, costs_t{18}, rdr);
  check_query(make_filter("title", "ababab", 4, 0), docs_t{3, 4, 5, 6, 7, 10, 13, 14, 15, 16, 17, 18, 19, 21, 27, 30, 32, 34}, costs_t{18}, rdr);

  // default provider doesn't support Levenshtein distances > 4
  check_query(make_filter("title", "", 5, 1024), docs_t{}, costs_t{0}, rdr);
  check_query(make_filter("title", "", 5, 0), docs_t{}, costs_t{0}, rdr);
  check_query(make_filter("title", "", 6, 1024), docs_t{}, costs_t{0}, rdr);
  check_query(make_filter("title", "", 6, 0), docs_t{}, costs_t{0}, rdr);

  //////////////////////////////////////////////////////////////////////////////
  /// Damerau-Levenshtein
  //////////////////////////////////////////////////////////////////////////////

  // distance 0 (term query)
  check_query(make_filter("title", "aa", 0, 1024, true), docs_t{27}, costs_t{1}, rdr);
  check_query(make_filter("title", "aa", 0, 0, true), docs_t{27}, costs_t{1}, rdr);
  check_query(make_filter("title", "ababab", 0, 1024, true), docs_t{17}, costs_t{1}, rdr);
  check_query(make_filter("title", "ababab", 0, 0, true), docs_t{17}, costs_t{1}, rdr);

  // distance 1
  check_query(make_filter("title", "", 1, 1024, true), docs_t{28, 29}, costs_t{2}, rdr);
  check_query(make_filter("title", "", 1, 0, true), docs_t{28, 29}, costs_t{2}, rdr);
  check_query(make_filter("title", "aa", 1, 1024, true), docs_t{27, 28}, costs_t{2}, rdr);
  check_query(make_filter("title", "aa", 1, 0, true), docs_t{27, 28}, costs_t{2}, rdr);
  check_query(make_filter("title", "ababab", 1, 1024, true), docs_t{17}, costs_t{1}, rdr);
  check_query(make_filter("title", "ababab", 1, 0, true), docs_t{17}, costs_t{1}, rdr);

  // distance 2
  check_query(make_filter("title", "aa", 2, 1024, true), docs_t{27, 28, 29, 30, 32}, costs_t{5}, rdr);
  check_query(make_filter("title", "aa", 2, 0, true), docs_t{27, 28, 29, 30, 32}, costs_t{5}, rdr);
  check_query(make_filter("title", "ababab", 2, 1024, true), docs_t{17, 18}, costs_t{2}, rdr);
  check_query(make_filter("title", "ababab", 2, 0, true), docs_t{17, 18}, costs_t{2}, rdr);

  // distance 3
  check_query(make_filter("title", "", 3, 1024, true), docs_t{27, 28, 29, 30, 31}, costs_t{5}, rdr);
  check_query(make_filter("title", "", 3, 0, true), docs_t{27, 28, 29, 30, 31}, costs_t{5}, rdr);
  check_query(make_filter("title", "ababab", 3, 1024, true), docs_t{3, 5, 7, 13, 14, 15, 16, 17, 18, 32}, costs_t{10}, rdr);
  check_query(make_filter("title", "ababab", 3, 0, true), docs_t{3, 5, 7, 13, 14, 15, 16, 17, 18, 32}, costs_t{10}, rdr);

  // default provider doesn't support Damerau-Levenshtein distances > 3
  check_query(make_filter("title", "", 4, 1024, true), docs_t{}, costs_t{0}, rdr);
  check_query(make_filter("title", "", 4, 0, true), docs_t{}, costs_t{0}, rdr);
  check_query(make_filter("title", "", 5, 1024, true), docs_t{}, costs_t{0}, rdr);
  check_query(make_filter("title", "", 5, 0, true), docs_t{}, costs_t{0}, rdr);
}

TEST_P(by_edit_distance_test_case, bm25) {
  using tests::json_doc_generator;
  using tests::field_base;

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

    bool write(data_output&) const noexcept {
      return true;
    }

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
      [&analyzer](tests::document& doc,
         const std::string& name,
         const json_doc_generator::json_value& data) {
        if (json_doc_generator::ValueType::STRING == data.vt && name == "id") {
          auto field = std::make_shared<text_field>(
            *analyzer, std::string{data.str.data, data.str.size});
          doc.insert(field);
        }
      });

    irs::index_writer::init_options opts;
    opts.features = [](irs::type_info::type_id id) {
      const irs::column_info info{irs::type<irs::compression::lz4>::get(), {}, false};

      if (irs::type<irs::Norm>::id() == id) {
        return std::make_pair(info, &irs::Norm::MakeWriter);
      }

      return std::make_pair(info, irs::feature_writer_factory_t{});
    };

    add_segment(gen, irs::OM_CREATE, opts);
  }

  irs::order ord;
  auto scorer = irs::scorers::get(
    "bm25",
    irs::type<irs::text_format::json>::get(),
    irs::string_ref::NIL);
  ASSERT_NE(nullptr, scorer);
  ord.add(false, std::move(scorer));
  auto prepared_order = ord.prepare();

  auto index = open_reader();
  ASSERT_NE(nullptr, index);
  ASSERT_EQ(1, index->size());

  {
    irs::by_edit_distance filter;
    *filter.mutable_field() = "id";
    auto& opts = *filter.mutable_options();
    opts.term = irs::ref_cast<irs::byte_type>(irs::string_ref("end202"));
    opts.max_distance = 2;
    opts.provider = irs::default_pdp;
    opts.with_transpositions = true;

    auto prepared = filter.prepare(*index, prepared_order);
    ASSERT_NE(nullptr, prepared);

    auto docs = prepared->execute(index[0], prepared_order);
    ASSERT_NE(nullptr, docs);

    auto* score = irs::get<irs::score>(*docs);
    ASSERT_NE(nullptr, score);
    ASSERT_FALSE(score->is_default());

    constexpr std::pair<float_t, irs::doc_id_t> expected_docs[] {
      { 6.21361256f, 261 },
      { 9.32042027f, 272 },
      { 7.76701689f, 273 },
      { 6.21361256f, 289 },
    };

    auto expected_doc = std::begin(expected_docs);
    while (docs->next()) {
      const auto* scr = score->evaluate();
      ASSERT_NE(nullptr, scr);
      const float_t* value = reinterpret_cast<const float_t*>(scr);
      ASSERT_FLOAT_EQ(expected_doc->first, *value);
      ASSERT_EQ(expected_doc->second, docs->value());
      ++expected_doc;
    }

    ASSERT_FALSE(docs->next());
  }

  {
    irs::by_edit_distance filter;
    *filter.mutable_field() = "id";
    auto& opts = *filter.mutable_options();
    opts.term = irs::ref_cast<irs::byte_type>(irs::string_ref("end202"));
    opts.max_distance = 1;
    opts.provider = irs::default_pdp;
    opts.with_transpositions = true;

    auto prepared = filter.prepare(*index, prepared_order);
    ASSERT_NE(nullptr, prepared);

    auto docs = prepared->execute(index[0], prepared_order);
    ASSERT_NE(nullptr, docs);

    auto* score = irs::get<irs::score>(*docs);
    ASSERT_NE(nullptr, score);
    ASSERT_FALSE(score->is_default());

    constexpr std::pair<float_t, irs::doc_id_t> expected_docs[] {
      { 9.9112005f, 272 },
      { 8.2593336f, 273 },
    };

    auto expected_doc = std::begin(expected_docs);
    while (docs->next()) {
      const auto* scr = score->evaluate();
      ASSERT_NE(nullptr, scr);
      const float_t* value = reinterpret_cast<const float_t*>(scr);
      ASSERT_FLOAT_EQ(expected_doc->first, *value);
      ASSERT_EQ(expected_doc->second, docs->value());
      ++expected_doc;
    }
  }

  // with prefix
  {
    irs::by_edit_distance filter;
    *filter.mutable_field() = "id";
    auto& opts = *filter.mutable_options();
    opts.prefix = irs::ref_cast<irs::byte_type>(irs::string_ref("end"));
    opts.term = irs::ref_cast<irs::byte_type>(irs::string_ref("202"));
    opts.max_distance = 1;
    opts.provider = irs::default_pdp;
    opts.with_transpositions = true;

    auto prepared = filter.prepare(*index, prepared_order);
    ASSERT_NE(nullptr, prepared);

    auto docs = prepared->execute(index[0], prepared_order);
    ASSERT_NE(nullptr, docs);

    auto* score = irs::get<irs::score>(*docs);
    ASSERT_NE(nullptr, score);
    ASSERT_FALSE(score->is_default());

    constexpr std::pair<float_t, irs::doc_id_t> expected_docs[] {
      { 9.9112005f, 272 },
      { 8.2593336f, 273 },
    };

    auto expected_doc = std::begin(expected_docs);
    while (docs->next()) {
      const auto* scr = score->evaluate();
      ASSERT_NE(nullptr, scr);
      const float_t* value = reinterpret_cast<const float_t*>(scr);
      ASSERT_FLOAT_EQ(expected_doc->first, *value);
      ASSERT_EQ(expected_doc->second, docs->value());
      ++expected_doc;
    }

    ASSERT_FALSE(docs->next());
  }

  {
    irs::by_edit_distance filter;
    *filter.mutable_field() = "id";
    auto& opts = *filter.mutable_options();
    opts.term = irs::ref_cast<irs::byte_type>(irs::string_ref("asm212"));
    opts.max_distance = 2;
    opts.provider = irs::default_pdp;
    opts.with_transpositions = true;

    auto prepared = filter.prepare(*index, prepared_order);
    ASSERT_NE(nullptr, prepared);

    auto docs = prepared->execute(index[0], prepared_order);
    ASSERT_NE(nullptr, docs);

    auto* score = irs::get<irs::score>(*docs);
    ASSERT_NE(nullptr, score);
    ASSERT_FALSE(score->is_default());

    constexpr std::pair<float_t, irs::doc_id_t> expected_docs[] {
      { 8.1443901f, 265   },
      { 6.9717164f, 46355 },
      { 6.9717164f, 46356 },
      { 6.9717164f, 46357 },
      { 6.7869916f, 264   },
      { 6.7869916f, 3054  },
      { 6.7869916f, 3069  },
      { 5.8097634f, 46353 },
      { 5.8097634f, 46354 },
      { 5.4295926f, 263   },
      { 5.4295926f, 3062  },
      { 4.6478105f, 46350 },
      { 4.6478105f, 46351 },
      { 4.6478105f, 46352 },
    };

    std::vector<std::pair<float_t, irs::doc_id_t>> actual_docs;
    while (docs->next()) {
      const auto* scr = score->evaluate();
      ASSERT_NE(nullptr, scr);
      const float_t* value = reinterpret_cast<const float_t*>(scr);
      actual_docs.emplace_back(*value, docs->value());
    }
    ASSERT_FALSE(docs->next());
    ASSERT_EQ(IRESEARCH_COUNTOF(expected_docs), actual_docs.size());

    std::sort(
      std::begin(actual_docs), std::end(actual_docs),
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

  {
    irs::by_edit_distance filter;
    *filter.mutable_field() = "id";
    auto& opts = *filter.mutable_options();
    opts.term = irs::ref_cast<irs::byte_type>(irs::string_ref("et038-pm"));
    opts.max_distance = 3;
    opts.provider = irs::default_pdp;
    opts.with_transpositions = true;

    auto prepared = filter.prepare(*index, prepared_order);
    ASSERT_NE(nullptr, prepared);

    auto docs = prepared->execute(index[0], prepared_order);
    ASSERT_NE(nullptr, docs);

    auto* score = irs::get<irs::score>(*docs);
    ASSERT_NE(nullptr, score);
    ASSERT_FALSE(score->is_default());

    constexpr std::pair<float_t, irs::doc_id_t> expected_docs[] {
        { 3.8292058f, 275 },
        { 3.2778559f, 46376 },
        { 3.2778559f, 46377 },
    };

    std::vector<std::pair<float_t, irs::doc_id_t>> actual_docs;
    while (docs->next()) {
      const auto* scr = score->evaluate();
      ASSERT_NE(nullptr, scr);
      const float_t* value = reinterpret_cast<const float_t*>(scr);
      actual_docs.emplace_back(*value, docs->value());
    }

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(IRESEARCH_COUNTOF(expected_docs), actual_docs.size());

    std::sort(
      std::begin(actual_docs), std::end(actual_docs),
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

  // with prefix
  {
    irs::by_edit_distance filter;
    *filter.mutable_field() = "id";
    auto& opts = *filter.mutable_options();
    opts.prefix = irs::ref_cast<irs::byte_type>(irs::string_ref("et038"));
    opts.term = irs::ref_cast<irs::byte_type>(irs::string_ref("-pm"));
    opts.max_distance = 3;
    opts.provider = irs::default_pdp;
    opts.with_transpositions = true;

    auto prepared = filter.prepare(*index, prepared_order);
    ASSERT_NE(nullptr, prepared);

    auto docs = prepared->execute(index[0], prepared_order);
    ASSERT_NE(nullptr, docs);

    auto* score = irs::get<irs::score>(*docs);
    ASSERT_NE(nullptr, score);
    ASSERT_FALSE(score->is_default());

    constexpr std::pair<float_t, irs::doc_id_t> expected_docs[] {
      { 3.8292058f, 275 },
      { 3.2778559f, 46376 },
      { 3.2778559f, 46377 },
    };

    std::vector<std::pair<float_t, irs::doc_id_t>> actual_docs;
    while (docs->next()) {
      const auto* scr = score->evaluate();
      ASSERT_NE(nullptr, scr);
      const float_t* value = reinterpret_cast<const float_t*>(scr);
      actual_docs.emplace_back(*value, docs->value());
    }

    ASSERT_FALSE(docs->next());
    ASSERT_EQ(IRESEARCH_COUNTOF(expected_docs), actual_docs.size());

    std::sort(
      std::begin(actual_docs), std::end(actual_docs),
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
}

TEST_P(by_edit_distance_test_case, visit) {
  // add segment
  {
    tests::json_doc_generator gen(
      resource("simple_sequential.json"),
      &tests::generic_json_field_factory);
    add_segment(gen);
  }
  const irs::string_ref field = "prefix";
  const auto term = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
  // read segment
  auto index = open_reader();
  ASSERT_EQ(1, index.size());
  auto& segment = index[0];
  // get term dictionary for field
  const auto* reader = segment.field(field);
  ASSERT_NE(nullptr, reader);

  {
    irs::by_edit_distance_filter_options opts;
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
    ASSERT_EQ(
      (std::vector<std::pair<irs::string_ref, irs::boost_t>>{{"abc", irs::no_boost()}}),
      visitor.term_refs<char>());
    visitor.reset();
  }

  {
    irs::by_edit_distance_filter_options opts;
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
    std::vector<std::pair<irs::string_ref, irs::boost_t>> expected_terms{
      {"abc",  irs::no_boost()},
      {"abcd", 2.f/3},
      {"abcy", 2.f/3},
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
    irs::by_edit_distance_filter_options opts;
    opts.term = irs::ref_cast<irs::byte_type>(irs::string_ref("c"));
    opts.max_distance = 2;
    opts.provider = irs::default_pdp;
    opts.with_transpositions = false;
    opts.prefix = irs::ref_cast<irs::byte_type>(irs::string_ref("ab"));

    tests::empty_filter_visitor visitor;
    auto field_visitor = irs::by_edit_distance::visitor(opts);
    ASSERT_TRUE(field_visitor);
    field_visitor(segment, *reader, visitor);
    ASSERT_EQ(1, visitor.prepare_calls_counter());
    ASSERT_EQ(5, visitor.visit_calls_counter());

    const auto actual_terms = visitor.term_refs<char>();
    std::vector<std::pair<irs::string_ref, irs::boost_t>> expected_terms{
      {"abc",  irs::no_boost()},
      {"abcd", 2.f/3},
      {"abcde", 1.f/3},
      {"abcy", 2.f/3},
      {"abde", 1.f/3},
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

INSTANTIATE_TEST_SUITE_P(
  by_edit_distance_test,
  by_edit_distance_test_case,
  ::testing::Combine(
    ::testing::Values(
      &tests::directory<&tests::memory_directory>,
      &tests::directory<&tests::fs_directory>,
      &tests::directory<&tests::mmap_directory>),
    ::testing::Values(
      tests::format_info{"1_0"},
      tests::format_info{"1_1", "1_0"},
      tests::format_info{"1_2", "1_0"})),
  by_edit_distance_test_case::to_string
);
