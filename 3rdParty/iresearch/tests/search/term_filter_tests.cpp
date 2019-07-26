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
#include "search/term_filter.hpp"
#include "search/range_filter.hpp"

NS_LOCAL

class term_filter_test_case : public tests::filter_test_case_base {
 protected:
  void by_term_sequential_cost() {
    // add segment
    {
      tests::json_doc_generator gen(
        resource("simple_sequential.json"),
        &tests::generic_json_field_factory);
      add_segment(gen);
    }

    // read segment
    auto rdr = open_reader();

    check_query(irs::by_term(), docs_t{ }, costs_t{0}, rdr);

    // empty term
    check_query(irs::by_term().field("name"), docs_t{}, costs_t{0}, rdr);

    // empty field
    check_query(irs::by_term().term("xyz"), docs_t{}, costs_t{0}, rdr);

    // search : invalid field
    check_query(irs::by_term().field("invalid_field").term("A"), docs_t{}, costs_t{0}, rdr);

    // search : single term
    check_query(irs::by_term().field("name").term("A"), docs_t{1}, costs_t{1}, rdr);

    { 
      irs::by_term q;
      q.field("name").term("A");

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto docs0 = prepared->execute(*sub);
      auto& doc = docs0->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs0->value(), doc->value);
      auto docs1 = prepared->execute(*sub);
      ASSERT_TRUE(docs0->next());
      ASSERT_EQ(docs0->value(), docs1->seek(docs0->value()));
    }

    // search : all terms
    check_query(
      irs::by_term().field( "same" ).term( "xyz" ),
      docs_t{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
      costs_t{ 32 },
      rdr
    );

    // search : empty result
    check_query(irs::by_term().field("same").term("invalid_term"), docs_t{}, costs_t{0}, rdr);
  }

  void by_term_sequential_boost() {
    // add segment
    {
      tests::json_doc_generator gen(
        resource("simple_sequential.json"),
        &tests::generic_json_field_factory);
      add_segment( gen );
    }

    // read segment
    auto rdr = open_reader();

    // create filter
    irs::by_term filter;
    filter.field( "name" ).term( "A" );
    filter.boost(0.f);

    // create order
    irs::order ord;
    ord.add<tests::sort::boost>(false);
    auto pord = ord.prepare();

    // without boost
    {
      auto prep = filter.prepare(rdr, pord);
      auto docs = prep->execute(*(rdr.begin()), pord);
      auto& doc = docs->attributes().get<irs::document>();
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);

      auto& scr = docs->attributes().get<irs::score>();
      ASSERT_FALSE(!scr);

      // first hit
      {
        ASSERT_TRUE(docs->next());
        scr->evaluate();
        auto doc_boost = pord.get<tests::sort::boost::score_t>(scr->c_str(), 0);
        ASSERT_EQ(irs::boost_t(0), doc_boost);
        ASSERT_EQ(docs->value(), doc->value);
      }

      ASSERT_FALSE(docs->next());
      ASSERT_EQ(docs->value(), doc->value);
    }

