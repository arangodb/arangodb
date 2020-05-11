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

#include "index_tests.hpp"
#include "formats/formats_10.hpp"
#include "iql/query_builder.hpp"
#include "store/memory_directory.hpp"
#include "utils/type_limits.hpp"
#include "utils/lz4compression.hpp"
#include "index/merge_writer.hpp"
#include "index/comparer.hpp"

namespace tests {
  class merge_writer_tests: public ::testing::Test {

    virtual void SetUp() {
      // Code here will be called immediately after the constructor (right before each test).
    }

    virtual void TearDown() {
      // Code here will be called immediately after each test (right before the destructor).
    }
  };

  template<typename T>
  void validate_terms(
      const irs::sub_reader& segment,
      const irs::term_reader& terms,
      uint64_t doc_count,
      const irs::bytes_ref& min,
      const irs::bytes_ref& max,
      size_t term_size,
      const irs::flags& term_features,
      std::unordered_map<T, std::unordered_set<irs::doc_id_t>>& expected_terms,
      size_t* frequency = nullptr,
      std::vector<uint32_t>* position = nullptr) {
    ASSERT_EQ(doc_count, terms.docs_count());
    ASSERT_EQ((max), (terms.max)());
    ASSERT_EQ((min), (terms.min)());
    ASSERT_EQ(term_size, terms.size());
    ASSERT_EQ(term_features, terms.meta().features);

    for (auto term_itr = terms.iterator(); term_itr->next();) {
      auto itr = expected_terms.find(term_itr->value());

      ASSERT_NE(expected_terms.end(), itr);

      for (auto docs_itr = segment.mask(term_itr->postings(term_features)); docs_itr->next();) {
        ASSERT_EQ(1, itr->second.erase(docs_itr->value()));
        ASSERT_TRUE(docs_itr->get(irs::type<irs::document>::id()));

        if (frequency) {
          ASSERT_TRUE(docs_itr->get(irs::type<irs::frequency>::id()));
          ASSERT_EQ(*frequency, irs::get<irs::frequency>(*docs_itr)->value);
        }

        if (position) {
          auto* docs_itr_pos = irs::get_mutable<irs::position>(docs_itr.get());
          ASSERT_TRUE(docs_itr_pos);

          for (auto pos: *position) {
            ASSERT_TRUE(docs_itr_pos->next());
            ASSERT_EQ(pos, docs_itr_pos->value());
          }

          ASSERT_FALSE(docs_itr_pos->next());
        }
      }

      ASSERT_TRUE(itr->second.empty());
      expected_terms.erase(itr);
    }

    ASSERT_TRUE(expected_terms.empty());
  }
}

using namespace tests;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

TEST_F(merge_writer_tests, test_merge_writer_columns_remove) {
  const irs::flags STRING_FIELD_FEATURES{
    irs::type<irs::frequency>::get(), irs::type<irs::position>::get()
  };

  const irs::flags TEXT_FIELD_FEATURES{
    irs::type<irs::frequency>::get(), irs::type<irs::position>::get(),
    irs::type<irs::offset>::get(), irs::type<irs::payload>::get()
  };

  std::string string1;
  std::string string2;
  std::string string3;
  std::string string4;

  string1.append("string1_data");
  string2.append("string2_data");
  string3.append("string3_data");
  string4.append("string4_data");

  tests::document doc1; // doc_int, doc_string
  tests::document doc2; // doc_string, doc_int
  tests::document doc3; // doc_string, doc_int
  tests::document doc4; // doc_string, another_column

  doc1.insert(std::make_shared<tests::int_field>()); {
    auto& field = doc1.indexed.back<tests::int_field>();
    field.name(irs::string_ref("doc_int"));
    field.value(42 * 1);
  }
  doc1.insert(
    std::make_shared<tests::templates::string_field>("doc_string", string1)
  );

  doc2.insert(std::make_shared<tests::templates::string_field>("doc_string", string2));
  doc2.insert(std::make_shared<tests::int_field>());
  {
    auto& field = doc2.indexed.back<tests::int_field>();
    field.name(irs::string_ref("doc_int"));
    field.value(42 * 2);
  }

  doc3.insert(std::make_shared<tests::templates::string_field>("doc_string", string3));
  doc3.insert(std::make_shared<tests::int_field>()); {
    auto& field = doc3.indexed.back<tests::int_field>();
    field.name(irs::string_ref("doc_int"));
    field.value(42 * 3);
  }

  doc4.insert(std::make_shared<tests::templates::string_field>("doc_string", string4));
  doc4.insert(std::make_shared<tests::templates::string_field>("another_column", "another_value"));

  auto codec_ptr = irs::formats::get("1_0");
  irs::memory_directory dir;

  // populate directory
  {
    auto query_doc4 = irs::iql::query_builder().build("doc_string==string4_data", std::locale::classic());
    auto writer = irs::index_writer::make(dir, codec_ptr, irs::OM_CREATE);
    ASSERT_TRUE(insert(*writer, doc1.indexed.end(), doc1.indexed.end(), doc1.stored.begin(), doc1.stored.end()));
    ASSERT_TRUE(insert(*writer, doc3.indexed.end(), doc3.indexed.end(), doc3.stored.begin(), doc3.stored.end()));
    writer->commit();
    ASSERT_TRUE(insert(*writer, doc2.indexed.end(), doc2.indexed.end(), doc2.stored.begin(), doc2.stored.end()));
    ASSERT_TRUE(insert(*writer, doc4.indexed.begin(), doc4.indexed.end(), doc4.stored.begin(), doc4.stored.end()));
    writer->commit();
    writer->documents().remove(std::move(query_doc4.filter));
    writer->commit();
  }

  irs::column_info_provider_t column_info = [](const irs::string_ref&) {
    return irs::column_info(irs::type<irs::compression::lz4>::get(), irs::compression::options{}, true );
  };

  auto reader = irs::directory_reader::open(dir, codec_ptr);
  irs::merge_writer writer(dir, column_info);

  ASSERT_EQ(2, reader.size());
  ASSERT_EQ(2, reader[0].docs_count());
  ASSERT_EQ(2, reader[1].docs_count());

  // check for columns segment 0
  {
    auto& segment = reader[0];

    auto columns = segment.columns();
    ASSERT_TRUE(columns->next());
    ASSERT_EQ("doc_int", columns->value().name);
    ASSERT_EQ(0, columns->value().id);
    ASSERT_TRUE(columns->next());
    ASSERT_EQ("doc_string", columns->value().name);
    ASSERT_EQ(1, columns->value().id);
    ASSERT_FALSE(columns->next());

    // check 'doc_int' column
    {
      std::unordered_map<int, irs::doc_id_t> expected_values{
        { 1 * 42, 1 },
        { 3 * 42, 2 }
      };

      size_t calls_count = 0;
      auto reader = [&calls_count, &expected_values] (irs::doc_id_t doc, const irs::bytes_ref& value) {
        ++calls_count;
        irs::bytes_ref_input in(value);
        const auto actual_value = irs::read_zvint(in);

        auto it = expected_values.find(actual_value);
        if (it == expected_values.end()) {
          // can't find value
          return false;
        }

        if (it->second != doc) {
          // wrong document
          return false;
        }

        return true;
      };

      // read values for 'doc_int'
      auto* meta = segment.column("doc_int");
      ASSERT_NE(nullptr, meta);
      auto* column = segment.column_reader(meta->id);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->name));
      ASSERT_TRUE(column->visit(reader));
      ASSERT_EQ(expected_values.size(), calls_count);
    }

    // check 'doc_string' column
    {
      std::unordered_map <irs::string_ref, irs::doc_id_t > expected_values{
        { "string1_data", 1 },
        { "string3_data", 2 }
      };

      size_t calls_count = 0;
      auto reader = [&calls_count, &expected_values] (irs::doc_id_t doc, const irs::bytes_ref& actual_value) {
        ++calls_count;

        const auto actual_value_string = irs::to_string<irs::string_ref>(actual_value.c_str());

        auto it = expected_values.find(actual_value_string);
        if (it == expected_values.end()) {
          // can't find value
          return false;
        }

        if (it->second != doc) {
          // wrong document
          return false;
        }

        return true;
      };

      // read values for 'doc_string'
      auto* meta = segment.column("doc_string");
      ASSERT_NE(nullptr, meta);
      auto* column = segment.column_reader(meta->id);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->name));
      ASSERT_TRUE(column->visit(reader));
      ASSERT_EQ(expected_values.size(), calls_count);
    }

    // check wrong column
    {
      ASSERT_EQ(nullptr, segment.column("invalid_column"));
      ASSERT_EQ(nullptr, segment.column_reader("invalid_column"));
    }
  }
  
  // check for columns segment 1
  {
    auto& segment = reader[1];

    auto columns = segment.columns();
    ASSERT_TRUE(columns->next());
    ASSERT_EQ("another_column", columns->value().name);
    ASSERT_EQ(2, columns->value().id);
    ASSERT_TRUE(columns->next());
    ASSERT_EQ("doc_int", columns->value().name);
    ASSERT_EQ(1, columns->value().id);
    ASSERT_TRUE(columns->next());
    ASSERT_EQ("doc_string", columns->value().name);
    ASSERT_EQ(0, columns->value().id);
    ASSERT_FALSE(columns->next());

    // check 'doc_int' column
    {
      std::unordered_map<int, irs::doc_id_t> expected_values{
        { 2 * 42, 1 },
      };

      size_t calls_count = 0;
      auto reader = [&calls_count, &expected_values] (irs::doc_id_t doc, const irs::bytes_ref& in) {
        ++calls_count;
        irs::bytes_ref_input stream(in);
        const auto actual_value = irs::read_zvint(stream);

        auto it = expected_values.find(actual_value);
        if (it == expected_values.end()) {
          // can't find value
          return false;
        }

        if (it->second != doc) {
          // wrong document
          return false;
        }

        return true;
      };

      // read values for 'doc_int'
      auto* meta = segment.column("doc_int");
      ASSERT_NE(nullptr, meta);
      auto* column = segment.column_reader(meta->id);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->name));
      ASSERT_TRUE(column->visit(reader));
      ASSERT_EQ(expected_values.size(), calls_count);
    }

    // check 'doc_string' column
    {
      std::unordered_map <irs::string_ref, irs::doc_id_t > expected_values{
        { "string2_data", 1 },
        { "string4_data", 2 }
      };

      size_t calls_count = 0;
      auto reader = [&calls_count, &expected_values] (irs::doc_id_t doc, const irs::bytes_ref& in) {
        ++calls_count;
        irs::bytes_ref_input stream(in);
        const auto actual_value = irs::read_string<std::string>(stream);

        auto it = expected_values.find(actual_value);
        if (it == expected_values.end()) {
          // can't find value
          return false;
        }

        if (it->second != doc) {
          // wrong document
          return false;
        }

        return true;
      };

      // read values for 'doc_string'
      auto* meta = segment.column("doc_string");
      ASSERT_NE(nullptr, meta);
      auto* column = segment.column_reader(meta->id);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->name));
      ASSERT_TRUE(column->visit(reader));
      ASSERT_EQ(expected_values.size(), calls_count);
    }

    // check 'another_column' column
    {
      std::unordered_map <std::string, irs::doc_id_t > expected_values{
        { "another_value", 2 }
      };

      size_t calls_count = 0;
      auto reader = [&calls_count, &expected_values] (irs::doc_id_t doc, const irs::bytes_ref& in) {
        ++calls_count;
        irs::bytes_ref_input stream(in);
        const auto actual_value = irs::read_string<std::string>(stream);

        auto it = expected_values.find(actual_value);
        if (it == expected_values.end()) {
          // can't find value
          return false;
        }

        if (it->second != doc) {
          // wrong document
          return false;
        }

        return true;
      };

      // read values for 'another_column'
      auto* meta = segment.column("another_column");
      ASSERT_NE(nullptr, meta);
      auto* column = segment.column_reader(meta->id);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->name));
      ASSERT_TRUE(column->visit(reader));
      ASSERT_EQ(expected_values.size(), calls_count);
    }

    // check invalid column 
    {
      ASSERT_EQ(nullptr, segment.column("invalid_column"));
      ASSERT_EQ(nullptr, segment.column_reader("invalid_column"));
    }
  }

  writer.add(reader[0]);
  writer.add(reader[1]);

  irs::index_meta::index_segment_t index_segment;

  index_segment.meta.codec = codec_ptr;
  writer.flush(index_segment);

  {
    auto segment = irs::segment_reader::open(dir, index_segment.meta);
    ASSERT_EQ(3, segment.docs_count());

    auto columns = segment.columns();
    ASSERT_TRUE(columns->next());
    ASSERT_EQ("doc_int", columns->value().name);
    ASSERT_EQ(0, columns->value().id); // 0 since 'doc_int' < 'doc_string'
    ASSERT_TRUE(columns->next());
    ASSERT_EQ("doc_string", columns->value().name);
    ASSERT_EQ(1, columns->value().id);
    ASSERT_FALSE(columns->next());

    // check 'doc_int' column
    {
      std::unordered_map<int, irs::doc_id_t> expected_values{
        // segment 0
        { 1 * 42, 1 },
        { 3 * 42, 2 },
        // segment 1
        { 2 * 42, 3 }
      };

      size_t calls_count = 0;
      auto reader = [&calls_count, &expected_values] (irs::doc_id_t doc, const irs::bytes_ref& value) {
        ++calls_count;
        irs::bytes_ref_input in(value);
        const auto actual_value = irs::read_zvint(in);

        auto it = expected_values.find(actual_value);
        if (it == expected_values.end()) {
          // can't find value
          return false;
        }

        if (it->second != doc) {
          // wrong document
          return false;
        }

        return true;
      };

      // read values for 'doc_int'
      auto* meta = segment.column("doc_int");
      ASSERT_NE(nullptr, meta);
      auto* column = segment.column_reader(meta->id);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->name));
      ASSERT_TRUE(column->visit(reader));
      ASSERT_EQ(expected_values.size(), calls_count);
    }

    // check 'doc_string' column
    {
      std::unordered_map <irs::string_ref, irs::doc_id_t > expected_values{
        // segment 0
        { "string1_data", 1 },
        { "string3_data", 2 },
        // segment 1
        { "string2_data", 3 }
      };

      size_t calls_count = 0;
      auto reader = [&calls_count, &expected_values] (irs::doc_id_t doc, const irs::bytes_ref& value) {
        ++calls_count;
        irs::bytes_ref_input in(value);
        const auto actual_value = irs::read_string<std::string>(in);

        auto it = expected_values.find(actual_value);
        if (it == expected_values.end()) {
          // can't find value
          return false;
        }

        if (it->second != doc) {
          // wrong document
          return false;
        }

        return true;
      };

      // read values for 'doc_string'
      auto* meta = segment.column("doc_string");
      ASSERT_NE(nullptr, meta);
      auto* column = segment.column_reader(meta->id);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->name));
      ASSERT_TRUE(column->visit(reader));
      ASSERT_EQ(expected_values.size(), calls_count);
    }

    // check that 'another_column' has been removed
    {
      ASSERT_EQ(nullptr, segment.column("another_column"));
      ASSERT_EQ(nullptr, segment.column_reader("another_column"));
    }
  }
}

