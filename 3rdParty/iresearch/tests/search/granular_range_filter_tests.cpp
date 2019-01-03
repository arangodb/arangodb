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
#include "filter_test_case_base.hpp"
#include "formats/formats_10.hpp"
#include "search/granular_range_filter.hpp"
#include "store/memory_directory.hpp"

NS_BEGIN(tests)

class granular_float_field: public float_field {
 public:
  const irs::flags& features() const {
    static const irs::flags features{ irs::granularity_prefix::type() };
    return features;
  }
};

class granular_double_field: public double_field {
 public:
  const irs::flags& features() const {
    static const irs::flags features{ irs::granularity_prefix::type() };
    return features;
  }
};

class granular_int_field: public int_field {
 public:
  const irs::flags& features() const {
    static const irs::flags features{ irs::granularity_prefix::type() };
    return features;
  }
};

class granular_long_field: public long_field {
 public:
  const irs::flags& features() const {
    static const irs::flags features{ irs::granularity_prefix::type() };
    return features;
  }
};

class granular_range_filter_test_case: public filter_test_case_base {
 protected:
  static void by_range_json_field_factory(
    tests::document& doc,
    const std::string& name,
    const json_doc_generator::json_value& data
  ) {
    if (data.is_string()) {
      doc.insert(std::make_shared<tests::templates::string_field>(
        irs::string_ref(name),
        data.str
      ));
    } else if (data.is_null()) {
      doc.insert(std::make_shared<tests::binary_field>());
      auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
      field.name(iresearch::string_ref(name));
      field.value(irs::null_token_stream::value_null());
    } else if (data.is_bool() && data.b) {
      doc.insert(std::make_shared<tests::binary_field>());
      auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
      field.name(iresearch::string_ref(name));
      field.value(irs::boolean_token_stream::value_true());
    } else if (data.is_bool() && !data.b) {
      doc.insert(std::make_shared<tests::binary_field>());
      auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
      field.name(iresearch::string_ref(name));
      field.value(irs::boolean_token_stream::value_true());
    } else if (data.is_number()) {
      // 'value' can be interpreted as a double
      const auto dValue = data.as_number<double_t>();
      {
        doc.insert(std::make_shared<granular_double_field>());
        auto& field = (doc.indexed.end() - 1).as<double_field>();
        field.name(iresearch::string_ref(name));
        field.value(dValue);
      }

      // 'value' can be interpreted as a float
      doc.insert(std::make_shared<granular_float_field>());
      auto& field = (doc.indexed.end() - 1).as<float_field>();
      field.name(iresearch::string_ref(name));
      field.value(data.as_number<float_t>());

      const uint64_t lValue = uint64_t(std::ceil(dValue));
      {
        doc.insert(std::make_shared<granular_long_field>());
        auto& field = (doc.indexed.end() - 1).as<long_field>();
        field.name(iresearch::string_ref(name));
        field.value(lValue);
      }

      {
        doc.insert(std::make_shared<granular_int_field>());
        auto& field = (doc.indexed.end() - 1).as<int_field>();
        field.name(iresearch::string_ref(name));
        field.value(int32_t(lValue));
      }
    }
  }

  void by_range_granularity_boost() {
    // add segment
    {
      tests::json_doc_generator gen(
        resource("granular_sequential.json"),
        &by_range_json_field_factory
      );
      add_segment(gen);
    }

    auto rdr = open_reader();
    ASSERT_EQ(1, rdr->size());

    auto& segment = (*rdr)[0];

    // without boost
    {
      irs::by_granular_range q;
      q.field("name")
       .include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>("A")
       .include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>("M");

      auto prepared = q.prepare(irs::sub_reader::empty());
      ASSERT_EQ(irs::boost::no_boost(), irs::boost::extract(prepared->attributes()));
    }

    // with boost
    {
      iresearch::boost::boost_t boost = 1.5f;
      irs::by_granular_range q;
      q.field("name")
       .include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>("A")
       .include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>("M");
      q.boost(boost);

      auto prepared = q.prepare(segment);
      ASSERT_EQ(boost, irs::boost::extract(prepared->attributes()));
    }
  }

