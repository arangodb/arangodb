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
#include "search/score.hpp"

using namespace irs::columnstore2;

class columnstore2_test_case : public virtual tests::directory_test_case_base<bool> {
 public:
  static std::string to_string(
      const testing::TestParamInfo<std::tuple<tests::dir_param_f, bool>>& info) {
    auto [factory, consolidation] = info.param;

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

  auto finalizer = [](auto&){
    // Must not be called
    EXPECT_FALSE(true);
    return irs::string_ref::NIL;
  };

  irs::columnstore2::writer writer(this->consolidation());
  writer.prepare(dir(), meta);
  writer.push_column({ irs::type<irs::compression::none>::get(), {}, false }, finalizer);
  writer.push_column({ irs::type<irs::compression::none>::get(), {}, false }, finalizer);
  ASSERT_FALSE(writer.commit(state));

  irs::columnstore2::reader reader;
  ASSERT_FALSE(reader.prepare(dir(), meta));
}

TEST_P(columnstore2_test_case, empty_column) {
  constexpr irs::doc_id_t MAX = 1;
  const irs::segment_meta meta("test", nullptr);
  const bool has_encryption = bool(dir().attributes().encryption());

  irs::flush_state state;
  state.doc_count = MAX;
  state.name = meta.name;

  const irs::column_info info{
      irs::type<irs::compression::none>::get(),
      {}, has_encryption };

  irs::columnstore2::writer writer(this->consolidation());
  writer.prepare(dir(), meta);
  [[maybe_unused]] auto [id0, handle0] = writer.push_column(
      info,
      [](irs::bstring& out) {
          EXPECT_TRUE(out.empty());
          out += 1;
          return "foobar";
      });
  [[maybe_unused]] auto [id1, handle1] = writer.push_column(
      info,
      [](irs::bstring& out) {
          EXPECT_TRUE(out.empty());
          out += 2;
          return irs::string_ref::NIL;
      });
  [[maybe_unused]] auto [id2, handle2] = writer.push_column(
      info,
      [](auto&) {
          // Must no be called
          EXPECT_TRUE(false);
          return irs::string_ref::NIL;
      });
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
    ASSERT_EQ(ColumnType::kMask, header->type);
    ASSERT_EQ(has_encryption ? ColumnProperty::kEncrypt
                             : ColumnProperty::kNormal,
              header->props);

    auto column = reader.column(0);
    ASSERT_NE(nullptr, column);
    ASSERT_EQ(0, column->id());
    ASSERT_EQ("foobar", column->name());
    ASSERT_EQ(0, column->size());
    const auto header_payload = column->payload();
    ASSERT_EQ(1, header_payload.size());
    ASSERT_EQ(1, header_payload[0]);
    auto it = column->iterator(consolidation());
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
    ASSERT_EQ(ColumnType::kSparse, header->type); // FIXME why sparse?
    ASSERT_EQ(has_encryption ? (ColumnProperty::kEncrypt | ColumnProperty::kNoName)
                             : ColumnProperty::kNoName,
              header->props);

    auto column = reader.column(1);
    ASSERT_NE(nullptr, column);
    ASSERT_EQ(1, column->id());
    ASSERT_TRUE(column->name().null());
    ASSERT_EQ(1, column->size());
    const auto header_payload = column->payload();
    ASSERT_EQ(1, header_payload.size());
    ASSERT_EQ(2, header_payload[0]);
    auto it = column->iterator(consolidation());
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
  const bool has_encryption = bool(dir().attributes().encryption());

  irs::flush_state state;
  state.doc_count = MAX;
  state.name = meta.name;

  {
    irs::columnstore2::writer writer(this->consolidation());
    writer.prepare(dir(), meta);

    auto [id, column] = writer.push_column(
        { irs::type<irs::compression::none>::get(), {}, has_encryption},
        [](irs::bstring& out) {
            EXPECT_TRUE(out.empty());
            out += 42;
            return irs::string_ref::NIL;
        });

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
    ASSERT_EQ(ColumnType::kMask, header->type);
    ASSERT_EQ(has_encryption ? (ColumnProperty::kEncrypt | ColumnProperty::kNoName)
                             : ColumnProperty::kNoName,
              header->props);

    auto* column = reader.column(0);
    ASSERT_NE(nullptr, column);
    ASSERT_EQ(MAX/2, column->size());
    ASSERT_EQ(0, column->id());
    ASSERT_TRUE(column->name().null());

    const auto header_payload = column->payload();
    ASSERT_EQ(1, header_payload.size());
    ASSERT_EQ(42, header_payload[0]);

    // seek stateful
    {
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_EQ(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; doc += 2) {
        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(doc, it->seek(doc));
      }
    }

    // seek stateless
    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; doc += 2) {
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_EQ(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());

      ASSERT_EQ(doc, it->seek(doc));
      ASSERT_EQ(doc, it->seek(doc));
    }

    // seek + next
    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; doc += 5000) {
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_EQ(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());

      ASSERT_EQ(doc, it->seek(doc));
      ASSERT_EQ(doc, it->seek(doc));
      ASSERT_EQ(doc, it->seek(doc-1));

      auto next_it = column->iterator(consolidation());
      ASSERT_EQ(doc, next_it->seek(doc));
      for (auto next_doc = doc + 2; next_doc <= MAX; next_doc += 2) {
        ASSERT_TRUE(next_it->next());
        ASSERT_EQ(next_doc, next_it->value());
      }
    }

