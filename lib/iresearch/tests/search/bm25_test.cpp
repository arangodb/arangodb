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

#include "search/bm25.hpp"

#include "index/index_tests.hpp"
#include "index/norm.hpp"
#include "search/all_filter.hpp"
#include "search/boolean_filter.hpp"
#include "search/column_existence_filter.hpp"
#include "search/phrase_filter.hpp"
#include "search/prefix_filter.hpp"
#include "search/range_filter.hpp"
#include "search/score.hpp"
#include "search/scorer.hpp"
#include "search/scorers.hpp"
#include "search/term_filter.hpp"
#include "tests_shared.hpp"

namespace {

struct bstring_data_output : public irs::data_output {
  irs::bstring out_;
  void write_byte(irs::byte_type b) final { write_bytes(&b, 1); }
  void write_bytes(const irs::byte_type* b, size_t size) final {
    out_.append(b, size);
  }
};

using namespace tests;

/////////////////
// Freq | Term //
/////////////////
// 4    | 0    //
// 3    | 1    //
// 10   | 2    //
// 7    | 3    //
// 5    | 4    //
// 4    | 5    //
// 3    | 6    //
// 7    | 7    //
// 2    | 8    //
// 7    | 9    //
/////////////////

//////////////////////////////////////////////////
// Stats                                        //
//////////////////////////////////////////////////
// TotalFreq = 52                               //
// DocsCount = 8                                //
// AverageDocLength (TotalFreq/DocsCount) = 6.5 //
//////////////////////////////////////////////////

class bm25_test_case : public index_test_base {
 protected:
  void test_query_norms(irs::type_info::type_id norm,
                        irs::FeatureWriterFactory handler);
};

void bm25_test_case::test_query_norms(irs::type_info::type_id norm,
                                      irs::FeatureWriterFactory handler) {
  {
    const std::vector<irs::type_info::type_id> extra_features = {norm};

    tests::json_doc_generator gen(
      resource("simple_sequential_order.json"),
      [&extra_features](tests::document& doc, const std::string& name,
                        const tests::json_doc_generator::json_value& data) {
        if (data.is_string()) {  // field
          doc.insert(
            std::make_shared<string_field>(
              name, data.str, irs::IndexFeatures::NONE, extra_features),
            true, false);
        } else if (data.is_number()) {  // seq
          const auto value = std::to_string(data.as_number<uint64_t>());
          doc.insert(std::make_shared<string_field>(
                       name, value, irs::IndexFeatures::NONE, extra_features),
                     false, true);
        }
      });

    irs::IndexWriterOptions opts;
    opts.features = [&](irs::type_info::type_id id) {
      irs::ColumnInfo info{irs::type<irs::compression::lz4>::get(), {}, false};

      if (id == norm) {
        return std::make_pair(info, handler);
      }

      return std::make_pair(info, irs::FeatureWriterFactory{});
    };

    add_segment(gen, irs::OM_CREATE, opts);
  }

  auto scorer = irs::BM25{irs::BM25::K(), irs::BM25::B(), true};
  auto prepared_order = irs::Scorers::Prepare(scorer);

  auto reader = irs::DirectoryReader(dir(), codec());
  auto& segment = *(reader.begin());
  const auto* column = segment.column("seq");
  ASSERT_NE(nullptr, column);

  MaxMemoryCounter counter;

  // by_range multiple
  {
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    irs::by_range filter;
    *filter.mutable_field() = "field";
    filter.mutable_options()->range.min =
      irs::ViewCast<irs::byte_type>(std::string_view("6"));
    filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
    filter.mutable_options()->range.max =
      irs::ViewCast<irs::byte_type>(std::string_view("8"));
    filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

    std::multimap<irs::score_t, uint32_t, std::greater<>> sorted;
    constexpr std::array expected{7, 3, 0, 1, 5};

    irs::bytes_view_input in;
    auto prepared_filter = filter.prepare({
      .index = reader,
      .memory = counter,
      .scorers = prepared_order,
    });
    auto docs =
      prepared_filter->execute({.segment = segment, .scorers = prepared_order});
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_TRUE(bool(score));

    while (docs->next()) {
      irs::score_t score_value;
      (*score)(&score_value);
      ASSERT_EQ(docs->value(), values->seek(docs->value()));
      in.reset(actual_value->value);

      auto str_seq = irs::read_string<std::string>(in);
      auto seq = strtoull(str_seq.c_str(), nullptr, 10);
      sorted.emplace(score_value, seq);
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry : sorted) {
      ASSERT_EQ(expected[i++], entry.second);
    }
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  // by_range multiple (3 values)
  {
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    irs::by_range filter;
    *filter.mutable_field() = "field";
    filter.mutable_options()->range.min =
      irs::ViewCast<irs::byte_type>(std::string_view("6"));
    filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
    filter.mutable_options()->range.max =
      irs::ViewCast<irs::byte_type>(std::string_view("8"));
    filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

    std::multimap<irs::score_t, uint32_t, std::greater<>> sorted;
    const auto expected = norm == irs::type<irs::Norm2>::id()
                            ? std::array{0, 7, 5, 3, 2, 1}
                            : std::array{7, 0, 5, 3, 2, 1};

    irs::bytes_view_input in;
    auto prepared_filter = filter.prepare({
      .index = reader,
      .memory = counter,
      .scorers = prepared_order,
    });
    auto docs =
      prepared_filter->execute({.segment = segment, .scorers = prepared_order});
    auto* score = irs::get<irs::score>(*docs);

    while (docs->next()) {
      irs::score_t score_value;
      (*score)(&score_value);
      ASSERT_EQ(docs->value(), values->seek(docs->value()));
      in.reset(actual_value->value);

      auto str_seq = irs::read_string<std::string>(in);
      auto seq = strtoull(str_seq.c_str(), nullptr, 10);
      sorted.emplace(score_value, seq);
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry : sorted) {
      ASSERT_EQ(expected[i++], entry.second);
    }
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
}

TEST_P(bm25_test_case, consts) {
  static_assert("bm25" == irs::type<irs::BM25>::name());
}

TEST_P(bm25_test_case, test_load) {
  auto scorer = irs::scorers::get(
    "bm25", irs::type<irs::text_format::json>::get(), std::string_view{});
  ASSERT_NE(nullptr, scorer);
}

TEST_P(bm25_test_case, make_from_array) {
  // default args
  {
    auto scorer = irs::scorers::get(
      "bm25", irs::type<irs::text_format::json>::get(), std::string_view{});
    ASSERT_NE(nullptr, scorer);
    ASSERT_EQ(irs::type<irs::BM25>::id(), scorer->type());
    auto& bm25 = dynamic_cast<irs::BM25&>(*scorer);
    ASSERT_EQ(irs::BM25::K(), bm25.k());
    ASSERT_EQ(irs::BM25::B(), bm25.b());
    ASSERT_EQ(irs::BM25::BOOST_AS_SCORE(), bm25.use_boost_as_score());
  }

  // default args
  {
    auto scorer =
      irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(), "[]");
    ASSERT_NE(nullptr, scorer);
    ASSERT_EQ(irs::type<irs::BM25>::id(), scorer->type());
    auto& bm25 = dynamic_cast<irs::BM25&>(*scorer);
    ASSERT_EQ(irs::BM25::K(), bm25.k());
    ASSERT_EQ(irs::BM25::B(), bm25.b());
    ASSERT_EQ(irs::BM25::BOOST_AS_SCORE(), bm25.use_boost_as_score());
  }

  // `k` argument
  {
    auto scorer = irs::scorers::get(
      "bm25", irs::type<irs::text_format::json>::get(), "[ 1.5 ]");
    ASSERT_NE(nullptr, scorer);
    ASSERT_EQ(irs::type<irs::BM25>::id(), scorer->type());
    auto& bm25 = dynamic_cast<irs::BM25&>(*scorer);
    ASSERT_EQ(1.5f, bm25.k());
    ASSERT_EQ(irs::BM25::B(), bm25.b());
    ASSERT_EQ(irs::BM25::BOOST_AS_SCORE(), bm25.use_boost_as_score());
  }

  // invalid `k` argument
  ASSERT_EQ(nullptr,
            irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(),
                              "[ \"1.5\" ]"));
  ASSERT_EQ(nullptr,
            irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(),
                              "[ \"abc\" ]"));
  ASSERT_EQ(nullptr,
            irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(),
                              "[ null]"));
  ASSERT_EQ(nullptr,
            irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(),
                              "[ true ]"));
  ASSERT_EQ(nullptr,
            irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(),
                              "[ false ]"));
  ASSERT_EQ(nullptr,
            irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(),
                              "[ {} ]"));
  ASSERT_EQ(nullptr,
            irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(),
                              "[ [] ]"));

  // `b` argument
  {
    auto scorer = irs::scorers::get(
      "bm25", irs::type<irs::text_format::json>::get(), "[ 1.5, 1.7 ]");
    ASSERT_NE(nullptr, scorer);
    ASSERT_EQ(irs::type<irs::BM25>::id(), scorer->type());
    auto& bm25 = dynamic_cast<irs::BM25&>(*scorer);
    ASSERT_EQ(1.5f, bm25.k());
    ASSERT_EQ(1.7f, bm25.b());
    ASSERT_EQ(irs::BM25::BOOST_AS_SCORE(), bm25.use_boost_as_score());
  }

  // invalid `b` argument
  ASSERT_EQ(nullptr,
            irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(),
                              "[ 1.5, \"1.7\" ]"));
  ASSERT_EQ(nullptr,
            irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(),
                              "[ 1.5, \"abc\" ]"));
  ASSERT_EQ(nullptr,
            irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(),
                              "[ 1.5, null]"));
  ASSERT_EQ(nullptr,
            irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(),
                              "[ 1.5, true ]"));
  ASSERT_EQ(nullptr,
            irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(),
                              "[ 1.5, false ]"));
  ASSERT_EQ(nullptr,
            irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(),
                              "[ 1.5, {} ]"));
  ASSERT_EQ(nullptr,
            irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(),
                              "[ 1.5, [] ]"));
}

