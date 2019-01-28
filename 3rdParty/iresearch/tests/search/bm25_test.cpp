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
#include "store/memory_directory.hpp"
#include "search/boolean_filter.hpp"
#include "search/phrase_filter.hpp"
#include "search/prefix_filter.hpp"
#include "search/range_filter.hpp"
#include "search/scorers.hpp"
#include "search/sort.hpp"
#include "search/score.hpp"
#include "search/bm25.hpp"
#include "search/term_filter.hpp"
#include "utils/utf8_path.hpp"

NS_LOCAL

struct bstring_data_output: public data_output {
  irs::bstring out_;
  virtual void close() override {}
  virtual void write_byte(irs::byte_type b) override { write_bytes(&b, 1); }
  virtual void write_bytes(const irs::byte_type* b, size_t size) override {
    out_.append(b, size);
  }
};

NS_END

NS_BEGIN(tests)

class bm25_test: public index_test_base { 
 protected:
  virtual iresearch::directory* get_directory() {
    return new iresearch::memory_directory();
   }

  virtual iresearch::format::ptr get_codec() {
    return irs::formats::get("1_0");
  }
};

NS_END

using namespace tests;

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

TEST_F(bm25_test, test_load) {
  irs::order order;
  auto scorer = irs::scorers::get("bm25", irs::text_format::json, irs::string_ref::NIL);

  ASSERT_NE(nullptr, scorer);
  ASSERT_EQ(1, order.add(true, scorer).size());
}

#ifndef IRESEARCH_DLL

TEST_F(bm25_test, make_from_array) {
  // default args
  {
    auto scorer = irs::scorers::get("bm25", irs::text_format::json, irs::string_ref::NIL);
    ASSERT_NE(nullptr, scorer);
    auto& bm25 = dynamic_cast<irs::bm25_sort&>(*scorer);
    ASSERT_EQ(irs::bm25_sort::K(), bm25.k());
    ASSERT_EQ(irs::bm25_sort::B(), bm25.b());
  }

  // default args
  {
    auto scorer = irs::scorers::get("bm25", irs::text_format::json, "[]");
    ASSERT_NE(nullptr, scorer);
    auto& bm25 = dynamic_cast<irs::bm25_sort&>(*scorer);
    ASSERT_EQ(irs::bm25_sort::K(), bm25.k());
    ASSERT_EQ(irs::bm25_sort::B(), bm25.b());
  }

  // `k` argument
  {
    auto scorer = irs::scorers::get("bm25", irs::text_format::json, "[ 1.5 ]");
    ASSERT_NE(nullptr, scorer);
    auto& bm25 = dynamic_cast<irs::bm25_sort&>(*scorer);
    ASSERT_EQ(1.5f, bm25.k());
    ASSERT_EQ(irs::bm25_sort::B(), bm25.b());
  }

  // invalid `k` argument
  ASSERT_EQ(nullptr, irs::scorers::get("bm25", irs::text_format::json, "[ \"1.5\" ]"));
  ASSERT_EQ(nullptr, irs::scorers::get("bm25", irs::text_format::json, "[ \"abc\" ]"));
  ASSERT_EQ(nullptr, irs::scorers::get("bm25", irs::text_format::json, "[ null]"));
  ASSERT_EQ(nullptr, irs::scorers::get("bm25", irs::text_format::json, "[ true ]"));
  ASSERT_EQ(nullptr, irs::scorers::get("bm25", irs::text_format::json, "[ false ]"));
  ASSERT_EQ(nullptr, irs::scorers::get("bm25", irs::text_format::json, "[ {} ]"));
  ASSERT_EQ(nullptr, irs::scorers::get("bm25", irs::text_format::json, "[ [] ]"));

  // `b` argument
  {
    auto scorer = irs::scorers::get("bm25", irs::text_format::json, "[ 1.5, 1.7 ]");
    ASSERT_NE(nullptr, scorer);
    auto& bm25 = dynamic_cast<irs::bm25_sort&>(*scorer);
    ASSERT_EQ(1.5f, bm25.k());
    ASSERT_EQ(1.7f, bm25.b());
  }

  // invalid `b` argument
  ASSERT_EQ(nullptr, irs::scorers::get("bm25", irs::text_format::json, "[ 1.5, \"1.7\" ]"));
  ASSERT_EQ(nullptr, irs::scorers::get("bm25", irs::text_format::json, "[ 1.5, \"abc\" ]"));
  ASSERT_EQ(nullptr, irs::scorers::get("bm25", irs::text_format::json, "[ 1.5, null]"));
  ASSERT_EQ(nullptr, irs::scorers::get("bm25", irs::text_format::json, "[ 1.5, true ]"));
  ASSERT_EQ(nullptr, irs::scorers::get("bm25", irs::text_format::json, "[ 1.5, false ]"));
  ASSERT_EQ(nullptr, irs::scorers::get("bm25", irs::text_format::json, "[ 1.5, {} ]"));
  ASSERT_EQ(nullptr, irs::scorers::get("bm25", irs::text_format::json, "[ 1.5, [] ]"));
}

