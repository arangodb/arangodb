////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2021 ArangoDB GmbH, Cologne, Germany
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
#include "tests_param.hpp"

#include "formats/columnstore2.hpp"

class columnstore2_test_case : public virtual tests::directory_test_case_base<bool> {
 public:
  static std::string to_string(
      const testing::TestParamInfo<std::tuple<tests::dir_factory_f, bool>>& info) {
    tests::dir_factory_f factory;
    bool consolidation;
    std::tie(factory, consolidation) = info.param;

    if (consolidation) {
      return (*factory)(nullptr).second + "___consolidation";
    }

    return (*factory)(nullptr).second;
  }

  bool consolidation() const noexcept {
    auto& p = this->GetParam();
    return std::get<bool>(p);
  }
};

TEST_P(columnstore2_test_case, reader_ctor) {
  irs::columnstore2::reader reader;
  ASSERT_EQ(0, reader.size());
  ASSERT_EQ(nullptr, reader.column(0));
  ASSERT_EQ(nullptr, reader.header(0));
}

TEST_P(columnstore2_test_case, empty_columnstore) {
  constexpr irs::doc_id_t MAX = 1;
  const irs::segment_meta meta("test", nullptr);

  irs::flush_state state;
  state.doc_count = MAX;
  state.name = meta.name;

  irs::columnstore2::writer writer(this->consolidation());
  writer.prepare(dir(), meta);
  writer.push_column({ irs::type<irs::compression::none>::get(), {}, false });
  writer.push_column({ irs::type<irs::compression::none>::get(), {}, false });
  ASSERT_FALSE(writer.commit(state));

  irs::columnstore2::reader reader;
  ASSERT_FALSE(reader.prepare(dir(), meta));
}

TEST_P(columnstore2_test_case, empty_column) {
  constexpr irs::doc_id_t MAX = 1;
  const irs::segment_meta meta("test", nullptr);
  const bool has_encryption = bool(irs::get_encryption(dir().attributes()));

  irs::flush_state state;
  state.doc_count = MAX;
  state.name = meta.name;

  irs::columnstore2::writer writer(this->consolidation());
  writer.prepare(dir(), meta);
  [[maybe_unused]] auto [id0, handle0] = writer.push_column(
    { irs::type<irs::compression::none>::get(), {}, has_encryption });
  [[maybe_unused]] auto [id1, handle1] = writer.push_column(
    { irs::type<irs::compression::none>::get(), {}, has_encryption });
  [[maybe_unused]] auto [id2, handle2] = writer.push_column(
    { irs::type<irs::compression::none>::get(), {}, has_encryption });
  handle1(42).write_byte(42);
  ASSERT_TRUE(writer.commit(state));

  irs::columnstore2::reader reader;
  ASSERT_TRUE(reader.prepare(dir(), meta));
  ASSERT_EQ(2, reader.size());

  // column 0
  {
    auto* header = reader.header(0);
    ASSERT_NE(nullptr, header);
    ASSERT_EQ(0, header->docs_count);
    ASSERT_EQ(0, header->docs_index);
    ASSERT_EQ(irs::doc_limits::invalid(), header->min);
    ASSERT_EQ(irs::columnstore2::ColumnType::MASK, header->type);
    ASSERT_EQ(has_encryption ? irs::columnstore2::ColumnProperty::ENCRYPT
                             : irs::columnstore2::ColumnProperty::NORMAL,
              header->props);

    auto column = reader.column(0);
    ASSERT_NE(nullptr, column);
    auto it = column->iterator();
    ASSERT_NE(nullptr, it);
    ASSERT_EQ(0, irs::cost::extract(*it));
    ASSERT_TRUE(irs::doc_limits::eof(it->value()));
  }

  // column 1
  {
    auto* header = reader.header(1);
    ASSERT_NE(nullptr, header);
    ASSERT_EQ(1, header->docs_count);
    ASSERT_EQ(0, header->docs_index);
    ASSERT_EQ(42, header->min);
    ASSERT_EQ(irs::columnstore2::ColumnType::SPARSE, header->type); // FIXME why sparse?
    ASSERT_EQ(has_encryption ? irs::columnstore2::ColumnProperty::ENCRYPT
                             : irs::columnstore2::ColumnProperty::NORMAL,
              header->props);

    auto column = reader.column(1);
    ASSERT_NE(nullptr, column);
    auto it = column->iterator();
    auto* document = irs::get<irs::document>(*it);
    ASSERT_NE(nullptr, document);
    auto* payload = irs::get<irs::payload>(*it);
    ASSERT_NE(nullptr, payload);
    auto* cost = irs::get<irs::cost>(*it);
    ASSERT_NE(nullptr, cost);
    ASSERT_EQ(column->size(), cost->estimate());
    ASSERT_NE(nullptr, it);
    ASSERT_FALSE(irs::doc_limits::valid(it->value()));
    ASSERT_TRUE(it->next());
    ASSERT_EQ(42, it->value());
    ASSERT_EQ(1, payload->value.size());
    ASSERT_EQ(42, payload->value[0]);
    ASSERT_FALSE(it->next());
    ASSERT_FALSE(it->next());
  }
}