TEST_P(bm25_test_case, test_normalize_features) {
  // default norms
  {
    auto scorer = irs::scorers::get(
      "bm25", irs::type<irs::text_format::json>::get(), std::string_view{});
    ASSERT_NE(nullptr, scorer);
    ASSERT_EQ(irs::IndexFeatures::FREQ, scorer->index_features());
  }

  // without norms (bm15)
  {
    auto scorer = irs::scorers::get(
      "bm25", irs::type<irs::text_format::json>::get(), "{\"b\": 0.0}");
    ASSERT_NE(nullptr, scorer);
    ASSERT_EQ(irs::IndexFeatures::FREQ, scorer->index_features());
  }

  // without norms (bm15), integer argument
  {
    auto scorer = irs::scorers::get(
      "bm25", irs::type<irs::text_format::json>::get(), "{\"b\": 0}");
    ASSERT_NE(nullptr, scorer);
    ASSERT_EQ(irs::IndexFeatures::FREQ, scorer->index_features());
  }
}

TEST_P(bm25_test_case, test_phrase) {
  auto analyzed_json_field_factory =
    [](tests::document& doc, const std::string& name,
       const tests::json_doc_generator::json_value& data) {
      typedef text_field<std::string> text_field;

      class string_field : public tests::string_field {
       public:
        string_field(const std::string& name, const std::string_view& value)
          : tests::string_field(name, value) {
          this->index_features_ = irs::IndexFeatures::FREQ;
        }
      };

      if (data.is_string()) {
        // analyzed field
        doc.indexed.push_back(std::make_shared<text_field>(
          std::string(name.c_str()) + "_anl", data.str));

        // not analyzed field
        doc.insert(std::make_shared<string_field>(name, data.str));
      }
    };

  // add segment
  {
    tests::json_doc_generator gen(resource("phrase_sequential.json"),
                                  analyzed_json_field_factory);
    add_segment(gen);
  }

  auto impl = irs::scorers::get(
    "bm25", irs::type<irs::text_format::json>::get(), "{ \"b\" : 0 }");
  auto prepared_order = irs::Scorers::Prepare(*impl);

  // read segment
  auto index = open_reader();
  ASSERT_EQ(1, index->size());
  auto& segment = *(index.begin());

  MaxMemoryCounter counter;

  // "jumps high" with order
  {
    irs::by_phrase filter;
    *filter.mutable_field() = "phrase_anl";
    filter.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("jumps"));
    filter.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("high"));

    std::multimap<irs::score_t, std::string, std::greater<>> sorted;

    constexpr std::array<std::string_view, 4> expected{
      "O",   // jumps high jumps high hotdog
      "P",   // jumps high jumps left jumps right jumps down jumps back
      "Q",   // jumps high jumps left jumps right jumps down walks back
      "R"};  // jumps high jumps left jumps right walks down walks back

    auto prepared_filter = filter.prepare({
      .index = *index,
      .memory = counter,
      .scorers = prepared_order,
    });
    auto docs =
      prepared_filter->execute({.segment = segment, .scorers = prepared_order});
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_TRUE(bool(score));

    auto column = segment.column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    while (docs->next()) {
      irs::score_t score_value;
      (*score)(&score_value);
      ASSERT_EQ(docs->value(), values->seek(docs->value()));

      sorted.emplace(score_value,
                     irs::to_string<std::string>(actual_value->value.data()));
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry : sorted) {
      ASSERT_EQ(expected[i++], entry.second);
    }
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  // "cookies ca* p_e bisKuit meringue|marshmallows" with order
  {
    irs::by_phrase filter;
    *filter.mutable_field() = "phrase_anl";
    auto& phrase = *filter.mutable_options();
    phrase.push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("cookies"));
    phrase.push_back<irs::by_prefix_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("ca"));
    phrase.push_back<irs::by_wildcard_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("p_e"));
    auto& lt = phrase.push_back<irs::by_edit_distance_options>();
    lt.max_distance = 1;
    lt.term = irs::ViewCast<irs::byte_type>(std::string_view("biscuit"));
    auto& ct = phrase.push_back<irs::by_terms_options>();
    ct.terms.emplace(
      irs::ViewCast<irs::byte_type>(std::string_view("meringue")));
    ct.terms.emplace(
      irs::ViewCast<irs::byte_type>(std::string_view("marshmallows")));

    std::multimap<irs::score_t, std::string, std::greater<>> sorted;

    constexpr std::array<std::string_view, 4> expected{
      "SPWLC0",   // cookies cake pie biscuit meringue cookies cake pie biscuit
                  // marshmallows paste bread
      "SPWLC1",   // cookies cake pie biskuit marshmallows cookies pie meringue
      "SPWLC2",   // cookies cake pie biscwit meringue pie biscuit paste
      "SPWLC3"};  // cookies cake pie biscuet marshmallows cake meringue

    auto prepared_filter = filter.prepare({
      .index = *index,
      .memory = counter,
      .scorers = prepared_order,
    });
    auto docs =
      prepared_filter->execute({.segment = segment, .scorers = prepared_order});
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_TRUE(bool(score));

    auto column = segment.column("name");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    while (docs->next()) {
      irs::score_t score_value;
      (*score)(&score_value);
      ASSERT_EQ(docs->value(), values->seek(docs->value()));

      sorted.emplace(score_value,
                     irs::to_string<std::string>(actual_value->value.data()));
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry : sorted) {
      ASSERT_EQ(expected[i++], entry.second);
    }
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
}

