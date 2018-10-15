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
#include "search/phrase_filter.hpp"
#include "search/range_filter.hpp"
#include "search/scorers.hpp"
#include "search/sort.hpp"
#include "search/score.hpp"
#include "search/term_filter.hpp"
#include "search/tfidf.hpp"
#include "utils/utf8_path.hpp"

NS_BEGIN(tests)

class tfidf_test: public index_test_base { 
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

TEST_F(tfidf_test, test_load) {
  irs::order order;
  auto scorer = irs::scorers::get("tfidf", irs::text_format::json, irs::string_ref::NIL);

  ASSERT_NE(nullptr, scorer);
  ASSERT_EQ(1, order.add(true, scorer).size());
}

#ifndef IRESEARCH_DLL

TEST_F(tfidf_test, make_from_bool) {
  // `with-norms` argument
  {
    auto scorer = irs::scorers::get("tfidf", irs::text_format::json, "true");
    ASSERT_NE(nullptr, scorer);
    auto& tfidf = dynamic_cast<irs::tfidf_sort&>(*scorer);
    ASSERT_EQ(true, tfidf.normalize());
  }

  // invalid `with-norms` argument
  ASSERT_EQ(nullptr, irs::scorers::get("tfidf", irs::text_format::json, "\"false\""));
  ASSERT_EQ(nullptr, irs::scorers::get("tfidf", irs::text_format::json, "null"));
  ASSERT_EQ(nullptr, irs::scorers::get("tfidf", irs::text_format::json, "1"));
}

TEST_F(tfidf_test, make_from_array) {
  // default args
  {
    auto scorer = irs::scorers::get("tfidf", irs::text_format::json, irs::string_ref::NIL);
    ASSERT_NE(nullptr, scorer);
    auto& tfidf = dynamic_cast<irs::tfidf_sort&>(*scorer);
    ASSERT_EQ(irs::tfidf_sort::WITH_NORMS(), tfidf.normalize());
  }

  // default args
  {
    auto scorer = irs::scorers::get("tfidf", irs::text_format::json, "[]");
    ASSERT_NE(nullptr, scorer);
    auto& tfidf = dynamic_cast<irs::tfidf_sort&>(*scorer);
    ASSERT_EQ(irs::tfidf_sort::WITH_NORMS(), tfidf.normalize());
  }

  // `with-norms` argument
  {
    auto scorer = irs::scorers::get("tfidf", irs::text_format::json, "[ true ]");
    ASSERT_NE(nullptr, scorer);
    auto& tfidf = dynamic_cast<irs::tfidf_sort&>(*scorer);
    ASSERT_EQ(true, tfidf.normalize());
  }

  // invalid `with-norms` argument
  ASSERT_EQ(nullptr, irs::scorers::get("tfidf", irs::text_format::json, "[ \"false\" ]"));
  ASSERT_EQ(nullptr, irs::scorers::get("tfidf", irs::text_format::json, "[ null]"));
  ASSERT_EQ(nullptr, irs::scorers::get("tfidf", irs::text_format::json, "[ 1 ]"));
  ASSERT_EQ(nullptr, irs::scorers::get("tfidf", irs::text_format::json, "[ {} ]"));
  ASSERT_EQ(nullptr, irs::scorers::get("tfidf", irs::text_format::json, "[ [] ]"));
}

#endif // IRESEARCH_DLL

TEST_F(tfidf_test, test_normalize_features) {
  // default norms
  {
    auto scorer = irs::scorers::get("tfidf", irs::text_format::json, irs::string_ref::NIL);
    ASSERT_NE(nullptr, scorer);
    auto prepared = scorer->prepare();
    ASSERT_NE(nullptr, prepared);
    ASSERT_EQ(irs::flags({irs::frequency::type()}), prepared->features());
  }

  // with norms (as args)
  {
    auto scorer = irs::scorers::get("tfidf", irs::text_format::json, "true");
    ASSERT_NE(nullptr, scorer);
    auto prepared = scorer->prepare();
    ASSERT_NE(nullptr, prepared);
    ASSERT_EQ(irs::flags({irs::frequency::type(), irs::norm::type()}), prepared->features());
  }

  // with norms
  {
    auto scorer = irs::scorers::get("tfidf", irs::text_format::json, "{\"with-norms\": true}");
    ASSERT_NE(nullptr, scorer);
    auto prepared = scorer->prepare();
    ASSERT_NE(nullptr, prepared);
    ASSERT_EQ(irs::flags({irs::frequency::type(), irs::norm::type()}), prepared->features());
  }

  // without norms (as args)
  {
    auto scorer = irs::scorers::get("tfidf", irs::text_format::json, "false");
    ASSERT_NE(nullptr, scorer);
    auto prepared = scorer->prepare();
    ASSERT_NE(nullptr, prepared);
    ASSERT_EQ(irs::flags({irs::frequency::type()}), prepared->features());
  }

  // without norms
  {
    auto scorer = irs::scorers::get("tfidf", irs::text_format::json, "{\"with-norms\": false}");
    ASSERT_NE(nullptr, scorer);
    auto prepared = scorer->prepare();
    ASSERT_NE(nullptr, prepared);
    ASSERT_EQ(irs::flags({irs::frequency::type()}), prepared->features());
  }
}

TEST_F(tfidf_test, test_phrase) {
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
  order.add(true, irs::scorers::get("tfidf", irs::text_format::json, irs::string_ref::NIL));
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
      "P", // jumps high jumps left jumps right jumps down jumps back
      "Q", // jumps high jumps left jumps right jumps down walks back
      "O", // jumps high jumps high hotdog
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

TEST_F(tfidf_test, test_query) {
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

  order.add(true, irs::scorers::get("tfidf", irs::text_format::json, irs::string_ref::NIL));

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

  // by_phrase
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
      auto& expected_entry = expected[i++];
      ASSERT_TRUE(
        sizeof(float_t) == entry.first.size()
        //&& expected_entry.first == *reinterpret_cast<const float_t*>(&entry.first[0])
      );
      ASSERT_EQ(expected_entry.second, entry.second);
    }
  }
}