TEST_P(columnstore2_test_case, sparse_mask_column) {
  constexpr irs::doc_id_t MAX = 1000000;
  const irs::segment_meta meta("test", nullptr);
  const bool has_encryption = bool(irs::get_encryption(dir().attributes()));

  irs::flush_state state;
  state.doc_count = MAX;
  state.name = meta.name;

  {
    irs::columnstore2::writer writer(this->consolidation());
    writer.prepare(dir(), meta);

    auto [id, column] = writer.push_column({
      irs::type<irs::compression::none>::get(),
      {}, has_encryption});

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; doc += 2) {
      column(doc);
    }

    ASSERT_TRUE(writer.commit(state));
  }

  {
    irs::columnstore2::reader reader;
    ASSERT_TRUE(reader.prepare(dir(), meta));
    ASSERT_EQ(1, reader.size());

    auto* header = reader.header(0);
    ASSERT_NE(nullptr, header);
    ASSERT_EQ(MAX/2, header->docs_count);
    ASSERT_NE(0, header->docs_index);
    ASSERT_EQ(irs::doc_limits::min(), header->min);
    ASSERT_EQ(irs::columnstore2::ColumnType::MASK, header->type);
    ASSERT_EQ(has_encryption ? irs::columnstore2::ColumnProperty::ENCRYPT
                             : irs::columnstore2::ColumnProperty::NORMAL,
              header->props);

    auto* column = reader.column(0);
    ASSERT_NE(nullptr, column);
    ASSERT_EQ(MAX/2, column->size());

    // seek stateful
    {
      auto it = column->iterator();
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_EQ(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; doc += 2) {
        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(doc, it->seek(doc));
      }
    }

    // seek stateless
    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; doc += 2) {
      auto it = column->iterator();
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_EQ(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());

      ASSERT_EQ(doc, it->seek(doc));
      ASSERT_EQ(doc, it->seek(doc));
    }

    // seek + next
    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; doc += 5000) {
      auto it = column->iterator();
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_EQ(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());

      ASSERT_EQ(doc, it->seek(doc));
      ASSERT_EQ(doc, it->seek(doc));
      ASSERT_EQ(doc, it->seek(doc-1));

      auto next_it = column->iterator();
      ASSERT_EQ(doc, next_it->seek(doc));
      for (auto next_doc = doc + 2; next_doc < MAX; next_doc += 2) {
        ASSERT_TRUE(next_it->next());
        ASSERT_EQ(next_doc, next_it->value());
      }
    }

    // next + seek
    {
      auto it = column->iterator();
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_EQ(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      ASSERT_TRUE(it->next());
      ASSERT_EQ(irs::doc_limits::min(), document->value);
      ASSERT_EQ(118775, it->seek(118774));
      ASSERT_TRUE(irs::doc_limits::eof(it->seek(MAX + 1)));
    }
  }
}

