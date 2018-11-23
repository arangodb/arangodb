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
#include "search/range_filter.hpp"
#include "store/memory_directory.hpp"
#include "formats/formats_10.hpp"

NS_BEGIN(tests)

class range_filter_test_case : public filter_test_case_base {
 protected:
  void by_range_sequential_numeric() {
    /* add segment */
    {
      tests::json_doc_generator gen(
        resource("simple_sequential.json"),
        [](tests::document& doc,
           const std::string& name,
           const tests::json_doc_generator::json_value& data) {
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
          } else if (data.is_number()){
            const double dValue = data.as_number<double_t>();
            // 'value' can be interpreted as a double
            {
              doc.insert(std::make_shared<tests::double_field>());
              auto& field = (doc.indexed.end() - 1).as<tests::double_field>();
              field.name(iresearch::string_ref(name));
              field.value(dValue);
            }

            const float fValue = data.as_number<float_t>();
            {
              // 'value' can be interpreted as a float 
              doc.insert(std::make_shared<tests::float_field>());
              auto& field = (doc.indexed.end() - 1).as<tests::float_field>();
              field.name(iresearch::string_ref(name));
              field.value(fValue);
            }

            uint64_t lValue = uint64_t(std::ceil(dValue));
            {
              doc.insert(std::make_shared<tests::long_field>());
              auto& field = (doc.indexed.end() - 1).as<tests::long_field>();
              field.name(iresearch::string_ref(name));
              field.value(lValue);
            }
            {
              doc.insert(std::make_shared<tests::int_field>());
              auto& field = (doc.indexed.end() - 1).as<tests::int_field>();
              field.name(iresearch::string_ref(name));
              field.value(int32_t(lValue));
            }
          }
        });
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

      irs::by_range query;
      query.field("seq")
           .include<irs::Bound::MIN>(true)
           .term<irs::Bound::MIN>(min_term->value())
           .include<irs::Bound::MAX>(true)
           .term<irs::Bound::MAX>(max_term->value());

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

      irs::by_range query;
      query.field("seq")
           .include<irs::Bound::MIN>(true)
           .term<irs::Bound::MIN>(min_term->value())
           .include<irs::Bound::MAX>(true)
           .term<irs::Bound::MAX>(max_term->value());

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

    // long - seq > 28 
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(INT64_C(28));
      auto& min_term = min_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(min_stream.next());

      irs::by_range query;
      query.field("seq")
           .include<irs::Bound::MIN>(false)
           .term<irs::Bound::MIN>(min_term->value())
           .include<irs::Bound::MAX>(true)
           .term<irs::Bound::MAX>((irs::numeric_utils::numeric_traits<int64_t>::max)());

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

    // long - seq <= 5 
    {
      irs::numeric_token_stream max_stream;
      max_stream.reset(INT64_C(5));
      auto& max_term = max_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(max_stream.next());

      irs::by_range query;
      query.field("seq")
           .include<irs::Bound::MIN>(true)
           .term<irs::Bound::MIN>((irs::numeric_utils::numeric_traits<int64_t>::min)())
           .include<irs::Bound::MAX>(true)
           .term<irs::Bound::MAX>(max_term->value());

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

      irs::by_range query;
      query.field("seq")
           .include<irs::Bound::MIN>(true)
           .term<irs::Bound::MIN>(min_term->value())
           .include<irs::Bound::MAX>(true)
           .term<irs::Bound::MAX>(max_term->value());

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

      irs::by_range query;
      query.field("seq")
           .include<irs::Bound::MIN>(true)
           .term<irs::Bound::MIN>(min_term->value())
           .include<irs::Bound::MAX>(true)
           .term<irs::Bound::MAX>(max_term->value());

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

    // int - seq > 28 
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(INT32_C(28));
      auto& min_term = min_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(min_stream.next());

      irs::by_range query;
      query.field("seq")
           .include<irs::Bound::MIN>(false)
           .term<irs::Bound::MIN>(min_term->value())
           .include<irs::Bound::MAX>(true)
           .term<irs::Bound::MAX>((irs::numeric_utils::numeric_traits<int32_t>::max)());

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

    // int - seq <= 5 
    {
      irs::numeric_token_stream max_stream;
      max_stream.reset(INT32_C(5));
      auto& max_term = max_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(max_stream.next());

      irs::by_range query;
      query.field("seq")
           .include<irs::Bound::MIN>(true)
           .term<irs::Bound::MIN>((irs::numeric_utils::numeric_traits<int32_t>::min)())
           .include<irs::Bound::MAX>(true)
           .term<irs::Bound::MAX>(max_term->value());

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

      irs::by_range query;
      query.field("value")
           .include<irs::Bound::MIN>(true)
           .term<irs::Bound::MIN>(min_term->value())
           .include<irs::Bound::MAX>(true)
           .term<irs::Bound::MAX>(max_term->value());

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

      irs::by_range query;
      query.field("value")
           .include<irs::Bound::MIN>(true)
           .term<irs::Bound::MIN>(min_term->value())
           .include<irs::Bound::MAX>(false)
           .term<irs::Bound::MAX>(max_term->value());

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

    // float - value < 91.565
    {
      irs::numeric_token_stream max_stream;
      max_stream.reset((float_t)90.565f);
      auto& max_term = max_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(max_stream.next());

      irs::by_range query;
      query.field("value")
           .include<irs::Bound::MIN>(true)
           .term<irs::Bound::MIN>(irs::numeric_utils::numeric_traits<float_t>::ninf())
           .include<irs::Bound::MAX>(false)
           .term<irs::Bound::MAX>(max_term->value());

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

      irs::by_range query;
      query.field("value")
           .include<irs::Bound::MIN>(false)
           .term<irs::Bound::MIN>(min_term->value())
           .include<irs::Bound::MAX>(true)
           .term<irs::Bound::MAX>(irs::numeric_utils::numeric_traits<float_t>::inf());

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

    // double - value = [123..123]
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset((double_t)123.);
      auto& min_term = min_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(min_stream.next());
      irs::numeric_token_stream max_stream;
      max_stream.reset((double_t)123.);
      auto& max_term = max_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(max_stream.next());

      irs::by_range query;
      query.field("value")
           .include<irs::Bound::MIN>(true)
           .term<irs::Bound::MIN>(min_term->value())
           .include<irs::Bound::MAX>(true)
           .term<irs::Bound::MAX>(max_term->value());

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

      irs::by_range query;
      query.field("value")
           .include<irs::Bound::MIN>(false)
           .term<irs::Bound::MIN>(min_term->value())
           .include<irs::Bound::MAX>(true)
           .term<irs::Bound::MAX>(max_term->value());

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

    // double - value < 5; 
    {
      irs::numeric_token_stream max_stream;
      max_stream.reset((double_t)5.);
      auto& max_term = max_stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(max_stream.next());

      irs::by_range query;
      query.field("value")
           .include<irs::Bound::MIN>(false)
           .term<irs::Bound::MIN>(irs::numeric_utils::numeric_traits<double_t>::ninf())
           .include<irs::Bound::MAX>(false)
           .term<irs::Bound::MAX>(max_term->value());

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

      irs::by_range query;
      query.field("value")
           .include<irs::Bound::MIN>(false)
           .term<irs::Bound::MIN>(min_term->value())
           .include<irs::Bound::MAX>(true)
           .term<irs::Bound::MAX>(irs::numeric_utils::numeric_traits<double_t>::inf());

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
  }

  void by_range_sequential_cost() {
    /* add segment */
    {
      tests::json_doc_generator gen(
        resource("simple_sequential.json"),
        &tests::generic_json_field_factory);
      add_segment( gen );
    }

    auto rdr = open_reader();

    // empty query
    check_query(irs::by_range(), docs_t{}, rdr);

    // name = (..;..)
    {
      docs_t docs{ 
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
      };
      costs_t costs{ docs.size() };

      check_query(irs::by_range().field("name"), docs, costs, rdr);
    }

    // invalid_name = (..;..)
    check_query(
      irs::by_range().field("invalid_name"), docs_t{}, rdr);

    // name = [..;..)
    {
      docs_t docs{ 
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
      };
      costs_t costs{ docs.size() };

      check_query(irs::by_range().field("name"), docs, costs, rdr);
    }

    // name = (..;..]
    {
      docs_t docs{ 
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
      };
      costs_t costs{ docs.size() };

      check_query(irs::by_range().field("name"), docs, costs, rdr);
    }

    // name = [..;..]
    {
      docs_t docs{ 
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
      };
      costs_t costs{ docs.size() };

      check_query(irs::by_range().field("name"), docs, costs, rdr);
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
        irs::by_range().field("name")
          .term<irs::Bound::MIN>("A").include<irs::Bound::MIN>(false),
        docs, costs, rdr
      );
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
        irs::by_range().field("name")
          .term<irs::Bound::MIN>("A").include<irs::Bound::MIN>(true),
        docs, costs, rdr
      );
    }

    // name = (..;C)
    // result: A, B, !, @, #, $, %
    {

      docs_t docs{ 1, 2, 28, 29, 30, 31, 32 };
      costs_t costs{ docs.size() };

      check_query(
        irs::by_range().field("name")
          .term<irs::Bound::MAX>("C").include<irs::Bound::MAX>(false),
        docs, costs, rdr
      );
    }

    // name = (..;C]
    // result: A, B, C, !, @, #, $, %
    {
      docs_t docs{ 1, 2, 3, 28, 29, 30, 31, 32 };
      costs_t costs{ docs.size() };

      check_query(
        irs::by_range().field("name")
          .term<irs::Bound::MAX>("C").include<irs::Bound::MAX>(true),
        docs, costs, rdr
      );
    }

    // name = [A;C] 
    // result: A, B, C
    {
      docs_t docs{ 1, 2, 3 };
      costs_t costs{ docs.size() };

      check_query(
        irs::by_range().field("name")
          .term<irs::Bound::MIN>("A").include<irs::Bound::MIN>(true)
          .term<irs::Bound::MAX>("C").include<irs::Bound::MAX>(true),
        docs, costs, rdr
      );
    }

    // name = [A;B]
    // result: A, B
    {
      docs_t docs{ 1, 2 };
      costs_t costs{ docs.size() };

      check_query(
        irs::by_range().field("name")
          .term<irs::Bound::MIN>("A").include<irs::Bound::MIN>(true)
          .term<irs::Bound::MAX>("B").include<irs::Bound::MAX>(true),
        docs, costs, rdr
      );
    }

    // name = [A;B)
    // result: A
    {
      docs_t docs{ 1 };
      costs_t costs{ docs.size() };

      check_query(
        irs::by_range().field("name")
          .term<irs::Bound::MIN>("A").include<irs::Bound::MIN>(true)
          .term<irs::Bound::MAX>("B").include<irs::Bound::MAX>(false),
        docs, costs, rdr
      );
    }

    // name = (A;B]
    // result: A
    {
      docs_t docs{ 2 };
      costs_t costs{ docs.size() };

      check_query(
        irs::by_range().field("name")
          .term<irs::Bound::MIN>("A").include<irs::Bound::MIN>(false)
          .term<irs::Bound::MAX>("B").include<irs::Bound::MAX>(true),
        docs, costs, rdr
      );
    }

    // name = (A;B)
    // result:
    check_query(
      irs::by_range().field("name")
        .term<irs::Bound::MIN>("A").include<irs::Bound::MIN>(false)
        .term<irs::Bound::MAX>("B").include<irs::Bound::MAX>(false),
      docs_t{}, costs_t{0}, rdr
    );


    // name = [A;C)
    // result: A, B
    {
      docs_t docs{ 1, 2 };
      costs_t costs{ docs.size() };

      check_query(
        irs::by_range().field("name")
          .term<irs::Bound::MIN>("A").include<irs::Bound::MIN>(true)
          .term<irs::Bound::MAX>("C").include<irs::Bound::MAX>(false),
        docs, costs, rdr
      );
    }

    // name = (A;C]
    // result: B, C
    {
      docs_t docs{ 2, 3 };
      costs_t costs{ docs.size() };

      check_query(
        irs::by_range().field("name")
          .term<irs::Bound::MIN>("A").include<irs::Bound::MIN>(false)
          .term<irs::Bound::MAX>("C").include<irs::Bound::MAX>(true),
        docs, costs, rdr
      );
    }

    // name = (A;C)
    // result: B
    {
      docs_t docs{ 2 };
      costs_t costs{ docs.size() };

      check_query(
        irs::by_range().field("name")
          .term<irs::Bound::MIN>("A").include<irs::Bound::MIN>(false)
          .term<irs::Bound::MAX>("C").include<irs::Bound::MAX>(false),
        docs, costs, rdr
      );
    }

    // name = [C;A]
    // result:
    check_query(
      irs::by_range().field("name")
        .term<irs::Bound::MIN>("C").include<irs::Bound::MIN>(true)
        .term<irs::Bound::MAX>("A").include<irs::Bound::MAX>(true),
      docs_t{}, costs_t{0}, rdr
    );

    // name = [~;..]
    // result: ~
    {
      docs_t docs{ 27 };
      costs_t costs{ docs.size() };

      check_query(
        irs::by_range().field("name")
          .term<irs::Bound::MIN>("~").include<irs::Bound::MIN>(true),
        docs, costs, rdr
      );
    }

    // name = (~;..]
    // result:
    check_query(
      irs::by_range().field("name")
        .term<irs::Bound::MIN>("~").include<irs::Bound::MIN>(false),
      docs_t{}, costs_t{0}, rdr
    );

    // name = [a;..]
    // result: ~
    {
      docs_t docs{ 27 };
      costs_t costs{ 1 };

      check_query(
        irs::by_range().field("name")
          .term<irs::Bound::MIN>("a").include<irs::Bound::MIN>(false),
        docs, costs, rdr
      );
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
        irs::by_range().field("name")
          .term<irs::Bound::MAX>("a").include<irs::Bound::MAX>(true),
        docs, costs, rdr
      );
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
        irs::by_range().field("name")
          .term<irs::Bound::MAX>("a").include<irs::Bound::MAX>(false),
        docs, costs, rdr
      );
    }

    // name = [DEL;..]
    // result:
    check_query(
      irs::by_range().field("name")
        .term<irs::Bound::MIN>("\x7f").include<irs::Bound::MIN>(false),
      docs_t{}, costs_t{0}, rdr
    );
  }