TEST_F(merge_writer_tests, test_merge_writer_columns) {
  irs::flags STRING_FIELD_FEATURES{ irs::type<irs::frequency>::get(), irs::type<irs::position>::get() };
  irs::flags TEXT_FIELD_FEATURES{ irs::type<irs::frequency>::get(), irs::type<irs::position>::get(), irs::type<irs::offset>::get(), irs::type<irs::payload>::get() };

  std::string string1;
  std::string string2;
  std::string string3;
  std::string string4;

  string1.append("string1_data");
  string2.append("string2_data");
  string3.append("string3_data");
  string4.append("string4_data");

  tests::document doc1; // doc_string, doc_int
  tests::document doc2; // doc_string, doc_int
  tests::document doc3; // doc_string, doc_int
  tests::document doc4; // doc_string

  doc1.insert(std::make_shared<tests::int_field>()); 
  {
    auto& field = doc1.indexed.back<tests::int_field>();
    field.name(irs::string_ref("doc_int"));
    field.value(42 * 1);
  }
  doc1.insert(std::make_shared<tests::templates::string_field>("doc_string", string1));

  doc2.insert(std::make_shared<tests::templates::string_field>("doc_string", string2));
  doc2.insert(std::make_shared<tests::int_field>()); {
    auto& field = doc2.indexed.back<tests::int_field>();
    field.name(irs::string_ref("doc_int"));
    field.value(42 * 2);
  }
  
  doc3.insert(std::make_shared<tests::templates::string_field>("doc_string", string3));
  doc3.insert(std::make_shared<tests::int_field>()); 
  {
    auto& field = doc3.indexed.back<tests::int_field>();
    field.name(irs::string_ref("doc_int"));
    field.value(42 * 3);
  }

  doc4.insert(std::make_shared<tests::templates::string_field>("doc_string", string4));

  auto codec_ptr = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec_ptr);
  irs::memory_directory dir;

  // populate directory
  {
    auto writer = irs::index_writer::make(dir, codec_ptr, irs::OM_CREATE);
    ASSERT_TRUE(insert(*writer, doc1.indexed.end(), doc1.indexed.end(), doc1.stored.begin(), doc1.stored.end()));
    ASSERT_TRUE(insert(*writer, doc3.indexed.end(), doc3.indexed.end(), doc3.stored.begin(), doc3.stored.end()));
    writer->commit();
    ASSERT_TRUE(insert(*writer, doc2.indexed.end(), doc2.indexed.end(), doc2.stored.begin(), doc2.stored.end()));
    ASSERT_TRUE(insert(*writer, doc4.indexed.end(), doc4.indexed.end(), doc4.stored.begin(), doc4.stored.end()));
    writer->commit();
  }

  irs::column_info_provider_t column_info = [](const irs::string_ref&) {
    return irs::column_info(irs::type<irs::compression::lz4>::get(), irs::compression::options{}, true );
  };

  auto reader = irs::directory_reader::open(dir, codec_ptr);
  irs::merge_writer writer(dir, column_info);

  ASSERT_EQ(2, reader.size());
  ASSERT_EQ(2, reader[0].docs_count());
  ASSERT_EQ(2, reader[1].docs_count());

  // check for columns segment 0
  {
    auto& segment = reader[0];

    auto columns = segment.columns();
    ASSERT_TRUE(columns->next());
    ASSERT_EQ("doc_int", columns->value().name);
    ASSERT_EQ(0, columns->value().id);
    ASSERT_TRUE(columns->next());
    ASSERT_EQ("doc_string", columns->value().name);
    ASSERT_EQ(1, columns->value().id);
    ASSERT_FALSE(columns->next());
    ASSERT_FALSE(columns->next());

    // check 'doc_int' column
    {
      std::unordered_map<int, irs::doc_id_t> expected_values{
        { 1 * 42, 1 },
        { 3 * 42, 2 }
      };

      size_t calls_count = 0;
      auto reader = [&calls_count, &expected_values] (irs::doc_id_t doc, const irs::bytes_ref& value) {
        ++calls_count;
        irs::bytes_ref_input in(value);
        const auto actual_value = irs::read_zvint(in);

        auto it = expected_values.find(actual_value);
        if (it == expected_values.end()) {
          // can't find value
          return false;
        }

        if (it->second != doc) {
          // wrong document
          return false;
        }

        return true;
      };

      // read values for 'doc_int'
      auto* meta = segment.column("doc_int");
      ASSERT_NE(nullptr, meta);
      auto* column = segment.column_reader(meta->id);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->name));
      ASSERT_TRUE(column->visit(reader));
      ASSERT_EQ(expected_values.size(), calls_count);
    }

    // check 'doc_string' column
    {
      std::unordered_map <irs::string_ref, irs::doc_id_t > expected_values{
        { "string1_data", 1 },
        { "string3_data", 2 }
      };

      size_t calls_count = 0;
      auto reader = [&calls_count, &expected_values] (irs::doc_id_t doc, const irs::bytes_ref& value) {
        ++calls_count;
        irs::bytes_ref_input in(value);
        const auto actual_value = irs::read_string<std::string>(in);

        auto it = expected_values.find(actual_value);
        if (it == expected_values.end()) {
          // can't find value
          return false;
        }

        if (it->second != doc) {
          // wrong document
          return false;
        }

        return true;
      };

      // read values for 'doc_string'
      auto* meta = segment.column("doc_string");
      ASSERT_NE(nullptr, meta);
      auto* column = segment.column_reader(meta->id);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->name));
      ASSERT_TRUE(column->visit(reader));
      ASSERT_EQ(expected_values.size(), calls_count);
    }

    // check wrong column
    {
      ASSERT_EQ(nullptr, segment.column("invalid_column"));
      ASSERT_EQ(nullptr, segment.column_reader("invalid_column"));
    }
  }

  // check for columns segment 1
  {
    auto& segment = reader[1];

    auto columns = segment.columns();
    ASSERT_TRUE(columns->next());
    ASSERT_EQ("doc_int", columns->value().name);
    ASSERT_EQ(1, columns->value().id);
    ASSERT_TRUE(columns->next());
    ASSERT_EQ("doc_string", columns->value().name);
    ASSERT_EQ(0, columns->value().id);
    ASSERT_FALSE(columns->next());

    // check 'doc_int' column
    {
      std::unordered_map<int, irs::doc_id_t> expected_values{
        { 2 * 42, 1 },
      };

      size_t calls_count = 0;
      auto reader = [&calls_count, &expected_values] (irs::doc_id_t doc, const irs::bytes_ref& value) {
        ++calls_count;
        irs::bytes_ref_input in(value);
        const auto actual_value = irs::read_zvint(in);

        auto it = expected_values.find(actual_value);
        if (it == expected_values.end()) {
          // can't find value
          return false;
        }

        if (it->second != doc) {
          // wrong document
          return false;
        }

        return true;
      };

      // read values for 'doc_int'
      auto* meta = segment.column("doc_int");
      ASSERT_NE(nullptr, meta);
      auto* column = segment.column_reader(meta->id);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->name));
      ASSERT_TRUE(column->visit(reader));
      ASSERT_EQ(expected_values.size(), calls_count);
    }

    // check 'doc_string' column
    {
      std::unordered_map <irs::string_ref, irs::doc_id_t > expected_values{
        { "string2_data", 1 },
        { "string4_data", 2 }
      };

      size_t calls_count = 0;
      auto reader = [&calls_count, &expected_values] (irs::doc_id_t doc, const irs::bytes_ref& value) {
        ++calls_count;
        irs::bytes_ref_input in(value);
        const auto actual_value = irs::read_string<std::string>(in);

        auto it = expected_values.find(actual_value);
        if (it == expected_values.end()) {
          // can't find value
          return false;
        }

        if (it->second != doc) {
          // wrong document
          return false;
        }

        return true;
      };

      // read values for 'doc_string'
      auto* meta = segment.column("doc_string");
      ASSERT_NE(nullptr, meta);
      auto* column = segment.column_reader(meta->id);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->name));
      ASSERT_TRUE(column->visit(reader));
      ASSERT_EQ(expected_values.size(), calls_count);
    }

    // check wrong column
    {
      ASSERT_EQ(nullptr, segment.column("invalid_column"));
      ASSERT_EQ(nullptr, segment.column_reader("invalid_column"));
    }
  }

  writer.add(reader[0]);
  writer.add(reader[1]);

  irs::index_meta::index_segment_t index_segment;

  index_segment.meta.codec = codec_ptr;
  writer.flush(index_segment);

  {
    auto segment = irs::segment_reader::open(dir, index_segment.meta);
    ASSERT_EQ(4, segment.docs_count());

    auto columns = segment.columns();
    ASSERT_TRUE(columns->next());
    ASSERT_EQ("doc_int", columns->value().name);
    ASSERT_EQ(0, columns->value().id); // 0 since 'doc_int' < 'doc_string'
    ASSERT_TRUE(columns->next());
    ASSERT_EQ("doc_string", columns->value().name);
    ASSERT_EQ(1, columns->value().id);
    ASSERT_FALSE(columns->next());

    // check 'doc_int' column
    {
      std::unordered_map<int, irs::doc_id_t> expected_values{
        // segment 0
        { 1 * 42, 1 },
        { 3 * 42, 2 },
        // segment 1
        { 2 * 42, 3 }
      };

      size_t calls_count = 0;
      auto reader = [&calls_count, &expected_values] (irs::doc_id_t doc, const irs::bytes_ref& value) {
        ++calls_count;
        irs::bytes_ref_input in(value);
        const auto actual_value = irs::read_zvint(in);

        auto it = expected_values.find(actual_value);
        if (it == expected_values.end()) {
          // can't find value
          return false;
        }

        if (it->second != doc) {
          // wrong document
          return false;
        }

        return true;
      };

      // read values for 'doc_int'
      auto* meta = segment.column("doc_int");
      ASSERT_NE(nullptr, meta);
      auto* column = segment.column_reader(meta->id);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->name));
      ASSERT_TRUE(column->visit(reader));
      ASSERT_EQ(expected_values.size(), calls_count);
    }

    // check 'doc_string' column
    {
      std::unordered_map <irs::string_ref, irs::doc_id_t > expected_values{
        // segment 0
        { "string1_data", 1 },
        { "string3_data", 2 },
        // segment 1
        { "string2_data", 3 },
        { "string4_data", 4 }
      };

      size_t calls_count = 0;
      auto reader = [&calls_count, &expected_values] (irs::doc_id_t doc, const irs::bytes_ref& value) {
        ++calls_count;
        irs::bytes_ref_input in(value);
        const auto actual_value = irs::read_string<std::string>(in);

        auto it = expected_values.find(actual_value);
        if (it == expected_values.end()) {
          // can't find value
          return false;
        }

        if (it->second != doc) {
          // wrong document
          return false;
        }

        return true;
      };

      // read values for 'doc_string'
      auto* meta = segment.column("doc_string");
      ASSERT_NE(nullptr, meta);
      auto* column = segment.column_reader(meta->id);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->name));
      ASSERT_TRUE(column->visit(reader));
      ASSERT_EQ(expected_values.size(), calls_count);
    }
  }
}