    // next + seek
    {
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_EQ(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());
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
  const bool has_encryption = bool(dir().attributes().encryption());

  irs::flush_state state;
  state.doc_count = MAX;
  state.name = meta.name;

  {
    irs::columnstore2::writer writer(this->consolidation());
    writer.prepare(dir(), meta);

    auto [id, column] = writer.push_column(
        { irs::type<irs::compression::none>::get(), {}, has_encryption },
        [](irs::bstring& out) {
            EXPECT_TRUE(out.empty());
            out += 42;
            return "foobaz";
        });

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
    ASSERT_EQ(ColumnType::kSparse, header->type);
    ASSERT_EQ(has_encryption ? ColumnProperty::kEncrypt
                             : ColumnProperty::kNormal,
              header->props);

    auto* column = reader.column(0);
    ASSERT_NE(nullptr, column);
    ASSERT_EQ(MAX/2, column->size());
    ASSERT_EQ(0, column->id());
    ASSERT_EQ("foobaz", column->name());

    const auto header_payload = column->payload();
    ASSERT_EQ(1, header_payload.size());
    ASSERT_EQ(42, header_payload[0]);

    {
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; doc += 2) {
        SCOPED_TRACE(doc);
        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(doc, it->seek(doc));
        const auto str = std::to_string(doc);
        ASSERT_EQ(str, irs::ref_cast<char>(payload->value));
      }
    }

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; doc += 2) {
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());

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
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());

      const auto str = std::to_string(doc);
      ASSERT_EQ(doc, it->seek(doc));
      EXPECT_EQ(str, irs::ref_cast<char>(payload->value));
      ASSERT_EQ(doc, it->seek(doc));
      EXPECT_EQ(str, irs::ref_cast<char>(payload->value));
      ASSERT_EQ(doc, it->seek(doc-1));
      EXPECT_EQ(str, irs::ref_cast<char>(payload->value));

      auto next_it = column->iterator(consolidation());
      auto* next_payload = irs::get<irs::payload>(*next_it);
      ASSERT_NE(nullptr, next_payload);
      ASSERT_EQ(doc, next_it->seek(doc));
      EXPECT_EQ(str, irs::ref_cast<char>(next_payload->value));
      for (auto next_doc = doc + 2; next_doc <= MAX; next_doc += 2) {
        ASSERT_TRUE(next_it->next());
        ASSERT_EQ(next_doc, next_it->value());
        const auto str = std::to_string(next_doc);
        EXPECT_EQ(str, irs::ref_cast<char>(next_payload->value));
      }
    }

    // next + seek
    {
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());
      ASSERT_TRUE(it->next());
      ASSERT_EQ(irs::doc_limits::min(), document->value);
      ASSERT_EQ(118775, it->seek(118774));
      const auto str = std::to_string(it->value());
      EXPECT_EQ(str, irs::ref_cast<char>(payload->value));
      ASSERT_TRUE(irs::doc_limits::eof(it->seek(MAX + 1)));
    }
  }
}

TEST_P(columnstore2_test_case, sparse_column_gap) {
  static constexpr irs::doc_id_t MAX = 500000;
  static constexpr auto BLOCK_SIZE = irs::sparse_bitmap_writer::kBlockSize;
  static constexpr auto GAP_BEGIN = ((MAX / BLOCK_SIZE) - 4) * BLOCK_SIZE;
  const irs::segment_meta meta("test", nullptr);
  const bool has_encryption = bool(dir().attributes().encryption());

  irs::flush_state state;
  state.doc_count = MAX;
  state.name = meta.name;

  {
    irs::columnstore2::writer writer(this->consolidation());
    writer.prepare(dir(), meta);

    auto [id, column] = writer.push_column(
        { irs::type<irs::compression::none>::get(), {}, has_encryption },
        [](irs::bstring& out) {
            EXPECT_TRUE(out.empty());
            out += 42;
            return "foobarbaz";
        });

    auto write_payload = [](irs::doc_id_t doc, irs::data_output& stream) {
      if (doc <= GAP_BEGIN || doc > (GAP_BEGIN + BLOCK_SIZE)) {
        stream.write_bytes(reinterpret_cast<const irs::byte_type*>(&doc), sizeof doc);
      }
    };

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
      write_payload(doc, column(doc));
    }

    ASSERT_TRUE(writer.commit(state));
  }

  {
    auto assert_payload = [](irs::doc_id_t doc, irs::bytes_ref payload) {
      SCOPED_TRACE(doc);
      if (doc <= GAP_BEGIN || doc > (GAP_BEGIN + BLOCK_SIZE)) {
        ASSERT_EQ(sizeof doc, payload.size());
        const irs::doc_id_t actual_doc
          = *reinterpret_cast<const irs::doc_id_t*>(payload.c_str());
        ASSERT_EQ(doc, actual_doc);
      } else {
        ASSERT_TRUE(payload.empty());
      }
    };

    irs::columnstore2::reader reader;
    ASSERT_TRUE(reader.prepare(dir(), meta));
    ASSERT_EQ(1, reader.size());

    auto* header = reader.header(0);
    ASSERT_NE(nullptr, header);
    ASSERT_EQ(MAX, header->docs_count);
    ASSERT_EQ(0, header->docs_index);
    ASSERT_EQ(irs::doc_limits::min(), header->min);
    ASSERT_EQ(ColumnType::kSparse, header->type);
    ASSERT_EQ(has_encryption ? ColumnProperty::kEncrypt
                             : ColumnProperty::kNormal,
              header->props);

    auto* column = reader.column(0);
    ASSERT_NE(nullptr, column);
    ASSERT_EQ(MAX, column->size());

    ASSERT_EQ(0, column->id());
    ASSERT_EQ("foobarbaz", column->name());

    const auto header_payload = column->payload();
    ASSERT_EQ(1, header_payload.size());
    ASSERT_EQ(42, header_payload[0]);

    {
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
        SCOPED_TRACE(doc);
        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(doc, it->seek(doc));
        assert_payload(doc, payload->value);
      }
    }

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());

      const auto str = std::to_string(doc);
      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
      ASSERT_EQ(doc, it->seek(doc-1));
      assert_payload(doc, payload->value);
    }

    // seek + next
    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; doc += 5000) {
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());

      const auto str = std::to_string(doc);
      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
      ASSERT_EQ(doc, it->seek(doc-1));
      assert_payload(doc, payload->value);

      auto next_it = column->iterator(consolidation());
      auto* next_payload = irs::get<irs::payload>(*next_it);
      ASSERT_NE(nullptr, next_payload);
      ASSERT_EQ(doc, next_it->seek(doc));
      assert_payload(doc, next_payload->value);
      for (auto next_doc = doc + 1; next_doc <= MAX; ++next_doc) {
        ASSERT_TRUE(next_it->next());
        ASSERT_EQ(next_doc, next_it->value());
        assert_payload(next_doc, next_payload->value);
      }
    }

    // next + seek
    {
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());
      ASSERT_TRUE(it->next());
      ASSERT_EQ(irs::doc_limits::min(), document->value);
      ASSERT_EQ(118775, it->seek(118775));
      assert_payload(118775, payload->value);
      ASSERT_TRUE(irs::doc_limits::eof(it->seek(MAX + 1)));
    }
  }
}

