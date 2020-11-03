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

#include "tests_shared.hpp"
#include "index/index_tests.hpp"
#include "search/all_filter.hpp"
#include "search/column_existence_filter.hpp"
#include "search/boolean_filter.hpp"
#include "search/phrase_filter.hpp"
#include "search/prefix_filter.hpp"
#include "search/range_filter.hpp"
#include "search/scorers.hpp"
#include "search/sort.hpp"
#include "search/score.hpp"
#include "search/term_filter.hpp"
#include "search/tfidf.hpp"
#include "utils/utf8_path.hpp"
#include "utils/type_limits.hpp"

namespace {

using namespace tests;

struct bstring_data_output: public data_output {
  irs::bstring out_;
  virtual void close() override {}
  virtual void write_byte(irs::byte_type b) override { write_bytes(&b, 1); }
  virtual void write_bytes(const irs::byte_type* b, size_t size) override {
    out_.append(b, size);
  }
};

class tfidf_test: public index_test_base { 
};

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

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

TEST_P(tfidf_test, consts) {
  static_assert("tfidf" == irs::type<irs::tfidf_sort>::name());
}

TEST_P(tfidf_test, test_load) {
  irs::order order;
  auto scorer = irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), irs::string_ref::NIL);

  ASSERT_NE(nullptr, scorer);
  ASSERT_EQ(1, order.add(true, std::move(scorer)).size());
}

#ifndef IRESEARCH_DLL

TEST_P(tfidf_test, make_from_bool) {
  // `withNorms` argument
  {
    auto scorer = irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), "true");
    ASSERT_NE(nullptr, scorer);
    auto& tfidf = dynamic_cast<irs::tfidf_sort&>(*scorer);
    ASSERT_EQ(true, tfidf.normalize());
    ASSERT_FALSE(tfidf.use_boost_as_score());
  }

  // invalid `withNorms` argument
  ASSERT_EQ(nullptr, irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), "\"false\""));
  ASSERT_EQ(nullptr, irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), "null"));
  ASSERT_EQ(nullptr, irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), "1"));
}

TEST_P(tfidf_test, make_from_array) {
  // default args
  {
    auto scorer = irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), irs::string_ref::NIL);
    ASSERT_NE(nullptr, scorer);
    ASSERT_EQ(irs::type<irs::tfidf_sort>::id(), scorer->type());
    auto& tfidf = dynamic_cast<irs::tfidf_sort&>(*scorer);
    ASSERT_EQ(irs::tfidf_sort::WITH_NORMS(), tfidf.normalize());
    ASSERT_EQ(irs::tfidf_sort::BOOST_AS_SCORE(), tfidf.use_boost_as_score());
  }

  // default args
  {
    auto scorer = irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), "[]");
    ASSERT_NE(nullptr, scorer);
    ASSERT_EQ(irs::type<irs::tfidf_sort>::id(), scorer->type());
    auto& tfidf = dynamic_cast<irs::tfidf_sort&>(*scorer);
    ASSERT_EQ(irs::tfidf_sort::WITH_NORMS(), tfidf.normalize());
    ASSERT_EQ(irs::tfidf_sort::BOOST_AS_SCORE(), tfidf.use_boost_as_score());
  }

  // `withNorms` argument
  {
    auto scorer = irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), "[ true ]");
    ASSERT_NE(nullptr, scorer);
    ASSERT_EQ(irs::type<irs::tfidf_sort>::id(), scorer->type());
    auto& tfidf = dynamic_cast<irs::tfidf_sort&>(*scorer);
    ASSERT_EQ(true, tfidf.normalize());
    ASSERT_EQ(irs::tfidf_sort::BOOST_AS_SCORE(), tfidf.use_boost_as_score());
  }

  // invalid `withNorms` argument
  ASSERT_EQ(nullptr, irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), "[ \"false\" ]"));
  ASSERT_EQ(nullptr, irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), "[ null]"));
  ASSERT_EQ(nullptr, irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), "[ 1 ]"));
  ASSERT_EQ(nullptr, irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), "[ {} ]"));
  ASSERT_EQ(nullptr, irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), "[ [] ]"));
}

#endif // IRESEARCH_DLL

TEST_P(tfidf_test, test_normalize_features) {
  // default norms
  {
    auto scorer = irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), irs::string_ref::NIL);
    ASSERT_NE(nullptr, scorer);
    auto prepared = scorer->prepare();
    ASSERT_NE(nullptr, prepared);
    ASSERT_EQ(irs::flags({irs::type<irs::frequency>::get()}), prepared->features());
  }

  // with norms (as args)
  {
    auto scorer = irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), "true");
    ASSERT_NE(nullptr, scorer);
    auto prepared = scorer->prepare();
    ASSERT_NE(nullptr, prepared);
    ASSERT_EQ(irs::flags({irs::type<irs::frequency>::get(), irs::type<irs::norm>::get()}), prepared->features());
  }

  // with norms
  {
    auto scorer = irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), "{\"withNorms\": true}");
    ASSERT_NE(nullptr, scorer);
    auto prepared = scorer->prepare();
    ASSERT_NE(nullptr, prepared);
    ASSERT_EQ(irs::flags({irs::type<irs::frequency>::get(), irs::type<irs::norm>::get()}), prepared->features());
  }

  // without norms (as args)
  {
    auto scorer = irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), "false");
    ASSERT_NE(nullptr, scorer);
    auto prepared = scorer->prepare();
    ASSERT_NE(nullptr, prepared);
    ASSERT_EQ(irs::flags({irs::type<irs::frequency>::get()}), prepared->features());
  }

  // without norms
  {
    auto scorer = irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), "{\"withNorms\": false}");
    ASSERT_NE(nullptr, scorer);
    auto prepared = scorer->prepare();
    ASSERT_NE(nullptr, prepared);
    ASSERT_EQ(irs::flags({irs::type<irs::frequency>::get()}), prepared->features());
  }
}