TEST_P(columnstore2_test_case, sparse_column) {
  constexpr irs::doc_id_t MAX = 1000000;
  const irs::segment_meta meta("test", nullptr);
  const bool has_encryption = bool(irs::get_encryption(dir().attributes()));

  irs::flush_state state;
  state.doc_count = MAX;
  state.name = meta.name;

  {
    irs::columnstore2::writer writer(this->consolidation());
    writer.prepare(dir(), meta);

    auto [id, column] = writer.push_column({
      irs::type<irs::compression::none>::get(),
      {}, has_encryption });

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; doc += 2) {
      auto& stream = column(doc);
      const auto str = std::to_string(doc);
      stream.write_bytes(reinterpret_cast<const irs::byte_type*>(str.c_str()), str.size());
    }

    ASSERT_TRUE(writer.commit(state));
  }

  {
    irs::columnstore2::reader reader;
    ASSERT_TRUE(reader.prepare(dir(), meta));
    ASSERT_EQ(1, reader.size());

    auto* header = reader.header(0);
    ASSERT_NE(nullptr, header);
    ASSERT_EQ(MAX/2, header->docs_count);
    ASSERT_NE(0, header->docs_index);
    ASSERT_EQ(irs::doc_limits::min(), header->min);
    ASSERT_EQ(irs::columnstore2::ColumnType::SPARSE, header->type);
    ASSERT_EQ(has_encryption ? irs::columnstore2::ColumnProperty::ENCRYPT
                             : irs::columnstore2::ColumnProperty::NORMAL,
              header->props);

    auto* column = reader.column(0);
    ASSERT_NE(nullptr, column);
    ASSERT_EQ(MAX/2, column->size());

    {
      auto it = column->iterator();
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; doc += 2) {
        SCOPED_TRACE(doc);
        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(doc, it->seek(doc));
        const auto str = std::to_string(doc);
        ASSERT_EQ(str, irs::ref_cast<char>(payload->value));
      }
    }

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; doc += 2) {
      auto it = column->iterator();
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());

      const auto str = std::to_string(doc);
      ASSERT_EQ(doc, it->seek(doc));
      EXPECT_EQ(str, irs::ref_cast<char>(payload->value));
      ASSERT_EQ(doc, it->seek(doc));
      EXPECT_EQ(str, irs::ref_cast<char>(payload->value));
      ASSERT_EQ(doc, it->seek(doc-1));
      EXPECT_EQ(str, irs::ref_cast<char>(payload->value));
    }

    // seek + next
    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; doc += 5000) {
      auto it = column->iterator();
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());

      const auto str = std::to_string(doc);
      ASSERT_EQ(doc, it->seek(doc));
      EXPECT_EQ(str, irs::ref_cast<char>(payload->value));
      ASSERT_EQ(doc, it->seek(doc));
      EXPECT_EQ(str, irs::ref_cast<char>(payload->value));
      ASSERT_EQ(doc, it->seek(doc-1));
      EXPECT_EQ(str, irs::ref_cast<char>(payload->value));

      auto next_it = column->iterator();
      auto* next_payload = irs::get<irs::payload>(*next_it);
      ASSERT_NE(nullptr, next_payload);
      ASSERT_EQ(doc, next_it->seek(doc));
      EXPECT_EQ(str, irs::ref_cast<char>(next_payload->value));
      for (auto next_doc = doc + 2; next_doc < MAX; next_doc += 2) {
        ASSERT_TRUE(next_it->next());
        ASSERT_EQ(next_doc, next_it->value());
        const auto str = std::to_string(next_doc);
        EXPECT_EQ(str, irs::ref_cast<char>(next_payload->value));
      }
    }

    // next + seek
    {
      auto it = column->iterator();
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      ASSERT_TRUE(it->next());
      ASSERT_EQ(irs::doc_limits::min(), document->value);
      ASSERT_EQ(118775, it->seek(118774));
      const auto str = std::to_string(it->value());
      EXPECT_EQ(str, irs::ref_cast<char>(payload->value));
      ASSERT_TRUE(irs::doc_limits::eof(it->seek(MAX + 1)));
    }
  }
}