TEST_F(merge_writer_tests, test_merge_writer) {
  auto codec_ptr = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec_ptr);
  irs::memory_directory dir;

  irs::bstring bytes1;
  irs::bstring bytes2;
  irs::bstring bytes3;

  bytes1.append(irs::ref_cast<irs::byte_type>(irs::string_ref("bytes1_data")));
  bytes2.append(irs::ref_cast<irs::byte_type>(irs::string_ref("bytes2_data")));
  bytes3.append(irs::ref_cast<irs::byte_type>(irs::string_ref("bytes3_data")));

  irs::flags STRING_FIELD_FEATURES{ irs::type<irs::frequency>::get(), irs::type<irs::position>::get() };
  irs::flags TEXT_FIELD_FEATURES{ irs::type<irs::frequency>::get(), irs::type<irs::position>::get(), irs::type<irs::offset>::get(), irs::type<irs::payload>::get() };

  std::string string1;
  std::string string2;
  std::string string3;
  std::string string4;

  string1.append("string1_data");
  string2.append("string2_data");
  string3.append("string3_data");
  string4.append("string4_data");

  std::string text1;
  std::string text2;
  std::string text3;

  text1.append("text1_data");
  text2.append("text2_data");
  text3.append("text3_data");

  tests::document doc1;
  tests::document doc2;
  tests::document doc3;
  tests::document doc4;

  // norm for 'doc_bytes' in 'doc1' : 1/sqrt(4)
  doc1.insert(std::make_shared<tests::binary_field>()); {
    auto& field = doc1.indexed.back<tests::binary_field>();
    field.name(irs::string_ref("doc_bytes"));
    field.value(bytes1);
    field.features().add<irs::norm>();
  }
  doc1.insert(std::make_shared<tests::binary_field>()); {
    auto& field = doc1.indexed.back<tests::binary_field>();
    field.name(irs::string_ref("doc_bytes"));
    field.value(bytes1);
    field.features().add<irs::norm>();
  }
  doc1.insert(std::make_shared<tests::binary_field>()); {
    auto& field = doc1.indexed.back<tests::binary_field>();
    field.name(irs::string_ref("doc_bytes"));
    field.value(bytes1);
    field.features().add<irs::norm>();
  }
  doc1.insert(std::make_shared<tests::binary_field>()); {
    auto& field = doc1.indexed.back<tests::binary_field>();
    field.name(irs::string_ref("doc_bytes"));
    field.value(bytes1);
    field.features().add<irs::norm>();
  }

  // do not track norms for 'doc_bytes' in 'doc2'
  doc2.insert(std::make_shared<tests::binary_field>()); {
    auto& field = doc2.indexed.back<tests::binary_field>();
    field.name(irs::string_ref("doc_bytes"));
    field.value(bytes2);
  }
  doc2.insert(std::make_shared<tests::binary_field>()); {
    auto& field = doc2.indexed.back<tests::binary_field>();
    field.name(irs::string_ref("doc_bytes"));
    field.value(bytes2);
  }

  // norm for 'doc_bytes' in 'doc3' : 1/sqrt(2)
  doc3.insert(std::make_shared<tests::binary_field>()); {
    auto& field = doc3.indexed.back<tests::binary_field>();
    field.name(irs::string_ref("doc_bytes"));
    field.value(bytes3);
    field.features().add<irs::norm>();
  }
  doc3.insert(std::make_shared<tests::binary_field>()); {
    auto& field = doc3.indexed.back<tests::binary_field>();
    field.name(irs::string_ref("doc_bytes"));
    field.value(bytes3);
    field.features().add<irs::norm>();
  }

  doc1.insert(std::make_shared<tests::double_field>()); {
    auto& field = doc1.indexed.back<tests::double_field>();
    field.name(irs::string_ref("doc_double"));
    field.value(2.718281828 * 1);
  }
  doc2.insert(std::make_shared<tests::double_field>()); {
    auto& field = doc2.indexed.back<tests::double_field>();
    field.name(irs::string_ref("doc_double"));
    field.value(2.718281828 * 2);
  }
  doc3.insert(std::make_shared<tests::double_field>()); {
    auto& field = doc3.indexed.back<tests::double_field>();
    field.name(irs::string_ref("doc_double"));
    field.value(2.718281828 * 3);
  }
  doc1.insert(std::make_shared<tests::float_field>()); {
    auto& field = doc1.indexed.back<tests::float_field>();
    field.name(irs::string_ref("doc_float"));
    field.value(3.1415926535f * 1);
  }
  doc2.insert(std::make_shared<tests::float_field>()); {
    auto& field = doc2.indexed.back<tests::float_field>();
    field.name(irs::string_ref("doc_float"));
    field.value(3.1415926535f * 2);
  }
  doc3.insert(std::make_shared<tests::float_field>()); {
    auto& field = doc3.indexed.back<tests::float_field>();
    field.name(irs::string_ref("doc_float"));
    field.value(3.1415926535f * 3);
  }
  doc1.insert(std::make_shared<tests::int_field>()); {
    auto& field = doc1.indexed.back<tests::int_field>();
    field.name(irs::string_ref("doc_int"));
    field.value(42 * 1);
  }
  doc2.insert(std::make_shared<tests::int_field>()); {
    auto& field = doc2.indexed.back<tests::int_field>();
    field.name(irs::string_ref("doc_int"));
    field.value(42 * 2);
  }
  doc3.insert(std::make_shared<tests::int_field>()); {
    auto& field = doc3.indexed.back<tests::int_field>();
    field.name(irs::string_ref("doc_int"));
    field.value(42 * 3);
  }
  doc1.insert(std::make_shared<tests::long_field>()); {
    auto& field = doc1.indexed.back<tests::long_field>();
    field.name(irs::string_ref("doc_long"));
    field.value(12345 * 1);
  }
  doc2.insert(std::make_shared<tests::long_field>()); {
    auto& field = doc2.indexed.back<tests::long_field>();
    field.name(irs::string_ref("doc_long"));
    field.value(12345 * 2);
  }
  doc3.insert(std::make_shared<tests::long_field>()); {
    auto& field = doc3.indexed.back<tests::long_field>();
    field.name(irs::string_ref("doc_long"));
    field.value(12345 * 3);
  }
  doc1.insert(std::make_shared<tests::templates::string_field>("doc_string", string1));
  doc2.insert(std::make_shared<tests::templates::string_field>("doc_string", string2));
  doc3.insert(std::make_shared<tests::templates::string_field>("doc_string", string3));
  doc4.insert(std::make_shared<tests::templates::string_field>("doc_string", string4));
  doc1.indexed.push_back(std::make_shared<tests::templates::text_field<irs::string_ref>>("doc_text", text1));
  doc2.indexed.push_back(std::make_shared<tests::templates::text_field<irs::string_ref>>("doc_text", text2));
  doc3.indexed.push_back(std::make_shared<tests::templates::text_field<irs::string_ref>>("doc_text", text3));

  // populate directory
  {
    auto query_doc4 = irs::iql::query_builder().build("doc_string==string4_data", std::locale::classic());
    auto writer = irs::index_writer::make(dir, codec_ptr, irs::OM_CREATE);

    ASSERT_TRUE(insert(*writer,
      doc1.indexed.begin(), doc1.indexed.end(),
      doc1.stored.begin(), doc1.stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc2.indexed.begin(), doc2.indexed.end(),
      doc2.stored.begin(), doc2.stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(insert(*writer,
      doc3.indexed.begin(), doc3.indexed.end(),
      doc3.stored.begin(), doc3.stored.end()
    ));
    ASSERT_TRUE(insert(*writer,
      doc4.indexed.begin(), doc4.indexed.end(),
      doc4.stored.begin(), doc4.stored.end()
    ));
    writer->commit();
    writer->documents().remove(std::move(query_doc4.filter));
    writer->commit();
  }

  auto docs_count = [](const irs::sub_reader& segment, const irs::string_ref& field) {
    auto* reader = segment.field(field);
    return reader ? reader->docs_count() : 0;
  };

  irs::column_info_provider_t column_info = [](const irs::string_ref&) {
    return irs::column_info(irs::type<irs::compression::lz4>::get(), irs::compression::options{}, true );
  };

  auto reader = irs::directory_reader::open(dir, codec_ptr);
  irs::merge_writer writer(dir, column_info);

  ASSERT_EQ(2, reader.size());
  ASSERT_EQ(2, reader[0].docs_count());
  ASSERT_EQ(2, reader[1].docs_count());

  // validate initial data (segment 0)
  {
    auto& segment = reader[0];
    ASSERT_EQ(2, segment.docs_count());

    {
      auto fields = segment.fields();
      size_t size = 0;
      while (fields->next()) {
        ++size;
      }
      ASSERT_EQ(7, size);
    }

    // validate bytes field
    {
      auto terms = segment.field("doc_bytes");
      ASSERT_NE(nullptr, terms);
      auto& field = terms->meta();
      auto features = tests::binary_field().features();
      features.add<irs::norm>();
      std::unordered_map<irs::bytes_ref, std::unordered_set<irs::doc_id_t>> expected_terms;

      expected_terms[irs::ref_cast<irs::byte_type>(irs::string_ref("bytes1_data"))].emplace(1);
      expected_terms[irs::ref_cast<irs::byte_type>(irs::string_ref("bytes2_data"))].emplace(2);

      ASSERT_EQ(2, docs_count(segment, "doc_bytes"));
      ASSERT_TRUE(irs::type_limits<irs::type_t::field_id_t>::valid(field.norm)); // 'norm' attribute has been specified
      ASSERT_EQ(features, field.features);
      validate_terms(
        segment,
        *terms,
        2,
        bytes1,
        bytes2,
        2,
        features,
        expected_terms
      );

      std::unordered_map<float_t, irs::doc_id_t> expected_values{
        { 0.5f, 1 },
      };

      auto reader = [&expected_values] (irs::doc_id_t doc, const irs::bytes_ref& value) {
        irs::bytes_ref_input in(value);
        const auto actual_value = irs::read_zvfloat(in); // read norm value

        auto it = expected_values.find(actual_value);
        if (it == expected_values.end()) {
          // can't find value
          return false;
        }

        if (it->second != doc) {
          // wrong document
          return false;
        }

        expected_values.erase(it);
        return true;
      };

      auto* column = segment.column_reader(field.norm);
      ASSERT_NE(nullptr, column);
      ASSERT_TRUE(column->visit(reader));
      ASSERT_TRUE(expected_values.empty());
    }

    // validate double field
    {
      auto terms = segment.field("doc_double");
      ASSERT_NE(nullptr, terms);
      auto& field = terms->meta();
      auto features = tests::double_field().features();
      irs::numeric_token_stream max;
      max.reset((double_t) (2.718281828 * 2));
      irs::numeric_token_stream min;
      min.reset((double_t) (2.718281828 * 1));
      std::unordered_map<irs::bstring, std::unordered_set<irs::doc_id_t>> expected_terms;

      {
        irs::numeric_token_stream itr;
        itr.reset((double_t) (2.718281828 * 1));
        for (; itr.next(); expected_terms[irs::bstring(irs::get<irs::term_attribute>(itr)->value)].emplace(1));
      }

      {
        irs::numeric_token_stream itr;
        itr.reset((double_t) (2.718281828 * 2));
        for (; itr.next(); expected_terms[irs::bstring(irs::get<irs::term_attribute>(itr)->value)].emplace(2));
      }

      ASSERT_EQ(2, docs_count(segment, "doc_double"));
      ASSERT_FALSE(irs::type_limits<irs::type_t::field_id_t>::valid(field.norm)); // norm attribute has not been specified
      ASSERT_EQ(features, field.features);
      ASSERT_NE(nullptr, terms);
      ASSERT_TRUE(max.next() && max.next() && max.next() && max.next()); // skip to last value
      ASSERT_TRUE(min.next()); // skip to first value
      validate_terms(
        segment,
        *terms,
        2,
        irs::get<irs::term_attribute>(min)->value,
        irs::get<irs::term_attribute>(max)->value,
        8,
        features,
        expected_terms
      );
    }

    // validate float field
    {
      auto terms = segment.field("doc_float");
      ASSERT_NE(nullptr, terms);
      auto& field = terms->meta();
      auto features = tests::float_field().features();
      irs::numeric_token_stream max;
      max.reset((float_t) (3.1415926535 * 2));
      irs::numeric_token_stream min;
      min.reset((float_t) (3.1415926535 * 1));
      std::unordered_map<irs::bstring, std::unordered_set<irs::doc_id_t>> expected_terms;

      {
        irs::numeric_token_stream itr;
        itr.reset((float_t) (3.1415926535 * 1));
        for (; itr.next(); expected_terms[irs::bstring(irs::get<irs::term_attribute>(itr)->value)].emplace(1));
      }

      {
        irs::numeric_token_stream itr;
        itr.reset((float_t) (3.1415926535 * 2));
        for (; itr.next(); expected_terms[irs::bstring(irs::get<irs::term_attribute>(itr)->value)].emplace(2));
      }

      ASSERT_EQ(2, docs_count(segment, "doc_float"));
      ASSERT_FALSE(irs::type_limits<irs::type_t::field_id_t>::valid(field.norm)); // norm attribute has not been specified
      ASSERT_EQ(features, field.features);
      ASSERT_NE(nullptr, terms);
      ASSERT_TRUE(max.next() && max.next()); // skip to last value
      ASSERT_TRUE(min.next()); // skip to first value
      validate_terms(
        segment,
        *terms,
        2,
        irs::get<irs::term_attribute>(min)->value,
        irs::get<irs::term_attribute>(max)->value,
        4,
        features,
        expected_terms
      );
    }

    // validate int field
    {
      auto terms = segment.field("doc_int");
      ASSERT_NE(nullptr, terms);
      auto& field = terms->meta();
      auto features = tests::int_field().features();
      irs::numeric_token_stream max;
      max.reset(42 * 2);
      irs::numeric_token_stream min;
      min.reset(42 * 1);
      std::unordered_map<irs::bstring, std::unordered_set<irs::doc_id_t>> expected_terms;

      {
        irs::numeric_token_stream itr;
        itr.reset(42 * 1);
        for (; itr.next(); expected_terms[irs::bstring(irs::get<irs::term_attribute>(itr)->value)].emplace(1));
      }

      {
        irs::numeric_token_stream itr;
        itr.reset(42 * 2);
        for (; itr.next(); expected_terms[irs::bstring(irs::get<irs::term_attribute>(itr)->value)].emplace(2));
      }

      ASSERT_EQ(2, docs_count(segment, "doc_int"));
      ASSERT_FALSE(irs::type_limits<irs::type_t::field_id_t>::valid(field.norm)); // norm attribute has not been specified
      ASSERT_EQ(features, field.features);
      ASSERT_NE(nullptr, terms);
      ASSERT_TRUE(max.next() && max.next()); // skip to last value
      ASSERT_TRUE(min.next()); // skip to first value
      validate_terms(
        segment,
        *terms,
        2,
        irs::get<irs::term_attribute>(min)->value,
        irs::get<irs::term_attribute>(max)->value,
        3,
        features,
        expected_terms
      );
    }

    // validate long field
    {
      auto terms = segment.field("doc_long");
      ASSERT_NE(nullptr, terms);
      auto& field = terms->meta();
      auto features = tests::long_field().features();
      irs::numeric_token_stream max;
      max.reset((int64_t) 12345 * 2);
      irs::numeric_token_stream min;
      min.reset((int64_t) 12345 * 1);
      std::unordered_map<irs::bstring, std::unordered_set<irs::doc_id_t>> expected_terms;

      {
        irs::numeric_token_stream itr;
        itr.reset((int64_t) 12345 * 1);
        for (; itr.next(); expected_terms[irs::bstring(irs::get<irs::term_attribute>(itr)->value)].emplace(1));
      }

      {
        irs::numeric_token_stream itr;
        itr.reset((int64_t) 12345 * 2);
        for (; itr.next(); expected_terms[irs::bstring(irs::get<irs::term_attribute>(itr)->value)].emplace(2));
      }

      ASSERT_EQ(2, docs_count(segment, "doc_long"));
      ASSERT_EQ(features, field.features);
      ASSERT_NE(nullptr, terms);
      ASSERT_TRUE(max.next() && max.next() && max.next() && max.next()); // skip to last value
      ASSERT_TRUE(min.next()); // skip to first value
      validate_terms(
        segment,
        *terms,
        2,
        irs::get<irs::term_attribute>(min)->value,
        irs::get<irs::term_attribute>(max)->value,
        5,
        features,
        expected_terms
      );
    }

    // validate string field
    {
      auto terms = segment.field("doc_string");
      ASSERT_NE(nullptr, terms);
      auto& field = terms->meta();
      auto& features = STRING_FIELD_FEATURES;
      size_t frequency = 1;
      std::vector<uint32_t> position = { irs::pos_limits::min() };
      std::unordered_map<irs::bytes_ref, std::unordered_set<irs::doc_id_t>> expected_terms;

      expected_terms[irs::ref_cast<irs::byte_type>(irs::string_ref("string1_data"))].emplace(1);
      expected_terms[irs::ref_cast<irs::byte_type>(irs::string_ref("string2_data"))].emplace(2);

      ASSERT_EQ(2, docs_count(segment, "doc_string"));
      ASSERT_FALSE(irs::type_limits<irs::type_t::field_id_t>::valid(field.norm)); // norm attribute has not been specified
      ASSERT_EQ(features, field.features);
      ASSERT_NE(nullptr, terms);
      validate_terms(
        segment,
        *terms,
        2,
        irs::ref_cast<irs::byte_type>(irs::string_ref(string1)),
        irs::ref_cast<irs::byte_type>(irs::string_ref(string2)),
        2,
        features,
        expected_terms,
        &frequency,
        &position
      );
    }

    // validate text field
    {
      auto terms = segment.field("doc_text");
      ASSERT_NE(nullptr, terms);
      auto& field = terms->meta();
      auto& features = TEXT_FIELD_FEATURES;
      size_t frequency = 1;
      std::vector<uint32_t> position = { irs::pos_limits::min() };
      std::unordered_map<irs::bytes_ref, std::unordered_set<irs::doc_id_t>> expected_terms;

      expected_terms[irs::ref_cast<irs::byte_type>(irs::string_ref("text1_data"))].emplace(1);
      expected_terms[irs::ref_cast<irs::byte_type>(irs::string_ref("text2_data"))].emplace(2);

      ASSERT_EQ(2, docs_count(segment, "doc_text"));
      ASSERT_FALSE(irs::type_limits<irs::type_t::field_id_t>::valid(field.norm)); // norm attribute has not been specified
      ASSERT_EQ(features, field.features);
      ASSERT_NE(nullptr, terms);
      validate_terms(
        segment,
        *terms,
        2,
        irs::ref_cast<irs::byte_type>(irs::string_ref(text1)),
        irs::ref_cast<irs::byte_type>(irs::string_ref(text2)),
        2,
        features,
        expected_terms,
        &frequency,
        &position
      );
    }

    // ...........................................................................
    // validate documents
    // ...........................................................................
    std::unordered_set<irs::bytes_ref> expected_bytes;
    auto column = segment.column_reader("doc_bytes");
    ASSERT_NE(nullptr, column);
    auto bytes_values = column->values();
    std::unordered_set<double> expected_double;
    column = segment.column_reader("doc_double");
    ASSERT_NE(nullptr, column);
    auto double_values = column->values();
    std::unordered_set<float> expected_float;
    column = segment.column_reader("doc_float");
    ASSERT_NE(nullptr, column);
    auto float_values = column->values();
    std::unordered_set<int> expected_int;
    column = segment.column_reader("doc_int");
    ASSERT_NE(nullptr, column);
    auto int_values = column->values();
    std::unordered_set<int64_t> expected_long;
    column = segment.column_reader("doc_long");
    ASSERT_NE(nullptr, column);
    auto long_values = column->values();
    std::unordered_set<std::string> expected_string;
    column = segment.column_reader("doc_string");
    ASSERT_NE(nullptr, column);
    auto string_values = column->values();

    expected_bytes = { irs::bytes_ref(bytes1), irs::bytes_ref(bytes2) };
    expected_double = { 2.718281828 * 1, 2.718281828 * 2 };
    expected_float = { (float)(3.1415926535 * 1), (float)(3.1415926535 * 2) };
    expected_int = { 42 * 1, 42 * 2 };
    expected_long = { 12345 * 1, 12345 * 2 };
    expected_string = { string1, string2 };

    // can't have more docs then highest doc_id
    irs::bytes_ref value;
    irs::bytes_ref_input in;
    for (size_t i = 0, count = segment.docs_count(); i < count; ++i) {
      const auto doc = irs::doc_id_t((irs::type_limits<irs::type_t::doc_id_t>::min)() + i);
      ASSERT_TRUE(bytes_values(doc, value)); in.reset(value);
      ASSERT_EQ(1, expected_bytes.erase(irs::read_string<irs::bstring>(in)));

      ASSERT_TRUE(double_values(doc, value)); in.reset(value);
      ASSERT_EQ(1, expected_double.erase(irs::read_zvdouble(in)));

      ASSERT_TRUE(float_values(doc, value)); in.reset(value);
      ASSERT_EQ(1, expected_float.erase(irs::read_zvfloat(in)));

      ASSERT_TRUE(int_values(doc, value)); in.reset(value);
      ASSERT_EQ(1, expected_int.erase(irs::read_zvint(in)));

      ASSERT_TRUE(long_values(doc, value)); in.reset(value);
      ASSERT_EQ(1, expected_long.erase(irs::read_zvlong(in)));

      ASSERT_TRUE(string_values(doc, value)); in.reset(value);
      ASSERT_EQ(1, expected_string.erase(irs::read_string<std::string>(in)));
    }

    ASSERT_TRUE(expected_bytes.empty());
    ASSERT_TRUE(expected_double.empty());
    ASSERT_TRUE(expected_float.empty());
    ASSERT_TRUE(expected_int.empty());
    ASSERT_TRUE(expected_long.empty());
    ASSERT_TRUE(expected_string.empty());
  }

  // validate initial data (segment 1)
  {
    auto& segment = reader[1];
    ASSERT_EQ(2, segment.docs_count());

    {
      auto fields = segment.fields();
      size_t size = 0;
      while (fields->next()) {
        ++size;
      }
      ASSERT_EQ(7, size);
    }

    // validate bytes field
    {
      auto terms = segment.field("doc_bytes");
      ASSERT_NE(nullptr, terms);
      auto& field = terms->meta();
      auto features = tests::binary_field().features();
      std::unordered_map<irs::bytes_ref, std::unordered_set<irs::doc_id_t>> expected_terms;
      features.add<irs::norm>();
      expected_terms[irs::ref_cast<irs::byte_type>(irs::string_ref("bytes3_data"))].emplace(1);

      ASSERT_EQ(1, docs_count(segment, "doc_bytes"));
      ASSERT_TRUE(irs::type_limits<irs::type_t::field_id_t>::valid(field.norm)); // norm attribute has been specified
      ASSERT_EQ(features, field.features);
      ASSERT_NE(nullptr, terms);
      validate_terms(
        segment,
        *terms,
        1,
        bytes3,
        bytes3,
        1,
        features,
        expected_terms
      );

      std::unordered_map<float_t, irs::doc_id_t> expected_values{
        { float(1./std::sqrt(2)), 1 },
      };

      auto reader = [&expected_values] (irs::doc_id_t doc, const irs::bytes_ref& value) {
        irs::bytes_ref_input in(value);
        const auto actual_value = irs::read_zvfloat(in); // read norm value

        auto it = expected_values.find(actual_value);
        if (it == expected_values.end()) {
          // can't find value
          return false;
        }

        if (it->second != doc) {
          // wrong document
          return false;
        }

        expected_values.erase(it);
        return true;
      };

      auto* column = segment.column_reader(field.norm);
      ASSERT_NE(nullptr, column);
      ASSERT_TRUE(column->visit(reader));
      ASSERT_TRUE(expected_values.empty());
    }

    // validate double field
    {
      auto terms = segment.field("doc_double");
      ASSERT_NE(nullptr, terms);
      auto& field = terms->meta();
      auto features = tests::double_field().features();
      irs::numeric_token_stream max;
      max.reset((double_t) (2.718281828 * 3));
      irs::numeric_token_stream min;
      min.reset((double_t) (2.718281828 * 3));
      std::unordered_map<irs::bstring, std::unordered_set<irs::doc_id_t>> expected_terms;

      {
        irs::numeric_token_stream itr;
        itr.reset((double_t) (2.718281828 * 3));
        for (; itr.next(); expected_terms[irs::bstring(irs::get<irs::term_attribute>(itr)->value)].emplace(1));
      }

      ASSERT_EQ(1, docs_count(segment, "doc_double"));
      ASSERT_FALSE(irs::type_limits<irs::type_t::field_id_t>::valid(field.norm)); // norm attribute has not been specified
      ASSERT_EQ(features, field.features);
      ASSERT_NE(nullptr, terms);
      ASSERT_TRUE(max.next() && max.next() && max.next() && max.next()); // skip to last value
      ASSERT_TRUE(min.next()); // skip to first value
      validate_terms(
        segment,
        *terms,
        1,
        irs::get<irs::term_attribute>(min)->value,
        irs::get<irs::term_attribute>(max)->value,
        4,
        features,
        expected_terms
      );
    }

    // validate float field
    {
      auto terms = segment.field("doc_float");
      ASSERT_NE(nullptr, terms);
      auto& field = terms->meta();
      auto features = tests::float_field().features();
      irs::numeric_token_stream max;
      max.reset((float_t) (3.1415926535 * 3));
      irs::numeric_token_stream min;
      min.reset((float_t) (3.1415926535 * 3));
      std::unordered_map<irs::bstring, std::unordered_set<irs::doc_id_t>> expected_terms;

      {
        irs::numeric_token_stream itr;
        itr.reset((float_t) (3.1415926535 * 3));
        for (; itr.next(); expected_terms[irs::bstring(irs::get<irs::term_attribute>(itr)->value)].emplace(1));
      }

      ASSERT_EQ(1, docs_count(segment, "doc_float"));
      ASSERT_FALSE(irs::type_limits<irs::type_t::field_id_t>::valid(field.norm)); // norm attribute has not been specified
      ASSERT_EQ(features, field.features);
      ASSERT_NE(nullptr, terms);
      ASSERT_TRUE(max.next() && max.next()); // skip to last value
      ASSERT_TRUE(min.next()); // skip to first value
      validate_terms(
        segment,
        *terms,
        1,
        irs::get<irs::term_attribute>(min)->value,
        irs::get<irs::term_attribute>(max)->value,
        2,
        features,
        expected_terms
      );
    }

    // validate int field
    {
      auto terms = segment.field("doc_int");
      ASSERT_NE(nullptr, terms);
      auto& field = terms->meta();
      auto features = tests::int_field().features();
      irs::numeric_token_stream max;
      max.reset(42 * 3);
      irs::numeric_token_stream min;
      min.reset(42 * 3);
      std::unordered_map<irs::bstring, std::unordered_set<irs::doc_id_t>> expected_terms;

      {
        irs::numeric_token_stream itr;
        itr.reset(42 * 3);
        for (; itr.next(); expected_terms[irs::bstring(irs::get<irs::term_attribute>(itr)->value)].emplace(1));
      }

      ASSERT_EQ(1, docs_count(segment, "doc_int"));
      ASSERT_FALSE(irs::type_limits<irs::type_t::field_id_t>::valid(field.norm)); // norm attribute has not been specified
      ASSERT_EQ(features, field.features);
      ASSERT_NE(nullptr, terms);
      ASSERT_TRUE(max.next() && max.next()); // skip to last value
      ASSERT_TRUE(min.next()); // skip to first value
      validate_terms(
        segment,
        *terms,
        1,
        irs::get<irs::term_attribute>(min)->value,
        irs::get<irs::term_attribute>(max)->value,
        2,
        features,
        expected_terms
      );
    }

    // validate long field
    {
      auto terms = segment.field("doc_long");
      ASSERT_NE(nullptr, terms);
      auto& field = terms->meta();
      auto features = tests::long_field().features();
      irs::numeric_token_stream max;
      max.reset((int64_t) 12345 * 3);
      irs::numeric_token_stream min;
      min.reset((int64_t) 12345 * 3);
      std::unordered_map<irs::bstring, std::unordered_set<irs::doc_id_t>> expected_terms;

      {
        irs::numeric_token_stream itr;
        itr.reset((int64_t) 12345 * 3);
        for (; itr.next(); expected_terms[irs::bstring(irs::get<irs::term_attribute>(itr)->value)].emplace(1));
      }

      ASSERT_EQ(1, docs_count(segment, "doc_long"));
      ASSERT_FALSE(irs::type_limits<irs::type_t::field_id_t>::valid(field.norm)); // norm attribute has not been specified
      ASSERT_EQ(features, field.features);
      ASSERT_NE(nullptr, terms);
      ASSERT_TRUE(max.next() && max.next() && max.next() && max.next()); // skip to last value
      ASSERT_TRUE(min.next()); // skip to first value
      validate_terms(
        segment,
        *terms,
        1,
        irs::get<irs::term_attribute>(min)->value,
        irs::get<irs::term_attribute>(max)->value,
        4,
        features,
        expected_terms
      );
    }

    // validate string field
    {
      auto terms = segment.field("doc_string");
      ASSERT_NE(nullptr, terms);
      auto& field = terms->meta();
      auto& features = STRING_FIELD_FEATURES;
      size_t frequency = 1;
      std::vector<uint32_t> position = { irs::pos_limits::min() };
      std::unordered_map<irs::bytes_ref, std::unordered_set<irs::doc_id_t>> expected_terms;

      expected_terms[irs::ref_cast<irs::byte_type>(irs::string_ref("string3_data"))].emplace(1);
      expected_terms[irs::ref_cast<irs::byte_type>(irs::string_ref("string4_data"))];

      ASSERT_EQ(2, docs_count(segment, "doc_string"));
      ASSERT_FALSE(irs::type_limits<irs::type_t::field_id_t>::valid(field.norm)); // norm attribute has not been specified
      ASSERT_EQ(features, field.features);
      ASSERT_NE(nullptr, terms);
      validate_terms(
        segment,
        *terms,
        2,
        irs::ref_cast<irs::byte_type>(irs::string_ref(string3)),
        irs::ref_cast<irs::byte_type>(irs::string_ref(string4)),
        2,
        features,
        expected_terms,
        &frequency,
        &position
      );
    }

    // validate text field
    {
      auto terms = segment.field("doc_text");
      ASSERT_NE(nullptr, terms);
      auto& field = terms->meta();
      auto& features = TEXT_FIELD_FEATURES;
      size_t frequency = 1;
      std::vector<uint32_t> position = { irs::pos_limits::min() };
      std::unordered_map<irs::bytes_ref, std::unordered_set<irs::doc_id_t>> expected_terms;

      expected_terms[irs::ref_cast<irs::byte_type>(irs::string_ref("text3_data"))].emplace(1);

      ASSERT_EQ(1, docs_count(segment, "doc_text"));
      ASSERT_FALSE(irs::type_limits<irs::type_t::field_id_t>::valid(field.norm)); // norm attribute has not been specified
      ASSERT_EQ(features, field.features);
      ASSERT_NE(nullptr, terms);
      validate_terms(
        segment,
        *terms,
        1,
        irs::ref_cast<irs::byte_type>(irs::string_ref(text3)),
        irs::ref_cast<irs::byte_type>(irs::string_ref(text3)),
        1,
        features,
        expected_terms,
        &frequency,
        &position
      );
    }

    // ...........................................................................
    // validate documents
    // ...........................................................................
    std::unordered_set<irs::bytes_ref> expected_bytes;
    auto column = segment.column_reader("doc_bytes");
    ASSERT_NE(nullptr, column);
    auto bytes_values = column->values();
    std::unordered_set<double> expected_double;
    column = segment.column_reader("doc_double");
    ASSERT_NE(nullptr, column);
    auto double_values = column->values();
    std::unordered_set<float> expected_float;
    column = segment.column_reader("doc_float");
    ASSERT_NE(nullptr, column);
    auto float_values = column->values();
    std::unordered_set<int> expected_int;
    column = segment.column_reader("doc_int");
    ASSERT_NE(nullptr, column);
    auto int_values = column->values();
    std::unordered_set<int64_t> expected_long;
    column = segment.column_reader("doc_long");
    ASSERT_NE(nullptr, column);
    auto long_values = column->values();
    std::unordered_set<std::string> expected_string;
    column = segment.column_reader("doc_string");
    ASSERT_NE(nullptr, column);
    auto string_values = column->values();

    expected_bytes = { irs::bytes_ref(bytes3) };
    expected_double = { 2.718281828 * 3 };
    expected_float = { (float)(3.1415926535 * 3) };
    expected_int = { 42 * 3 };
    expected_long = { 12345 * 3 };
    expected_string = { string3, string4 };

    // can't have more docs then highest doc_id
    irs::bytes_ref value;
    irs::bytes_ref_input in;
    for (size_t i = 0, count = segment.docs_count(); i < count; ++i) {
      const auto doc = irs::doc_id_t((irs::type_limits<irs::type_t::doc_id_t>::min)() + i);
      ASSERT_EQ(!expected_bytes.empty(), bytes_values(doc, value)); in.reset(value);
      expected_bytes.erase(irs::read_string<irs::bstring>(in));

      ASSERT_EQ(!expected_double.empty(), double_values(doc, value)); in.reset(value);
      expected_double.erase(irs::read_zvdouble(in));

      ASSERT_EQ(!expected_float.empty(), float_values(doc, value)); in.reset(value);
      expected_float.erase(irs::read_zvfloat(in));

      ASSERT_EQ(!expected_int.empty(), int_values(doc, value)); in.reset(value);
      expected_int.erase(irs::read_zvint(in));

      ASSERT_EQ(!expected_long.empty(), long_values(doc, value)); in.reset(value);
      expected_long.erase(irs::read_zvlong(in));

      ASSERT_TRUE(string_values(doc, value)); in.reset(value);
      ASSERT_EQ(1, expected_string.erase(irs::read_string<std::string>(in)));
    }

    ASSERT_TRUE(expected_bytes.empty());
    ASSERT_TRUE(expected_double.empty());
    ASSERT_TRUE(expected_float.empty());
    ASSERT_TRUE(expected_int.empty());
    ASSERT_TRUE(expected_long.empty());
    ASSERT_TRUE(expected_string.empty());
  }

  writer.add(reader[0]);
  writer.add(reader[1]);

  irs::index_meta::index_segment_t index_segment;

  index_segment.meta.codec = codec_ptr;
  writer.flush(index_segment);

  auto segment = irs::segment_reader::open(dir, index_segment.meta);

  ASSERT_EQ(3, segment.docs_count()); //doc4 removed during merge

  {
    auto fields = segment.fields();
    size_t size = 0;
    while (fields->next()) {
      ++size;
    }
    ASSERT_EQ(7, size);
  }

  // validate bytes field
  {
    auto terms = segment.field("doc_bytes");
    ASSERT_NE(nullptr, terms);
    auto& field = terms->meta();
    auto features = tests::binary_field().features();
    features.add<irs::norm>();
    std::unordered_map<irs::bytes_ref, std::unordered_set<irs::doc_id_t>> expected_terms;

    expected_terms[irs::ref_cast<irs::byte_type>(irs::string_ref("bytes1_data"))].emplace(1);
    expected_terms[irs::ref_cast<irs::byte_type>(irs::string_ref("bytes2_data"))].emplace(2);
    expected_terms[irs::ref_cast<irs::byte_type>(irs::string_ref("bytes3_data"))].emplace(3);

    ASSERT_EQ(3, docs_count(segment, "doc_bytes"));
    ASSERT_TRUE(irs::type_limits<irs::type_t::field_id_t>::valid(field.norm)); // norm attribute has been specified
    ASSERT_EQ(features, field.features);
    ASSERT_NE(nullptr, terms);
    validate_terms(
      segment,
      *terms,
      3,
      bytes1,
      bytes3,
      3,
      features,
      expected_terms
    );

    std::unordered_map<float_t, irs::doc_id_t> expected_values{
      { 0.5f, 1 },                    // norm value for 'doc_bytes' in 'doc1'
      { float_t(1/std::sqrt(2)), 3 }, // norm value for 'doc_bytes' in 'doc3'
    };

    auto reader = [&expected_values] (irs::doc_id_t doc, const irs::bytes_ref& value) {
      irs::bytes_ref_input in(value);
      const auto actual_value = irs::read_zvfloat(in); // read norm value

      auto it = expected_values.find(actual_value);
      if (it == expected_values.end()) {
        // can't find value
        return false;
      }

      if (it->second != doc) {
        // wrong document
        return false;
      }

      expected_values.erase(it);
      return true;
    };

    auto* column = segment.column_reader(field.norm);
    ASSERT_NE(nullptr, column);
    ASSERT_TRUE(column->visit(reader));
    ASSERT_TRUE(expected_values.empty());
  }

  // validate double field
  {
    auto terms = segment.field("doc_double");
    ASSERT_NE(nullptr, terms);
    auto& field = terms->meta();
    auto features = tests::double_field().features();
    irs::numeric_token_stream max;
    max.reset((double_t) (2.718281828 * 3));
    irs::numeric_token_stream min;
    min.reset((double_t) (2.718281828 * 1));
    std::unordered_map<irs::bstring, std::unordered_set<irs::doc_id_t>> expected_terms;

    {
      irs::numeric_token_stream itr;
      itr.reset((double_t) (2.718281828 * 1));
      for (; itr.next(); expected_terms[irs::bstring(irs::get<irs::term_attribute>(itr)->value)].emplace(1));
    }

    {
      irs::numeric_token_stream itr;
      itr.reset((double_t) (2.718281828 * 2));
      for (; itr.next(); expected_terms[irs::bstring(irs::get<irs::term_attribute>(itr)->value)].emplace(2));
    }

    {
      irs::numeric_token_stream itr;
      itr.reset((double_t) (2.718281828 * 3));
      for (; itr.next(); expected_terms[irs::bstring(irs::get<irs::term_attribute>(itr)->value)].emplace(3));
    }

    ASSERT_EQ(3, docs_count(segment, "doc_double"));
    ASSERT_FALSE(irs::type_limits<irs::type_t::field_id_t>::valid(field.norm)); // norm attribute has not been specified
    ASSERT_EQ(features, field.features);
    ASSERT_NE(nullptr, terms);
    ASSERT_TRUE(max.next() && max.next() && max.next() && max.next()); // skip to last value
    ASSERT_TRUE(min.next()); // skip to first value
    validate_terms(
      segment,
      *terms,
      3,
      irs::get<irs::term_attribute>(min)->value,
      irs::get<irs::term_attribute>(max)->value,
      12,
      features,
      expected_terms
    );
  }

  // validate float field
  {
    auto terms = segment.field("doc_float");
    ASSERT_NE(nullptr, terms);
    auto& field = terms->meta();
    auto features = tests::float_field().features();
    irs::numeric_token_stream max;
    max.reset((float_t) (3.1415926535 * 3));
    irs::numeric_token_stream min;
    min.reset((float_t) (3.1415926535 * 1));
    std::unordered_map<irs::bstring, std::unordered_set<irs::doc_id_t>> expected_terms;

    {
      irs::numeric_token_stream itr;
      itr.reset((float_t) (3.1415926535 * 1));
      for (; itr.next(); expected_terms[irs::bstring(irs::get<irs::term_attribute>(itr)->value)].emplace(1));
    }

    {
      irs::numeric_token_stream itr;
      itr.reset((float_t) (3.1415926535 * 2));
      for (; itr.next(); expected_terms[irs::bstring(irs::get<irs::term_attribute>(itr)->value)].emplace(2));
    }

    {
      irs::numeric_token_stream itr;
      itr.reset((float_t) (3.1415926535 * 3));
      for (; itr.next(); expected_terms[irs::bstring(irs::get<irs::term_attribute>(itr)->value)].emplace(3));
    }

    ASSERT_EQ(3, docs_count(segment, "doc_float"));
    ASSERT_FALSE(irs::type_limits<irs::type_t::field_id_t>::valid(field.norm)); // norm attribute has not been specified
    ASSERT_EQ(features, field.features);
    ASSERT_NE(nullptr, terms);
    ASSERT_TRUE(max.next() && max.next()); // skip to last value
    ASSERT_TRUE(min.next()); // skip to first value
    validate_terms(
      segment,
      *terms,
      3,
      irs::get<irs::term_attribute>(min)->value,
      irs::get<irs::term_attribute>(max)->value,
      6,
      features,
      expected_terms
    );
  }

  // validate int field
  {
    auto terms = segment.field("doc_int");
    ASSERT_NE(nullptr, terms);
    auto& field = terms->meta();
    auto features = tests::int_field().features();
    irs::numeric_token_stream max;
    max.reset(42 * 3);
    irs::numeric_token_stream min;
    min.reset(42 * 1);
    std::unordered_map<irs::bstring, std::unordered_set<irs::doc_id_t>> expected_terms;

    {
      irs::numeric_token_stream itr;
      itr.reset(42 * 1);
      for (; itr.next(); expected_terms[irs::bstring(irs::get<irs::term_attribute>(itr)->value)].emplace(1));
    }

    {
      irs::numeric_token_stream itr;
      itr.reset(42 * 2);
      for (; itr.next(); expected_terms[irs::bstring(irs::get<irs::term_attribute>(itr)->value)].emplace(2));
    }

    {
      irs::numeric_token_stream itr;
      itr.reset(42 * 3);
      for (; itr.next(); expected_terms[irs::bstring(irs::get<irs::term_attribute>(itr)->value)].emplace(3));
    }

    ASSERT_EQ(3, docs_count(segment, "doc_int"));
    ASSERT_FALSE(irs::type_limits<irs::type_t::field_id_t>::valid(field.norm)); // norm attribute has not been specified
    ASSERT_EQ(features, field.features);
    ASSERT_NE(nullptr, terms);
    ASSERT_TRUE(max.next() && max.next()); // skip to last value
    ASSERT_TRUE(min.next()); // skip to first value
    validate_terms(
      segment,
      *terms,
      3,
      irs::get<irs::term_attribute>(min)->value,
      irs::get<irs::term_attribute>(max)->value,
      4,
      features,
      expected_terms
    );
  }

  // validate long field
  {
    auto terms = segment.field("doc_long");
    ASSERT_NE(nullptr, terms);
    auto& field = terms->meta();
    auto features = tests::long_field().features();
    irs::numeric_token_stream max;
    max.reset((int64_t) 12345 * 3);
    irs::numeric_token_stream min;
    min.reset((int64_t) 12345 * 1);
    std::unordered_map<irs::bstring, std::unordered_set<irs::doc_id_t>> expected_terms;

    {
      irs::numeric_token_stream itr;
      itr.reset((int64_t) 12345 * 1);
      for (; itr.next(); expected_terms[irs::bstring(irs::get<irs::term_attribute>(itr)->value)].emplace(1));
    }

    {
      irs::numeric_token_stream itr;
      itr.reset((int64_t) 12345 * 2);
      for (; itr.next(); expected_terms[irs::bstring(irs::get<irs::term_attribute>(itr)->value)].emplace(2));
    }

    {
      irs::numeric_token_stream itr;
      itr.reset((int64_t) 12345 * 3);
      for (; itr.next(); expected_terms[irs::bstring(irs::get<irs::term_attribute>(itr)->value)].emplace(3));
    }

    ASSERT_EQ(3, docs_count(segment, "doc_long"));
    ASSERT_FALSE(irs::type_limits<irs::type_t::field_id_t>::valid(field.norm)); // norm attribute has not been specified
    ASSERT_EQ(features, field.features);
    ASSERT_NE(nullptr, terms);
    ASSERT_TRUE(max.next() && max.next() && max.next() && max.next()); // skip to last value
    ASSERT_TRUE(min.next()); // skip to first value
    validate_terms(
      segment,
      *terms,
      3,
      irs::get<irs::term_attribute>(min)->value,
      irs::get<irs::term_attribute>(max)->value,
      6,
      features,
      expected_terms
    );
  }

  // validate string field
  {
    auto terms = segment.field("doc_string");
    ASSERT_NE(nullptr, terms);
    auto& field = terms->meta();
    auto& features = STRING_FIELD_FEATURES;
    size_t frequency = 1;
    std::vector<uint32_t> position = { irs::pos_limits::min() };
    std::unordered_map<irs::bytes_ref, std::unordered_set<irs::doc_id_t>> expected_terms;

    expected_terms[irs::ref_cast<irs::byte_type>(irs::string_ref("string1_data"))].emplace(1);
    expected_terms[irs::ref_cast<irs::byte_type>(irs::string_ref("string2_data"))].emplace(2);
    expected_terms[irs::ref_cast<irs::byte_type>(irs::string_ref("string3_data"))].emplace(3);

    ASSERT_EQ(3, docs_count(segment, "doc_string"));
    ASSERT_FALSE(irs::type_limits<irs::type_t::field_id_t>::valid(field.norm)); // norm attribute has not been specified
    ASSERT_EQ(features, field.features);
    ASSERT_NE(nullptr, terms);
    validate_terms(
      segment,
      *terms,
      3,
      irs::ref_cast<irs::byte_type>(irs::string_ref(string1)),
      irs::ref_cast<irs::byte_type>(irs::string_ref(string3)),
      3,
      features,
      expected_terms,
      &frequency,
      &position
    );
  }

  // validate text field
  {
    auto terms = segment.field("doc_text");
    ASSERT_NE(nullptr, terms);
    auto& field = terms->meta();
    auto& features = TEXT_FIELD_FEATURES;
    size_t frequency = 1;
    std::vector<uint32_t> position = { irs::pos_limits::min() };
    std::unordered_map<irs::bytes_ref, std::unordered_set<irs::doc_id_t>> expected_terms;

    expected_terms[irs::ref_cast<irs::byte_type>(irs::string_ref("text1_data"))].emplace(1);
    expected_terms[irs::ref_cast<irs::byte_type>(irs::string_ref("text2_data"))].emplace(2);
    expected_terms[irs::ref_cast<irs::byte_type>(irs::string_ref("text3_data"))].emplace(3);

    ASSERT_EQ(3, docs_count(segment, "doc_text"));
    ASSERT_EQ(features, field.features);
    ASSERT_NE(nullptr, terms);
    validate_terms(
      segment,
      *terms,
      3,
      irs::ref_cast<irs::byte_type>(irs::string_ref(text1)),
      irs::ref_cast<irs::byte_type>(irs::string_ref(text3)),
      3,
      features,
      expected_terms,
      &frequency,
      &position
    );
  }

  // ...........................................................................
  // validate documents
  // ...........................................................................
  std::unordered_set<irs::bytes_ref> expected_bytes;
  auto column = segment.column_reader("doc_bytes");
  ASSERT_NE(nullptr, column);
  auto bytes_values = column->values();
  std::unordered_set<double> expected_double;
  column = segment.column_reader("doc_double");
  ASSERT_NE(nullptr, column);
  auto double_values = column->values();
  std::unordered_set<float> expected_float;
  column = segment.column_reader("doc_float");
  ASSERT_NE(nullptr, column);
  auto float_values = column->values();
  std::unordered_set<int> expected_int;
  column = segment.column_reader("doc_int");
  ASSERT_NE(nullptr, column);
  auto int_values = column->values();
  std::unordered_set<int64_t> expected_long;
  column = segment.column_reader("doc_long");
  ASSERT_NE(nullptr, column);
  auto long_values = column->values();
  std::unordered_set<std::string> expected_string;
  column = segment.column_reader("doc_string");
  ASSERT_NE(nullptr, column);
  auto string_values = column->values();

  expected_bytes = { irs::bytes_ref(bytes1), irs::bytes_ref(bytes2), irs::bytes_ref(bytes3) };
  expected_double = { 2.718281828 * 1, 2.718281828 * 2, 2.718281828 * 3 };
  expected_float = { (float)(3.1415926535 * 1), (float)(3.1415926535 * 2), (float)(3.1415926535 * 3) };
  expected_int = { 42 * 1, 42 * 2, 42 * 3 };
  expected_long = { 12345 * 1, 12345 * 2, 12345 * 3 };
  expected_string = { string1, string2, string3 };

  // can't have more docs then highest doc_id
  irs::bytes_ref value;
  irs::bytes_ref_input in;
  for (size_t i = 0, count = segment.docs_count(); i < count; ++i) {
    const auto doc = irs::doc_id_t((irs::type_limits<irs::type_t::doc_id_t>::min)() + i);

    ASSERT_TRUE(bytes_values(doc, value)); in.reset(value);
    ASSERT_EQ(1, expected_bytes.erase(irs::read_string<irs::bstring>(in)));

    ASSERT_TRUE(double_values(doc, value)); in.reset(value);
    ASSERT_EQ(1, expected_double.erase(irs::read_zvdouble(in)));

    ASSERT_TRUE(float_values(doc, value)); in.reset(value);
    ASSERT_EQ(1, expected_float.erase(irs::read_zvfloat(in)));

    ASSERT_TRUE(int_values(doc, value)); in.reset(value);
    ASSERT_EQ(1, expected_int.erase(irs::read_zvint(in)));

    ASSERT_TRUE(long_values(doc, value)); in.reset(value);
    ASSERT_EQ(1, expected_long.erase(irs::read_zvlong(in)));

    ASSERT_TRUE(string_values(doc, value)); in.reset(value);
    ASSERT_EQ(1, expected_string.erase(irs::read_string<std::string>(in)));
  }

  ASSERT_TRUE(expected_bytes.empty());
  ASSERT_TRUE(expected_double.empty());
  ASSERT_TRUE(expected_float.empty());
  ASSERT_TRUE(expected_int.empty());
  ASSERT_TRUE(expected_long.empty());
  ASSERT_TRUE(expected_string.empty());
}

TEST_F(merge_writer_tests, test_merge_writer_add_segments) {
  auto codec_ptr = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec_ptr);
  irs::memory_directory data_dir;

  // populate directory
  {
    tests::json_doc_generator gen(
      test_base::resource("simple_sequential_33.json"),
      &tests::generic_json_field_factory
    );
    std::vector<const tests::document*> docs;
    docs.reserve(33);

    for (size_t i = 0; i < 33; ++i) {
      docs.emplace_back(gen.next());
    }

    auto writer = irs::index_writer::make(data_dir, codec_ptr, irs::OM_CREATE);

    for (auto* doc: docs) {
      ASSERT_NE(nullptr, doc);
      ASSERT_TRUE(insert(
        *writer,
        doc->indexed.begin(), doc->indexed.end(),
        doc->stored.begin(), doc->stored.end()
      ));
      writer->commit(); // create segmentN
    }
  }

  auto reader = irs::directory_reader::open(data_dir, codec_ptr);

  ASSERT_EQ(33, reader.size());

  // merge 33 segments to writer (segments > 32 to trigger GCC 8.2.0 optimizer bug)
  {
    irs::column_info_provider_t column_info = [](const irs::string_ref&) {
      return irs::column_info(irs::type<irs::compression::lz4>::get(), irs::compression::options{}, true );
    };

    irs::memory_directory dir;
    irs::index_meta::index_segment_t index_segment;
    irs::merge_writer writer(dir, column_info);

    for (auto& sub_reader: reader) {
      writer.add(sub_reader);
    }

    index_segment.meta.codec = codec_ptr;
    ASSERT_TRUE(writer.flush(index_segment));

    auto segment = irs::segment_reader::open(dir, index_segment.meta);
    ASSERT_EQ(33, segment.docs_count());
    ASSERT_EQ(33, segment.field("name")->docs_count());
    ASSERT_EQ(33, segment.field("seq")->docs_count());
    ASSERT_EQ(33, segment.field("same")->docs_count());
    ASSERT_EQ(13, segment.field("duplicated")->docs_count());
  }
}