  void by_range_sequential_order() {
    // add segment
    {
      tests::json_doc_generator gen(
        resource("simple_sequential.json"),
        &tests::generic_json_field_factory
      );
      add_segment(gen);
    }

    auto rdr = open_reader();

    // empty query
    check_query(irs::by_range(), docs_t{}, rdr);

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
        irs::by_range()
          .field("value")
          .term<irs::Bound::MIN>(irs::numeric_utils::numeric_traits<double_t>::ninf())
          .term<irs::Bound::MAX>(irs::numeric_utils::numeric_traits<double_t>::inf())
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
        irs::by_range()
          .field("value")
          .term<irs::Bound::MIN>(irs::numeric_utils::numeric_traits<double_t>::ninf())
          .term<irs::Bound::MAX>(irs::numeric_utils::numeric_traits<double_t>::inf())
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
        irs::by_range()
          .field("value")
          .term<irs::Bound::MIN>(irs::numeric_utils::numeric_traits<double_t>::ninf())
          .term<irs::Bound::MAX>(irs::numeric_utils::numeric_traits<double_t>::inf())
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
        irs::by_range()
          .field("value")
          .term<irs::Bound::MIN>(irs::numeric_utils::numeric_traits<double_t>::ninf())
          .term<irs::Bound::MAX>(max_term->value())
        , order, docs, rdr
      );
    }
  }
}; // range_filter_test_case 

