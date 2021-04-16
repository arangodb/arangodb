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

namespace {

irs::by_range make_filter(
    const irs::string_ref& field,
    const irs::bytes_ref& min, irs::BoundType min_type,
    const irs::bytes_ref& max, irs::BoundType max_type) {
  irs::by_range filter;
  *filter.mutable_field() = field;

  auto& range = filter.mutable_options()->range;
  range.min = min;
  range.min_type = min_type;
  range.max = max;
  range.max_type = max_type;
  return filter;
}

class range_filter_test_case : public tests::filter_test_case_base {
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
              name,
              data.str
            ));
          } else if (data.is_null()) {
            doc.insert(std::make_shared<tests::binary_field>());
            auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
            field.name(name);
            field.value(irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null()));
          } else if (data.is_bool() && data.b) {
            doc.insert(std::make_shared<tests::binary_field>());
            auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
            field.name(name);
            field.value(irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true()));
          } else if (data.is_bool() && !data.b) {
            doc.insert(std::make_shared<tests::binary_field>());
            auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
            field.name(name);
            field.value(irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true()));
          } else if (data.is_number()){
            const double dValue = data.as_number<double_t>();
            // 'value' can be interpreted as a double
            {
              doc.insert(std::make_shared<tests::double_field>());
              auto& field = (doc.indexed.end() - 1).as<tests::double_field>();
              field.name(name);
              field.value(dValue);
            }

            const float fValue = data.as_number<float_t>();
            {
              // 'value' can be interpreted as a float 
              doc.insert(std::make_shared<tests::float_field>());
              auto& field = (doc.indexed.end() - 1).as<tests::float_field>();
              field.name(name);
              field.value(fValue);
            }

            uint64_t lValue = uint64_t(std::ceil(dValue));
            {
              doc.insert(std::make_shared<tests::long_field>());
              auto& field = (doc.indexed.end() - 1).as<tests::long_field>();
              field.name(name);
              field.value(lValue);
            }
            {
              doc.insert(std::make_shared<tests::int_field>());
              auto& field = (doc.indexed.end() - 1).as<tests::int_field>();
              field.name(name);
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
      auto* min_term = irs::get<irs::term_attribute>(min_stream);
      ASSERT_TRUE(min_stream.next());

      irs::numeric_token_stream max_stream;
      max_stream.reset(INT64_C(7));
      auto* max_term = irs::get<irs::term_attribute>(max_stream);
      ASSERT_TRUE(max_stream.next());

      irs::by_range query = make_filter(
        "seq",
        min_term->value, irs::BoundType::INCLUSIVE,
        max_term->value, irs::BoundType::INCLUSIVE);

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 8 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub);
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(docs->value(), doc->value);
        for (;docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // long - seq = [1..7]
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(INT64_C(1));
      auto* min_term = irs::get<irs::term_attribute>(min_stream);
      ASSERT_TRUE(min_stream.next());

      irs::numeric_token_stream max_stream;
      max_stream.reset(INT64_C(7));
      auto* max_term = irs::get<irs::term_attribute>(max_stream);
      ASSERT_TRUE(max_stream.next());

      irs::by_range query = make_filter(
        "seq",
         min_term->value, irs::BoundType::INCLUSIVE,
         max_term->value, irs::BoundType::INCLUSIVE);

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 2, 3, 4, 5, 6, 7, 8 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(docs->value(), doc->value);
        for (;docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // long - seq > 28 
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(INT64_C(28));
      auto* min_term = irs::get<irs::term_attribute>(min_stream);
      ASSERT_TRUE(min_stream.next());

      irs::by_range query = make_filter(
        "seq",
         min_term->value, irs::BoundType::EXCLUSIVE,
         (irs::numeric_utils::numeric_traits<int64_t>::max)(), irs::BoundType::INCLUSIVE);

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 30, 31, 32 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(docs->value(), doc->value);
        for (;docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // long - seq <= 5 
    {
      irs::numeric_token_stream max_stream;
      max_stream.reset(INT64_C(5));
      auto* max_term = irs::get<irs::term_attribute>(max_stream);
      ASSERT_TRUE(max_stream.next());

      irs::by_range query = make_filter(
        "seq",
        (irs::numeric_utils::numeric_traits<int64_t>::min)(), irs::BoundType::INCLUSIVE,
        max_term->value, irs::BoundType::INCLUSIVE);

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 1, 2, 3, 4, 5, 6 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(docs->value(), doc->value);
        for (;docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // int - seq = [7..7]
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(INT32_C(7));
      auto* min_term = irs::get<irs::term_attribute>(min_stream);
      ASSERT_TRUE(min_stream.next());

      irs::numeric_token_stream max_stream;
      max_stream.reset(INT32_C(7));
      auto* max_term = irs::get<irs::term_attribute>(max_stream);
      ASSERT_TRUE(max_stream.next());

      irs::by_range query = make_filter(
        "seq",
         min_term->value, irs::BoundType::INCLUSIVE,
         max_term->value, irs::BoundType::INCLUSIVE);

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 8 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(docs->value(), doc->value);
        for (;docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // int - seq = [1..7]
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(INT32_C(1));
      auto* min_term = irs::get<irs::term_attribute>(min_stream);
      ASSERT_TRUE(min_stream.next());

      irs::numeric_token_stream max_stream;
      max_stream.reset(INT32_C(7));
      auto* max_term = irs::get<irs::term_attribute>(max_stream);
      ASSERT_TRUE(max_stream.next());

      irs::by_range query = make_filter(
        "seq",
         min_term->value, irs::BoundType::INCLUSIVE,
         max_term->value, irs::BoundType::INCLUSIVE);

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 2, 3, 4, 5, 6, 7, 8 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(docs->value(), doc->value);
        for (;docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // int - seq > 28 
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(INT32_C(28));
      auto* min_term = irs::get<irs::term_attribute>(min_stream);
      ASSERT_TRUE(min_stream.next());

      irs::by_range query = make_filter(
        "seq",
         min_term->value, irs::BoundType::EXCLUSIVE,
         (irs::numeric_utils::numeric_traits<int32_t>::max)(), irs::BoundType::INCLUSIVE);

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 30, 31, 32 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(docs->value(), doc->value);
        for (;docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // int - seq <= 5 
    {
      irs::numeric_token_stream max_stream;
      max_stream.reset(INT32_C(5));
      auto* max_term = irs::get<irs::term_attribute>(max_stream);
      ASSERT_TRUE(max_stream.next());

      irs::by_range query = make_filter(
        "seq",
         (irs::numeric_utils::numeric_traits<int32_t>::min)(), irs::BoundType::INCLUSIVE,
         max_term->value, irs::BoundType::INCLUSIVE);

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 1, 2, 3, 4, 5, 6 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(docs->value(), doc->value);
        for (;docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // float - value = [123..123]
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset((float_t)123.f);
      auto* min_term = irs::get<irs::term_attribute>(min_stream);
      ASSERT_TRUE(min_stream.next());

      irs::numeric_token_stream max_stream;
      max_stream.reset((float_t)123.f);
      auto* max_term = irs::get<irs::term_attribute>(max_stream);
      ASSERT_TRUE(max_stream.next());

      irs::by_range query = make_filter(
        "value",
         min_term->value, irs::BoundType::INCLUSIVE,
         max_term->value, irs::BoundType::INCLUSIVE);

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 3, 8 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(docs->value(), doc->value);
        for (;docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // float - value = [91.524..123)
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset((float_t)91.524f);
      auto* min_term = irs::get<irs::term_attribute>(min_stream);
      ASSERT_TRUE(min_stream.next());

      irs::numeric_token_stream max_stream;
      max_stream.reset((float_t)123.f);
      auto* max_term = irs::get<irs::term_attribute>(max_stream);
      ASSERT_TRUE(max_stream.next());

      irs::by_range query = make_filter(
        "value",
         min_term->value, irs::BoundType::INCLUSIVE,
         max_term->value, irs::BoundType::EXCLUSIVE);

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 1, 2, 5, 7, 9, 10, 12 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(docs->value(), doc->value);
        for (;docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // float - value < 91.565
    {
      irs::numeric_token_stream max_stream;
      max_stream.reset((float_t)90.565f);
      auto* max_term = irs::get<irs::term_attribute>(max_stream);
      ASSERT_TRUE(max_stream.next());

      irs::by_range query = make_filter(
        "value",
         irs::numeric_utils::numeric_traits<float_t>::ninf(), irs::BoundType::INCLUSIVE,
         max_term->value, irs::BoundType::EXCLUSIVE);

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected{ 4, 11, 13, 14, 15, 16, 17 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(docs->value(), doc->value);
        for (;docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // float - value > 91.565
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset((float_t)90.565f);
      auto* min_term = irs::get<irs::term_attribute>(min_stream);
      ASSERT_TRUE(min_stream.next());

      irs::by_range query = make_filter(
        "value",
         min_term->value, irs::BoundType::EXCLUSIVE,
         irs::numeric_utils::numeric_traits<float_t>::inf(), irs::BoundType::INCLUSIVE);

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected{ 1, 2, 3, 5, 6, 7, 8, 9, 10, 12 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(docs->value(), doc->value);
        for (;docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // double - value = [123..123]
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset((double_t)123.);
      auto* min_term = irs::get<irs::term_attribute>(min_stream);
      ASSERT_TRUE(min_stream.next());
      irs::numeric_token_stream max_stream;
      max_stream.reset((double_t)123.);
      auto* max_term = irs::get<irs::term_attribute>(max_stream);
      ASSERT_TRUE(max_stream.next());

      irs::by_range query = make_filter(
        "value",
         min_term->value, irs::BoundType::INCLUSIVE,
         max_term->value, irs::BoundType::INCLUSIVE);

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected{ 3, 8 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(docs->value(), doc->value);
        for (;docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // double - value = (-40; 90.564]
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset((double_t)-40.);
      auto* min_term = irs::get<irs::term_attribute>(min_stream);
      ASSERT_TRUE(min_stream.next());
      irs::numeric_token_stream max_stream;
      max_stream.reset((double_t)90.564);
      auto* max_term = irs::get<irs::term_attribute>(max_stream);
      ASSERT_TRUE(max_stream.next());

      irs::by_range query = make_filter(
        "value",
         min_term->value, irs::BoundType::EXCLUSIVE,
         max_term->value, irs::BoundType::INCLUSIVE);

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected{ 4, 11, 13, 14, 15, 16, 17 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(docs->value(), doc->value);
        for (;docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // double - value < 5; 
    {
      irs::numeric_token_stream max_stream;
      max_stream.reset((double_t)5.);
      auto* max_term = irs::get<irs::term_attribute>(max_stream);
      ASSERT_TRUE(max_stream.next());

      irs::by_range query = make_filter(
        "value",
         irs::numeric_utils::numeric_traits<double_t>::ninf(), irs::BoundType::EXCLUSIVE,
         max_term->value, irs::BoundType::EXCLUSIVE);

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected{ 14, 15, 17 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(docs->value(), doc->value);
        for (;docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // double - value > 90.543; 
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset((double_t)90.543);
      auto* min_term = irs::get<irs::term_attribute>(min_stream);
      ASSERT_TRUE(min_stream.next());

      irs::by_range query = make_filter(
        "value",
         min_term->value, irs::BoundType::EXCLUSIVE,
         irs::numeric_utils::numeric_traits<double_t>::inf(), irs::BoundType::INCLUSIVE);

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected{ 1, 2, 3, 5, 6, 7, 8, 9, 10, 12, 13 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(docs->value(), doc->value);
        for (;docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
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

      irs::by_range filter;
      *filter.mutable_field() = "name";

      check_query(filter, docs, costs, rdr);
    }

    // invalid_name = (..;..)
    {
      irs::by_range filter;
      *filter.mutable_field() = "invalid_name";

      check_query(filter, docs_t{}, rdr);
    }

    // name = ["";..)
    {
      docs_t docs{ 
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
      };
      costs_t costs{ docs.size() };


      irs::by_range filter;
      *filter.mutable_field() = "name";
      filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;

      check_query(filter, docs, costs, rdr);
    }

    // name = ("";..]
    {
      docs_t docs{ 
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32
      };
      costs_t costs{ docs.size() };

      irs::by_range filter;
      *filter.mutable_field() = "name";
      filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;

      check_query(filter, docs, costs, rdr);
    }

    // name = ["";""]
    {
      docs_t docs{ };
      costs_t costs{ docs.size() };


      irs::by_range filter;
      *filter.mutable_field() = "name";
      filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      check_query(filter, docs, costs, rdr);
    }

    // name = [A;..)
    // result: A .. Z, ~
    {
      docs_t docs{
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,
        18, 19, 20, 21, 22, 23, 24, 25, 26, 27
      }; 
      costs_t costs{ docs.size() };

      irs::by_range filter;
      *filter.mutable_field() = "name";
      filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;

      check_query(filter, docs, costs, rdr);
    }

    // name = (A;..)
    // result: A .. Z, ~
    {
      docs_t docs{
        2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27
      };
      costs_t costs{ docs.size() };

      irs::by_range filter;
      *filter.mutable_field() = "name";
      filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;

      check_query(filter, docs, costs, rdr);
    }

    // name = (..;C)
    // result: A, B, !, @, #, $, %
    {
      docs_t docs{ 1, 2, 28, 29, 30, 31, 32 };
      costs_t costs{ docs.size() };

      irs::by_range filter;
      *filter.mutable_field() = "name";
      filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;

      check_query(filter, docs, costs, rdr);
    }

    // name = (..;C]
    // result: A, B, C, !, @, #, $, %
    {
      docs_t docs{ 1, 2, 3, 28, 29, 30, 31, 32 };
      costs_t costs{ docs.size() };

      irs::by_range filter;
      *filter.mutable_field() = "name";
      filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      check_query(filter, docs, costs, rdr);
    }

    // name = [A;C] 
    // result: A, B, C
    {
      docs_t docs{ 1, 2, 3 };
      costs_t costs{ docs.size() };

      irs::by_range filter;
      *filter.mutable_field() = "name";
      filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      check_query(filter, docs, costs, rdr);
    }

    // name = [A;B]
    // result: A, B
    {
      docs_t docs{ 1, 2 };
      costs_t costs{ docs.size() };

      irs::by_range filter;
      *filter.mutable_field() = "name";
      filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      check_query(filter, docs, costs, rdr);
    }

    // name = [A;B)
    // result: A
    {
      docs_t docs{ 1 };
      costs_t costs{ docs.size() };

      irs::by_range filter;
      *filter.mutable_field() = "name";
      filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;

      check_query(filter, docs, costs, rdr);
    }

    // name = (A;B]
    // result: A
    {
      docs_t docs{ 2 };
      costs_t costs{ docs.size() };

      irs::by_range filter;
      *filter.mutable_field() = "name";
      filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      check_query(filter, docs, costs, rdr);
    }

    // name = (A;B)
    // result:
    {
      irs::by_range filter;
      *filter.mutable_field() = "name";
      filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("B"));
      filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;

      check_query(filter, docs_t{}, costs_t{0}, rdr);
    }

    // name = [A;C)
    // result: A, B
    {
      docs_t docs{ 1, 2 };
      costs_t costs{ docs.size() };

      irs::by_range filter;
      *filter.mutable_field() = "name";
      filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;

      check_query(filter, docs, costs, rdr);
    }

    // name = (A;C]
    // result: B, C
    {
      docs_t docs{ 2, 3 };
      costs_t costs{ docs.size() };

      irs::by_range filter;
      *filter.mutable_field() = "name";
      filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      check_query(filter, docs, costs, rdr);
    }

    // name = (A;C)
    // result: B
    {
      docs_t docs{ 2 };
      costs_t costs{ docs.size() };

      irs::by_range filter;
      *filter.mutable_field() = "name";
      filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;

      check_query(filter, docs, costs, rdr);
    }

    // name = [C;A]
    // result:
    {
      irs::by_range filter;
      *filter.mutable_field() = "name";
      filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("C"));
      filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("A"));
      filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      check_query(filter,  docs_t{}, costs_t{0}, rdr);
    }

    // name = [~;..]
    // result: ~
    {
      docs_t docs{ 27 };
      costs_t costs{ docs.size() };

      irs::by_range filter;
      *filter.mutable_field() = "name";
      filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("~"));
      filter.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("~"));
      filter.mutable_options()->range.max_type = irs::BoundType::UNBOUNDED;

      check_query(filter, docs, costs, rdr);
    }

    // name = (~;..]
    // result:
    {
      irs::by_range filter;
      *filter.mutable_field() = "name";
      filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("~"));
      filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;

      check_query(filter, docs_t{}, costs_t{0}, rdr);
    }

    // name = (a;..]
    // result: ~
    {
      docs_t docs{ 27 };
      costs_t costs{ 1 };

      irs::by_range filter;
      *filter.mutable_field() = "name";
      filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("a"));
      filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("a"));
      filter.mutable_options()->range.max_type = irs::BoundType::UNBOUNDED;

      check_query(filter, docs, costs, rdr);
    }

    // name = [..;a]
    // result: !, @, #, $, %, A..Z
    {
      docs_t docs{
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 28, 29, 30, 31, 32
      };
      costs_t costs{ docs.size() };

      irs::by_range filter;
      *filter.mutable_field() = "name";
      filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("a"));
      filter.mutable_options()->range.min_type = irs::BoundType::UNBOUNDED;
      filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("a"));
      filter.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      check_query(filter,  docs, costs, rdr);
    }

    // name = [..;a)
    // result: !, @, #, $, %, A..Z
    {
      docs_t docs{
        1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
        17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 28, 29, 30, 31, 32
      };
      costs_t costs{ docs.size() };

      irs::by_range filter;
      *filter.mutable_field() = "name";
      filter.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("a"));
      filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;

      check_query(filter, docs, costs, rdr);
    }

    // name = [DEL;..]
    // result:
    {
      irs::by_range filter;
      *filter.mutable_field() = "name";
      filter.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("\x7f"));
      filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;

      check_query(filter, docs_t{}, costs_t{0}, rdr);
    }
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
      scorer.prepare_field_collector_ = [&scorer]()->irs::sort::field_collector::ptr {
        return irs::memory::make_unique<tests::sort::custom_sort::prepared::field_collector>(scorer);
      };
      scorer.prepare_term_collector_ = [&scorer]()->irs::sort::term_collector::ptr {
        return irs::memory::make_unique<tests::sort::custom_sort::prepared::term_collector>(scorer);
      };

      irs::by_range filter;
      *filter.mutable_field() = "value";
      filter.mutable_options()->range.min = irs::numeric_utils::numeric_traits<double_t>::ninf();
      filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      filter.mutable_options()->range.max = irs::numeric_utils::numeric_traits<double_t>::inf();
      filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;

      check_query(filter, order, docs, rdr);
      ASSERT_EQ(11, collect_field_count); // 1 field in 1 segment
      ASSERT_EQ(11, collect_term_count); // 11 different terms
      ASSERT_EQ(11, finish_count); // 11 different terms
    }

    // value = (..;..)
    {
      docs_t docs{ 1, 5, 7, 9, 10, 3, 4, 8, 11, 2, 6, 12, 13, 14, 15, 16, 17 };
      costs_t costs{ docs.size() };
      irs::order order;

      irs::by_range filter;
      *filter.mutable_field() = "value";

      order.add<tests::sort::frequency_sort>(false);
      check_query(filter, order, docs, rdr);
    }

    // value = (..;..) + scored_terms_limit
    {
      docs_t docs{ 1, 5, 7, 9, 10, 3, 8, 2, 4, 6, 11, 12, 13, 14, 15, 16, 17 };
      costs_t costs{ docs.size() };
      irs::order order;

      order.add<tests::sort::frequency_sort>(false);

      irs::by_range filter;
      *filter.mutable_field() = "value";
      filter.mutable_options()->range.min = irs::numeric_utils::numeric_traits<double_t>::ninf();
      filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      filter.mutable_options()->range.max = irs::numeric_utils::numeric_traits<double_t>::inf();
      filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
      filter.mutable_options()->scored_terms_limit = 2;

      check_query(filter, order, docs, rdr);
    }

    // value = (..;100)
    {
      docs_t docs{ 4, 11, 12, 13, 14, 15, 16, 17 };
      costs_t costs{ docs.size() };
      irs::order order;
      irs::numeric_token_stream max_stream;
      max_stream.reset((double_t)100.);
      auto* max_term = irs::get<irs::term_attribute>(max_stream);

      ASSERT_TRUE(max_stream.next());
      order.add<tests::sort::frequency_sort>(false);

      irs::by_range filter;
      *filter.mutable_field() = "value";
      filter.mutable_options()->range.min = irs::numeric_utils::numeric_traits<double_t>::ninf();
      filter.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      filter.mutable_options()->range.max = max_term->value;
      filter.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;

      check_query(filter, order, docs, rdr);
    }
  }
}; // range_filter_test_case

TEST(by_range_test, options) {
  irs::by_range_options opts;
  ASSERT_TRUE(opts.range.min.empty());
  ASSERT_EQ(irs::BoundType::UNBOUNDED, opts.range.min_type);
  ASSERT_TRUE(opts.range.max.empty());
  ASSERT_EQ(irs::BoundType::UNBOUNDED, opts.range.max_type);
  ASSERT_EQ(1024, opts.scored_terms_limit);
}

TEST(by_range_test, ctor) {
  irs::by_range q;
  ASSERT_EQ(irs::type<irs::by_range>::id(), q.type());
  ASSERT_EQ(irs::by_range_options{}, q.options());
  ASSERT_EQ(irs::no_boost(), q.boost());
}

TEST(by_range_test, equal) {
  irs::by_range q0;
  *q0.mutable_field() = "field";
  q0.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("min_term"));
  q0.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
  q0.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("max_term"));
  q0.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

  irs::by_range q1;
  *q1.mutable_field() = "field";
  q1.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("min_term"));
  q1.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
  q1.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("max_term"));
  q1.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

  ASSERT_EQ(q0, q1);
  ASSERT_EQ(q0.hash(), q1.hash());

  irs::by_range q2;
  *q2.mutable_field() = "field1";
  q2.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("min_term"));
  q2.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
  q2.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("max_term"));
  q2.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

  ASSERT_NE(q0, q2);

  irs::by_range q3;
  *q3.mutable_field() = "field";
  q3.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("min_term1"));
  q3.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
  q3.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("max_term"));
  q3.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

  ASSERT_NE(q0, q3);

  irs::by_range q4;
  *q4.mutable_field() = "field";
  q4.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("min_term"));
  q4.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
  q4.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("max_term1"));
  q4.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

  ASSERT_NE(q0, q4);

  irs::by_range q5;
  *q5.mutable_field() = "field";
  q5.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("min_term"));
  q5.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
  q5.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("max_term"));
  q5.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

  ASSERT_NE(q0, q5);

  irs::by_range q6;
  *q6.mutable_field() = "field";
  q6.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("min_term"));
  q6.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
  q6.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("max_term"));
  q6.mutable_options()->range.max_type = irs::BoundType::UNBOUNDED;

  ASSERT_NE(q0, q6);
}

