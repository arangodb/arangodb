
////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Andrey Abramov
////////////////////////////////////////////////////////////////////////////////

#include "tests_shared.hpp"
#include "iql/query_builder.hpp"
#include "utils/lz4compression.hpp"
#include "store/memory_directory.hpp"

#include "index_tests.hpp"

class index_column_test_case : public tests::index_test_base { };

TEST_P(index_column_test_case, read_write_doc_attributes_sparse_column_sparse_variable_length) {
  // sparse_column<sparse_block>
  irs::index_writer::init_options options;
  options.column_info = [](const irs::string_ref&) {
    return irs::column_info{ irs::type<irs::compression::lz4>::get(), irs::compression::options{}, true };
  };

  static const irs::doc_id_t MAX_DOCS = 1500;
  static const iresearch::string_ref column_name = "id";
  size_t inserted = 0;

  // write documents
  {
    struct stored {
      const irs::string_ref& name() { return column_name; }

      const irs::flags& features() const {
        return irs::flags::empty_instance();
      }

      bool write(irs::data_output& out) {
        auto str = std::to_string(value);
        if (value % 3) {
          str.append(column_name.c_str(), column_name.size());
        }

        irs::write_string(out, str);
        return true;
      }

      uint64_t value{};
    } field;

    auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE, options);
    auto ctx = writer->documents();

    do {
      auto doc = ctx.insert();

      if (field.value % 2) {
        doc.insert<irs::Action::STORE>(field);
        ++inserted;
      }
    } while (++field.value < MAX_DOCS); // insert MAX_DOCS documents

    { irs::index_writer::documents_context(std::move(ctx)); } // force flush of documents()
    writer->commit();
  }

  // check inserted values:
  // - visit (not cached)
  // - random read (not cached)
  // - random read (cached)
  // - visit (cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(this->dir(), this->codec());
    ASSERT_EQ(1, reader.size());

    auto& segment = *(reader.begin());
    ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

    auto* meta = segment.column(column_name);
    ASSERT_NE(nullptr, meta);

    // check number of documents in the column
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_EQ(MAX_DOCS/2, column->size());
    }

    // visit values (not cached)
    {
      irs::doc_id_t expected_doc = 2;
      irs::doc_id_t expected_value = 1;
      auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

        auto expected_str  = std::to_string(expected_value);
        if (expected_value % 3) {
          expected_str.append(column_name.c_str(), column_name.size());
        }

        if (expected_str != actual_str) {
          return false;
        }

        expected_doc += 2;
        expected_value += 2;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    // read values
    {
      irs::bytes_ref actual_value;
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      // not cached
      for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
        const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();

        if (i % 2) {
          ASSERT_TRUE(values(doc, actual_value));
          const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
          auto expected_str_value = std::to_string(i);
          if (i % 3) {
            expected_str_value.append(column_name.c_str(), column_name.size());
          }
          ASSERT_EQ(expected_str_value, actual_str_value);
        } else {
          ASSERT_FALSE(values(doc, actual_value));
        }
      }

      // cached
      for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
        const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();

        if (i % 2) {
          ASSERT_TRUE(values(doc, actual_value));
          const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
          auto expected_str_value = std::to_string(i);
          if (i % 3) {
            expected_str_value.append(column_name.c_str(), column_name.size());
          }
          ASSERT_EQ(expected_str_value, actual_str_value);
        } else {
          ASSERT_FALSE(values(doc, actual_value));
        }
      }
    }

    // visit values (cached)
    {
      irs::doc_id_t expected_doc = 2;
      irs::doc_id_t expected_value = 1;
      auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

        auto expected_str  = std::to_string(expected_value);
        if (expected_value % 3) {
          expected_str.append(column_name.c_str(), column_name.size());
        }

        if (expected_str != actual_str) {
          return false;
        }

        expected_doc += 2;
        expected_value += 2;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    // iterate over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = 2;
      irs::doc_id_t expected_value = 1;
      size_t docs = 0;
      for (; it->next(); ) {
        const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto expected_value_str  = std::to_string(expected_value);

        if (expected_value % 3) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value_str, actual_str_value);

        expected_doc += 2;
        expected_value += 2;
        ++docs;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(inserted, docs);
    }
  }

  // check inserted values:
  // - visit (not cached)
  // - iterate (not cached)
  // - random read (cached)
  // - visit (cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(this->dir(), this->codec());
    ASSERT_EQ(1, reader.size());

    auto& segment = *(reader.begin());
    ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

    auto* meta = segment.column(column_name);
    ASSERT_NE(nullptr, meta);

    // visit values (not cached)
    {
      irs::doc_id_t expected_doc = 2;
      irs::doc_id_t expected_value = 1;
      auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

        auto expected_str  = std::to_string(expected_value);
        if (expected_value % 3) {
          expected_str.append(column_name.c_str(), column_name.size());
        }

        if (expected_str != actual_str) {
          return false;
        }

        expected_doc += 2;
        expected_value += 2;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    {
      // iterate over column (not cached)
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);


      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = 2;
      irs::doc_id_t expected_value = 1;
      size_t docs = 0;
      for (; it->next(); ) {
        const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto expected_value_str  = std::to_string(expected_value);

        if (expected_value % 3) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value_str, actual_str_value);

        expected_doc += 2;
        expected_value += 2;
        ++docs;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(inserted, docs);
    }

    // read values
    {
      irs::bytes_ref actual_value;
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      // cached
      for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
        const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();

        if (i % 2) {
          ASSERT_TRUE(values(doc, actual_value));
          const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
          auto expected_str_value = std::to_string(i);
          if (i % 3) {
            expected_str_value.append(column_name.c_str(), column_name.size());
          }
          ASSERT_EQ(expected_str_value, actual_str_value);
        } else {
          ASSERT_FALSE(values(doc, actual_value));
        }
      }
    }

    // visit values (cached)
    {
      irs::doc_id_t expected_doc = 2;
      irs::doc_id_t expected_value = 1;
      auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

        auto expected_str  = std::to_string(expected_value);
        if (expected_value % 3) {
          expected_str.append(column_name.c_str(), column_name.size());
        }

        if (expected_str != actual_str) {
          return false;
        }

        expected_doc += 2;
        expected_value += 2;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    // iterate over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = 2;
      irs::doc_id_t expected_value = 1;
      size_t docs = 0;
      for (; it->next(); ) {
        const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto expected_value_str  = std::to_string(expected_value);

        if (expected_value % 3) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value_str, actual_str_value);

        expected_doc += 2;
        expected_value += 2;
        ++docs;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(inserted, docs);
    }
  }

  // check inserted values:
  // - visit (not cached)
  // - seek (not cached)
  // - random read (cached)
  // - visit (cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(this->dir(), this->codec());
    ASSERT_EQ(1, reader.size());

    auto& segment = *(reader.begin());
    ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

    auto* meta = segment.column(column_name);
    ASSERT_NE(nullptr, meta);

    // visit values (not cached)
    {
      irs::doc_id_t expected_doc = 2;
      irs::doc_id_t expected_value = 1;
      auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

        auto expected_str  = std::to_string(expected_value);
        if (expected_value % 3) {
          expected_str.append(column_name.c_str(), column_name.size());
        }

        if (expected_str != actual_str) {
          return false;
        }

        expected_doc += 2;
        expected_value += 2;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    // seek over column (not cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = 2;
      irs::doc_id_t expected_value = 1;
      size_t docs = 0;
      for (; expected_doc <= MAX_DOCS; ) {
        auto expected_value_str  = std::to_string(expected_value);
        if (expected_value % 3) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        auto actual_str_value = irs::to_string<irs::string_ref>(payload->value.c_str());
        ASSERT_EQ(expected_value_str, actual_str_value);

        ASSERT_EQ(expected_doc, it->seek(expected_value)); // seek before the existing key (value should remain the same)
        actual_str_value = irs::to_string<irs::string_ref>(payload->value.c_str());
        ASSERT_EQ(expected_value_str, actual_str_value);

        expected_doc += 2;
        expected_value += 2;
        ++docs;
      }

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(expected_doc));
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(MAX_DOCS + 1));
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(inserted, docs);
    }

    // seek over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = 2;
      irs::doc_id_t expected_value = 1;
      size_t docs = 0;
      for (; expected_doc <= MAX_DOCS; ) {
        auto expected_value_str  = std::to_string(expected_value);
        if (expected_value % 3) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_doc, it->seek(expected_value));
        auto actual_str_value = irs::to_string<irs::string_ref>(payload->value.c_str());
        ASSERT_EQ(expected_value_str, actual_str_value);

        ASSERT_EQ(expected_doc, it->seek(expected_doc)); // seek to the existing key (value should remain the same)
        actual_str_value = irs::to_string<irs::string_ref>(payload->value.c_str());
        ASSERT_EQ(expected_value_str, actual_str_value);

        expected_doc += 2;
        expected_value += 2;
        ++docs;
      }

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(expected_doc));
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(MAX_DOCS + 1));
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(inserted, docs);
    }

    // seek to the begin + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = 2;
      irs::doc_id_t expected_value = 1;
      size_t docs = 0;

      ASSERT_EQ(expected_doc, it->seek(expected_doc));
      const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      auto expected_value_str  = std::to_string(expected_value);

      if (expected_value % 3) {
        expected_value_str.append(column_name.c_str(), column_name.size());
      }

      ASSERT_EQ(expected_value_str, actual_value_str);

      expected_doc += 2;
      expected_value += 2;
      ++docs;

      for (; it->next(); ) {
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto expected_value_str  = std::to_string(expected_value);

        if (expected_value % 3) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value_str, actual_value_str);

        expected_doc += 2;
        expected_value += 2;
        ++docs;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(inserted, docs);
    }

    // seek before the begin + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = 2;
      irs::doc_id_t expected_value = 1;
      size_t docs = 0;

      ASSERT_EQ(expected_doc, it->seek(expected_doc - 1));
      const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      auto expected_value_str  = std::to_string(expected_value);

      if (expected_value % 3) {
        expected_value_str.append(column_name.c_str(), column_name.size());
      }

      ASSERT_EQ(expected_value_str, actual_value_str);

      expected_doc += 2;
      expected_value += 2;
      ++docs;

      for (; it->next(); ) {
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto expected_value_str  = std::to_string(expected_value);

        if (expected_value % 3) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value_str, actual_value_str);

        expected_doc += 2;
        expected_value += 2;
        ++docs;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(inserted, docs);
    }

    // seek to the end + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      auto expected_doc = MAX_DOCS;
      auto expected_value = MAX_DOCS-1;
      auto expected_value_str  = std::to_string(expected_value);
      if (expected_value % 3) {
        expected_value_str.append(column_name.c_str(), column_name.size());
      }

      it->seek(expected_doc);
      const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      ASSERT_EQ(expected_doc, it->value());
      ASSERT_EQ(expected_value_str, actual_value_str);

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
    }

    // seek to before the end + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      auto expected_value = MAX_DOCS-1;
      auto expected_value_str  = std::to_string(expected_value);
      if (expected_value % 3) {
        expected_value_str.append(column_name.c_str(), column_name.size());
      }

      it->seek(expected_value);
      const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      ASSERT_EQ(MAX_DOCS, it->value());
      ASSERT_EQ(expected_value_str, actual_value_str);

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
    }

    // seek to after the end + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(MAX_DOCS + 1));
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      // can't seek backwards
      ASSERT_EQ(irs::doc_limits::eof(), it->seek(MAX_DOCS - 1));
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
    }

    // seek + next(x5)
    {
      const size_t steps_forward = 5;

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = 2;
      irs::doc_id_t expected_value = 1;
      size_t docs = 0;

      for (;;) {
        it->seek(expected_doc);

        if (irs::doc_limits::eof(it->value())) {
          break;
        }

        auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto expected_value_str  = std::to_string(expected_value);

        if (expected_value % 3) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value_str, actual_value_str);

        ++docs;

        auto next_expected_doc = expected_doc + 2;
        auto next_expected_value = expected_value + 2;
        for (auto i = 0; i < steps_forward && it->next(); ++i) {
          actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
          auto next_expected_value_str  = std::to_string(next_expected_value);

          if (next_expected_value % 3) {
            next_expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(next_expected_doc, it->value());
          ASSERT_EQ(next_expected_value_str, actual_value_str);

          // can't seek backwards
          ASSERT_EQ(next_expected_doc, it->seek(expected_doc));
          ASSERT_EQ(next_expected_value_str, actual_value_str);

          next_expected_doc += 2;
          next_expected_value += 2;
          ++docs;
        }

        expected_doc = next_expected_doc;
        expected_value = next_expected_value;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(inserted, docs);
    }

    // seek backwards + next(x5)
    {
      const size_t steps_forward = 5;

      const irs::doc_id_t min_doc = 2;
      irs::doc_id_t expected_doc = MAX_DOCS;
      irs::doc_id_t expected_value = expected_doc - 1;
      size_t docs = 0;

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      for (; expected_doc >= min_doc && expected_doc <= MAX_DOCS;) {
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_FALSE(!payload);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto expected_value_str  = std::to_string(expected_value);

        if (expected_value % 3) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_value_str, actual_value_str);

        ++docs;

        auto next_expected_doc = expected_doc + 2;
        auto next_expected_value = expected_value + 2;
        for (auto i = 0; i < steps_forward && it->next(); ++i) {
          actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
          auto next_expected_value_str  = std::to_string(next_expected_value);

          if (next_expected_value % 3) {
            next_expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(next_expected_doc, it->value());
          ASSERT_EQ(next_expected_value_str, actual_value_str);

          next_expected_doc += 2;
          next_expected_value += 2;
        }

        expected_doc -= 2;
        expected_value -= 2;
      }

      ASSERT_EQ(inserted, docs);

      // seek before the first document
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      it->seek(expected_doc);
      expected_doc = min_doc;
      expected_value = expected_doc - 1;
      ASSERT_EQ(min_doc, it->seek(expected_doc));
      auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      auto expected_value_str  = std::to_string(expected_value);

      if (expected_value % 3) {
        expected_value_str.append(column_name.c_str(), column_name.size());
      }

      ASSERT_EQ(expected_value_str, actual_value_str);

      auto next_expected_doc = expected_doc + 2;
      auto next_expected_value = expected_value + 2;
      for (auto i = 0; i < steps_forward; ++i) {
        ASSERT_TRUE(it->next());
        actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

        auto next_expected_value_str  = std::to_string(next_expected_value);
        if (next_expected_value % 3) {
          next_expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(next_expected_doc, it->value());
        ASSERT_EQ(next_expected_value_str, actual_value_str);

        next_expected_doc += 2;
        next_expected_value += 2;
      }
    }

    // seek backwards + next(x5)
    {
      const size_t steps_forward = 5;

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = MAX_DOCS;
      irs::doc_id_t expected_value = expected_doc - 1;

      ASSERT_EQ(expected_doc, it->seek(expected_doc));
      auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      auto expected_value_str  = std::to_string(expected_value);

      if (expected_value % 3) {
        expected_value_str.append(column_name.c_str(), column_name.size());
      }

      ASSERT_EQ(expected_value_str, actual_value_str);

      auto next_expected_doc = expected_doc + 2;
      auto next_expected_value = expected_value + 2;
      for (auto i = 0; i < steps_forward && it->next(); ++i) {
        actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto next_expected_value_str  = std::to_string(next_expected_value);

        if (next_expected_value % 3) {
          next_expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(next_expected_doc, it->value());
        ASSERT_EQ(next_expected_value_str, actual_value_str);

        next_expected_doc += 2;
        next_expected_value += 2;
      }

      expected_doc -= 2;
      ASSERT_EQ(irs::doc_limits::eof(), it->seek(expected_doc));
    }

    // seek over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = 2;
      irs::doc_id_t expected_value = 1;
      size_t docs = 0;
      for (; expected_doc <= MAX_DOCS; ) {
        ASSERT_EQ(expected_doc, it->seek(expected_doc - 1));
        const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto expected_value_str  = std::to_string(expected_value);

        if (expected_value % 3) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_value_str, actual_str_value);

        expected_doc += 2;
        expected_value += 2;
        ++docs;
      }

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(expected_doc));
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(MAX_DOCS + 1));
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
    }

    // read values
    {
      irs::bytes_ref actual_value;
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      // cached
      for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
        const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();

        if (i % 2) {
          ASSERT_TRUE(values(doc, actual_value));
          const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
          auto expected_str_value = std::to_string(i);
          if (i % 3) {
            expected_str_value.append(column_name.c_str(), column_name.size());
          }
          ASSERT_EQ(expected_str_value, actual_str_value);
        } else {
          ASSERT_FALSE(values(doc, actual_value));
        }
      }
    }

    // visit values (cached)
    {
      irs::doc_id_t expected_doc = 2;
      irs::doc_id_t expected_value = 1;
      auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

        auto expected_str  = std::to_string(expected_value);
        if (expected_value % 3) {
          expected_str.append(column_name.c_str(), column_name.size());
        }

        if (expected_str != actual_str) {
          return false;
        }

        expected_doc += 2;
        expected_value += 2;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    // iterate over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = 2;
      irs::doc_id_t expected_value = 1;
      size_t docs = 0;
      for (; it->next(); ) {
        const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto expected_value_str  = std::to_string(expected_value);

        if (expected_value % 3) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value_str, actual_str_value);

        expected_doc += 2;
        expected_value += 2;
        ++docs;
      }
      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(inserted, docs);
    }
  }
}