#ifndef IRESEARCH_DLL

TEST_F(tfidf_test, test_make) {
  // default values
  {
    auto scorer = irs::scorers::get("tfidf", irs::text_format::json, irs::string_ref::NIL);
    ASSERT_NE(nullptr, scorer);
    auto& scr = dynamic_cast<irs::tfidf_sort&>(*scorer);
    ASSERT_FALSE(scr.normalize());
  }

  // invalid args
  {
    auto scorer = irs::scorers::get("tfidf", irs::text_format::json, "\"12345");
    ASSERT_EQ(nullptr, scorer);
  }

  // custom value
  {
    auto scorer = irs::scorers::get("tfidf", irs::text_format::json, "true");
    ASSERT_NE(nullptr, scorer);
    auto& scr = dynamic_cast<irs::tfidf_sort&>(*scorer);
    ASSERT_EQ(true, scr.normalize());
  }

  // invalid value (non-bool)
  {
    auto scorer = irs::scorers::get("tfidf", irs::text_format::json, "42");
    ASSERT_EQ(nullptr, scorer);
  }

  // custom values
  {
    auto scorer = irs::scorers::get("tfidf", irs::text_format::json, "{\"with-norms\": true}");
    ASSERT_NE(nullptr, scorer);
    auto& scr = dynamic_cast<irs::tfidf_sort&>(*scorer);
    ASSERT_EQ(true, scr.normalize());
  }

  // invalid values (with-norms)
  {
    auto scorer = irs::scorers::get("tfidf", irs::text_format::json, "{\"with-norms\": 42}");
    ASSERT_EQ(nullptr, scorer);
  }
}

TEST_F(tfidf_test, test_order) {
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
  ord.add<iresearch::tfidf_sort>(true);
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