TEST_P(tfidf_test, test_phrase) {
  auto analyzed_json_field_factory = [](
      tests::document& doc,
      const std::string& name,
      const tests::json_doc_generator::json_value& data) {
    typedef templates::text_field<std::string> text_field;

    class string_field : public templates::string_field {
     public:
      string_field(const irs::string_ref& name, const irs::string_ref& value)
        : templates::string_field(name, value) {
      }

      const irs::flags& features() const {
        static irs::flags features{ irs::type<irs::frequency>::get() };
        return features;
      }
    }; // string_field

    if (data.is_string()) {
      // analyzed field
      doc.indexed.push_back(std::make_shared<text_field>(
        std::string(name.c_str()) + "_anl",
        data.str
      ));

      // not analyzed field
      doc.insert(std::make_shared<string_field>(
        irs::string_ref(name),
        data.str
      ));
    }
  };

  // add segment
  {
    tests::json_doc_generator gen(
      resource("phrase_sequential.json"),
      analyzed_json_field_factory);
    add_segment(gen);
  }

  irs::order order;
  order.add(true, std::make_unique<irs::tfidf_sort>(false, true));
  auto prepared_order = order.prepare();

  auto comparer = [&prepared_order] (const iresearch::bstring& lhs, const iresearch::bstring& rhs) {
    return prepared_order.less(lhs.c_str(), rhs.c_str());
  };

  // read segment
  auto index = open_reader();
  ASSERT_EQ(1, index->size());
  auto& segment = *(index.begin());

  // "jumps high" with order
  {
    irs::by_phrase filter;
    *filter.mutable_field() = "phrase_anl";
    auto& phrase = *filter.mutable_options();
    phrase.push_back(irs::by_term_options{}).term = irs::ref_cast<irs::byte_type>(irs::string_ref("jumps"));
    phrase.push_back(irs::by_term_options{}).term = irs::ref_cast<irs::byte_type>(irs::string_ref("high"));

    std::multimap<irs::bstring, std::string, decltype(comparer)> sorted(comparer);

    std::vector<std::string> expected {
      "O", // jumps high jumps high hotdog
      "P", // jumps high jumps left jumps right jumps down jumps back
      "Q", // jumps high jumps left jumps right jumps down walks back
      "R"  // jumps high jumps left jumps right walks down walks back
    };

    auto prepared_filter = filter.prepare(*index, prepared_order);
    auto docs = prepared_filter->execute(segment, prepared_order);
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_TRUE(bool(score));

    auto column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    irs::bytes_ref key_value;

    while (docs->next()) {
      ASSERT_TRUE(values(docs->value(), key_value));

      sorted.emplace(
        irs::bytes_ref(score->evaluate(), prepared_order.score_size()),
        irs::to_string<std::string>(key_value.c_str()));
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry: sorted) {
      ASSERT_EQ(expected[i++], entry.second);
    }
  }

  // "cookies ca* p_e bisKuit meringue|marshmallows" with order
  {
    irs::by_phrase filter;
    *filter.mutable_field() = "phrase_anl";
    auto& phrase = *filter.mutable_options();
    phrase.push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("cookies"));
    phrase.push_back<irs::by_prefix_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("ca"));
    phrase.push_back<irs::by_wildcard_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("p_e"));
    auto& lt = phrase.push_back<irs::by_edit_distance_filter_options>();
    lt.max_distance = 1;
    lt.term = irs::ref_cast<irs::byte_type>(irs::string_ref("biscuit"));
    auto& ct = phrase.push_back<irs::by_terms_options>();
    ct.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("meringue")));
    ct.terms.emplace(irs::ref_cast<irs::byte_type>(irs::string_ref("marshmallows")));

    std::multimap<irs::bstring, std::string, decltype(comparer)> sorted(comparer);

    std::vector<std::string> expected{
      "SPWLC0", // cookies cake pie biscuit meringue cookies cake pie biscuit marshmallows paste bread
      "SPWLC1", // cookies cake pie biskuit marshmallows cookies pie meringue
      "SPWLC2", // cookies cake pie biscwit meringue pie biscuit paste
      "SPWLC3"  // cookies cake pie biscuet marshmallows cake meringue
    };

    auto prepared_filter = filter.prepare(*index, prepared_order);
    auto docs = prepared_filter->execute(segment, prepared_order);
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_TRUE(bool(score));

    auto column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    irs::bytes_ref key_value;

    while (docs->next()) {
      ASSERT_TRUE(values(docs->value(), key_value));

      sorted.emplace(
        irs::bytes_ref(score->evaluate(), prepared_order.score_size()),
        irs::to_string<std::string>(key_value.c_str())
      );
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry: sorted) {
      ASSERT_EQ(expected[i++], entry.second);
    }
  }
}