TEST_P(columnstore2_test_case, dense_mask_column) {
  constexpr irs::doc_id_t MAX = 1000000;
  const irs::segment_meta meta("test", nullptr);
  const bool has_encryption = bool(irs::get_encryption(dir().attributes()));

  irs::flush_state state;
  state.doc_count = MAX;
  state.name = meta.name;

  {
    irs::columnstore2::writer writer(this->consolidation());
    writer.prepare(dir(), meta);

    auto [id, column] = writer.push_column({
      irs::type<irs::compression::none>::get(),
      {}, has_encryption });

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
      column(doc);
    }

    ASSERT_TRUE(writer.commit(state));
  }

  {
    irs::columnstore2::reader reader;
    ASSERT_TRUE(reader.prepare(dir(), meta));
    ASSERT_EQ(1, reader.size());

    auto* header = reader.header(0);
    ASSERT_NE(nullptr, header);
    ASSERT_EQ(MAX, header->docs_count);
    ASSERT_EQ(0, header->docs_index);
    ASSERT_EQ(irs::doc_limits::min(), header->min);
    ASSERT_EQ(irs::columnstore2::ColumnType::MASK, header->type);
    ASSERT_EQ(has_encryption ? irs::columnstore2::ColumnProperty::ENCRYPT
                             : irs::columnstore2::ColumnProperty::NORMAL,
              header->props);

    auto* column = reader.column(0);
    ASSERT_NE(nullptr, column);
    ASSERT_EQ(MAX, column->size());

    {
      auto it = column->iterator();
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      ASSERT_TRUE(payload->value.null());
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_TRUE(payload->value.null());
        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_TRUE(payload->value.null());
        ASSERT_EQ(doc, it->seek(doc-1));
        ASSERT_TRUE(payload->value.null());
      }
    }

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
      auto it = column->iterator();
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      ASSERT_TRUE(payload->value.null());
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());

      ASSERT_EQ(doc, it->seek(doc));
      ASSERT_TRUE(payload->value.null());
      ASSERT_EQ(doc, it->seek(doc));
      ASSERT_TRUE(payload->value.null());
      ASSERT_EQ(doc, it->seek(doc-1));
      ASSERT_TRUE(payload->value.null());
    }

    // seek + next
    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; doc += 10000) {
      auto it = column->iterator();
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());

      ASSERT_EQ(doc, it->seek(doc));
      ASSERT_EQ(doc, it->seek(doc));

      auto next_it = column->iterator();
      ASSERT_EQ(doc, next_it->seek(doc));
      for (auto next_doc = doc + 1; next_doc < MAX; ++next_doc) {
        ASSERT_TRUE(next_it->next());
        ASSERT_EQ(next_doc, next_it->value());
      }
    }

    // next + seek
    {
      auto it = column->iterator();
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      ASSERT_TRUE(it->next());
      ASSERT_EQ(irs::doc_limits::min(), document->value);
      ASSERT_TRUE(payload->value.null());
      ASSERT_EQ(118774, it->seek(118774));
      ASSERT_TRUE(payload->value.null());
      ASSERT_TRUE(irs::doc_limits::eof(it->seek(MAX + 1)));
      ASSERT_TRUE(irs::doc_limits::eof(it->seek(irs::doc_limits::eof())));
    }
  }
}