TEST_P(columnstore2_test_case, sparse_column_tail_block) {
  static constexpr irs::doc_id_t MAX = 500000;
  static constexpr auto BLOCK_SIZE = irs::sparse_bitmap_writer::kBlockSize;
  static constexpr auto TAIL_BEGIN = (MAX / BLOCK_SIZE) * BLOCK_SIZE;
  const irs::segment_meta meta("test", nullptr);
  const bool has_encryption = bool(dir().attributes().encryption());

  irs::flush_state state;
  state.doc_count = MAX;
  state.name = meta.name;

  {
    auto write_payload = [](irs::doc_id_t doc, irs::data_output& stream) {
      stream.write_bytes(reinterpret_cast<const irs::byte_type*>(&doc), sizeof doc);
      if (doc > TAIL_BEGIN) {
        stream.write_bytes(reinterpret_cast<const irs::byte_type*>(&doc), sizeof doc);
      }
    };

    irs::columnstore2::writer writer(this->consolidation());
    writer.prepare(dir(), meta);

    auto [id, column] = writer.push_column(
        { irs::type<irs::compression::none>::get(), {}, has_encryption },
        [](irs::bstring& out) {
            EXPECT_TRUE(out.empty());
            out += 42;
            return irs::string_ref::NIL;
        });

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
      write_payload(doc, column(doc));
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
    ASSERT_EQ(ColumnType::kSparse, header->type);
    ASSERT_EQ(has_encryption ? (ColumnProperty::kEncrypt | ColumnProperty::kNoName)
                             : ColumnProperty::kNoName,
              header->props);

    auto* column = reader.column(0);
    ASSERT_NE(nullptr, column);
    ASSERT_EQ(MAX, column->size());

    ASSERT_EQ(0, column->id());
    ASSERT_TRUE(column->name().null());

    const auto header_payload = column->payload();
    ASSERT_EQ(1, header_payload.size());
    ASSERT_EQ(42, header_payload[0]);

    auto assert_payload = [](irs::doc_id_t doc, irs::bytes_ref payload) {
      SCOPED_TRACE(doc);
      ASSERT_FALSE(payload.empty());
      if (doc > TAIL_BEGIN) {
        ASSERT_EQ(2*sizeof doc, payload.size());
        const irs::doc_id_t* actual_doc
          = reinterpret_cast<const irs::doc_id_t*>(payload.c_str());
        ASSERT_EQ(doc, actual_doc[0]);
        ASSERT_EQ(doc, actual_doc[1]);
      } else {
        ASSERT_EQ(sizeof doc, payload.size());
        const irs::doc_id_t actual_doc
          = *reinterpret_cast<const irs::doc_id_t*>(payload.c_str());
        ASSERT_EQ(doc, actual_doc);
      }
    };

    {
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(doc, it->seek(doc));
        assert_payload(doc, payload->value);
      }
    }

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());

      ASSERT_EQ(doc, it->seek(doc));
      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
    }

    // seek + next
    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; doc += 10000) {
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());

      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
      ASSERT_EQ(doc, it->seek(doc-1));
      assert_payload(doc, payload->value);

      auto next_it = column->iterator(consolidation());
      auto* next_payload = irs::get<irs::payload>(*next_it);
      ASSERT_NE(nullptr, next_payload);
      ASSERT_EQ(doc, next_it->seek(doc));
      assert_payload(doc, next_payload->value);
      for (auto next_doc = doc + 1; next_doc <= MAX; ++next_doc) {
        ASSERT_TRUE(next_it->next());
        assert_payload(next_doc, next_payload->value);
      }
    }

    // next + seek
    {
      constexpr irs::doc_id_t doc = 118774;

      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      ASSERT_TRUE(it->next());
      ASSERT_EQ(irs::doc_limits::min(), document->value);
      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
      ASSERT_TRUE(irs::doc_limits::eof(it->seek(MAX + 1)));
    }
  }
}