TEST_P(tfidf_test, test_query) {
  {
    tests::json_doc_generator gen(
      resource("simple_sequential_order.json"),
      [](tests::document& doc, const std::string& name, const json_doc_generator::json_value& data) {
        if (data.is_string()) { // field
          doc.insert(std::make_shared<templates::string_field>(name, data.str), true, false);
        } else if (data.is_number()) { // seq
          const auto value = std::to_string(data.as_number<uint64_t>());
          doc.insert(std::make_shared<templates::string_field>(name, value), false, true);
        }
    });
    add_segment(gen);
  }

  irs::order order;
  order.add(true, std::make_unique<irs::tfidf_sort>(false, true));

  auto prepared_order = order.prepare();
  auto comparer = [&prepared_order](const irs::bstring& lhs, const irs::bstring& rhs)->bool {
    return prepared_order.less(lhs.c_str(), rhs.c_str());
  };

  auto reader = iresearch::directory_reader::open(dir(), codec());
  auto& segment = *(reader.begin());
  const auto* column = segment.column_reader("seq");
  ASSERT_NE(nullptr, column);
  auto values = column->values();

  // by_term
  {
    irs::by_term filter;
    *filter.mutable_field() = "field";
    filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("7"));

    std::multimap<irs::bstring, uint64_t, decltype(comparer)> sorted(comparer);
    std::vector<uint64_t> expected{ 0, 1, 5, 7 };

    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    auto prepared_filter = filter.prepare(reader, prepared_order);
    auto docs = prepared_filter->execute(segment, prepared_order);
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_TRUE(bool(score));

    while(docs->next()) {
      ASSERT_TRUE(values(docs->value(), actual_value));
      in.reset(actual_value);

      auto str_seq = irs::read_string<std::string>(in);
      auto seq = strtoull(str_seq.c_str(), nullptr, 10);
      sorted.emplace(
        irs::bytes_ref(score->evaluate(), prepared_order.score_size()),
        seq);
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry: sorted) {
      ASSERT_EQ(expected[i++], entry.second);
    }
  }

  // by term multi-segment, same term (same score for all docs)
  {
    tests::json_doc_generator gen(
      resource("simple_sequential_order.json"),
      [](tests::document& doc, const std::string& name, const json_doc_generator::json_value& data) {
        if (data.is_string()) { // field
          doc.insert(std::make_shared<templates::string_field>(name, data.str), true, false);
        } else if (data.is_number()) { // seq
          const auto value = std::to_string(data.as_number<uint64_t>());
          doc.insert(std::make_shared<templates::string_field>(name, value), false, true);
        }
    });
    auto writer = open_writer(irs::OM_CREATE);
    const document* doc;

    // add first segment (even 'seq')
    {
      gen.reset();
      while ((doc = gen.next())) {
        ASSERT_TRUE(insert(
          *writer,
          doc->indexed.begin(), doc->indexed.end(),
          doc->stored.begin(), doc->stored.end()
        ));
        gen.next(); // skip 1 doc
      }
      writer->commit();
    }

    // add second segment (odd 'seq')
    {
      gen.reset();
      gen.next(); // skip 1 doc
      while ((doc = gen.next())) {
        ASSERT_TRUE(insert(
          *writer,
          doc->indexed.begin(), doc->indexed.end(),
          doc->stored.begin(), doc->stored.end()
        ));
        gen.next(); // skip 1 doc
      }
      writer->commit();
    }

    auto reader = irs::directory_reader::open(dir(), codec());
    irs::by_term filter;
    *filter.mutable_field() = "field";
    filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("6"));

    std::multimap<irs::bstring, uint64_t, decltype(comparer)> sorted(comparer);
    std::vector<uint64_t> expected{
      0, 2, // segment 0
      5 // segment 1
    };

    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    auto prepared_filter = filter.prepare(reader, prepared_order);

    for (auto& segment: reader) {
      const auto* column = segment.column_reader("seq");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared_filter->execute(segment, prepared_order);
      auto* score = irs::get<irs::score>(*docs);
      ASSERT_TRUE(bool(score));

      while(docs->next()) {
        ASSERT_TRUE(values(docs->value(), actual_value));
        in.reset(actual_value);

        auto str_seq = irs::read_string<std::string>(in);
        auto seq = strtoull(str_seq.c_str(), nullptr, 10);
        sorted.emplace(
          irs::bytes_ref(score->evaluate(), prepared_order.score_size()),
          seq);
      }
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry: sorted) {
      ASSERT_EQ(expected[i++], entry.second);
    }
  }

  // by_term disjunction multi-segment, different terms (same score for all docs)
  {
    tests::json_doc_generator gen(
      resource("simple_sequential_order.json"),
      [](tests::document& doc, const std::string& name, const json_doc_generator::json_value& data) {
        if (data.is_string()) { // field
          doc.insert(std::make_shared<templates::string_field>(name, data.str), true, false);
        } else if (data.is_number()) { // seq
          const auto value = std::to_string(data.as_number<uint64_t>());
          doc.insert(std::make_shared<templates::string_field>(name, value), false, true);
        }
    });
    auto writer = open_writer(irs::OM_CREATE);
    const document* doc;

    // add first segment (even 'seq')
    {
      gen.reset();
      while ((doc = gen.next())) {
        ASSERT_TRUE(insert(
          *writer,
          doc->indexed.begin(), doc->indexed.end(),
          doc->stored.begin(), doc->stored.end()
        ));
        gen.next(); // skip 1 doc
      }
      writer->commit();
    }

    // add second segment (odd 'seq')
    {
      gen.reset();
      gen.next(); // skip 1 doc
      while ((doc = gen.next())) {
        ASSERT_TRUE(insert(
          *writer,
          doc->indexed.begin(), doc->indexed.end(),
          doc->stored.begin(), doc->stored.end()
        ));
        gen.next(); // skip 1 doc
      }
      writer->commit();
    }

    auto reader = irs::directory_reader::open(dir(), codec());
    irs::Or filter;
    {
      // doc 0, 2, 5
      auto& sub = filter.add<irs::by_term>();
      *sub.mutable_field() = "field";
      sub.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("6"));
    }
    {
      // doc 3, 7
      auto& sub = filter.add<irs::by_term>();
      *sub.mutable_field() = "field";
      sub.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("8"));
    }

    std::multimap<irs::bstring, uint64_t, decltype(comparer)> sorted(comparer);
    std::vector<uint64_t> expected{
      3, 7, // same value in 2 documents
      0, 2, 5 // same value in 3 documents
    };

    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    auto prepared_filter = filter.prepare(reader, prepared_order);

    for (auto& segment: reader) {
      const auto* column = segment.column_reader("seq");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared_filter->execute(segment, prepared_order);
      auto* score = irs::get<irs::score>(*docs);
      ASSERT_TRUE(bool(score));

      while(docs->next()) {
        ASSERT_TRUE(values(docs->value(), actual_value));
        in.reset(actual_value);

        auto str_seq = irs::read_string<std::string>(in);
        auto seq = strtoull(str_seq.c_str(), nullptr, 10);
        sorted.emplace(
          irs::bytes_ref(score->evaluate(), prepared_order.score_size()),
          seq);
      }
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry: sorted) {
      ASSERT_EQ(expected[i++], entry.second);
    }
  }

  // by_prefix empty multi-segment, different terms (same score for all docs)
  {
    tests::json_doc_generator gen(
      resource("simple_sequential.json"),
      [](tests::document& doc, const std::string& name, const json_doc_generator::json_value& data) {
        if (data.is_string()) { // field
          doc.insert(std::make_shared<templates::string_field>(name, data.str), true, false);
        } else if (data.is_number()) { // seq
          const auto value = std::to_string(data.as_number<uint64_t>());
          doc.insert(std::make_shared<templates::string_field>(name, value), false, true);
        }
    });
    auto writer = open_writer(irs::OM_CREATE);
    const document* doc;

    // add first segment (even 'seq')
    {
      gen.reset();
      while ((doc = gen.next())) {
        ASSERT_TRUE(insert(
          *writer,
          doc->indexed.begin(), doc->indexed.end(),
          doc->stored.begin(), doc->stored.end()
        ));
        gen.next(); // skip 1 doc
      }
      writer->commit();
    }

    // add second segment (odd 'seq')
    {
      gen.reset();
      gen.next(); // skip 1 doc
      while ((doc = gen.next())) {
        ASSERT_TRUE(insert(
          *writer,
          doc->indexed.begin(), doc->indexed.end(),
          doc->stored.begin(), doc->stored.end()
        ));
        gen.next(); // skip 1 doc
      }
      writer->commit();
    }

    auto reader = irs::directory_reader::open(dir(), codec());
    irs::by_prefix filter;
    *filter.mutable_field() = "prefix";
    filter.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref(""));

    std::multimap<irs::bstring, uint64_t, decltype(comparer)> sorted(comparer);
    std::vector<uint64_t> expected{
      0, 8, 20, 28, // segment 0
      3, 15, 23, 25, // segment 1
      30, 31, // same value in segment 0 and segment 1 (smaller idf() -> smaller tfidf() + reverse)
    };

    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    auto prepared_filter = filter.prepare(reader, prepared_order);

    for (auto& segment: reader) {
      const auto* column = segment.column_reader("seq");
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      auto docs = prepared_filter->execute(segment, prepared_order);
      auto* score = irs::get<irs::score>(*docs);
      ASSERT_TRUE(bool(score));

      while(docs->next()) {
        ASSERT_TRUE(values(docs->value(), actual_value));
        in.reset(actual_value);

        auto str_seq = irs::read_string<std::string>(in);
        auto seq = strtoull(str_seq.c_str(), nullptr, 10);
        sorted.emplace(
          irs::bytes_ref(score->evaluate(), prepared_order.score_size()),
          seq);
      }
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry: sorted) {
      ASSERT_EQ(expected[i++], entry.second);
    }
  }

  // by_range single
  {
    irs::by_range filter;
    *filter.mutable_field() = "field";
    filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("6"));
    filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
    filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("8"));
    filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;

    std::multimap<irs::bstring, uint64_t, decltype(comparer)> sorted(comparer);
    std::vector<uint64_t> expected{ 0, 1, 5, 7 };

    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    auto prepared_filter = filter.prepare(reader, prepared_order);
    auto docs = prepared_filter->execute(segment, prepared_order);
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_TRUE(bool(score));

    while(docs->next()) {
      ASSERT_TRUE(values(docs->value(), actual_value));
      in.reset(actual_value);

      auto str_seq = irs::read_string<std::string>(in);
      auto seq = strtoull(str_seq.c_str(), nullptr, 10);
      sorted.emplace(
        irs::bytes_ref(score->evaluate(), prepared_order.score_size()),
        seq);
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry: sorted) {
      ASSERT_EQ(expected[i++], entry.second);
    }
  }

  // by_range single + scored_terms_limit(1)
  {
    irs::by_range filter;
    *filter.mutable_field() = "field";
    filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("8"));
    filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
    filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("9"));
    filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
    filter.mutable_options()->scored_terms_limit = 1;

    std::multimap<irs::bstring, uint64_t, decltype(comparer)> sorted(comparer);
    std::vector<uint64_t> expected{ 3, 7 };

    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    auto prepared_filter = filter.prepare(reader, prepared_order);
    auto docs = prepared_filter->execute(segment, prepared_order);
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_TRUE(bool(score));

    while(docs->next()) {
      ASSERT_TRUE(values(docs->value(), actual_value));
      in.reset(actual_value);

      auto str_seq = irs::read_string<std::string>(in);
      auto seq = strtoull(str_seq.c_str(), nullptr, 10);
      sorted.emplace(
       irs::bytes_ref(score->evaluate(), prepared_order.score_size()),
       seq);
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry: sorted) {
      ASSERT_EQ(expected[i++], entry.second);
    }
  }

  //FIXME!!!
  // by_range single + scored_terms_limit(0)