TEST_P(index_column_test_case, read_write_doc_attributes_sparse_column_dense_mask) {
  // sparse_column<dense_mask_block>
  irs::index_writer::init_options options;
  options.column_info = [](const irs::string_ref&) {
    return irs::column_info{ irs::type<irs::compression::lz4>::get(), irs::compression::options{}, true };
  };

  static const irs::doc_id_t BLOCK_SIZE = 1024;
  static const irs::doc_id_t MAX_DOCS
    = BLOCK_SIZE*BLOCK_SIZE // full index block
    + 2051;               // tail index block
  static const iresearch::string_ref column_name = "id";

  // write documents
  {
    struct stored {
      explicit stored(const irs::string_ref& name) noexcept
        : column_name(name) {
      }
      const irs::string_ref& name() { return column_name; }
      const irs::flags& features() const {
        return irs::flags::empty_instance();
      }
      bool write(irs::data_output&) { return true; }
      const irs::string_ref column_name;
    } field(column_name), gap("gap");

    irs::doc_id_t docs_count = 0;
    auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE, options);
    auto ctx = writer->documents();

    do {
      ctx.insert().insert<irs::Action::STORE>(field);
    } while (++docs_count < BLOCK_SIZE); // insert BLOCK_SIZE documents

    ctx.insert().insert<irs::Action::STORE>(gap);

    do {
      ctx.insert().insert<irs::Action::STORE>(field);
    } while (++docs_count < MAX_DOCS); // insert BLOCK_SIZE documents

    { irs::index_writer::documents_context(std::move(ctx)); } // force flush of documents()
    writer->commit();
  }

  // check inserted values:
  // - visit (not cached)
  // - random read (not cached)
  // - random read (cached)
  // - visit (cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(this->dir(), this->codec());
    ASSERT_EQ(1, reader.size());

    auto& segment = *(reader.begin());
    ASSERT_EQ(irs::doc_id_t(1+MAX_DOCS), segment.live_docs_count());

    auto* meta = segment.column(column_name);
    ASSERT_NE(nullptr, meta);

    // check number of documents in the column
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_EQ(MAX_DOCS, column->size());
    }

    // visit values (not cached)
    {
      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        if (!actual_data.null()) {
          return false;
        }

        ++expected_doc;
        ++docs_count;

        if (docs_count == BLOCK_SIZE) {
          ++expected_doc; // gap
        }

        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
    }

    // read values
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      irs::bytes_ref actual_value;

      // not cached
      {
        irs::doc_id_t i = 0;
        for (; i < BLOCK_SIZE; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        }

        ASSERT_FALSE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));

        for (++i; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        }
      }

      // cached
      {
        irs::doc_id_t i = 0;
        for (; i < BLOCK_SIZE; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        }

        ASSERT_FALSE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));

        for (++i; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        }
      }
    }

    // visit values (not cached)
    {
      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        if (!actual_data.null()) {
          return false;
        }

        ++expected_doc;
        ++docs_count;

        if (docs_count == BLOCK_SIZE) {
          ++expected_doc; // gap
        }

        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
    }

    // iterate over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_TRUE(payload);

      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      for (; it->next(); ) {
        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value); // mask block has no data
        ++expected_doc;
        ++docs_count;

        if (docs_count == BLOCK_SIZE) {
          // gap
          ++expected_doc;
        }
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
    }
  }

  // check inserted values:
  // - visit (not cached)
  // - iterate (not cached)
  // - random read (cached)
  // - visit (cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(this->dir(), this->codec());
    ASSERT_EQ(1, reader.size());

    auto& segment = *(reader.begin());
    ASSERT_EQ(irs::doc_id_t(1+MAX_DOCS), segment.live_docs_count());

    auto* meta = segment.column(column_name);
    ASSERT_NE(nullptr, meta);

    // visit values (not cached)
    {
      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        if (!actual_data.null()) {
          return false;
        }

        ++expected_doc;
        ++docs_count;

        if (BLOCK_SIZE == docs_count) {
          // gap
          ++expected_doc;
        }

        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
    }

    {
      // iterate over column (not cached)
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_TRUE(payload);

      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      for (; it->next(); ) {
        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value); // mask block has no data
        ++expected_doc;
        ++docs_count;

        if (BLOCK_SIZE == docs_count) {
          // gap
          ++expected_doc;
        }
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
    }

    // read values
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      irs::bytes_ref actual_value;

      // cached
      {
        irs::doc_id_t i = 0;
        for (; i < BLOCK_SIZE; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        }

        // gap
        ASSERT_FALSE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));

        for (++i; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        }
      }
    }

    // visit values (cached)
    {
      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        if (!actual_data.null()) {
          return false;
        }

        ++expected_doc;
        ++docs_count;

        if (BLOCK_SIZE == docs_count) {
          // gap
          ++expected_doc;
        }

        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
    }

    // iterate over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_TRUE(payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      for (; it->next(); ) {
        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value); // mask block has no data
        ++expected_doc;
        ++docs_count;

        if (BLOCK_SIZE == docs_count) {
          // gap
          ++expected_doc;
        }
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
    }
  }

  // check inserted values:
  // - visit (not cached)
  // - seek (not cached)
  // - random read (cached)
  // - visit (cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(this->dir(), this->codec());
    ASSERT_EQ(1, reader.size());

    auto& segment = *(reader.begin());
    ASSERT_EQ(irs::doc_id_t(1+MAX_DOCS), segment.live_docs_count());

    auto* meta = segment.column(column_name);
    ASSERT_NE(nullptr, meta);

    // visit values (not cached)
    {
      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        if (!actual_data.null()) {
          return false;
        }

        ++expected_doc;
        ++docs_count;

        if (BLOCK_SIZE == docs_count) {
          // gap
          ++expected_doc;
        }

        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
    }

    // seek over column (not cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_TRUE(payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      for (; expected_doc <= MAX_DOCS+1; ) {
        if (expected_doc == 1+BLOCK_SIZE) {
          ASSERT_EQ(expected_doc+1, it->seek(expected_doc));
          ++expected_doc; // gap
        } else {
          ASSERT_EQ(expected_doc, it->seek(expected_doc));
        }
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value); // mask block has no data
        ++expected_doc;
        ++docs_count;
      }

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(expected_doc));

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(MAX_DOCS + 1));

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(MAX_DOCS, docs_count);
    }

    // seek to begin + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_TRUE(payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      size_t docs_count = 0;

      ASSERT_EQ(expected_doc, it->seek(expected_doc));
      ++expected_doc;
      ++docs_count;

      for (; it->next(); ) {
        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value); // mask block has no data
        ++expected_doc;
        ++docs_count;

        if (docs_count == BLOCK_SIZE) {
          ++expected_doc; // gap
        }
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(MAX_DOCS, docs_count);
    }

    // seek before begin + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_TRUE(payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      size_t docs_count = 0;

      ASSERT_EQ(expected_doc, it->seek(expected_doc - 1));
      ++expected_doc;
      ++docs_count;

      for (; it->next(); ) {
        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value); // mask block has no data
        ++expected_doc;
        ++docs_count;

        if (docs_count == BLOCK_SIZE) {
          ++expected_doc; // gap
        }
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(MAX_DOCS, docs_count);
    }

    // seek to the end + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_TRUE(payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

      ASSERT_EQ(MAX_DOCS+1, it->seek(MAX_DOCS+1));

      ASSERT_EQ(irs::bytes_ref::NIL, payload->value); // mask block has no data
      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
    }

    // seek to before the end + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_TRUE(payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

      ASSERT_EQ(MAX_DOCS, it->seek(MAX_DOCS));

      ASSERT_EQ(irs::bytes_ref::NIL, payload->value); // mask block has no data

      ASSERT_TRUE(it->next());
      ASSERT_EQ(MAX_DOCS+1, it->value());

      ASSERT_EQ(irs::bytes_ref::NIL, payload->value); // mask block has no data

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
    }

    // seek to after the end + next + seek before end
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_TRUE(payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

      it->seek(MAX_DOCS+2);
      ASSERT_EQ(irs::doc_limits::eof(), it->value());

      // can't seek backwards
      ASSERT_EQ(irs::doc_limits::eof(), it->seek(MAX_DOCS));

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
    }

    // seek to gap + next(x5)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_TRUE(payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

      irs::doc_id_t expected_doc = BLOCK_SIZE+2;
      size_t docs_count = 0;

      ASSERT_EQ(expected_doc, it->seek(expected_doc-1));
      ASSERT_EQ(expected_doc, it->value());

      for (; it->next(); ) {
        ++expected_doc;
        ++docs_count;

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value); // mask block has no data
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
    }

    // seek + next(x5)
    {
      const size_t steps_forward = 5;

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_TRUE(payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      size_t docs_count = 0;

      for (;;) {
        if (docs_count == BLOCK_SIZE) {
          ASSERT_EQ(expected_doc+1, it->seek(expected_doc));
          ++expected_doc; // gap
        } else {
          if (expected_doc > MAX_DOCS+1) {
            ASSERT_EQ(irs::doc_limits::eof(), it->seek(expected_doc));
          } else {
            ASSERT_EQ(expected_doc, it->seek(expected_doc));
          }
        }

        if (irs::doc_limits::eof(it->value())) {
          break;
        }

        ASSERT_EQ(irs::bytes_ref::NIL, payload->value); // mask block has no data

        ++docs_count;
        ASSERT_EQ(expected_doc, it->value());

        auto next_expected_doc = expected_doc + 1;
        for (auto i = 0; i < steps_forward && it->next(); ++i) {
          ASSERT_EQ(next_expected_doc, it->value());

          ASSERT_EQ(irs::bytes_ref::NIL, payload->value); // mask block has no data

          // can't seek backwards
          ASSERT_EQ(next_expected_doc, it->seek(expected_doc));

          ++next_expected_doc;
          ++docs_count;

          if (docs_count == BLOCK_SIZE) {
            ++next_expected_doc; // gap
          }
        }

        expected_doc = next_expected_doc;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(MAX_DOCS, docs_count);
    }

    // seek backwards + next(x5)
    {
      const size_t steps_forward = 5;

      const irs::doc_id_t min_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_doc = MAX_DOCS+1;
      size_t docs_count = 0;

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      for (; expected_doc >= min_doc && expected_doc <= MAX_DOCS+1;) {
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_TRUE(payload);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        ++docs_count;

        if (expected_doc == BLOCK_SIZE+1) {
          ASSERT_EQ(expected_doc+1, it->seek(expected_doc));
          ++expected_doc; // gap
        } else {
          ASSERT_EQ(expected_doc, it->seek(expected_doc));
        }

        auto next_expected_doc = expected_doc + 1;
        for (auto i = 0; i < steps_forward && it->next(); ++i) {
          if (next_expected_doc == BLOCK_SIZE+1) {
            ++next_expected_doc; // gap
          }

          ASSERT_EQ(next_expected_doc, it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value); // mask block has no data
          ++next_expected_doc;
        }

        --expected_doc;

        if (expected_doc == BLOCK_SIZE+1) {
          --expected_doc; // gap
        }
      }
      ASSERT_EQ(MAX_DOCS, docs_count);

      // seek before the first document
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_TRUE(payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

      ASSERT_EQ(min_doc, it->seek(expected_doc));
      expected_doc = min_doc;
      ASSERT_EQ(min_doc, it->seek(expected_doc));
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value); // mask block has no data

      auto next_expected_doc = expected_doc + 1;
      for (auto i = 0; i < steps_forward; ++i) {
        if (next_expected_doc == BLOCK_SIZE+1) {
          ++next_expected_doc; // gap
        }
        ASSERT_TRUE(it->next());
        ASSERT_EQ(next_expected_doc, it->value());
        ++next_expected_doc;
      }
    }

    // seek backwards + next(x5)
    {
      const size_t steps_forward = 5;

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_TRUE(payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

      irs::doc_id_t expected_doc = MAX_DOCS;

      ASSERT_EQ(expected_doc, it->seek(expected_doc));
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value); // mask block has no data

      auto next_expected_doc = expected_doc + 1;
      for (auto i = 0; i < steps_forward && it->next(); ++i) {
        ASSERT_EQ(next_expected_doc, it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value); // mask block has no data
        ++next_expected_doc;
      }

      --expected_doc;
      it->seek(expected_doc);
    }

    // read values
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      irs::bytes_ref actual_value;

      // cached
      {
        irs::doc_id_t i = 0;
        for (; i < BLOCK_SIZE; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        }

        ASSERT_FALSE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));

        for (++i; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        }
      }
    }

    // visit values (cached)
    {
      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        if (!actual_data.null()) {
          return false;
        }

        ++expected_doc;
        ++docs_count;

        if (docs_count == BLOCK_SIZE) {
          ++expected_doc; // gap
        }

        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
    }

    // iterate over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_TRUE(payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      for (; it->next(); ) {
        if (docs_count == BLOCK_SIZE) {
          ++expected_doc; // gap
        }

        ASSERT_EQ(irs::bytes_ref::NIL, payload->value); // mask block has no data

        ASSERT_EQ(expected_doc, it->value());
        ++expected_doc;
        ++docs_count;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
    }
  }
}

