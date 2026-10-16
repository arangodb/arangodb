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

#include "filter_test_case_base.hpp"
#include "search/granular_range_filter.hpp"
#include "tests_shared.hpp"

namespace {

class granular_float_field : public tests::float_field {
 public:
  granular_float_field() {
    this->features_.emplace_back(irs::type<irs::granularity_prefix>::id());
  }
};

class granular_double_field : public tests::double_field {
 public:
  granular_double_field() {
    this->features_.emplace_back(irs::type<irs::granularity_prefix>::id());
  }
};

class granular_int_field : public tests::int_field {
 public:
  granular_int_field() {
    this->features_.emplace_back(irs::type<irs::granularity_prefix>::id());
  }
};

class granular_long_field : public tests::long_field {
 public:
  granular_long_field() {
    this->features_.emplace_back(irs::type<irs::granularity_prefix>::id());
  }
};

class granular_range_filter_test_case : public tests::FilterTestCaseBase {
 protected:
  static void by_range_json_field_factory(
    tests::document& doc, const std::string& name,
    const tests::json_doc_generator::json_value& data) {
    if (data.is_string()) {
      doc.insert(std::make_shared<tests::string_field>(name, data.str));
    } else if (data.is_null()) {
      doc.insert(std::make_shared<tests::binary_field>());
      auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
      field.name(name);
      field.value(
        irs::ViewCast<irs::byte_type>(irs::null_token_stream::value_null()));
    } else if (data.is_bool() && data.b) {
      doc.insert(std::make_shared<tests::binary_field>());
      auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
      field.name(name);
      field.value(
        irs::ViewCast<irs::byte_type>(irs::boolean_token_stream::value_true()));
    } else if (data.is_bool() && !data.b) {
      doc.insert(std::make_shared<tests::binary_field>());
      auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
      field.name(name);
      field.value(
        irs::ViewCast<irs::byte_type>(irs::boolean_token_stream::value_true()));
    } else if (data.is_number()) {
      // 'value' can be interpreted as a double
      {
        doc.insert(std::make_shared<granular_double_field>());
        const auto dValue = data.as_number<double_t>();
        auto& field = (doc.indexed.end() - 1).as<tests::double_field>();
        field.name(name);
        field.value(dValue);
      }

      // 'value' can be interpreted as a float
      {
        doc.insert(std::make_shared<granular_float_field>());
        auto fValue = data.as_number<float_t>();
        auto& field = (doc.indexed.end() - 1).as<tests::float_field>();
        field.name(name);
        field.value(fValue);
      }

      // 'value' can be interpreted as int64
      {
        doc.insert(std::make_shared<granular_long_field>());
        const auto liValue = data.as_number<int64_t>();
        auto& field = (doc.indexed.end() - 1).as<tests::long_field>();
        field.name(name);
        field.value(liValue);
      }

      // 'value' can be interpreted as int32
      {
        doc.insert(std::make_shared<granular_int_field>());
        auto lValue = data.as_number<int32_t>();
        auto& field = (doc.indexed.end() - 1).as<tests::int_field>();
        field.name(name);
        field.value(lValue);
      }
    }
  }

  void by_range_granularity_boost() {
    // add segment
    {
      tests::json_doc_generator gen(resource("granular_sequential.json"),
                                    &by_range_json_field_factory);

      add_segment(gen, irs::OM_CREATE);
    }

    auto rdr = open_reader();
    ASSERT_EQ(1, rdr->size());

    auto& segment = (*rdr)[0];

    MaxMemoryCounter counter;

    // without boost
    {
      irs::by_granular_range q;
      *q.mutable_field() = "name";
      irs::set_granular_term(
        q.mutable_options()->range.min,
        irs::ViewCast<irs::byte_type>(std::string_view("A")));
      irs::set_granular_term(
        q.mutable_options()->range.max,
        irs::ViewCast<irs::byte_type>(std::string_view("M")));
      q.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      q.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

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
      irs::by_granular_range q;
      *q.mutable_field() = "name";
      irs::set_granular_term(
        q.mutable_options()->range.min,
        irs::ViewCast<irs::byte_type>(std::string_view("A")));
      irs::set_granular_term(
        q.mutable_options()->range.max,
        irs::ViewCast<irs::byte_type>(std::string_view("M")));
      q.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      q.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
      q.boost(boost);

      auto prepared = q.prepare({.index = segment});
      ASSERT_EQ(boost, prepared->boost());
    }
  }