    // with boost
    {
      const irs::boost_t value = 5;
      filter.boost(value);

      auto prep = filter.prepare(rdr, pord);
      auto docs = prep->execute(*(rdr.begin()), pord);

      auto& scr = docs->attributes().get<irs::score>();
      ASSERT_FALSE(!scr);

      // first hit
      {
        ASSERT_TRUE(docs->next());
        scr->evaluate();

        auto doc_boost = pord.get<tests::sort::boost::score_t>(scr->c_str(), 0);
        ASSERT_EQ(irs::boost_t(value), doc_boost);
      }

      ASSERT_FALSE(docs->next());
    }
  }

  void by_term_sequential_numeric() {
    // add segment
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
            field.name(irs::string_ref(name));
            field.value(irs::null_token_stream::value_null());
          } else if (data.is_bool() && data.b) {
            doc.insert(std::make_shared<tests::binary_field>());
            auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
            field.name(irs::string_ref(name));
            field.value(irs::boolean_token_stream::value_true());
          } else if (data.is_bool() && !data.b) {
            doc.insert(std::make_shared<tests::binary_field>());
            auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
            field.name(irs::string_ref(name));
            field.value(irs::boolean_token_stream::value_true());
          } else if (data.is_number()) {
            const double dValue = data.as_number<double_t>();
            {
              // 'value' can be interpreted as a double
              doc.insert(std::make_shared<tests::double_field>());
              auto& field = (doc.indexed.end() - 1).as<tests::double_field>();
              field.name(irs::string_ref(name));
              field.value(dValue);
            }

            const float fValue = data.as_number<float_t>();
            {
              // 'value' can be interpreted as a float 
              doc.insert(std::make_shared<tests::float_field>());
              auto& field = (doc.indexed.end() - 1).as<tests::float_field>();
              field.name(irs::string_ref(name));
              field.value(fValue);
            }

            uint64_t lValue = uint64_t(std::ceil(dValue));
            {
              doc.insert(std::make_shared<tests::long_field>());
              auto& field = (doc.indexed.end() - 1).as<tests::long_field>();
              field.name(irs::string_ref(name));
              field.value(lValue);
            }

            {
              doc.insert(std::make_shared<tests::int_field>());
              auto& field = (doc.indexed.end() - 1).as<tests::int_field>();
              field.name(irs::string_ref(name));
              field.value(int32_t(lValue));
            }
          }
        });
      add_segment(gen);
    }

    auto rdr = open_reader();

    // long (20)
    {
      irs::numeric_token_stream stream;
      stream.reset(INT64_C(20));
      auto& term = stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(stream.next());

      irs::by_term query;
      query.field("seq").term(term->value());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 21 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        for (;docs->next();) {
          actual.push_back(docs->value());
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // int (21)
    {
      irs::numeric_token_stream stream;
      stream.reset(INT32_C(21));
      auto& term = stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(stream.next());

      irs::by_term query;
      query.field("seq").term(term->value());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 22 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        auto& doc = docs->attributes().get<irs::document>();
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(docs->value(), doc->value);
        for (;docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // double (90.564) 
    {
      irs::numeric_token_stream stream;
      stream.reset((double_t)90.564);
      auto& term = stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(stream.next());

      irs::by_term query;
      query.field("value").term(term->value());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 13 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        auto& doc = docs->attributes().get<irs::document>();
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(docs->value(), doc->value);
        for (;docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // float (90.564) 
    {
      irs::numeric_token_stream stream;
      stream.reset((float_t)90.564f);
      auto& term = stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(stream.next());

      irs::by_term query;
      query.field("value").term(term->value());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 13 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        auto& doc = docs->attributes().get<irs::document>();
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(docs->value(), doc->value);
        for (;docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // double (100)
    {
      irs::numeric_token_stream stream;
      stream.reset((double_t)100.);
      auto& term = stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(stream.next());

      irs::by_term query;
      query.field("value").term(term->value());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 1, 5, 7, 9, 10 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        auto& doc = docs->attributes().get<irs::document>();
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(docs->value(), doc->value);
        for (;docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // float_t(100)
    {
      irs::numeric_token_stream stream;
      stream.reset((float_t)100.f);
      auto& term = stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(stream.next());

      irs::by_term query;
      query.field("value").term(term->value());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 1, 5, 7, 9, 10 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        auto& doc = docs->attributes().get<irs::document>();
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(docs->value(), doc->value);
        for (;docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // int(100)
    {
      irs::numeric_token_stream stream;
      stream.reset(100);
      auto& term = stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(stream.next());

      irs::by_term query;
      query.field("value").term(term->value());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 1, 5, 7, 9, 10 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        auto& doc = docs->attributes().get<irs::document>();
        ASSERT_TRUE(bool(doc));
        ASSERT_EQ(docs->value(), doc->value);
        for (;docs->next();) {
          actual.push_back(docs->value());
          ASSERT_EQ(docs->value(), doc->value);
        }
      }
      ASSERT_EQ(expected, actual);
    }

    // long(100)
    {
      irs::numeric_token_stream stream;
      stream.reset(INT64_C(100));
      auto& term = stream.attributes().get<irs::term_attribute>();
      ASSERT_TRUE(stream.next());

      irs::by_term query;
      query.field("value").term(term->value());

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 1, 5, 7, 9, 10 };
      std::vector<irs::doc_id_t> actual;

      for (const auto& sub: rdr) {
        auto docs = prepared->execute(sub); 
        auto& doc = docs->attributes().get<irs::document>();
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

  void by_term_sequential_order() {
    // add segment
    {
      tests::json_doc_generator gen(
        resource("simple_sequential.json"),
        &tests::generic_json_field_factory);
      add_segment(gen);
    }

    // read segment
    auto rdr = open_reader();

    {
      // create filter
      irs::by_term filter;
      filter.field("prefix").term("abcy");

      // create order
      size_t collect_field_count = 0;
      size_t collect_term_count = 0;
      size_t finish_count = 0;
      irs::order ord;
      auto& scorer = ord.add<tests::sort::custom_sort>(false);

      scorer.collector_collect_field = [&collect_field_count](const irs::sub_reader&, const irs::term_reader&)->void{
        ++collect_field_count;
      };
      scorer.collector_collect_term = [&collect_term_count](const irs::sub_reader&, const irs::term_reader&, const irs::attribute_view&)->void{
        ++collect_term_count;
      };
      scorer.collectors_collect_ = [&finish_count](irs::byte_type*, const irs::index_reader&, const irs::sort::field_collector*, const irs::sort::term_collector*)->void {
        ++finish_count;
      };
      scorer.prepare_field_collector_ = [&scorer]()->irs::sort::field_collector::ptr {
        return irs::memory::make_unique<tests::sort::custom_sort::prepared::collector>(scorer);
      };
      scorer.prepare_term_collector_ = [&scorer]()->irs::sort::term_collector::ptr {
        return irs::memory::make_unique<tests::sort::custom_sort::prepared::collector>(scorer);
      };

      std::set<irs::doc_id_t> expected{ 31, 32 };
      auto pord = ord.prepare();
      auto prep = filter.prepare(rdr, pord);
      auto docs = prep->execute(*(rdr.begin()), pord);

      auto& scr = docs->attributes().get<irs::score>();
      ASSERT_FALSE(!scr);

      while (docs->next()) {
        scr->evaluate();
        ASSERT_EQ(1, expected.erase(docs->value()));
      }

      ASSERT_TRUE(expected.empty());
      ASSERT_EQ(1, collect_field_count); // 1 field in 1 segment
      ASSERT_EQ(1, collect_term_count); // 1 term in 1 field in 1 segment
      ASSERT_EQ(1, finish_count); // 1 unique term
    }
  }

  void by_term_sequential() {
    // add segment
    {
      tests::json_doc_generator gen(
        resource("simple_sequential.json"),
        &tests::generic_json_field_factory);
      add_segment( gen );
    }

    auto rdr = open_reader();

    // empty query
    check_query(irs::by_term(), docs_t{ }, rdr);

    // empty term
    check_query(irs::by_term().field("name"), docs_t{ }, rdr);

    // empty field
    check_query(irs::by_term().term("xyz"), docs_t{ }, rdr);

    // search : invalid field
    check_query(irs::by_term().field("invalid_field").term( "A"), docs_t{ }, rdr );

    // search : single term
    check_query(irs::by_term().field("name").term("A"), docs_t{ 1 }, rdr);

    // search : all terms
    check_query(
      irs::by_term().field( "same" ).term( "xyz" ),
      docs_t{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
      rdr
    );

    // search : empty result
    check_query(irs::by_term().field("same").term("invalid_term"), docs_t{}, rdr);
  }

  void by_term_schemas() {
    // write segments
    {
      auto writer = open_writer(irs::OM_CREATE);

      std::vector<tests::doc_generator_base::ptr> gens;
      gens.emplace_back(new tests::json_doc_generator(
        resource("AdventureWorks2014.json"),
        &tests::generic_json_field_factory
      ));
      gens.emplace_back(new tests::json_doc_generator(
        resource("AdventureWorks2014Edges.json"),
        &tests::generic_json_field_factory
      ));
      gens.emplace_back(new tests::json_doc_generator(
        resource("Northwnd.json"),
        &tests::generic_json_field_factory
      ));
      gens.emplace_back(new tests::json_doc_generator(
        resource("NorthwndEdges.json"),
        &tests::generic_json_field_factory
      ));
      gens.emplace_back(new tests::json_doc_generator(
        resource("Northwnd.json"),
        &tests::generic_json_field_factory
      ));
      gens.emplace_back(new tests::json_doc_generator(
        resource("NorthwndEdges.json"),
        &tests::generic_json_field_factory
      ));
      add_segments(*writer, gens);
    }

    auto rdr = open_reader();
    check_query(irs::by_term().field("Fields").term("FirstName"), docs_t{ 28, 167, 194 }, rdr);

    // address to the [SDD-179]
    check_query(irs::by_term().field("Name").term("Product"), docs_t{ 32 }, rdr);
  }
}; // term_filter_test_case

TEST_P(term_filter_test_case, by_term) {
  by_term_sequential();
  by_term_schemas();
}

TEST_P(term_filter_test_case, by_term_numeric) {
  by_term_sequential_numeric();
}

TEST_P(term_filter_test_case, by_term_order) {
  by_term_sequential_order();
}

TEST_P(term_filter_test_case, by_term_boost) {
  by_term_sequential_boost();
}

TEST_P(term_filter_test_case, by_term_cost) {
  by_term_sequential_cost();
}

TEST(by_term_test, ctor) {
  irs::by_term q;
  ASSERT_EQ(irs::by_term::type(), q.type());
  ASSERT_TRUE(q.term().empty());
  ASSERT_EQ("", q.field());
  ASSERT_EQ(irs::no_boost(), q.boost());
}

TEST(by_term_test, equal) { 
  irs::by_term q;
  q.field("field").term("term");
  ASSERT_EQ(q, irs::by_term().field("field").term("term"));
  ASSERT_NE(q, irs::by_term().field("field1").term("term"));
}

TEST(by_term_test, boost) {
  // no boost
  {
    irs::by_term q;
    q.field("field").term("term");

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(irs::no_boost(), prepared->boost());
  }

  // with boost
  {
    irs::boost_t boost = 1.5f;
    irs::by_term q;
    q.field("field").term("term");
    q.boost(boost);

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(boost, prepared->boost());
  }
}

INSTANTIATE_TEST_CASE_P(
  term_filter_test,
  term_filter_test_case,
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

NS_END

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