TEST_P(columnstore2_test_case, dense_column) {
  constexpr irs::doc_id_t MAX = 1000000;
  const irs::segment_meta meta("test", nullptr);
  const bool has_encryption = bool(irs::get_encryption(dir().attributes()));

  irs::flush_state state;
  state.doc_count = MAX;
  state.name = meta.name;

  {
    irs::columnstore2::writer writer(this->consolidation());
    writer.prepare(dir(), meta);

    auto [id, column] = writer.push_column({
      irs::type<irs::compression::none>::get(),
      {}, has_encryption});

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
      auto& stream = column(doc);
      const auto str = std::to_string(doc);
      stream.write_bytes(reinterpret_cast<const irs::byte_type*>(str.c_str()), str.size());
    }

    ASSERT_TRUE(writer.commit(state));
  }

  {
    irs::columnstore2::reader reader;
    ASSERT_TRUE(reader.prepare(dir(), meta));
    ASSERT_EQ(1, reader.size());

    auto* header = reader.header(0);
    ASSERT_NE(nullptr, header);
    ASSERT_EQ(MAX, header->docs_count);
    ASSERT_EQ(0, header->docs_index);
    ASSERT_EQ(irs::doc_limits::min(), header->min);
    ASSERT_EQ(irs::columnstore2::ColumnType::SPARSE, header->type);
    ASSERT_EQ(has_encryption ? irs::columnstore2::ColumnProperty::ENCRYPT
                             : irs::columnstore2::ColumnProperty::NORMAL,
              header->props);

    auto* column = reader.column(0);
    ASSERT_NE(nullptr, column);
    ASSERT_EQ(MAX, column->size());

    {
      auto it = column->iterator();
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(doc, it->seek(doc));
        const auto str = std::to_string(doc);
        EXPECT_EQ(str, irs::ref_cast<char>(payload->value));
      }
    }

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
      auto it = column->iterator();
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());

      const auto str = std::to_string(doc);
      ASSERT_EQ(doc, it->seek(doc));
      EXPECT_EQ(str, irs::ref_cast<char>(payload->value));
      ASSERT_EQ(doc, it->seek(doc));
      EXPECT_EQ(str, irs::ref_cast<char>(payload->value));
      ASSERT_EQ(doc, it->seek(doc-1));
      EXPECT_EQ(str, irs::ref_cast<char>(payload->value));
    }

    // seek + next
    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; doc += 10000) {
      auto it = column->iterator();
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());

      const auto str = std::to_string(doc);
      ASSERT_EQ(doc, it->seek(doc));
      EXPECT_EQ(str, irs::ref_cast<char>(payload->value));
      ASSERT_EQ(doc, it->seek(doc));
      EXPECT_EQ(str, irs::ref_cast<char>(payload->value));

      auto next_it = column->iterator();
      auto* next_payload = irs::get<irs::payload>(*next_it);
      ASSERT_NE(nullptr, next_payload);
      ASSERT_EQ(doc, next_it->seek(doc));
      EXPECT_EQ(str, irs::ref_cast<char>(next_payload->value));
      for (auto next_doc = doc + 1; next_doc < MAX; ++next_doc) {
        ASSERT_TRUE(next_it->next());
        ASSERT_EQ(next_doc, next_it->value());
        const auto str = std::to_string(next_doc);
        EXPECT_EQ(str, irs::ref_cast<char>(next_payload->value));
      }
    }

    // next + seek
    {
      auto it = column->iterator();
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      ASSERT_TRUE(it->next());
      ASSERT_EQ(irs::doc_limits::min(), document->value);
      const auto str = std::to_string(118774);
      ASSERT_EQ(118774, it->seek(118774));
      EXPECT_EQ(str, irs::ref_cast<char>(payload->value));
      ASSERT_TRUE(irs::doc_limits::eof(it->seek(MAX + 1)));
    }
  }
}