TEST_P(index_column_test_case, read_write_doc_attributes_sparse_column_dense_variable_length) {
  // sparse_column<dense_block>
  irs::index_writer::init_options options;
  options.column_info = [](const irs::string_ref&) {
    return irs::column_info{ irs::type<irs::compression::none>::get(), irs::compression::options{}, true };
  };

  static const irs::doc_id_t BLOCK_SIZE = 1024;
  static const irs::doc_id_t MAX_DOCS = 1500;
  static const iresearch::string_ref column_name = "id";

  // write documents
  {
    struct stored {
      explicit stored(const irs::string_ref& name) noexcept
        : column_name(name) {
      }
      const irs::string_ref& name() { return column_name; }

      const irs::flags& features() const {
        return irs::flags::empty_instance();
      }

      bool write(irs::data_output& out) {
        auto str = std::to_string(value);
        if (value % 2) {
          str.append(column_name.c_str(), column_name.size());
        }

        irs::write_string(out, str);
        return true;
      }

      uint64_t value{};
      const irs::string_ref column_name;
    } field(column_name), gap("gap");

    auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE, options);
    auto ctx = writer->documents();

    do {
      ctx.insert().insert<irs::Action::STORE>(field);
    } while (++field.value < BLOCK_SIZE); // insert MAX_DOCS documents

    ctx.insert().insert<irs::Action::STORE>(gap); // gap
    ++field.value;

    do {
      ctx.insert().insert<irs::Action::STORE>(field);
    } while (++field.value <= MAX_DOCS); // insert MAX_DOCS documents

    { irs::index_writer::documents_context(std::move(ctx)); } // force flush of documents()
    writer->commit();
  }

  // check inserted values:
  // - visit (not cached)
  // - random read (not cached)
  // - random read (cached)
  // - visit (cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(this->dir(), this->codec());
    ASSERT_EQ(1, reader.size());

    auto& segment = *(reader.begin());
    ASSERT_EQ(irs::doc_id_t(1+MAX_DOCS), segment.live_docs_count());

    auto* meta = segment.column(column_name);
    ASSERT_NE(nullptr, meta);

    // check number of documents in the column
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_EQ(MAX_DOCS, column->size());
    }

    // visit values (not cached)
    {
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

        auto expected_str  = std::to_string(expected_value);
        if (expected_value % 2) {
          expected_str.append(column_name.c_str(), column_name.size());
        }

        if (expected_str != actual_str) {
          return false;
        }

        ++expected_doc;
        ++expected_value;

        if (expected_doc == BLOCK_SIZE + 1) {
          ++expected_doc; // gap
          ++expected_value;
        }

        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    // read values
    {
      irs::bytes_ref actual_value;
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      // not cached
      {
        irs::doc_id_t i = 0;

        for (; i < BLOCK_SIZE; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
          auto expected_str_value = std::to_string(i);
          if (i % 2) {
            expected_str_value.append(column_name.c_str(), column_name.size());
          }
          ASSERT_EQ(expected_str_value, actual_str_value);
        }

        ASSERT_FALSE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value)); // gap

        for (++i; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
          auto expected_str_value = std::to_string(i);
          if (i % 2) {
            expected_str_value.append(column_name.c_str(), column_name.size());
          }
          ASSERT_EQ(expected_str_value, actual_str_value);
        }
      }

      // cached
      {
        irs::doc_id_t i = 0;

        for (; i < BLOCK_SIZE; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
          auto expected_str_value = std::to_string(i);
          if (i % 2) {
            expected_str_value.append(column_name.c_str(), column_name.size());
          }
          ASSERT_EQ(expected_str_value, actual_str_value);
        }

        ASSERT_FALSE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value)); // gap

        for (++i; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
          auto expected_str_value = std::to_string(i);
          if (i % 2) {
            expected_str_value.append(column_name.c_str(), column_name.size());
          }
          ASSERT_EQ(expected_str_value, actual_str_value);
        }
      }
    }

    // visit values (cached)
    {
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

        auto expected_str  = std::to_string(expected_value);
        if (expected_value % 2) {
          expected_str.append(column_name.c_str(), column_name.size());
        }

        if (expected_str != actual_str) {
          return false;
        }

        ++expected_doc;
        ++expected_value;

        if (expected_doc == BLOCK_SIZE + 1) {
          ++expected_doc; // gap
          ++expected_value;
        }

        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    // iterate over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      size_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      for (; it->next(); ) {
        const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto expected_value_str = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value_str, actual_str_value);

        ++expected_doc;
        ++expected_value;
        ++docs_count;

        if (docs_count == BLOCK_SIZE) {
          ++expected_doc; // gap
          ++expected_value;
        }
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(MAX_DOCS, docs_count);
    }
  }

  // check inserted values:
  // - visit (not cached)
  // - iterate (not cached)
  // - random read (cached)
  // - visit (cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(this->dir(), this->codec());
    ASSERT_EQ(1, reader.size());

    auto& segment = *(reader.begin());
    ASSERT_EQ(irs::doc_id_t(1+MAX_DOCS), segment.live_docs_count());

    auto* meta = segment.column(column_name);
    ASSERT_NE(nullptr, meta);

    // visit values (not cached)
    {
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

        auto expected_str  = std::to_string(expected_value);
        if (expected_value % 2) {
          expected_str.append(column_name.c_str(), column_name.size());
        }

        if (expected_str != actual_str) {
          return false;
        }

        ++expected_doc;
        ++expected_value;

        if (expected_doc == BLOCK_SIZE + 1) {
          ++expected_doc; // gap
          ++expected_value;
        }

        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    {
      // iterate over column (not cached)
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      size_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      for (; it->next(); ) {
        const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto expected_value_str = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value_str, actual_str_value);

        ++docs_count;
        ++expected_doc;
        ++expected_value;

        if (expected_doc == BLOCK_SIZE + 1) {
          ++expected_doc; // gap
          ++expected_value;
        }
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(MAX_DOCS, docs_count);
    }

    // read values
    {
      irs::bytes_ref actual_value;
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      // cached
      {
        irs::doc_id_t i = 0;

        for (; i < BLOCK_SIZE; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
          auto expected_str_value = std::to_string(i);
          if (i % 2) {
            expected_str_value.append(column_name.c_str(), column_name.size());
          }
          ASSERT_EQ(expected_str_value, actual_str_value);
        }

        ASSERT_FALSE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value)); // gap

        for (++i; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
          auto expected_str_value = std::to_string(i);
          if (i % 2) {
            expected_str_value.append(column_name.c_str(), column_name.size());
          }
          ASSERT_EQ(expected_str_value, actual_str_value);
        }
      }
    }

    // visit values (cached)
    {
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

        auto expected_str  = std::to_string(expected_value);
        if (expected_value % 2) {
          expected_str.append(column_name.c_str(), column_name.size());
        }

        if (expected_str != actual_str) {
          return false;
        }

        ++expected_doc;
        ++expected_value;

        if (expected_doc == BLOCK_SIZE+1) {
          ++expected_doc; // gap
          ++expected_value;
        }

        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    // iterate over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      size_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      for (; it->next(); ) {
        const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto expected_value_str = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value_str, actual_str_value);

        ++docs_count;
        ++expected_doc;
        ++expected_value;

        if (expected_doc == BLOCK_SIZE+1) {
          ++expected_doc; // gap
          ++expected_value;
        }
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(MAX_DOCS, docs_count);
    }
  }

  // check inserted values:
  // - visit (not cached)
  // - seek (not cached)
  // - random read (cached)
  // - visit (cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(this->dir(), this->codec());
    ASSERT_EQ(1, reader.size());

    auto& segment = *(reader.begin());
    ASSERT_EQ(irs::doc_id_t(1+MAX_DOCS), segment.live_docs_count());

    auto* meta = segment.column(column_name);
    ASSERT_NE(nullptr, meta);

    // visit values (not cached)
    {
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

        auto expected_str  = std::to_string(expected_value);
        if (expected_value % 2) {
          expected_str.append(column_name.c_str(), column_name.size());
        }

        if (expected_str != actual_str) {
          return false;
        }

        ++expected_doc;
        ++expected_value;

        if (expected_doc == BLOCK_SIZE+1) {
          ++expected_doc; // gap
          ++expected_value;
        }

        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    // seek over column (not cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      size_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      for (; expected_doc <= MAX_DOCS+1; ) {
        if (expected_doc == BLOCK_SIZE+1) {
          ASSERT_EQ(expected_doc+1, it->seek(expected_doc));
          ++expected_doc; // gap
          ++expected_value;
        } else {
          ASSERT_EQ(expected_doc, it->seek(expected_doc));
        }

        const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto expected_value_str = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_value_str, actual_str_value);

        ++expected_doc;
        ++expected_value;
        ++docs_count;
      }

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(expected_doc));
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(MAX_DOCS + 1));
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(MAX_DOCS, docs_count);
    }

    // seek to the begin + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      size_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;

      ASSERT_EQ(expected_doc, it->seek(expected_doc));
      const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      auto expected_value_str = std::to_string(expected_value);

      if (expected_value % 2) {
        expected_value_str.append(column_name.c_str(), column_name.size());
      }

      ASSERT_EQ(expected_value_str, actual_value_str);

      ++docs_count;
      ++expected_doc;
      ++expected_value;

      for (; it->next(); ) {
        if (expected_doc == BLOCK_SIZE+1) {
          ++expected_doc; // gap
          ++expected_value;
        }

        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto expected_value_str = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value_str, actual_value_str);

        ++docs_count;
        ++expected_doc;
        ++expected_value;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(MAX_DOCS, docs_count);
    }

    // seek before the begin + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      size_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;

      ASSERT_EQ(expected_doc, it->seek(expected_doc - 1));
      const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      auto expected_value_str = std::to_string(expected_value);

      if (expected_value % 2) {
        expected_value_str.append(column_name.c_str(), column_name.size());
      }

      ASSERT_EQ(expected_value_str, actual_value_str);

      ++docs_count;
      ++expected_doc;
      ++expected_value;

      for (; it->next(); ) {
        if (expected_doc == BLOCK_SIZE+1) {
          ++expected_doc; // gap
          ++expected_value;
        }

        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto expected_value_str = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value_str, actual_value_str);

        ++docs_count;
        ++expected_doc;
        ++expected_value;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(MAX_DOCS, docs_count);
    }

    // seek to the end + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      auto expected_doc = MAX_DOCS+1;
      auto expected_value = MAX_DOCS;

      ASSERT_EQ(expected_doc, it->seek(expected_doc));
      const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      auto expected_value_str  = std::to_string(expected_value);

      if (expected_value % 2) {
        expected_value_str.append(column_name.c_str(), column_name.size());
      }

      ASSERT_EQ(expected_value_str, actual_value_str);

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
    }

    // seek to before the end + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      auto expected_doc = MAX_DOCS;
      auto expected_value = expected_doc-1;

      ASSERT_EQ(expected_doc, it->seek(expected_doc));
      auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      auto expected_value_str  = std::to_string(expected_value);

      if (expected_value % 2) {
        expected_value_str.append(column_name.c_str(), column_name.size());
      }

      ASSERT_EQ(expected_value_str, actual_value_str);

      ++expected_doc;
      ++expected_value;
      expected_value_str  = std::to_string(expected_value);
      if (expected_value % 2) {
        expected_value_str.append(column_name.c_str(), column_name.size());
      }

      ASSERT_TRUE(it->next());
      ASSERT_EQ(expected_doc, it->value());
      ASSERT_EQ(expected_value_str, irs::to_string<irs::string_ref>(payload->value.c_str()));

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
    }

    // seek to after the end + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(MAX_DOCS + 2));
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      // can't seek backwards
      ASSERT_EQ(irs::doc_limits::eof(), it->seek(MAX_DOCS));
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
    }

    // seek + next(x5)
    {
      const size_t steps_forward = 5;

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      size_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;

      for (;;) {
        if (expected_doc == BLOCK_SIZE+1) {
          ASSERT_EQ(expected_doc+1, it->seek(expected_doc));
          ++expected_doc; // gap
          ++expected_value;
        } else {
          if (expected_doc > MAX_DOCS+1) {
            ASSERT_EQ(irs::doc_limits::eof(), it->seek(expected_doc));
          } else {
            ASSERT_EQ(expected_doc, it->seek(expected_doc));
          }
        }

        if (irs::doc_limits::eof(it->value())) {
          break;
        }

        ++docs_count;

        auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto expected_value_str  = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value_str, actual_value_str);

        auto next_expected_doc = expected_doc + 1;
        auto next_expected_value = expected_value + 1;
        for (auto i = 0; i < steps_forward && it->next(); ++i) {
          if (next_expected_doc == BLOCK_SIZE + 1) {
            ++next_expected_doc; // gap
            ++next_expected_value;
          }

          actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
          auto next_expected_value_str  = std::to_string(next_expected_value);

          if (next_expected_value % 2) {
            next_expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(next_expected_doc, it->value());
          ASSERT_EQ(next_expected_value_str, actual_value_str);

          // can't seek backwards
          ASSERT_EQ(next_expected_doc, it->seek(expected_doc));
          ASSERT_EQ(next_expected_value_str, actual_value_str);

          ++docs_count;
          ++next_expected_doc;
          ++next_expected_value;
        }

        expected_doc = next_expected_doc;
        expected_value = next_expected_value;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(MAX_DOCS, docs_count);
    }

    // seek backwards + next(x5)
    {
      const size_t steps_forward = 5;

      const irs::doc_id_t min_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_doc = MAX_DOCS+1;
      irs::doc_id_t expected_value = expected_doc - 1;
      size_t docs_count = 0;

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      for (; expected_doc >= min_doc && expected_doc <= MAX_DOCS+1;) {
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_FALSE(!payload);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto expected_value_str  = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ++docs_count;

        ASSERT_EQ(expected_value_str, actual_value_str);

        auto next_expected_doc = expected_doc + 1;
        auto next_expected_value = expected_value + 1;
        for (auto i = 0; i < steps_forward && it->next(); ++i) {
          if (next_expected_doc == BLOCK_SIZE+1) {
            ++next_expected_doc; // gap
            ++next_expected_value;
          }

          actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
          auto next_expected_value_str  = std::to_string(next_expected_value);

          if (next_expected_value % 2) {
            next_expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(next_expected_doc, it->value());
          ASSERT_EQ(next_expected_value_str, actual_value_str);

          ++next_expected_doc;
          ++next_expected_value;
        }

        --expected_doc;
        --expected_value;

        if (expected_doc == BLOCK_SIZE+1) {
          --expected_doc; // gap
          --expected_value;
        }
      }
      ASSERT_EQ(MAX_DOCS, docs_count);

      // seek before the first document
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      it->seek(expected_doc);
      expected_doc = min_doc;
      expected_value = expected_doc - 1;
      ASSERT_EQ(min_doc, it->seek(expected_doc));
      auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      auto expected_value_str  = std::to_string(expected_value);

      if (expected_value % 2) {
        expected_value_str.append(column_name.c_str(), column_name.size());
      }

      ASSERT_EQ(expected_value_str, actual_value_str);

      auto next_expected_doc = expected_doc + 1;
      auto next_expected_value = expected_value + 1;
      for (auto i = 0; i < steps_forward; ++i) {
        ASSERT_TRUE(it->next());
        actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

        auto next_expected_value_str  = std::to_string(next_expected_value);
        if (next_expected_value % 2) {
          next_expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(next_expected_doc, it->value());
        ASSERT_EQ(next_expected_value_str, actual_value_str);

        ++next_expected_doc;
        ++next_expected_value;
      }
    }

    // seek backwards + next(x5)
    {
      const size_t steps_forward = 5;

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = MAX_DOCS;
      irs::doc_id_t expected_value = expected_doc - 1;

      ASSERT_EQ(expected_doc, it->seek(expected_doc));
      auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      auto expected_value_str  = std::to_string(expected_value);

      if (expected_value % 2) {
        expected_value_str.append(column_name.c_str(), column_name.size());
      }

      ASSERT_EQ(expected_value_str, actual_value_str);

      auto next_expected_doc = expected_doc + 1;
      auto next_expected_value = expected_value + 1;
      for (auto i = 0; i < steps_forward && it->next(); ++i) {
        if (next_expected_doc == BLOCK_SIZE+1) {
          ++next_expected_doc; // gap
          ++next_expected_value;
        }

        actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto next_expected_value_str  = std::to_string(next_expected_value);

        if (next_expected_value % 2) {
          next_expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(next_expected_doc, it->value());
        ASSERT_EQ(next_expected_value_str, actual_value_str);

        ++next_expected_doc;
        ++next_expected_value;
      }

      --expected_doc;
      ASSERT_EQ(irs::doc_limits::eof(), it->seek(expected_doc));
    }

    // read values
    {
      irs::bytes_ref actual_value;
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      // cached
      {
        irs::doc_id_t i = 0;

        for (; i < BLOCK_SIZE; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
          auto expected_str_value = std::to_string(i);
          if (i % 2) {
            expected_str_value.append(column_name.c_str(), column_name.size());
          }
          ASSERT_EQ(expected_str_value, actual_str_value);
        }

        ASSERT_FALSE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value)); // gap

        for (++i; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
          auto expected_str_value = std::to_string(i);
          if (i % 2) {
            expected_str_value.append(column_name.c_str(), column_name.size());
          }
          ASSERT_EQ(expected_str_value, actual_str_value);
        }
      }
    }

    // visit values (cached)
    {
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

        auto expected_str  = std::to_string(expected_value);
        if (expected_value % 2) {
          expected_str.append(column_name.c_str(), column_name.size());
        }

        if (expected_str != actual_str) {
          return false;
        }

        ++expected_doc;
        ++expected_value;

        if (expected_doc == BLOCK_SIZE+1) {
          ++expected_doc; // gap
          ++expected_value;
        }

        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    // iterate over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      size_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      for (; it->next(); ) {
        const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto expected_value_str = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value_str, actual_str_value);

        ++docs_count;
        ++expected_doc;
        ++expected_value;

        if (expected_doc == BLOCK_SIZE+1) {
          ++expected_doc; // gap
          ++expected_value;
        }
      }
      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(MAX_DOCS, docs_count);
    }
  }
}

TEST_P(index_column_test_case, read_write_doc_attributes_sparse_column_dense_fixed_offset) {
  // sparse_column<dense_fixed_length_block>

  irs::index_writer::init_options options;
  options.column_info = [](const irs::string_ref&) {
    return irs::column_info{ irs::type<irs::compression::none>::get(), irs::compression::options{}, false };
  };

  // border case for sparse fixed offset columns, e.g.
  // |--------------|------------|
  // |doc           | value_size |
  // |--------------|------------|
  // | 1            | 0          |
  // | .            | 0          |
  // | .            | 0          |
  // | .            | 0          |
  // | BLOCK_SIZE-1 | 1          | <-- end of column block
  // | BLOCK_SIZE+1 | 0          |
  // | .            | 0          |
  // | .            | 0          |
  // | MAX_DOCS     | 1          |
  // |--------------|------------|

  static const irs::doc_id_t BLOCK_SIZE = 1024;
  static const irs::doc_id_t MAX_DOCS = 1500;
  static const irs::string_ref column_name = "id";
  size_t inserted = 0;

  // write documents
  {
    struct stored {
      explicit stored(const irs::string_ref& name) noexcept
        : column_name(name) {
      }

      const irs::string_ref& name() noexcept { return column_name; }

      const irs::flags& features() const {
        return irs::flags::empty_instance();
      }

      bool write(irs::data_output& out) {
        if (value == BLOCK_SIZE-1) {
          out.write_byte(0);
        } else if (value == MAX_DOCS) {
          out.write_byte(1);
        }

        return true;
      }

      uint32_t value{};
      const irs::string_ref column_name;
    } field(column_name), gap("gap");

    auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE);
    auto ctx = writer->documents();

    do {
      ctx.insert().insert<irs::Action::STORE>(field);
      ++inserted;
    } while (++field.value < BLOCK_SIZE); // insert BLOCK_SIZE documents

    ctx.insert().insert<irs::Action::STORE>(gap); // gap
    ++field.value;

    do {
      ctx.insert().insert<irs::Action::STORE>(field);
      ++inserted;
    } while (++field.value < (1+MAX_DOCS)); // insert BLOCK_SIZE documents

    { irs::index_writer::documents_context(std::move(ctx)); } // force flush of documents()
    writer->commit();
  }

  // check inserted values:
  // - visit (not cached)
  // - random read (not cached)
  // - random read (cached)
  // - visit (cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(this->dir(), this->codec());
    ASSERT_EQ(1, reader.size());

    auto& segment = *(reader.begin());
    ASSERT_EQ(irs::doc_id_t(1+MAX_DOCS), segment.live_docs_count());

    auto* meta = segment.column(column_name);
    ASSERT_NE(nullptr, meta);

    // check number of documents in the column
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_EQ(MAX_DOCS, column->size());
    }

    // visit values (not cached)
    {
      size_t count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      auto visitor = [&count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        ++expected_doc;

        if (++count == BLOCK_SIZE) {
          ++expected_doc; // gap
        }

        if (count == BLOCK_SIZE) {
          if (irs::ref_cast<irs::byte_type>(irs::string_ref("\0", 1)) != actual_data) {
            return false;
          }
        } else if (count == MAX_DOCS) {
          if (irs::ref_cast<irs::byte_type>(irs::string_ref("\1", 1)) != actual_data) {
            return false;
          }
        } else if (!actual_data.empty()) {
          return false;
        }

        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    // read values
    {
      irs::bytes_ref actual_value;
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      // not cached
      {
        irs::doc_id_t i = 0;
        for (; i < BLOCK_SIZE-1; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        }

        ASSERT_TRUE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));
        ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("\0", 1)), actual_value);
        ASSERT_FALSE(values(++i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));

        for (++i; i <= MAX_DOCS-1; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        }

        ASSERT_TRUE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));
        ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("\1", 1)), actual_value);
      }

      // cached
      {
        irs::doc_id_t i = 0;
        for (; i < BLOCK_SIZE-1; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        }

        ASSERT_TRUE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));
        ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("\0", 1)), actual_value);
        ASSERT_FALSE(values(++i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));

        for (++i; i <= MAX_DOCS-1; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
        }

        ASSERT_TRUE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));
        ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("\1", 1)), actual_value);
      }
    }

    // visit values (cached)
    {
      size_t count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      auto visitor = [&count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        ++expected_doc;

        if (++count == BLOCK_SIZE) {
          ++expected_doc; // gap
        }

        if (count == BLOCK_SIZE) {
          if (irs::ref_cast<irs::byte_type>(irs::string_ref("\0", 1)) != actual_data) {
            return false;
          }
        } else if (count == MAX_DOCS) {
          if (irs::ref_cast<irs::byte_type>(irs::string_ref("\1", 1)) != actual_data) {
            return false;
          }
        } else if (!actual_data.empty()) {
          return false;
        }

        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    // iterate over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      size_t count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      for (; it->next(); ) {
        const auto actual_data = payload->value;

        ASSERT_EQ(expected_doc, it->value());

        ++expected_doc;

        if (++count == BLOCK_SIZE) {
          ++expected_doc; // gap
        }

        if (count == BLOCK_SIZE) {
          ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("\0", 1)), actual_data);
        } else if (count == MAX_DOCS) {
          ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("\1", 1)), actual_data);
        } else {
          ASSERT_EQ(irs::bytes_ref::NIL, actual_data);
        }
      }
      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(MAX_DOCS, count);
    }
  }
}

