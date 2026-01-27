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
#include "search/range_filter.hpp"
#include "search/term_filter.hpp"
#include "search/term_query.hpp"
#include "tests_shared.hpp"

namespace {

irs::by_term make_filter(const std::string_view& field,
                         const std::string_view term) {
  irs::by_term q;
  *q.mutable_field() = field;
  q.mutable_options()->term = irs::ViewCast<irs::byte_type>(term);
  return q;
}

class term_filter_test_case : public tests::FilterTestCaseBase {
 protected:
  void by_term_sequential_cost() {
    // add segment
    {
      tests::json_doc_generator gen(resource("simple_sequential.json"),
                                    &tests::generic_json_field_factory);
      add_segment(gen);
    }

    // read segment
    auto rdr = open_reader();

    CheckQuery(irs::by_term(), Docs{}, Costs{0}, rdr);

    // empty term
    CheckQuery(make_filter("name", ""), Docs{}, Costs{0}, rdr);

    // empty field
    CheckQuery(make_filter("", "xyz"), Docs{}, Costs{0}, rdr);

    // search : invalid field
    CheckQuery(make_filter("invalid_field", "A"), Docs{}, Costs{0}, rdr);

    // search : single term
    CheckQuery(make_filter("name", "A"), Docs{1}, Costs{1}, rdr);

    MaxMemoryCounter counter;
    {
      irs::by_term q = make_filter("name", "A");

      auto prepared = q.prepare({
        .index = rdr,
        .memory = counter,
      });
      auto sub = rdr.begin();
      auto docs0 = prepared->execute({.segment = *sub});
      auto* doc = irs::get<irs::document>(*docs0);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs0->value(), doc->value);
      auto docs1 = prepared->execute({.segment = *sub});
      ASSERT_TRUE(docs0->next());
      ASSERT_EQ(docs0->value(), docs1->seek(docs0->value()));
    }
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // search : all terms
    CheckQuery(
      make_filter("same", "xyz"),
      Docs{1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
           17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32},
      Costs{32}, rdr);

    // search : empty result
    CheckQuery(make_filter("same", "invalid_term"), Docs{}, Costs{0}, rdr);
  }