TEST_P(columnstore2_test_case, sparse_column_tail_block_last_value) {
  static constexpr irs::doc_id_t MAX = 500000;
  static constexpr auto TAIL_BEGIN = MAX - 1; // last value has different length
  const irs::segment_meta meta("test", nullptr);
  const bool has_encryption = bool(dir().attributes().encryption());

  irs::flush_state state;
  state.doc_count = MAX;
  state.name = meta.name;

  {
    auto write_payload = [](irs::doc_id_t doc, irs::data_output& stream) {
      stream.write_bytes(reinterpret_cast<const irs::byte_type*>(&doc), sizeof doc);
      if (doc > TAIL_BEGIN) {
        stream.write_byte(42);
      }
    };

    irs::columnstore2::writer writer(this->consolidation());
    writer.prepare(dir(), meta);

    auto [id, column] = writer.push_column(
        { irs::type<irs::compression::none>::get(), {}, has_encryption },
        [](irs::bstring& out) {
            EXPECT_TRUE(out.empty());
            out += 42;
            return irs::string_ref::NIL;
        });

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
      write_payload(doc, column(doc));
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
    ASSERT_EQ(ColumnType::kSparse, header->type);
    ASSERT_EQ(has_encryption ? (ColumnProperty::kEncrypt | ColumnProperty::kNoName)
                             : ColumnProperty::kNoName,
              header->props);

    auto* column = reader.column(0);
    ASSERT_NE(nullptr, column);
    ASSERT_EQ(MAX, column->size());

    ASSERT_EQ(0, column->id());
    ASSERT_TRUE(column->name().null());

    const auto header_payload = column->payload();
    ASSERT_EQ(1, header_payload.size());
    ASSERT_EQ(42, header_payload[0]);

    auto assert_payload = [](irs::doc_id_t doc, irs::bytes_ref payload) {
      SCOPED_TRACE(doc);
      ASSERT_FALSE(payload.empty());
      if (doc > TAIL_BEGIN) {
        ASSERT_EQ(1 + sizeof doc, payload.size());
        const irs::doc_id_t actual_doc
          = *reinterpret_cast<const irs::doc_id_t*>(payload.c_str());
        ASSERT_EQ(doc, actual_doc);
        ASSERT_EQ(42, payload[sizeof doc]);
      } else {
        ASSERT_EQ(sizeof doc, payload.size());
        const irs::doc_id_t actual_doc
          = *reinterpret_cast<const irs::doc_id_t*>(payload.c_str());
        ASSERT_EQ(doc, actual_doc);
      }
    };

    {
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(doc, it->seek(doc));
        assert_payload(doc, payload->value);
      }
    }

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());

      ASSERT_EQ(doc, it->seek(doc));
      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
    }

    // seek + next
    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; doc += 10000) {
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());

      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
      ASSERT_EQ(doc, it->seek(doc-1));
      assert_payload(doc, payload->value);

      auto next_it = column->iterator(consolidation());
      auto* next_payload = irs::get<irs::payload>(*next_it);
      ASSERT_NE(nullptr, next_payload);
      ASSERT_EQ(doc, next_it->seek(doc));
      assert_payload(doc, next_payload->value);
      for (auto next_doc = doc + 1; next_doc <= MAX; ++next_doc) {
        ASSERT_TRUE(next_it->next());
        assert_payload(next_doc, next_payload->value);
      }
    }

    // next + seek
    {
      constexpr irs::doc_id_t doc = 118774;

      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      ASSERT_TRUE(it->next());
      ASSERT_EQ(irs::doc_limits::min(), document->value);
      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
      ASSERT_TRUE(irs::doc_limits::eof(it->seek(MAX + 1)));
    }
  }
}