TEST_P(columnstore2_test_case, dense_column_range) {
  constexpr irs::doc_id_t MIN = 500000;
  constexpr irs::doc_id_t MAX = 1000000;
  const irs::segment_meta meta("test", nullptr);
  const bool has_encryption = bool(irs::get_encryption(dir().attributes()));

  irs::flush_state state;
  state.doc_count = MAX;
  state.name = meta.name;

  {
    irs::columnstore2::writer writer(this->consolidation());
    writer.prepare(dir(), meta);

    auto [id, column] = writer.push_column({
      irs::type<irs::compression::none>::get(),
      {}, has_encryption});

    for (irs::doc_id_t doc = MIN; doc <= MAX; ++doc) {
      auto& stream = column(doc);
      const auto str = std::to_string(doc);
      stream.write_bytes(reinterpret_cast<const irs::byte_type*>(str.c_str()), str.size());
    }

    ASSERT_TRUE(writer.commit(state));
  }

  {
    irs::columnstore2::reader reader;
    ASSERT_TRUE(reader.prepare(dir(), meta));
    ASSERT_EQ(1, reader.size());

    auto* header = reader.header(0);
    ASSERT_NE(nullptr, header);
    ASSERT_EQ(MAX-MIN+1, header->docs_count);
    ASSERT_EQ(0, header->docs_index);
    ASSERT_EQ(MIN, header->min);
    ASSERT_EQ(irs::columnstore2::ColumnType::SPARSE, header->type);
    ASSERT_EQ(has_encryption ? irs::columnstore2::ColumnProperty::ENCRYPT
                             : irs::columnstore2::ColumnProperty::NORMAL,
              header->props);

    auto* column = reader.column(0);
    ASSERT_NE(nullptr, column);
    ASSERT_EQ(MAX-MIN+1, column->size());

    {
      auto it = column->iterator();
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
        const auto expected_doc = (doc <= MIN ? MIN : doc);
        const auto str = std::to_string(expected_doc);
        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        EXPECT_EQ(str, irs::ref_cast<char>(payload->value));
        ASSERT_EQ(expected_doc, it->seek(expected_doc));
        EXPECT_EQ(str, irs::ref_cast<char>(payload->value));
        ASSERT_EQ(expected_doc, it->seek(expected_doc-1));
        EXPECT_EQ(str, irs::ref_cast<char>(payload->value));
      }
    }

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
      auto it = column->iterator();
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());

      const auto expected_doc = (doc <= MIN ? MIN : doc);
      const auto str = std::to_string(expected_doc);
      ASSERT_EQ(expected_doc, it->seek(expected_doc));
      EXPECT_EQ(str, irs::ref_cast<char>(payload->value));
      ASSERT_EQ(expected_doc, it->seek(expected_doc));
      EXPECT_EQ(str, irs::ref_cast<char>(payload->value));
    }

    // seek + next
    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; doc += 10000) {
      auto it = column->iterator();
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());

      const auto expected_doc = (doc <= MIN ? MIN : doc);
      const auto str = std::to_string(expected_doc);
      ASSERT_EQ(expected_doc, it->seek(expected_doc));
      EXPECT_EQ(str, irs::ref_cast<char>(payload->value));
      ASSERT_EQ(expected_doc, it->seek(expected_doc));
      EXPECT_EQ(str, irs::ref_cast<char>(payload->value));

      auto next_it = column->iterator();
      ASSERT_EQ(expected_doc, next_it->seek(expected_doc));
      auto* next_payload = irs::get<irs::payload>(*next_it);
      ASSERT_NE(nullptr, next_payload);
      EXPECT_EQ(str, irs::ref_cast<char>(next_payload->value));
      for (auto next_doc = expected_doc + 1; next_doc < MAX; ++next_doc) {
        ASSERT_TRUE(next_it->next());
        ASSERT_EQ(next_doc, next_it->value());
        const auto str = std::to_string(next_doc);
        EXPECT_EQ(str, irs::ref_cast<char>(next_payload->value));
      }
    }
  }
}

