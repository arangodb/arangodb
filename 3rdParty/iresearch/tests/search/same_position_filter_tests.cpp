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
#include "store/memory_directory.hpp"
#include "filter_test_case_base.hpp"
#include "analysis/token_attributes.hpp"
#include "search/same_position_filter.hpp"
#include "search/term_filter.hpp" 

NS_BEGIN(tests)

class same_position_filter_test_case : public filter_test_case_base {
 protected:
  void sub_objects_ordered() {
    // add segment
    {
      tests::json_doc_generator gen(
        resource("phrase_sequential.json"),
        [](tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
          if (data.is_string()) { // field
            doc.insert(std::make_shared<templates::text_field<std::string>>(name, data.str), true, false);
          } else if (data.is_number()) { // seq
            const auto value = std::to_string(data.as_number<uint64_t>());
            doc.insert(std::make_shared<templates::string_field>(name, value), false, true);
          }
      });
      add_segment(gen);
      gen.reset();
      add_segment(gen, irs::OM_APPEND);
    }

    // read segment
    auto index = open_reader();

    // collector count (no branches)
    {
      irs::by_same_position filter;

      size_t collect_count = 0;
      size_t finish_count = 0;
      irs::order order;
      auto& scorer = order.add<tests::sort::custom_sort>(false);
      scorer.collector_collect = [&collect_count](const irs::sub_reader&, const irs::term_reader&, const irs::attribute_view&)->void{
        ++collect_count;
      };
      scorer.collector_finish = [&finish_count](irs::attribute_store&, const irs::index_reader&)->void{
        ++finish_count;
      };
      scorer.prepare_collector = [&scorer]()->irs::sort::collector::ptr{
        return irs::memory::make_unique<tests::sort::custom_sort::prepared::collector>(scorer);
      };

      auto pord = order.prepare();
      auto prepared = filter.prepare(index, pord);
      ASSERT_EQ(0, collect_count);
      ASSERT_EQ(0, finish_count); // no terms optimization
    }

    // collector count (single term)
    {
      irs::by_same_position filter;
      filter.push_back("phrase", irs::ref_cast<irs::byte_type>(irs::string_ref("quick")));

      size_t collect_count = 0;
      size_t finish_count = 0;
      irs::order order;
      auto& scorer = order.add<tests::sort::custom_sort>(false);
      scorer.collector_collect = [&collect_count](const irs::sub_reader&, const irs::term_reader&, const irs::attribute_view&)->void{
        ++collect_count;
      };
      scorer.collector_finish = [&finish_count](irs::attribute_store&, const irs::index_reader&)->void{
        ++finish_count;
      };
      scorer.prepare_collector = [&scorer]()->irs::sort::collector::ptr{
        return irs::memory::make_unique<tests::sort::custom_sort::prepared::collector>(scorer);
      };

      auto pord = order.prepare();
      auto prepared = filter.prepare(index, pord);
      ASSERT_EQ(2, collect_count); // 1 term in 2 segments
      ASSERT_EQ(1, finish_count); // 1 unique term
    }

    // collector count (multiple terms)
    {
      irs::by_same_position filter;
      filter.push_back("phrase", irs::ref_cast<irs::byte_type>(irs::string_ref("quick")));
      filter.push_back("phrase", irs::ref_cast<irs::byte_type>(irs::string_ref("brown")));

      size_t collect_count = 0;
      size_t finish_count = 0;
      irs::order order;
      auto& scorer = order.add<tests::sort::custom_sort>(false);
      scorer.collector_collect = [&collect_count](const irs::sub_reader&, const irs::term_reader&, const irs::attribute_view&)->void{
        ++collect_count;
      };
      scorer.collector_finish = [&finish_count](irs::attribute_store&, const irs::index_reader&)->void{
        ++finish_count;
      };
      scorer.prepare_collector = [&scorer]()->irs::sort::collector::ptr{
        return irs::memory::make_unique<tests::sort::custom_sort::prepared::collector>(scorer);
      };

      auto pord = order.prepare();
      auto prepared = filter.prepare(index, pord);
      ASSERT_EQ(4, collect_count); // 2 term in 2 segments
      ASSERT_EQ(2, finish_count); // 2 unique terms
    }
  }