TEST_P(columnstore2_test_case, dense_mask_column) {
  constexpr irs::doc_id_t MAX = 1000000;
  const irs::segment_meta meta("test", nullptr);
  const bool has_encryption = bool(dir().attributes().encryption());

  irs::flush_state state;
  state.doc_count = MAX;
  state.name = meta.name;

  {
    irs::columnstore2::writer writer(this->consolidation());
    writer.prepare(dir(), meta);

    auto [id, column] = writer.push_column(
        { irs::type<irs::compression::none>::get(), {}, has_encryption },
        [](irs::bstring& out) {
          EXPECT_TRUE(out.empty());
          out += 42;
          return "foobar";
        });

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
    ASSERT_EQ(ColumnType::kMask, header->type);
    ASSERT_EQ(has_encryption ? ColumnProperty::kEncrypt
                             : ColumnProperty::kNormal,
              header->props);

    auto* column = reader.column(0);
    ASSERT_NE(nullptr, column);
    ASSERT_EQ(MAX, column->size());

    ASSERT_EQ(0, column->id());
    ASSERT_EQ("foobar", column->name());

    const auto header_payload = column->payload();
    ASSERT_EQ(1, header_payload.size());
    ASSERT_EQ(42, header_payload[0]);

    {
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      ASSERT_TRUE(payload->value.null());
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());

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
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      ASSERT_TRUE(payload->value.null());
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());

      ASSERT_EQ(doc, it->seek(doc));
      ASSERT_TRUE(payload->value.null());
      ASSERT_EQ(doc, it->seek(doc));
      ASSERT_TRUE(payload->value.null());
      ASSERT_EQ(doc, it->seek(doc-1));
      ASSERT_TRUE(payload->value.null());
    }

    // seek + next
    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; doc += 10000) {
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());

      ASSERT_EQ(doc, it->seek(doc));
      ASSERT_EQ(doc, it->seek(doc));

      auto next_it = column->iterator(consolidation());
      ASSERT_EQ(doc, next_it->seek(doc));
      for (auto next_doc = doc + 1; next_doc <= MAX; ++next_doc) {
        ASSERT_TRUE(next_it->next());
        ASSERT_EQ(next_doc, next_it->value());
      }
    }

    // next + seek
    {
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());
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
  const bool has_encryption = bool(dir().attributes().encryption());

  irs::flush_state state;
  state.doc_count = MAX;
  state.name = meta.name;

  {
    irs::columnstore2::writer writer(this->consolidation());
    writer.prepare(dir(), meta);

    auto [id, column] = writer.push_column(
        { irs::type<irs::compression::none>::get(), {}, has_encryption},
        [](irs::bstring& out) {
          EXPECT_TRUE(out.empty());
          out += 42;
          return "foobar";
        });

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
    ASSERT_EQ(ColumnType::kSparse, header->type);
    ASSERT_EQ(has_encryption ? ColumnProperty::kEncrypt
                             : ColumnProperty::kNormal,
              header->props);

    auto* column = reader.column(0);
    ASSERT_NE(nullptr, column);
    ASSERT_EQ(MAX, column->size());

    ASSERT_EQ(0, column->id());
    ASSERT_EQ("foobar", column->name());

    const auto header_payload = column->payload();
    ASSERT_EQ(1, header_payload.size());
    ASSERT_EQ(42, header_payload[0]);

    {
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(doc, it->seek(doc));
        const auto str = std::to_string(doc);
        EXPECT_EQ(str, irs::ref_cast<char>(payload->value));
      }
    }

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());

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
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());

      const auto str = std::to_string(doc);
      ASSERT_EQ(doc, it->seek(doc));
      EXPECT_EQ(str, irs::ref_cast<char>(payload->value));
      ASSERT_EQ(doc, it->seek(doc));
      EXPECT_EQ(str, irs::ref_cast<char>(payload->value));

      auto next_it = column->iterator(consolidation());
      auto* next_payload = irs::get<irs::payload>(*next_it);
      ASSERT_NE(nullptr, next_payload);
      ASSERT_EQ(doc, next_it->seek(doc));
      EXPECT_EQ(str, irs::ref_cast<char>(next_payload->value));
      for (auto next_doc = doc + 1; next_doc <= MAX; ++next_doc) {
        ASSERT_TRUE(next_it->next());
        ASSERT_EQ(next_doc, next_it->value());
        const auto str = std::to_string(next_doc);
        EXPECT_EQ(str, irs::ref_cast<char>(next_payload->value));
      }
    }

    // next + seek
    {
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());
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
  const bool has_encryption = bool(dir().attributes().encryption());

  irs::flush_state state;
  state.doc_count = MAX;
  state.name = meta.name;

  {
    irs::columnstore2::writer writer(this->consolidation());
    writer.prepare(dir(), meta);

    auto [id, column] = writer.push_column(
        { irs::type<irs::compression::none>::get(), {}, has_encryption},
        [](irs::bstring& out) {
          EXPECT_TRUE(out.empty());
          out += 42;
          return irs::string_ref::NIL;
        });

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
    ASSERT_EQ(ColumnType::kSparse, header->type);
    ASSERT_EQ(has_encryption ? (ColumnProperty::kEncrypt | ColumnProperty::kNoName)
                             : ColumnProperty::kNoName,
              header->props);

    auto* column = reader.column(0);
    ASSERT_NE(nullptr, column);
    ASSERT_EQ(MAX-MIN+1, column->size());

    ASSERT_EQ(0, column->id());
    ASSERT_TRUE(column->name().null());

    const auto header_payload = column->payload();
    ASSERT_EQ(1, header_payload.size());
    ASSERT_EQ(42, header_payload[0]);

    // seek before range
    {
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());

      const auto str = std::to_string(MIN);
      ASSERT_EQ(MIN, it->seek(42));
      EXPECT_EQ(str, irs::ref_cast<char>(payload->value));

      irs::doc_id_t expected_doc = MIN + 1;
      for (; expected_doc <= MAX; ++expected_doc) {
        const auto str = std::to_string(expected_doc);
        ASSERT_EQ(expected_doc, it->seek(expected_doc));
      }
      ASSERT_FALSE(it->next());
      ASSERT_TRUE(irs::doc_limits::eof(it->value()));
    }

    {
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
        const auto expected_doc = (doc <= MIN ? MIN : doc);
        const auto str = std::to_string(expected_doc);
        ASSERT_EQ(expected_doc, it->seek(doc));
        EXPECT_EQ(str, irs::ref_cast<char>(payload->value));
        ASSERT_EQ(expected_doc, it->seek(doc));
        EXPECT_EQ(str, irs::ref_cast<char>(payload->value));
        ASSERT_EQ(expected_doc, it->seek(doc-1));
        EXPECT_EQ(str, irs::ref_cast<char>(payload->value));
      }
    }

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());

      const auto expected_doc = (doc <= MIN ? MIN : doc);
      const auto str = std::to_string(expected_doc);
      ASSERT_EQ(expected_doc, it->seek(doc));
      EXPECT_EQ(str, irs::ref_cast<char>(payload->value));
      ASSERT_EQ(expected_doc, it->seek(doc));
      EXPECT_EQ(str, irs::ref_cast<char>(payload->value));
    }

    // seek + next
    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; doc += 10000) {
      auto it = column->iterator(consolidation());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->is_default());

      const auto expected_doc = (doc <= MIN ? MIN : doc);
      const auto str = std::to_string(expected_doc);
      ASSERT_EQ(expected_doc, it->seek(doc));
      EXPECT_EQ(str, irs::ref_cast<char>(payload->value));
      ASSERT_EQ(expected_doc, it->seek(doc));
      EXPECT_EQ(str, irs::ref_cast<char>(payload->value));

      auto next_it = column->iterator(consolidation());
      ASSERT_EQ(expected_doc, next_it->seek(doc));
      auto* next_payload = irs::get<irs::payload>(*next_it);
      ASSERT_NE(nullptr, next_payload);
      EXPECT_EQ(str, irs::ref_cast<char>(next_payload->value));
      for (auto next_doc = expected_doc + 1; next_doc <= MAX; ++next_doc) {
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
  const bool has_encryption = bool(dir().attributes().encryption());

  irs::flush_state state;
  state.doc_count = MAX;
  state.name = meta.name;

  {
    irs::columnstore2::writer writer(this->consolidation());
    writer.prepare(dir(), meta);

    {
      auto [id, column] = writer.push_column(
          { irs::type<irs::compression::none>::get(), {}, has_encryption },
          [](irs::bstring& out) {
            EXPECT_TRUE(out.empty());
            out += 42;
            return irs::string_ref::NIL;
          });

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
        auto& stream = column(doc);
        stream.write_bytes(reinterpret_cast<const irs::byte_type*>(&doc), sizeof doc);
      }
    }

    {
      auto [id, column] = writer.push_column(
          { irs::type<irs::compression::none>::get(), {}, has_encryption },
          [](irs::bstring& out) {
            EXPECT_TRUE(out.empty());
            out += 43;
            return irs::string_ref::NIL;
          });

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
        auto& stream = column(doc);
        stream.write_byte(static_cast<irs::byte_type>(doc & 0xFF));
      }
    }

    ASSERT_TRUE(writer.commit(state));
  }

  {
    irs::columnstore2::reader reader;
    ASSERT_TRUE(reader.prepare(dir(), meta));
    ASSERT_EQ(2, reader.size());

    {
      constexpr irs::field_id kColumnId = 0;

      auto* header = reader.header(kColumnId);
      ASSERT_NE(nullptr, header);
      ASSERT_EQ(MAX, header->docs_count);
      ASSERT_EQ(0, header->docs_index);
      ASSERT_EQ(irs::doc_limits::min(), header->min);
      ASSERT_EQ(
        this->consolidation()
          ? ColumnType::kDenseFixed
          : ColumnType::kFixed,
        header->type);
      ASSERT_EQ(has_encryption ? (ColumnProperty::kEncrypt | ColumnProperty::kNoName)
                               : ColumnProperty::kNoName,
                header->props);

      auto* column = reader.column(kColumnId);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(MAX, column->size());

      ASSERT_EQ(0, column->id());
      ASSERT_TRUE(column->name().null());

      const auto header_payload = column->payload();
      ASSERT_EQ(1, header_payload.size());
      ASSERT_EQ(42, header_payload[0]);

      {
        auto it = column->iterator(consolidation());
        auto* document = irs::get<irs::document>(*it);
        ASSERT_NE(nullptr, document);
        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_NE(nullptr, payload);
        auto* cost = irs::get<irs::cost>(*it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(column->size(), cost->estimate());
        auto* score = irs::get<irs::score>(*it);
        ASSERT_NE(nullptr, score);
        ASSERT_TRUE(score->is_default());

        for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
          ASSERT_EQ(doc, it->seek(doc));
          ASSERT_EQ(doc, it->seek(doc));
          ASSERT_EQ(sizeof doc, payload->value.size());
          const irs::doc_id_t actual_doc = *reinterpret_cast<const irs::doc_id_t*>(payload->value.c_str());
          EXPECT_EQ(doc, actual_doc);
        }
      }

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
        auto it = column->iterator(consolidation());
        auto* document = irs::get<irs::document>(*it);
        ASSERT_NE(nullptr, document);
        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_NE(nullptr, payload);
        auto* cost = irs::get<irs::cost>(*it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(column->size(), cost->estimate());
        auto* score = irs::get<irs::score>(*it);
        ASSERT_NE(nullptr, score);
        ASSERT_TRUE(score->is_default());

        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(sizeof doc, payload->value.size());
        const irs::doc_id_t actual_doc = *reinterpret_cast<const irs::doc_id_t*>(payload->value.c_str());
        EXPECT_EQ(doc, actual_doc);
      }

      // seek + next
      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; doc += 10000) {
        auto it = column->iterator(consolidation());
        auto* document = irs::get<irs::document>(*it);
        ASSERT_NE(nullptr, document);
        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_NE(nullptr, payload);
        auto* cost = irs::get<irs::cost>(*it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(column->size(), cost->estimate());
        auto* score = irs::get<irs::score>(*it);
        ASSERT_NE(nullptr, score);
        ASSERT_TRUE(score->is_default());

        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(sizeof doc, payload->value.size());
        EXPECT_EQ(doc, *reinterpret_cast<const irs::doc_id_t*>(payload->value.c_str()));
        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(sizeof doc, payload->value.size());
        EXPECT_EQ(doc, *reinterpret_cast<const irs::doc_id_t*>(payload->value.c_str()));
        ASSERT_EQ(doc, it->seek(doc-1));
        ASSERT_EQ(sizeof doc, payload->value.size());
        EXPECT_EQ(doc, *reinterpret_cast<const irs::doc_id_t*>(payload->value.c_str()));

        auto next_it = column->iterator(consolidation());
        auto* next_payload = irs::get<irs::payload>(*next_it);
        ASSERT_NE(nullptr, next_payload);
        ASSERT_EQ(doc, next_it->seek(doc));
        ASSERT_EQ(sizeof doc, next_payload->value.size());
        EXPECT_EQ(doc, *reinterpret_cast<const irs::doc_id_t*>(next_payload->value.c_str()));
        for (auto next_doc = doc + 1; next_doc <= MAX; ++next_doc) {
          ASSERT_TRUE(next_it->next());
          ASSERT_EQ(next_doc, next_it->value());
          ASSERT_EQ(sizeof next_doc, next_payload->value.size());
          ASSERT_EQ(next_doc, *reinterpret_cast<const irs::doc_id_t*>(next_payload->value.c_str()));
        }
      }

      // next + seek
      {
        auto it = column->iterator(consolidation());
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

    {
      constexpr irs::field_id kColumnId = 1;

      auto* header = reader.header(kColumnId);
      ASSERT_NE(nullptr, header);
      ASSERT_EQ(MAX, header->docs_count);
      ASSERT_EQ(0, header->docs_index);
      ASSERT_EQ(irs::doc_limits::min(), header->min);
      ASSERT_EQ(
        this->consolidation()
          ? ColumnType::kDenseFixed
          : ColumnType::kFixed,
        header->type);
      ASSERT_EQ(has_encryption ? (ColumnProperty::kEncrypt | ColumnProperty::kNoName)
                               : ColumnProperty::kNoName,
                header->props);

      auto* column = reader.column(kColumnId);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(MAX, column->size());

      ASSERT_EQ(1, column->id());
      ASSERT_TRUE(column->name().null());

      const auto header_payload = column->payload();
      ASSERT_EQ(1, header_payload.size());
      ASSERT_EQ(43, header_payload[0]);

      {
        auto it = column->iterator(consolidation());
        auto* document = irs::get<irs::document>(*it);
        ASSERT_NE(nullptr, document);
        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_NE(nullptr, payload);
        auto* cost = irs::get<irs::cost>(*it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(column->size(), cost->estimate());
        auto* score = irs::get<irs::score>(*it);
        ASSERT_NE(nullptr, score);
        ASSERT_TRUE(score->is_default());

        for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
          ASSERT_EQ(doc, it->seek(doc));
          ASSERT_EQ(doc, it->seek(doc));
          ASSERT_EQ(1, payload->value.size());
          EXPECT_EQ(static_cast<irs::byte_type>(doc & 0xFF), payload->value[0]);
        }
      }

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
        auto it = column->iterator(consolidation());
        auto* document = irs::get<irs::document>(*it);
        ASSERT_NE(nullptr, document);
        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_NE(nullptr, payload);
        auto* cost = irs::get<irs::cost>(*it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(column->size(), cost->estimate());
        auto* score = irs::get<irs::score>(*it);
        ASSERT_NE(nullptr, score);
        ASSERT_TRUE(score->is_default());

        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(1, payload->value.size());
        EXPECT_EQ(static_cast<irs::byte_type>(doc & 0xFF), payload->value[0]);
      }

      // seek + next
      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; doc += 10000) {
        auto it = column->iterator(consolidation());
        auto* document = irs::get<irs::document>(*it);
        ASSERT_NE(nullptr, document);
        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_NE(nullptr, payload);
        auto* cost = irs::get<irs::cost>(*it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(column->size(), cost->estimate());
        auto* score = irs::get<irs::score>(*it);
        ASSERT_NE(nullptr, score);
        ASSERT_TRUE(score->is_default());

        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(1, payload->value.size());
        EXPECT_EQ(static_cast<irs::byte_type>(doc & 0xFF), payload->value[0]);
        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(1, payload->value.size());
        EXPECT_EQ(static_cast<irs::byte_type>(doc & 0xFF), payload->value[0]);
        ASSERT_EQ(doc, it->seek(doc-1));
        ASSERT_EQ(1, payload->value.size());
        EXPECT_EQ(static_cast<irs::byte_type>(doc & 0xFF), payload->value[0]);

        auto next_it = column->iterator(consolidation());
        auto* next_payload = irs::get<irs::payload>(*next_it);
        ASSERT_NE(nullptr, next_payload);
        ASSERT_EQ(doc, next_it->seek(doc));
        ASSERT_EQ(1, next_payload->value.size());
        EXPECT_EQ(static_cast<irs::byte_type>(doc & 0xFF), next_payload->value[0]);
        for (auto next_doc = doc + 1; next_doc <= MAX; ++next_doc) {
          ASSERT_TRUE(next_it->next());
          ASSERT_EQ(next_doc, next_it->value());
          ASSERT_EQ(1, next_payload->value.size());
          EXPECT_EQ(static_cast<irs::byte_type>(next_doc & 0xFF), next_payload->value[0]);
        }
      }

      // next + seek
      {
        auto it = column->iterator(consolidation());
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
        ASSERT_EQ(1, payload->value.size());
        EXPECT_EQ(static_cast<irs::byte_type>(118774 & 0xFF), payload->value[0]);
        ASSERT_TRUE(irs::doc_limits::eof(it->seek(MAX + 1)));
      }
    }
  }
}

TEST_P(columnstore2_test_case, dense_fixed_length_column_empty_tail) {
  constexpr irs::doc_id_t MAX = 1000000;
  const irs::segment_meta meta("test", nullptr);
  const bool has_encryption = bool(dir().attributes().encryption());

  irs::flush_state state;
  state.doc_count = MAX;
  state.name = meta.name;

  {
    irs::columnstore2::writer writer(this->consolidation());
    writer.prepare(dir(), meta);

    {
      auto [id, column] = writer.push_column(
          { irs::type<irs::compression::none>::get(), {}, has_encryption },
          [](irs::bstring& out) {
            EXPECT_TRUE(out.empty());
            out += 42;
            return irs::string_ref::NIL;
          });

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
        auto& stream = column(doc);
        stream.write_bytes(reinterpret_cast<const irs::byte_type*>(&doc), sizeof doc);
      }
    }

    {
      // empty column has to be removed
      auto [id, column] = writer.push_column(
          { irs::type<irs::compression::none>::get(), {}, has_encryption },
          [](auto&) {
            // Must not be called
            EXPECT_FALSE(true);
            return irs::string_ref::NIL;
          });
    }

    ASSERT_TRUE(writer.commit(state));
  }

  {
    irs::columnstore2::reader reader;
    ASSERT_TRUE(reader.prepare(dir(), meta));
    ASSERT_EQ(1, reader.size());

    {
      constexpr irs::field_id kColumnId = 0;

      auto* header = reader.header(kColumnId);
      ASSERT_NE(nullptr, header);
      ASSERT_EQ(MAX, header->docs_count);
      ASSERT_EQ(0, header->docs_index);
      ASSERT_EQ(irs::doc_limits::min(), header->min);
      ASSERT_EQ(
        this->consolidation()
          ? ColumnType::kDenseFixed
          : ColumnType::kFixed,
        header->type);
      ASSERT_EQ(has_encryption ? (ColumnProperty::kEncrypt | ColumnProperty::kNoName)
                               : ColumnProperty::kNoName,
                header->props);

      auto* column = reader.column(kColumnId);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(MAX, column->size());

      ASSERT_EQ(0, column->id());
      ASSERT_TRUE(column->name().null());

      const auto header_payload = column->payload();
      ASSERT_EQ(1, header_payload.size());
      ASSERT_EQ(42, header_payload[0]);

      {
        auto it = column->iterator(consolidation());
        auto* document = irs::get<irs::document>(*it);
        ASSERT_NE(nullptr, document);
        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_NE(nullptr, payload);
        auto* cost = irs::get<irs::cost>(*it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(column->size(), cost->estimate());
        auto* score = irs::get<irs::score>(*it);
        ASSERT_NE(nullptr, score);
        ASSERT_TRUE(score->is_default());

        for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
          ASSERT_EQ(doc, it->seek(doc));
          ASSERT_EQ(doc, it->seek(doc));
          ASSERT_EQ(sizeof doc, payload->value.size());
          const irs::doc_id_t actual_doc = *reinterpret_cast<const irs::doc_id_t*>(payload->value.c_str());
          EXPECT_EQ(doc, actual_doc);
        }
      }

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
        auto it = column->iterator(consolidation());
        auto* document = irs::get<irs::document>(*it);
        ASSERT_NE(nullptr, document);
        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_NE(nullptr, payload);
        auto* cost = irs::get<irs::cost>(*it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(column->size(), cost->estimate());
        auto* score = irs::get<irs::score>(*it);
        ASSERT_NE(nullptr, score);
        ASSERT_TRUE(score->is_default());

        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(sizeof doc, payload->value.size());
        const irs::doc_id_t actual_doc = *reinterpret_cast<const irs::doc_id_t*>(payload->value.c_str());
        EXPECT_EQ(doc, actual_doc);
      }

      // seek + next
      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; doc += 10000) {
        auto it = column->iterator(consolidation());
        auto* document = irs::get<irs::document>(*it);
        ASSERT_NE(nullptr, document);
        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_NE(nullptr, payload);
        auto* cost = irs::get<irs::cost>(*it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(column->size(), cost->estimate());
        auto* score = irs::get<irs::score>(*it);
        ASSERT_NE(nullptr, score);
        ASSERT_TRUE(score->is_default());

        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(sizeof doc, payload->value.size());
        EXPECT_EQ(doc, *reinterpret_cast<const irs::doc_id_t*>(payload->value.c_str()));
        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(sizeof doc, payload->value.size());
        EXPECT_EQ(doc, *reinterpret_cast<const irs::doc_id_t*>(payload->value.c_str()));
        ASSERT_EQ(doc, it->seek(doc-1));
        ASSERT_EQ(sizeof doc, payload->value.size());
        EXPECT_EQ(doc, *reinterpret_cast<const irs::doc_id_t*>(payload->value.c_str()));

        auto next_it = column->iterator(consolidation());
        auto* next_payload = irs::get<irs::payload>(*next_it);
        ASSERT_NE(nullptr, next_payload);
        ASSERT_EQ(doc, next_it->seek(doc));
        ASSERT_EQ(sizeof doc, next_payload->value.size());
        EXPECT_EQ(doc, *reinterpret_cast<const irs::doc_id_t*>(next_payload->value.c_str()));
        for (auto next_doc = doc + 1; next_doc <= MAX; ++next_doc) {
          ASSERT_TRUE(next_it->next());
          ASSERT_EQ(next_doc, next_it->value());
          ASSERT_EQ(sizeof next_doc, next_payload->value.size());
          ASSERT_EQ(next_doc, *reinterpret_cast<const irs::doc_id_t*>(next_payload->value.c_str()));
        }
      }

      // next + seek
      {
        auto it = column->iterator(consolidation());
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
}

TEST_P(columnstore2_test_case, empty_columns) {
  constexpr irs::doc_id_t MAX = 1000000;
  const irs::segment_meta meta("test", nullptr);
  const bool has_encryption = bool(dir().attributes().encryption());

  irs::flush_state state;
  state.doc_count = MAX;
  state.name = meta.name;

  {
    irs::columnstore2::writer writer(this->consolidation());
    writer.prepare(dir(), meta);

    {
      // empty column must be removed
      auto [id, column] = writer.push_column(
          { irs::type<irs::compression::none>::get(), {}, has_encryption },
          [](auto&) {
            // Must not be called
            EXPECT_FALSE(true);
            return irs::string_ref::NIL;
          });
    }

    {
      // empty column must be removed
      auto [id, column] = writer.push_column(
          { irs::type<irs::compression::none>::get(), {}, has_encryption },
          [](auto&) {
            // Must not be called
            EXPECT_FALSE(true);
            return irs::string_ref::NIL;
          });
    }

    ASSERT_FALSE(writer.commit(state));
  }

  size_t count = 0;
  ASSERT_TRUE(dir().visit([&count](auto&) {
    ++count;
    return false; }));

  ASSERT_EQ(0, count);
}

INSTANTIATE_TEST_SUITE_P(
  columnstore2_test,
  columnstore2_test_case,
  ::testing::Combine(
    ::testing::Values(
      &tests::directory<&tests::memory_directory>,
      &tests::directory<&tests::fs_directory>,
      &tests::directory<&tests::mmap_directory>,
      &tests::rot13_directory<&tests::memory_directory, 16>,
      &tests::rot13_directory<&tests::fs_directory, 16>,
      &tests::rot13_directory<&tests::mmap_directory, 16>),
    ::testing::Values(false, true)),
  &columnstore2_test_case::to_string
);