TEST_P(index_column_test_case, read_write_doc_attributes_dense_column_dense_fixed_offset) {
  // dense_fixed_length_column<dense_fixed_length_block>

  irs::index_writer::init_options options;
  options.column_info = [](const irs::string_ref&) {
    return irs::column_info{ irs::type<irs::compression::lz4>::get(), irs::compression::options{}, true };
  };

  // border case for dense fixed offset columns, e.g.
  // |--------------|------------|
  // |doc           | value_size |
  // |--------------|------------|
  // | 1            | 0          |
  // | .            | 0          |
  // | .            | 0          |
  // | .            | 0          |
  // | BLOCK_SIZE-1 | 1          | <-- end of column block
  // | BLOCK_SIZE   | 0          |
  // | .            | 0          |
  // | .            | 0          |
  // | MAX_DOCS     | 1          |
  // |--------------|------------|

  static const irs::doc_id_t MAX_DOCS = 1500;
  static const irs::doc_id_t BLOCK_SIZE = 1024;
  static const iresearch::string_ref column_name = "id";

  // write documents
  {
    struct stored {
      const irs::string_ref& name() { return column_name; }

      const irs::flags& features() const {
        return irs::flags::empty_instance();
      }

      bool write(irs::data_output& out) {
        if (value == BLOCK_SIZE-1) {
          out.write_byte(0);
        } else if (value == MAX_DOCS-1) {
          out.write_byte(1);
        }
        return true;
      }

      uint64_t value{};
    } field;

    auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE, options);
    auto ctx = writer->documents();

    do {
      ctx.insert().insert<irs::Action::STORE>(field);
    } while (++field.value < MAX_DOCS); // insert MAX_DOCS documents

    { irs::index_writer::documents_context(std::move(ctx)); } // force flush of documents()
    writer->commit();
  }

  // check inserted values:
  // - visit (not cached)
  // - random read (not cached)
  // - random read (cached)
  // - visit (cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(this->dir(), this->codec());
    ASSERT_EQ(1, reader.size());

    auto& segment = *(reader.begin());
    ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

    auto* meta = segment.column(column_name);
    ASSERT_NE(nullptr, meta);

    // check number of documents in the column
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_EQ(MAX_DOCS, column->size());
    }

    // visit values (not cached)
    {
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      size_t count = 0;
      auto visitor = [&count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        ++expected_doc;
        ++count;

        if (count == BLOCK_SIZE) {
          if (irs::ref_cast<irs::byte_type>(irs::string_ref("\0", 1)) != actual_data) {
            return false;
          }
        } else if (count == MAX_DOCS) {
          if (irs::ref_cast<irs::byte_type>(irs::string_ref("\1", 1)) != actual_data) {
            return false;
          }
        } else if (!actual_data.empty()) {
          return false;
        }

        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    // read values
    {
      irs::bytes_ref actual_value;
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      irs::doc_id_t i = 0;
      for (; i < BLOCK_SIZE-1; ++i) {
        const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
        ASSERT_TRUE(values(doc, actual_value));
        ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
      }

      ASSERT_TRUE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));
      ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("\0", 1)), actual_value);

      for (++i; i < MAX_DOCS-1; ++i) {
        const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
        ASSERT_TRUE(values(doc, actual_value));
        ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
      }

      ASSERT_TRUE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));
      ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("\1", 1)), actual_value);
    }


    // visit values (cached)
    {
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      size_t count = 0;
      auto visitor = [&count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        ++expected_doc;
        ++count;

        if (count == BLOCK_SIZE) {
          if (irs::ref_cast<irs::byte_type>(irs::string_ref("\0", 1)) != actual_data) {
            return false;
          }
        } else if (count == MAX_DOCS) {
          if (irs::ref_cast<irs::byte_type>(irs::string_ref("\1", 1)) != actual_data) {
            return false;
          }
        } else if (!actual_data.empty()) {
          return false;
        }

        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    // iterate over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      size_t count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      for (; it->next(); ) {
        const auto actual_data = payload->value;

        ASSERT_EQ(expected_doc, it->value());

        ++expected_doc;
        ++count;

        if (count == BLOCK_SIZE) {
          ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("\0", 1)), actual_data);
        } else if (count == MAX_DOCS) {
          ASSERT_EQ(irs::ref_cast<irs::byte_type>(irs::string_ref("\1", 1)), actual_data);
        } else {
          ASSERT_EQ(irs::bytes_ref::NIL, actual_data);
        }
      }
      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(MAX_DOCS, count);
    }
  }
}

TEST_P(index_column_test_case, read_write_doc_attributes_sparse_column_dense_fixed_length) {
  // sparse_column<dense_fixed_length_block>
  irs::index_writer::init_options options;
  options.column_info = [](const irs::string_ref&) {
    return irs::column_info{ irs::type<irs::compression::lz4>::get(), irs::compression::options{}, false };
  };

  static const irs::doc_id_t BLOCK_SIZE = 1024;
  static const irs::doc_id_t MAX_DOCS = 1500;
  static const irs::string_ref column_name = "id";
  size_t inserted = 0;

  // write documents
  {
    struct stored {
      explicit stored(const irs::string_ref& name) noexcept
        : column_name(name) {
      }

      const irs::string_ref& name() noexcept { return column_name; }

      const irs::flags& features() const {
        return irs::flags::empty_instance();
      }

      bool write(irs::data_output& out) {
        irs::write_string(
          out,
          irs::numeric_utils::numeric_traits<uint32_t>::raw_ref(value)
        );
        return true;
      }

      uint32_t value{};
      const irs::string_ref column_name;
    } field(column_name), gap("gap");

    auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE, options);
    auto ctx = writer->documents();

    do {
      ctx.insert().insert<irs::Action::STORE>(field);
      ++inserted;
    } while (++field.value < BLOCK_SIZE); // insert BLOCK_SIZE documents

    ctx.insert().insert<irs::Action::STORE>(gap); // gap
    ++field.value;

    do {
      ctx.insert().insert<irs::Action::STORE>(field);
      ++inserted;
    } while (++field.value < (1+MAX_DOCS)); // insert BLOCK_SIZE documents

    { irs::index_writer::documents_context(std::move(ctx)); } // force flush of documents()
    writer->commit();
  }

  // check inserted values:
  // - visit (not cached)
  // - random read (not cached)
  // - random read (cached)
  // - visit (cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(this->dir(), this->codec());
    ASSERT_EQ(1, reader.size());

    auto& segment = *(reader.begin());
    ASSERT_EQ(irs::doc_id_t(1+MAX_DOCS), segment.live_docs_count());

    auto* meta = segment.column(column_name);
    ASSERT_NE(nullptr, meta);

    // check number of documents in the column
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_EQ(MAX_DOCS, column->size());
    }

    // visit values (not cached)
    {
      size_t count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      auto visitor = [&count, &expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_value = irs::to_string<irs::string_ref>(actual_data.c_str());
        if (expected_value != *reinterpret_cast<const irs::doc_id_t*>(actual_value.c_str())) {
          return false;
        }

        ++expected_doc;
        ++expected_value;

        if (++count == BLOCK_SIZE) {
          ++expected_doc; // gap
          ++expected_value;
        }
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    // read values
    {
      irs::bytes_ref actual_value;
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      // not cached
      {
        irs::doc_id_t i = 0;
        for (; i < BLOCK_SIZE; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.c_str());
          ASSERT_EQ(i, *reinterpret_cast<const irs::doc_id_t*>(actual_str_value.c_str()));
        }

        ASSERT_FALSE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));

        for (++i; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.c_str());
          ASSERT_EQ(i, *reinterpret_cast<const irs::doc_id_t*>(actual_str_value.c_str()));
        }
      }

      // cached
      {
        irs::doc_id_t i = 0;
        for (; i < BLOCK_SIZE; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.c_str());
          ASSERT_EQ(i, *reinterpret_cast<const irs::doc_id_t*>(actual_str_value.c_str()));
        }

        ASSERT_FALSE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));

        for (++i; i < MAX_DOCS; ++i) {
          const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
          ASSERT_TRUE(values(doc, actual_value));
          const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.c_str());
          ASSERT_EQ(i, *reinterpret_cast<const irs::doc_id_t*>(actual_str_value.c_str()));
        }
      }
    }

    // visit values (cached)
    {
      size_t count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      auto visitor = [&count, &expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_value = irs::to_string<irs::string_ref>(actual_data.c_str());
        if (expected_value != *reinterpret_cast<const irs::doc_id_t*>(actual_value.c_str())) {
          return false;
        }

        ++expected_doc;
        ++expected_value;

        if (++count == BLOCK_SIZE) {
          ++expected_doc; // gap
          ++expected_value;
        }
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    // iterate over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      size_t count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      for (; it->next(); ) {
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ++expected_doc;
        ++expected_value;

        if (++count == BLOCK_SIZE) {
          ++expected_doc; // gap
          ++expected_value;
        }
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(irs::doc_id_t(1+MAX_DOCS), expected_value);
    }
  }

  // check inserted values:
  // - visit (not cached)
  // - iterate (not cached)
  // - random read (cached)
  // - visit (cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(this->dir(), this->codec());
    ASSERT_EQ(1, reader.size());

    auto& segment = *(reader.begin());
    ASSERT_EQ(irs::doc_id_t(1+MAX_DOCS), segment.live_docs_count());

    auto* meta = segment.column(column_name);
    ASSERT_NE(nullptr, meta);

    // visit values (not cached)
    {
      size_t count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      auto visitor = [&count, &expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_value = irs::to_string<irs::string_ref>(actual_data.c_str());
        if (expected_value != *reinterpret_cast<const irs::doc_id_t*>(actual_value.c_str())) {
          return false;
        }

        ++expected_doc;
        ++expected_value;

        if (++count == BLOCK_SIZE) {
          ++expected_doc; // gap
          ++expected_value;
        }

        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    {
      // iterate over column (not cached)
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      size_t count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      for (; it->next(); ) {
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ++expected_doc;
        ++expected_value;

        if (++count == BLOCK_SIZE) {
          ++expected_doc; // gap
          ++expected_value;
        }
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(irs::doc_id_t(1+MAX_DOCS), expected_value);
    }

    // read values
    {
      irs::bytes_ref actual_value;
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      // cached
      irs::doc_id_t i = 0;
      for (; i < BLOCK_SIZE; ++i) {
        const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
        ASSERT_TRUE(values(doc, actual_value));
        const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.c_str());
        ASSERT_EQ(i, *reinterpret_cast<const irs::doc_id_t*>(actual_str_value.c_str()));
      }

      ASSERT_FALSE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));

      for (++i; i < MAX_DOCS; ++i) {
        const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
        ASSERT_TRUE(values(doc, actual_value));
        const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.c_str());
        ASSERT_EQ(i, *reinterpret_cast<const irs::doc_id_t*>(actual_str_value.c_str()));
      }
    }

    // visit values (cached)
    {
      size_t count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      auto visitor = [&count, &expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_value = irs::to_string<irs::string_ref>(actual_data.c_str());
        if (expected_value != *reinterpret_cast<const irs::doc_id_t*>(actual_value.c_str())) {
          return false;
        }

        ++expected_doc;
        ++expected_value;

        if (++count == BLOCK_SIZE) {
          ++expected_doc; // gap
          ++expected_value;
        }

        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    // iterate over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      size_t count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      for (; it->next(); ) {
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ++expected_doc;
        ++expected_value;

        if (++count == BLOCK_SIZE) {
          ++expected_doc; // gap
          ++expected_value;
        }
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(irs::doc_id_t(1+MAX_DOCS), expected_value);
    }
  }

  // check inserted values:
  // - visit (not cached)
  // - seek (not cached)
  // - random read (cached)
  // - visit (cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(this->dir(), this->codec());
    ASSERT_EQ(1, reader.size());

    auto& segment = *(reader.begin());
    ASSERT_EQ(irs::doc_id_t(1+MAX_DOCS), segment.live_docs_count());

    auto* meta = segment.column(column_name);
    ASSERT_NE(nullptr, meta);

    // visit values (not cached)
    {
      size_t count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      auto visitor = [&count, &expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_value = irs::to_string<irs::string_ref>(actual_data.c_str());
        if (expected_value != *reinterpret_cast<const irs::doc_id_t*>(actual_value.c_str())) {
          return false;
        }

        ++expected_doc;
        ++expected_value;

        if (++count == BLOCK_SIZE) {
          ++expected_doc; // gap
          ++expected_value;
        }

        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    // seek over column (not cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      size_t count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      for (; expected_doc <= 1+MAX_DOCS; ) {
        if (expected_doc == 1025) {
          ASSERT_EQ(expected_doc+1, it->seek(expected_doc));
          ++expected_doc;
          ++expected_value;
        } else {
          ASSERT_EQ(expected_doc, it->seek(expected_doc));
        }
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ++expected_doc;
        ++expected_value;
      }

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(expected_doc));
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(MAX_DOCS + 1));
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(1+MAX_DOCS, expected_value);
    }

    // seek to the begin + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;

      ASSERT_EQ(expected_doc, it->seek(expected_doc));
      const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

      ++expected_doc;
      ++expected_value;

      for (; it->next(); ) {
        if (expected_doc == 1025) {
          ++expected_doc; // gap
          ++expected_value;
        }

        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ++expected_doc;
        ++expected_value;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(1+MAX_DOCS, expected_value);
    }

    // seek before the begin + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;

      ASSERT_EQ(expected_doc, it->seek(expected_doc - 1));
      const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

      ++expected_doc;
      ++expected_value;

      for (; it->next(); ) {
        if (expected_doc == 1025) {
          ++expected_doc; // gap
          ++expected_value;
        }

        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ++expected_doc;
        ++expected_value;
      }
      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(1+MAX_DOCS, expected_value);
    }

    // seek to the end + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      auto expected_doc = MAX_DOCS+1;
      auto expected_value = MAX_DOCS;

      ASSERT_EQ(expected_doc, it->seek(expected_doc));
      const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
    }

    // seek to before the end + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      auto expected_doc = MAX_DOCS;
      auto expected_value = MAX_DOCS-1;

      ASSERT_EQ(expected_doc, it->seek(expected_doc));
      auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

      ++expected_doc;
      ++expected_value;
      ASSERT_TRUE(it->next());
      actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      ASSERT_EQ(expected_doc, it->value());
      ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
    }

    // seek to after the end + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(MAX_DOCS + 2));
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      // can't seek backwards
      ASSERT_EQ(irs::doc_limits::eof(), it->seek(MAX_DOCS - 1));
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
    }

    // FIXME revisit
    // seek to gap + next(x5)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_TRUE(payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

      irs::doc_id_t expected_doc = BLOCK_SIZE+2;
      irs::doc_id_t expected_value = expected_doc-1;

      ASSERT_EQ(expected_doc, it->seek(expected_doc-1));
      ASSERT_EQ(expected_doc, it->value());
      auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

      for (; it->next(); ) {
        ++expected_doc;
        ++expected_value;

        ASSERT_EQ(expected_doc, it->value());
        auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
    }

    // seek + next(x5)
    {
      const size_t steps_forward = 5;

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;

      for (;;) {
        if (expected_doc == 1025) {
          ASSERT_EQ(expected_doc+1, it->seek(expected_doc));
          ++expected_doc; // gap
          ++expected_value;
        } else {
          if (expected_doc > MAX_DOCS+1) {
            ASSERT_EQ(irs::doc_limits::eof(), it->seek(expected_doc));
          } else {
            ASSERT_EQ(expected_doc, it->seek(expected_doc));
          }
        }

        if (irs::doc_limits::eof(it->value())) {
          break;
        }

        auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        auto next_expected_doc = expected_doc + 1;
        auto next_expected_value = expected_value + 1;
        for (auto i = 0; i < steps_forward && it->next(); ++i) {
          if (next_expected_doc == 1025) {
            ++next_expected_doc; // gap
            ++next_expected_value;
          }

          actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

          ASSERT_EQ(next_expected_doc, it->value());
          ASSERT_EQ(next_expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          // can't seek backwards
          ASSERT_EQ(next_expected_doc, it->seek(expected_doc));
          ASSERT_EQ(next_expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++next_expected_doc;
          ++next_expected_value;
        }

        expected_doc = next_expected_doc;
        expected_value = next_expected_value;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(1+MAX_DOCS, expected_value);
    }

    // seek backwards + next(x5)
    {
      const size_t steps_forward = 5;

      const irs::doc_id_t min_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_doc = MAX_DOCS;
      irs::doc_id_t expected_value = expected_doc - 1;
      size_t docs_count = 0;

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      for (; expected_doc >= min_doc && expected_doc <= MAX_DOCS;) {
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_FALSE(!payload);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

        if (expected_doc == 1025) {
          ASSERT_EQ(expected_doc+1, it->seek(expected_doc));
          expected_doc++;
          expected_value++;
        } else {
          if (expected_doc > MAX_DOCS+1) {
            ASSERT_EQ(irs::doc_limits::eof(), it->seek(expected_doc));
          } else {
            ASSERT_EQ(expected_doc, it->seek(expected_doc));
          }
        }

        auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

        ++docs_count;

        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        auto next_expected_doc = expected_doc + 1;
        auto next_expected_value = expected_value + 1;
        for (auto i = 0; i < steps_forward && it->next(); ++i) {
          if (next_expected_doc == 1025) {
            ++next_expected_doc; // gap
            ++next_expected_value;
          }

          actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

          ASSERT_EQ(next_expected_doc, it->value());
          ASSERT_EQ(next_expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++next_expected_doc;
          ++next_expected_value;
        }

        --expected_doc;
        --expected_value;

        if (expected_doc == 1025) {
          // gap
          --expected_doc;
          --expected_value;
        }
      }
      ASSERT_EQ(MAX_DOCS-1, docs_count);

      // seek before the first document
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      it->seek(expected_doc);
      expected_doc = min_doc;
      expected_value = expected_doc - 1;
      ASSERT_EQ(min_doc, it->seek(expected_doc));
      auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

      auto next_expected_doc = expected_doc + 1;
      auto next_expected_value = expected_value + 1;
      for (auto i = 0; i < steps_forward; ++i) {
        ASSERT_TRUE(it->next());
        actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

        ASSERT_EQ(next_expected_doc, it->value());
        ASSERT_EQ(next_expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ++next_expected_doc;
        ++next_expected_value;
      }
    }

    // seek backwards + next(x5)
    {
      const size_t steps_forward = 5;

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = MAX_DOCS;
      irs::doc_id_t expected_value = expected_doc - 1;

      if (expected_doc == 1025) {
        ASSERT_EQ(expected_doc+1, it->seek(expected_doc));
        expected_doc++;
        expected_value++;
      } else {
        ASSERT_EQ(expected_doc, it->seek(expected_doc));
      }
      auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

      auto next_expected_doc = expected_doc + 1;
      auto next_expected_value = expected_value + 1;
      for (auto i = 0; i < steps_forward && it->next(); ++i) {
        if (next_expected_doc == 1025) {
          next_expected_doc++; // gap
          next_expected_value++;
        }

        actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

        ASSERT_EQ(next_expected_doc, it->value());
        ASSERT_EQ(next_expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ++next_expected_doc;
        ++next_expected_value;
      }

      --expected_doc;
      ASSERT_EQ(irs::doc_limits::eof(), it->seek(expected_doc));
    }

    // read values
    {
      irs::bytes_ref actual_value;
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      // cached
      irs::doc_id_t i = 0;
      for (; i < BLOCK_SIZE; ++i) {
        const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
        ASSERT_TRUE(values(doc, actual_value));
        const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.c_str());
        ASSERT_EQ(i, *reinterpret_cast<const irs::doc_id_t*>(actual_str_value.c_str()));
      }

      ASSERT_FALSE(values(i + (irs::type_limits<irs::type_t::doc_id_t>::min)(), actual_value));

      for (++i; i < MAX_DOCS; ++i) {
        const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
        ASSERT_TRUE(values(doc, actual_value));
        const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.c_str());
        ASSERT_EQ(i, *reinterpret_cast<const irs::doc_id_t*>(actual_str_value.c_str()));
      }
    }

    // visit values (cached)
    {
      size_t count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      auto visitor = [&count, &expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_value = irs::to_string<irs::string_ref>(actual_data.c_str());
        if (expected_value != *reinterpret_cast<const irs::doc_id_t*>(actual_value.c_str())) {
          return false;
        }

        ++expected_doc;
        ++expected_value;

        if (++count == BLOCK_SIZE) {
          ++expected_doc; // gap
          ++expected_value;
        }

        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    // iterate over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      size_t count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      for (; it->next(); ) {
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ++expected_doc;
        ++expected_value;

        if (++count == BLOCK_SIZE) {
          ++expected_doc; // gap
          ++expected_value;
        }
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(irs::doc_id_t(1+MAX_DOCS), expected_value);
    }
  }
}

TEST_P(index_column_test_case, read_write_doc_attributes_sparse_column_sparse_mask) {
  // sparse_column<sparse_mask_block>
  irs::index_writer::init_options options;
  options.column_info = [](const irs::string_ref&) {
    return irs::column_info{ irs::type<irs::compression::lz4>::get(), irs::compression::options{}, true };
  };

  static const irs::doc_id_t MAX_DOCS = 1500;
  static const iresearch::string_ref column_name = "id";

  // write documents
  {
    struct stored {
      const irs::string_ref& name() { return column_name; }
      const irs::flags& features() const {
        return irs::flags::empty_instance();
      }
      bool write(irs::data_output& out) { return true; }
    } field;

    irs::doc_id_t docs_count = 0;
    auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE, options);
    auto ctx = writer->documents();

    do {
      auto doc = ctx.insert();

      if (docs_count % 2) {
        doc.insert<irs::Action::STORE>(field);
      }
    } while (++docs_count < MAX_DOCS); // insert MAX_DOCS/2 documents

    { irs::index_writer::documents_context(std::move(ctx)); } // force flush of documents()
    writer->commit();
  }

  // check inserted values:
  // - visit (not cached)
  // - random read (not cached)
  // - random read (cached)
  // - visit (cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(this->dir(), this->codec());
    ASSERT_EQ(1, reader.size());

    auto& segment = *(reader.begin());
    ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

    auto* meta = segment.column(column_name);
    ASSERT_NE(nullptr, meta);

    // check number of documents in the column
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_EQ(MAX_DOCS/2, column->size());
    }

    // visit values (not cached)
    {
      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = 2;
      auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        if (!actual_data.null()) {
          return false;
        }

        expected_doc += 2;
        ++docs_count;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_TRUE(column->visit(visitor));
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
    }

    // read values
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      irs::bytes_ref actual_value;

      // read (not cached)
      for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
        const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
        ASSERT_EQ(i % 2, values(doc, actual_value));
        ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
      }

      // read (cached)
      for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
        const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
        ASSERT_EQ(i % 2, values(doc, actual_value));
        ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
      }
    }

    // visit values (cached)
    {
      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = 2;
      auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        if (!actual_data.null()) {
          return false;
        }

        expected_doc += 2;
        ++docs_count;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
    }

    // iterate over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = 2;
      for (; it->next(); ) {
        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

        expected_doc += 2;
        ++docs_count;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
    }
  }

  // check inserted values:
  // - visit (not cached)
  // - iterate (not cached)
  // - random read (cached)
  // - visit (cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(this->dir(), this->codec());
    ASSERT_EQ(1, reader.size());

    auto& segment = *(reader.begin());
    ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

    auto* meta = segment.column(column_name);
    ASSERT_NE(nullptr, meta);

    // visit values (not cached)
    {
      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = 2;
      auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        if (!actual_data.null()) {
          return false;
        }

        expected_doc += 2;
        ++docs_count;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
    }

    // iterate over column (not cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = 2;
      for (; it->next(); ) {
        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

        expected_doc += 2;
        ++docs_count;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
    }

    // read values
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      irs::bytes_ref actual_value;

      // read (cached)
      for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
        const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
        ASSERT_EQ(i % 2, values(doc, actual_value));
        ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
      }
    }

    // visit values (cached)
    {
      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = 2;
      auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        if (!actual_data.null()) {
          return false;
        }

        expected_doc += 2;
        ++docs_count;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
    }

    // iterate over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = 2;
      for (; it->next(); ) {
        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

        expected_doc += 2;
        ++docs_count;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
    }
  }

  // check inserted values:
  // - visit (not cached)
  // - seek (not cached)
  // - random read (cached)
  // - visit (cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(this->dir(), this->codec());
    ASSERT_EQ(1, reader.size());

    auto& segment = *(reader.begin());
    ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

    auto* meta = segment.column(column_name);
    ASSERT_NE(nullptr, meta);

    // visit values (not cached)
    {
      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = 2;
      auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        if (!actual_data.null()) {
          return false;
        }

        expected_doc += 2;
        ++docs_count;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
    }

    // seek over column (not cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = 2;
      for (; expected_doc <= MAX_DOCS; ) {
        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

        expected_doc += 2;
        ++docs_count;
      }

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(expected_doc));
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(MAX_DOCS + 1));
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
    }

    // seek over column (not cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = 2;
      for (; expected_doc <= MAX_DOCS; ) {
        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

        ASSERT_EQ(expected_doc, it->seek(expected_doc - 1)); // seek before the existing key (value should remain the same)
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

        expected_doc += 2;
        ++docs_count;
      }

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(expected_doc));
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(MAX_DOCS + 1));
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
    }

    // seek over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = 2;
      size_t docs_count = 0;
      for (; expected_doc <= MAX_DOCS; ) {
        ASSERT_EQ(expected_doc, it->seek(expected_doc - 1));
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

        ASSERT_EQ(expected_doc, it->seek(expected_doc)); // seek to the existing key (value should remain the same)
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

        expected_doc += 2;
        ++docs_count;
      }

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(expected_doc));
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(MAX_DOCS + 1));
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
    }

    // seek to the begin + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = 2;
      size_t docs_count = 0;

      ASSERT_EQ(expected_doc, it->seek(expected_doc));
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      expected_doc += 2;
      ++docs_count;

      for (; it->next(); ) {
        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

        expected_doc += 2;
        ++docs_count;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
    }

    // seek before the begin + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = 2;
      size_t docs_count = 0;

      ASSERT_EQ(expected_doc, it->seek(expected_doc - 1));
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      expected_doc += 2;
      ++docs_count;

      for (; it->next(); ) {
        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

        expected_doc += 2;
        ++docs_count;
      }
      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
    }

    // seek to the end + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_EQ(MAX_DOCS, it->seek(MAX_DOCS));
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
    }

    // seek to before the end + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_EQ(MAX_DOCS, it->seek(MAX_DOCS - 1));
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
    }

    // seek to after the end + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(MAX_DOCS + 1));
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      // can't seek backwards
      ASSERT_EQ(irs::doc_limits::eof(), it->seek(MAX_DOCS - 1));
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
    }

    // seek + next(x5)
    {
      const size_t steps_forward = 5;

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = 2;
      size_t docs_count = 0;

      for (;;) {
        it->seek(expected_doc);

        if (irs::doc_limits::eof(it->value())) {
          break;
        }

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

        ++docs_count;

        auto next_expected_doc = expected_doc + 2;
        for (auto i = 0; i < steps_forward && it->next(); ++i) {
          ASSERT_EQ(next_expected_doc, it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

          // can't seek backwards
          ASSERT_EQ(next_expected_doc, it->seek(expected_doc));
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

          next_expected_doc += 2;
          ++docs_count;
        }

        expected_doc = next_expected_doc;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(MAX_DOCS/2, docs_count);
    }

    // seek backwards + next(x5)
    {
      const size_t steps_forward = 5;

      const irs::doc_id_t min_doc = 2;
      irs::doc_id_t expected_doc = MAX_DOCS;
      size_t docs_count = 0;

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);

      for (; expected_doc >= min_doc && expected_doc <= MAX_DOCS;) {
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_FALSE(!payload);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

        ++docs_count;

        auto next_expected_doc = expected_doc + 2;
        for (auto i = 0; i < steps_forward && it->next(); ++i) {
          ASSERT_EQ(next_expected_doc, it->value());
          ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

          next_expected_doc += 2;
        }

        expected_doc -= 2;
      }
      ASSERT_EQ(MAX_DOCS/2, docs_count);

      // seek before the first document
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      it->seek(expected_doc);
      expected_doc = min_doc;
      ASSERT_EQ(min_doc, it->seek(expected_doc));
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      auto next_expected_doc = expected_doc + 2;
      for (auto i = 0; i < steps_forward; ++i) {
        ASSERT_TRUE(it->next());
        ASSERT_EQ(next_expected_doc, it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

        next_expected_doc += 2;
      }
    }

    // seek backwards + next(x5)
    {
      const size_t steps_forward = 5;

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = MAX_DOCS;

      ASSERT_EQ(expected_doc, it->seek(expected_doc));
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      auto next_expected_doc = expected_doc + 2;
      for (auto i = 0; i < steps_forward && it->next(); ++i) {
        ASSERT_EQ(next_expected_doc, it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

        next_expected_doc += 2;
      }

      expected_doc -= 2;
      ASSERT_EQ(irs::doc_limits::eof(), it->seek(expected_doc));
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
    }

    // seek over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = 2;
      size_t docs_count = 0;
      for (; expected_doc <= MAX_DOCS; ) {
        ASSERT_EQ(expected_doc, it->seek(expected_doc - 1));
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

        expected_doc += 2;
        ++docs_count;
      }

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(expected_doc));
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(MAX_DOCS + 1));
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
    }

    // read values
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      irs::bytes_ref actual_value;

      // read (cached)
      for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
        const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
        ASSERT_EQ(i % 2, values(doc, actual_value));
        ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
      }
    }

    // visit values (cached)
    {
      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = 2;
      auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        if (!actual_data.null()) {
          return false;
        }

        expected_doc += 2;
        ++docs_count;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
    }

    // iterate over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = 2;
      for (; it->next(); ) {
        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

        expected_doc += 2;
        ++docs_count;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS/2), docs_count);
    }
  }
}

