//
// IResearch search engine 
// 
// Copyright (c) 2016 by EMC Corporation, All Rights Reserved
// 
// This software contains the intellectual property of EMC Corporation or is licensed to
// EMC Corporation from third parties. Use of this software and the intellectual property
// contained therein is expressly limited to the terms and conditions of the License
// Agreement under which it is provided by or on behalf of EMC.
// 

#include "tests_shared.hpp"
#include "index/index_tests.hpp"
#include "store/memory_directory.hpp"
#include "search/phrase_filter.hpp"
#include "search/range_filter.hpp"
#include "search/scorers.hpp"
#include "search/sort.hpp"
#include "search/score.hpp"
#include "search/bm25.hpp"
#include "search/term_filter.hpp"
#include "utils/utf8_path.hpp"

NS_BEGIN(tests)

class bm25_test: public index_test_base { 
 protected:
  virtual iresearch::directory* get_directory() {
    return new iresearch::memory_directory();
   }

  virtual iresearch::format::ptr get_codec() {
    return ir::formats::get("1_0");
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
  auto scorer = irs::scorers::get("bm25", irs::string_ref::nil);

  ASSERT_NE(nullptr, scorer);
  ASSERT_EQ(1, order.add(scorer).size());
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

  order.add(irs::scorers::get("bm25", irs::string_ref::nil));

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
    ASSERT_TRUE(score);

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
    ASSERT_TRUE(score);

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
//    ASSERT_TRUE(score);
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
      0, // 3.60357 = (log(8/(3+1))+1)*(1.2+1)*sqrt(1)/(sqrt(1)+1.2) + (log(8/(4+1))+1)*(1.2+1)*sqrt(3)/(sqrt(3)+1.2) + (log(8/(2+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0) + 1.2)
      7, // 3.45083 = (log(8/(3+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0)+1.2) + (log(8/(4+1))+1)*(1.2+1)*sqrt(1)/(sqrt(1)+1.2) + (log(8/(2+1))+1)*(1.2+1)*sqrt(1)/(sqrt(1) + 1.2)
      5, // 3.16315 = (log(8/(3+1))+1)*(1.2+1)*sqrt(1)/(sqrt(1)+1.2) + (log(8/(4+1))+1)*(1.2+1)*sqrt(1)/(sqrt(1)+1.2) + (log(8/(2+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0) + 1.2)
      3, // 1.98082 = (log(8/(3+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0)+1.2) + (log(8/(4+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0)+1.2) + (log(8/(2+1))+1)*(1.2+1)*sqrt(1)/(sqrt(1) + 1.2)
      1, // 1.74950 = (log(8/(3+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0)+1.2) + (log(8/(4+1))+1)*(1.2+1)*sqrt(2)/(sqrt(2)+1.2) + (log(8/(2+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0) + 1.2)
      2, // 1.69314 = (log(8/(3+1))+1)*(1.2+1)*sqrt(1)/(sqrt(1)+1.2) + (log(8/(4+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0)+1.2) + (log(8/(2+1))+1)*(1.2+1)*sqrt(0)/(sqrt(0) + 1.2)
    };

    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    auto prepared_filter = filter.prepare(reader, prepared_order);
    auto docs = prepared_filter->execute(segment, prepared_order);
    auto& score = docs->attributes().get<irs::score>();
    ASSERT_TRUE(score);

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

  order.add(irs::scorers::get("bm25", irs::string_ref::nil));

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
    ASSERT_TRUE(score);

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
  ord.add<iresearch::bm25_sort>();
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
    ASSERT_TRUE(score);

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