  void by_range_granularity_level() {
    // add segment
    {
      tests::json_doc_generator gen(
        resource("granular_sequential.json"),
        &by_range_json_field_factory
      );
      add_segment(gen);
    }

    auto rdr = open_reader();

    // range under same granularity value for topmost element, (i.e. last value from numeric_token_stream)
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(INT32_C(0));

      irs::numeric_token_stream max_stream;
      max_stream.reset(INT32_C(1000));

      irs::by_granular_range query;
      query.field("value")
           .include<irs::Bound::MIN>(true)
           .insert<irs::Bound::MIN>(min_stream)
           .include<irs::Bound::MAX>(true)
           .insert<irs::Bound::MAX>(max_stream);
      ASSERT_EQ(2, query.size<irs::Bound::MIN>());
      ASSERT_EQ(2, query.size<irs::Bound::MAX>());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 1, 2, 3 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub);
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // range under different granularity value for topmost element, (i.e. last value from numeric_token_stream)
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(INT32_C(-1000));

      irs::numeric_token_stream max_stream;
      max_stream.reset(INT32_C(+1000));

      irs::by_granular_range query;
      query.field("value")
           .include<irs::Bound::MIN>(true)
           .insert<irs::Bound::MIN>(min_stream)
           .include<irs::Bound::MAX>(true)
           .insert<irs::Bound::MAX>(max_stream);
      ASSERT_EQ(2, query.size<irs::Bound::MIN>());
      ASSERT_EQ(2, query.size<irs::Bound::MAX>());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 1, 2, 3, 11, 12 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub);
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // double - value = [-20000..+20000]
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(double_t(-20000));

      irs::numeric_token_stream max_stream;
      max_stream.reset(double_t(+20000));

      irs::by_granular_range query;
      query.field("value")
           .include<irs::Bound::MIN>(true)
           .insert<irs::Bound::MIN>(min_stream)
           .include<irs::Bound::MAX>(true)
           .insert<irs::Bound::MAX>(max_stream);

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 1, 2, 3, 4, 5, 6, 7, 10, 11, 12 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub);
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // double - value > 100
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(double_t(100));

      irs::by_granular_range query;
      query.field("value")
           .include<irs::Bound::MIN>(false)
           .insert<irs::Bound::MIN>(min_stream)
           .include<irs::Bound::MAX>(true)
           .insert<irs::Bound::MAX>(irs::numeric_utils::numeric_traits<double_t>::inf());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 3, 4, 5, 6, 7, 8 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub);
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // double - value => 100
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(double_t(100));

      irs::by_granular_range query;
      query.field("value")
           .include<irs::Bound::MIN>(true)
           .insert<irs::Bound::MIN>(min_stream);

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 2, 3, 4, 5, 6, 7, 8 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub);
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // double - value => 20007 (largest value)
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(double_t(20007));

      irs::by_granular_range query;
      query.field("value")
           .include<irs::Bound::MIN>(true)
           .insert<irs::Bound::MIN>(min_stream);

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 8 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub);
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // double - value < 10000.123
    {
      irs::numeric_token_stream max_stream;
      max_stream.reset(double_t(10000.123));

      irs::by_granular_range query;
      query.field("value")
           .include<irs::Bound::MIN>(true)
           .insert<irs::Bound::MIN>(irs::numeric_utils::numeric_traits<double_t>::ninf())
           .include<irs::Bound::MAX>(false)
           .insert<irs::Bound::MAX>(max_stream);

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 1, 2, 3, 4, 9, 10, 11, 12 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub);
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // double - value <= 10000.123
    {
      irs::numeric_token_stream max_stream;
      max_stream.reset(double_t(10000.123));

      irs::by_granular_range query;
      query.field("value")
           .include<irs::Bound::MAX>(true)
           .insert<irs::Bound::MAX>(max_stream);

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 1, 2, 3, 4, 5, 9, 10, 11, 12 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub);
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // all documents
    {
      irs::by_granular_range query;
      query.field("value");

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub);
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }
  }

  void by_range_sequential_numeric() {
    // add segment
    {
      tests::json_doc_generator gen(
        resource("simple_sequential.json"),
        &by_range_json_field_factory
      );
      add_segment(gen);
    }

    auto rdr = open_reader();

    // long - seq = [7..7]
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(INT64_C(7));
      auto& min_term = min_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(min_stream.next());

      irs::numeric_token_stream max_stream;
      max_stream.reset(INT64_C(7));
      auto& max_term = max_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(max_stream.next());

      irs::by_granular_range query;
      query.field("seq")
           .include<irs::Bound::MIN>(true)
           .insert<irs::Bound::MIN>(min_term->value())
           .include<irs::Bound::MAX>(true)
           .insert<irs::Bound::MAX>(max_term->value());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 8 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub);
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // long - seq = [1..7]
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(INT64_C(1));
      auto& min_term = min_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(min_stream.next());

      irs::numeric_token_stream max_stream;
      max_stream.reset(INT64_C(7));
      auto& max_term = max_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(max_stream.next());

      irs::by_granular_range query;
      query.field("seq")
           .include<irs::Bound::MIN>(true)
           .insert<irs::Bound::MIN>(min_term->value())
           .include<irs::Bound::MAX>(true)
           .insert<irs::Bound::MAX>(max_term->value());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 2, 3, 4, 5, 6, 7, 8 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub);
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // long - value = [31 .. 32] with same-level granularity (last value in segment)
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(INT64_C(31));

      irs::numeric_token_stream max_stream;
      max_stream.reset(INT64_C(32));

      irs::by_granular_range query;
      query.field("seq")
        .include<irs::Bound::MIN>(true)
        .insert<irs::Bound::MIN>(min_stream)
        .include<irs::Bound::MAX>(true)
        .insert<irs::Bound::MAX>(max_stream);

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected{ 32 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub);
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // long - seq > 28
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(INT64_C(28));
      auto& min_term = min_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(min_stream.next());

      irs::by_granular_range query;
      query.field("seq")
           .include<irs::Bound::MIN>(false)
           .insert<irs::Bound::MIN>(min_term->value())
           .include<irs::Bound::MAX>(true)
           .insert<irs::Bound::MAX>((irs::numeric_utils::numeric_traits<int64_t>::max)());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 30, 31, 32 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // long - seq >= 31 (match largest value)
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(INT64_C(31));
      auto& min_term = min_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(min_stream.next());

      irs::by_granular_range query;
      query.field("seq")
           .include<irs::Bound::MIN>(true)
           .insert<irs::Bound::MIN>(min_term->value())
           .include<irs::Bound::MAX>(true)
           .insert<irs::Bound::MAX>((irs::numeric_utils::numeric_traits<int64_t>::max)());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 32 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub);
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // long - seq <= 5
    {
      irs::numeric_token_stream max_stream;
      max_stream.reset(INT64_C(5));
      auto& max_term = max_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(max_stream.next());

      irs::by_granular_range query;
      query.field("seq")
           .include<irs::Bound::MIN>(true)
           .insert<irs::Bound::MIN>((irs::numeric_utils::numeric_traits<int64_t>::min)())
           .include<irs::Bound::MAX>(true)
           .insert<irs::Bound::MAX>(max_term->value());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 1, 2, 3, 4, 5, 6 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // int - seq = [7..7]
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(INT32_C(7));
      auto& min_term = min_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(min_stream.next());

      irs::numeric_token_stream max_stream;
      max_stream.reset(INT32_C(7));
      auto& max_term = max_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(max_stream.next());

      irs::by_granular_range query;
      query.field("seq")
           .include<irs::Bound::MIN>(true)
           .insert<irs::Bound::MIN>(min_term->value())
           .include<irs::Bound::MAX>(true)
           .insert<irs::Bound::MAX>(max_term->value());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 8 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // int - seq = [1..7]
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(INT32_C(1));
      auto& min_term = min_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(min_stream.next());

      irs::numeric_token_stream max_stream;
      max_stream.reset(INT32_C(7));
      auto& max_term = max_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(max_stream.next());

      irs::by_granular_range query;
      query.field("seq")
           .include<irs::Bound::MIN>(true)
           .insert<irs::Bound::MIN>(min_term->value())
           .include<irs::Bound::MAX>(true)
           .insert<irs::Bound::MAX>(max_term->value());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 2, 3, 4, 5, 6, 7, 8 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // int - value = [31 .. 32] with same-level granularity (last value in segment)
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(INT32_C(31));

      irs::numeric_token_stream max_stream;
      max_stream.reset(INT32_C(32));

      irs::by_granular_range query;
      query.field("seq")
        .include<irs::Bound::MIN>(true)
        .insert<irs::Bound::MIN>(min_stream)
        .include<irs::Bound::MAX>(true)
        .insert<irs::Bound::MAX>(max_stream);

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected{ 32 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub);
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // int - seq > 28
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(INT32_C(28));
      auto& min_term = min_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(min_stream.next());

      irs::by_granular_range query;
      query.field("seq")
           .include<irs::Bound::MIN>(false)
           .insert<irs::Bound::MIN>(min_term->value())
           .include<irs::Bound::MAX>(true)
           .insert<irs::Bound::MAX>((irs::numeric_utils::numeric_traits<int32_t>::max)());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 30, 31, 32 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // int - seq >= 31 (match largest value)
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(INT32_C(31));
      auto& min_term = min_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(min_stream.next());

      irs::by_granular_range query;
      query.field("seq")
           .include<irs::Bound::MIN>(true)
           .insert<irs::Bound::MIN>(min_term->value())
           .include<irs::Bound::MAX>(true)
           .insert<irs::Bound::MAX>((irs::numeric_utils::numeric_traits<int32_t>::max)());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 32 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub);
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // int - seq <= 5
    {
      irs::numeric_token_stream max_stream;
      max_stream.reset(INT32_C(5));
      auto& max_term = max_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(max_stream.next());

      irs::by_granular_range query;
      query.field("seq")
           .include<irs::Bound::MIN>(true)
           .insert<irs::Bound::MIN>((irs::numeric_utils::numeric_traits<int32_t>::min)())
           .include<irs::Bound::MAX>(true)
           .insert<irs::Bound::MAX>(max_term->value());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 1, 2, 3, 4, 5, 6 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // float - value = [123..123]
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset((float_t)123.f);
      auto& min_term = min_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(min_stream.next());

      irs::numeric_token_stream max_stream;
      max_stream.reset((float_t)123.f);
      auto& max_term = max_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(max_stream.next());

      irs::by_granular_range query;
      query.field("value")
           .include<irs::Bound::MIN>(true)
           .insert<irs::Bound::MIN>(min_term->value())
           .include<irs::Bound::MAX>(true)
           .insert<irs::Bound::MAX>(max_term->value());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 3, 8 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // float - value = [91.524..123)
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset((float_t)91.524f);
      auto& min_term = min_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(min_stream.next());

      irs::numeric_token_stream max_stream;
      max_stream.reset((float_t)123.f);
      auto& max_term = max_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(max_stream.next());

      irs::by_granular_range query;
      query.field("value")
           .include<irs::Bound::MIN>(true)
           .insert<irs::Bound::MIN>(min_term->value())
           .include<irs::Bound::MAX>(false)
           .insert<irs::Bound::MAX>(max_term->value());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 1, 2, 5, 7, 9, 10, 12 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // float - value = [31 .. 32] with same-level granularity (last value in segment)
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(float_t(31));

      irs::numeric_token_stream max_stream;
      max_stream.reset(float_t(32));

      irs::by_granular_range query;
      query.field("seq")
        .include<irs::Bound::MIN>(true)
        .insert<irs::Bound::MIN>(min_stream)
        .include<irs::Bound::MAX>(true)
        .insert<irs::Bound::MAX>(max_stream);

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected{ 32 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub);
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // float - value < 91.565
    {
      irs::numeric_token_stream max_stream;
      max_stream.reset((float_t)90.565f);
      auto& max_term = max_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(max_stream.next());

      irs::by_granular_range query;
      query.field("value")
           .include<irs::Bound::MIN>(true)
           .insert<irs::Bound::MIN>(irs::numeric_utils::numeric_traits<float_t>::ninf())
           .include<irs::Bound::MAX>(false)
           .insert<irs::Bound::MAX>(max_term->value());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected{ 4, 11, 13, 14, 15, 16, 17 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // float - value > 91.565
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset((float_t)90.565f);
      auto& min_term = min_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(min_stream.next());

      irs::by_granular_range query;
      query.field("value")
           .include<irs::Bound::MIN>(false)
           .insert<irs::Bound::MIN>(min_term->value())
           .include<irs::Bound::MAX>(true)
           .insert<irs::Bound::MAX>(irs::numeric_utils::numeric_traits<float_t>::inf());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected{ 1, 2, 3, 5, 6, 7, 8, 9, 10, 12 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // float - value >= 31 (largest value)
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(float_t(31));
      auto& min_term = min_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(min_stream.next());

      irs::by_granular_range query;
      query.field("seq")
           .include<irs::Bound::MIN>(true)
           .insert<irs::Bound::MIN>(min_term->value())
           .include<irs::Bound::MAX>(true)
           .insert<irs::Bound::MAX>(irs::numeric_utils::numeric_traits<float_t>::inf());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected{ 32 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub);
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // double - value = [123...123]
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset((double_t)123.);
      auto& min_term = min_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(min_stream.next());
      irs::numeric_token_stream max_stream;
      max_stream.reset((double_t)123.);
      auto& max_term = max_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(max_stream.next());

      irs::by_granular_range query;
      query.field("value")
           .include<irs::Bound::MIN>(true)
           .insert<irs::Bound::MIN>(min_term->value())
           .include<irs::Bound::MAX>(true)
           .insert<irs::Bound::MAX>(max_term->value());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected{ 3, 8 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // double - value = (-40; 90.564]
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset((double_t)-40.);
      auto& min_term = min_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(min_stream.next());
      irs::numeric_token_stream max_stream;
      max_stream.reset((double_t)90.564);
      auto& max_term = max_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(max_stream.next());

      irs::by_granular_range query;
      query.field("value")
           .include<irs::Bound::MIN>(false)
           .insert<irs::Bound::MIN>(min_term->value())
           .include<irs::Bound::MAX>(true)
           .insert<irs::Bound::MAX>(max_term->value());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected{ 4, 11, 13, 14, 15, 16, 17 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // double - value = [31 .. 32] with same-level granularity (last value in segment)
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(double_t(31));

      irs::numeric_token_stream max_stream;
      max_stream.reset(double_t(32));

      irs::by_granular_range query;
      query.field("seq")
        .include<irs::Bound::MIN>(true)
        .insert<irs::Bound::MIN>(min_stream)
        .include<irs::Bound::MAX>(true)
        .insert<irs::Bound::MAX>(max_stream);

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected{ 32 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub);
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // double - value < 5;
    {
      irs::numeric_token_stream max_stream;
      max_stream.reset((double_t)5.);
      auto& max_term = max_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(max_stream.next());

      irs::by_granular_range query;
      query.field("value")
           .include<irs::Bound::MIN>(false)
           .insert<irs::Bound::MIN>(irs::numeric_utils::numeric_traits<double_t>::ninf())
           .include<irs::Bound::MAX>(false)
           .insert<irs::Bound::MAX>(max_term->value());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected{ 14, 15, 17 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // double - value > 90.543; 
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset((double_t)90.543);
      auto& min_term = min_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(min_stream.next());

      irs::by_granular_range query;
      query.field("value")
           .include<irs::Bound::MIN>(false)
           .insert<irs::Bound::MIN>(min_term->value())
           .include<irs::Bound::MAX>(true)
           .insert<irs::Bound::MAX>(irs::numeric_utils::numeric_traits<double_t>::inf());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected{ 1, 2, 3, 5, 6, 7, 8, 9, 10, 12, 13 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // double - value >= 31 (largest value)
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(double_t(31));
      auto& min_term = min_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(min_stream.next());

      irs::by_granular_range query;
      query.field("seq")
           .include<irs::Bound::MIN>(true)
           .insert<irs::Bound::MIN>(min_term->value())
           .include<irs::Bound::MAX>(true)
           .insert<irs::Bound::MAX>(irs::numeric_utils::numeric_traits<double_t>::inf());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected{ 32 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub);
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }
  }

  void by_range_sequential_cost() {
    // add segment
    {
      tests::json_doc_generator gen(
        resource("simple_sequential.json"),
        &by_range_json_field_factory
      );
      add_segment( gen );
    }

    auto rdr = open_reader();

    // empty query
    check_query(irs::by_granular_range(), docs_t{}, rdr);

    // name = (..;..)
    {
      docs_t docs{
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
      };
      costs_t costs{ docs.size() };

      check_query(irs::by_granular_range().field("name"), docs, costs, rdr);
    }

    // invalid_name = (..;..)
    check_query(
      irs::by_granular_range().field("invalid_name"), docs_t{}, rdr);

    // name = [..;..)
    {
      docs_t docs{ 
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
      };
      costs_t costs{ docs.size() };

      check_query(
        irs::by_granular_range().field("name"),
        docs, costs, rdr);
    }

    // name = (..;..]
    {
      docs_t docs{
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
      };
      costs_t costs{ docs.size() };

      check_query(
        irs::by_granular_range().field("name"),
        docs, costs, rdr);
    }

    // name = [..;..]
    {
      docs_t docs{
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
      };
      costs_t costs{ docs.size() };

      check_query(
        irs::by_granular_range().field("name"),
        docs, costs, rdr);
    }

    // name = [A;..)
    // result: A .. Z, ~
    {
      docs_t docs{
        2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
        18, 19, 20, 21, 22, 23, 24, 25, 26, 27
      }; 
      costs_t costs{ docs.size() };

      check_query(
        irs::by_granular_range().field("name")
          .insert<irs::Bound::MIN>("A").include<irs::Bound::MIN>(false),
        docs, costs, rdr);
    }

    // name = (A;..)
    // result: A .. Z, ~
    {
      docs_t docs{
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27
      };
      costs_t costs{ docs.size() };

      check_query(
        irs::by_granular_range().field("name")
          .insert<irs::Bound::MIN>("A").include<irs::Bound::MIN>(true),
        docs, costs, rdr);
    }

    // name = (..;C)
    // result: A, B, !, @, #, $, %
    {

      docs_t docs{ 1, 2, 28, 29, 30, 31, 32 };
      costs_t costs{ docs.size() };

      check_query(
        irs::by_granular_range().field("name")
          .insert<irs::Bound::MAX>("C").include<irs::Bound::MAX>(false),
        docs, costs, rdr);
    }

    // name = (..;C]
    // result: A, B, C, !, @, #, $, %
    {
      docs_t docs{ 1, 2, 3, 28, 29, 30, 31, 32 };
      costs_t costs{ docs.size() };

      check_query(
        irs::by_granular_range().field("name")
          .insert<irs::Bound::MAX>("C").include<irs::Bound::MAX>(true),
        docs, costs, rdr);
    }

    // name = [A;C]
    // result: A, B, C
    {
      docs_t docs{ 1, 2, 3 };
      costs_t costs{ docs.size() };

      check_query(
        irs::by_granular_range().field("name")
          .insert<irs::Bound::MIN>("A").include<irs::Bound::MIN>(true)
          .insert<irs::Bound::MAX>("C").include<irs::Bound::MAX>(true),
        docs, costs, rdr);
    }

    // name = [A;B]
    // result: A, B
    {
      docs_t docs{ 1, 2 };
      costs_t costs{ docs.size() };

      check_query(
        irs::by_granular_range().field("name")
          .insert<irs::Bound::MIN>("A").include<irs::Bound::MIN>(true)
          .insert<irs::Bound::MAX>("B").include<irs::Bound::MAX>(true),
        docs, costs, rdr);
    }

    // name = [A;B)
    // result: A
    {
      docs_t docs{ 1 };
      costs_t costs{ docs.size() };

      check_query(
        irs::by_granular_range().field("name")
          .insert<irs::Bound::MIN>("A").include<irs::Bound::MIN>(true)
          .insert<irs::Bound::MAX>("B").include<irs::Bound::MAX>(false),
        docs, costs, rdr);
    }

    // name = (A;B]
    // result: A
    {
      docs_t docs{ 2 };
      costs_t costs{ docs.size() };

      check_query(
        irs::by_granular_range().field("name")
          .insert<irs::Bound::MIN>("A").include<irs::Bound::MIN>(false)
          .insert<irs::Bound::MAX>("B").include<irs::Bound::MAX>(true),
        docs, costs, rdr);
    }

    // name = (A;B)
    // result:
    check_query(
      irs::by_granular_range().field("name")
        .insert<irs::Bound::MIN>("A").include<irs::Bound::MIN>(false)
        .insert<irs::Bound::MAX>("B").include<irs::Bound::MAX>(false),
      docs_t{}, costs_t{0}, rdr);


    // name = [A;C)
    // result: A, B
    {
      docs_t docs{ 1, 2 };
      costs_t costs{ docs.size() };

      check_query(
        irs::by_granular_range().field("name")
          .insert<irs::Bound::MIN>("A").include<irs::Bound::MIN>(true)
          .insert<irs::Bound::MAX>("C").include<irs::Bound::MAX>(false),
        docs, costs, rdr);
    }

    // name = (A;C]
    // result: B, C
    {
      docs_t docs{ 2, 3 };
      costs_t costs{ docs.size() };

      check_query(
        irs::by_granular_range().field("name")
          .insert<irs::Bound::MIN>("A").include<irs::Bound::MIN>(false)
          .insert<irs::Bound::MAX>("C").include<irs::Bound::MAX>(true),
        docs, costs, rdr);
    }

    // name = (A;C)
    // result: B
    {
      docs_t docs{ 2 };
      costs_t costs{ docs.size() };

      check_query(
        irs::by_granular_range().field("name")
          .insert<irs::Bound::MIN>("A").include<irs::Bound::MIN>(false)
          .insert<irs::Bound::MAX>("C").include<irs::Bound::MAX>(false),
        docs, costs, rdr);
    }

    // name = [C;A]
    // result:
    check_query(
      irs::by_granular_range().field("name")
        .insert<irs::Bound::MIN>("C").include<irs::Bound::MIN>(true)
        .insert<irs::Bound::MAX>("A").include<irs::Bound::MAX>(true),
      docs_t{}, costs_t{0}, rdr);

    // name = [~;..]
    // result: ~
    {
      docs_t docs{ 27 };
      costs_t costs{ docs.size() };

      check_query(
        irs::by_granular_range().field("name")
          .insert<irs::Bound::MIN>("~").include<irs::Bound::MIN>(true),
        docs, costs, rdr);
    }

    // name = (~;..]
    // result:
    check_query(
      irs::by_granular_range().field("name")
        .insert<irs::Bound::MIN>("~").include<irs::Bound::MIN>(false),
      docs_t{}, costs_t{0}, rdr);

    // name = [a;..]
    // result: ~
    {
      docs_t docs{ 27 };
      costs_t costs{ 1 };

      check_query(
        irs::by_granular_range().field("name")
          .insert<irs::Bound::MIN>("a").include<irs::Bound::MIN>(false),
        docs, costs, rdr);
    }

    // name = [..;a]
    // result: !, @, #, $, %, A..Z
    {
      docs_t docs{
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 28, 29, 30, 31, 32
      };
      costs_t costs{ docs.size() };

      check_query(
        irs::by_granular_range().field("name")
          .insert<irs::Bound::MAX>("a").include<irs::Bound::MAX>(true),
        docs, costs, rdr);
    }

    // name = [..;a)
    // result: !, @, #, $, %, A..Z
    {
      docs_t docs{
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 28, 29, 30, 31, 32
      };
      costs_t costs{ docs.size() };

      check_query(
        irs::by_granular_range().field("name")
          .insert<irs::Bound::MAX>("a").include<irs::Bound::MAX>(false),
        docs, costs, rdr);
    }

    // name = [DEL;..]
    // result:
    check_query(
      irs::by_granular_range().field("name")
        .insert<irs::Bound::MIN>("\x7f").include<irs::Bound::MIN>(false),
      docs_t{}, costs_t{0}, rdr);
  }

  void by_range_sequential_order() {
    // add segment
    {
      tests::json_doc_generator gen(
        resource("simple_sequential.json"),
        &by_range_json_field_factory
      );
      add_segment(gen);
    }

    auto rdr = open_reader();

    // empty query
    check_query(irs::by_granular_range(), docs_t{}, rdr);

    // value = (..;..) test collector call count for field/term/finish
    {
      docs_t docs{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17 };
      costs_t costs{ docs.size() };
      irs::order order;

      size_t collect_count = 0;
      size_t finish_count = 0;
      auto& scorer = order.add<sort::custom_sort>(false);
      scorer.collector_collect = [&collect_count](const irs::sub_reader&, const irs::term_reader&, const irs::attribute_view&)->void{
        ++collect_count;
      };
      scorer.collector_finish = [&finish_count](irs::attribute_store&, const irs::index_reader&)->void{
        ++finish_count;
      };
      scorer.prepare_collector = [&scorer]()->irs::sort::collector::ptr{
        return irs::memory::make_unique<sort::custom_sort::prepared::collector>(scorer);
      };
      check_query(
        irs::by_granular_range()
          .field("value")
          .insert<irs::Bound::MIN>(irs::numeric_utils::numeric_traits<double_t>::ninf())
          .insert<irs::Bound::MAX>(irs::numeric_utils::numeric_traits<double_t>::inf())
        , order, docs, rdr
      );
      ASSERT_EQ(11, collect_count);
      ASSERT_EQ(11, finish_count); // 11 different terms
    }

    // value = (..;..)
    {
      docs_t docs{ 1, 5, 7, 9, 10, 3, 4, 8, 11, 2, 6, 12, 13, 14, 15, 16, 17 };
      costs_t costs{ docs.size() };
      irs::order order;

      order.add<sort::frequency_sort>(false);
      check_query(
        irs::by_granular_range()
          .field("value")
          .insert<irs::Bound::MIN>(irs::numeric_utils::numeric_traits<double_t>::ninf())
          .insert<irs::Bound::MAX>(irs::numeric_utils::numeric_traits<double_t>::inf())
        , order, docs, rdr
      );
    }

    // value = (..;..) + scored_terms_limit
    {
      docs_t docs{ 1, 5, 7, 9, 10, 3, 8, 2, 4, 6, 11, 12, 13, 14, 15, 16, 17 };
      costs_t costs{ docs.size() };
      irs::order order;

      order.add<sort::frequency_sort>(false);
      check_query(
        irs::by_granular_range()
          .field("value")
          .insert<irs::Bound::MIN>(irs::numeric_utils::numeric_traits<double_t>::ninf())
          .insert<irs::Bound::MAX>(irs::numeric_utils::numeric_traits<double_t>::inf())
          .scored_terms_limit(2)
        , order, docs, rdr
      );
    }

    // value = (..;100)
    {
      docs_t docs{ 4, 11, 12, 13, 14, 15, 16, 17 };
      costs_t costs{ docs.size() };
      irs::order order;
      irs::numeric_token_stream max_stream;
      max_stream.reset((double_t)100.);
      auto& max_term = max_stream.attributes().get<irs::term_attribute>();

      ASSERT_TRUE(max_stream.next());
      order.add<sort::frequency_sort>(false);
      check_query(
        irs::by_granular_range()
          .field("value")
          .insert<irs::Bound::MIN>(irs::numeric_utils::numeric_traits<double_t>::ninf())
          .insert<irs::Bound::MAX>(max_term->value())
        , order, docs, rdr
      );
    }
  }
}; // granular_range_filter_test_case

NS_END // tests

// ----------------------------------------------------------------------------
// --SECTION--                                     by_granular_range base tests
// ----------------------------------------------------------------------------

TEST(by_granular_range_test, ctor) {
  irs::by_granular_range q;
  ASSERT_EQ(irs::by_granular_range::type(), q.type());
  ASSERT_TRUE(q.term<irs::Bound::MIN>(0).empty());
  ASSERT_TRUE(q.term<irs::Bound::MAX>(0).empty());
  ASSERT_FALSE(q.include<irs::Bound::MAX>());
  ASSERT_FALSE(q.include<irs::Bound::MIN>());
  ASSERT_EQ(irs::boost::no_boost(), q.boost());
}

TEST(by_granular_range_test, equal) {
  irs::by_granular_range q;
  q.field("field")
   .include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>("min_term")
   .include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>("max_term");

  ASSERT_TRUE(
    q == irs::by_granular_range().field("field")
         .include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>("min_term")
         .include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>("max_term")
  );

  ASSERT_FALSE(
    q == irs::by_granular_range().field("field1")
         .include<irs::Bound::MIN>(false).insert<irs::Bound::MIN>("min_term")
         .include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>("max_term")
  );
}

TEST(by_granular_range_test, boost) {
  // no boost
  {
    irs::by_granular_range q;
    q.field("field")
     .include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>("min_term")
     .include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>("max_term");

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(irs::boost::no_boost(), irs::boost::extract(prepared->attributes()));
  }

  // with boost, empty query
  {
    iresearch::boost::boost_t boost = 1.5f;
    irs::by_granular_range q;
    q.field("field")
      .include<irs::Bound::MIN>(true).insert<irs::Bound::MIN>("min_term")
      .include<irs::Bound::MAX>(true).insert<irs::Bound::MAX>("max_term");
    q.boost(boost);

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(irs::boost::no_boost(), irs::boost::extract(prepared->attributes()));
  }
}

// ----------------------------------------------------------------------------
// --SECTION--                           memory_directory + iresearch_format_10
// ----------------------------------------------------------------------------

class memory_granular_range_filter_test_case: public tests::granular_range_filter_test_case {
protected:
  virtual irs::directory* get_directory() override {
    return new irs::memory_directory();
  }

  virtual irs::format::ptr get_codec() override {
    return irs::formats::get("1_0");
  }
};

TEST_F(memory_granular_range_filter_test_case, by_range) {
  by_range_sequential_cost();
}

TEST_F(memory_granular_range_filter_test_case, by_range_granularity) {
  by_range_granularity_level();
}

TEST_F(memory_granular_range_filter_test_case, by_range_granularity_boost) {
  by_range_granularity_boost();
}

TEST_F(memory_granular_range_filter_test_case, by_range_numeric) {
  by_range_sequential_numeric();
}

TEST_F(memory_granular_range_filter_test_case, by_range_order) {
  by_range_sequential_order();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
