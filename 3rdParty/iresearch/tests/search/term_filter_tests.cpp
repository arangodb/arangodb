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
#include "search/term_query.hpp"
#include "search/range_filter.hpp"

namespace {

irs::by_term make_filter(
    const irs::string_ref& field,
    const irs::string_ref term) {
  irs::by_term q;
  *q.mutable_field() = field;
  q.mutable_options()->term = irs::ref_cast<irs::byte_type>(term);
  return q;
}

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
    check_query(make_filter("name", ""), docs_t{}, costs_t{0}, rdr);

    // empty field
    check_query(make_filter("", "xyz"), docs_t{}, costs_t{0}, rdr);

    // search : invalid field
    check_query(make_filter("invalid_field", "A"), docs_t{}, costs_t{0}, rdr);

    // search : single term
    check_query(make_filter("name", "A"), docs_t{1}, costs_t{1}, rdr);

    { 
      irs::by_term q = make_filter("name", "A");

      auto prepared = q.prepare(rdr);
      auto sub = rdr.begin();
      auto docs0 = prepared->execute(*sub);
      auto* doc = irs::get<irs::document>(*docs0);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs0->value(), doc->value);
      auto docs1 = prepared->execute(*sub);
      ASSERT_TRUE(docs0->next());
      ASSERT_EQ(docs0->value(), docs1->seek(docs0->value()));
    }

    // search : all terms
    check_query(
      make_filter("same" , "xyz"),
      docs_t{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
      costs_t{ 32 },
      rdr
    );

    // search : empty result
    check_query(make_filter("same", "invalid_term"), docs_t{}, costs_t{0}, rdr);
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
    irs::by_term filter = make_filter("name", "A");
    filter.boost(0.f);

    // create order
    irs::order ord;
    ord.add<tests::sort::boost>(false);
    auto pord = ord.prepare();

    // without boost
    {
      auto prep = filter.prepare(rdr, pord);
      auto docs = prep->execute(*(rdr.begin()), pord);
      auto* doc = irs::get<irs::document>(*docs);
      ASSERT_TRUE(bool(doc));
      ASSERT_EQ(docs->value(), doc->value);

      auto* scr = irs::get<irs::score>(*docs);
      ASSERT_FALSE(!scr);

      // first hit
      {
        ASSERT_TRUE(docs->next());
        auto doc_boost = pord.get<tests::sort::boost::score_t>(scr->evaluate(), 0);
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

      auto* scr = irs::get<irs::score>(*docs);
      ASSERT_FALSE(!scr);

      // first hit
      {
        ASSERT_TRUE(docs->next());
        auto doc_boost = pord.get<tests::sort::boost::score_t>(scr->evaluate(), 0);
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
            field.value(irs::ref_cast<irs::byte_type>(irs::null_token_stream::value_null()));
          } else if (data.is_bool() && data.b) {
            doc.insert(std::make_shared<tests::binary_field>());
            auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
            field.name(irs::string_ref(name));
            field.value(irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true()));
          } else if (data.is_bool() && !data.b) {
            doc.insert(std::make_shared<tests::binary_field>());
            auto& field = (doc.indexed.end() - 1).as<tests::binary_field>();
            field.name(irs::string_ref(name));
            field.value(irs::ref_cast<irs::byte_type>(irs::boolean_token_stream::value_true()));
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
      auto* term = irs::get<irs::term_attribute>(stream);
      ASSERT_TRUE(stream.next());

      irs::by_term query = make_filter("seq", irs::ref_cast<char>(term->value));

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
      auto* term = irs::get<irs::term_attribute>(stream);
      ASSERT_TRUE(stream.next());

      irs::by_term query = make_filter("seq", irs::ref_cast<char>(term->value));

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 22 };
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

    // double (90.564) 
    {
      irs::numeric_token_stream stream;
      stream.reset((double_t)90.564);
      auto* term = irs::get<irs::term_attribute>(stream);
      ASSERT_TRUE(stream.next());

      irs::by_term query = make_filter("value", irs::ref_cast<char>(term->value));

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 13 };
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

    // float (90.564) 
    {
      irs::numeric_token_stream stream;
      stream.reset((float_t)90.564f);
      auto* term = irs::get<irs::term_attribute>(stream);
      ASSERT_TRUE(stream.next());

      irs::by_term query = make_filter("value", irs::ref_cast<char>(term->value));

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 13 };
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

    // double (100)
    {
      irs::numeric_token_stream stream;
      stream.reset((double_t)100.);
      auto* term = irs::get<irs::term_attribute>(stream);
      ASSERT_TRUE(stream.next());

      irs::by_term query = make_filter("value", irs::ref_cast<char>(term->value));

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 1, 5, 7, 9, 10 };
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

    // float_t(100)
    {
      irs::numeric_token_stream stream;
      stream.reset((float_t)100.f);
      auto* term = irs::get<irs::term_attribute>(stream);
      ASSERT_TRUE(stream.next());

      irs::by_term query = make_filter("value", irs::ref_cast<char>(term->value));

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 1, 5, 7, 9, 10 };
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

    // int(100)
    {
      irs::numeric_token_stream stream;
      stream.reset(100);
      auto* term = irs::get<irs::term_attribute>(stream);
      ASSERT_TRUE(stream.next());

      irs::by_term query = make_filter("value", irs::ref_cast<char>(term->value));

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 1, 5, 7, 9, 10 };
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

    // long(100)
    {
      irs::numeric_token_stream stream;
      stream.reset(INT64_C(100));
      auto* term = irs::get<irs::term_attribute>(stream);
      ASSERT_TRUE(stream.next());

      irs::by_term query = make_filter("value", irs::ref_cast<char>(term->value));

      auto prepared = query.prepare(rdr);

      std::vector<irs::doc_id_t> expected { 1, 5, 7, 9, 10 };
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
      irs::by_term filter = make_filter("prefix", "abcy");

      // create order
      size_t collect_field_count = 0;
      size_t collect_term_count = 0;
      size_t finish_count = 0;
      irs::order ord;
      auto& scorer = ord.add<tests::sort::custom_sort>(false);

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

      std::set<irs::doc_id_t> expected{ 31, 32 };
      auto pord = ord.prepare();
      auto prep = filter.prepare(rdr, pord);
      auto docs = prep->execute(*(rdr.begin()), pord);

      auto* scr = irs::get<irs::score>(*docs);
      ASSERT_FALSE(!scr);

      while (docs->next()) {
        const auto* score_value = scr->evaluate();
        UNUSED(score_value);
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
    check_query(make_filter("name", ""), docs_t{ }, rdr);

    // empty field
    check_query(make_filter("", "xyz"), docs_t{ }, rdr);

    // search : invalid field
    check_query(make_filter("invalid_field",  "A"), docs_t{ }, rdr );

    // search : single term
    check_query(make_filter("name", "A"), docs_t{ 1 }, rdr);

    // search : all terms
    check_query(
      make_filter("same" , "xyz"),
      docs_t{ 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32 },
      rdr
    );

    // search : empty result
    check_query(make_filter("same", "invalid_term"), docs_t{}, rdr);
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
    check_query(make_filter("Fields", "FirstName"), docs_t{ 28, 167, 194 }, rdr);

    // address to the [SDD-179]
    check_query(make_filter("Name", "Product"), docs_t{ 32 }, rdr);
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

TEST_P(term_filter_test_case, visit) {
  // add segment
  {
    tests::json_doc_generator gen(
      resource("simple_sequential.json"),
      &tests::generic_json_field_factory);
    add_segment(gen);
  }

  const irs::string_ref field = "prefix";
  const auto term = irs::ref_cast<irs::byte_type>(irs::string_ref("abc"));

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
  ASSERT_EQ((std::vector<std::pair<irs::string_ref, irs::boost_t>>{{"abc", irs::no_boost()}}),
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
  ASSERT_EQ(irs::no_boost(), q.boost());
}

TEST(by_term_test, equal) { 
  irs::by_term q = make_filter("field", "term");
  ASSERT_EQ(q, make_filter("field", "term"));
  ASSERT_NE(q, make_filter("field1", "term"));
}

TEST(by_term_test, boost) {
  // no boost
  {
    irs::by_term q = make_filter("field", "term");

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(irs::no_boost(), prepared->boost());
  }

  // with boost
  {
    irs::boost_t boost = 1.5f;
    irs::by_term q = make_filter("field", "term");
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

}