#endif // IRESEARCH_DLL

TEST_F(bm25_test, test_normalize_features) {
  // default norms
  {
    auto scorer = irs::scorers::get("bm25", irs::text_format::json, irs::string_ref::NIL);
    ASSERT_NE(nullptr, scorer);
    auto prepared = scorer->prepare();
    ASSERT_NE(nullptr, prepared);
    ASSERT_EQ(irs::flags({irs::frequency::type(), irs::norm::type()}), prepared->features());
  }

  // without norms (bm15)
  {
    auto scorer = irs::scorers::get("bm25", irs::text_format::json, "{\"b\": 0.0}");
    ASSERT_NE(nullptr, scorer);
    auto prepared = scorer->prepare();
    ASSERT_NE(nullptr, prepared);
    ASSERT_EQ(irs::flags({irs::frequency::type()}), prepared->features());
  }

  // without norms (bm15), integer argument
  {
    auto scorer = irs::scorers::get("bm25", irs::text_format::json, "{\"b\": 0}");
    ASSERT_NE(nullptr, scorer);
    auto prepared = scorer->prepare();
    ASSERT_NE(nullptr, prepared);
    ASSERT_EQ(irs::flags({irs::frequency::type()}), prepared->features());
  }
}

TEST_F(bm25_test, test_phrase) {
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
        static irs::flags features{ irs::frequency::type() };
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
  order.add(true, irs::scorers::get("bm25", irs::text_format::json, "{ \"b\" : 0 }"));
  auto prepared_order = order.prepare();

  auto comparer = [&prepared_order] (const iresearch::bstring& lhs, const iresearch::bstring& rhs) {
    return prepared_order.less(lhs.c_str(), rhs.c_str());
  };

  // read segment
  auto index = open_reader();
  ASSERT(1, index->size());
  auto& segment = *(index.begin());

  // "jumps high" with order
  {
    irs::by_phrase filter;
    filter.field("phrase_anl")
          .push_back("jumps")
          .push_back("high");

    std::multimap<irs::bstring, std::string, decltype(comparer)> sorted(comparer);

    std::vector<std::string> expected{
      "O", // jumps high jumps high hotdog
      "P", // jumps high jumps left jumps right jumps down jumps back
      "Q", // jumps high jumps left jumps right jumps down walks back
      "R"  // jumps high jumps left jumps right walks down walks back
    };

    auto prepared_filter = filter.prepare(*index, prepared_order);
    auto docs = prepared_filter->execute(segment, prepared_order);
    auto& score = docs->attributes().get<irs::score>();
    ASSERT_TRUE(bool(score));

    auto column = segment.column_reader("name");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    // ensure that we avoid COW for pre c++11 std::basic_string
    const irs::bytes_ref score_value = score->value();
    irs::bytes_ref key_value;

    while (docs->next()) {
      score->evaluate();
      ASSERT_TRUE(values(docs->value(), key_value));

      sorted.emplace(
        score_value,
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

TEST_F(bm25_test, test_query) {
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

  irs::order order;

  order.add(true, irs::scorers::get("bm25", irs::text_format::json, irs::string_ref::NIL));

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

    filter.field("field").term("7");

    std::multimap<irs::bstring, uint64_t, decltype(comparer)> sorted(comparer);
    std::vector<uint64_t> expected{ 0, 1, 5, 7 };

    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    auto prepared_filter = filter.prepare(reader, prepared_order);
    auto docs = prepared_filter->execute(segment, prepared_order);
    auto& score = docs->attributes().get<irs::score>();
    ASSERT_TRUE(bool(score));

    // ensure that we avoid COW for pre c++11 std::basic_string
    const irs::bytes_ref score_value = score->value();

    while(docs->next()) {
      score->evaluate();
      ASSERT_TRUE(values(docs->value(), actual_value));
      in.reset(actual_value);

      auto str_seq = irs::read_string<std::string>(in);
      auto seq = strtoull(str_seq.c_str(), nullptr, 10);
      sorted.emplace(score_value, seq);
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
    filter.field("field").term("6");

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
      auto& score = docs->attributes().get<irs::score>();
      ASSERT_TRUE(bool(score));

      // ensure that we avoid COW for pre c++11 std::basic_string
      const irs::bytes_ref score_value = score->value();

      while(docs->next()) {
        score->evaluate();
        ASSERT_TRUE(values(docs->value(), actual_value));
        in.reset(actual_value);

        auto str_seq = irs::read_string<std::string>(in);
        auto seq = strtoull(str_seq.c_str(), nullptr, 10);
        sorted.emplace(score_value, seq);
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
    filter.add<irs::by_term>().field("field").term("6"); // doc 0, 2, 5
    filter.add<irs::by_term>().field("field").term("8"); // doc 3, 7

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
      auto& score = docs->attributes().get<irs::score>();
      ASSERT_TRUE(bool(score));

      // ensure that we avoid COW for pre c++11 std::basic_string
      const irs::bytes_ref score_value = score->value();

      while(docs->next()) {
        score->evaluate();
        ASSERT_TRUE(values(docs->value(), actual_value));
        in.reset(actual_value);

        auto str_seq = irs::read_string<std::string>(in);
        auto seq = strtoull(str_seq.c_str(), nullptr, 10);
        sorted.emplace(score_value, seq);
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
    filter.field("prefix").term("");

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
      auto& score = docs->attributes().get<irs::score>();
      ASSERT_TRUE(bool(score));

      // ensure that we avoid COW for pre c++11 std::basic_string
      const irs::bytes_ref score_value = score->value();

      while(docs->next()) {
        score->evaluate();
        ASSERT_TRUE(values(docs->value(), actual_value));
        in.reset(actual_value);

        auto str_seq = irs::read_string<std::string>(in);
        auto seq = strtoull(str_seq.c_str(), nullptr, 10);
        sorted.emplace(score_value, seq);
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

    filter.field("field")
      .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("6")
      .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("8");

    std::multimap<irs::bstring, uint64_t, decltype(comparer)> sorted(comparer);
    std::vector<uint64_t> expected{ 0, 1, 5, 7 };

    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    auto prepared_filter = filter.prepare(reader, prepared_order);
    auto docs = prepared_filter->execute(segment, prepared_order);
    auto& score = docs->attributes().get<irs::score>();

    // ensure that we avoid COW for pre c++11 std::basic_string
    const irs::bytes_ref score_value = score->value();

    while(docs->next()) {
      score->evaluate();
      ASSERT_TRUE(values(docs->value(), actual_value));
      in.reset(actual_value);

      auto str_seq = irs::read_string<std::string>(in);
      auto seq = strtoull(str_seq.c_str(), nullptr, 10);
      sorted.emplace(score_value, seq);
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

    filter.field("field").scored_terms_limit(1)
      .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("8")
      .include<irs::Bound::MAX>(false).term<irs::Bound::MAX>("9");

    std::multimap<irs::bstring, uint64_t, decltype(comparer)> sorted(comparer);
    std::vector<uint64_t> expected{ 3, 7 };

    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    auto prepared_filter = filter.prepare(reader, prepared_order);
    auto docs = prepared_filter->execute(segment, prepared_order);
    auto& score = docs->attributes().get<irs::score>();
    ASSERT_TRUE(bool(score));

    // ensure that we avoid COW for pre c++11 std::basic_string
    const irs::bytes_ref score_value = score->value();

    while(docs->next()) {
      score->evaluate();
      ASSERT_TRUE(values(docs->value(), actual_value));
      in.reset(actual_value);

      auto str_seq = irs::read_string<std::string>(in);
      auto seq = strtoull(str_seq.c_str(), nullptr, 10);
      sorted.emplace(score_value, seq);
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry: sorted) {
      ASSERT_EQ(expected[i++], entry.second);
    }
  }

//FIXME!!!
//  // by_range single + scored_terms_limit(0)
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
//    auto& score = docs->attributes().get<irs::score>();
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

    filter.field("field")
      .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("6")
      .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("8");

    std::multimap<irs::bstring, uint64_t, decltype(comparer)> sorted(comparer);
    std::vector<uint64_t> expected{
      // FIXME the following calculation is based on old formula
      7, // 3.45083 = (log(8/(4+1))+1)*(1.2+1)*sqrt(1)/(sqrt(1)+1.2) + (log(8/(2+1))+1)*(1.2+1)*sqrt(1)/(sqrt(1) + 1.2)
      3, // 1.98083 = (log(8/(4+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0)+1.2) + (log(8/(2+1))+1)*(1.2+1)*sqrt(1)/(sqrt(0) + 1.2)
      0, // 1.91043 = (log(8/(4+1))+1)*(1.2+1)*sqrt(3)/(sqrt(3)+1.2) + (log(8/(2+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0) + 1.2)
      1, // 1.74950 = (log(8/(4+1))+1)*(1.2+1)*sqrt(2)/(sqrt(2)+1.2) + (log(8/(2+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0) + 1.2)
      5, // 1.47000 = (log(8/(4+1))+1)*(1.2+1)*sqrt(1)/(sqrt(1)+1.2) + (log(8/(2+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0) + 1.2)
    };

    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    auto prepared_filter = filter.prepare(reader, prepared_order);
    auto docs = prepared_filter->execute(segment, prepared_order);
    auto& score = docs->attributes().get<irs::score>();

    // ensure that we avoid COW for pre c++11 std::basic_string
    const irs::bytes_ref score_value = score->value();

    while(docs->next()) {
      score->evaluate();
      ASSERT_TRUE(values(docs->value(), actual_value));
      in.reset(actual_value);

      auto str_seq = irs::read_string<std::string>(in);
      auto seq = strtoull(str_seq.c_str(), nullptr, 10);
      sorted.emplace(score_value, seq);
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

    filter.field("field")
      .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("6")
      .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("8");

    std::multimap<irs::bstring, uint64_t, decltype(comparer)> sorted(comparer);
    std::vector<uint64_t> expected{
      // FIXME the following calculation is based on old formula
      7, // 3.45083 = (log(8/(3+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0)+1.2) + (log(8/(4+1))+1)*(1.2+1)*sqrt(1)/(sqrt(1)+1.2) + (log(8/(2+1))+1)*(1.2+1)*sqrt(1)/(sqrt(1) + 1.2)
      0, // 3.60357 = (log(8/(3+1))+1)*(1.2+1)*sqrt(1)/(sqrt(1)+1.2) + (log(8/(4+1))+1)*(1.2+1)*sqrt(3)/(sqrt(3)+1.2) + (log(8/(2+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0) + 1.2)
      5, // 3.16315 = (log(8/(3+1))+1)*(1.2+1)*sqrt(1)/(sqrt(1)+1.2) + (log(8/(4+1))+1)*(1.2+1)*sqrt(1)/(sqrt(1)+1.2) + (log(8/(2+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0) + 1.2)
      3, // 1.98082 = (log(8/(3+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0)+1.2) + (log(8/(4+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0)+1.2) + (log(8/(2+1))+1)*(1.2+1)*sqrt(1)/(sqrt(1) + 1.2)
      2, // 1.69314 = (log(8/(3+1))+1)*(1.2+1)*sqrt(1)/(sqrt(1)+1.2) + (log(8/(4+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0)+1.2) + (log(8/(2+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0) + 1.2)
      1, // 1.74950 = (log(8/(3+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0)+1.2) + (log(8/(4+1))+1)*(1.2+1)*sqrt(2)/(sqrt(2)+1.2) + (log(8/(2+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0) + 1.2)
    };

    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    auto prepared_filter = filter.prepare(reader, prepared_order);
    auto docs = prepared_filter->execute(segment, prepared_order);
    auto& score = docs->attributes().get<irs::score>();
    ASSERT_TRUE(bool(score));

    // ensure that we avoid COW for pre c++11 std::basic_string
    const irs::bytes_ref score_value = score->value();

    while(docs->next()) {
      score->evaluate();
      ASSERT_TRUE(values(docs->value(), actual_value));
      in.reset(actual_value);

      auto str_seq = irs::read_string<std::string>(in);
      auto seq = strtoull(str_seq.c_str(), nullptr, 10);
      sorted.emplace(score_value, seq);
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry: sorted) {
      ASSERT_EQ(expected[i++], entry.second);
    }
  }

  // by phrase
  {
    irs::by_phrase filter;

    filter.field("field").push_back("7");

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
    auto& score = docs->attributes().get<irs::score>();

    // ensure that we avoid COW for pre c++11 std::basic_string
    const irs::bytes_ref score_value = score->value();

    while(docs->next()) {
      score->evaluate();
      ASSERT_TRUE(values(docs->value(), actual_value));
      in.reset(actual_value);

      auto str_seq = irs::read_string<std::string>(in);
      auto seq = strtoull(str_seq.c_str(), nullptr, 10);
      sorted.emplace(score_value, seq);
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
}

TEST_F(bm25_test, test_query_norms) {
  {
    tests::json_doc_generator gen(
      resource("simple_sequential_order.json"),
      [](tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
        static irs::flags extra_features = { irs::norm::type() };

        if (data.is_string()) { // field
          doc.insert(std::make_shared<templates::string_field>(name, data.str, extra_features), true, false);
        } else if (data.is_number()) { // seq
          const auto value = std::to_string(data.as_number<uint64_t>());
          doc.insert(std::make_shared<templates::string_field>(name, value, extra_features), false, true);
        }
    });
    add_segment(gen);
  }

  irs::order order;

  order.add(true, irs::scorers::get("bm25", irs::text_format::json, irs::string_ref::NIL));

  auto prepared_order = order.prepare();
  auto comparer = [&prepared_order](const irs::bstring& lhs, const irs::bstring& rhs)->bool {
    return prepared_order.less(lhs.c_str(), rhs.c_str());
  };

  auto reader = iresearch::directory_reader::open(dir(), codec());
  auto& segment = *(reader.begin());
  const auto* column = segment.column_reader("seq");
  ASSERT_NE(nullptr, column);
  auto values = column->values();

  // by_range multiple
  {
    irs::by_range filter;

    filter.field("field")
      .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("6")
      .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("8");

    std::multimap<irs::bstring, uint64_t, decltype(comparer)> sorted(comparer);
    std::vector<uint64_t> expected{
      // FIXME the following calculation is based on old formula
      7, // 5.62794 = (log(8/(4+1))+1)*(1.2+1)*sqrt(1)/(sqrt(1)+1.2*(1-0.75+0.75*(1/sqrt(8))/(52/8))) + (log(8/(2+1))+1)*(1.2+1)*sqrt(1)/(sqrt(1)+1.2*(1-0.75+0.75*(1/sqrt(8))/(52/8)))
      3, // 3.22245 = (log(8/(4+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0)+1.2*(1-0.75+0.75*(1/sqrt(7))/(52/8))) + (log(8/(2+1))+1)*(1.2+1)*sqrt(1)/(sqrt(1)+1.2*(1-0.75+0.75*(1/sqrt(7))/(52/8)))
      0, // 2.68195 = (log(8/(4+1))+1)*(1.2+1)*sqrt(3)/(sqrt(3)+1.2*(1-0.75+0.75*(1/sqrt(6))/(52/8))) + (log(8/(2+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0)+1.2*(1-0.75+0.75*(1/sqrt(6))/(52/8)))
      1, // 2.60158 = (log(8/(4+1))+1)*(1.2+1)*sqrt(2)/(sqrt(2)+1.2*(1-0.75+0.75*(1/sqrt(10))/(52/8))) + (log(8/(2+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0)+1.2*(1-0.75+0.75*(1/sqrt(10))/(52/8)))
      5, // 2.39742 = (log(8/(4+1))+1)*(1.2+1)*sqrt(1)/(sqrt(1)+1.2*(1-0.75+0.75*(1/sqrt(8))/(52/8))) + (log(8/(2+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0)+1.2*(1-0.75+0.75*(1/sqrt(8))/(52/8)))
    };

    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    auto prepared_filter = filter.prepare(reader, prepared_order);
    auto docs = prepared_filter->execute(segment, prepared_order);
    auto& score = docs->attributes().get<irs::score>();
    ASSERT_TRUE(bool(score));

    // ensure that we avoid COW for pre c++11 std::basic_string
    const irs::bytes_ref score_value = score->value();

    while(docs->next()) {
      score->evaluate();
      ASSERT_TRUE(values(docs->value(), actual_value));
      in.reset(actual_value);

      auto str_seq = irs::read_string<std::string>(in);
      auto seq = strtoull(str_seq.c_str(), nullptr, 10);
      sorted.emplace(score_value, seq);
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

    filter.field("field")
      .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("6")
      .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("8");

    std::multimap<irs::bstring, uint64_t, decltype(comparer)> sorted(comparer);
    std::vector<uint64_t> expected{
      // FIXME the following calculation is based on old formula
      7, // 5.62794 = (log(8/(3+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0)+1.2*(1-0.75+0.75*(1/sqrt(8))/(52/8))) + (log(8/(4+1))+1)*(1.2+1)*sqrt(1)/(sqrt(1)+1.2*(1-0.75+0.75*(1/sqrt(8))/(52/8))) + (log(8/(2+1))+1)*(1.2+1)*sqrt(1)/(sqrt(1)+1.2*(1-0.75+0.75*(1/sqrt(8))/(52/8)))
      0, // 5.42788 = (log(8/(3+1))+1)*(1.2+1)*sqrt(1)/(sqrt(1)+1.2*(1-0.75+0.75*(1/sqrt(6))/(52/8))) + (log(8/(4+1))+1)*(1.2+1)*sqrt(3)/(sqrt(3)+1.2*(1-0.75+0.75*(1/sqrt(6))/(52/8))) + (log(8/(2+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0)+1.2*(1-0.75+0.75*(1/sqrt(6))/(52/8)))
      5, // 5.15876 = (log(8/(3+1))+1)*(1.2+1)*sqrt(1)/(sqrt(1)+1.2*(1-0.75+0.75*(1/sqrt(8))/(52/8))) + (log(8/(4+1))+1)*(1.2+1)*sqrt(1)/(sqrt(1)+1.2*(1-0.75+0.75*(1/sqrt(8))/(52/8))) + (log(8/(2+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0)+1.2*(1-0.75+0.75*(1/sqrt(8))/(52/8)))
      3, // 3.22245 = (log(8/(3+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0)+1.2*(1-0.75+0.75*(1/sqrt(7))/(52/8))) + (log(8/(4+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0)+1.2*(1-0.75+0.75*(1/sqrt(7))/(52/8))) + (log(8/(2+1))+1)*(1.2+1)*sqrt(1)/(sqrt(1)+1.2*(1-0.75+0.75*(1/sqrt(7))/(52/8)))
      2, // 2.73505 = (log(8/(3+1))+1)*(1.2+1)*sqrt(1)/(sqrt(1)+1.2*(1-0.75+0.75*(1/sqrt(5))/(52/8))) + (log(8/(4+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0)+1.2*(1-0.75+0.75*(1/sqrt(5))/(52/8))) + (log(8/(2+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0)+1.2*(1-0.75+0.75*(1/sqrt(5))/(52/8)))
      1, // 2.60158 = (log(8/(3+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0)+1.2*(1-0.75+0.75*(1/sqrt(10))/(52/8))) + (log(8/(4+1))+1)*(1.2+1)*sqrt(2)/(sqrt(2)+1.2*(1-0.75+0.75*(1/sqrt(10))/(52/8))) + (log(8/(2+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0)+1.2*(1-0.75+0.75*(1/sqrt(10))/(52/8)))
    };

    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    auto prepared_filter = filter.prepare(reader, prepared_order);
    auto docs = prepared_filter->execute(segment, prepared_order);
    auto& score = docs->attributes().get<irs::score>();

    // ensure that we avoid COW for pre c++11 std::basic_string
    const irs::bytes_ref score_value = score->value();

    while(docs->next()) {
      score->evaluate();
      ASSERT_TRUE(values(docs->value(), actual_value));
      in.reset(actual_value);

      auto str_seq = irs::read_string<std::string>(in);
      auto seq = strtoull(str_seq.c_str(), nullptr, 10);
      sorted.emplace(score_value, seq);
    }

    ASSERT_EQ(expected.size(), sorted.size());
    size_t i = 0;

    for (auto& entry: sorted) {
      ASSERT_EQ(expected[i++], entry.second);
    }
  }
}

#ifndef IRESEARCH_DLL

TEST_F(bm25_test, test_collector_serialization) {
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
    irs::bm25_sort sort;
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
    irs::bm25_sort sort;
    auto prepared_sort = sort.prepare();
    ASSERT_NE(nullptr, prepared_sort);
    auto collector = prepared_sort->prepare_term_collector();
    ASSERT_NE(nullptr, collector);
    bstring_data_output out0;
    collector->write(out0);
    collector->collect(reader[0], *field, term->attributes());
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
    irs::bm25_sort sort;
    auto prepared_sort = sort.prepare();
    ASSERT_NE(nullptr, prepared_sort);
    auto collector = prepared_sort->prepare_field_collector();
    ASSERT_NE(nullptr, collector);
    ASSERT_THROW(collector->collect(irs::bytes_ref(&fcollector_out[0], fcollector_out.size() - 1)), irs::io_error);
  }

  // serialized too short (term_collector)
  {
    irs::bm25_sort sort;
    auto prepared_sort = sort.prepare();
    ASSERT_NE(nullptr, prepared_sort);
    auto collector = prepared_sort->prepare_term_collector();
    ASSERT_NE(nullptr, collector);
    ASSERT_THROW(collector->collect(irs::bytes_ref(&tcollector_out[0], tcollector_out.size() - 1)), irs::io_error);
  }

  // serialized too long (field_collector)
  {
    irs::bm25_sort sort;
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
    irs::bm25_sort sort;
    auto prepared_sort = sort.prepare();
    ASSERT_NE(nullptr, prepared_sort);
    auto collector = prepared_sort->prepare_term_collector();
    ASSERT_NE(nullptr, collector);
    auto out = tcollector_out;
    out.append(1, 42);
    ASSERT_THROW(collector->collect(out), irs::io_error);
  }
}

TEST_F(bm25_test, test_make) {
  // default values
  {
    auto scorer = irs::scorers::get("bm25", irs::text_format::json, irs::string_ref::NIL);
    ASSERT_NE(nullptr, scorer);
    auto& scr = dynamic_cast<irs::bm25_sort&>(*scorer);
    ASSERT_EQ(0.75f, scr.b());
    ASSERT_EQ(1.2f, scr.k());
    ASSERT_FALSE(scr.bm11());
    ASSERT_FALSE(scr.bm15());
  }

  // custom values
  {
    auto scorer = irs::scorers::get("bm25", irs::text_format::json, "{\"b\": 123.456, \"k\": 78.9 }");
    ASSERT_NE(nullptr, scorer);
    auto& scr = dynamic_cast<irs::bm25_sort&>(*scorer);
    ASSERT_EQ(123.456f, scr.b());
    ASSERT_EQ(78.9f, scr.k());
    ASSERT_FALSE(scr.bm11());
    ASSERT_FALSE(scr.bm15());
  }

  // custom values (integer argument)
  {
    auto scorer = irs::scorers::get("bm25", irs::text_format::json, "{\"b\": 123.456, \"k\": 78 }");
    ASSERT_NE(nullptr, scorer);
    auto& scr = dynamic_cast<irs::bm25_sort&>(*scorer);
    ASSERT_EQ(123.456f, scr.b());
    ASSERT_EQ(78.0f, scr.k());
    ASSERT_FALSE(scr.bm11());
    ASSERT_FALSE(scr.bm15());
  }

  // bm11
  {
    auto scorer = irs::scorers::get("bm25", irs::text_format::json, "{\"b\": 1.0, \"k\": 78.9 }");
    ASSERT_NE(nullptr, scorer);
    auto& scr = dynamic_cast<irs::bm25_sort&>(*scorer);
    ASSERT_EQ(1.f, scr.b());
    ASSERT_EQ(78.9f, scr.k());
    ASSERT_TRUE(scr.bm11());
    ASSERT_FALSE(scr.bm15());
  }

  // bm11 (integer argument)
  {
    auto scorer = irs::scorers::get("bm25", irs::text_format::json, "{\"b\": 1, \"k\": 78.9 }");
    ASSERT_NE(nullptr, scorer);
    auto& scr = dynamic_cast<irs::bm25_sort&>(*scorer);
    ASSERT_EQ(1.f, scr.b());
    ASSERT_EQ(78.9f, scr.k());
    ASSERT_TRUE(scr.bm11());
    ASSERT_FALSE(scr.bm15());
  }

  // bm15
  {
    auto scorer = irs::scorers::get("bm25", irs::text_format::json, "{\"b\": 0.0, \"k\": 78.9 }");
    ASSERT_NE(nullptr, scorer);
    auto& scr = dynamic_cast<irs::bm25_sort&>(*scorer);
    ASSERT_EQ(0.f, scr.b());
    ASSERT_EQ(78.9f, scr.k());
    ASSERT_FALSE(scr.bm11());
    ASSERT_TRUE(scr.bm15());
  }

  // bm15 (integer argument)
  {
    auto scorer = irs::scorers::get("bm25", irs::text_format::json, "{\"b\": 0, \"k\": 78.9 }");
    ASSERT_NE(nullptr, scorer);
    auto& scr = dynamic_cast<irs::bm25_sort&>(*scorer);
    ASSERT_EQ(0.f, scr.b());
    ASSERT_EQ(78.9f, scr.k());
    ASSERT_FALSE(scr.bm11());
    ASSERT_TRUE(scr.bm15());
  }

  // invalid args
  {
    auto scorer = irs::scorers::get("bm25", irs::text_format::json, "\"12345");
    ASSERT_EQ(nullptr, scorer);
  }

  // invalid values (b)
  {
    auto scorer = irs::scorers::get("bm25", irs::text_format::json, "{\"b\": false, \"k\": 78.9}");
    ASSERT_EQ(nullptr, scorer);
  }

  // invalid values (k)
  {
    auto scorer = irs::scorers::get("bm25", irs::text_format::json, "{\"b\": 123.456, \"k\": true}");
    ASSERT_EQ(nullptr, scorer);
  }
}

TEST_F(bm25_test, test_order) {
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

  iresearch::by_term query;
  query.field("field");

  iresearch::order ord;
  ord.add<iresearch::bm25_sort>(true);
  auto prepared_order = ord.prepare();

  auto comparer = [&prepared_order] (const iresearch::bstring& lhs, const iresearch::bstring& rhs) {
    return prepared_order.less(lhs.c_str(), rhs.c_str());
  };

  uint64_t seq = 0;
  const auto* column = segment.column_reader("seq");
  ASSERT_NE(nullptr, column);
  auto values = column->values();

  {
    query.term("7");

    std::multimap<iresearch::bstring, uint64_t, decltype(comparer)> sorted(comparer);
    std::vector<uint64_t> expected{ 0, 1, 5, 7 };

    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    auto prepared = query.prepare(reader, prepared_order);
    auto docs = prepared->execute(segment, prepared_order);
    auto& score = docs->attributes().get<iresearch::score>();
    ASSERT_TRUE(bool(score));

    // ensure that we avoid COW for pre c++11 std::basic_string
    const irs::bytes_ref score_value = score->value();

    for (; docs->next();) {
      score->evaluate();
      ASSERT_TRUE(values(docs->value(), actual_value));
      in.reset(actual_value);

      auto str_seq = iresearch::read_string<std::string>(in);
      seq = strtoull(str_seq.c_str(), nullptr, 10);
      sorted.emplace(score_value, seq);
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

#endif // IRESEARCH_DLL

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------