TEST_P(columnstore2_test_case, dense_fixed_length_column) {
  constexpr irs::doc_id_t MAX = 1000000;
  const irs::segment_meta meta("test", nullptr);
  const bool has_encryption = bool(irs::get_encryption(dir().attributes()));

  irs::flush_state state;
  state.doc_count = MAX;
  state.name = meta.name;

  {
    irs::columnstore2::writer writer(this->consolidation());
    writer.prepare(dir(), meta);

    auto [id, column] = writer.push_column({
      irs::type<irs::compression::none>::get(),
      {}, has_encryption });

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
      auto& stream = column(doc);
      const auto str = std::to_string(doc);
      stream.write_bytes(reinterpret_cast<const irs::byte_type*>(&doc), sizeof doc);
    }

    ASSERT_TRUE(writer.commit(state));
  }

  {
    irs::columnstore2::reader reader;
    ASSERT_TRUE(reader.prepare(dir(), meta));
    ASSERT_EQ(1, reader.size());

    auto* header = reader.header(0);
    ASSERT_NE(nullptr, header);
    ASSERT_EQ(MAX, header->docs_count);
    ASSERT_EQ(0, header->docs_index);
    ASSERT_EQ(irs::doc_limits::min(), header->min);
    ASSERT_EQ(
      this->consolidation()
        ? irs::columnstore2::ColumnType::DENSE_FIXED
        : irs::columnstore2::ColumnType::FIXED,
      header->type);
    ASSERT_EQ(has_encryption ? irs::columnstore2::ColumnProperty::ENCRYPT
                             : irs::columnstore2::ColumnProperty::NORMAL,
              header->props);

    auto* column = reader.column(0);
    ASSERT_NE(nullptr, column);
    ASSERT_EQ(MAX, column->size());

    {
      auto it = column->iterator();
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(sizeof doc, payload->value.size());
        const irs::doc_id_t actual_doc = *reinterpret_cast<const irs::doc_id_t*>(payload->value.c_str());
        EXPECT_EQ(doc, actual_doc);
      }
    }

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
      auto it = column->iterator();
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());

      ASSERT_EQ(doc, it->seek(doc));
      ASSERT_EQ(doc, it->seek(doc));
      ASSERT_EQ(sizeof doc, payload->value.size());
      const irs::doc_id_t actual_doc = *reinterpret_cast<const irs::doc_id_t*>(payload->value.c_str());
      EXPECT_EQ(doc, actual_doc);
    }

    // seek + next
    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; doc += 10000) {
      auto it = column->iterator();
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());

      ASSERT_EQ(doc, it->seek(doc));
      ASSERT_EQ(sizeof doc, payload->value.size());
      EXPECT_EQ(doc, *reinterpret_cast<const irs::doc_id_t*>(payload->value.c_str()));
      ASSERT_EQ(doc, it->seek(doc));
      ASSERT_EQ(sizeof doc, payload->value.size());
      EXPECT_EQ(doc, *reinterpret_cast<const irs::doc_id_t*>(payload->value.c_str()));
      ASSERT_EQ(doc, it->seek(doc-1));
      ASSERT_EQ(sizeof doc, payload->value.size());
      EXPECT_EQ(doc, *reinterpret_cast<const irs::doc_id_t*>(payload->value.c_str()));

      auto next_it = column->iterator();
      auto* next_payload = irs::get<irs::payload>(*next_it);
      ASSERT_NE(nullptr, next_payload);
      ASSERT_EQ(doc, next_it->seek(doc));
      ASSERT_EQ(sizeof doc, next_payload->value.size());
      EXPECT_EQ(doc, *reinterpret_cast<const irs::doc_id_t*>(next_payload->value.c_str()));
      for (auto next_doc = doc + 1; next_doc < MAX; ++next_doc) {
        ASSERT_TRUE(next_it->next());
        ASSERT_EQ(next_doc, next_it->value());
        ASSERT_EQ(sizeof next_doc, next_payload->value.size());
        EXPECT_EQ(next_doc, *reinterpret_cast<const irs::doc_id_t*>(next_payload->value.c_str()));
      }
    }

    // next + seek
    {
      auto it = column->iterator();
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      ASSERT_TRUE(it->next());
      ASSERT_EQ(irs::doc_limits::min(), document->value);
      ASSERT_EQ(118774, it->seek(118774));
      ASSERT_EQ(sizeof(irs::doc_id_t), payload->value.size());
      EXPECT_EQ(118774, *reinterpret_cast<const irs::doc_id_t*>(payload->value.c_str()));
      ASSERT_TRUE(irs::doc_limits::eof(it->seek(MAX + 1)));
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
  columnstore2_test,
  columnstore2_test_case,
  ::testing::Combine(
    ::testing::Values(
      &tests::memory_directory,
      &tests::fs_directory,
      &tests::mmap_directory,
      &tests::rot13_cipher_directory<&tests::memory_directory, 16>,
      &tests::rot13_cipher_directory<&tests::fs_directory, 16>,
      &tests::rot13_cipher_directory<&tests::mmap_directory, 16>),
    ::testing::Values(false, true)),
  &columnstore2_test_case::to_string
);