NS_END // tests

// ----------------------------------------------------------------------------
// --SECTION--                                              by_range base tests 
// ----------------------------------------------------------------------------

TEST(by_range_test, ctor) {
  irs::by_range q;
  ASSERT_EQ(irs::by_range::type(), q.type());
  ASSERT_TRUE(q.term<irs::Bound::MIN>().empty());
  ASSERT_TRUE(q.term<irs::Bound::MAX>().empty());
  ASSERT_FALSE(q.include<irs::Bound::MAX>());
  ASSERT_FALSE(q.include<irs::Bound::MIN>());
  ASSERT_EQ(irs::boost::no_boost(), q.boost());
}

TEST(by_range_test, equal) { 
  irs::by_range q;
  q.field("field")
    .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("min_term")
    .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("max_term");

  ASSERT_EQ(
    q, irs::by_range().field("field")
         .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("min_term")
         .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("max_term")
  );
  
  ASSERT_NE(
    q, irs::by_range().field("field1")
         .include<irs::Bound::MIN>(false).term<irs::Bound::MIN>("min_term")
         .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("max_term")
  );
}

TEST(by_range_test, boost) {
  // no boost
  {
    irs::by_range q;
    q.field("field")
      .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("min_term")
      .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("max_term");

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(irs::boost::no_boost(), irs::boost::extract(prepared->attributes()));
  }

  // with boost
  {
    iresearch::boost::boost_t boost = 1.5f;
    irs::by_range q;
    q.field("field")
      .include<irs::Bound::MIN>(true).term<irs::Bound::MIN>("min_term")
      .include<irs::Bound::MAX>(true).term<irs::Bound::MAX>("max_term");
    q.boost(boost);

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(boost, irs::boost::extract(prepared->attributes()));
  }
}

// ----------------------------------------------------------------------------
// --SECTION--                           memory_directory + iresearch_format_10
// ----------------------------------------------------------------------------

class memory_range_filter_test_case : public tests::range_filter_test_case {
protected:
  virtual irs::directory* get_directory() override {
    return new irs::memory_directory();
  }

  virtual irs::format::ptr get_codec() override {
    return irs::formats::get("1_0");
  }
};

TEST_F(memory_range_filter_test_case, by_range) {
  by_range_sequential_cost();
}

TEST_F(memory_range_filter_test_case, by_range_numeric) {
  by_range_sequential_numeric();
}

TEST_F(memory_range_filter_test_case, by_range_order) {
  by_range_sequential_order();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