TEST(by_range_test, boost) {
  // no boost
  {
    irs::by_range q;
    *q.mutable_field() = "field";
    q.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("min_term"));
    q.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
    q.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("max_term"));
    q.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(irs::no_boost(), prepared->boost());
  }

  // with boost
  {
    irs::boost_t boost = 1.5f;

    irs::by_range q;
    *q.mutable_field() = "field";
    q.mutable_options()->range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("min_term"));
    q.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
    q.mutable_options()->range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("max_term"));
    q.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
    q.boost(boost);

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(boost, prepared->boost());
  }
}

TEST_P(range_filter_test_case, by_range) {
  by_range_sequential_cost();
}

TEST_P(range_filter_test_case, by_range_numeric) {
  by_range_sequential_numeric();
}

TEST_P(range_filter_test_case, by_range_order) {
  by_range_sequential_order();
}

TEST_P(range_filter_test_case, visit) {
  // add segment
  {
    tests::json_doc_generator gen(
      resource("simple_sequential.json"),
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  const irs::string_ref field = "prefix";
  irs::by_range_options::range_type range;
  range.min = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));
  range.max = irs::ref_cast<irs::byte_type>(irs::string_ref("abcd"));
  range.min_type = irs::BoundType::INCLUSIVE;
  range.max_type = irs::BoundType::INCLUSIVE;

  tests::empty_filter_visitor visitor;
  // read segment
  auto index = open_reader();
  ASSERT_EQ(1, index.size());
  auto& segment = index[0];

  // get term dictionary for field
  const auto* reader = segment.field(field);
  ASSERT_NE(nullptr, reader);
  irs::by_range::visit(segment, *reader, range, visitor);
  ASSERT_EQ(1, visitor.prepare_calls_counter());
  ASSERT_EQ(2, visitor.visit_calls_counter());
  ASSERT_EQ(
    (std::vector<std::pair<irs::string_ref, irs::boost_t>>{
      {"abc", irs::no_boost()},
      {"abcd", irs::no_boost()}
    }),
    visitor.term_refs<char>());

  visitor.reset();
}

INSTANTIATE_TEST_CASE_P(
  range_filter_test,
  range_filter_test_case,
  ::testing::Combine(
    ::testing::Values(
      &tests::memory_directory,
      &tests::fs_directory,
      &tests::mmap_directory
    ),
    ::testing::Values(tests::format_info{"1_0"},
                      tests::format_info{"1_1", "1_0"},
                      tests::format_info{"1_2", "1_0"},
                      tests::format_info{"1_3", "1_0"})
  ),
  tests::to_string
);

}