TEST_P(index_column_test_case, read_write_doc_attributes_dense_column_dense_mask) {
  // dense_fixed_length_column<dense_mask_block>

  irs::index_writer::init_options options;
  options.column_info = [](const irs::string_ref&) {
    return irs::column_info{ irs::type<irs::compression::lz4>::get(), irs::compression::options{}, true };
  };

  static const irs::doc_id_t MAX_DOCS
    = 1024*1024 // full index block
    + 2051;     // tail index block
  static const iresearch::string_ref column_name = "id";

  // write documents
  {
    struct stored {
      const irs::string_ref& name() { return column_name; }
      const irs::flags& features() const {
        return irs::flags::empty_instance();
      }
      bool write(irs::data_output& out) { return true; }
    } field;

    irs::doc_id_t docs_count = 0;
    auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE, options);
    auto ctx = writer->documents();

    do {
      ctx.insert().insert<irs::Action::STORE>(field);
    } while (++docs_count < MAX_DOCS); // insert MAX_DOCS documents

    { irs::index_writer::documents_context(std::move(ctx)); } // force flush of documents()
    writer->commit();
  }

  // check inserted values:
  // - visit (not cached)
  // - random read (not cached)
  // - random read (cached)
  // - visit (cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(this->dir(), this->codec());
    ASSERT_EQ(1, reader.size());

    auto& segment = *(reader.begin());
    ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

    auto* meta = segment.column(column_name);
    ASSERT_NE(nullptr, meta);

    // check number of documents in the column
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_EQ(MAX_DOCS, column->size());
    }

    // visit values (not cached)
    {
      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        if (!actual_data.null()) {
          return false;
        }

        ++expected_doc;
        ++docs_count;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
    }

    // read values
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      irs::bytes_ref actual_value;

      // not cached
      for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
        const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
        ASSERT_TRUE(values(doc, actual_value));
        ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
      }

      // cached
      for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
        const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
        ASSERT_TRUE(values(doc, actual_value));
        ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
      }
    }

    // visit values (cached)
    {
      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        if (!actual_data.null()) {
          return false;
        }

        ++expected_doc;
        ++docs_count;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
    }

    // iterate over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      ASSERT_TRUE(!irs::get<irs::payload>(*it)); // dense_mask does not have a payload
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      for (; it->next(); ) {
        ASSERT_EQ(expected_doc, it->value());
        ++expected_doc;
        ++docs_count;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
    }
  }

  // check inserted values:
  // - visit (not cached)
  // - iterate (not cached)
  // - random read (cached)
  // - visit (cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(this->dir(), this->codec());
    ASSERT_EQ(1, reader.size());

    auto& segment = *(reader.begin());
    ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

    auto* meta = segment.column(column_name);
    ASSERT_NE(nullptr, meta);

    // visit values (not cached)
    {
      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        if (!actual_data.null()) {
          return false;
        }

        ++expected_doc;
        ++docs_count;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
    }

    {
      // iterate over column (not cached)
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      ASSERT_TRUE(!irs::get<irs::payload>(*it)); // dense_mask does not have a payload
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      for (; it->next(); ) {
        ASSERT_EQ(expected_doc, it->value());
        ++expected_doc;
        ++docs_count;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
    }

    // read values
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      irs::bytes_ref actual_value;

      // cached
      for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
        const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
        ASSERT_TRUE(values(doc, actual_value));
        ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
      }
    }

    // visit values (cached)
    {
      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        if (!actual_data.null()) {
          return false;
        }

        ++expected_doc;
        ++docs_count;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
    }

    // iterate over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      ASSERT_TRUE(!irs::get<irs::payload>(*it)); // dense_mask does not have a payload
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      for (; it->next(); ) {
        ASSERT_EQ(expected_doc, it->value());
        ++expected_doc;
        ++docs_count;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
    }
  }

  // check inserted values:
  // - visit (not cached)
  // - seek (not cached)
  // - random read (cached)
  // - visit (cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(this->dir(), this->codec());
    ASSERT_EQ(1, reader.size());

    auto& segment = *(reader.begin());
    ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

    auto* meta = segment.column(column_name);
    ASSERT_NE(nullptr, meta);

    // visit values (not cached)
    {
      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        if (!actual_data.null()) {
          return false;
        }

        ++expected_doc;
        ++docs_count;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
    }

    // seek over column (not cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      ASSERT_TRUE(!irs::get<irs::payload>(*it)); // dense_mask does not have a payload
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      for (; expected_doc <= MAX_DOCS; ) {
        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        ++expected_doc;
        ++docs_count;
      }

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(expected_doc));

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(MAX_DOCS + 1));

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(MAX_DOCS, docs_count);
    }

    // seek to the begin + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      ASSERT_TRUE(!irs::get<irs::payload>(*it)); // dense_mask does not have a payload
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      size_t docs_count = 0;

      ASSERT_EQ(expected_doc, it->seek(expected_doc));
      ++expected_doc;
      ++docs_count;

      for (; it->next(); ) {
        ASSERT_EQ(expected_doc, it->value());
        ++expected_doc;
        ++docs_count;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(MAX_DOCS, docs_count);
    }

    // seek before the begin + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      ASSERT_TRUE(!irs::get<irs::payload>(*it)); // dense_mask does not have a payload
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      size_t docs_count = 0;

      ASSERT_EQ(expected_doc, it->seek(expected_doc - 1));
      ++expected_doc;
      ++docs_count;

      for (; it->next(); ) {
        ASSERT_EQ(expected_doc, it->value());
        ++expected_doc;
        ++docs_count;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(MAX_DOCS, docs_count);
    }

    // seek to the end + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      ASSERT_TRUE(!irs::get<irs::payload>(*it)); // dense_mask does not have a payload
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

      ASSERT_EQ(MAX_DOCS, it->seek(MAX_DOCS));

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
    }

    // seek to before the end + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      ASSERT_TRUE(!irs::get<irs::payload>(*it)); // dense_mask does not have a payload
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

      ASSERT_EQ(MAX_DOCS - 1, it->seek(MAX_DOCS - 1));

      ASSERT_TRUE(it->next());
      ASSERT_EQ(MAX_DOCS, it->value());

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
    }

    // seek to after the end + next + seek before end
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      ASSERT_TRUE(!irs::get<irs::payload>(*it)); // dense_mask does not have a payload
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

      it->seek(MAX_DOCS+1);
      ASSERT_EQ(irs::doc_limits::eof(), it->value());

      // can't seek backwards
      ASSERT_EQ(irs::doc_limits::eof(), it->seek(MAX_DOCS - 1));

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
    }

    // seek + next(x5)
    {
      const size_t steps_forward = 5;

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      ASSERT_TRUE(!irs::get<irs::payload>(*it)); // dense_mask does not have a payload
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      size_t docs_count = 0;

      for (;;) {
        it->seek(expected_doc);

        if (irs::doc_limits::eof(it->value())) {
          break;
        }

        ++docs_count;
        ASSERT_EQ(expected_doc, it->value());

        auto next_expected_doc = expected_doc + 1;
        for (auto i = 0; i < steps_forward && it->next(); ++i) {
          ASSERT_EQ(next_expected_doc, it->value());

          // can't seek backwards
          ASSERT_EQ(next_expected_doc, it->seek(expected_doc));

          ++next_expected_doc;
          ++docs_count;
        }

        expected_doc = next_expected_doc;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(MAX_DOCS, docs_count);
    }

    // seek backwards + next(x5)
    {
      const size_t steps_forward = 5;

      const irs::doc_id_t min_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_doc = MAX_DOCS;
      size_t docs_count = 0;

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      for (; expected_doc >= min_doc && expected_doc <= MAX_DOCS;) {
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        ASSERT_TRUE(!irs::get<irs::payload>(*it)); // dense_mask does not have a payload
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

        ++docs_count;
        ASSERT_EQ(expected_doc, it->seek(expected_doc));

        auto next_expected_doc = expected_doc + 1;
        for (auto i = 0; i < steps_forward && it->next(); ++i) {
          ASSERT_EQ(next_expected_doc, it->value());
          ++next_expected_doc;
        }

        --expected_doc;
      }
      ASSERT_EQ(MAX_DOCS, docs_count);

      // seek before the first document
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      ASSERT_TRUE(!irs::get<irs::payload>(*it)); // dense_mask does not have a payload
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

      it->seek(expected_doc);
      expected_doc = min_doc;
      ASSERT_EQ(min_doc, it->seek(expected_doc));

      auto next_expected_doc = expected_doc + 1;
      for (auto i = 0; i < steps_forward; ++i) {
        ASSERT_TRUE(it->next());
        ASSERT_EQ(next_expected_doc, it->value());
        ++next_expected_doc;
      }
    }

    // seek backwards + next(x5)
    {
      const size_t steps_forward = 5;

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      ASSERT_TRUE(!irs::get<irs::payload>(*it)); // dense_mask does not have a payload
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

      irs::doc_id_t expected_doc = MAX_DOCS;

      ASSERT_EQ(expected_doc, it->seek(expected_doc));

      auto next_expected_doc = expected_doc + 1;
      for (auto i = 0; i < steps_forward && it->next(); ++i) {
        ASSERT_EQ(next_expected_doc, it->value());
        ++next_expected_doc;
      }

      --expected_doc;
      it->seek(expected_doc);
    }

    // read values
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto values = column->values();
      irs::bytes_ref actual_value;

      // cached
      for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
        const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
        ASSERT_TRUE(values(doc, actual_value));
        ASSERT_EQ(irs::bytes_ref::NIL, actual_value);
      }
    }

    // visit values (cached)
    {
      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      auto visitor = [&docs_count, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        if (!actual_data.null()) {
          return false;
        }

        ++expected_doc;
        ++docs_count;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
    }

    // iterate over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      ASSERT_TRUE(!irs::get<irs::payload>(*it)); // dense_mask does not have a payload
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());

      irs::doc_id_t docs_count = 0;
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      for (; it->next(); ) {
        ASSERT_EQ(expected_doc, it->value());
        ++expected_doc;
        ++docs_count;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), docs_count);
    }
  }
}

