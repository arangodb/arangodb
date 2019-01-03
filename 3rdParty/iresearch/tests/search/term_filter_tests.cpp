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
#include "store/memory_directory.hpp"
#include "store/fs_directory.hpp"
#include "formats/formats_10.hpp"

NS_BEGIN(tests)

class term_filter_test_case : public filter_test_case_base {
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

    // create order
    iresearch::order ord;
    ord.add<tests::sort::boost>(false);
    auto pord = ord.prepare();

    // without boost
    {
      auto prep = filter.prepare(rdr, pord);
      auto docs = prep->execute(*(rdr.begin()), pord);

      auto& scr = docs->attributes().get<iresearch::score>();
      ASSERT_FALSE(!scr);

      // first hit
      {
        ASSERT_TRUE(docs->next());
        scr->evaluate();
        auto doc_boost = pord.get<tests::sort::boost::score_t>(scr->c_str(), 0);
        ASSERT_EQ(iresearch::boost::boost_t(0), doc_boost);
      }

      ASSERT_FALSE(docs->next());
    }

    // with boost
    {
      const iresearch::boost::boost_t value = 5;
      filter.boost(value);

      auto prep = filter.prepare(rdr, pord);
      auto docs = prep->execute(*(rdr.begin()), pord);

      auto& scr = docs->attributes().get<iresearch::score>();
      ASSERT_FALSE(!scr);

      // first hit
      {
        ASSERT_TRUE(docs->next());
        scr->evaluate();

        auto doc_boost = pord.get<tests::sort::boost::score_t>(scr->c_str(), 0);
        ASSERT_EQ(iresearch::boost::boost_t(value), doc_boost);
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
            doc.insert(std::make_shared<templates::string_field>(
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
            const double dValue = data.as_number<double_t>();
            {
              // 'value' can be interpreted as a double
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
        for (;docs->next();) {
          actual.push_back(docs->value());
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
        for (;docs->next();) {
          actual.push_back(docs->value());
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
        for (;docs->next();) {
          actual.push_back(docs->value());
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
        for (;docs->next();) {
          actual.push_back(docs->value());
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
        for (;docs->next();) {
          actual.push_back(docs->value());
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
        for (;docs->next();) {
          actual.push_back(docs->value());
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
        for (;docs->next();) {
          actual.push_back(docs->value());
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
      size_t collect_count = 0;
      size_t finish_count = 0;
      iresearch::order ord;
      auto& scorer = ord.add<sort::custom_sort>(false);
      scorer.collector_collect = [&collect_count](const irs::sub_reader&, const irs::term_reader&, const irs::attribute_view&)->void{
        ++collect_count;
      };
      scorer.collector_finish = [&finish_count](irs::attribute_store&, const irs::index_reader&)->void{
        ++finish_count;
      };
      scorer.prepare_collector = [&scorer]()->irs::sort::collector::ptr{
        return irs::memory::make_unique<sort::custom_sort::prepared::collector>(scorer);
      };

      std::set<irs::doc_id_t> expected{ 31, 32 };
      auto pord = ord.prepare();
      auto prep = filter.prepare(rdr, pord);
      auto docs = prep->execute(*(rdr.begin()), pord);

      auto& scr = docs->attributes().get<iresearch::score>();
      ASSERT_FALSE(!scr);

      while (docs->next()) {
        scr->evaluate();
        ASSERT_EQ(1, expected.erase(docs->value()));
      }

      ASSERT_TRUE(expected.empty());
      ASSERT_EQ(1, collect_count);
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
      auto writer = open_writer(iresearch::OM_CREATE);

      std::vector<doc_generator_base::ptr> gens;
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
};

NS_END // tests

// ----------------------------------------------------------------------------
// --SECTION--                                               by_term base tests 
// ----------------------------------------------------------------------------

TEST(by_term_test, ctor) {
  irs::by_term q;
  ASSERT_EQ(irs::by_term::type(), q.type());
  ASSERT_TRUE(q.term().empty());
  ASSERT_EQ("", q.field());
  ASSERT_EQ(irs::boost::no_boost(), q.boost());
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
    ASSERT_EQ(irs::boost::no_boost(), irs::boost::extract(prepared->attributes()));
  }

  // with boost
  {
    iresearch::boost::boost_t boost = 1.5f;
    irs::by_term q;
    q.field("field").term("term");
    q.boost(boost);

    auto prepared = q.prepare(irs::sub_reader::empty());
    ASSERT_EQ(boost, irs::boost::extract(prepared->attributes()));
  }
}

// ----------------------------------------------------------------------------
// --SECTION--                           memory_directory + iresearch_format_10
// ----------------------------------------------------------------------------

class memory_term_filter_test_case : public tests::term_filter_test_case {
protected:
  virtual irs::directory* get_directory() override {
    return new irs::memory_directory();
  }

  virtual irs::format::ptr get_codec() override {
    return irs::formats::get("1_0");
  }
};

TEST_F(memory_term_filter_test_case, by_term) {
  by_term_sequential();  
  by_term_schemas();
}

TEST_F(memory_term_filter_test_case, by_term_numeric) {
  by_term_sequential_numeric();
}

TEST_F(memory_term_filter_test_case, by_term_order) {
  by_term_sequential_order();
}

TEST_F(memory_term_filter_test_case, by_term_boost) {
  by_term_sequential_boost();
}

TEST_F(memory_term_filter_test_case, by_term_cost) {
  by_term_sequential_cost();
}

// ----------------------------------------------------------------------------
// --SECTION--                               fs_directory + iresearch_format_10
// ----------------------------------------------------------------------------

class fs_term_filter_test_case : public tests::term_filter_test_case {
protected:
  virtual irs::directory* get_directory() override {
    auto dir = test_dir();

    dir /= "index";

    return new iresearch::fs_directory(dir.utf8());
  }

  virtual irs::format::ptr get_codec() override {
    return irs::formats::get("1_0");
  }
};

TEST_F(fs_term_filter_test_case, by_term) {
  by_term_sequential();
  by_term_schemas();
}

TEST_F(fs_term_filter_test_case, by_term_boost) {
  by_term_sequential_boost();
}

TEST_F(fs_term_filter_test_case, by_term_numeric) {
  by_term_sequential_numeric();
}

TEST_F(fs_term_filter_test_case, by_term_order) {
  by_term_sequential_order();
}

TEST_F(fs_term_filter_test_case, by_term_cost) {
  by_term_sequential_cost();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