  void sub_objects_unordered() {
    // add segment
    tests::json_doc_generator gen(
      resource("same_position.json"),
      [] (tests::document& doc, const std::string& name, const tests::json_doc_generator::json_value& data) {
        typedef templates::text_field<std::string> text_field;
        if (data.is_string()) {
          // a || b || c
          doc.indexed.push_back(std::make_shared<text_field>(name, data.str));
        } else if (data.is_number()) {
          // _id
          const auto lValue = data.as_number<uint64_t>();

          // 'value' can be interpreted as a double
          doc.insert(std::make_shared<tests::long_field>());
          auto& field = (doc.indexed.end() - 1).as<tests::long_field>();
          field.name(name);
          field.value(lValue);
        }

    });
    add_segment(gen);

    // read segment
    auto index = open_reader();
    ASSERT_EQ(1, index.size());
    auto& segment = *(index.begin());

    irs::bytes_ref actual_value;
    irs::bytes_ref_input in;
    auto column = segment.column_reader("_id");
    ASSERT_NE(nullptr, column);
    auto values = column->values();

    // empty query
    {
      irs::by_same_position q;
      auto prepared = q.prepare(index);
      auto docs = prepared->execute(segment);
      ASSERT_FALSE(docs->next());
    }

    // { a: 100 } - equal to 'by_term' 
    {
      irs::by_same_position query;
      query.push_back("a", irs::ref_cast<irs::byte_type>(irs::string_ref("100")));

      irs::by_term expected_query;
      expected_query.field("a").term("100");

      auto prepared = query.prepare(index);
      auto expected_prepared = expected_query.prepare(index);

      auto docs = prepared->execute(segment);
      auto expected_docs = prepared->execute(segment);

      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), docs->value());
      while (expected_docs->next()) {
        ASSERT_TRUE(docs->next());
        ASSERT_EQ(expected_docs->value(), docs->value());
      }
      ASSERT_FALSE(docs->next());
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), docs->value());
    }

    // { a: 100, b:30, c:6 }
    {
      irs::by_same_position q;
      q.push_back("a", irs::ref_cast<irs::byte_type>(irs::string_ref("100")));
      q.push_back("b", irs::ref_cast<irs::byte_type>(irs::string_ref("30")));
      q.push_back("c", irs::ref_cast<irs::byte_type>(irs::string_ref("6")));

      auto prepared = q.prepare(index);

      // next
      {
        auto docs = prepared->execute(segment);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), docs->value());
        ASSERT_TRUE(docs->next());
        ASSERT_TRUE(values(docs->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(6, irs::read_zvlong(in));
        ASSERT_TRUE(docs->next());
        ASSERT_TRUE(values(docs->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(27, irs::read_zvlong(in));
        ASSERT_FALSE(docs->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), docs->value());
      }

      // seek
      {
        auto docs = prepared->execute(segment);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), docs->value());
        ASSERT_EQ((irs::type_limits<irs::type_t::doc_id_t>::min)() + 6, docs->seek((irs::type_limits<irs::type_t::doc_id_t>::min)()));
        ASSERT_TRUE(values(docs->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(6, irs::read_zvlong(in));
        ASSERT_EQ((irs::type_limits<irs::type_t::doc_id_t>::min)() + 27, docs->seek(27));
        ASSERT_TRUE(values(docs->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(27, irs::read_zvlong(in));
        ASSERT_EQ((irs::type_limits<irs::type_t::doc_id_t>::min)() + 27, docs->seek(8)); // seek backwards
        ASSERT_EQ((irs::type_limits<irs::type_t::doc_id_t>::min)() + 27, docs->seek(27)); // seek to same position
        ASSERT_FALSE(docs->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), docs->value());
      }
    }

    // { c: 8, b:80, a:700 }
    {
      irs::by_same_position q;
      q.push_back("c", irs::ref_cast<irs::byte_type>(irs::string_ref("8")));
      q.push_back("b", irs::ref_cast<irs::byte_type>(irs::string_ref("80")));
      q.push_back("a", irs::ref_cast<irs::byte_type>(irs::string_ref("700")));

      auto prepared = q.prepare(index);

      // next
      {
        auto docs = prepared->execute(segment);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), docs->value());
        ASSERT_TRUE(docs->next());
        ASSERT_TRUE(values(docs->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(14, irs::read_zvlong(in));
        ASSERT_TRUE(docs->next());
        ASSERT_TRUE(values(docs->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(91, irs::read_zvlong(in));
        ASSERT_FALSE(docs->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), docs->value());
      }

      // seek
      {
        auto docs = prepared->execute(segment);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), docs->value());
        ASSERT_EQ((irs::type_limits<irs::type_t::doc_id_t>::min)() + 91, docs->seek(27));
        ASSERT_TRUE(values(docs->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(91, irs::read_zvlong(in));
        ASSERT_EQ((irs::type_limits<irs::type_t::doc_id_t>::min)() + 91, docs->seek(8)); // seek backwards
        ASSERT_EQ((irs::type_limits<irs::type_t::doc_id_t>::min)() + 91, docs->seek(27)); // seek to same position
        ASSERT_FALSE(docs->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), docs->value());
      }
    }

    // { a: 700, b:*, c: 7 }
    {
      irs::by_same_position q;
      q.push_back("a", irs::ref_cast<irs::byte_type>(irs::string_ref("700")));
      q.push_back("c", irs::ref_cast<irs::byte_type>(irs::string_ref("7")));

      auto prepared = q.prepare(index);

      // next
      {
        auto docs = prepared->execute(segment);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), docs->value());
        ASSERT_TRUE(docs->next());
        ASSERT_TRUE(values(docs->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(1, irs::read_zvlong(in));
        ASSERT_TRUE(docs->next());
        ASSERT_TRUE(values(docs->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(6, irs::read_zvlong(in));
        ASSERT_TRUE(docs->next());
        ASSERT_TRUE(values(docs->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(11, irs::read_zvlong(in));
        ASSERT_TRUE(docs->next());
        ASSERT_TRUE(values(docs->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(17, irs::read_zvlong(in));
        ASSERT_TRUE(docs->next());
        ASSERT_TRUE(values(docs->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(18, irs::read_zvlong(in));
        ASSERT_TRUE(docs->next());
        ASSERT_TRUE(values(docs->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(23, irs::read_zvlong(in));
        ASSERT_TRUE(docs->next());
        ASSERT_TRUE(values(docs->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(24, irs::read_zvlong(in));
        ASSERT_TRUE(docs->next());
        ASSERT_TRUE(values(docs->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(28, irs::read_zvlong(in));
        ASSERT_TRUE(docs->next());
        ASSERT_TRUE(values(docs->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(38, irs::read_zvlong(in));
        ASSERT_TRUE(docs->next());
        ASSERT_TRUE(values(docs->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(51, irs::read_zvlong(in));
        ASSERT_TRUE(docs->next());
        ASSERT_TRUE(values(docs->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(66, irs::read_zvlong(in));
        ASSERT_TRUE(docs->next());
        ASSERT_TRUE(values(docs->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(79, irs::read_zvlong(in));
        ASSERT_TRUE(docs->next());
        ASSERT_TRUE(values(docs->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(89, irs::read_zvlong(in));
        ASSERT_FALSE(docs->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), docs->value());
      }

      // seek + next
      {
        auto docs = prepared->execute(segment);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), docs->value());
        ASSERT_TRUE(docs->next());
        ASSERT_TRUE(values(docs->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(1, irs::read_zvlong(in));
        ASSERT_EQ((irs::type_limits<irs::type_t::doc_id_t>::min)() + 28, docs->seek((irs::type_limits<irs::type_t::doc_id_t>::min)() + 28));
        ASSERT_TRUE(values(docs->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(28, irs::read_zvlong(in));
        ASSERT_TRUE(docs->next());
        ASSERT_TRUE(values(docs->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(38, irs::read_zvlong(in));
        ASSERT_EQ((irs::type_limits<irs::type_t::doc_id_t>::min)() + 51, docs->seek(45));
        ASSERT_TRUE(values(docs->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(51, irs::read_zvlong(in));
        ASSERT_TRUE(docs->next());
        ASSERT_TRUE(values(docs->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(66, irs::read_zvlong(in));
        ASSERT_TRUE(docs->next());
        ASSERT_TRUE(values(docs->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(79, irs::read_zvlong(in));
        ASSERT_TRUE(docs->next());
        ASSERT_TRUE(values(docs->value(), actual_value)); in.reset(actual_value);
        ASSERT_EQ(89, irs::read_zvlong(in));
        ASSERT_FALSE(docs->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), docs->value());
      }

      // seek to the end
      {
        auto docs = prepared->execute(segment);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), docs->value());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), docs->seek(irs::type_limits<irs::type_t::doc_id_t>::eof()));
        ASSERT_FALSE(docs->next());
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::eof(), docs->value());
      }
    }
  }
}; // same_position_filter_test_case 

NS_END // tests

// ----------------------------------------------------------------------------
// --SECTION--                                      by_same_position base tests 
// ----------------------------------------------------------------------------

TEST(by_same_position_test, ctor) {
  irs::by_same_position q;
  ASSERT_EQ(irs::by_same_position::type(), q.type());
  ASSERT_TRUE(q.empty());
  ASSERT_EQ(0, q.size());
  ASSERT_EQ(q.begin(), q.end());
  ASSERT_EQ(irs::boost::no_boost(), q.boost());

  auto& features = irs::by_same_position::features();
  ASSERT_EQ(2, features.size());
  ASSERT_TRUE(features.check<irs::frequency>());
  ASSERT_TRUE(features.check<irs::position>());
}

TEST(by_same_position_test, push_back_insert_clear) {
  irs::by_same_position q;

  // push_back 
  {
    q.push_back("speed", irs::ref_cast<irs::byte_type>(irs::string_ref("quick")));
    const std::string color = "color";
    q.push_back(color, irs::ref_cast<irs::byte_type>(irs::string_ref("brown")));
    const irs::bstring fox = irs::ref_cast<irs::byte_type>(irs::string_ref("fox"));
    q.push_back("name", fox);
    const std::string name = "name";
    const irs::bstring squirrel = irs::ref_cast<irs::byte_type>(irs::string_ref("squirrel"));
    q.push_back(name, squirrel);
    ASSERT_FALSE(q.empty());
    ASSERT_EQ(4, q.size());

    // check elements via iterators 
    {
      auto it = q.begin();
      ASSERT_NE(q.end(), it);
      ASSERT_EQ("speed", it->first);
      ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("quick")), it->second);

      ++it;
      ASSERT_NE(q.end(), it);
      ASSERT_EQ("color", it->first);
      ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("brown")), it->second);

      ++it;
      ASSERT_NE(q.end(), it);
      ASSERT_EQ("name", it->first);
      ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("fox")), it->second);

      ++it;
      ASSERT_NE(q.end(), it);
      ASSERT_EQ("name", it->first);
      ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("squirrel")), it->second);

      ++it;
      ASSERT_EQ(q.end(), it);
    }
  }

  q.clear();
  ASSERT_TRUE(q.empty());
  ASSERT_EQ(0, q.size());
  ASSERT_EQ(q.begin(), q.end());
}

TEST(by_same_position_test, boost) {
  // no boost
  {
    // no branches 
    {
      irs::by_same_position q;

      auto prepared = q.prepare(irs::sub_reader::empty());
      ASSERT_EQ(irs::boost::no_boost(), irs::boost::extract(prepared->attributes()));
    }

    // single term
    {
      irs::by_same_position q;
      q.push_back("field", irs::ref_cast<irs::byte_type>(irs::string_ref("quick")));

      auto prepared = q.prepare(irs::sub_reader::empty());
      ASSERT_EQ(irs::boost::no_boost(), irs::boost::extract(prepared->attributes()));
    }

    // multiple terms
    {
      irs::by_same_position q;
      q.push_back("field", irs::ref_cast<irs::byte_type>(irs::string_ref("quick")));
      q.push_back("field", irs::ref_cast<irs::byte_type>(irs::string_ref("brown")));

      auto prepared = q.prepare(irs::sub_reader::empty());
      ASSERT_EQ(irs::boost::no_boost(), irs::boost::extract(prepared->attributes()));
    }
  }

  // with boost
  {
    iresearch::boost::boost_t boost = 1.5f;
    
    // no terms, return empty query
    {
      irs::by_same_position q;
      q.boost(boost);

      auto prepared = q.prepare(irs::sub_reader::empty());
      ASSERT_EQ(irs::boost::no_boost(), irs::boost::extract(prepared->attributes()));
    }

    // single term
    {
      irs::by_same_position q;
      q.push_back("field", irs::ref_cast<irs::byte_type>(irs::string_ref("quick")));
      q.boost(boost);

      auto prepared = q.prepare(irs::sub_reader::empty());
      ASSERT_EQ(boost, irs::boost::extract(prepared->attributes()));
    }

    // single multiple terms 
    {
      irs::by_same_position q;
      q.push_back("field", irs::ref_cast<irs::byte_type>(irs::string_ref("quick")));
      q.push_back("field", irs::ref_cast<irs::byte_type>(irs::string_ref("brown")));
      q.boost(boost);

      auto prepared = q.prepare(irs::sub_reader::empty());
      ASSERT_EQ(boost, irs::boost::extract(prepared->attributes()));
    }
  }
}

TEST(by_same_position_test, equal) {
  ASSERT_EQ(irs::by_same_position(), irs::by_same_position());

  {
    irs::by_same_position q0;
    q0.push_back("speed", iresearch::ref_cast<iresearch::byte_type>(iresearch::string_ref("quick")));
    q0.push_back("color", iresearch::ref_cast<iresearch::byte_type>(iresearch::string_ref("brown")));

    irs::by_same_position q1;
    q1.push_back("speed", iresearch::ref_cast<iresearch::byte_type>(iresearch::string_ref("quick")));
    q1.push_back("color", iresearch::ref_cast<iresearch::byte_type>(iresearch::string_ref("brown")));
    ASSERT_EQ(q0, q1);
  }

  {
    irs::by_same_position q0;
    q0.push_back("speed", iresearch::ref_cast<iresearch::byte_type>(iresearch::string_ref("quick")));
    q0.push_back("color", iresearch::ref_cast<iresearch::byte_type>(iresearch::string_ref("brown")));
    q0.push_back("name", iresearch::ref_cast<iresearch::byte_type>(iresearch::string_ref("fox")));

    irs::by_same_position q1;
    q1.push_back("speed", iresearch::ref_cast<iresearch::byte_type>(iresearch::string_ref("quick")));
    q1.push_back("color", iresearch::ref_cast<iresearch::byte_type>(iresearch::string_ref("brown")));
    q1.push_back("name", iresearch::ref_cast<iresearch::byte_type>(iresearch::string_ref("squirrel")));
    ASSERT_NE(q0, q1);
  }

  {
    irs::by_same_position q0;
    q0.push_back("Speed", iresearch::ref_cast<iresearch::byte_type>(iresearch::string_ref("quick")));
    q0.push_back("color", iresearch::ref_cast<iresearch::byte_type>(iresearch::string_ref("brown")));
    q0.push_back("name", iresearch::ref_cast<iresearch::byte_type>(iresearch::string_ref("fox")));

    irs::by_same_position q1;
    q1.push_back("speed", iresearch::ref_cast<iresearch::byte_type>(iresearch::string_ref("quick")));
    q1.push_back("color", iresearch::ref_cast<iresearch::byte_type>(iresearch::string_ref("brown")));
    q1.push_back("name", iresearch::ref_cast<iresearch::byte_type>(iresearch::string_ref("fox")));
    ASSERT_NE(q0, q1);
  }

  {
    irs::by_same_position q0;
    q0.push_back("speed", iresearch::ref_cast<iresearch::byte_type>(iresearch::string_ref("quick")));
    q0.push_back("color", iresearch::ref_cast<iresearch::byte_type>(iresearch::string_ref("brown")));

    irs::by_same_position q1;
    q1.push_back("speed", iresearch::ref_cast<iresearch::byte_type>(iresearch::string_ref("quick")));
    q1.push_back("color", iresearch::ref_cast<iresearch::byte_type>(iresearch::string_ref("brown")));
    q1.push_back("name", iresearch::ref_cast<iresearch::byte_type>(iresearch::string_ref("fox")));
    ASSERT_NE(q0, q1);
  }
}

// ----------------------------------------------------------------------------
// --SECTION--                           memory_directory + iresearch_format_10
// ----------------------------------------------------------------------------

class memory_same_position_filter_test_case : public tests::same_position_filter_test_case {
protected:
  virtual irs::directory* get_directory() override {
    return new irs::memory_directory();
  }

  virtual irs::format::ptr get_codec() override {
    return irs::formats::get("1_0");
  }
};

TEST_F(memory_same_position_filter_test_case, by_same_position) {
  sub_objects_ordered();
  sub_objects_unordered();
}

// ----------------------------------------------------------------------------
// --SECTION--                               fs_directory + iresearch_format_10
// ----------------------------------------------------------------------------

class fs_same_position_filter_test_case : public tests::same_position_filter_test_case {
protected:
  virtual irs::directory* get_directory() override {
    return new irs::memory_directory();
  }

  virtual irs::format::ptr get_codec() override {
    return irs::formats::get("1_0");
  }
};

TEST_F(fs_same_position_filter_test_case, by_same_position) {
  sub_objects_ordered();
  sub_objects_unordered();
}

// -----------------------------------------------------------------------------
// --SECTION--                                                       END-OF-FILE
// -----------------------------------------------------------------------------