//  {
//    irs::by_range filter;
//
//    filter.field("field").scored_terms_limit(0)
//      .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("8")
//      .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("9");
//
//    std::multimap<irs::bstring, uint64_t, decltype(comparer)> sorted(comparer);
//    std::vector<uint64_t> expected{ 3, 7 };
//
//    irs::bytes_ref actual_value;
//    irs::bytes_ref_input in;
//    auto prepared_filter = filter.prepare(reader, prepared_order);
//    auto docs = prepared_filter->execute(segment, prepared_order);
//    auto* score = irs::get<irs::score>(*docs);
//    ASSERT_TRUE(bool(score));
//
//    // ensure that we avoid COW for pre c++11 std::basic_string
//    const irs::bytes_ref score_value = score->value();
//
//    while(docs->next()) {
//      score->evaluate();
//      ASSERT_TRUE(values(docs->value(), actual_value));
//      in.reset(actual_value);
//
//      auto str_seq = irs::read_string<std::string>(in);
//      auto seq = strtoull(str_seq.c_str(), nullptr, 10);
//      sorted.emplace(score_value, seq);
//    }
//
//    ASSERT_EQ(expected.size(), sorted.size());
//    size_t i = 0;
//
//    for (auto& entry: sorted) {
//      ASSERT_EQ(expected[i++], entry.second);
//    }
//  }

  // by_range multiple
  {
    irs::by_range filter;
    *filter.mutable_field() = "field";
    filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("6"));
    filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
    filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("8"));
    filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

    std::multimap<irs::bstring, uint64_t, decltype(comparer)> sorted(comparer);
    std::vector<uint64_t> expected{
      // FIXME the following calculation is based on old formula
      7, // 3.45083 = sqrt(1)*(log(8/(4+1))+1) + sqrt(1)*(log(8/(2+1))+1)
      0, // 2.54612 = sqrt(3)*(log(8/(4+1))+1) + sqrt(0)*(log(8/(2+1))+1)
      1, // 2.0789  = sqrt(2)*(log(8/(4+1))+1) + sqrt(0)*(log(8/(2+1))+1)
      3, // 1.98083 = sqrt(0)*(log(8/(4+1))+1) + sqrt(1)*(log(8/(2+1))+1)
      5, // 1.47    = sqrt(1)*(log(8/(4+1))+1) + sqrt(0)*(log(8/(2+1))+1)
    };

    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    auto prepared_filter = filter.prepare(reader, prepared_order);
    auto docs = prepared_filter->execute(segment, prepared_order);
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_TRUE(bool(score));

    while(docs->next()) {
      ASSERT_TRUE(values(docs->value(), actual_value));
      in.reset(actual_value);

      auto str_seq = irs::read_string<std::string>(in);
      auto seq = strtoull(str_seq.c_str(), nullptr, 10);
      sorted.emplace(
        irs::bytes_ref(score->evaluate(), prepared_order.score_size()),
        seq);
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry: sorted) {
      ASSERT_EQ(expected[i++], entry.second);
    }
  }

  // by_range multiple (3 values)
  {
    irs::by_range filter;
    *filter.mutable_field() = "field";
    filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("6"));
    filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
    filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("8"));
    filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

    std::multimap<irs::bstring, uint64_t, decltype(comparer)> sorted(comparer);
    std::vector<uint64_t> expected{
      // FIXME the following calculation is based on old formula
      0, // 4.239268 = sqrt(1)*(log(8/(3+1))+1) + sqrt(3)*(log(8/(4+1))+1) + sqrt(0)*(log(8/(2+1))+1)
      7, // 3.450832 = sqrt(0)*(log(8/(3+1))+1) + sqrt(1)*(log(8/(4+1))+1) + sqrt(1)*(log(8/(2+1))+1)
      5, // 3.163150 = sqrt(1)*(log(8/(3+1))+1) + sqrt(1)*(log(8/(4+1))+1) + sqrt(0)*(log(8/(2+1))+1)
      1, // 2.078899 = sqrt(0)*(log(8/(3+1))+1) + sqrt(2)*(log(8/(4+1))+1) + sqrt(0)*(log(8/(2+1))+1)
      3, // 1.980829 = sqrt(0)*(log(8/(3+1))+1) + sqrt(0)*(log(8/(4+1))+1) + sqrt(1)*(log(8/(2+1))+1)
      2, // 1.693147 = sqrt(1)*(log(8/(3+1))+1) + sqrt(0)*(log(8/(4+1))+1) + sqrt(0)*(log(8/(2+1))+1)
    };

    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    auto prepared_filter = filter.prepare(reader, prepared_order);
    auto docs = prepared_filter->execute(segment, prepared_order);
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_TRUE(bool(score));

    while(docs->next()) {
      ASSERT_TRUE(values(docs->value(), actual_value));
      in.reset(actual_value);

      auto str_seq = irs::read_string<std::string>(in);
      auto seq = strtoull(str_seq.c_str(), nullptr, 10);
      sorted.emplace(
        irs::bytes_ref(score->evaluate(), prepared_order.score_size()),
        seq);
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry: sorted) {
      ASSERT_EQ(expected[i++], entry.second);
    }
  }

  // by_phrase
  {
    irs::by_phrase filter;
    *filter.mutable_field() = "field";
    auto& phrase = *filter.mutable_options();
    phrase.push_back<irs::by_term_options>().term = irs::ref_cast<irs::byte_type>(irs::string_ref("7"));

    std::multimap<irs::bstring, uint64_t, decltype(comparer)> sorted(comparer);
    std::vector<std::pair<float_t, uint64_t>> expected = {
      { -1, 0 },
      { -1, 1 },
      { -1, 5 },
      { -1, 7 },
    };

    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    auto prepared_filter = filter.prepare(reader, prepared_order);
    auto docs = prepared_filter->execute(segment, prepared_order);
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_TRUE(bool(score));

    while(docs->next()) {
      ASSERT_TRUE(values(docs->value(), actual_value));
      in.reset(actual_value);

      auto str_seq = irs::read_string<std::string>(in);
      auto seq = strtoull(str_seq.c_str(), nullptr, 10);
      sorted.emplace(
        irs::bytes_ref(score->evaluate(), prepared_order.score_size()),
        seq);
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry: sorted) {
      auto& expected_entry = expected[i++];
      ASSERT_TRUE(
        sizeof(float_t) == entry.first.size()
        //&& expected_entry.first == *reinterpret_cast<const float_t*>(&entry.first[0])
      );
      ASSERT_EQ(expected_entry.second, entry.second);
    }
  }

  // all
  {
    irs::all filter;
    filter.boost(1.5f);

    irs::bytes_ref actual_value;
    auto prepared_filter = filter.prepare(reader, prepared_order);
    auto docs = prepared_filter->execute(segment, prepared_order);
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_TRUE(bool(score));
    ASSERT_FALSE(score->is_default());

    irs::doc_id_t doc = irs::type_limits<irs::type_t::doc_id_t>::min();
    while(docs->next()) {
      ASSERT_EQ(doc, docs->value());

      const irs::bytes_ref score_value(score->evaluate(), prepared_order.score_size());
      ASSERT_TRUE(values(docs->value(), actual_value));
      ++doc;
      ASSERT_EQ(1.5f, *reinterpret_cast<const float_t*>(score_value.c_str()));
    }
    ASSERT_EQ(irs::doc_limits::eof(), docs->value());
  }

  // all
  {
    irs::all filter;
    filter.boost(0.f);

    irs::bytes_ref actual_value;
    auto prepared_filter = filter.prepare(reader, prepared_order);
    auto docs = prepared_filter->execute(segment, prepared_order);
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_TRUE(bool(score));
    ASSERT_TRUE(score->is_default());

    irs::doc_id_t doc = irs::type_limits<irs::type_t::doc_id_t>::min();
    while(docs->next()) {
      ASSERT_EQ(doc, docs->value());

      const irs::bytes_ref score_value(score->evaluate(), prepared_order.score_size());
      ASSERT_TRUE(values(docs->value(), actual_value));
      ++doc;
      ASSERT_EQ(0.f, *reinterpret_cast<const float_t*>(score_value.c_str()));
    }
    ASSERT_EQ(irs::doc_limits::eof(), docs->value());
  }

  // column existence
  {
    irs::by_column_existence filter;
    *filter.mutable_field() = "seq";
    filter.mutable_options()->prefix_match = false;

    irs::bytes_ref actual_value;
    auto prepared_filter = filter.prepare(reader, prepared_order);
    auto docs = prepared_filter->execute(segment, prepared_order);
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_TRUE(bool(score));
    ASSERT_FALSE(score->is_default());

    irs::doc_id_t doc = irs::doc_limits::min();
    while(docs->next()) {
      ASSERT_EQ(doc, docs->value());

      const irs::bytes_ref score_value(score->evaluate(), prepared_order.score_size());
      ASSERT_TRUE(values(docs->value(), actual_value));
      ++doc;
      ASSERT_EQ(1.f, *reinterpret_cast<const float_t*>(score_value.c_str()));
    }
    ASSERT_EQ(irs::doc_limits::eof(), docs->value());
  }

  // column existence
  {
    irs::by_column_existence filter;
    *filter.mutable_field() = "seq";
    filter.mutable_options()->prefix_match = false;
    filter.boost(0.f);

    irs::bytes_ref actual_value;
    auto prepared_filter = filter.prepare(reader, prepared_order);
    auto docs = prepared_filter->execute(segment, prepared_order);
    auto* score = irs::get<irs::score>(*docs);
    ASSERT_TRUE(bool(score));
    ASSERT_TRUE(score->is_default());

    irs::doc_id_t doc = irs::doc_limits::min();
    while(docs->next()) {
      ASSERT_EQ(doc, docs->value());

      const irs::bytes_ref score_value(score->evaluate(), prepared_order.score_size());
      ASSERT_TRUE(values(docs->value(), actual_value));
      ++doc;
      ASSERT_EQ(0.f, *reinterpret_cast<const float_t*>(score_value.c_str()));
    }
    ASSERT_EQ(irs::doc_limits::eof(), docs->value());
  }
}