TEST_F(merge_writer_tests, test_merge_writer_flush_progress) {
  auto codec_ptr = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec_ptr);
  irs::memory_directory data_dir;

  // populate directory
  {
    tests::json_doc_generator gen(
      test_base::resource("simple_sequential.json"),
      &tests::generic_json_field_factory
    );
    auto* doc1 = gen.next();
    auto* doc2 = gen.next();
    auto writer = irs::index_writer::make(data_dir, codec_ptr, irs::OM_CREATE);
    ASSERT_TRUE(insert(
      *writer,
      doc1->indexed.begin(), doc1->indexed.end(),
      doc1->stored.begin(), doc1->stored.end()
    ));
    writer->commit(); // create segment0
    ASSERT_TRUE(insert(
      *writer,
      doc2->indexed.begin(), doc2->indexed.end(),
      doc2->stored.begin(), doc2->stored.end()
    ));
    writer->commit(); // create segment1
  }

  auto reader = irs::directory_reader::open(data_dir, codec_ptr);

  ASSERT_EQ(2, reader.size());
  ASSERT_EQ(1, reader[0].docs_count());
  ASSERT_EQ(1, reader[1].docs_count());

  // test default progress (false)
  {
    irs::column_info_provider_t column_info = [](const irs::string_ref&) {
      return irs::column_info(irs::type<irs::compression::lz4>::get(), irs::compression::options{}, true );
    };

    irs::memory_directory dir;
    irs::index_meta::index_segment_t index_segment;
    irs::merge_writer::flush_progress_t progress;
    irs::merge_writer writer(dir, column_info);

    index_segment.meta.codec = codec_ptr;
    writer.add(reader[0]);
    writer.add(reader[1]);
    ASSERT_TRUE(writer.flush(index_segment, progress));

    ASSERT_FALSE(index_segment.meta.files.empty());
    ASSERT_EQ(2, index_segment.meta.docs_count);
    ASSERT_EQ(2, index_segment.meta.live_docs_count);
    ASSERT_EQ(0, index_segment.meta.version);
    ASSERT_EQ(true, index_segment.meta.column_store);

    auto segment = irs::segment_reader::open(dir, index_segment.meta);
    ASSERT_EQ(2, segment.docs_count());
  }

  // test always-false progress
  {
    irs::column_info_provider_t column_info = [](const irs::string_ref&) {
      return irs::column_info(irs::type<irs::compression::lz4>::get(), irs::compression::options{}, true );
    };

    irs::memory_directory dir;
    irs::index_meta::index_segment_t index_segment;
    irs::merge_writer::flush_progress_t progress = []()->bool { return false; };
    irs::merge_writer writer(dir, column_info);

    index_segment.meta.codec = codec_ptr;
    writer.add(reader[0]);
    writer.add(reader[1]);
    ASSERT_FALSE(writer.flush(index_segment, progress));

    ASSERT_TRUE(index_segment.filename.empty());
    ASSERT_TRUE(index_segment.meta.name.empty());
    ASSERT_TRUE(index_segment.meta.files.empty());
    ASSERT_FALSE(index_segment.meta.column_store);
    ASSERT_EQ(0, index_segment.meta.version);
    ASSERT_EQ(0, index_segment.meta.docs_count);
    ASSERT_EQ(0, index_segment.meta.live_docs_count);
    ASSERT_EQ(0, index_segment.meta.size);

    ASSERT_ANY_THROW(irs::segment_reader::open(dir, index_segment.meta));
  }

  size_t progress_call_count = 0;

  // test always-true progress
  {
    irs::column_info_provider_t column_info = [](const irs::string_ref&) {
      return irs::column_info(irs::type<irs::compression::lz4>::get(), irs::compression::options{}, true );
    };

    irs::memory_directory dir;
    irs::index_meta::index_segment_t index_segment;
    irs::merge_writer::flush_progress_t progress =
      [&progress_call_count]()->bool { ++progress_call_count; return true; };
    irs::merge_writer writer(dir, column_info);

    index_segment.meta.codec = codec_ptr;
    writer.add(reader[0]);
    writer.add(reader[1]);
    ASSERT_TRUE(writer.flush(index_segment, progress));

    ASSERT_FALSE(index_segment.meta.files.empty());
    ASSERT_EQ(2, index_segment.meta.docs_count);
    ASSERT_EQ(2, index_segment.meta.live_docs_count);
    ASSERT_EQ(0, index_segment.meta.version);
    ASSERT_EQ(true, index_segment.meta.column_store);

    auto segment = irs::segment_reader::open(dir, index_segment.meta);
    ASSERT_EQ(2, segment.docs_count());
  }

  ASSERT_TRUE(progress_call_count); // there should have been at least some calls

  irs::column_info_provider_t column_info = [](const irs::string_ref&) {
    return irs::column_info(irs::type<irs::compression::lz4>::get(), irs::compression::options{}, true );
  };

  // test limited-true progress
  for (size_t i = 1; i < progress_call_count; ++i) { // +1 for pre-decrement in 'progress'
    size_t call_count = i;
    irs::memory_directory dir;
    irs::index_meta::index_segment_t index_segment;
    irs::merge_writer::flush_progress_t progress =
      [&call_count]()->bool { return --call_count; };
    irs::merge_writer writer(dir, column_info);

    index_segment.meta.codec = codec_ptr;
    index_segment.meta.name = "merged";
    writer.add(reader[0]);
    writer.add(reader[1]);
    ASSERT_FALSE(writer.flush(index_segment, progress));
    ASSERT_EQ(0, call_count);

    ASSERT_TRUE(index_segment.filename.empty());
    ASSERT_TRUE(index_segment.meta.name.empty());
    ASSERT_TRUE(index_segment.meta.files.empty());
    ASSERT_FALSE(index_segment.meta.column_store);
    ASSERT_EQ(0, index_segment.meta.version);
    ASSERT_EQ(0, index_segment.meta.docs_count);
    ASSERT_EQ(0, index_segment.meta.live_docs_count);
    ASSERT_EQ(0, index_segment.meta.size);

    ASSERT_ANY_THROW(irs::segment_reader::open(dir, index_segment.meta));
  }
}