TEST_P(index_column_test_case, read_write_doc_attributes_dense_column_dense_fixed_length) {
  // dense_fixed_length_column<dense_fixed_length_block>
  irs::index_writer::init_options options;
  options.column_info = [](const irs::string_ref&) {
    return irs::column_info{ irs::type<irs::compression::lz4>::get(), irs::compression::options{}, true };
  };

  static const irs::doc_id_t MAX_DOCS = 1500;
  static const iresearch::string_ref column_name = "id";

  // write documents
  {
    struct stored {
      const irs::string_ref& name() { return column_name; }

      const irs::flags& features() const {
        return irs::flags::empty_instance();
      }

      bool write(irs::data_output& out) {
        irs::write_string(
          out,
          irs::numeric_utils::numeric_traits<uint64_t>::raw_ref(value)
        );
        return true;
      }

      uint64_t value{};
    } field;

    auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE, options);
    auto ctx = writer->documents();

    do {
      ctx.insert().insert<irs::Action::STORE>(field);
    } while (++field.value < MAX_DOCS); // insert MAX_DOCS documents

    { irs::index_writer::documents_context(std::move(ctx)); } // force flush of documents()
    writer->commit();
  }

  // check inserted values:
  // - visit (not cached)
  // - random read (not cached)
  // - random read (cached)
  // - visit (cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(this->dir(), this->codec());
    ASSERT_EQ(1, reader.size());

    auto& segment = *(reader.begin());
    ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

    auto* meta = segment.column(column_name);
    ASSERT_NE(nullptr, meta);

    // check number of documents in the column
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_EQ(MAX_DOCS, column->size());
    }

    // visit values (not cached)
    {
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_value = irs::to_string<irs::string_ref>(actual_data.c_str());
        if (expected_value != *reinterpret_cast<const irs::doc_id_t*>(actual_value.c_str())) {
          return false;
        }

        ++expected_doc;
        ++expected_value;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    // read values
    {
      irs::bytes_ref actual_value;
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      // not cached
      for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
        const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
        ASSERT_TRUE(values(doc, actual_value));
        const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.c_str());
        ASSERT_EQ(i, *reinterpret_cast<const irs::doc_id_t*>(actual_str_value.c_str()));
      }

      // cached
      for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
        const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
        ASSERT_TRUE(values(doc, actual_value));
        const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.c_str());
        ASSERT_EQ(i, *reinterpret_cast<const irs::doc_id_t*>(actual_str_value.c_str()));
      }
    }

    // visit values (cached)
    {
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_value = irs::to_string<irs::string_ref>(actual_data.c_str());
        if (expected_value != *reinterpret_cast<const irs::doc_id_t*>(actual_value.c_str())) {
          return false;
        }

        ++expected_doc;
        ++expected_value;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    // iterate over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      for (; it->next(); ) {
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ++expected_doc;
        ++expected_value;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), expected_value);
    }
  }

  // check inserted values:
  // - visit (not cached)
  // - iterate (not cached)
  // - random read (cached)
  // - visit (cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(this->dir(), this->codec());
    ASSERT_EQ(1, reader.size());

    auto& segment = *(reader.begin());
    ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

    auto* meta = segment.column(column_name);
    ASSERT_NE(nullptr, meta);

    // visit values (not cached)
    {
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_value = irs::to_string<irs::string_ref>(actual_data.c_str());
        if (expected_value != *reinterpret_cast<const irs::doc_id_t*>(actual_value.c_str())) {
          return false;
        }

        ++expected_doc;
        ++expected_value;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    {
      // iterate over column (not cached)
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      for (; it->next(); ) {
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ++expected_doc;
        ++expected_value;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), expected_value);
    }

    // read values
    {
      irs::bytes_ref actual_value;
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      // cached
      for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
        const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
        ASSERT_TRUE(values(doc, actual_value));
        const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.c_str());
        ASSERT_EQ(i, *reinterpret_cast<const irs::doc_id_t*>(actual_str_value.c_str()));
      }
    }

    // visit values (cached)
    {
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_value = irs::to_string<irs::string_ref>(actual_data.c_str());
        if (expected_value != *reinterpret_cast<const irs::doc_id_t*>(actual_value.c_str())) {
          return false;
        }

        ++expected_doc;
        ++expected_value;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    // iterate over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      for (; it->next(); ) {
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ++expected_doc;
        ++expected_value;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), expected_value);
    }
  }

  // check inserted values:
  // - visit (not cached)
  // - seek (not cached)
  // - random read (cached)
  // - visit (cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(this->dir(), this->codec());
    ASSERT_EQ(1, reader.size());

    auto& segment = *(reader.begin());
    ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

    auto* meta = segment.column(column_name);
    ASSERT_NE(nullptr, meta);

    // visit values (not cached)
    {
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_value = irs::to_string<irs::string_ref>(actual_data.c_str());
        if (expected_value != *reinterpret_cast<const irs::doc_id_t*>(actual_value.c_str())) {
          return false;
        }

        ++expected_doc;
        ++expected_value;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    // seek over column (not cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      for (; expected_doc <= MAX_DOCS; ) {
        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ++expected_doc;
        ++expected_value;
      }

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(expected_doc));
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(MAX_DOCS + 1));
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(MAX_DOCS, expected_value);
    }

    // seek to the begin + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;

      ASSERT_EQ(expected_doc, it->seek(expected_doc));
      const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

      ++expected_doc;
      ++expected_value;

      for (; it->next(); ) {
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ++expected_doc;
        ++expected_value;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(MAX_DOCS, expected_value);
    }

    // seek before the begin + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;

      ASSERT_EQ(expected_doc, it->seek(expected_doc - 1));
      const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

      ++expected_doc;
      ++expected_value;

      for (; it->next(); ) {
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ++expected_doc;
        ++expected_value;
      }
      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(MAX_DOCS, expected_value);
    }

    // seek to the end + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      auto expected_doc = MAX_DOCS;
      auto expected_value = MAX_DOCS-1;

      ASSERT_EQ(expected_doc, it->seek(expected_doc));
      const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
    }

    // seek to before the end + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      auto expected_doc = MAX_DOCS-1;
      auto expected_value = expected_doc-1;

      ASSERT_EQ(expected_doc, it->seek(expected_doc));
      auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

      ++expected_doc;
      ++expected_value;
      ASSERT_TRUE(it->next());
      actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      ASSERT_EQ(expected_doc, it->value());
      ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
    }

    // seek to after the end + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(MAX_DOCS + 1));
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      // can't seek backwards
      ASSERT_EQ(irs::doc_limits::eof(), it->seek(MAX_DOCS - 1));
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
    }

    // seek + next(x5)
    {
      const size_t steps_forward = 5;

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;

      for (;;) {
        it->seek(expected_doc);

        if (irs::doc_limits::eof(it->value())) {
          break;
        }

        auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        auto next_expected_doc = expected_doc + 1;
        auto next_expected_value = expected_value + 1;
        for (auto i = 0; i < steps_forward && it->next(); ++i) {
          actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

          ASSERT_EQ(next_expected_doc, it->value());
          ASSERT_EQ(next_expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          // can't seek backwards
          ASSERT_EQ(next_expected_doc, it->seek(expected_doc));
          ASSERT_EQ(next_expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++next_expected_doc;
          ++next_expected_value;
        }

        expected_doc = next_expected_doc;
        expected_value = next_expected_value;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(MAX_DOCS, expected_value);
    }

    // seek backwards + next(x5)
    {
      const size_t steps_forward = 5;

      const irs::doc_id_t min_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_doc = MAX_DOCS;
      irs::doc_id_t expected_value = expected_doc - 1;
      size_t docs_count = 0;

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      for (; expected_doc >= min_doc && expected_doc <= MAX_DOCS;) {
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_FALSE(!payload);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

        ++docs_count;

        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        auto next_expected_doc = expected_doc + 1;
        auto next_expected_value = expected_value + 1;
        for (auto i = 0; i < steps_forward && it->next(); ++i) {
          actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

          ASSERT_EQ(next_expected_doc, it->value());
          ASSERT_EQ(next_expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

          ++next_expected_doc;
          ++next_expected_value;
        }

        --expected_doc;
        --expected_value;
      }
      ASSERT_EQ(MAX_DOCS, docs_count);

      // seek before the first document
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      it->seek(expected_doc);
      expected_doc = min_doc;
      expected_value = expected_doc - 1;
      ASSERT_EQ(min_doc, it->seek(expected_doc));
      auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

      auto next_expected_doc = expected_doc + 1;
      auto next_expected_value = expected_value + 1;
      for (auto i = 0; i < steps_forward; ++i) {
        ASSERT_TRUE(it->next());
        actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

        ASSERT_EQ(next_expected_doc, it->value());
        ASSERT_EQ(next_expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ++next_expected_doc;
        ++next_expected_value;
      }
    }

    // seek backwards + next(x5)
    {
      const size_t steps_forward = 5;

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = MAX_DOCS;
      irs::doc_id_t expected_value = expected_doc - 1;

      ASSERT_EQ(expected_doc, it->seek(expected_doc));
      auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

      auto next_expected_doc = expected_doc + 1;
      auto next_expected_value = expected_value + 1;
      for (auto i = 0; i < steps_forward && it->next(); ++i) {
        actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

        ASSERT_EQ(next_expected_doc, it->value());
        ASSERT_EQ(next_expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ++next_expected_doc;
        ++next_expected_value;
      }

      --expected_doc;
      ASSERT_EQ(irs::doc_limits::eof(), it->seek(expected_doc));
    }

    // read values
    {
      irs::bytes_ref actual_value;
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      // cached
      for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
        const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
        ASSERT_TRUE(values(doc, actual_value));
        const auto actual_str_value = irs::to_string<irs::string_ref>(actual_value.c_str());
        ASSERT_EQ(i, *reinterpret_cast<const irs::doc_id_t*>(actual_str_value.c_str()));
      }
    }

    // visit values (cached)
    {
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_value = irs::to_string<irs::string_ref>(actual_data.c_str());
        if (expected_value != *reinterpret_cast<const irs::doc_id_t*>(actual_value.c_str())) {
          return false;
        }

        ++expected_doc;
        ++expected_value;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    // iterate over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      for (; it->next(); ) {
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value, *reinterpret_cast<const irs::doc_id_t*>(actual_value_str.c_str()));

        ++expected_doc;
        ++expected_value;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(irs::doc_id_t(MAX_DOCS), expected_value);
    }
  }
}

TEST_P(index_column_test_case, read_write_doc_attributes_dense_column_dense_variable_length) {
  // sparse_column<dense_block>
  irs::index_writer::init_options options;
  options.column_info = [](const irs::string_ref&) {
    return irs::column_info{ irs::type<irs::compression::lz4>::get(), irs::compression::options{}, true };
  };

  static const irs::doc_id_t MAX_DOCS = 1500;
  static const iresearch::string_ref column_name = "id";

  // write documents
  {
    struct stored {
      const irs::string_ref& name() { return column_name; }

      const irs::flags& features() const {
        return irs::flags::empty_instance();
      }

      bool write(irs::data_output& out) {
        auto str = std::to_string(value);
        if (value % 2) {
          str.append(column_name.c_str(), column_name.size());
        }

        irs::write_string(out, str);
        return true;
      }

      uint64_t value{};
    } field;

    auto writer = irs::index_writer::make(this->dir(), this->codec(), irs::OM_CREATE, options);
    auto ctx = writer->documents();

    do {
      ctx.insert().insert<irs::Action::STORE>(field);
    } while (++field.value < MAX_DOCS); // insert MAX_DOCS documents

    { irs::index_writer::documents_context(std::move(ctx)); } // force flush of documents()
    writer->commit();
  }

  // check inserted values:
  // - visit (not cached)
  // - random read (not cached)
  // - random read (cached)
  // - visit (cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(this->dir(), this->codec());
    ASSERT_EQ(1, reader.size());

    auto& segment = *(reader.begin());
    ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

    auto* meta = segment.column(column_name);
    ASSERT_NE(nullptr, meta);

    // check number of documents in the column
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_EQ(MAX_DOCS, column->size());
    }

    // visit values (not cached)
    {
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

        auto expected_str  = std::to_string(expected_value);
        if (expected_value % 2) {
          expected_str.append(column_name.c_str(), column_name.size());
        }

        if (expected_str != actual_str) {
          return false;
        }

        ++expected_doc;
        ++expected_value;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    // read values
    {
      irs::bytes_ref actual_value;
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      // not cached
      for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
        const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
        ASSERT_TRUE(values(doc, actual_value));
        const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
        auto expected_str_value = std::to_string(i);
        if (i % 2) {
          expected_str_value.append(column_name.c_str(), column_name.size());
        }
        ASSERT_EQ(expected_str_value, actual_str_value);
      }

      // cached
      for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
        const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
        ASSERT_TRUE(values(doc, actual_value));
        const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
        auto expected_str_value = std::to_string(i);
        if (i % 2) {
          expected_str_value.append(column_name.c_str(), column_name.size());
        }
        ASSERT_EQ(expected_str_value, actual_str_value);
      }
    }

    // visit values (cached)
    {
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

        auto expected_str  = std::to_string(expected_value);
        if (expected_value % 2) {
          expected_str.append(column_name.c_str(), column_name.size());
        }

        if (expected_str != actual_str) {
          return false;
        }

        ++expected_doc;
        ++expected_value;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    // iterate over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      for (; it->next(); ) {
        const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto expected_value_str = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value_str, actual_str_value);

        ++expected_doc;
        ++expected_value;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(MAX_DOCS, expected_value);
    }
  }

  // check inserted values:
  // - visit (not cached)
  // - iterate (not cached)
  // - random read (cached)
  // - visit (cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(this->dir(), this->codec());
    ASSERT_EQ(1, reader.size());

    auto& segment = *(reader.begin());
    ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

    auto* meta = segment.column(column_name);
    ASSERT_NE(nullptr, meta);

    // visit values (not cached)
    {
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

        auto expected_str  = std::to_string(expected_value);
        if (expected_value % 2) {
          expected_str.append(column_name.c_str(), column_name.size());
        }

        if (expected_str != actual_str) {
          return false;
        }

        ++expected_doc;
        ++expected_value;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    {
      // iterate over column (not cached)
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      for (; it->next(); ) {
        const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto expected_value_str = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value_str, actual_str_value);

        ++expected_doc;
        ++expected_value;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(MAX_DOCS, expected_value);
    }

    // read values
    {
      irs::bytes_ref actual_value;
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      // cached
      for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
        const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
        ASSERT_TRUE(values(doc, actual_value));
        const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
        auto expected_str_value = std::to_string(i);
        if (i % 2) {
          expected_str_value.append(column_name.c_str(), column_name.size());
        }
        ASSERT_EQ(expected_str_value, actual_str_value);
      }
    }

    // visit values (cached)
    {
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

        auto expected_str  = std::to_string(expected_value);
        if (expected_value % 2) {
          expected_str.append(column_name.c_str(), column_name.size());
        }

        if (expected_str != actual_str) {
          return false;
        }

        ++expected_doc;
        ++expected_value;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    // iterate over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      for (; it->next(); ) {
        const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto expected_value_str = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value_str, actual_str_value);

        ++expected_doc;
        ++expected_value;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(MAX_DOCS, expected_value);
    }
  }

  // check inserted values:
  // - visit (not cached)
  // - seek (not cached)
  // - random read (cached)
  // - visit (cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(this->dir(), this->codec());
    ASSERT_EQ(1, reader.size());

    auto& segment = *(reader.begin());
    ASSERT_EQ(irs::doc_id_t(MAX_DOCS), segment.live_docs_count());

    auto* meta = segment.column(column_name);
    ASSERT_NE(nullptr, meta);

    // visit values (not cached)
    {
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

        auto expected_str  = std::to_string(expected_value);
        if (expected_value % 2) {
          expected_str.append(column_name.c_str(), column_name.size());
        }

        if (expected_str != actual_str) {
          return false;
        }

        ++expected_doc;
        ++expected_value;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    // seek over column (not cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      for (; expected_doc <= MAX_DOCS; ) {
        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto expected_value_str = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_value_str, actual_str_value);

        ++expected_doc;
        ++expected_value;
      }

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(expected_doc));
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(MAX_DOCS + 1));
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(MAX_DOCS, expected_value);
    }

    // seek to the begin + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;

      ASSERT_EQ(expected_doc, it->seek(expected_doc));
      const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      auto expected_value_str = std::to_string(expected_value);

      if (expected_value % 2) {
        expected_value_str.append(column_name.c_str(), column_name.size());
      }

      ASSERT_EQ(expected_value_str, actual_value_str);

      ++expected_doc;
      ++expected_value;

      for (; it->next(); ) {
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto expected_value_str = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value_str, actual_value_str);

        ++expected_doc;
        ++expected_value;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(MAX_DOCS, expected_value);
    }

    // seek before the begin + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;

      ASSERT_EQ(expected_doc, it->seek(expected_doc - 1));
      const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      auto expected_value_str = std::to_string(expected_value);

      if (expected_value % 2) {
        expected_value_str.append(column_name.c_str(), column_name.size());
      }

      ASSERT_EQ(expected_value_str, actual_value_str);

      ++expected_doc;
      ++expected_value;

      for (; it->next(); ) {
        const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto expected_value_str = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value_str, actual_value_str);

        ++expected_doc;
        ++expected_value;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(MAX_DOCS, expected_value);
    }

    // seek to the end + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      auto expected_doc = MAX_DOCS;
      auto expected_value = MAX_DOCS-1;

      ASSERT_EQ(expected_doc, it->seek(expected_doc));
      const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      auto expected_value_str  = std::to_string(expected_value);

      if (expected_value % 2) {
        expected_value_str.append(column_name.c_str(), column_name.size());
      }

      ASSERT_EQ(expected_value_str, actual_value_str);

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
    }

    // seek to before the end + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      auto expected_doc = MAX_DOCS-1;
      auto expected_value = expected_doc-1;

      ASSERT_EQ(expected_doc, it->seek(expected_doc));
      auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      auto expected_value_str  = std::to_string(expected_value);

      if (expected_value % 2) {
        expected_value_str.append(column_name.c_str(), column_name.size());
      }

      ASSERT_EQ(expected_value_str, actual_value_str);

      ++expected_doc;
      ++expected_value;
      expected_value_str  = std::to_string(expected_value);
      if (expected_value % 2) {
        expected_value_str.append(column_name.c_str(), column_name.size());
      }

      ASSERT_TRUE(it->next());
      ASSERT_EQ(expected_doc, it->value());
      ASSERT_EQ(expected_value_str, irs::to_string<irs::string_ref>(payload->value.c_str()));

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
    }

    // seek to after the end + next
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_EQ(irs::doc_limits::eof(), it->seek(MAX_DOCS + 1));
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      // can't seek backwards
      ASSERT_EQ(irs::doc_limits::eof(), it->seek(MAX_DOCS - 1));
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
    }

    // seek + next(x5)
    {
      const size_t steps_forward = 5;

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;

      for (;;) {
        it->seek(expected_doc);

        if (irs::doc_limits::eof(it->value())) {
          break;
        }

        auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto expected_value_str  = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value_str, actual_value_str);

        auto next_expected_doc = expected_doc + 1;
        auto next_expected_value = expected_value + 1;
        for (auto i = 0; i < steps_forward && it->next(); ++i) {
          actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
          auto next_expected_value_str  = std::to_string(next_expected_value);

          if (next_expected_value % 2) {
            next_expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(next_expected_doc, it->value());
          ASSERT_EQ(next_expected_value_str, actual_value_str);

          // can't seek backwards
          ASSERT_EQ(next_expected_doc, it->seek(expected_doc));
          ASSERT_EQ(next_expected_value_str, actual_value_str);

          ++next_expected_doc;
          ++next_expected_value;
        }

        expected_doc = next_expected_doc;
        expected_value = next_expected_value;
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(MAX_DOCS, expected_value);
    }

    // seek backwards + next(x5)
    {
      const size_t steps_forward = 5;

      const irs::doc_id_t min_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_doc = MAX_DOCS;
      irs::doc_id_t expected_value = expected_doc - 1;
      size_t docs_count = 0;

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      for (; expected_doc >= min_doc && expected_doc <= MAX_DOCS;) {
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_FALSE(!payload);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto expected_value_str  = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ++docs_count;

        ASSERT_EQ(expected_value_str, actual_value_str);

        auto next_expected_doc = expected_doc + 1;
        auto next_expected_value = expected_value + 1;
        for (auto i = 0; i < steps_forward && it->next(); ++i) {
          actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
          auto next_expected_value_str  = std::to_string(next_expected_value);

          if (next_expected_value % 2) {
            next_expected_value_str.append(column_name.c_str(), column_name.size());
          }

          ASSERT_EQ(next_expected_doc, it->value());
          ASSERT_EQ(next_expected_value_str, actual_value_str);

          ++next_expected_doc;
          ++next_expected_value;
        }

        --expected_doc;
        --expected_value;
      }
      ASSERT_EQ(MAX_DOCS, docs_count);

      // seek before the first document
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      it->seek(expected_doc);
      expected_doc = min_doc;
      expected_value = expected_doc - 1;
      ASSERT_EQ(min_doc, it->seek(expected_doc));
      auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      auto expected_value_str  = std::to_string(expected_value);

      if (expected_value % 2) {
        expected_value_str.append(column_name.c_str(), column_name.size());
      }

      ASSERT_EQ(expected_value_str, actual_value_str);

      auto next_expected_doc = expected_doc + 1;
      auto next_expected_value = expected_value + 1;
      for (auto i = 0; i < steps_forward; ++i) {
        ASSERT_TRUE(it->next());
        actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

        auto next_expected_value_str  = std::to_string(next_expected_value);
        if (next_expected_value % 2) {
          next_expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(next_expected_doc, it->value());
        ASSERT_EQ(next_expected_value_str, actual_value_str);

        ++next_expected_doc;
        ++next_expected_value;
      }
    }

    // seek backwards + next(x5)
    {
      const size_t steps_forward = 5;

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = MAX_DOCS;
      irs::doc_id_t expected_value = expected_doc - 1;

      ASSERT_EQ(expected_doc, it->seek(expected_doc));
      auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
      auto expected_value_str  = std::to_string(expected_value);

      if (expected_value % 2) {
        expected_value_str.append(column_name.c_str(), column_name.size());
      }

      ASSERT_EQ(expected_value_str, actual_value_str);

      auto next_expected_doc = expected_doc + 1;
      auto next_expected_value = expected_value + 1;
      for (auto i = 0; i < steps_forward && it->next(); ++i) {
        actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto next_expected_value_str  = std::to_string(next_expected_value);

        if (next_expected_value % 2) {
          next_expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(next_expected_doc, it->value());
        ASSERT_EQ(next_expected_value_str, actual_value_str);

        ++next_expected_doc;
        ++next_expected_value;
      }

      --expected_doc;
      ASSERT_EQ(irs::doc_limits::eof(), it->seek(expected_doc));
    }

    // read values
    {
      irs::bytes_ref actual_value;
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto values = column->values();

      // cached
      for (irs::doc_id_t i = 0; i < MAX_DOCS; ++i) {
        const irs::doc_id_t doc = i + (irs::type_limits<irs::type_t::doc_id_t>::min)();
        ASSERT_TRUE(values(doc, actual_value));
        const auto actual_str_value = irs::to_string<std::string>(actual_value.c_str());
        auto expected_str_value = std::to_string(i);
        if (i % 2) {
          expected_str_value.append(column_name.c_str(), column_name.size());
        }
        ASSERT_EQ(expected_str_value, actual_str_value);
      }
    }

    // visit values (cached)
    {
      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      auto visitor = [&expected_value, &expected_doc](irs::doc_id_t actual_doc, const irs::bytes_ref& actual_data) {
        if (expected_doc != actual_doc) {
          return false;
        }

        const auto actual_str = irs::to_string<irs::string_ref>(actual_data.c_str());

        auto expected_str  = std::to_string(expected_value);
        if (expected_value % 2) {
          expected_str.append(column_name.c_str(), column_name.size());
        }

        if (expected_str != actual_str) {
          return false;
        }

        ++expected_doc;
        ++expected_value;
        return true;
      };

      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(column, segment.column_reader(meta->id));
      ASSERT_TRUE(column->visit(visitor));
    }

    // iterate over column (cached)
    {
      auto column = segment.column_reader(column_name);
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      irs::doc_id_t expected_doc = (irs::type_limits<irs::type_t::doc_id_t>::min)();
      irs::doc_id_t expected_value = 0;
      for (; it->next(); ) {
        const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value.c_str());
        auto expected_value_str = std::to_string(expected_value);

        if (expected_value % 2) {
          expected_value_str.append(column_name.c_str(), column_name.size());
        }

        ASSERT_EQ(expected_doc, it->value());
        ASSERT_EQ(expected_value_str, actual_str_value);

        ++expected_doc;
        ++expected_value;
      }
      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(MAX_DOCS, expected_value);
    }
  }
}