#ifndef IRESEARCH_DLL

TEST_P(tfidf_test, test_collector_serialization) {
  // initialize test data
  {
    tests::json_doc_generator gen(
      resource("simple_sequential.json"),
      &tests::payloaded_json_field_factory
    );
    auto writer = open_writer(irs::OM_CREATE);
    const document* doc;

    while ((doc = gen.next())) {
      ASSERT_TRUE(insert(
        *writer,
        doc->indexed.begin(), doc->indexed.end(),
        doc->stored.begin(), doc->stored.end()
      ));
    }

    writer->commit();
  }

  auto reader = irs::directory_reader::open(dir(), codec());
  ASSERT_EQ(1, reader.size());
  auto* field = reader[0].field("name");
  ASSERT_NE(nullptr, field);
  auto term = field->iterator();
  ASSERT_NE(nullptr, term);
  ASSERT_TRUE(term->next());
  term->read(); // fill term_meta
  irs::bstring fcollector_out;
  irs::bstring tcollector_out;

  // default init (field_collector)
  {
    irs::tfidf_sort sort;
    auto prepared_sort = sort.prepare();
    ASSERT_NE(nullptr, prepared_sort);
    auto collector = prepared_sort->prepare_field_collector();
    ASSERT_NE(nullptr, collector);
    bstring_data_output out0;
    collector->write(out0);
    collector->collect(reader[0], *field);
    bstring_data_output out1;
    collector->write(out1);
    fcollector_out = out1.out_;
    ASSERT_TRUE(out0.out_.size() != out1.out_.size() || 0 != std::memcmp(&out0.out_[0], &out1.out_[0], out0.out_.size()));

    // identical to default
    collector = prepared_sort->prepare_field_collector();
    collector->collect(out0.out_);
    bstring_data_output out2;
    collector->write(out2);
    ASSERT_TRUE(out0.out_.size() == out2.out_.size() && 0 == std::memcmp(&out0.out_[0], &out2.out_[0], out0.out_.size()));

    // identical to modified
    collector = prepared_sort->prepare_field_collector();
    collector->collect(out1.out_);
    bstring_data_output out3;
    collector->write(out3);
    ASSERT_TRUE(out1.out_.size() == out3.out_.size() && 0 == std::memcmp(&out1.out_[0], &out3.out_[0], out1.out_.size()));
  }

  // default init (term_collector)
  {
    irs::tfidf_sort sort;
    auto prepared_sort = sort.prepare();
    ASSERT_NE(nullptr, prepared_sort);
    auto collector = prepared_sort->prepare_term_collector();
    ASSERT_NE(nullptr, collector);
    bstring_data_output out0;
    collector->write(out0);
    collector->collect(reader[0], *field, *term);
    bstring_data_output out1;
    collector->write(out1);
    tcollector_out = out1.out_;
    ASSERT_TRUE(out0.out_.size() != out1.out_.size() || 0 != std::memcmp(&out0.out_[0], &out1.out_[0], out0.out_.size()));

    // identical to default
    collector = prepared_sort->prepare_term_collector();
    collector->collect(out0.out_);
    bstring_data_output out2;
    collector->write(out2);
    ASSERT_TRUE(out0.out_.size() == out2.out_.size() && 0 == std::memcmp(&out0.out_[0], &out2.out_[0], out0.out_.size()));

    // identical to modified
    collector = prepared_sort->prepare_term_collector();
    collector->collect(out1.out_);
    bstring_data_output out3;
    collector->write(out3);
    ASSERT_TRUE(out1.out_.size() == out3.out_.size() && 0 == std::memcmp(&out1.out_[0], &out3.out_[0], out1.out_.size()));
  }

  // serialized too short (field_collector)
  {
    irs::tfidf_sort sort;
    auto prepared_sort = sort.prepare();
    ASSERT_NE(nullptr, prepared_sort);
    auto collector = prepared_sort->prepare_field_collector();
    ASSERT_NE(nullptr, collector);
    ASSERT_THROW(collector->collect(irs::bytes_ref(&fcollector_out[0], fcollector_out.size() - 1)), irs::io_error);
  }

  // serialized too short (term_collector)
  {
    irs::tfidf_sort sort;
    auto prepared_sort = sort.prepare();
    ASSERT_NE(nullptr, prepared_sort);
    auto collector = prepared_sort->prepare_term_collector();
    ASSERT_NE(nullptr, collector);
    ASSERT_THROW(collector->collect(irs::bytes_ref(&tcollector_out[0], tcollector_out.size() - 1)), irs::io_error);
  }

  // serialized too long (field_collector)
  {
    irs::tfidf_sort sort;
    auto prepared_sort = sort.prepare();
    ASSERT_NE(nullptr, prepared_sort);
    auto collector = prepared_sort->prepare_field_collector();
    ASSERT_NE(nullptr, collector);
    auto out = fcollector_out;
    out.append(1, 42);
    ASSERT_THROW(collector->collect(out), irs::io_error);
  }

  // serialized too long (term_collector)
  {
    irs::tfidf_sort sort;
    auto prepared_sort = sort.prepare();
    ASSERT_NE(nullptr, prepared_sort);
    auto collector = prepared_sort->prepare_term_collector();
    ASSERT_NE(nullptr, collector);
    auto out = tcollector_out;
    out.append(1, 42);
    ASSERT_THROW(collector->collect(out), irs::io_error);
  }
}