  void by_range_granularity_level() {
    // add segment
    {
      tests::json_doc_generator gen(resource("granular_sequential.json"),
                                    &by_range_json_field_factory);

      add_segment(gen, irs::OM_CREATE);
    }

    auto rdr = open_reader();

    // range under same granularity value for topmost element, (i.e. last value
    // from numeric_token_stream)
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(INT32_C(0));

      irs::numeric_token_stream max_stream;
      max_stream.reset(INT32_C(1000));

      irs::by_granular_range query;
      *query.mutable_field() = "value";
      irs::set_granular_term(query.mutable_options()->range.min, min_stream);
      irs::set_granular_term(query.mutable_options()->range.max, max_stream);
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      ASSERT_EQ(2, query.options().range.min.size());
      ASSERT_EQ(2, query.options().range.max.size());

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{1, 2, 3};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // range under different granularity value for topmost element, (i.e. last
    // value from numeric_token_stream)
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(INT32_C(-1000));

      irs::numeric_token_stream max_stream;
      max_stream.reset(INT32_C(+1000));

      irs::by_granular_range query;
      *query.mutable_field() = "value";
      irs::set_granular_term(query.mutable_options()->range.min, min_stream);
      irs::set_granular_term(query.mutable_options()->range.max, max_stream);
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      ASSERT_EQ(2, query.options().range.min.size());
      ASSERT_EQ(2, query.options().range.max.size());

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{1, 2, 3, 11, 12};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
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
      *query.mutable_field() = "value";
      irs::set_granular_term(query.mutable_options()->range.min, min_stream);
      irs::set_granular_term(query.mutable_options()->range.max, max_stream);
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{1, 2, 3, 4, 5, 6, 7, 10, 11, 12};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // double - value > 100
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(double_t(100));

      irs::by_granular_range query;
      *query.mutable_field() = "value";
      irs::set_granular_term(query.mutable_options()->range.min, min_stream);
      irs::set_granular_term(
        query.mutable_options()->range.max,
        irs::numeric_utils::numeric_traits<double_t>::inf());
      query.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{3, 4, 5, 6, 7, 8};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // double - value => 100
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(double_t(100));

      irs::by_granular_range query;
      *query.mutable_field() = "value";
      irs::set_granular_term(query.mutable_options()->range.min, min_stream);
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{2, 3, 4, 5, 6, 7, 8};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // double - value => 20007 (largest value)
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(double_t(20007));

      irs::by_granular_range query;
      *query.mutable_field() = "value";
      irs::set_granular_term(query.mutable_options()->range.min, min_stream);
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{8};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // double - value < 10000.123
    {
      irs::numeric_token_stream max_stream;
      max_stream.reset(double_t(10000.123));

      irs::by_granular_range query;
      *query.mutable_field() = "value";
      irs::set_granular_term(
        query.mutable_options()->range.min,
        irs::numeric_utils::numeric_traits<double_t>::ninf());
      irs::set_granular_term(query.mutable_options()->range.max, max_stream);
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{1, 2, 3, 4, 9, 10, 11, 12};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // double - value <= 10000.123
    {
      irs::numeric_token_stream max_stream;
      max_stream.reset(double_t(10000.123));

      irs::by_granular_range query;
      *query.mutable_field() = "value";
      irs::set_granular_term(query.mutable_options()->range.max, max_stream);
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{1, 2, 3, 4, 5, 9, 10, 11, 12};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // all documents
    {
      irs::by_granular_range query;
      *query.mutable_field() = "value";

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{1, 2, 3, 4,  5,  6,
                                          7, 8, 9, 10, 11, 12};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }
  }

  void by_range_sequential_numeric() {
    // add segment
    {
      tests::json_doc_generator gen(resource("simple_sequential.json"),
                                    &by_range_json_field_factory);

      add_segment(gen, irs::OM_CREATE);
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

      irs::by_granular_range query;
      *query.mutable_field() = "seq";
      irs::set_granular_term(query.mutable_options()->range.min,
                             min_term->value);
      irs::set_granular_term(query.mutable_options()->range.max,
                             max_term->value);
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{8};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
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

      irs::by_granular_range query;
      *query.mutable_field() = "seq";
      irs::set_granular_term(query.mutable_options()->range.min,
                             min_term->value);
      irs::set_granular_term(query.mutable_options()->range.max,
                             max_term->value);
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{2, 3, 4, 5, 6, 7, 8};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // long - value = [31 .. 32] with same-level granularity (last value in
    // segment)
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(INT64_C(31));

      irs::numeric_token_stream max_stream;
      max_stream.reset(INT64_C(32));

      irs::by_granular_range query;
      *query.mutable_field() = "seq";
      irs::set_granular_term(query.mutable_options()->range.min, min_stream);
      irs::set_granular_term(query.mutable_options()->range.max, max_stream);
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{32};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
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

      irs::by_granular_range query;
      *query.mutable_field() = "seq";
      irs::set_granular_term(query.mutable_options()->range.min,
                             min_term->value);
      irs::set_granular_term(
        query.mutable_options()->range.max,
        (irs::numeric_utils::numeric_traits<int64_t>::max)());
      query.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{30, 31, 32};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // long - seq >= 31 (match largest value)
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(INT64_C(31));
      auto* min_term = irs::get<irs::term_attribute>(min_stream);
      ASSERT_TRUE(min_stream.next());

      irs::by_granular_range query;
      *query.mutable_field() = "seq";
      irs::set_granular_term(query.mutable_options()->range.min,
                             min_term->value);
      irs::set_granular_term(
        query.mutable_options()->range.max,
        (irs::numeric_utils::numeric_traits<int64_t>::max)());
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{32};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
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

      irs::by_granular_range query;
      *query.mutable_field() = "seq";
      irs::set_granular_term(
        query.mutable_options()->range.min,
        (irs::numeric_utils::numeric_traits<int64_t>::min)());
      irs::set_granular_term(query.mutable_options()->range.max,
                             max_term->value);
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{1, 2, 3, 4, 5, 6};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
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

      irs::by_granular_range query;
      *query.mutable_field() = "seq";
      irs::set_granular_term(query.mutable_options()->range.min,
                             min_term->value);
      irs::set_granular_term(query.mutable_options()->range.max,
                             max_term->value);
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{8};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
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

      irs::by_granular_range query;
      *query.mutable_field() = "seq";
      irs::set_granular_term(query.mutable_options()->range.min,
                             min_term->value);
      irs::set_granular_term(query.mutable_options()->range.max,
                             max_term->value);
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{2, 3, 4, 5, 6, 7, 8};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // int - value = [31 .. 32] with same-level granularity (last value in
    // segment)
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(INT32_C(31));

      irs::numeric_token_stream max_stream;
      max_stream.reset(INT32_C(32));

      irs::by_granular_range query;
      *query.mutable_field() = "seq";
      irs::set_granular_term(query.mutable_options()->range.min, min_stream);
      irs::set_granular_term(query.mutable_options()->range.max, max_stream);
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{32};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
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

      irs::by_granular_range query;
      *query.mutable_field() = "seq";
      irs::set_granular_term(query.mutable_options()->range.min,
                             min_term->value);
      irs::set_granular_term(
        query.mutable_options()->range.max,
        (irs::numeric_utils::numeric_traits<int32_t>::max)());
      query.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{30, 31, 32};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(docs->value(), doc->value);
        for (; docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // int - seq >= 31 (match largest value)
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(INT32_C(31));
      auto* min_term = irs::get<irs::term_attribute>(min_stream);
      ASSERT_TRUE(min_stream.next());

      irs::by_granular_range query;
      *query.mutable_field() = "seq";
      irs::set_granular_term(query.mutable_options()->range.min,
                             min_term->value);
      irs::set_granular_term(
        query.mutable_options()->range.max,
        (irs::numeric_utils::numeric_traits<int32_t>::max)());
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{32};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
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

      irs::by_granular_range query;
      *query.mutable_field() = "seq";
      irs::set_granular_term(
        query.mutable_options()->range.min,
        (irs::numeric_utils::numeric_traits<int32_t>::min)());
      irs::set_granular_term(query.mutable_options()->range.max,
                             max_term->value);
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{1, 2, 3, 4, 5, 6};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
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

      irs::by_granular_range query;
      *query.mutable_field() = "value";
      irs::set_granular_term(query.mutable_options()->range.min,
                             min_term->value);
      irs::set_granular_term(query.mutable_options()->range.max,
                             max_term->value);
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{3, 8};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
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

      irs::by_granular_range query;
      *query.mutable_field() = "value";
      irs::set_granular_term(query.mutable_options()->range.min,
                             min_term->value);
      irs::set_granular_term(query.mutable_options()->range.max,
                             max_term->value);
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{1, 2, 5, 7, 9, 10, 12};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // float - value = [31 .. 32] with same-level granularity (last value in
    // segment)
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(float_t(31));

      irs::numeric_token_stream max_stream;
      max_stream.reset(float_t(32));

      irs::by_granular_range query;
      *query.mutable_field() = "seq";
      irs::set_granular_term(query.mutable_options()->range.min, min_stream);
      irs::set_granular_term(query.mutable_options()->range.max, max_stream);
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{32};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
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

      irs::by_granular_range query;
      *query.mutable_field() = "value";
      irs::set_granular_term(
        query.mutable_options()->range.min,
        irs::numeric_utils::numeric_traits<float_t>::ninf());
      irs::set_granular_term(query.mutable_options()->range.max,
                             max_term->value);
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{4, 11, 13, 14, 15, 16, 17};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
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

      irs::by_granular_range query;
      *query.mutable_field() = "value";
      irs::set_granular_term(query.mutable_options()->range.min,
                             min_term->value);
      irs::set_granular_term(
        query.mutable_options()->range.max,
        irs::numeric_utils::numeric_traits<float_t>::inf());
      query.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{1, 2, 3, 5, 6, 7, 8, 9, 10, 12};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // float - value >= 31 (largest value)
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(float_t(31));
      auto* min_term = irs::get<irs::term_attribute>(min_stream);
      ASSERT_TRUE(min_stream.next());

      irs::by_granular_range query;
      *query.mutable_field() = "seq";
      irs::set_granular_term(query.mutable_options()->range.min,
                             min_term->value);
      irs::set_granular_term(
        query.mutable_options()->range.max,
        irs::numeric_utils::numeric_traits<float_t>::inf());
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{32};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // double - value = [123...123]
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset((double_t)123.);
      auto* min_term = irs::get<irs::term_attribute>(min_stream);
      ASSERT_TRUE(min_stream.next());
      irs::numeric_token_stream max_stream;
      max_stream.reset((double_t)123.);
      auto* max_term = irs::get<irs::term_attribute>(max_stream);
      ASSERT_TRUE(max_stream.next());

      irs::by_granular_range query;
      *query.mutable_field() = "value";
      irs::set_granular_term(query.mutable_options()->range.min,
                             min_term->value);
      irs::set_granular_term(query.mutable_options()->range.max,
                             max_term->value);
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{3, 8};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
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

      irs::by_granular_range query;
      *query.mutable_field() = "value";
      irs::set_granular_term(query.mutable_options()->range.min,
                             min_term->value);
      irs::set_granular_term(query.mutable_options()->range.max,
                             max_term->value);
      query.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{4, 11, 13, 14, 15, 16, 17};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // double - value = [31 .. 32] with same-level granularity (last value in
    // segment)
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(double_t(31));

      irs::numeric_token_stream max_stream;
      max_stream.reset(double_t(32));

      irs::by_granular_range query;
      *query.mutable_field() = "seq";
      irs::set_granular_term(query.mutable_options()->range.min, min_stream);
      irs::set_granular_term(query.mutable_options()->range.max, max_stream);
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{32};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
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

      irs::by_granular_range query;
      *query.mutable_field() = "value";
      irs::set_granular_term(
        query.mutable_options()->range.min,
        irs::numeric_utils::numeric_traits<double_t>::ninf());
      irs::set_granular_term(query.mutable_options()->range.max,
                             max_term->value);
      query.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{14, 15, 17};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
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

      irs::by_granular_range query;
      *query.mutable_field() = "value";
      irs::set_granular_term(query.mutable_options()->range.min,
                             min_term->value);
      irs::set_granular_term(
        query.mutable_options()->range.max,
        irs::numeric_utils::numeric_traits<double_t>::inf());
      query.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{1, 2, 3, 5, 6, 7, 8, 9, 10, 12, 13};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // double - value >= 31 (largest value)
    {
      irs::numeric_token_stream min_stream;
      min_stream.reset(double_t(31));
      auto* min_term = irs::get<irs::term_attribute>(min_stream);
      ASSERT_TRUE(min_stream.next());

      irs::by_granular_range query;
      *query.mutable_field() = "seq";
      irs::set_granular_term(query.mutable_options()->range.min,
                             min_term->value);
      irs::set_granular_term(
        query.mutable_options()->range.max,
        irs::numeric_utils::numeric_traits<double_t>::inf());
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      auto prepared = query.prepare({.index = rdr});

      std::vector<irs::doc_id_t> expected{32};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        auto* doc = irs::get<irs::document>(*docs);
        ASSERT_TRUE(bool(doc));
        for (; docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }
  }

  void by_range_sequential_cost() {
    // add segment
    {
      tests::json_doc_generator gen(resource("simple_sequential.json"),
                                    &by_range_json_field_factory);

      add_segment(gen, irs::OM_CREATE);
    }

    auto rdr = open_reader();

    // empty query
    CheckQuery(irs::by_granular_range(), Docs{}, rdr);

    // name = (..;..)
    {
      Docs docs{1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
                17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32};
      Costs costs{docs.size()};

      irs::by_granular_range query;
      *query.mutable_field() = "name";

      CheckQuery(query, docs, costs, rdr);
    }

    // invalid_name = (..;..)
    {
      irs::by_granular_range query;
      *query.mutable_field() = "invalid_name";

      CheckQuery(query, Docs{}, rdr);
    }

    // name = [A;..)
    // result: A .. Z, ~
    {
      Docs docs{2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14,
                15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27};
      Costs costs{docs.size()};

      irs::by_granular_range query;
      *query.mutable_field() = "name";
      irs::set_granular_term(
        query.mutable_options()->range.min,
        irs::ViewCast<irs::byte_type>(std::string_view("A")));
      query.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;

      CheckQuery(query, docs, costs, rdr);
    }

    // name = (A;..)
    // result: A .. Z, ~
    {
      Docs docs{1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14,
                15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27};
      Costs costs{docs.size()};

      irs::by_granular_range query;
      *query.mutable_field() = "name";
      irs::set_granular_term(
        query.mutable_options()->range.min,
        irs::ViewCast<irs::byte_type>(std::string_view("A")));
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;

      CheckQuery(query, docs, costs, rdr);
    }

    // name = (..;C)
    // result: A, B, !, @, #, $, %
    {
      Docs docs{1, 2, 28, 29, 30, 31, 32};
      Costs costs{docs.size()};

      irs::by_granular_range query;
      *query.mutable_field() = "name";
      irs::set_granular_term(
        query.mutable_options()->range.max,
        irs::ViewCast<irs::byte_type>(std::string_view("C")));
      query.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;

      CheckQuery(query, docs, costs, rdr);
    }

    // name = (..;C]
    // result: A, B, C, !, @, #, $, %
    {
      Docs docs{1, 2, 3, 28, 29, 30, 31, 32};
      Costs costs{docs.size()};

      irs::by_granular_range query;
      *query.mutable_field() = "name";
      irs::set_granular_term(
        query.mutable_options()->range.max,
        irs::ViewCast<irs::byte_type>(std::string_view("C")));
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      CheckQuery(query, docs, costs, rdr);
    }

    // name = [A;C]
    // result: A, B, C
    {
      Docs docs{1, 2, 3};
      Costs costs{docs.size()};

      irs::by_granular_range query;
      *query.mutable_field() = "name";
      irs::set_granular_term(
        query.mutable_options()->range.min,
        irs::ViewCast<irs::byte_type>(std::string_view("A")));
      irs::set_granular_term(
        query.mutable_options()->range.max,
        irs::ViewCast<irs::byte_type>(std::string_view("C")));
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      CheckQuery(query, docs, costs, rdr);
    }

    // name = [A;B]
    // result: A, B
    {
      Docs docs{1, 2};
      Costs costs{docs.size()};

      irs::by_granular_range query;
      *query.mutable_field() = "name";
      irs::set_granular_term(
        query.mutable_options()->range.min,
        irs::ViewCast<irs::byte_type>(std::string_view("A")));
      irs::set_granular_term(
        query.mutable_options()->range.max,
        irs::ViewCast<irs::byte_type>(std::string_view("B")));
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      CheckQuery(query, docs, costs, rdr);
    }

    // name = [A;B)
    // result: A
    {
      Docs docs{1};
      Costs costs{docs.size()};

      irs::by_granular_range query;
      *query.mutable_field() = "name";
      irs::set_granular_term(
        query.mutable_options()->range.min,
        irs::ViewCast<irs::byte_type>(std::string_view("A")));
      irs::set_granular_term(
        query.mutable_options()->range.max,
        irs::ViewCast<irs::byte_type>(std::string_view("B")));
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;

      CheckQuery(query, docs, costs, rdr);
    }

    // name = (A;B]
    // result: A
    {
      Docs docs{2};
      Costs costs{docs.size()};

      irs::by_granular_range query;
      *query.mutable_field() = "name";
      irs::set_granular_term(
        query.mutable_options()->range.min,
        irs::ViewCast<irs::byte_type>(std::string_view("A")));
      irs::set_granular_term(
        query.mutable_options()->range.max,
        irs::ViewCast<irs::byte_type>(std::string_view("B")));
      query.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      CheckQuery(query, docs, costs, rdr);
    }

    // name = (A;B)
    // result:
    {
      irs::by_granular_range query;
      *query.mutable_field() = "name";
      irs::set_granular_term(
        query.mutable_options()->range.min,
        irs::ViewCast<irs::byte_type>(std::string_view("A")));
      irs::set_granular_term(
        query.mutable_options()->range.max,
        irs::ViewCast<irs::byte_type>(std::string_view("B")));
      query.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;

      CheckQuery(query, Docs{}, Costs{0}, rdr);
    }

    // name = [A;C)
    // result: A, B
    {
      Docs docs{1, 2};
      Costs costs{docs.size()};

      irs::by_granular_range query;
      *query.mutable_field() = "name";
      irs::set_granular_term(
        query.mutable_options()->range.min,
        irs::ViewCast<irs::byte_type>(std::string_view("A")));
      irs::set_granular_term(
        query.mutable_options()->range.max,
        irs::ViewCast<irs::byte_type>(std::string_view("C")));
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;

      CheckQuery(query, docs, costs, rdr);
    }

    // name = (A;C]
    // result: B, C
    {
      Docs docs{2, 3};
      Costs costs{docs.size()};

      irs::by_granular_range query;
      *query.mutable_field() = "name";
      irs::set_granular_term(
        query.mutable_options()->range.min,
        irs::ViewCast<irs::byte_type>(std::string_view("A")));
      irs::set_granular_term(
        query.mutable_options()->range.max,
        irs::ViewCast<irs::byte_type>(std::string_view("C")));
      query.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      CheckQuery(query, docs, costs, rdr);
    }

    // name = (A;C)
    // result: B
    {
      Docs docs{2};
      Costs costs{docs.size()};

      irs::by_granular_range query;
      *query.mutable_field() = "name";
      irs::set_granular_term(
        query.mutable_options()->range.min,
        irs::ViewCast<irs::byte_type>(std::string_view("A")));
      irs::set_granular_term(
        query.mutable_options()->range.max,
        irs::ViewCast<irs::byte_type>(std::string_view("C")));
      query.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;

      CheckQuery(query, docs, costs, rdr);
    }

    // name = [C;A]
    // result:
    {
      irs::by_granular_range query;
      *query.mutable_field() = "name";
      irs::set_granular_term(
        query.mutable_options()->range.min,
        irs::ViewCast<irs::byte_type>(std::string_view("C")));
      irs::set_granular_term(
        query.mutable_options()->range.max,
        irs::ViewCast<irs::byte_type>(std::string_view("A")));
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      CheckQuery(query, Docs{}, Costs{0}, rdr);
    }

    // name = [~;..]
    // result: ~
    {
      Docs docs{27};
      Costs costs{docs.size()};

      irs::by_granular_range query;
      *query.mutable_field() = "name";
      irs::set_granular_term(
        query.mutable_options()->range.min,
        irs::ViewCast<irs::byte_type>(std::string_view("~")));
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;

      CheckQuery(query, docs, costs, rdr);
    }

    // name = (~;..]
    // result:
    {
      irs::by_granular_range query;
      *query.mutable_field() = "name";
      irs::set_granular_term(
        query.mutable_options()->range.min,
        irs::ViewCast<irs::byte_type>(std::string_view("~")));
      query.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;

      CheckQuery(query, Docs{}, Costs{0}, rdr);
    }

    // name = (a;..]
    // result: ~
    {
      Docs docs{27};
      Costs costs{1};

      irs::by_granular_range query;
      *query.mutable_field() = "name";
      irs::set_granular_term(
        query.mutable_options()->range.min,
        irs::ViewCast<irs::byte_type>(std::string_view("a")));
      query.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;

      CheckQuery(query, docs, costs, rdr);
    }

    // name = [..;a]
    // result: !, @, #, $, %, A..Z
    {
      Docs docs{1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
                17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 28, 29, 30, 31, 32};
      Costs costs{docs.size()};

      irs::by_granular_range query;
      *query.mutable_field() = "name";
      irs::set_granular_term(
        query.mutable_options()->range.max,
        irs::ViewCast<irs::byte_type>(std::string_view("a")));
      query.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

      CheckQuery(query, docs, costs, rdr);
    }

    // name = [..;a)
    // result: !, @, #, $, %, A..Z
    {
      Docs docs{1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
                17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 28, 29, 30, 31, 32};
      Costs costs{docs.size()};

      irs::by_granular_range query;
      *query.mutable_field() = "name";
      irs::set_granular_term(
        query.mutable_options()->range.max,
        irs::ViewCast<irs::byte_type>(std::string_view("a")));
      query.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;

      CheckQuery(query, docs, costs, rdr);
    }

    // name = [DEL;..]
    // result:
    {
      irs::by_granular_range query;
      *query.mutable_field() = "name";
      irs::set_granular_term(
        query.mutable_options()->range.min,
        irs::ViewCast<irs::byte_type>(std::string_view("\x7f")));
      query.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;

      CheckQuery(query, Docs{}, Costs{0}, rdr);
    }
  }
};

TEST(by_granular_range_test, options) {
  irs::by_granular_range_options opts;
  ASSERT_TRUE(opts.range.min.empty());
  ASSERT_EQ(irs::BoundType::UNBOUNDED, opts.range.min_type);
  ASSERT_TRUE(opts.range.max.empty());
  ASSERT_EQ(irs::BoundType::UNBOUNDED, opts.range.max_type);
  ASSERT_EQ(1024, opts.scored_terms_limit);
}

TEST(by_granular_range_test, ctor) {
  irs::by_granular_range q;
  ASSERT_EQ(irs::type<irs::by_granular_range>::id(), q.type());
  ASSERT_EQ(irs::by_granular_range_options{}, q.options());
  ASSERT_EQ(irs::kNoBoost, q.boost());
}

TEST(by_granular_range_test, equal) {
  irs::by_granular_range q0;
  *q0.mutable_field() = "field";
  irs::set_granular_term(
    q0.mutable_options()->range.min,
    irs::ViewCast<irs::byte_type>(std::string_view("min_term")));
  irs::set_granular_term(
    q0.mutable_options()->range.max,
    irs::ViewCast<irs::byte_type>(std::string_view("max_term")));
  q0.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
  q0.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

  irs::by_granular_range q1;
  *q1.mutable_field() = "field";
  irs::set_granular_term(
    q1.mutable_options()->range.min,
    irs::ViewCast<irs::byte_type>(std::string_view("min_term")));
  irs::set_granular_term(
    q1.mutable_options()->range.max,
    irs::ViewCast<irs::byte_type>(std::string_view("max_term")));
  q1.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
  q1.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

  ASSERT_EQ(q0, q1);

  irs::by_granular_range q2;
  *q2.mutable_field() = "field1";
  irs::set_granular_term(
    q2.mutable_options()->range.min,
    irs::ViewCast<irs::byte_type>(std::string_view("min_term")));
  irs::set_granular_term(
    q2.mutable_options()->range.max,
    irs::ViewCast<irs::byte_type>(std::string_view("max_term")));
  q2.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
  q2.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

  ASSERT_NE(q0, q2);

  irs::by_granular_range q3;
  *q3.mutable_field() = "field";
  irs::set_granular_term(
    q3.mutable_options()->range.min,
    irs::ViewCast<irs::byte_type>(std::string_view("min_term1")));
  irs::set_granular_term(
    q3.mutable_options()->range.max,
    irs::ViewCast<irs::byte_type>(std::string_view("max_term")));
  q3.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
  q3.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

  ASSERT_NE(q0, q3);

  irs::by_granular_range q4;
  *q4.mutable_field() = "field";
  irs::set_granular_term(
    q4.mutable_options()->range.min,
    irs::ViewCast<irs::byte_type>(std::string_view("min_term")));
  irs::set_granular_term(
    q4.mutable_options()->range.max,
    irs::ViewCast<irs::byte_type>(std::string_view("max_term1")));
  q4.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
  q4.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

  ASSERT_NE(q0, q4);

  irs::by_granular_range q5;
  *q5.mutable_field() = "field";
  irs::set_granular_term(
    q5.mutable_options()->range.min,
    irs::ViewCast<irs::byte_type>(std::string_view("min_term")));
  irs::set_granular_term(
    q5.mutable_options()->range.max,
    irs::ViewCast<irs::byte_type>(std::string_view("max_term")));
  q5.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
  q5.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

  ASSERT_NE(q0, q5);

  irs::by_granular_range q6;
  *q6.mutable_field() = "field";
  irs::set_granular_term(
    q6.mutable_options()->range.min,
    irs::ViewCast<irs::byte_type>(std::string_view("min_term")));
  irs::set_granular_term(
    q6.mutable_options()->range.max,
    irs::ViewCast<irs::byte_type>(std::string_view("max_term")));
  q6.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
  q6.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;

  ASSERT_NE(q0, q6);

  irs::by_granular_range q7;
  *q7.mutable_field() = "field";
  irs::set_granular_term(
    q7.mutable_options()->range.min,
    irs::ViewCast<irs::byte_type>(std::string_view("min_term")));
  irs::set_granular_term(
    q7.mutable_options()->range.max,
    irs::ViewCast<irs::byte_type>(std::string_view("max_term")));
  q7.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
  q7.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
  q7.mutable_options()->scored_terms_limit = 100;

  ASSERT_NE(q0, q7);

  ASSERT_NE(q0, q6);
}

TEST(by_granular_range_test, boost) {
  // no boost
  {
    irs::by_granular_range q;
    *q.mutable_field() = "field";
    irs::set_granular_term(
      q.mutable_options()->range.min,
      irs::ViewCast<irs::byte_type>(std::string_view("min_term")));
    irs::set_granular_term(
      q.mutable_options()->range.max,
      irs::ViewCast<irs::byte_type>(std::string_view("max_term")));
    q.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
    q.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;

    auto prepared = q.prepare({.index = irs::SubReader::empty()});
    ASSERT_EQ(irs::kNoBoost, prepared->boost());
  }

  // with boost, empty query
  {
    irs::score_t boost = 1.5f;
    irs::by_granular_range q;
    *q.mutable_field() = "field";
    irs::set_granular_term(
      q.mutable_options()->range.min,
      irs::ViewCast<irs::byte_type>(std::string_view("min_term")));
    irs::set_granular_term(
      q.mutable_options()->range.max,
      irs::ViewCast<irs::byte_type>(std::string_view("max_term")));
    q.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;
    q.mutable_options()->range.max_type = irs::BoundType::INCLUSIVE;
    q.boost(boost);

    auto prepared = q.prepare({.index = irs::SubReader::empty()});
    ASSERT_EQ(irs::kNoBoost, prepared->boost());
  }
}

TEST_P(granular_range_filter_test_case, by_range) {
  by_range_sequential_cost();
}

TEST_P(granular_range_filter_test_case, by_range_granularity) {
  by_range_granularity_level();
}

TEST_P(granular_range_filter_test_case, by_range_granularity_boost) {
  by_range_granularity_boost();
}

TEST_P(granular_range_filter_test_case, by_range_numeric) {
  by_range_sequential_numeric();
}

TEST_P(granular_range_filter_test_case, by_range_order) {
  // add segment
  {
    tests::json_doc_generator gen(resource("simple_sequential.json"),
                                  &by_range_json_field_factory);

    add_segment(gen, irs::OM_CREATE);
  }

  auto rdr = open_reader();

  // empty query
  CheckQuery(irs::by_granular_range(), Docs{}, rdr);

  // name = (..;..) test collector call count for field/term/finish
  {
    Docs docs{};
    Costs costs{docs.size()};

    size_t collect_field_count = 0;
    size_t collect_term_count = 0;
    size_t finish_count = 0;

    std::array<irs::Scorer::ptr, 1> scorers{
      std::make_unique<tests::sort::custom_sort>()};
    auto& scorer = static_cast<tests::sort::custom_sort&>(*scorers.front());

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
    scorer.prepare_field_collector_ = [&scorer]() -> irs::FieldCollector::ptr {
      return std::make_unique<tests::sort::custom_sort::field_collector>(
        scorer);
    };
    scorer.prepare_term_collector_ = [&scorer]() -> irs::TermCollector::ptr {
      return std::make_unique<tests::sort::custom_sort::term_collector>(scorer);
    };

    irs::by_granular_range q;
    *q.mutable_field() = "invalid_field";
    irs::set_granular_term(
      q.mutable_options()->range.min,
      irs::numeric_utils::numeric_traits<double_t>::ninf());
    irs::set_granular_term(q.mutable_options()->range.max,
                           irs::numeric_utils::numeric_traits<double_t>::inf());
    q.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
    q.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;

    CheckQuery(q, scorers, docs, rdr, false);
    ASSERT_EQ(0, collect_field_count);
    ASSERT_EQ(0, collect_term_count);
    ASSERT_EQ(0, finish_count);
  }

  // value = (..;..) test collector call count for field/term/finish
  {
    Docs docs{1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17};
    Costs costs{docs.size()};

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
    scorer.prepare_field_collector_ = [&scorer]() -> irs::FieldCollector::ptr {
      return std::make_unique<tests::sort::custom_sort::field_collector>(
        scorer);
    };
    scorer.prepare_term_collector_ = [&scorer]() -> irs::TermCollector::ptr {
      return std::make_unique<tests::sort::custom_sort::term_collector>(scorer);
    };

    irs::by_granular_range q;
    *q.mutable_field() = "value";
    irs::set_granular_term(
      q.mutable_options()->range.min,
      irs::numeric_utils::numeric_traits<double_t>::ninf());
    irs::set_granular_term(q.mutable_options()->range.max,
                           irs::numeric_utils::numeric_traits<double_t>::inf());
    q.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
    q.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;

    CheckQuery(q, order, docs, rdr);
    ASSERT_EQ(11, collect_field_count);  // 11 fields (1 per term since treated
                                         // as a disjunction) in 1 segment
    ASSERT_EQ(11, collect_term_count);   // 11 different terms
    ASSERT_EQ(11, finish_count);         // 11 different terms
  }

  // value = (..;..)
  {
    Docs docs{1, 5, 7, 9, 10, 3, 4, 8, 11, 2, 6, 12, 13, 14, 15, 16, 17};
    Costs costs{docs.size()};
    std::array<irs::Scorer::ptr, 1> order{
      std::make_unique<tests::sort::frequency_sort>()};

    irs::by_granular_range q;
    *q.mutable_field() = "value";
    irs::set_granular_term(
      q.mutable_options()->range.min,
      irs::numeric_utils::numeric_traits<double_t>::ninf());
    irs::set_granular_term(q.mutable_options()->range.max,
                           irs::numeric_utils::numeric_traits<double_t>::inf());
    q.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
    q.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;

    CheckQuery(q, order, docs, rdr);
  }

  // value = (..;..) + scored_terms_limit
  {
    Docs docs{2, 4, 6, 11, 12, 13, 14, 15, 16, 17, 1, 5, 7, 9, 10, 3, 8};
    Costs costs{docs.size()};
    std::array<irs::Scorer::ptr, 1> order{
      std::make_unique<tests::sort::frequency_sort>()};

    irs::by_granular_range q;
    *q.mutable_field() = "value";
    irs::set_granular_term(
      q.mutable_options()->range.min,
      irs::numeric_utils::numeric_traits<double_t>::ninf());
    irs::set_granular_term(q.mutable_options()->range.max,
                           irs::numeric_utils::numeric_traits<double_t>::inf());
    q.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
    q.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;
    q.mutable_options()->scored_terms_limit = 2;

    CheckQuery(q, order, docs, rdr);
  }

  // value = (..;100)
  {
    Docs docs{4, 11, 12, 13, 14, 15, 16, 17};
    Costs costs{docs.size()};
    irs::numeric_token_stream max_stream;
    max_stream.reset((double_t)100.);
    auto* max_term = irs::get<irs::term_attribute>(max_stream);

    ASSERT_TRUE(max_stream.next());

    std::array<irs::Scorer::ptr, 1> order{
      std::make_unique<tests::sort::frequency_sort>()};

    irs::by_granular_range q;
    *q.mutable_field() = "value";
    irs::set_granular_term(
      q.mutable_options()->range.min,
      irs::numeric_utils::numeric_traits<double_t>::ninf());
    irs::set_granular_term(q.mutable_options()->range.max, max_term->value);
    q.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
    q.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;

    CheckQuery(q, order, docs, rdr);
  }
}

TEST_P(granular_range_filter_test_case, by_range_order_multiple_sorts) {
  {
    auto writer = open_writer(irs::OM_CREATE);
    ASSERT_NE(nullptr, writer);

    // add segment
    index().emplace_back(writer->FeatureInfo());
    auto& segment = index().back();

    {
      const auto* data =
        "[                                 \
           { \"seq\": -6, \"value\": 2  }, \
           { \"seq\": -5, \"value\": 1  }, \
           { \"seq\": -4, \"value\": 1  }, \
           { \"seq\": -3, \"value\": 3  }, \
           { \"seq\": -2, \"value\": 1  }, \
           { \"seq\": -1, \"value\": 56 }  \
         ]";

      tests::json_doc_generator gen(data, &by_range_json_field_factory);
      write_segment(*writer, segment, gen);
    }

    // add segment
    {
      tests::json_doc_generator gen(resource("simple_sequential.json"),
                                    &by_range_json_field_factory);

      write_segment(*writer, segment, gen);
    }
    writer->Commit();
    AssertSnapshotEquality(*writer);
  }

  auto rdr = open_reader();

  // value = [...;...)
  const int seed = -6;
  for (int begin = seed, end = begin + int(rdr->docs_count()); begin != end;
       ++begin) {
    Docs docs;
    docs.resize(size_t(end - begin));
    std::iota(docs.begin(), docs.end(),
              size_t(begin - seed + irs::doc_limits::min()));
    Costs costs{docs.size()};
    irs::numeric_token_stream min_stream;
    min_stream.reset((double_t)begin);

    std::array<irs::Scorer::ptr, 1> order{
      std::make_unique<tests::sort::frequency_sort>()};

    irs::by_granular_range q;
    *q.mutable_field() = "seq";
    irs::set_granular_term(q.mutable_options()->range.min, min_stream);
    q.mutable_options()->range.min_type = irs::BoundType::INCLUSIVE;

    CheckQuery(q, order, docs, rdr);
  }
}

// covers https://github.com/arangodb/backlog/issues/528
TEST_P(granular_range_filter_test_case, by_range_numeric_sequence) {
  // add segment
  tests::json_doc_generator gen(
    resource("numeric_sequence.json"),
    [](tests::document& doc, const std::string& name,
       const tests::json_doc_generator::json_value& data) {
      if (data.is_string()) {
        doc.insert(std::make_shared<tests::string_field>(name, data.str));
      } else if (data.is_null()) {
        doc.insert(std::make_shared<tests::binary_field>());
        auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
        field.name(name);
        field.value(
          irs::ViewCast<irs::byte_type>(irs::null_token_stream::value_null()));
      } else if (data.is_bool() && data.b) {
        doc.insert(std::make_shared<tests::binary_field>());
        auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
        field.name(name);
        field.value(irs::ViewCast<irs::byte_type>(
          irs::boolean_token_stream::value_true()));
      } else if (data.is_bool() && !data.b) {
        doc.insert(std::make_shared<tests::binary_field>());
        auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
        field.name(name);
        field.value(irs::ViewCast<irs::byte_type>(
          irs::boolean_token_stream::value_true()));
      } else if (data.is_number()) {
        // 'value' can be interpreted as a double
        const auto dValue = data.as_number<double_t>();
        {
          doc.insert(std::make_shared<granular_double_field>());
          auto& field = (doc.indexed.end() - 1).as<tests::double_field>();
          field.name(name);
          field.value(dValue);
        }
      }
    });

  add_segment(gen, irs::OM_CREATE);

  auto reader = open_reader();
  ASSERT_EQ(1, reader->size());
  auto& segment = reader[0];

  // a > -inf && a < 30.
  {
    std::set<std::string> expected;

    // fill expected values
    {
      gen.reset();
      while (auto* doc = gen.next()) {
        auto* numeric_field =
          dynamic_cast<granular_double_field*>(doc->indexed.get("a"));
        ASSERT_NE(nullptr, numeric_field);

        if (numeric_field->value() < 30.) {
          auto* key_field =
            dynamic_cast<tests::string_field*>(doc->indexed.get("_key"));
          ASSERT_NE(nullptr, key_field);

          expected.emplace(std::string(key_field->value()));
        }
      }
    }

    irs::numeric_token_stream max_stream;
    max_stream.reset(30.);

    irs::by_granular_range query;
    *query.mutable_field() = "a";
    irs::set_granular_term(
      query.mutable_options()->range.min,
      irs::numeric_utils::numeric_traits<double_t>::ninf());
    irs::set_granular_term(query.mutable_options()->range.max, max_stream);
    query.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
    query.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;

    auto prepared = query.prepare({.index = reader});
    ASSERT_NE(nullptr, prepared);
    auto* column = segment.column("_key");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    std::set<std::string> actual;

    auto docs = prepared->execute({.segment = segment});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    while (docs->next()) {
      const auto doc = docs->value();
      ASSERT_EQ(doc, values->seek(doc));
      actual.emplace(irs::to_string<std::string>(actual_value->value.data()));
    }
    ASSERT_EQ(expected, actual);
  }

  // a < 30.
  {
    std::set<std::string> expected;

    // fill expected values
    {
      gen.reset();
      while (auto* doc = gen.next()) {
        auto* numeric_field =
          dynamic_cast<granular_double_field*>(doc->indexed.get("a"));
        ASSERT_NE(nullptr, numeric_field);

        if (numeric_field->value() < 30.) {
          auto* key_field =
            dynamic_cast<tests::string_field*>(doc->indexed.get("_key"));
          ASSERT_NE(nullptr, key_field);

          expected.emplace(std::string(key_field->value()));
        }
      }
    }

    irs::numeric_token_stream max_stream;
    max_stream.reset(30.);

    irs::by_granular_range query;
    *query.mutable_field() = "a";
    irs::set_granular_term(query.mutable_options()->range.max, max_stream);
    query.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;

    auto prepared = query.prepare({.index = reader});
    ASSERT_NE(nullptr, prepared);
    auto* column = segment.column("_key");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    std::set<std::string> actual;

    auto docs = prepared->execute({.segment = segment});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    while (docs->next()) {
      const auto doc = docs->value();
      ASSERT_EQ(doc, values->seek(doc));
      actual.emplace(irs::to_string<std::string>(actual_value->value.data()));
    }
    ASSERT_EQ(expected, actual);
  }

  // a > 30. && a < inf
  {
    std::set<std::string> expected;

    // fill expected values
    {
      gen.reset();
      while (auto* doc = gen.next()) {
        auto* numeric_field =
          dynamic_cast<granular_double_field*>(doc->indexed.get("a"));
        ASSERT_NE(nullptr, numeric_field);

        if (numeric_field->value() > 30.) {
          auto* key_field =
            dynamic_cast<tests::string_field*>(doc->indexed.get("_key"));
          ASSERT_NE(nullptr, key_field);

          expected.emplace(std::string(key_field->value()));
        }
      }
    }

    irs::numeric_token_stream min_stream;
    min_stream.reset(30.);

    irs::by_granular_range query;
    *query.mutable_field() = "a";
    irs::set_granular_term(query.mutable_options()->range.min, min_stream);
    irs::set_granular_term(query.mutable_options()->range.max,
                           irs::numeric_utils::numeric_traits<double_t>::inf());
    query.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;
    query.mutable_options()->range.max_type = irs::BoundType::EXCLUSIVE;

    auto prepared = query.prepare({.index = reader});
    ASSERT_NE(nullptr, prepared);
    auto* column = segment.column("_key");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    std::set<std::string> actual;

    auto docs = prepared->execute({.segment = segment});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    while (docs->next()) {
      const auto doc = docs->value();
      ASSERT_EQ(doc, values->seek(doc));
      actual.emplace(irs::to_string<std::string>(actual_value->value.data()));
    }
    ASSERT_EQ(expected, actual);
  }

  // a > 30.
  {
    std::set<std::string> expected;

    // fill expected values
    {
      gen.reset();
      while (auto* doc = gen.next()) {
        auto* numeric_field =
          dynamic_cast<granular_double_field*>(doc->indexed.get("a"));
        ASSERT_NE(nullptr, numeric_field);

        if (numeric_field->value() > 30.) {
          auto* key_field =
            dynamic_cast<tests::string_field*>(doc->indexed.get("_key"));
          ASSERT_NE(nullptr, key_field);

          expected.emplace(std::string(key_field->value()));
        }
      }
    }

    irs::numeric_token_stream min_stream;
    min_stream.reset(30.);

    irs::by_granular_range query;
    *query.mutable_field() = "a";
    irs::set_granular_term(query.mutable_options()->range.min, min_stream);
    query.mutable_options()->range.min_type = irs::BoundType::EXCLUSIVE;

    auto prepared = query.prepare({.index = reader});
    ASSERT_NE(nullptr, prepared);
    auto* column = segment.column("_key");
    ASSERT_NE(nullptr, column);
    auto values = column->iterator(irs::ColumnHint::kNormal);
    ASSERT_NE(nullptr, values);
    auto* actual_value = irs::get<irs::payload>(*values);
    ASSERT_NE(nullptr, actual_value);

    std::set<std::string> actual;

    auto docs = prepared->execute({.segment = segment});
    auto* doc = irs::get<irs::document>(*docs);
    ASSERT_TRUE(bool(doc));
    while (docs->next()) {
      const auto doc = docs->value();
      ASSERT_EQ(doc, values->seek(doc));
      actual.emplace(irs::to_string<std::string>(actual_value->value.data()));
    }
    ASSERT_EQ(expected, actual);
  }
}

TEST_P(granular_range_filter_test_case, visit) {
  // add segment
  {
    tests::json_doc_generator gen(resource("simple_sequential.json"),
                                  &tests::generic_json_field_factory);

    add_segment(gen, irs::OM_CREATE);
  }

  tests::empty_filter_visitor visitor;
  std::string fld = "prefix";
  std::string_view field = std::string_view(fld);
  irs::by_granular_range::options_type::range_type rng;
  rng.min = {static_cast<irs::bstring>(
    irs::ViewCast<irs::byte_type>(std::string_view("abc")))};
  rng.max = {static_cast<irs::bstring>(
    irs::ViewCast<irs::byte_type>(std::string_view("abcd")))};
  rng.min_type = irs::BoundType::INCLUSIVE;
  rng.max_type = irs::BoundType::INCLUSIVE;
  // read segment
  auto index = open_reader();
  ASSERT_EQ(1, index.size());
  auto& segment = index[0];
  // get term dictionary for field
  const auto* reader = segment.field(field);
  ASSERT_TRUE(reader != nullptr);

  irs::by_granular_range::visit(segment, *reader, rng, visitor);
  ASSERT_EQ(2, visitor.prepare_calls_counter());
  ASSERT_EQ(2, visitor.visit_calls_counter());
  ASSERT_EQ((std::vector<std::pair<std::string_view, irs::score_t>>{
              {"abc", irs::kNoBoost}, {"abcd", irs::kNoBoost}}),
            visitor.term_refs<char>());

  visitor.reset();
}

static constexpr auto kTestDirs = tests::getDirectories<tests::kTypesDefault>();

INSTANTIATE_TEST_SUITE_P(granular_range_filter_test,
                         granular_range_filter_test_case,
                         ::testing::Combine(::testing::ValuesIn(kTestDirs),
                                            ::testing::Values("1_0")),
                         granular_range_filter_test_case::to_string);

}  // namespace