TEST_P(index_column_test_case, read_write_doc_attributes_big) {
  irs::index_writer::init_options options;
  options.column_info = [](const irs::string_ref&) {
    return irs::column_info{ irs::type<irs::compression::lz4>::get(), irs::compression::options{}, true };
  };

  struct csv_doc_template_t: public tests::csv_doc_generator::doc_template {
    virtual void init() {
      clear();
      reserve(2);
      insert(std::make_shared<tests::templates::string_field>("id"));
      insert(std::make_shared<tests::templates::string_field>("label"));
    }

    virtual void value(size_t idx, const irs::string_ref& value) {
      switch(idx) {
       case 0:
        indexed.get<tests::templates::string_field>("id")->value(value);
        break;
       case 1:
        indexed.get<tests::templates::string_field>("label")->value(value);
      }
    }
  };

  csv_doc_template_t csv_doc_template;
  tests::csv_doc_generator gen(resource("simple_two_column.csv"), csv_doc_template);
  size_t docs_count = 0;

  // write attributes 
  {
    auto writer = irs::index_writer::make(dir(), codec(), irs::OM_CREATE, options);

    const tests::document* doc;
    while ((doc = gen.next())) {
      ASSERT_TRUE(insert(*writer, doc->indexed.end(), doc->indexed.end(), doc->stored.begin(), doc->stored.end()));
      ++docs_count;
    }
    writer->commit();
  }

  // check inserted values:
  // - visit (not cached)
  // - random read (cached)
  // - visit (cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(dir());
    ASSERT_EQ(1, reader.size());

    auto& segment = reader[0];
    auto columns = segment.columns();
    ASSERT_TRUE(columns->next());
    ASSERT_EQ("id", columns->value().name);
    ASSERT_EQ(0, columns->value().id);
    ASSERT_TRUE(columns->next());
    ASSERT_EQ("label", columns->value().name);
    ASSERT_EQ(1, columns->value().id);
    ASSERT_FALSE(columns->next());
    ASSERT_FALSE(columns->next());

    // check 'id' column
    {
      const iresearch::string_ref column_name = "id";
      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // visit column (not cached)
      {
        gen.reset();
        irs::doc_id_t expected_id = 0;
        auto visitor = [&gen, &column_name, &expected_id](irs::doc_id_t id, const irs::bytes_ref& in) {
          if (id != ++expected_id) {
            return false;
          }

          auto* doc = gen.next();
          auto* field = doc->stored.get<tests::templates::string_field>(column_name);

          if (!field) {
            return false;
          }

          const auto actual_value = irs::to_string<irs::string_ref>(in.c_str());
          if (field->value() != actual_value) {
            return false;
          }

          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // random access
      {
        const tests::document* doc = nullptr;
        irs::bytes_ref actual_value;
        auto column = segment.column_reader(meta->id);
        ASSERT_NE(nullptr, column);
        auto reader = column->values();

        irs::doc_id_t id = 0;
        gen.reset();
        while ((doc = gen.next())) {
          ++id;
          ASSERT_TRUE(reader(id, actual_value));

          auto* field = doc->stored.get<tests::templates::string_field>(column_name);

          ASSERT_NE(nullptr, field);
          ASSERT_EQ(field->value(), irs::to_string<irs::string_ref>(actual_value.c_str()));
        }
      }

      // visit column (cached)
      {
        gen.reset();
        irs::doc_id_t expected_id = 0;
        auto visitor = [&gen, &column_name, &expected_id](irs::doc_id_t id, const irs::bytes_ref& in) {
          if (id != ++expected_id) {
            return false;
          }

          auto* doc = gen.next();
          auto* field = doc->stored.get<tests::templates::string_field>(column_name);

          if (!field) {
            return false;
          }

          const auto actual_value = irs::to_string<irs::string_ref>(in.c_str());
          if (field->value() != actual_value) {
            return false;
          }

          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // iterate over column (cached)
      {
        gen.reset();
        irs::doc_id_t expected_id = 0;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_FALSE(!payload);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

        for (; it->next(); ) {
          ++expected_id;

          auto* doc = gen.next();
          auto* field = doc->stored.get<tests::templates::string_field>(column_name);
          ASSERT_NE(nullptr, field);

          const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

          ASSERT_EQ(expected_id, it->value());
          ASSERT_EQ(field->value(), actual_value_str);
        }

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::doc_limits::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
        ASSERT_EQ(docs_count, expected_id);
      }
    }

    // check 'label' column
    {
      const iresearch::string_ref column_name = "label";
      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // visit column (not cached)
      {
        gen.reset();
        irs::doc_id_t expected_id = 0;
        auto visitor = [&gen, &column_name, &expected_id](irs::doc_id_t id, const irs::bytes_ref& in) {
          if (id != ++expected_id) {
            return false;
          }

          auto* doc = gen.next();
          auto* field = doc->stored.get<tests::templates::string_field>(column_name);

          if (!field) {
            return false;
          }

          if (field->value() != irs::to_string<irs::string_ref>(in.c_str())) {
            return false;
          }

          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // random access
      {
        const tests::document* doc = nullptr;

        irs::bytes_ref actual_value;
        auto column = segment.column_reader(meta->id);
        ASSERT_NE(nullptr, column);
        auto reader = column->values();

        irs::doc_id_t id = 0;
        while ((doc = gen.next())) {
          ASSERT_TRUE(reader(++id, actual_value));

          auto* field = doc->stored.get<tests::templates::string_field>(column_name);
          ASSERT_NE(nullptr, field);
          ASSERT_EQ(field->value(), irs::to_string<irs::string_ref>(actual_value.c_str()));
        }
      }

      // visit column (cached)
      {
        gen.reset();
        irs::doc_id_t expected_id = 0;
        auto visitor = [&gen, &column_name, &expected_id](irs::doc_id_t id, const irs::bytes_ref& in) {
          if (id != ++expected_id) {
            return false;
          }

          auto* doc = gen.next();
          auto* field = doc->stored.get<tests::templates::string_field>(column_name);

          if (!field) {
            return false;
          }

          if (field->value() != irs::to_string<irs::string_ref>(in.c_str())) {
            return false;
          }

          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // iterate over 'label' column (cached)
      {
        gen.reset();
        irs::doc_id_t expected_id = 0;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_FALSE(!payload);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

        for (; it->next(); ) {
          ++expected_id;

          auto* doc = gen.next();
          auto* field = doc->stored.get<tests::templates::string_field>(column_name);
          ASSERT_NE(nullptr, field);
          const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

          ASSERT_EQ(expected_id, it->value());
          ASSERT_EQ(field->value(), actual_value_str);
        }

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::doc_limits::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
        ASSERT_EQ(docs_count, expected_id);
      }
    }
  }

  // check inserted values:
  // - visit (not cached)
  // - iterate (not cached)
  // - random read (cached)
  // - visit (cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(dir());
    ASSERT_EQ(1, reader.size());

    auto& segment = reader[0];
    auto columns = segment.columns();
    ASSERT_TRUE(columns->next());
    ASSERT_EQ("id", columns->value().name);
    ASSERT_EQ(0, columns->value().id);
    ASSERT_TRUE(columns->next());
    ASSERT_EQ("label", columns->value().name);
    ASSERT_EQ(1, columns->value().id);
    ASSERT_FALSE(columns->next());
    ASSERT_FALSE(columns->next());

    // check 'id' column
    {
      const iresearch::string_ref column_name = "id";
      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // visit column (not cached)
      {
        gen.reset();
        irs::doc_id_t expected_id = 0;
        auto visitor = [&gen, &column_name, &expected_id](irs::doc_id_t id, const irs::bytes_ref& in) {
          if (id != ++expected_id) {
            return false;
          }

          auto* doc = gen.next();
          auto* field = doc->stored.get<tests::templates::string_field>(column_name);

          if (!field) {
            return false;
          }

          const auto actual_value = irs::to_string<irs::string_ref>(in.c_str());
          if (field->value() != actual_value) {
            return false;
          }

          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // iterate over column (not cached)
      {
        gen.reset();
        irs::doc_id_t expected_id = 0;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_FALSE(!payload);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

        for (; it->next(); ) {
          ++expected_id;

          auto* doc = gen.next();
          auto* field = doc->stored.get<tests::templates::string_field>(column_name);
          ASSERT_NE(nullptr, field);
          const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

          ASSERT_EQ(expected_id, it->value());
          ASSERT_EQ(field->value(), actual_value_str);
        }

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::doc_limits::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
        ASSERT_EQ(docs_count, expected_id);
      }

      // random access
      {
        const tests::document* doc = nullptr;
        irs::bytes_ref actual_value;
        auto column = segment.column_reader(meta->id);
        ASSERT_NE(nullptr, column);
        auto reader = column->values();

        irs::doc_id_t id = 0;
        gen.reset();
        while ((doc = gen.next())) {
          ++id;
          ASSERT_TRUE(reader(id, actual_value));

          auto* field = doc->stored.get<tests::templates::string_field>(column_name);

          ASSERT_NE(nullptr, field);
          ASSERT_EQ(field->value(), irs::to_string<irs::string_ref>(actual_value.c_str()));
        }
      }

      // visit column (cached)
      {
        gen.reset();
        irs::doc_id_t expected_id = 0;
        auto visitor = [&gen, &column_name, &expected_id](irs::doc_id_t id, const irs::bytes_ref& in) {
          if (id != ++expected_id) {
            return false;
          }

          auto* doc = gen.next();
          auto* field = doc->stored.get<tests::templates::string_field>(column_name);

          if (!field) {
            return false;
          }

          const auto actual_value = irs::to_string<irs::string_ref>(in.c_str());
          if (field->value() != actual_value) {
            return false;
          }

          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // iterate over column (cached)
      {
        gen.reset();
        irs::doc_id_t expected_id = 0;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_FALSE(!payload);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

        for (; it->next(); ) {
          ++expected_id;

          auto* doc = gen.next();
          auto* field = doc->stored.get<tests::templates::string_field>(column_name);
          ASSERT_NE(nullptr, field);
          const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

          ASSERT_EQ(expected_id, it->value());
          ASSERT_EQ(field->value(), actual_value_str);
        }

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::doc_limits::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
        ASSERT_EQ(docs_count, expected_id);
      }
    }

    // check 'label' column
    {
      const iresearch::string_ref column_name = "label";
      auto* meta = segment.column(column_name);
      ASSERT_NE(nullptr, meta);

      // visit column (not cached)
      {
        gen.reset();
        irs::doc_id_t expected_id = 0;
        auto visitor = [&gen, &column_name, &expected_id](irs::doc_id_t id, const irs::bytes_ref& in) {
          if (id != ++expected_id) {
            return false;
          }

          auto* doc = gen.next();
          auto* field = doc->stored.get<tests::templates::string_field>(column_name);

          if (!field) {
            return false;
          }

          if (field->value() != irs::to_string<irs::string_ref>(in.c_str())) {
            return false;
          }

          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // iterate over 'label' column (not cached)
      {
        gen.reset();
        irs::doc_id_t expected_id = 0;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_FALSE(!payload);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

        for (; it->next(); ) {
          ++expected_id;

          auto* doc = gen.next();
          auto* field = doc->stored.get<tests::templates::string_field>(column_name);
          ASSERT_NE(nullptr, field);
          const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

          ASSERT_EQ(expected_id, it->value());
          ASSERT_EQ(field->value(), actual_value_str);
        }

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::doc_limits::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
        ASSERT_EQ(docs_count, expected_id);
      }

      // random access
      {
        const tests::document* doc = nullptr;

        irs::bytes_ref actual_value;
        auto column = segment.column_reader(meta->id);
        ASSERT_NE(nullptr, column);
        auto reader = column->values();

        irs::doc_id_t id = 0;
        while ((doc = gen.next())) {
          ASSERT_TRUE(reader(++id, actual_value));

          auto* field = doc->stored.get<tests::templates::string_field>(column_name);
          ASSERT_NE(nullptr, field);
          ASSERT_EQ(field->value(), irs::to_string<irs::string_ref>(actual_value.c_str()));
        }
      }

      // visit column (cached)
      {
        gen.reset();
        irs::doc_id_t expected_id = 0;
        auto visitor = [&gen, &column_name, &expected_id] (irs::doc_id_t id, const irs::bytes_ref& in) {
          if (id != ++expected_id) {
            return false;
          }

          auto* doc = gen.next();
          auto* field = doc->stored.get<tests::templates::string_field>(column_name);

          if (!field) {
            return false;
          }

          if (field->value() != irs::to_string<irs::string_ref>(in.c_str())) {
            return false;
          }

          return true;
        };

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        ASSERT_EQ(column, segment.column_reader(meta->id));
        ASSERT_TRUE(column->visit(visitor));
      }

      // iterate over 'label' column (cached)
      {
        gen.reset();
        irs::doc_id_t expected_id = 0;

        auto column = segment.column_reader(column_name);
        ASSERT_NE(nullptr, column);
        auto it = column->iterator();
        ASSERT_NE(nullptr, it);

        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_FALSE(!payload);
        ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

        for (; it->next(); ) {
          ++expected_id;

          auto* doc = gen.next();
          auto* field = doc->stored.get<tests::templates::string_field>(column_name);
          ASSERT_NE(nullptr, field);
          const auto actual_value_str = irs::to_string<irs::string_ref>(payload->value.c_str());

          ASSERT_EQ(expected_id, it->value());
          ASSERT_EQ(field->value(), actual_value_str);
        }

        ASSERT_FALSE(it->next());
        ASSERT_EQ(irs::doc_limits::eof(), it->value());
        ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
        ASSERT_EQ(docs_count, expected_id);
      }
    }
  }
}

TEST_P(index_column_test_case, read_write_doc_attributes) {
  irs::index_writer::init_options options;
  options.column_info = [](const irs::string_ref&) {
    return irs::column_info{ irs::type<irs::compression::lz4>::get(), irs::compression::options{}, true };
  };

  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    &tests::generic_json_field_factory
  );
  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();
  tests::document const* doc3 = gen.next();
  tests::document const* doc4 = gen.next();

  // write documents
  {
    auto writer = irs::index_writer::make(dir(), codec(), irs::OM_CREATE, options);

    // attributes only
    ASSERT_TRUE(insert(*writer, doc1->indexed.end(), doc1->indexed.end(), doc1->stored.begin(), doc1->stored.end()));
    ASSERT_TRUE(insert(*writer, doc2->indexed.end(), doc2->indexed.end(), doc2->stored.begin(), doc2->stored.end()));
    ASSERT_TRUE(insert(*writer, doc3->indexed.end(), doc3->indexed.end(), doc3->stored.begin(), doc3->stored.end()));
    ASSERT_TRUE(insert(*writer, doc4->indexed.end(), doc4->indexed.end(), doc4->stored.begin(), doc4->stored.end()));
    writer->commit();
  }

  // check inserted values:
  // - random read (not cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());

    // read attribute from invalid column
    {
      ASSERT_EQ(nullptr, segment.column_reader("invalid_column"));
    }

    // check number of values in the column
    {
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(4, column->size());
    }

    // read attributes from 'name' column (dense)
    {
      irs::bytes_ref actual_value;

      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto value_reader = column->values();

      ASSERT_TRUE(value_reader(2, actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
      ASSERT_TRUE(value_reader(4, actual_value));
      ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
      ASSERT_TRUE(value_reader(1, actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_TRUE(value_reader(3, actual_value));
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
      ASSERT_FALSE(value_reader(5, actual_value)); // invalid document id
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // same as 'name' value in doc3
    }

    // iterate over 'name' column (cached)
    {
      auto column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      std::vector<std::pair<irs::doc_id_t, irs::string_ref>> expected_values = {
        { 1, "A" }, { 2, "B" }, { 3, "C" }, { 4, "D" }
      };

      size_t i = 0;
      for (; it->next(); ++i) {
        const auto& expected_value = expected_values[i];
        const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value.c_str());

        ASSERT_EQ(expected_value.first, it->value());
        ASSERT_EQ(expected_value.second, actual_str_value);
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(i, expected_values.size());
    }

    // read attributes from 'prefix' column (sparse)
    {
      irs::bytes_ref actual_value;
      const auto* column = segment.column_reader("prefix");
      ASSERT_NE(nullptr, column);
      auto value_reader = column->values();
      ASSERT_TRUE(value_reader(1, actual_value));
      ASSERT_EQ("abcd", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'prefix' value in doc1
      ASSERT_FALSE(value_reader(2, actual_value)); // doc2 does not contain 'prefix' column
      ASSERT_EQ("abcd", irs::to_string<irs::string_ref>(actual_value.c_str())); // same as 'prefix' value in doc1
      ASSERT_TRUE(value_reader(4, actual_value));
      ASSERT_EQ("abcde", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'prefix' value in doc4
      ASSERT_FALSE(value_reader(3, actual_value)); // doc3 does not contain 'prefix' column
      ASSERT_EQ("abcde", irs::to_string<irs::string_ref>(actual_value.c_str())); // same as 'prefix' value in doc4
      ASSERT_FALSE(value_reader(5, actual_value)); // invalid document id
      ASSERT_EQ("abcde", irs::to_string<irs::string_ref>(actual_value.c_str())); // same as 'prefix' value in doc4
    }

    // iterate over 'prefix' column (cached)
    {
      auto column = segment.column_reader("prefix");
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      std::vector<std::pair<irs::doc_id_t, irs::string_ref>> expected_values = {
        { 1, "abcd" }, { 4, "abcde" }
      };

      size_t i = 0;
      for (; it->next(); ++i) {
        const auto& expected_value = expected_values[i];
        const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value.c_str());

        ASSERT_EQ(expected_value.first, it->value());
        ASSERT_EQ(expected_value.second, actual_str_value);
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(i, expected_values.size());
    }
  }

  // check inserted values:
  // - iterate (not cached)
  // - random read (cached)
  // - iterate (cached)
  {
    auto reader = irs::directory_reader::open(dir(), codec());
    ASSERT_EQ(1, reader.size());
    auto& segment = *(reader.begin());

    // read attribute from invalid column
    {
      ASSERT_EQ(nullptr, segment.column_reader("invalid_column"));
    }

    {
      // iterate over 'name' column (not cached)
      auto column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      std::vector<std::pair<irs::doc_id_t, irs::string_ref>> expected_values = {
        { 1, "A" }, { 2, "B" }, { 3, "C" }, { 4, "D" }
      };

      size_t i = 0;
      for (; it->next(); ++i) {
        const auto& expected_value = expected_values[i];
        const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value.c_str());

        ASSERT_EQ(expected_value.first, it->value());
        ASSERT_EQ(expected_value.second, actual_str_value);
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(i, expected_values.size());
    }

    // read attributes from 'name' column (dense)
    {
      irs::bytes_ref actual_value;
      const auto* column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto value_reader = column->values();
      ASSERT_TRUE(value_reader(2, actual_value));
      ASSERT_EQ("B", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc2
      ASSERT_TRUE(value_reader(4, actual_value));
      ASSERT_EQ("D", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc4
      ASSERT_TRUE(value_reader(1, actual_value));
      ASSERT_EQ("A", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc1
      ASSERT_TRUE(value_reader(3, actual_value));
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'name' value in doc3
      ASSERT_FALSE(value_reader(5, actual_value)); // invalid document id
      ASSERT_EQ("C", irs::to_string<irs::string_ref>(actual_value.c_str())); // same as 'name' value in doc3
    }

    // iterate over 'name' column (cached)
    {
      auto column = segment.column_reader("name");
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      std::vector<std::pair<irs::doc_id_t, irs::string_ref>> expected_values = {
        { 1, "A" }, { 2, "B" }, { 3, "C" }, { 4, "D" }
      };

      size_t i = 0;
      for (; it->next(); ++i) {
        const auto& expected_value = expected_values[i];
        const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value.c_str());

        ASSERT_EQ(expected_value.first, it->value());
        ASSERT_EQ(expected_value.second, actual_str_value);
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(i, expected_values.size());
    }

    {
      // iterate over 'prefix' column (not cached)
      auto column = segment.column_reader("prefix");
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      std::vector<std::pair<irs::doc_id_t, irs::string_ref>> expected_values = {
        { 1, "abcd" }, { 4, "abcde" }
      };

      size_t i = 0;
      for (; it->next(); ++i) {
        const auto& expected_value = expected_values[i];
        const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value.c_str());

        ASSERT_EQ(expected_value.first, it->value());
        ASSERT_EQ(expected_value.second, actual_str_value);
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(i, expected_values.size());
    }

    // read attributes from 'prefix' column (sparse)
    {
      irs::bytes_ref actual_value;
      const auto* column = segment.column_reader("prefix");
      ASSERT_NE(nullptr, column);
      auto value_reader = column->values();
      ASSERT_TRUE(value_reader(1, actual_value));
      ASSERT_EQ("abcd", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'prefix' value in doc1
      ASSERT_FALSE(value_reader(2, actual_value)); // doc2 does not contain 'prefix' column
      ASSERT_EQ("abcd", irs::to_string<irs::string_ref>(actual_value.c_str())); // same as 'prefix' value in doc1
      ASSERT_TRUE(value_reader(4, actual_value));
      ASSERT_EQ("abcde", irs::to_string<irs::string_ref>(actual_value.c_str())); // 'prefix' value in doc4
      ASSERT_FALSE(value_reader(3, actual_value)); // doc3 does not contain 'prefix' column
      ASSERT_EQ("abcde", irs::to_string<irs::string_ref>(actual_value.c_str())); // same as 'prefix' value in doc4
      ASSERT_FALSE(value_reader(5, actual_value)); // invalid document id
      ASSERT_EQ("abcde", irs::to_string<irs::string_ref>(actual_value.c_str())); // same as 'prefix' value in doc4
    }

    // iterate over 'prefix' column (cached)
    {
      auto column = segment.column_reader("prefix");
      ASSERT_NE(nullptr, column);
      auto it = column->iterator();
      ASSERT_NE(nullptr, it);

      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_FALSE(!payload);
      ASSERT_EQ(irs::type_limits<irs::type_t::doc_id_t>::invalid(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);

      std::vector<std::pair<irs::doc_id_t, irs::string_ref>> expected_values = {
        { 1, "abcd" }, { 4, "abcde" }
      };

      size_t i = 0;
      for (; it->next(); ++i) {
        const auto& expected_value = expected_values[i];
        const auto actual_str_value = irs::to_string<irs::string_ref>(payload->value.c_str());

        ASSERT_EQ(expected_value.first, it->value());
        ASSERT_EQ(expected_value.second, actual_str_value);
      }

      ASSERT_FALSE(it->next());
      ASSERT_EQ(irs::doc_limits::eof(), it->value());
      ASSERT_EQ(irs::bytes_ref::NIL, payload->value);
      ASSERT_EQ(i, expected_values.size());
    }
  }
}

TEST_P(index_column_test_case, read_empty_doc_attributes) {
  irs::index_writer::init_options options;
  options.column_info = [](const irs::string_ref&) {
    return irs::column_info{ irs::type<irs::compression::lz4>::get(), irs::compression::options{}, true };
  };

  tests::json_doc_generator gen(
    resource("simple_sequential.json"),
    &tests::generic_json_field_factory
  );
  tests::document const* doc1 = gen.next();
  tests::document const* doc2 = gen.next();
  tests::document const* doc3 = gen.next();
  tests::document const* doc4 = gen.next();

  // write documents without attributes
  {
    auto writer = irs::index_writer::make(dir(), codec(), irs::OM_CREATE, options);

    // fields only
    ASSERT_TRUE(insert(*writer, doc1->indexed.begin(), doc1->indexed.end()));
    ASSERT_TRUE(insert(*writer, doc2->indexed.begin(), doc2->indexed.end()));
    ASSERT_TRUE(insert(*writer, doc3->indexed.begin(), doc3->indexed.end()));
    ASSERT_TRUE(insert(*writer, doc4->indexed.begin(), doc4->indexed.end()));
    writer->commit();
  }

  auto reader = irs::directory_reader::open(dir(), codec());
  ASSERT_EQ(1, reader.size());
  auto& segment = *(reader.begin());

  const auto* column = segment.column_reader("name");
  ASSERT_EQ(nullptr, column);
}

INSTANTIATE_TEST_CASE_P(
  index_column_test,
  index_column_test_case,
  ::testing::Combine(
    ::testing::Values(
      &tests::memory_directory,
      &tests::fs_directory,
      &tests::mmap_directory),
    ::testing::Values(
      tests::format_info{"1_0"},
      tests::format_info{"1_1", "1_0"},
      tests::format_info{"1_2", "1_0"})
  ),
  tests::to_string
);