TEST_F(merge_writer_tests, test_merge_writer_field_features) {
  //irs::flags STRING_FIELD_FEATURES{ irs::type<irs::frequency>::get(), irs::type<irs::position>::get() };
  //irs::flags TEXT_FIELD_FEATURES{ irs::type<irs::frequency>::get(), irs::type<irs::position>::get(), irs::type<irs::offset>::get(), irs::type<irs::payload>::get() };

  std::string field("doc_string");
  std::string data("string_data");
  tests::document doc1; // string
  tests::document doc2; // text

  doc1.insert(std::make_shared<tests::templates::string_field>(field, data));
  doc2.indexed.push_back(std::make_shared<tests::templates::text_field<irs::string_ref>>(field, data, true));

  ASSERT_TRUE(doc1.indexed.get(field)->features().is_subset_of(doc2.indexed.get(field)->features()));
  ASSERT_FALSE(doc2.indexed.get(field)->features().is_subset_of(doc1.indexed.get(field)->features()));

  auto codec_ptr = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec_ptr);
  irs::memory_directory dir;

  // populate directory
  {
    auto writer = irs::index_writer::make(dir, codec_ptr, irs::OM_CREATE);
    ASSERT_TRUE(insert(*writer,
      doc1.indexed.begin(), doc1.indexed.end(),
      doc1.stored.begin(), doc1.stored.end()
    ));
    writer->commit();
    ASSERT_TRUE(insert(*writer,
      doc2.indexed.begin(), doc2.indexed.end(),
      doc2.stored.begin(), doc2.stored.end()
    ));
    writer->commit();
  }

  auto reader = irs::directory_reader::open(dir, codec_ptr);

  ASSERT_EQ(2, reader.size());
  ASSERT_EQ(1, reader[0].docs_count());
  ASSERT_EQ(1, reader[1].docs_count());

  // test merge existing with feature subset (success)
  {
    irs::column_info_provider_t column_info = [](const irs::string_ref&) {
      return irs::column_info(irs::type<irs::compression::lz4>::get(), irs::compression::options{}, true );
    };

    irs::merge_writer writer(dir, column_info);
    writer.add(reader[1]); // assume 1 is segment with text field
    writer.add(reader[0]); // assume 0 is segment with string field

    irs::index_meta::index_segment_t index_segment;

    index_segment.meta.codec = codec_ptr;
    ASSERT_TRUE(writer.flush(index_segment));
  }

  // test merge existing with feature superset (fail)
  {
    irs::column_info_provider_t column_info = [](const irs::string_ref&) {
      return irs::column_info(irs::type<irs::compression::lz4>::get(), irs::compression::options{}, true );
    };

    irs::merge_writer writer(dir, column_info);
    writer.add(reader[0]); // assume 0 is segment with text field
    writer.add(reader[1]); // assume 1 is segment with string field

    irs::index_meta::index_segment_t index_segment;

    index_segment.meta.codec = codec_ptr;
    ASSERT_FALSE(writer.flush(index_segment));
  }
}