TEST_P(tfidf_test, test_make) {
  // default values
  {
    auto scorer = irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), irs::string_ref::NIL);
    ASSERT_NE(nullptr, scorer);
    auto& scr = dynamic_cast<irs::tfidf_sort&>(*scorer);
    ASSERT_FALSE(scr.normalize());
    ASSERT_EQ(false, scr.use_boost_as_score());
  }

  // invalid args
  {
    auto scorer = irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), "\"12345");
    ASSERT_EQ(nullptr, scorer);
  }

  // custom value
  {
    auto scorer = irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), "true");
    ASSERT_NE(nullptr, scorer);
    auto& scr = dynamic_cast<irs::tfidf_sort&>(*scorer);
    ASSERT_EQ(true, scr.normalize());
    ASSERT_EQ(false, scr.use_boost_as_score());
  }

  // invalid value (non-bool)
  {
    auto scorer = irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), "42");
    ASSERT_EQ(nullptr, scorer);
  }

  // custom values
  {
    auto scorer = irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), "{\"withNorms\": true}");
    ASSERT_NE(nullptr, scorer);
    auto& scr = dynamic_cast<irs::tfidf_sort&>(*scorer);
    ASSERT_EQ(true, scr.normalize());
    ASSERT_EQ(false, scr.use_boost_as_score());
  }

  // invalid values (withNorms)
  {
    auto scorer = irs::scorers::get("tfidf", irs::type<irs::text_format::json>::get(), "{\"withNorms\": 42}");
    ASSERT_EQ(nullptr, scorer);
  }
}