TEST_P(bm25_test_case, test_query) {
  {
    tests::json_doc_generator gen(
      resource("simple_sequential_order.json"),
      [](tests::document& doc, const std::string& name,
         const tests::json_doc_generator::json_value& data) {
        if (data.is_string()) {  // field
          doc.insert(std::make_shared<string_field>(name, data.str), true,
                     false);
        } else if (data.is_number()) {  // seq
          const auto value = std::to_string(data.as_number<uint64_t>());
          doc.insert(std::make_shared<string_field>(name, value), false, true);
        }
      });
    add_segment(gen);
  }

  irs::BM25 impl{irs::BM25::K(), irs::BM25::B(), true};
  const irs::Scorer* sort{&impl};

  auto prepared_order = irs::Scorers::Prepare(std::span{&sort, 1});

  auto reader = irs::DirectoryReader(dir(), codec());
  auto& segment = *(reader.begin());
  const auto* column = segment.column("seq");
  ASSERT_NE(nullptr, column);

  MaxMemoryCounter counter;

  // by_term
  {
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    irs::by_term filter;
    *filter.mutable_field() = "field";
    filter.mutable_options()->term =
      irs::ViewCast<irs::byte_type>(std::string_view("7"));

    std::multimap<irs::score_t, uint32_t, std::greater<>> sorted;
    constexpr std::array expected{0, 1, 5, 7};

    irs::bytes_view_input in;
    auto prepared_filter = filter.prepare({
      .index = reader,
      .memory = counter,
      .scorers = prepared_order,
    });
    auto docs =
      prepared_filter->execute({.segment = segment, .scorers = prepared_order});
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_TRUE(bool(score));

    while (docs->next()) {
      irs::score_t score_value;
      (*score)(&score_value);
      ASSERT_EQ(docs->value(), values->seek(docs->value()));
      in.reset(actual_value->value);

      auto str_seq = irs::read_string<std::string>(in);
      auto seq = strtoull(str_seq.c_str(), nullptr, 10);
      sorted.emplace(score_value, seq);
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry : sorted) {
      ASSERT_EQ(expected[i++], entry.second);
    }
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  // by term multi-segment, same term (same score for all docs)
  {
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    tests::json_doc_generator gen(
      resource("simple_sequential_order.json"),
      [](tests::document& doc, const std::string& name,
         const json_doc_generator::json_value& data) {
        if (data.is_string()) {  // field
          doc.insert(std::make_shared<string_field>(name, data.str), true,
                     false);
        } else if (data.is_number()) {  // seq
          const auto value = std::to_string(data.as_number<uint64_t>());
          doc.insert(std::make_shared<string_field>(name, value), false, true);
        }
      });
    auto writer = open_writer(irs::OM_CREATE);
    const document* doc;

    // add first segment (even 'seq')
    {
      gen.reset();
      while ((doc = gen.next())) {
        ASSERT_TRUE(insert(*writer, doc->indexed.begin(), doc->indexed.end(),
                           doc->stored.begin(), doc->stored.end()));
        gen.next();  // skip 1 doc
      }
      writer->Commit();
      AssertSnapshotEquality(*writer);
    }

    // add second segment (odd 'seq')
    {
      gen.reset();
      gen.next();  // skip 1 doc
      while ((doc = gen.next())) {
        ASSERT_TRUE(insert(*writer, doc->indexed.begin(), doc->indexed.end(),
                           doc->stored.begin(), doc->stored.end()));
        gen.next();  // skip 1 doc
      }
      writer->Commit();
      AssertSnapshotEquality(*writer);
    }

    auto reader = irs::DirectoryReader(dir(), codec());
    irs::by_term filter;
    *filter.mutable_field() = "field";
    filter.mutable_options()->term =
      irs::ViewCast<irs::byte_type>(std::string_view("6"));

    std::multimap<irs::score_t, uint32_t, std::greater<>> sorted;
    constexpr std::array expected{
      0, 2,  // segment 0
      5      // segment 1
    };

    irs::bytes_view_input in;
    auto prepared_filter = filter.prepare({
      .index = reader,
      .memory = counter,
      .scorers = prepared_order,
    });

    for (auto& segment : reader) {
      const auto* column = segment.column("seq");
      ASSERT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      auto docs = prepared_filter->execute(
        {.segment = segment, .scorers = prepared_order});
      auto* score = irs::get<irs::score>(*docs);
      ASSERT_TRUE(bool(score));

      while (docs->next()) {
        irs::score_t score_value;
        (*score)(&score_value);
        ASSERT_EQ(docs->value(), values->seek(docs->value()));
        in.reset(actual_value->value);

        auto str_seq = irs::read_string<std::string>(in);
        auto seq = strtoull(str_seq.c_str(), nullptr, 10);
        sorted.emplace(score_value, seq);
      }
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry : sorted) {
      ASSERT_EQ(expected[i++], entry.second);
    }
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  // by_term disjunction multi-segment, different terms (same score for all
  // docs)
  {
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    tests::json_doc_generator gen(
      resource("simple_sequential_order.json"),
      [](tests::document& doc, const std::string& name,
         const json_doc_generator::json_value& data) {
        if (data.is_string()) {  // field
          doc.insert(std::make_shared<string_field>(name, data.str), true,
                     false);
        } else if (data.is_number()) {  // seq
          const auto value = std::to_string(data.as_number<uint64_t>());
          doc.insert(std::make_shared<string_field>(name, value), false, true);
        }
      });
    auto writer = open_writer(irs::OM_CREATE);
    const document* doc;

    // add first segment (even 'seq')
    {
      gen.reset();
      while ((doc = gen.next())) {
        ASSERT_TRUE(insert(*writer, doc->indexed.begin(), doc->indexed.end(),
                           doc->stored.begin(), doc->stored.end()));
        gen.next();  // skip 1 doc
      }
      writer->Commit();
      AssertSnapshotEquality(*writer);
    }

    // add second segment (odd 'seq')
    {
      gen.reset();
      gen.next();  // skip 1 doc
      while ((doc = gen.next())) {
        ASSERT_TRUE(insert(*writer, doc->indexed.begin(), doc->indexed.end(),
                           doc->stored.begin(), doc->stored.end()));
        gen.next();  // skip 1 doc
      }
      writer->Commit();
      AssertSnapshotEquality(*writer);
    }

    auto reader = irs::DirectoryReader(dir(), codec());
    irs::Or filter;
    {
      // doc 0, 2, 5
      auto& sub = filter.add<irs::by_term>();
      *sub.mutable_field() = "field";
      sub.mutable_options()->term =
        irs::ViewCast<irs::byte_type>(std::string_view("6"));
    }
    {
      // doc 3, 7
      auto& sub = filter.add<irs::by_term>();
      *sub.mutable_field() = "field";
      sub.mutable_options()->term =
        irs::ViewCast<irs::byte_type>(std::string_view("8"));
    }

    std::multimap<irs::score_t, uint32_t, std::greater<>> sorted;
    constexpr std::array expected{
      3, 7,    // same value in 2 documents
      0, 2, 5  // same value in 3 documents
    };

    irs::bytes_view_input in;
    auto prepared_filter = filter.prepare({
      .index = reader,
      .memory = counter,
      .scorers = prepared_order,
    });

    for (auto& segment : reader) {
      const auto* column = segment.column("seq");
      ASSERT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      auto docs = prepared_filter->execute(
        {.segment = segment, .scorers = prepared_order});
      auto* score = irs::get<irs::score>(*docs);
      ASSERT_TRUE(bool(score));

      while (docs->next()) {
        irs::score_t score_value;
        (*score)(&score_value);
        ASSERT_EQ(docs->value(), values->seek(docs->value()));
        in.reset(actual_value->value);

        auto str_seq = irs::read_string<std::string>(in);
        auto seq = strtoull(str_seq.c_str(), nullptr, 10);
        sorted.emplace(score_value, seq);
      }
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry : sorted) {
      ASSERT_EQ(expected[i++], entry.second);
    }
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  // by_prefix empty multi-segment, different terms (same score for all docs)
  {
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    tests::json_doc_generator gen(
      resource("simple_sequential.json"),
      [](tests::document& doc, const std::string& name,
         const json_doc_generator::json_value& data) {
        if (data.is_string()) {  // field
          doc.insert(std::make_shared<string_field>(name, data.str), true,
                     false);
        } else if (data.is_number()) {  // seq
          const auto value = std::to_string(data.as_number<uint64_t>());
          doc.insert(std::make_shared<string_field>(name, value), false, true);
        }
      });
    auto writer = open_writer(irs::OM_CREATE);
    const document* doc;

    // add first segment (even 'seq')
    {
      gen.reset();
      while ((doc = gen.next())) {
        ASSERT_TRUE(insert(*writer, doc->indexed.begin(), doc->indexed.end(),
                           doc->stored.begin(), doc->stored.end()));
        gen.next();  // skip 1 doc
      }
      writer->Commit();
      AssertSnapshotEquality(*writer);
    }

    // add second segment (odd 'seq')
    {
      gen.reset();
      gen.next();  // skip 1 doc
      while ((doc = gen.next())) {
        ASSERT_TRUE(insert(*writer, doc->indexed.begin(), doc->indexed.end(),
                           doc->stored.begin(), doc->stored.end()));
        gen.next();  // skip 1 doc
      }
      writer->Commit();
      AssertSnapshotEquality(*writer);
    }

    auto reader = irs::DirectoryReader(dir(), codec());
    irs::by_prefix filter;
    *filter.mutable_field() = "prefix";
    filter.mutable_options()->term =
      irs::ViewCast<irs::byte_type>(std::string_view(""));

    std::multimap<irs::score_t, uint32_t, std::greater<>> sorted;
    constexpr std::array expected{
      0,  8,  20, 28,  // segment 0
      3,  15, 23, 25,  // segment 1
      30, 31,  // same value in segment 0 and segment 1 (smaller idf() ->
               // smaller tfidf() + reverse)
    };

    irs::bytes_view_input in;
    auto prepared_filter = filter.prepare({
      .index = reader,
      .memory = counter,
      .scorers = prepared_order,
    });

    for (auto& segment : reader) {
      const auto* column = segment.column("seq");
      ASSERT_NE(nullptr, column);
      auto values = column->iterator(irs::ColumnHint::kNormal);
      ASSERT_NE(nullptr, values);
      auto* actual_value = irs::get<irs::payload>(*values);
      ASSERT_NE(nullptr, actual_value);
      auto docs = prepared_filter->execute(
        {.segment = segment, .scorers = prepared_order});
      auto* score = irs::get<irs::score>(*docs);
      ASSERT_TRUE(bool(score));

      while (docs->next()) {
        irs::score_t score_value;
        (*score)(&score_value);
        ASSERT_EQ(docs->value(), values->seek(docs->value()));
        in.reset(actual_value->value);

        auto str_seq = irs::read_string<std::string>(in);
        auto seq = strtoull(str_seq.c_str(), nullptr, 10);
        sorted.emplace(score_value, seq);
      }
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry : sorted) {
      ASSERT_EQ(expected[i++], entry.second);
    }
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  // by_range single
  {
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    irs::by_range filter;
    *filter.mutable_field() = "field";
    filter.mutable_options()->range.min =
      irs::ViewCast<irs::byte_type>(std::string_view("6"));
    filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
    filter.mutable_options()->range.max =
      irs::ViewCast<irs::byte_type>(std::string_view("8"));
    filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;

    std::multimap<irs::score_t, uint32_t, std::greater<>> sorted;
    constexpr std::array expected{0, 1, 5, 7};

    irs::bytes_view_input in;
    auto prepared_filter = filter.prepare({
      .index = reader,
      .memory = counter,
      .scorers = prepared_order,
    });
    auto docs =
      prepared_filter->execute({.segment = segment, .scorers = prepared_order});
    auto* score = irs::get<irs::score>(*docs);

    while (docs->next()) {
      irs::score_t score_value;
      (*score)(&score_value);
      ASSERT_EQ(docs->value(), values->seek(docs->value()));
      in.reset(actual_value->value);

      auto str_seq = irs::read_string<std::string>(in);
      auto seq = strtoull(str_seq.c_str(), nullptr, 10);
      sorted.emplace(score_value, seq);
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry : sorted) {
      ASSERT_EQ(expected[i++], entry.second);
    }
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  // by_range single + scored_terms_limit(0)
  // by_range single + scored_terms_limit(1)
  for (size_t limit = 0; limit != 2; ++limit) {
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    irs::by_range filter;
    *filter.mutable_field() = "field";
    filter.mutable_options()->range.min =
      irs::ViewCast<irs::byte_type>(std::string_view("8"));
    filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
    filter.mutable_options()->range.max =
      irs::ViewCast<irs::byte_type>(std::string_view("9"));
    filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
    filter.mutable_options()->scored_terms_limit = limit;

    std::multimap<irs::score_t, uint32_t, std::greater<>> sorted;
    constexpr std::array expected{3, 7};

    irs::bytes_view_input in;
    auto prepared_filter = filter.prepare({
      .index = reader,
      .memory = counter,
      .scorers = prepared_order,
    });
    auto docs =
      prepared_filter->execute({.segment = segment, .scorers = prepared_order});
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_TRUE(bool(score));

    while (docs->next()) {
      irs::score_t score_value;
      (*score)(&score_value);
      ASSERT_EQ(docs->value(), values->seek(docs->value()));
      in.reset(actual_value->value);

      auto str_seq = irs::read_string<std::string>(in);
      auto seq = strtoull(str_seq.c_str(), nullptr, 10);
      sorted.emplace(score_value, seq);
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry : sorted) {
      ASSERT_EQ(expected[i++], entry.second);
    }
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  // by_range multiple
  {
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    irs::by_range filter;
    *filter.mutable_field() = "field";
    filter.mutable_options()->range.min =
      irs::ViewCast<irs::byte_type>(std::string_view("6"));
    filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
    filter.mutable_options()->range.max =
      irs::ViewCast<irs::byte_type>(std::string_view("8"));
    filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

    std::multimap<irs::score_t, uint32_t, std::greater<>> sorted;
    constexpr std::array expected{7, 3, 0, 1, 5};

    irs::bytes_view_input in;
    auto prepared_filter = filter.prepare({
      .index = reader,
      .memory = counter,
      .scorers = prepared_order,
    });
    auto docs =
      prepared_filter->execute({.segment = segment, .scorers = prepared_order});
    auto* score = irs::get<irs::score>(*docs);

    while (docs->next()) {
      irs::score_t score_value;
      (*score)(&score_value);
      ASSERT_EQ(docs->value(), values->seek(docs->value()));
      in.reset(actual_value->value);

      auto str_seq = irs::read_string<std::string>(in);
      auto seq = strtoull(str_seq.c_str(), nullptr, 10);
      sorted.emplace(score_value, seq);
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry : sorted) {
      ASSERT_EQ(expected[i++], entry.second);
    }
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  // by_range multiple (3 values)
  {
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    irs::by_range filter;
    *filter.mutable_field() = "field";
    filter.mutable_options()->range.min =
      irs::ViewCast<irs::byte_type>(std::string_view("6"));
    filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
    filter.mutable_options()->range.max =
      irs::ViewCast<irs::byte_type>(std::string_view("8"));
    filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

    std::multimap<irs::score_t, uint32_t, std::greater<>> sorted;
    constexpr std::array expected{7, 0, 5, 3, 2, 1};

    irs::bytes_view_input in;
    auto prepared_filter = filter.prepare({
      .index = reader,
      .memory = counter,
      .scorers = prepared_order,
    });
    auto docs =
      prepared_filter->execute({.segment = segment, .scorers = prepared_order});
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_TRUE(bool(score));

    while (docs->next()) {
      irs::score_t score_value;
      (*score)(&score_value);
      ASSERT_EQ(docs->value(), values->seek(docs->value()));
      in.reset(actual_value->value);

      auto str_seq = irs::read_string<std::string>(in);
      auto seq = strtoull(str_seq.c_str(), nullptr, 10);
      sorted.emplace(score_value, seq);
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry : sorted) {
      ASSERT_EQ(expected[i++], entry.second);
    }
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  // by phrase
  {
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    irs::by_phrase filter;
    *filter.mutable_field() = "field";
    filter.mutable_options()->push_back<irs::by_term_options>().term =
      irs::ViewCast<irs::byte_type>(std::string_view("7"));

    std::multimap<irs::score_t, uint32_t, std::greater<>> sorted;
    std::vector<std::pair<float_t, uint32_t>> expected = {
      {-1, 0},
      {-1, 1},
      {-1, 5},
      {-1, 7},
    };

    irs::bytes_view_input in;
    auto prepared_filter = filter.prepare({
      .index = reader,
      .memory = counter,
      .scorers = prepared_order,
    });
    auto docs =
      prepared_filter->execute({.segment = segment, .scorers = prepared_order});
    auto* score = irs::get<irs::score>(*docs);

    while (docs->next()) {
      irs::score_t score_value;
      (*score)(&score_value);
      ASSERT_EQ(docs->value(), values->seek(docs->value()));
      in.reset(actual_value->value);

      auto str_seq = irs::read_string<std::string>(in);
      auto seq = strtoull(str_seq.c_str(), nullptr, 10);
      sorted.emplace(score_value, seq);
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry : sorted) {
      auto& expected_entry = expected[i++];
      ASSERT_EQ(expected_entry.second, entry.second);
    }
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  // all
  {
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    irs::all filter;
    filter.boost(1.5f);

    auto prepared_filter = filter.prepare({
      .index = reader,
      .memory = counter,
      .scorers = prepared_order,
    });
    auto docs =
      prepared_filter->execute({.segment = segment, .scorers = prepared_order});
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_TRUE(bool(score));

    irs::doc_id_t doc = irs::doc_limits::min();
    while (docs->next()) {
      ASSERT_EQ(doc, docs->value());
      irs::score_t score_value;
      (*score)(&score_value);
      ASSERT_EQ(docs->value(), values->seek(docs->value()));
      ++doc;
      ASSERT_EQ(1.5f, score_value);
    }
    ASSERT_EQ(irs::doc_limits::eof(), docs->value());
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  // all
  {
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    irs::all filter;
    filter.boost(0.f);

    auto prepared_filter = filter.prepare({
      .index = reader,
      .memory = counter,
      .scorers = prepared_order,
    });
    auto docs =
      prepared_filter->execute({.segment = segment, .scorers = prepared_order});
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_TRUE(bool(score));
    ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

    irs::doc_id_t doc = irs::doc_limits::min();
    while (docs->next()) {
      ASSERT_EQ(doc, docs->value());

      irs::score_t score_value;
      (*score)(&score_value);
      ASSERT_EQ(docs->value(), values->seek(docs->value()));
      ++doc;
      ASSERT_EQ(0.f, score_value);
    }
    ASSERT_EQ(irs::doc_limits::eof(), docs->value());
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  // column existence
  {
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    irs::by_column_existence filter;
    *filter.mutable_field() = "seq";
    filter.mutable_options()->acceptor = {};

    auto prepared_filter = filter.prepare({
      .index = reader,
      .memory = counter,
      .scorers = prepared_order,
    });
    auto docs =
      prepared_filter->execute({.segment = segment, .scorers = prepared_order});
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_TRUE(bool(score));
    ASSERT_FALSE(score->Func() == &irs::ScoreFunction::DefaultScore);

    irs::doc_id_t doc = irs::doc_limits::min();
    while (docs->next()) {
      ASSERT_EQ(doc, docs->value());

      irs::score_t score_value;
      (*score)(&score_value);
      ASSERT_EQ(docs->value(), values->seek(docs->value()));
      ++doc;
      ASSERT_EQ(1.f, score_value);
    }
    ASSERT_EQ(irs::doc_limits::eof(), docs->value());
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();

  // column existence
  {
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    irs::by_column_existence filter;
    *filter.mutable_field() = "seq";
    filter.mutable_options()->acceptor = {};
    filter.boost(0.f);

    auto prepared_filter = filter.prepare({
      .index = reader,
      .memory = counter,
      .scorers = prepared_order,
    });
    auto docs =
      prepared_filter->execute({.segment = segment, .scorers = prepared_order});
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_TRUE(bool(score));
    ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

    irs::doc_id_t doc = irs::doc_limits::min();
    while (docs->next()) {
      ASSERT_EQ(doc, docs->value());

      irs::score_t score_value;
      (*score)(&score_value);

      ASSERT_EQ(docs->value(), values->seek(docs->value()));
      ++doc;
      ASSERT_EQ(0.f, score_value);
    }
    ASSERT_EQ(irs::doc_limits::eof(), docs->value());
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
}

TEST_P(bm25_test_case, test_query_norms) {
  test_query_norms(irs::type<irs::Norm>::id(), &irs::Norm::MakeWriter);
}

TEST_P(bm25_test_case, test_collector_serialization) {
  // initialize test data
  {
    tests::json_doc_generator gen(resource("simple_sequential.json"),
                                  &tests::payloaded_json_field_factory);
    auto writer = open_writer(irs::OM_CREATE);
    const document* doc;

    while ((doc = gen.next())) {
      ASSERT_TRUE(insert(*writer, doc->indexed.begin(), doc->indexed.end(),
                         doc->stored.begin(), doc->stored.end()));
    }

    writer->Commit();
    AssertSnapshotEquality(*writer);
  }

  auto reader = irs::DirectoryReader(dir(), codec());
  ASSERT_EQ(1, reader.size());
  auto* field = reader[0].field("name");
  ASSERT_NE(nullptr, field);
  auto term = field->iterator(irs::SeekMode::NORMAL);
  ASSERT_NE(nullptr, term);
  ASSERT_TRUE(term->next());
  term->read();  // fill term_meta
  irs::bstring fcollector_out;
  irs::bstring tcollector_out;

  // default init (field_collector)
  {
    irs::BM25 sort;
    auto collector = sort.prepare_field_collector();
    ASSERT_NE(nullptr, collector);
    bstring_data_output out0;
    collector->write(out0);
    collector->collect(reader[0], *field);
    bstring_data_output out1;
    collector->write(out1);
    fcollector_out = out1.out_;
    ASSERT_TRUE(out0.out_.size() != out1.out_.size() ||
                0 !=
                  std::memcmp(&out0.out_[0], &out1.out_[0], out0.out_.size()));

    // identical to default
    collector = sort.prepare_field_collector();
    collector->collect(out0.out_);
    bstring_data_output out2;
    collector->write(out2);
    ASSERT_TRUE(out0.out_.size() == out2.out_.size() &&
                0 ==
                  std::memcmp(&out0.out_[0], &out2.out_[0], out0.out_.size()));

    // identical to modified
    collector = sort.prepare_field_collector();
    collector->collect(out1.out_);
    bstring_data_output out3;
    collector->write(out3);
    ASSERT_TRUE(out1.out_.size() == out3.out_.size() &&
                0 ==
                  std::memcmp(&out1.out_[0], &out3.out_[0], out1.out_.size()));
  }

  // default init (term_collector)
  {
    irs::BM25 sort;
    auto collector = sort.prepare_term_collector();
    ASSERT_NE(nullptr, collector);
    bstring_data_output out0;
    collector->write(out0);
    collector->collect(reader[0], *field, *term);
    bstring_data_output out1;
    collector->write(out1);
    tcollector_out = out1.out_;
    ASSERT_TRUE(out0.out_.size() != out1.out_.size() ||
                0 !=
                  std::memcmp(&out0.out_[0], &out1.out_[0], out0.out_.size()));

    // identical to default
    collector = sort.prepare_term_collector();
    collector->collect(out0.out_);
    bstring_data_output out2;
    collector->write(out2);
    ASSERT_TRUE(out0.out_.size() == out2.out_.size() &&
                0 ==
                  std::memcmp(&out0.out_[0], &out2.out_[0], out0.out_.size()));

    // identical to modified
    collector = sort.prepare_term_collector();
    collector->collect(out1.out_);
    bstring_data_output out3;
    collector->write(out3);
    ASSERT_TRUE(out1.out_.size() == out3.out_.size() &&
                0 ==
                  std::memcmp(&out1.out_[0], &out3.out_[0], out1.out_.size()));
  }

  // serialized too short (field_collector)
  {
    irs::BM25 sort;
    auto collector = sort.prepare_field_collector();
    ASSERT_NE(nullptr, collector);
    ASSERT_THROW(collector->collect(irs::bytes_view(&fcollector_out[0],
                                                    fcollector_out.size() - 1)),
                 irs::io_error);
  }

  // serialized too short (term_collector)
  {
    irs::BM25 sort;
    auto collector = sort.prepare_term_collector();
    ASSERT_NE(nullptr, collector);
    ASSERT_THROW(collector->collect(irs::bytes_view(&tcollector_out[0],
                                                    tcollector_out.size() - 1)),
                 irs::io_error);
  }

  // serialized too long (field_collector)
  {
    irs::BM25 sort;
    auto collector = sort.prepare_field_collector();
    ASSERT_NE(nullptr, collector);
    auto out = fcollector_out;
    out.append(1, 42);
    ASSERT_THROW(collector->collect(out), irs::io_error);
  }

  // serialized too long (term_collector)
  {
    irs::BM25 sort;
    auto collector = sort.prepare_term_collector();
    auto out = tcollector_out;
    out.append(1, 42);
    ASSERT_THROW(collector->collect(out), irs::io_error);
  }
}

TEST_P(bm25_test_case, test_make) {
  // default values
  {
    auto scorer = irs::scorers::get(
      "bm25", irs::type<irs::text_format::json>::get(), std::string_view{});
    ASSERT_NE(nullptr, scorer);
    auto& scr = dynamic_cast<irs::BM25&>(*scorer);
    ASSERT_EQ(0.75f, scr.b());
    ASSERT_EQ(1.2f, scr.k());
    ASSERT_FALSE(scr.IsBM11());
    ASSERT_FALSE(scr.IsBM15());
  }

  // custom values
  {
    auto scorer =
      irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(),
                        "{\"b\": 123.456, \"k\": 78.9 }");
    ASSERT_NE(nullptr, scorer);
    auto& scr = dynamic_cast<irs::BM25&>(*scorer);
    ASSERT_EQ(123.456f, scr.b());
    ASSERT_EQ(78.9f, scr.k());
    ASSERT_FALSE(scr.IsBM11());
    ASSERT_FALSE(scr.IsBM15());
  }

  // custom values (integer argument)
  {
    auto scorer =
      irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(),
                        "{\"b\": 123.456, \"k\": 78 }");
    ASSERT_NE(nullptr, scorer);
    auto& scr = dynamic_cast<irs::BM25&>(*scorer);
    ASSERT_EQ(123.456f, scr.b());
    ASSERT_EQ(78.0f, scr.k());
    ASSERT_FALSE(scr.IsBM11());
    ASSERT_FALSE(scr.IsBM15());
  }

  // bm11
  {
    auto scorer =
      irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(),
                        "{\"b\": 1.0, \"k\": 78.9 }");
    ASSERT_NE(nullptr, scorer);
    auto& scr = dynamic_cast<irs::BM25&>(*scorer);
    ASSERT_EQ(1.f, scr.b());
    ASSERT_EQ(78.9f, scr.k());
    ASSERT_TRUE(scr.IsBM11());
    ASSERT_FALSE(scr.IsBM15());
  }

  // bm11 (integer argument)
  {
    auto scorer =
      irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(),
                        "{\"b\": 1, \"k\": 78.9 }");
    ASSERT_NE(nullptr, scorer);
    auto& scr = dynamic_cast<irs::BM25&>(*scorer);
    ASSERT_EQ(1.f, scr.b());
    ASSERT_EQ(78.9f, scr.k());
    ASSERT_TRUE(scr.IsBM11());
    ASSERT_FALSE(scr.IsBM15());
  }

  // bm15
  {
    auto scorer =
      irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(),
                        "{\"b\": 0.0, \"k\": 78.9 }");
    ASSERT_NE(nullptr, scorer);
    auto& scr = dynamic_cast<irs::BM25&>(*scorer);
    ASSERT_EQ(0.f, scr.b());
    ASSERT_EQ(78.9f, scr.k());
    ASSERT_FALSE(scr.IsBM11());
    ASSERT_TRUE(scr.IsBM15());
  }

  // bm15 (integer argument)
  {
    auto scorer =
      irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(),
                        "{\"b\": 0, \"k\": 78.9 }");
    ASSERT_NE(nullptr, scorer);
    auto& scr = dynamic_cast<irs::BM25&>(*scorer);
    ASSERT_EQ(0.f, scr.b());
    ASSERT_EQ(78.9f, scr.k());
    ASSERT_FALSE(scr.IsBM11());
    ASSERT_TRUE(scr.IsBM15());
  }

  // invalid args
  {
    auto scorer = irs::scorers::get(
      "bm25", irs::type<irs::text_format::json>::get(), "\"12345");
    ASSERT_EQ(nullptr, scorer);
  }

  // invalid values (b)
  {
    auto scorer =
      irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(),
                        "{\"b\": false, \"k\": 78.9}");
    ASSERT_EQ(nullptr, scorer);
  }

  // invalid values (k)
  {
    auto scorer =
      irs::scorers::get("bm25", irs::type<irs::text_format::json>::get(),
                        "{\"b\": 123.456, \"k\": true}");
    ASSERT_EQ(nullptr, scorer);
  }
}

TEST_P(bm25_test_case, test_order) {
  {
    tests::json_doc_generator gen(
      resource("simple_sequential_order.json"),
      [](tests::document& doc, const std::string& name,
         const tests::json_doc_generator::json_value& data) {
        if (data.is_string()) {  // field
          doc.insert(std::make_shared<string_field>(name, data.str), true,
                     false);
        } else if (data.is_number()) {  // seq
          const auto value = std::to_string(data.as_number<uint64_t>());
          doc.insert(std::make_shared<string_field>(name, value), false, true);
        }
      });
    add_segment(gen);
  }

  auto reader = irs::DirectoryReader(dir(), codec());
  auto& segment = *(reader.begin());

  MaxMemoryCounter counter;

  irs::by_term query;
  *query.mutable_field() = "field";

  irs::BM25 sort25;
  irs::BM25 sort15{irs::BM25::K(), 0.f};
  irs::BM25 sort11{irs::BM25::K(), 1.f};
  irs::BM25 sort1{0.f, 0.1337f};
  for (irs::score_t boost : {0.f, 0.5f, irs::kNoBoost}) {
    for (auto& sort : {sort25, sort15, sort11, sort1}) {
      const irs::Scorer* impl{&sort};

      auto prepared_order = irs::Scorers::Prepare(std::span{&impl, 1});
      ASSERT_EQ(sizeof(irs::score_t), prepared_order.score_size());
      ASSERT_EQ(1, prepared_order.buckets().size());

      uint64_t seq = 0;
      const auto* column = segment.column("seq");
      ASSERT_NE(nullptr, column);

      {
        auto values = column->iterator(irs::ColumnHint::kNormal);
        ASSERT_NE(nullptr, values);
        auto* actual_value = irs::get<irs::payload>(*values);
        ASSERT_NE(nullptr, actual_value);

        query.mutable_options()->term =
          irs::ViewCast<irs::byte_type>(std::string_view("7"));

        std::multimap<irs::score_t, uint32_t, std::greater<>> sorted;
        constexpr std::array expected{0, 1, 5, 7};

        irs::bytes_view_input in;
        auto prepared = query.prepare({
          .index = reader,
          .memory = counter,
          .scorers = prepared_order,
          .boost = boost,
        });
        auto docs =
          prepared->execute({.segment = segment, .scorers = prepared_order});
        auto* score = irs::get<irs::score>(*docs);
        ASSERT_TRUE(bool(score));

        for (; docs->next();) {
          irs::score_t score_value;
          (*score)(&score_value);

          ASSERT_EQ(docs->value(), values->seek(docs->value()));
          in.reset(actual_value->value);

          auto str_seq = irs::read_string<std::string>(in);
          seq = strtoull(str_seq.c_str(), nullptr, 10);
          sorted.emplace(score_value, seq);
        }

        ASSERT_EQ(expected.size(), sorted.size());
        const bool eq = std::equal(
          sorted.begin(), sorted.end(), expected.begin(),
          [](const auto& lhs, uint64_t rhs) { return lhs.second == rhs; });
        EXPECT_TRUE(eq);
      }
      EXPECT_EQ(counter.current, 0);
      EXPECT_GT(counter.max, 0);
      counter.Reset();
    }
  }
}

static constexpr auto kTestDirs = tests::getDirectories<tests::kTypesDefault>();

INSTANTIATE_TEST_SUITE_P(bm25_test, bm25_test_case,
                         ::testing::Combine(::testing::ValuesIn(kTestDirs),
                                            ::testing::Values("1_0")),
                         bm25_test_case::to_string);

class bm25_test_case_14 : public bm25_test_case {};

TEST_P(bm25_test_case_14, test_query_norms) {
  test_query_norms(irs::type<irs::Norm2>::id(), &irs::Norm2::MakeWriter);
}

INSTANTIATE_TEST_SUITE_P(bm25_test_14, bm25_test_case_14,
                         ::testing::Combine(::testing::ValuesIn(kTestDirs),
                                            ::testing::Values("1_4", "1_5")),
                         bm25_test_case_14::to_string);

}  // namespace