  void by_term_sequential_boost() {
    // add segment
    {
      tests::json_doc_generator gen(resource("simple_sequential.json"),
                                    &tests::generic_json_field_factory);
      add_segment(gen);
    }

    // read segment
    auto rdr = open_reader();

    // create filter
    irs::by_term filter = make_filter("name", "A");
    filter.boost(0.f);

    // create order

    auto scorer = tests::sort::boost{};
    auto pord = irs::Scorers::Prepare(scorer);

    MaxMemoryCounter counter;

    // without boost
    {
      auto prep = filter.prepare({
        .index = rdr,
        .memory = counter,
        .scorers = pord,
      });
      auto docs = prep->execute({.segment = *(rdr.begin()), .scorers = pord});
      auto* doc = irs::get<irs::document>(*docs);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);

      auto* scr = irs::get<irs::score>(*docs);
      ASSERT_FALSE(!scr);

      // first hit
      {
        ASSERT_TRUE(docs->next());
        irs::score_t score_value;
        (*scr)(&score_value);
        ASSERT_EQ(irs::score_t(0), score_value);
        ASSERT_EQ(docs->value(), doc->value);
      }

      ASSERT_FALSE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
    }
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // with boost
    {
      const irs::score_t value = 5;
      filter.boost(value);

      auto prep = filter.prepare({
        .index = rdr,
        .memory = counter,
        .scorers = pord,
      });
      auto docs = prep->execute({.segment = *(rdr.begin()), .scorers = pord});

      auto* scr = irs::get<irs::score>(*docs);
      ASSERT_FALSE(!scr);

      // first hit
      {
        ASSERT_TRUE(docs->next());
        irs::score_t score_value;
        (*scr)(&score_value);
        ASSERT_EQ(irs::score_t(value), score_value);
      }

      ASSERT_FALSE(docs->next());
    }
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();
  }

  void by_term_sequential_numeric() {
    // add segment
    {
      tests::json_doc_generator gen(
        resource("simple_sequential.json"),
        [](tests::document& doc, const std::string& name,
           const tests::json_doc_generator::json_value& data) {
          if (data.is_string()) {
            doc.insert(std::make_shared<tests::string_field>(name, data.str));
          } else if (data.is_null()) {
            doc.insert(std::make_shared<tests::binary_field>());
            auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
            field.name(name);
            field.value(irs::ViewCast<irs::byte_type>(
              irs::null_token_stream::value_null()));
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
            const double dValue = data.as_number<double_t>();
            {
              // 'value' can be interpreted as a double
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

    MaxMemoryCounter counter;

    // long (20)
    {
      irs::numeric_token_stream stream;
      stream.reset(INT64_C(20));
      auto* term = irs::get<irs::term_attribute>(stream);
      ASSERT_TRUE(stream.next());

      irs::by_term query = make_filter("seq", irs::ViewCast<char>(term->value));

      auto prepared = query.prepare({
        .index = rdr,
        .memory = counter,
      });

      std::vector<irs::doc_id_t> expected{21};
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub : rdr) {
        auto docs = prepared->execute({.segment = sub});
        for (; docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // int (21)
    {
      irs::numeric_token_stream stream;
      stream.reset(INT32_C(21));
      auto* term = irs::get<irs::term_attribute>(stream);
      ASSERT_TRUE(stream.next());

      irs::by_term query = make_filter("seq", irs::ViewCast<char>(term->value));

      auto prepared = query.prepare({
        .index = rdr,
        .memory = counter,
      });

      std::vector<irs::doc_id_t> expected{22};
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
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // double (90.564)
    {
      irs::numeric_token_stream stream;
      stream.reset((double_t)90.564);
      auto* term = irs::get<irs::term_attribute>(stream);
      ASSERT_TRUE(stream.next());

      irs::by_term query =
        make_filter("value", irs::ViewCast<char>(term->value));

      auto prepared = query.prepare({
        .index = rdr,
        .memory = counter,
      });

      std::vector<irs::doc_id_t> expected{13};
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
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // float (90.564)
    {
      irs::numeric_token_stream stream;
      stream.reset((float_t)90.564f);
      auto* term = irs::get<irs::term_attribute>(stream);
      ASSERT_TRUE(stream.next());

      irs::by_term query =
        make_filter("value", irs::ViewCast<char>(term->value));

      auto prepared = query.prepare({
        .index = rdr,
        .memory = counter,
      });

      std::vector<irs::doc_id_t> expected{13};
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
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // double (100)
    {
      irs::numeric_token_stream stream;
      stream.reset((double_t)100.);
      auto* term = irs::get<irs::term_attribute>(stream);
      ASSERT_TRUE(stream.next());

      irs::by_term query =
        make_filter("value", irs::ViewCast<char>(term->value));

      auto prepared = query.prepare({
        .index = rdr,
        .memory = counter,
      });

      std::vector<irs::doc_id_t> expected{1, 5, 7, 9, 10};
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
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // float_t(100)
    {
      irs::numeric_token_stream stream;
      stream.reset((float_t)100.f);
      auto* term = irs::get<irs::term_attribute>(stream);
      ASSERT_TRUE(stream.next());

      irs::by_term query =
        make_filter("value", irs::ViewCast<char>(term->value));

      auto prepared = query.prepare({
        .index = rdr,
        .memory = counter,
      });

      std::vector<irs::doc_id_t> expected{1, 5, 7, 9, 10};
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
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // int(100)
    {
      irs::numeric_token_stream stream;
      stream.reset(100);
      auto* term = irs::get<irs::term_attribute>(stream);
      ASSERT_TRUE(stream.next());

      irs::by_term query =
        make_filter("value", irs::ViewCast<char>(term->value));

      auto prepared = query.prepare({
        .index = rdr,
        .memory = counter,
      });

      std::vector<irs::doc_id_t> expected{1, 5, 7, 9, 10};
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
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();

    // long(100)
    {
      irs::numeric_token_stream stream;
      stream.reset(INT64_C(100));
      auto* term = irs::get<irs::term_attribute>(stream);
      ASSERT_TRUE(stream.next());

      irs::by_term query =
        make_filter("value", irs::ViewCast<char>(term->value));

      auto prepared = query.prepare({
        .index = rdr,
        .memory = counter,
      });

      std::vector<irs::doc_id_t> expected{1, 5, 7, 9, 10};
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
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();
  }

  void by_term_sequential_order() {
    // add segment
    {
      tests::json_doc_generator gen(resource("simple_sequential.json"),
                                    &tests::generic_json_field_factory);
      add_segment(gen);
    }

    // read segment
    auto rdr = open_reader();

    MaxMemoryCounter counter;

    {
      // create filter
      irs::by_term filter = make_filter("prefix", "abcy");

      // create order
      size_t collect_field_count = 0;
      size_t collect_term_count = 0;
      size_t finish_count = 0;

      tests::sort::custom_sort scorer;

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
      scorer.prepare_field_collector_ =
        [&scorer]() -> irs::FieldCollector::ptr {
        return std::make_unique<tests::sort::custom_sort::field_collector>(
          scorer);
      };
      scorer.prepare_term_collector_ = [&scorer]() -> irs::TermCollector::ptr {
        return std::make_unique<tests::sort::custom_sort::term_collector>(
          scorer);
      };

      std::set<irs::doc_id_t> expected{31, 32};
      auto pord = irs::Scorers::Prepare(scorer);
      auto prep = filter.prepare({
        .index = rdr,
        .memory = counter,
        .scorers = pord,
      });
      auto docs = prep->execute({.segment = *(rdr.begin()), .scorers = pord});

      auto* scr = irs::get<irs::score>(*docs);
      ASSERT_FALSE(!scr);

      while (docs->next()) {
        irs::score_t score_value;
        (*scr)(&score_value);
        IRS_IGNORE(score_value);
        ASSERT_EQ(1, expected.erase(docs->value()));
      }

      ASSERT_TRUE(expected.empty());
      ASSERT_EQ(1, collect_field_count);  // 1 field in 1 segment
      ASSERT_EQ(1, collect_term_count);   // 1 term in 1 field in 1 segment
      ASSERT_EQ(1, finish_count);         // 1 unique term
    }
    EXPECT_EQ(counter.current, 0);
    EXPECT_GT(counter.max, 0);
    counter.Reset();
  }

  void by_term_sequential() {
    // add segment
    {
      tests::json_doc_generator gen(resource("simple_sequential.json"),
                                    &tests::generic_json_field_factory);
      add_segment(gen);
    }

    auto rdr = open_reader();

    // empty query
    CheckQuery(irs::by_term(), Docs{}, rdr);

    // empty term
    CheckQuery(make_filter("name", ""), Docs{}, rdr);

    // empty field
    CheckQuery(make_filter("", "xyz"), Docs{}, rdr);

    // search : invalid field
    CheckQuery(make_filter("invalid_field", "A"), Docs{}, rdr);

    // search : single term
    CheckQuery(make_filter("name", "A"), Docs{1}, rdr);

    // search : all terms
    CheckQuery(
      make_filter("same", "xyz"),
      Docs{1,  2,  3,  4,  5,  6,  7,  8,  9,  10, 11, 12, 13, 14, 15, 16,
           17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32},
      rdr);

    // search : empty result
    CheckQuery(make_filter("same", "invalid_term"), Docs{}, rdr);
  }

  void by_term_schemas() {
    // write segments
    {
      auto writer = open_writer(irs::OM_CREATE);

      std::vector<tests::doc_generator_base::ptr> gens;
      gens.emplace_back(
        new tests::json_doc_generator(resource("AdventureWorks2014.json"),
                                      &tests::generic_json_field_factory));
      gens.emplace_back(
        new tests::json_doc_generator(resource("AdventureWorks2014Edges.json"),
                                      &tests::generic_json_field_factory));
      gens.emplace_back(new tests::json_doc_generator(
        resource("Northwnd.json"), &tests::generic_json_field_factory));
      gens.emplace_back(new tests::json_doc_generator(
        resource("NorthwndEdges.json"), &tests::generic_json_field_factory));
      gens.emplace_back(new tests::json_doc_generator(
        resource("Northwnd.json"), &tests::generic_json_field_factory));
      gens.emplace_back(new tests::json_doc_generator(
        resource("NorthwndEdges.json"), &tests::generic_json_field_factory));
      add_segments(*writer, gens);
    }

    auto rdr = open_reader();
    CheckQuery(make_filter("Fields", "FirstName"), Docs{28, 167, 194}, rdr);

    // address to the [SDD-179]
    CheckQuery(make_filter("Name", "Product"), Docs{32}, rdr);
  }
};

TEST_P(term_filter_test_case, by_term) {
  by_term_sequential();
  by_term_schemas();
}

TEST_P(term_filter_test_case, by_term_numeric) { by_term_sequential_numeric(); }

TEST_P(term_filter_test_case, by_term_order) { by_term_sequential_order(); }

TEST_P(term_filter_test_case, by_term_boost) { by_term_sequential_boost(); }

TEST_P(term_filter_test_case, by_term_cost) { by_term_sequential_cost(); }

TEST_P(term_filter_test_case, visit) {
  // add segment
  {
    tests::json_doc_generator gen(resource("simple_sequential.json"),
                                  &tests::generic_json_field_factory);
    add_segment(gen);
  }

  const std::string_view field = "prefix";
  const auto term = irs::ViewCast<irs::byte_type>(std::string_view("abc"));

  tests::empty_filter_visitor visitor;

  // read segment
  auto index = open_reader();
  ASSERT_EQ(1, index.size());
  auto& segment = index[0];

  // get term dictionary for field
  const auto* reader = segment.field(field);
  ASSERT_NE(nullptr, reader);
  irs::by_term::visit(segment, *reader, term, visitor);
  ASSERT_EQ(1, visitor.prepare_calls_counter());
  ASSERT_EQ(1, visitor.visit_calls_counter());
  ASSERT_EQ((std::vector<std::pair<std::string_view, irs::score_t>>{
              {"abc", irs::kNoBoost}}),
            visitor.term_refs<char>());
  visitor.reset();
}

TEST(by_prefix_test, options) {
  irs::by_term_options opts;
  ASSERT_TRUE(opts.term.empty());
}

TEST(by_term_test, ctor) {
  irs::by_term q;
  ASSERT_EQ(irs::type<irs::by_term>::id(), q.type());
  ASSERT_EQ(irs::by_term_options{}, q.options());
  ASSERT_EQ("", q.field());
  ASSERT_EQ(irs::kNoBoost, q.boost());
}

TEST(by_term_test, equal) {
  irs::by_term q = make_filter("field", "term");
  ASSERT_EQ(q, make_filter("field", "term"));
  ASSERT_NE(q, make_filter("field1", "term"));
}

TEST(by_term_test, boost) {
  MaxMemoryCounter counter;

  // no boost
  {
    irs::by_term q = make_filter("field", "term");

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
    irs::by_term q = make_filter("field", "term");
    q.boost(boost);

    auto prepared = q.prepare({
      .index = irs::SubReader::empty(),
      .memory = counter,
    });
    ASSERT_EQ(boost, prepared->boost());
  }
  EXPECT_EQ(counter.current, 0);
  EXPECT_GT(counter.max, 0);
  counter.Reset();
}

static constexpr auto kTestDirs = tests::getDirectories<tests::kTypesDefault>();

INSTANTIATE_TEST_SUITE_P(term_filter_test, term_filter_test_case,
                         ::testing::Combine(::testing::ValuesIn(kTestDirs),
                                            ::testing::Values("1_0")),
                         term_filter_test_case::to_string);

}  // namespace