TEST_P(tfidf_test, test_order) {
  {
    tests::json_doc_generator gen(
      resource("simple_sequential_order.json"),
      [](tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
        if (data.is_string()) { // field
          doc.insert(std::make_shared<templates::string_field>(name, data.str), true, false);
        } else if (data.is_number()) { // seq
          const auto value = std::to_string(data.as_number<uint64_t>());
          doc.insert(std::make_shared<templates::string_field>(name, value), false, true);
        }
    });
    add_segment(gen);
  }

  auto reader = iresearch::directory_reader::open(dir(), codec());
  auto& segment = *(reader.begin());

  irs::by_term query;
  *query.mutable_field() = "field";

  iresearch::order ord;
  ord.add<iresearch::tfidf_sort>(true, false, true);
  auto prepared_order = ord.prepare();

  auto comparer = [&prepared_order] (const iresearch::bstring& lhs, const iresearch::bstring& rhs) {
    return prepared_order.less(lhs.c_str(), rhs.c_str());
  };

  uint64_t seq = 0;
  const auto* column = segment.column_reader("seq");
  ASSERT_NE(nullptr, column);
  auto values = column->values();

  {
    query.mutable_options()->term = irs::ref_cast<irs::byte_type>(irs::string_ref("7"));

    std::multimap<iresearch::bstring, uint64_t, decltype(comparer)> sorted(comparer);
    std::vector<uint64_t> expected{ 0, 1, 5, 7 };

    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    auto prepared = query.prepare(reader, prepared_order);
    auto docs = prepared->execute(segment, prepared_order);
    auto* score = irs::get<iresearch::score>(*docs);
    ASSERT_TRUE(bool(score));

    for (; docs->next();) {
      ASSERT_TRUE(values(docs->value(), actual_value));
      in.reset(actual_value);

      auto str_seq = iresearch::read_string<std::string>(in);
      seq = strtoull(str_seq.c_str(), nullptr, 10);
      sorted.emplace(
        irs::bytes_ref(score->evaluate(), prepared_order.score_size()),
        seq);
    }

    ASSERT_EQ(expected.size(), sorted.size());
    const bool eq = std::equal(
      sorted.begin(), sorted.end(), expected.begin(),
      [](const std::pair<iresearch::bstring, uint64_t>& lhs, uint64_t rhs) {
        return lhs.second == rhs;
    });
    ASSERT_TRUE(eq);
  }
}

INSTANTIATE_TEST_CASE_P(
  tfidf_test,
  tfidf_test,
  ::testing::Combine(
    ::testing::Values(
      &tests::memory_directory,
      &tests::fs_directory,
      &tests::mmap_directory
    ),
    ::testing::Values("1_0")
  ),
  tests::to_string
);

#endif // IRESEARCH_DLL

} // namespace {