namespace {
struct binary_comparer : public irs::comparer {
 protected:
  bool less(const irs::bytes_ref& lhs, const irs::bytes_ref& rhs) const override {
    if (rhs.null() != lhs.null()) {
      return lhs.null();
    }
    if (!lhs.null()) {
      return lhs < rhs;
    }
    return false;
  }
};
} // namespace

TEST_F(merge_writer_tests, test_merge_writer_sorted) {
  std::string field("title");
  std::string field2("trigger"); // field present in all docs with same term -> will trigger out of order
  std::string value2{ "AAA" };
  std::string data1("A");
  std::string data2("C");
  std::string data3("B");
  std::string data4("D");
  tests::document doc1;
  tests::document doc2;
  tests::document doc3;
  tests::document doc4;

  doc1.insert(std::make_shared<tests::templates::string_field>(field2, value2));
  doc1.insert(std::make_shared<tests::templates::string_field>(field, data1));
  doc1.sorted = doc1.indexed.find(field)[0];
  doc2.insert(std::make_shared<tests::templates::string_field>(field2, value2));
  doc2.insert(std::make_shared<tests::templates::string_field>(field, data2));
  doc2.sorted = doc2.indexed.find(field)[0];
  doc3.insert(std::make_shared<tests::templates::string_field>(field2, value2));
  doc3.insert(std::make_shared<tests::templates::string_field>(field, data3));
  doc3.sorted = doc3.indexed.find(field)[0];
  doc4.insert(std::make_shared<tests::templates::string_field>(field2, value2));
  doc4.insert(std::make_shared<tests::templates::string_field>(field, data4));
  doc4.sorted = doc4.indexed.find(field)[0];

  auto codec_ptr = irs::formats::get("1_3");
  ASSERT_NE(nullptr, codec_ptr);
  irs::memory_directory dir;
  binary_comparer test_comparer;
  irs::column_info_provider_t column_info = [](const irs::string_ref&) {
    return irs::column_info(irs::type<irs::compression::lz4>::get(), irs::compression::options{}, true);
  };
  // populate directory
  {
    irs::index_writer::init_options opts;
    opts.comparator = &test_comparer;
    opts.column_info = column_info;
    auto writer = irs::index_writer::make(dir, codec_ptr, irs::OM_CREATE, opts);
    ASSERT_TRUE(insert(*writer,
      doc1.indexed.begin(), doc1.indexed.end(),
      doc1.stored.begin(), doc1.stored.end(),
      doc1.sorted));
    ASSERT_TRUE(insert(*writer,
      doc2.indexed.begin(), doc2.indexed.end(),
      doc2.stored.begin(), doc2.stored.end(),
      doc2.sorted));
    writer->commit();

    ASSERT_TRUE(insert(*writer,
      doc3.indexed.begin(), doc3.indexed.end(),
      doc3.stored.begin(), doc3.stored.end(),
      doc3.sorted));
    ASSERT_TRUE(insert(*writer,
      doc4.indexed.begin(), doc4.indexed.end(),
      doc4.stored.begin(), doc4.stored.end(),
      doc4.sorted));
    writer->commit();

    // this missing doc will trigger sorting error in merge writer as it will be mapped to eof
    // and block all documents from same segment to be written in correct order.
    // to trigger error documents from second segment need docuemnt from first segment to maintain merged order
    auto query_doc1 = irs::iql::query_builder().build(field + "==A", std::locale::classic());
    writer->documents().remove(std::move(query_doc1.filter));
    writer->commit();
  }


  auto reader = irs::directory_reader::open(dir, codec_ptr);

  ASSERT_EQ(2, reader.size());
  ASSERT_EQ(2, reader[0].docs_count());
  ASSERT_EQ(2, reader[1].docs_count());
  ASSERT_EQ(1, reader[0].live_docs_count());
  ASSERT_EQ(2, reader[1].live_docs_count());


  irs::merge_writer writer(dir, column_info, &test_comparer);
  writer.add(reader[0]);
  writer.add(reader[1]);

  irs::index_meta::index_segment_t index_segment;

  index_segment.meta.codec = codec_ptr;
  ASSERT_TRUE(writer.flush(index_segment));

  auto segment = irs::segment_reader::open(dir, index_segment.meta);
  ASSERT_EQ(3, segment.docs_count());
  ASSERT_EQ(3, segment.live_docs_count());
  auto docs = segment.docs_iterator();
  auto column = segment.column_reader(field);
  auto bytes_values = column->values();

  auto expected_id = irs::doc_limits::min();
  irs::bytes_ref value;
  irs::bytes_ref_input in;
  std::vector<std::string> expected_columns{ "B", "C", "D" };
  size_t idx = 0;
  while (docs->next()) {
    SCOPED_TRACE(testing::Message("Doc id ") << expected_id);
    EXPECT_EQ(expected_id, docs->value());
    ASSERT_TRUE(bytes_values(expected_id, value)); in.reset(value);
    auto actual = irs::read_string<std::string>(in);
    EXPECT_EQ(expected_columns[idx++], actual);
    ++expected_id;
  }
}
