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

#include "formats/columnstore2.hpp"

#include "search/score.hpp"
#include "tests_param.hpp"
#include "tests_shared.hpp"

using namespace irs::columnstore2;

static TestResourceManager kDummy;

class columnstore2_test_case
  : public virtual tests::directory_test_case_base<
      irs::ColumnHint, irs::columnstore2::Version, bool> {
 public:
  static std::string to_string(const testing::TestParamInfo<ParamType>& info) {
    auto [factory, hint, version, buffered] = info.param;

    std::string name = (*factory)(nullptr).second;

    switch (hint) {
      case irs::ColumnHint::kNormal:
        break;
      case irs::ColumnHint::kConsolidation:
        name += "_consolidation";
        break;
      case irs::ColumnHint::kMask:
        name += "_mask";
        break;
      case irs::ColumnHint::kPrevDoc:
        name += "_prev";
        break;
      default:
        EXPECT_FALSE(true);
        break;
    }

    if (buffered) {
      name += "_buffered";
    }

    return name + "_" + std::to_string(static_cast<uint32_t>(version));
  }

  bool buffered() const noexcept {
    auto& p = this->GetParam();
    return std::get<3>(p);
  }

  bool consolidation() const noexcept {
    return hint() == irs::ColumnHint::kConsolidation;
  }

  irs::columnstore_reader::options reader_options(
    TestResourceManager& mng = kDummy) {
    irs::columnstore_reader::options options{.resource_manager{mng.options}};
    options.warmup_column = [this](const irs::column_reader&) {
      return this->buffered();
    };
    return options;
  }

  irs::ColumnInfo column_info() const noexcept {
    return {.compression = irs::type<irs::compression::none>::get(),
            .options = {},
            .encryption = has_encryption(),
            .track_prev_doc = has_prev_doc()};
  }

  bool has_encryption() const noexcept {
    return nullptr != dir().attributes().encryption();
  }

  bool has_payload() const noexcept {
    return irs::ColumnHint::kNormal == (hint() & irs::ColumnHint::kMask);
  }

  bool has_prev_doc() const noexcept {
    return irs::ColumnHint::kPrevDoc == (hint() & irs::ColumnHint::kPrevDoc);
  }

  ColumnProperty column_property(ColumnProperty base_props) const noexcept {
    if (has_encryption()) {
      base_props |= ColumnProperty::kEncrypt;
    }
    if (has_prev_doc()) {
      base_props |= ColumnProperty::kPrevDoc;
    }
    return base_props;
  }

  irs::columnstore2::Version version() const noexcept {
    auto& p = this->GetParam();
    return std::get<irs::columnstore2::Version>(p);
  }

  irs::ColumnHint hint() const noexcept {
    auto& p = this->GetParam();
    return std::get<irs::ColumnHint>(p);
  }

  void assert_prev_doc(irs::doc_iterator& it, irs::doc_iterator& prev_it) {
    auto prev_doc = [](irs::doc_iterator& it, irs::doc_id_t target) {
      auto doc = it.value();
      auto prev = 0;
      while (doc < target && it.next()) {
        prev = doc;
        doc = it.value();
      }
      return prev;
    };

    auto* prev = irs::get<irs::prev_doc>(it);
    ASSERT_EQ(has_prev_doc(), nullptr != prev && nullptr != *prev);
    if (prev && *prev) {
      ASSERT_EQ(prev_doc(prev_it, it.value()), (*prev)());
    }
  }
};

TEST_P(columnstore2_test_case, reader_ctor) {
  irs::columnstore2::reader reader;
  ASSERT_EQ(0, reader.size());
  ASSERT_EQ(nullptr, reader.column(0));
  ASSERT_EQ(nullptr, reader.header(0));
}

TEST_P(columnstore2_test_case, empty_columnstore) {
  constexpr irs::doc_id_t kMax = 1;
  irs::SegmentMeta meta;
  meta.name = "test";

  irs::flush_state state{
    .name = meta.name,
    .doc_count = kMax,
  };

  auto finalizer = [](auto&) {
    // Must not be called
    EXPECT_FALSE(true);
    return std::string_view{};
  };
  TestResourceManager memory;
  {
    irs::columnstore2::writer writer(version(), *memory.options.transactions,
                                     consolidation());
    writer.prepare(dir(), meta);
    const auto pinned = memory.transactions.counter_;
    ASSERT_GT(pinned, 0);
    writer.push_column({irs::type<irs::compression::none>::get(), {}, false},
                       finalizer);
    writer.push_column({irs::type<irs::compression::none>::get(), {}, false},
                       finalizer);
    const auto pinned2 = memory.transactions.counter_;

    ASSERT_GT(pinned2, pinned);
    ASSERT_FALSE(writer.commit(state));
    const auto pinned3 = memory.transactions.counter_;
    // we do not release columns memory from vector.
    // but that maybe makes no sense as in real application
    // writer would be terminated anyway.
    ASSERT_LE(pinned3, pinned2);
    irs::columnstore2::reader reader;
    ASSERT_FALSE(reader.prepare(dir(), meta, reader_options(memory)));
    // empty columnstore - no readers to allocate
    ASSERT_EQ(memory.readers.counter_, 0);
  }
  ASSERT_EQ(memory.transactions.counter_, 0);
}

TEST_P(columnstore2_test_case, empty_column) {
  constexpr irs::doc_id_t kMax = 1;
  irs::SegmentMeta meta;
  meta.name = "test";

  irs::flush_state state{
    .name = meta.name,
    .doc_count = kMax,
  };
  TestResourceManager memory;
  {
    irs::columnstore2::writer writer(version(), *memory.options.transactions,
                                     consolidation());
    writer.prepare(dir(), meta);
    [[maybe_unused]] auto [id0, handle0] =
      writer.push_column(column_info(), [](irs::bstring& out) {
        EXPECT_TRUE(out.empty());
        out += 1;
        return "foobar";
      });
    [[maybe_unused]] auto [id1, handle1] =
      writer.push_column(column_info(), [](irs::bstring& out) {
        EXPECT_TRUE(out.empty());
        out += 2;
        return std::string_view{};
      });
    [[maybe_unused]] auto [id2, handle2] =
      writer.push_column(column_info(), [](auto&) {
        // Must no be called
        EXPECT_TRUE(false);
        return std::string_view{};
      });
    handle1(42).write_byte(42);
    const auto pinned = memory.transactions.counter_;
    ASSERT_GT(pinned, 0);
    ASSERT_TRUE(writer.commit(state));
  }
  {
    irs::columnstore2::reader reader;
    ASSERT_TRUE(reader.prepare(dir(), meta, reader_options(memory)));
    ASSERT_EQ(2, reader.size());
    ASSERT_GT(memory.readers.counter_, 0);
    // column 0
    {
      auto* header = reader.header(0);
      ASSERT_NE(nullptr, header);
      ASSERT_EQ(0, header->docs_count);
      ASSERT_EQ(0, header->docs_index);
      ASSERT_EQ(irs::doc_limits::invalid(), header->min);
      ASSERT_EQ(ColumnType::kMask, header->type);
      ASSERT_EQ(column_property(ColumnProperty::kNormal), header->props);

      auto column = reader.column(0);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(0, column->id());
      ASSERT_EQ("foobar", column->name());
      ASSERT_EQ(0, column->size());
      const auto header_payload = column->payload();
      ASSERT_EQ(1, header_payload.size());
      ASSERT_EQ(1, header_payload[0]);
      auto it = column->iterator(hint());
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
      ASSERT_EQ(ColumnType::kSparse, header->type);  // FIXME why sparse?
      ASSERT_EQ(column_property(ColumnProperty::kNoName), header->props);

      auto column = reader.column(1);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(1, column->id());
      ASSERT_TRUE(irs::IsNull(column->name()));
      ASSERT_EQ(1, column->size());
      const auto header_payload = column->payload();
      ASSERT_EQ(1, header_payload.size());
      ASSERT_EQ(2, header_payload[0]);
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      auto* prev = irs::get<irs::prev_doc>(*it);
      ASSERT_EQ(has_prev_doc(), prev && *prev);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      ASSERT_NE(nullptr, it);
      ASSERT_FALSE(irs::doc_limits::valid(it->value()));
      ASSERT_TRUE(it->next());
      if (prev && *prev) {
        ASSERT_EQ(0, (*prev)());
      }
      ASSERT_EQ(42, it->value());
      if (has_payload()) {
        ASSERT_EQ(1, payload->value.size());
        ASSERT_EQ(42, payload->value[0]);
      } else {
        ASSERT_TRUE(payload->value.empty());
      }
      ASSERT_FALSE(it->next());
      ASSERT_FALSE(it->next());
    }
  }
  ASSERT_EQ(memory.readers.counter_, 0);
}

TEST_P(columnstore2_test_case, sparse_mask_column) {
  constexpr irs::doc_id_t kMax = 1000000;
  irs::SegmentMeta meta;
  meta.name = "test";

  irs::flush_state state{
    .name = meta.name,
    .doc_count = kMax,
  };
  TestResourceManager memory;
  {
    irs::columnstore2::writer writer(version(), *memory.options.transactions,
                                     consolidation());
    writer.prepare(dir(), meta);

    auto [id, column] =
      writer.push_column(column_info(), [](irs::bstring& out) {
        EXPECT_TRUE(out.empty());
        out += 42;
        return std::string_view{};
      });
    ASSERT_GT(memory.transactions.counter_, 0);
    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; doc += 2) {
      column(doc);
    }

    ASSERT_TRUE(writer.commit(state));
  }
  ASSERT_EQ(memory.transactions.counter_, 0);
  {
    irs::columnstore2::reader reader;
    ASSERT_TRUE(reader.prepare(dir(), meta, reader_options(memory)));
    ASSERT_EQ(1, reader.size());
    ASSERT_GT(memory.readers.counter_, 0);
    auto* header = reader.header(0);
    ASSERT_NE(nullptr, header);
    ASSERT_EQ(kMax / 2, header->docs_count);
    ASSERT_NE(0, header->docs_index);
    ASSERT_EQ(irs::doc_limits::min(), header->min);
    ASSERT_EQ(ColumnType::kMask, header->type);
    ASSERT_EQ(column_property(ColumnProperty::kNoName), header->props);

    auto* column = reader.column(0);
    ASSERT_NE(nullptr, column);
    ASSERT_EQ(kMax / 2, column->size());
    ASSERT_EQ(0, column->id());
    ASSERT_TRUE(irs::IsNull(column->name()));

    const auto header_payload = column->payload();
    ASSERT_EQ(1, header_payload.size());
    ASSERT_EQ(42, header_payload[0]);

    // seek stateful
    {
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_EQ(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; doc += 2) {
        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(doc, it->seek(doc));
      }
    }

    // seek stateless
    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; doc += 2) {
      auto it = column->iterator(hint());
      auto* prev = irs::get<irs::prev_doc>(*it);
      ASSERT_EQ(has_prev_doc(), prev && *prev);
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_EQ(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

      ASSERT_EQ(doc, it->seek(doc));
      ASSERT_EQ(doc, it->seek(doc));
    }

    // seek + next
    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; doc += 5000) {
      auto prev_it = column->iterator(hint());
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_EQ(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

      ASSERT_EQ(doc, it->seek(doc));
      ASSERT_EQ(doc, it->seek(doc));
      ASSERT_EQ(doc, it->seek(doc - 1));

      assert_prev_doc(*it, *prev_it);

      auto next_it = column->iterator(hint());
      auto* prev = irs::get<irs::prev_doc>(*next_it);
      ASSERT_EQ(has_prev_doc(), nullptr != prev && nullptr != *prev);
      ASSERT_EQ(doc, next_it->seek(doc));
      for (auto next_doc = doc + 2; next_doc <= kMax; next_doc += 2) {
        ASSERT_TRUE(next_it->next());
        ASSERT_EQ(next_doc, next_it->value());
        assert_prev_doc(*next_it, *prev_it);
      }
    }

    // next + seek
    {
      auto prev_it = column->iterator(hint());
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_EQ(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_TRUE(it->next());
      assert_prev_doc(*it, *prev_it);
      ASSERT_EQ(irs::doc_limits::min(), document->value);
      ASSERT_EQ(118775, it->seek(118774));
      assert_prev_doc(*it, *prev_it);
      ASSERT_TRUE(irs::doc_limits::eof(it->seek(kMax + 1)));
    }
  }
  ASSERT_EQ(memory.readers.counter_, 0);
}

TEST_P(columnstore2_test_case, sparse_column_m) {
  constexpr irs::doc_id_t MAX = 5000;
  irs::SegmentMeta meta;
  meta.name = "test_m";
  const bool has_encryption = bool(dir().attributes().encryption());

  irs::flush_state state{
    .name = meta.name,
    .doc_count = MAX,
  };
  TestResourceManager mem;
  {
    irs::columnstore2::writer writer(version(), mem.transactions,
                                     consolidation());
    writer.prepare(dir(), meta);

    auto [id, column] = writer.push_column(
      {irs::type<irs::compression::none>::get(), {}, has_encryption},
      [](irs::bstring& out) {
        EXPECT_TRUE(out.empty());
        out += 42;
        return "foobaz";
      });
    ASSERT_GT(mem.transactions.counter_, 0);
    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; doc += 2) {
      auto& stream = column(doc);
      const auto str = std::to_string(doc);
      stream.write_bytes(reinterpret_cast<const irs::byte_type*>(str.c_str()),
                         str.size());
    }

    ASSERT_TRUE(writer.commit(state));
  }
  ASSERT_EQ(0, mem.transactions.counter_);
  {
    irs::columnstore2::reader reader;
    auto options = reader_options(mem);
    ASSERT_TRUE(reader.prepare(dir(), meta, options));
    ASSERT_EQ(1, reader.size());
    if (this->buffered()) {
      ASSERT_GT(mem.cached_columns.counter_, 0);
    } else {
      ASSERT_EQ(0, mem.cached_columns.counter_);
    }
    ASSERT_GT(mem.readers.counter_, 0);
    auto* header = reader.header(0);
    ASSERT_NE(nullptr, header);
    ASSERT_EQ(MAX / 2, header->docs_count);
    ASSERT_NE(0, header->docs_index);
    ASSERT_EQ(irs::doc_limits::min(), header->min);
    ASSERT_EQ(ColumnType::kSparse, header->type);
    ASSERT_EQ(
      has_encryption ? ColumnProperty::kEncrypt : ColumnProperty::kNormal,
      header->props);
  }
  ASSERT_EQ(0, mem.cached_columns.counter_);
  ASSERT_EQ(0, mem.readers.counter_);
}

TEST_P(columnstore2_test_case, sparse_column_mr) {
  constexpr irs::doc_id_t MAX = 5000;
  irs::SegmentMeta meta;
  meta.name = "test";
  const bool has_encryption = bool(dir().attributes().encryption());

  irs::flush_state state{
    .name = meta.name,
    .doc_count = MAX,
  };

  TestResourceManager memory;
  {
    irs::columnstore2::writer writer(version(), memory.transactions,
                                     consolidation());
    writer.prepare(dir(), meta);

    auto [id, column] = writer.push_column(
      {irs::type<irs::compression::none>::get(), {}, has_encryption},
      [](irs::bstring& out) {
        EXPECT_TRUE(out.empty());
        out += 42;
        return "foobaz";
      });

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; doc += 2) {
      auto& stream = column(doc);
      const auto str = std::to_string(doc);
      stream.write_bytes(reinterpret_cast<const irs::byte_type*>(str.c_str()),
                         str.size());
    }
    ASSERT_GT(memory.transactions.counter_, 0);
    ASSERT_TRUE(writer.commit(state));
  }
  ASSERT_EQ(memory.transactions.counter_, 0);
  memory.cached_columns.result_ = false;
  {
    irs::columnstore2::reader reader;
    auto options = reader_options(memory);
    ASSERT_TRUE(reader.prepare(dir(), meta, options));
    ASSERT_EQ(1, reader.size());
    if (this->buffered()) {
      // we still record an attempt of allocating
      ASSERT_GT(memory.cached_columns.counter_, 0);
    } else {
      ASSERT_EQ(0, memory.cached_columns.counter_);
    }
    ASSERT_GT(memory.readers.counter_, 0);
    auto* header = reader.header(0);
    ASSERT_NE(nullptr, header);
    ASSERT_EQ(MAX / 2, header->docs_count);
    ASSERT_NE(0, header->docs_index);
    ASSERT_EQ(irs::doc_limits::min(), header->min);
    ASSERT_EQ(ColumnType::kSparse, header->type);
    ASSERT_EQ(
      has_encryption ? ColumnProperty::kEncrypt : ColumnProperty::kNormal,
      header->props);
  }
  // should not be a deallocation
  if (this->buffered()) {
    ASSERT_GT(memory.cached_columns.counter_, 0);
  } else {
    ASSERT_EQ(0, memory.cached_columns.counter_);
  }
  ASSERT_EQ(memory.readers.counter_, 0);
}

TEST_P(columnstore2_test_case, sparse_column) {
  constexpr irs::doc_id_t kMax = 1000000;
  irs::SegmentMeta meta;
  meta.name = "test";

  irs::flush_state state{
    .name = meta.name,
    .doc_count = kMax,
  };

  {
    irs::columnstore2::writer writer(version(), irs::IResourceManager::kNoop,
                                     consolidation());
    writer.prepare(dir(), meta);

    auto [id, column] =
      writer.push_column(column_info(), [](irs::bstring& out) {
        EXPECT_TRUE(out.empty());
        out += 42;
        return "foobaz";
      });

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; doc += 2) {
      auto& stream = column(doc);
      const auto str = std::to_string(doc);
      stream.write_bytes(reinterpret_cast<const irs::byte_type*>(str.data()),
                         str.size());
    }

    ASSERT_TRUE(writer.commit(state));
  }
  TestResourceManager memory;
  {
    irs::columnstore2::reader reader;
    ASSERT_TRUE(reader.prepare(dir(), meta, reader_options(memory)));
    ASSERT_EQ(1, reader.size());

    auto* header = reader.header(0);
    ASSERT_NE(nullptr, header);
    ASSERT_EQ(kMax / 2, header->docs_count);
    ASSERT_NE(0, header->docs_index);
    ASSERT_EQ(irs::doc_limits::min(), header->min);
    ASSERT_EQ(ColumnType::kSparse, header->type);
    ASSERT_EQ(column_property(ColumnProperty::kNormal), header->props);

    auto* column = reader.column(0);
    ASSERT_NE(nullptr, column);
    ASSERT_EQ(kMax / 2, column->size());
    ASSERT_EQ(0, column->id());
    ASSERT_EQ("foobaz", column->name());

    const auto header_payload = column->payload();
    ASSERT_EQ(1, header_payload.size());
    ASSERT_EQ(42, header_payload[0]);

    auto assert_iterator = [&](irs::ColumnHint hint) {
      {
        auto prev_it = column->iterator(hint);
        auto it = column->iterator(hint);
        auto* document = irs::get<irs::document>(*it);
        ASSERT_NE(nullptr, document);
        const irs::payload* payload = nullptr;
        if (hint != irs::ColumnHint::kMask) {
          payload = irs::get<irs::payload>(*it);
          ASSERT_NE(nullptr, payload);
        } else {
          ASSERT_EQ(nullptr, irs::get<irs::payload>(*it));
        }
        auto* cost = irs::get<irs::cost>(*it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(column->size(), cost->estimate());
        auto* score = irs::get<irs::score>(*it);
        ASSERT_NE(nullptr, score);
        ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

        for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax;
             doc += 2) {
          SCOPED_TRACE(doc);
          ASSERT_EQ(doc, it->seek(doc));
          ASSERT_EQ(doc, it->seek(doc));
          const auto str = std::to_string(doc);
          if (payload) {
            ASSERT_EQ(str, irs::ViewCast<char>(payload->value));
          }
          assert_prev_doc(*it, *prev_it);
        }
      }

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; doc += 2) {
        auto it = column->iterator(hint);
        auto* document = irs::get<irs::document>(*it);
        ASSERT_NE(nullptr, document);
        const irs::payload* payload = nullptr;
        if (hint != irs::ColumnHint::kMask) {
          payload = irs::get<irs::payload>(*it);
          ASSERT_NE(nullptr, payload);
        } else {
          ASSERT_EQ(nullptr, irs::get<irs::payload>(*it));
        }
        auto* cost = irs::get<irs::cost>(*it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(column->size(), cost->estimate());
        auto* score = irs::get<irs::score>(*it);
        ASSERT_NE(nullptr, score);
        ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

        const auto str = std::to_string(doc);
        ASSERT_EQ(doc, it->seek(doc));
        if (payload) {
          EXPECT_EQ(str, irs::ViewCast<char>(payload->value));
        }
        ASSERT_EQ(doc, it->seek(doc));
        if (payload) {
          EXPECT_EQ(str, irs::ViewCast<char>(payload->value));
        }
        ASSERT_EQ(doc, it->seek(doc - 1));
        if (payload) {
          EXPECT_EQ(str, irs::ViewCast<char>(payload->value));
        }
      }

      // seek + next
      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax;
           doc += 5000) {
        auto prev_it = column->iterator(hint);
        auto it = column->iterator(hint);
        auto* document = irs::get<irs::document>(*it);
        ASSERT_NE(nullptr, document);
        const irs::payload* payload = nullptr;
        if (hint != irs::ColumnHint::kMask) {
          payload = irs::get<irs::payload>(*it);
          ASSERT_NE(nullptr, payload);
        } else {
          ASSERT_EQ(nullptr, irs::get<irs::payload>(*it));
        }
        auto* cost = irs::get<irs::cost>(*it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(column->size(), cost->estimate());
        auto* score = irs::get<irs::score>(*it);
        ASSERT_NE(nullptr, score);
        ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

        const auto str = std::to_string(doc);
        ASSERT_EQ(doc, it->seek(doc));
        if (payload) {
          EXPECT_EQ(str, irs::ViewCast<char>(payload->value));
        }
        ASSERT_EQ(doc, it->seek(doc));
        if (payload) {
          EXPECT_EQ(str, irs::ViewCast<char>(payload->value));
        }
        ASSERT_EQ(doc, it->seek(doc - 1));
        if (payload) {
          EXPECT_EQ(str, irs::ViewCast<char>(payload->value));
        }
        assert_prev_doc(*it, *prev_it);

        auto next_it = column->iterator(hint);
        ASSERT_NE(nullptr, next_it);
        const irs::payload* next_payload = nullptr;
        if (hint != irs::ColumnHint::kMask) {
          next_payload = irs::get<irs::payload>(*next_it);
          ASSERT_NE(nullptr, next_payload);
        } else {
          ASSERT_EQ(nullptr, irs::get<irs::payload>(*next_it));
        }
        ASSERT_EQ(doc, next_it->seek(doc));
        if (next_payload) {
          EXPECT_EQ(str, irs::ViewCast<char>(next_payload->value));
        }
        for (auto next_doc = doc + 2; next_doc <= kMax; next_doc += 2) {
          ASSERT_TRUE(next_it->next());
          ASSERT_EQ(next_doc, next_it->value());
          const auto str = std::to_string(next_doc);
          if (next_payload) {
            EXPECT_EQ(str, irs::ViewCast<char>(next_payload->value));
          }
          assert_prev_doc(*next_it, *prev_it);
        }
      }

      // next + seek
      {
        auto prev_it = column->iterator(hint);
        auto it = column->iterator(hint);
        auto* document = irs::get<irs::document>(*it);
        ASSERT_NE(nullptr, document);
        const irs::payload* payload = nullptr;
        if (hint != irs::ColumnHint::kMask) {
          payload = irs::get<irs::payload>(*it);
          ASSERT_NE(nullptr, payload);
        } else {
          ASSERT_EQ(nullptr, irs::get<irs::payload>(*it));
        }
        auto* cost = irs::get<irs::cost>(*it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(column->size(), cost->estimate());
        auto* score = irs::get<irs::score>(*it);
        ASSERT_NE(nullptr, score);
        ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);
        ASSERT_TRUE(it->next());
        ASSERT_EQ(irs::doc_limits::min(), document->value);
        assert_prev_doc(*it, *prev_it);
        ASSERT_EQ(118775, it->seek(118774));
        const auto str = std::to_string(it->value());
        if (payload) {
          EXPECT_EQ(str, irs::ViewCast<char>(payload->value));
        }
        assert_prev_doc(*it, *prev_it);
        ASSERT_TRUE(irs::doc_limits::eof(it->seek(kMax + 1)));
      }
    };

    assert_iterator(hint());
  }
}

TEST_P(columnstore2_test_case, sparse_column_gap) {
  static constexpr irs::doc_id_t kMax = 500000;
  static constexpr auto kBlockSize = irs::sparse_bitmap_writer::kBlockSize;
  static constexpr auto kGapBegin = ((kMax / kBlockSize) - 4) * kBlockSize;
  irs::SegmentMeta meta;
  meta.name = "test";

  irs::flush_state state{
    .name = meta.name,
    .doc_count = kMax,
  };
  TestResourceManager memory;
  {
    irs::columnstore2::writer writer(version(), irs::IResourceManager::kNoop,
                                     consolidation());
    writer.prepare(dir(), meta);

    auto [id, column] =
      writer.push_column(column_info(), [](irs::bstring& out) {
        EXPECT_TRUE(out.empty());
        out += 42;
        return "foobarbaz";
      });

    auto write_payload = [](irs::doc_id_t doc, irs::data_output& stream) {
      if (doc <= kGapBegin || doc > (kGapBegin + kBlockSize)) {
        stream.write_bytes(reinterpret_cast<const irs::byte_type*>(&doc),
                           sizeof doc);
      }
    };

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
      write_payload(doc, column(doc));
    }

    ASSERT_TRUE(writer.commit(state));
  }

  {
    auto assert_payload = [this](irs::doc_id_t doc, irs::bytes_view payload) {
      SCOPED_TRACE(doc);
      if (has_payload() &&
          (doc <= kGapBegin || doc > (kGapBegin + kBlockSize))) {
        ASSERT_EQ(sizeof doc, payload.size());
        const irs::doc_id_t actual_doc =
          *reinterpret_cast<const irs::doc_id_t*>(payload.data());
        ASSERT_EQ(doc, actual_doc);
      } else {
        ASSERT_TRUE(payload.empty());
      }
    };

    irs::columnstore2::reader reader;
    ASSERT_TRUE(reader.prepare(dir(), meta, reader_options(memory)));
    ASSERT_EQ(1, reader.size());

    auto* header = reader.header(0);
    ASSERT_NE(nullptr, header);
    ASSERT_EQ(kMax, header->docs_count);
    ASSERT_EQ(0, header->docs_index);
    ASSERT_EQ(irs::doc_limits::min(), header->min);
    ASSERT_EQ(ColumnType::kSparse, header->type);
    ASSERT_EQ(column_property(ColumnProperty::kNormal), header->props);

    auto* column = reader.column(0);
    ASSERT_NE(nullptr, column);
    ASSERT_EQ(kMax, column->size());

    ASSERT_EQ(0, column->id());
    ASSERT_EQ("foobarbaz", column->name());

    const auto header_payload = column->payload();
    ASSERT_EQ(1, header_payload.size());
    ASSERT_EQ(42, header_payload[0]);

    {
      auto prev_it = column->iterator(hint());
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
        SCOPED_TRACE(doc);
        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(doc, it->seek(doc));
        assert_payload(doc, payload->value);
        assert_prev_doc(*it, *prev_it);
      }
    }

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

      const auto str = std::to_string(doc);
      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
      ASSERT_EQ(doc, it->seek(doc - 1));
      assert_payload(doc, payload->value);
    }

    // seek + next
    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; doc += 5000) {
      auto prev_it = column->iterator(hint());
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

      const auto str = std::to_string(doc);
      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
      ASSERT_EQ(doc, it->seek(doc - 1));
      assert_payload(doc, payload->value);
      assert_prev_doc(*it, *prev_it);

      auto next_it = column->iterator(hint());
      auto* next_payload = irs::get<irs::payload>(*next_it);
      ASSERT_NE(nullptr, next_payload);
      ASSERT_EQ(doc, next_it->seek(doc));
      assert_payload(doc, next_payload->value);
      for (auto next_doc = doc + 1; next_doc <= kMax; ++next_doc) {
        ASSERT_TRUE(next_it->next());
        ASSERT_EQ(next_doc, next_it->value());
        assert_payload(next_doc, next_payload->value);
        assert_prev_doc(*next_it, *prev_it);
      }
    }

    // next + seek
    {
      auto prev_it = column->iterator(hint());
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_TRUE(it->next());
      ASSERT_EQ(irs::doc_limits::min(), document->value);
      assert_prev_doc(*it, *prev_it);
      ASSERT_EQ(118775, it->seek(118775));
      assert_payload(118775, payload->value);
      assert_prev_doc(*it, *prev_it);
      ASSERT_TRUE(irs::doc_limits::eof(it->seek(kMax + 1)));
    }
  }
}

TEST_P(columnstore2_test_case, sparse_column_tail_block) {
  static constexpr irs::doc_id_t kMax = 500000;
  static constexpr auto kBlockSize = irs::sparse_bitmap_writer::kBlockSize;
  static constexpr auto kTailBegin = (kMax / kBlockSize) * kBlockSize;
  irs::SegmentMeta meta;
  meta.name = "test";

  irs::flush_state state{
    .name = meta.name,
    .doc_count = kMax,
  };

  {
    auto write_payload = [](irs::doc_id_t doc, irs::data_output& stream) {
      stream.write_bytes(reinterpret_cast<const irs::byte_type*>(&doc),
                         sizeof doc);
      if (doc > kTailBegin) {
        stream.write_bytes(reinterpret_cast<const irs::byte_type*>(&doc),
                           sizeof doc);
      }
    };

    irs::columnstore2::writer writer(version(), irs::IResourceManager::kNoop,
                                     consolidation());
    writer.prepare(dir(), meta);

    auto [id, column] =
      writer.push_column(column_info(), [](irs::bstring& out) {
        EXPECT_TRUE(out.empty());
        out += 42;
        return std::string_view{};
      });

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
      write_payload(doc, column(doc));
    }

    ASSERT_TRUE(writer.commit(state));
  }
  TestResourceManager memory;
  {
    irs::columnstore2::reader reader;
    ASSERT_TRUE(reader.prepare(dir(), meta, reader_options(memory)));
    ASSERT_EQ(1, reader.size());

    auto* header = reader.header(0);
    ASSERT_NE(nullptr, header);
    ASSERT_EQ(kMax, header->docs_count);
    ASSERT_EQ(0, header->docs_index);
    ASSERT_EQ(irs::doc_limits::min(), header->min);
    ASSERT_EQ(ColumnType::kSparse, header->type);
    ASSERT_EQ(column_property(ColumnProperty::kNoName), header->props);

    auto* column = reader.column(0);
    ASSERT_NE(nullptr, column);
    ASSERT_EQ(kMax, column->size());

    ASSERT_EQ(0, column->id());
    ASSERT_TRUE(irs::IsNull(column->name()));

    const auto header_payload = column->payload();
    ASSERT_EQ(1, header_payload.size());
    ASSERT_EQ(42, header_payload[0]);

    auto assert_payload = [this](irs::doc_id_t doc, irs::bytes_view payload) {
      SCOPED_TRACE(doc);

      if (!has_payload()) {
        ASSERT_TRUE(payload.empty());
        return;
      }

      ASSERT_FALSE(payload.empty());
      if (doc > kTailBegin) {
        ASSERT_EQ(2 * sizeof doc, payload.size());
        const irs::doc_id_t* actual_doc =
          reinterpret_cast<const irs::doc_id_t*>(payload.data());
        ASSERT_EQ(doc, actual_doc[0]);
        ASSERT_EQ(doc, actual_doc[1]);
      } else {
        ASSERT_EQ(sizeof doc, payload.size());
        const irs::doc_id_t actual_doc =
          *reinterpret_cast<const irs::doc_id_t*>(payload.data());
        ASSERT_EQ(doc, actual_doc);
      }
    };

    {
      auto prev_it = column->iterator(hint());
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(doc, it->seek(doc));
        assert_payload(doc, payload->value);
        assert_prev_doc(*it, *prev_it);
      }
    }

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

      ASSERT_EQ(doc, it->seek(doc));
      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
    }

    // seek + next
    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax;
         doc += 10000) {
      auto prev_it = column->iterator(hint());
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
      ASSERT_EQ(doc, it->seek(doc - 1));
      assert_payload(doc, payload->value);
      assert_prev_doc(*it, *prev_it);

      auto next_it = column->iterator(hint());
      auto* next_payload = irs::get<irs::payload>(*next_it);
      ASSERT_NE(nullptr, next_payload);
      ASSERT_EQ(doc, next_it->seek(doc));
      assert_payload(doc, next_payload->value);
      for (auto next_doc = doc + 1; next_doc <= kMax; ++next_doc) {
        ASSERT_TRUE(next_it->next());
        assert_payload(next_doc, next_payload->value);
        assert_prev_doc(*next_it, *prev_it);
      }
    }

    // next + seek
    {
      constexpr irs::doc_id_t doc = 118774;

      auto prev_it = column->iterator(hint());
      auto it = column->iterator(hint());
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
      assert_prev_doc(*it, *prev_it);
      ASSERT_TRUE(irs::doc_limits::eof(it->seek(kMax + 1)));
    }
  }
}

TEST_P(columnstore2_test_case, sparse_column_tail_block_last_value) {
  static constexpr irs::doc_id_t kMax = 500000;
  // last value has different length
  static constexpr auto kTailBegin = kMax - 1;
  irs::SegmentMeta meta;
  meta.name = "test";

  irs::flush_state state{
    .name = meta.name,
    .doc_count = kMax,
  };

  {
    auto write_payload = [](irs::doc_id_t doc, irs::data_output& stream) {
      stream.write_bytes(reinterpret_cast<const irs::byte_type*>(&doc),
                         sizeof doc);
      if (doc > kTailBegin) {
        stream.write_byte(42);
      }
    };
    irs::columnstore2::writer writer(version(), irs::IResourceManager::kNoop,
                                     consolidation());
    writer.prepare(dir(), meta);

    auto [id, column] =
      writer.push_column(column_info(), [](irs::bstring& out) {
        EXPECT_TRUE(out.empty());
        out += 42;
        return std::string_view{};
      });

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
      write_payload(doc, column(doc));
    }

    ASSERT_TRUE(writer.commit(state));
  }

  {
    TestResourceManager memory;
    irs::columnstore2::reader reader;
    ASSERT_TRUE(reader.prepare(dir(), meta, reader_options(memory)));
    ASSERT_EQ(1, reader.size());

    auto* header = reader.header(0);
    ASSERT_NE(nullptr, header);
    ASSERT_EQ(kMax, header->docs_count);
    ASSERT_EQ(0, header->docs_index);
    ASSERT_EQ(irs::doc_limits::min(), header->min);
    ASSERT_EQ(ColumnType::kSparse, header->type);
    ASSERT_EQ(column_property(ColumnProperty::kNoName), header->props);

    auto* column = reader.column(0);
    ASSERT_NE(nullptr, column);
    ASSERT_EQ(kMax, column->size());

    ASSERT_EQ(0, column->id());
    ASSERT_TRUE(irs::IsNull(column->name()));

    const auto header_payload = column->payload();
    ASSERT_EQ(1, header_payload.size());
    ASSERT_EQ(42, header_payload[0]);

    auto assert_payload = [this](irs::doc_id_t doc, irs::bytes_view payload) {
      SCOPED_TRACE(doc);

      if (!has_payload()) {
        ASSERT_TRUE(payload.empty());
        return;
      }

      ASSERT_FALSE(payload.empty());
      if (doc > kTailBegin) {
        ASSERT_EQ(1 + sizeof doc, payload.size());
        const irs::doc_id_t actual_doc =
          *reinterpret_cast<const irs::doc_id_t*>(payload.data());
        ASSERT_EQ(doc, actual_doc);
        ASSERT_EQ(42, payload[sizeof doc]);
      } else {
        ASSERT_EQ(sizeof doc, payload.size());
        const irs::doc_id_t actual_doc =
          *reinterpret_cast<const irs::doc_id_t*>(payload.data());
        ASSERT_EQ(doc, actual_doc);
      }
    };

    {
      auto prev_it = column->iterator(hint());
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(doc, it->seek(doc));
        assert_payload(doc, payload->value);
        assert_prev_doc(*it, *prev_it);
      }
    }

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

      ASSERT_EQ(doc, it->seek(doc));
      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
    }

    // seek + next
    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax;
         doc += 10000) {
      auto prev_it = column->iterator(hint());
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
      ASSERT_EQ(doc, it->seek(doc - 1));
      assert_payload(doc, payload->value);
      assert_prev_doc(*it, *prev_it);

      auto next_it = column->iterator(hint());
      auto* next_payload = irs::get<irs::payload>(*next_it);
      ASSERT_NE(nullptr, next_payload);
      ASSERT_EQ(doc, next_it->seek(doc));
      assert_payload(doc, next_payload->value);
      for (auto next_doc = doc + 1; next_doc <= kMax; ++next_doc) {
        ASSERT_TRUE(next_it->next());
        assert_payload(next_doc, next_payload->value);
        assert_prev_doc(*next_it, *prev_it);
      }
    }

    // next + seek
    {
      constexpr irs::doc_id_t doc = 118774;

      auto prev_it = column->iterator(hint());
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      ASSERT_TRUE(it->next());
      ASSERT_EQ(irs::doc_limits::min(), document->value);
      assert_prev_doc(*it, *prev_it);
      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
      assert_prev_doc(*it, *prev_it);
      ASSERT_TRUE(irs::doc_limits::eof(it->seek(kMax + 1)));
    }
  }
}

TEST_P(columnstore2_test_case, sparse_column_full_blocks) {
  // Exactly 2 blocks
  static constexpr irs::doc_id_t kMax = 131072;
  // last value has different length
  static constexpr auto kTailBegin = kMax - 3;
  irs::SegmentMeta meta;
  meta.name = "test";

  irs::flush_state state{
    .name = meta.name,
    .doc_count = kMax,
  };

  constexpr std::string_view kValue{"aaaaaaaaaaaaaaaaaaaaaaaaaaa"};
  static_assert(kValue.size() == 27);

  {
    auto write_payload = [value = kValue](irs::doc_id_t doc,
                                          irs::data_output& stream) {
      stream.write_bytes(reinterpret_cast<const irs::byte_type*>(value.data()),
                         value.size());
      if (doc <= kTailBegin) {
        stream.write_byte(value.front());
      }
    };

    irs::columnstore2::writer writer(version(), irs::IResourceManager::kNoop,
                                     consolidation());
    writer.prepare(dir(), meta);

    auto [id, column] =
      writer.push_column(column_info(), [](irs::bstring& out) {
        EXPECT_TRUE(out.empty());
        out += 42;
        return std::string_view{};
      });

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
      write_payload(doc, column(doc));
    }

    ASSERT_TRUE(writer.commit(state));
  }

  {
    irs::columnstore2::reader reader;
    ASSERT_TRUE(reader.prepare(dir(), meta, reader_options()));
    ASSERT_EQ(1, reader.size());

    auto* header = reader.header(0);
    ASSERT_NE(nullptr, header);
    ASSERT_EQ(kMax, header->docs_count);
    ASSERT_EQ(0, header->docs_index);
    ASSERT_EQ(irs::doc_limits::min(), header->min);
    ASSERT_EQ(ColumnType::kSparse, header->type);
    ASSERT_EQ(column_property(ColumnProperty::kNoName), header->props);

    auto* column = reader.column(0);
    ASSERT_NE(nullptr, column);
    ASSERT_EQ(kMax, column->size());

    ASSERT_EQ(0, column->id());
    ASSERT_TRUE(irs::IsNull(column->name()));

    const auto header_payload = column->payload();
    ASSERT_EQ(1, header_payload.size());
    ASSERT_EQ(42, header_payload[0]);

    auto assert_payload = [this, value = kValue](irs::doc_id_t doc,
                                                 irs::bytes_view payload) {
      SCOPED_TRACE(doc);

      if (!has_payload()) {
        ASSERT_TRUE(payload.empty());
        return;
      }

      ASSERT_FALSE(payload.empty());
      if (doc <= kTailBegin) {
        ASSERT_EQ(1 + value.size(), payload.size());
        std::string expected{value.data(), value.size()};
        expected += value.front();
        ASSERT_EQ(expected, irs::ViewCast<char>(payload));
      } else {
        ASSERT_EQ(value.size(), payload.size());
        ASSERT_EQ(value, irs::ViewCast<char>(payload));
      }
    };

    {
      auto prev_it = column->iterator(hint());
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(doc, it->seek(doc));
        assert_payload(doc, payload->value);
        assert_prev_doc(*it, *prev_it);
      }
    }

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

      ASSERT_EQ(doc, it->seek(doc));
      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
    }

    // seek + next
    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax;
         doc += 10000) {
      auto prev_it = column->iterator(hint());
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
      ASSERT_EQ(doc, it->seek(doc - 1));
      assert_payload(doc, payload->value);
      assert_prev_doc(*it, *prev_it);

      auto next_it = column->iterator(hint());
      auto* next_payload = irs::get<irs::payload>(*next_it);
      ASSERT_NE(nullptr, next_payload);
      ASSERT_EQ(doc, next_it->seek(doc));
      assert_payload(doc, next_payload->value);
      for (auto next_doc = doc + 1; next_doc <= kMax; ++next_doc) {
        ASSERT_TRUE(next_it->next());
        assert_payload(next_doc, next_payload->value);
        assert_prev_doc(*next_it, *prev_it);
      }
    }

    // next + seek
    {
      constexpr irs::doc_id_t doc = 118774;

      auto prev_it = column->iterator(hint());
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      ASSERT_TRUE(it->next());
      ASSERT_EQ(irs::doc_limits::min(), document->value);
      assert_prev_doc(*it, *prev_it);
      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
      assert_prev_doc(*it, *prev_it);
      ASSERT_TRUE(irs::doc_limits::eof(it->seek(kMax + 1)));
    }
  }
}

TEST_P(columnstore2_test_case, sparse_column_full_blocks_all_equal) {
  // Exactly 2 blocks
  static constexpr irs::doc_id_t kMax = 131072;
  // last value has different length
  static constexpr auto kTailBegin = kMax - 1;
  irs::SegmentMeta meta;
  meta.name = "test";

  irs::flush_state state{
    .name = meta.name,
    .doc_count = kMax,
  };

  constexpr std::string_view kValue{"aaaaaaaaaaaaaaaaaaaaaaaaaaa"};
  static_assert(kValue.size() == 27);

  {
    auto write_payload = [value = kValue](irs::doc_id_t doc,
                                          irs::data_output& stream) {
      stream.write_bytes(reinterpret_cast<const irs::byte_type*>(value.data()),
                         value.size());
      if (doc <= kTailBegin) {
        stream.write_byte(value.front());
      }
    };

    irs::columnstore2::writer writer(version(), irs::IResourceManager::kNoop,
                                     consolidation());
    writer.prepare(dir(), meta);

    auto [id, column] =
      writer.push_column(column_info(), [](irs::bstring& out) {
        EXPECT_TRUE(out.empty());
        out += 42;
        return std::string_view{};
      });

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
      write_payload(doc, column(doc));
    }

    ASSERT_TRUE(writer.commit(state));
  }

  {
    irs::columnstore2::reader reader;
    ASSERT_TRUE(reader.prepare(dir(), meta, reader_options()));
    ASSERT_EQ(1, reader.size());

    auto* header = reader.header(0);
    ASSERT_NE(nullptr, header);
    ASSERT_EQ(kMax, header->docs_count);
    ASSERT_EQ(0, header->docs_index);
    ASSERT_EQ(irs::doc_limits::min(), header->min);
    ASSERT_EQ(ColumnType::kSparse, header->type);
    ASSERT_EQ(column_property(ColumnProperty::kNoName), header->props);

    auto* column = reader.column(0);
    ASSERT_NE(nullptr, column);
    ASSERT_EQ(kMax, column->size());

    ASSERT_EQ(0, column->id());
    ASSERT_TRUE(irs::IsNull(column->name()));

    const auto header_payload = column->payload();
    ASSERT_EQ(1, header_payload.size());
    ASSERT_EQ(42, header_payload[0]);

    auto assert_payload = [this, value = kValue](irs::doc_id_t doc,
                                                 irs::bytes_view payload) {
      SCOPED_TRACE(doc);

      if (!has_payload()) {
        ASSERT_TRUE(payload.empty());
        return;
      }

      ASSERT_FALSE(payload.empty());
      if (doc <= kTailBegin) {
        ASSERT_EQ(1 + value.size(), payload.size());
        std::string expected{value.data(), value.size()};
        expected += value.front();
        ASSERT_EQ(expected, irs::ViewCast<char>(payload));
      } else {
        ASSERT_EQ(value.size(), payload.size());
        ASSERT_EQ(value, irs::ViewCast<char>(payload));
      }
    };

    {
      auto prev_it = column->iterator(hint());
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(doc, it->seek(doc));
        assert_payload(doc, payload->value);
        assert_prev_doc(*it, *prev_it);
      }
    }

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

      ASSERT_EQ(doc, it->seek(doc));
      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
    }

    // seek + next
    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax;
         doc += 10000) {
      auto prev_it = column->iterator(hint());
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
      ASSERT_EQ(doc, it->seek(doc - 1));
      assert_payload(doc, payload->value);
      assert_prev_doc(*it, *prev_it);

      auto next_it = column->iterator(hint());
      auto* next_payload = irs::get<irs::payload>(*next_it);
      ASSERT_NE(nullptr, next_payload);
      ASSERT_EQ(doc, next_it->seek(doc));
      assert_payload(doc, next_payload->value);
      for (auto next_doc = doc + 1; next_doc <= kMax; ++next_doc) {
        ASSERT_TRUE(next_it->next());
        assert_payload(next_doc, next_payload->value);
        assert_prev_doc(*next_it, *prev_it);
      }
    }

    // next + seek
    {
      constexpr irs::doc_id_t doc = 118774;

      auto prev_it = column->iterator(hint());
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      ASSERT_TRUE(it->next());
      ASSERT_EQ(irs::doc_limits::min(), document->value);
      assert_prev_doc(*it, *prev_it);
      ASSERT_EQ(doc, it->seek(doc));
      assert_payload(doc, payload->value);
      assert_prev_doc(*it, *prev_it);
      ASSERT_TRUE(irs::doc_limits::eof(it->seek(kMax + 1)));
    }
  }
}

TEST_P(columnstore2_test_case, dense_mask_column) {
  constexpr irs::doc_id_t kMax = 1000000;
  irs::SegmentMeta meta;
  meta.name = "test";

  irs::flush_state state{
    .name = meta.name,
    .doc_count = kMax,
  };

  {
    irs::columnstore2::writer writer(version(), irs::IResourceManager::kNoop,
                                     consolidation());
    writer.prepare(dir(), meta);

    auto [id, column] =
      writer.push_column(column_info(), [](irs::bstring& out) {
        EXPECT_TRUE(out.empty());
        out += 42;
        return "foobar";
      });

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
      column(doc);
    }

    ASSERT_TRUE(writer.commit(state));
  }

  {
    irs::columnstore2::reader reader;
    ASSERT_TRUE(reader.prepare(dir(), meta, reader_options()));
    ASSERT_EQ(1, reader.size());

    auto* header = reader.header(0);
    ASSERT_NE(nullptr, header);
    ASSERT_EQ(kMax, header->docs_count);
    ASSERT_EQ(0, header->docs_index);
    ASSERT_EQ(irs::doc_limits::min(), header->min);
    ASSERT_EQ(ColumnType::kMask, header->type);
    ASSERT_EQ(column_property(ColumnProperty::kNormal), header->props);

    auto* column = reader.column(0);
    ASSERT_NE(nullptr, column);
    ASSERT_EQ(kMax, column->size());

    ASSERT_EQ(0, column->id());
    ASSERT_EQ("foobar", column->name());

    const auto header_payload = column->payload();
    ASSERT_EQ(1, header_payload.size());
    ASSERT_EQ(42, header_payload[0]);

    {
      auto prev_it = column->iterator(hint());
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      ASSERT_TRUE(irs::IsNull(payload->value));
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_TRUE(irs::IsNull(payload->value));
        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_TRUE(irs::IsNull(payload->value));
        ASSERT_EQ(doc, it->seek(doc - 1));
        ASSERT_TRUE(irs::IsNull(payload->value));
        assert_prev_doc(*it, *prev_it);
      }
    }

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
      const auto hint = doc % 2 ? this->hint() : irs::ColumnHint::kMask;
      auto it = column->iterator(hint);
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      ASSERT_TRUE(irs::IsNull(payload->value));
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

      ASSERT_EQ(doc, it->seek(doc));
      ASSERT_TRUE(irs::IsNull(payload->value));
      ASSERT_EQ(doc, it->seek(doc));
      ASSERT_TRUE(irs::IsNull(payload->value));
      ASSERT_EQ(doc, it->seek(doc - 1));
      ASSERT_TRUE(irs::IsNull(payload->value));
    }

    // seek + next
    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax;
         doc += 10000) {
      auto prev_it = column->iterator(hint());
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

      ASSERT_EQ(doc, it->seek(doc));
      ASSERT_EQ(doc, it->seek(doc));
      assert_prev_doc(*it, *prev_it);

      auto next_it = column->iterator(hint());
      ASSERT_EQ(doc, next_it->seek(doc));
      for (auto next_doc = doc + 1; next_doc <= kMax; ++next_doc) {
        ASSERT_TRUE(next_it->next());
        ASSERT_EQ(next_doc, next_it->value());
        assert_prev_doc(*next_it, *prev_it);
      }
    }

    // next + seek
    {
      auto prev_it = column->iterator(hint());
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);
      ASSERT_TRUE(it->next());
      ASSERT_EQ(irs::doc_limits::min(), document->value);
      assert_prev_doc(*it, *prev_it);
      ASSERT_TRUE(irs::IsNull(payload->value));
      ASSERT_EQ(118774, it->seek(118774));
      assert_prev_doc(*it, *prev_it);
      ASSERT_TRUE(irs::IsNull(payload->value));
      ASSERT_TRUE(irs::doc_limits::eof(it->seek(kMax + 1)));
      ASSERT_TRUE(irs::doc_limits::eof(it->seek(irs::doc_limits::eof())));
    }
  }
}

TEST_P(columnstore2_test_case, dense_column) {
  constexpr irs::doc_id_t kMax = 1000000;
  irs::SegmentMeta meta;
  meta.name = "test";

  irs::flush_state state{
    .name = meta.name,
    .doc_count = kMax,
  };

  {
    irs::columnstore2::writer writer(version(), irs::IResourceManager::kNoop,
                                     consolidation());
    writer.prepare(dir(), meta);

    auto [id, column] =
      writer.push_column(column_info(), [](irs::bstring& out) {
        EXPECT_TRUE(out.empty());
        out += 42;
        return "foobar";
      });

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
      auto& stream = column(doc);
      const auto str = std::to_string(doc);
      stream.write_bytes(reinterpret_cast<const irs::byte_type*>(str.c_str()),
                         str.size());
    }

    ASSERT_TRUE(writer.commit(state));
  }

  {
    irs::columnstore2::reader reader;
    ASSERT_TRUE(reader.prepare(dir(), meta, reader_options()));
    ASSERT_EQ(1, reader.size());

    auto* header = reader.header(0);
    ASSERT_NE(nullptr, header);
    ASSERT_EQ(kMax, header->docs_count);
    ASSERT_EQ(0, header->docs_index);
    ASSERT_EQ(irs::doc_limits::min(), header->min);
    ASSERT_EQ(ColumnType::kSparse, header->type);
    ASSERT_EQ(column_property(ColumnProperty::kNormal), header->props);

    auto* column = reader.column(0);
    ASSERT_NE(nullptr, column);
    ASSERT_EQ(kMax, column->size());

    ASSERT_EQ(0, column->id());
    ASSERT_EQ("foobar", column->name());

    const auto header_payload = column->payload();
    ASSERT_EQ(1, header_payload.size());
    ASSERT_EQ(42, header_payload[0]);

    auto assert_iterator = [&](irs::ColumnHint hint) {
      {
        auto prev_it = column->iterator(hint);
        auto it = column->iterator(hint);
        auto* document = irs::get<irs::document>(*it);
        ASSERT_NE(nullptr, document);
        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_NE(nullptr, payload);
        auto* cost = irs::get<irs::cost>(*it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(column->size(), cost->estimate());
        auto* score = irs::get<irs::score>(*it);
        ASSERT_NE(nullptr, score);
        ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

        for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
          ASSERT_EQ(doc, it->seek(doc));
          ASSERT_EQ(doc, it->seek(doc));
          assert_prev_doc(*it, *prev_it);
          const auto str = std::to_string(doc);
          if (hint == irs::ColumnHint::kMask) {
            EXPECT_TRUE(irs::IsNull(payload->value));
          } else {
            EXPECT_EQ(str, irs::ViewCast<char>(payload->value));
          }
        }
      }

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
        auto it = column->iterator(hint);
        auto* document = irs::get<irs::document>(*it);
        ASSERT_NE(nullptr, document);
        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_NE(nullptr, payload);
        auto* cost = irs::get<irs::cost>(*it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(column->size(), cost->estimate());
        auto* score = irs::get<irs::score>(*it);
        ASSERT_NE(nullptr, score);
        ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

        const auto str = std::to_string(doc);
        ASSERT_EQ(doc, it->seek(doc));
        if (hint == irs::ColumnHint::kMask) {
          EXPECT_TRUE(irs::IsNull(payload->value));
        } else {
          EXPECT_EQ(str, irs::ViewCast<char>(payload->value));
        }
        ASSERT_EQ(doc, it->seek(doc));
        if (hint == irs::ColumnHint::kMask) {
          EXPECT_TRUE(irs::IsNull(payload->value));
        } else {
          EXPECT_EQ(str, irs::ViewCast<char>(payload->value));
        }
        ASSERT_EQ(doc, it->seek(doc - 1));
        if (hint == irs::ColumnHint::kMask) {
          EXPECT_TRUE(irs::IsNull(payload->value));
        } else {
          EXPECT_EQ(str, irs::ViewCast<char>(payload->value));
        }
      }

      // seek + next
      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax;
           doc += 10000) {
        auto prev_it = column->iterator(hint);
        auto it = column->iterator(hint);
        auto* document = irs::get<irs::document>(*it);
        ASSERT_NE(nullptr, document);
        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_NE(nullptr, payload);
        auto* cost = irs::get<irs::cost>(*it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(column->size(), cost->estimate());
        auto* score = irs::get<irs::score>(*it);
        ASSERT_NE(nullptr, score);
        ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

        const auto str = std::to_string(doc);
        ASSERT_EQ(doc, it->seek(doc));
        if (hint == irs::ColumnHint::kMask) {
          EXPECT_TRUE(irs::IsNull(payload->value));
        } else {
          EXPECT_EQ(str, irs::ViewCast<char>(payload->value));
        }
        ASSERT_EQ(doc, it->seek(doc));
        if (hint == irs::ColumnHint::kMask) {
          EXPECT_TRUE(irs::IsNull(payload->value));
        } else {
          EXPECT_EQ(str, irs::ViewCast<char>(payload->value));
        }
        assert_prev_doc(*it, *prev_it);

        auto next_it = column->iterator(hint);
        auto* next_payload = irs::get<irs::payload>(*next_it);
        ASSERT_NE(nullptr, next_payload);
        ASSERT_EQ(doc, next_it->seek(doc));
        if (hint == irs::ColumnHint::kMask) {
          EXPECT_TRUE(irs::IsNull(next_payload->value));
        } else {
          EXPECT_EQ(str, irs::ViewCast<char>(next_payload->value));
        }
        for (auto next_doc = doc + 1; next_doc <= kMax; ++next_doc) {
          ASSERT_TRUE(next_it->next());
          ASSERT_EQ(next_doc, next_it->value());
          const auto str = std::to_string(next_doc);
          if (hint == irs::ColumnHint::kMask) {
            EXPECT_TRUE(irs::IsNull(next_payload->value));
          } else {
            EXPECT_EQ(str, irs::ViewCast<char>(next_payload->value));
          }
          assert_prev_doc(*next_it, *prev_it);
        }
      }

      // next + seek
      {
        auto prev_it = column->iterator(hint);
        auto it = column->iterator(hint);
        auto* document = irs::get<irs::document>(*it);
        ASSERT_NE(nullptr, document);
        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_NE(nullptr, payload);
        auto* cost = irs::get<irs::cost>(*it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(column->size(), cost->estimate());
        auto* score = irs::get<irs::score>(*it);
        ASSERT_NE(nullptr, score);
        ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);
        ASSERT_TRUE(it->next());
        ASSERT_EQ(irs::doc_limits::min(), document->value);
        assert_prev_doc(*it, *prev_it);
        const auto str = std::to_string(118774);
        ASSERT_EQ(118774, it->seek(118774));
        if (hint == irs::ColumnHint::kMask) {
          EXPECT_TRUE(irs::IsNull(payload->value));
        } else {
          EXPECT_EQ(str, irs::ViewCast<char>(payload->value));
        }
        assert_prev_doc(*it, *prev_it);
        ASSERT_TRUE(irs::doc_limits::eof(it->seek(kMax + 1)));
      }
    };

    assert_iterator(hint());
  }
}

TEST_P(columnstore2_test_case, dense_column_range) {
  constexpr irs::doc_id_t kMin = 500000;
  constexpr irs::doc_id_t kMax = 1000000;
  irs::SegmentMeta meta;
  meta.name = "test";

  irs::flush_state state{
    .name = meta.name,
    .doc_count = kMax,
  };

  {
    irs::columnstore2::writer writer(version(), irs::IResourceManager::kNoop,
                                     consolidation());
    writer.prepare(dir(), meta);

    auto [id, column] =
      writer.push_column(column_info(), [](irs::bstring& out) {
        EXPECT_TRUE(out.empty());
        out += 42;
        return std::string_view{};
      });

    for (irs::doc_id_t doc = kMin; doc <= kMax; ++doc) {
      auto& stream = column(doc);
      const auto str = std::to_string(doc);
      stream.write_bytes(reinterpret_cast<const irs::byte_type*>(str.c_str()),
                         str.size());
    }

    ASSERT_TRUE(writer.commit(state));
  }

  {
    irs::columnstore2::reader reader;
    ASSERT_TRUE(reader.prepare(dir(), meta, reader_options()));
    ASSERT_EQ(1, reader.size());

    auto* header = reader.header(0);
    ASSERT_NE(nullptr, header);
    ASSERT_EQ(kMax - kMin + 1, header->docs_count);
    ASSERT_EQ(0, header->docs_index);
    ASSERT_EQ(kMin, header->min);
    ASSERT_EQ(ColumnType::kSparse, header->type);
    ASSERT_EQ(column_property(ColumnProperty::kNoName), header->props);

    auto* column = reader.column(0);
    ASSERT_NE(nullptr, column);
    ASSERT_EQ(kMax - kMin + 1, column->size());

    ASSERT_EQ(0, column->id());
    ASSERT_TRUE(irs::IsNull(column->name()));

    const auto header_payload = column->payload();
    ASSERT_EQ(1, header_payload.size());
    ASSERT_EQ(42, header_payload[0]);

    auto assert_payload = [this](std::string_view str,
                                 const irs::payload& payload) {
      if (has_payload()) {
        EXPECT_EQ(str, irs::ViewCast<char>(payload.value));
      } else {
        ASSERT_TRUE(payload.value.empty());
      }
    };

    // seek before range
    {
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

      ASSERT_EQ(kMin, it->seek(42));
      assert_payload(std::to_string(kMin), *payload);

      irs::doc_id_t expected_doc = kMin + 1;
      for (; expected_doc <= kMax; ++expected_doc) {
        const auto str = std::to_string(expected_doc);
        ASSERT_EQ(expected_doc, it->seek(expected_doc));
      }
      ASSERT_FALSE(it->next());
      ASSERT_TRUE(irs::doc_limits::eof(it->value()));
    }

    {
      auto prev_it = column->iterator(hint());
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
        const auto expected_doc = (doc <= kMin ? kMin : doc);
        const auto str = std::to_string(expected_doc);
        ASSERT_EQ(expected_doc, it->seek(doc));
        assert_payload(str, *payload);
        ASSERT_EQ(expected_doc, it->seek(doc));
        assert_payload(str, *payload);
        ASSERT_EQ(expected_doc, it->seek(doc - 1));
        assert_payload(str, *payload);
        assert_prev_doc(*it, *prev_it);
      }
    }

    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

      const auto expected_doc = (doc <= kMin ? kMin : doc);
      const auto str = std::to_string(expected_doc);
      ASSERT_EQ(expected_doc, it->seek(doc));
      assert_payload(str, *payload);
      ASSERT_EQ(expected_doc, it->seek(doc));
      assert_payload(str, *payload);
    }

    // seek + next
    for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax;
         doc += 10000) {
      auto prev_it = column->iterator(hint());
      auto it = column->iterator(hint());
      auto* document = irs::get<irs::document>(*it);
      ASSERT_NE(nullptr, document);
      auto* payload = irs::get<irs::payload>(*it);
      ASSERT_NE(nullptr, payload);
      auto* cost = irs::get<irs::cost>(*it);
      ASSERT_NE(nullptr, cost);
      ASSERT_EQ(column->size(), cost->estimate());
      auto* score = irs::get<irs::score>(*it);
      ASSERT_NE(nullptr, score);
      ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

      const auto expected_doc = (doc <= kMin ? kMin : doc);
      const auto str = std::to_string(expected_doc);
      ASSERT_EQ(expected_doc, it->seek(doc));
      assert_payload(str, *payload);
      ASSERT_EQ(expected_doc, it->seek(doc));
      assert_payload(str, *payload);
      assert_prev_doc(*it, *prev_it);

      auto next_it = column->iterator(hint());
      ASSERT_EQ(expected_doc, next_it->seek(doc));
      auto* next_payload = irs::get<irs::payload>(*next_it);
      ASSERT_NE(nullptr, next_payload);
      assert_payload(str, *next_payload);
      for (auto next_doc = expected_doc + 1; next_doc <= kMax; ++next_doc) {
        ASSERT_TRUE(next_it->next());
        ASSERT_EQ(next_doc, next_it->value());
        const auto str = std::to_string(next_doc);
        assert_payload(str, *next_payload);
        assert_prev_doc(*next_it, *prev_it);
      }
    }
  }
}

TEST_P(columnstore2_test_case, dense_fixed_length_column_m) {
  constexpr irs::doc_id_t MAX = 5000;
  irs::SegmentMeta meta;
  meta.name = "test_m";
  const bool has_encryption = bool(dir().attributes().encryption());

  irs::flush_state state{
    .name = meta.name,
    .doc_count = MAX,
  };
  TestResourceManager mem;

  {
    irs::columnstore2::writer writer(version(), mem.transactions,
                                     consolidation());
    writer.prepare(dir(), meta);

    {
      auto [id, column] = writer.push_column(
        {irs::type<irs::compression::none>::get(), {}, has_encryption},
        [](irs::bstring& out) {
          EXPECT_TRUE(out.empty());
          out += 42;
          return std::string_view{};
        });

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
        auto& stream = column(doc);
        stream.write_bytes(reinterpret_cast<const irs::byte_type*>(&doc),
                           sizeof doc);
      }
    }

    {
      auto [id, column] = writer.push_column(
        {irs::type<irs::compression::none>::get(), {}, has_encryption},
        [](irs::bstring& out) {
          EXPECT_TRUE(out.empty());
          out += 43;
          return std::string_view{};
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
    auto options = reader_options(mem);
    ASSERT_TRUE(reader.prepare(dir(), meta, options));
    ASSERT_EQ(2, reader.size());
    if (buffered()) {
      ASSERT_GT(mem.cached_columns.counter_, 0);
    } else {
      ASSERT_EQ(0, mem.cached_columns.counter_);
    }
    auto* header = reader.header(0);
    ASSERT_NE(nullptr, header);
    ASSERT_EQ(MAX, header->docs_count);
    ASSERT_EQ(0, header->docs_index);
    ASSERT_EQ(irs::doc_limits::min(), header->min);
    ASSERT_EQ(consolidation() ? ColumnType::kDenseFixed : ColumnType::kFixed,
              header->type);
    ASSERT_EQ(has_encryption
                ? (ColumnProperty::kEncrypt | ColumnProperty::kNoName)
                : ColumnProperty::kNoName,
              header->props);
  }
  ASSERT_EQ(0, mem.cached_columns.counter_);
  ASSERT_EQ(0, mem.readers.counter_);
  ASSERT_EQ(0, mem.transactions.counter_);
}

TEST_P(columnstore2_test_case, dense_fixed_length_column_mr) {
  constexpr irs::doc_id_t MAX = 5000;
  irs::SegmentMeta meta;
  meta.name = "test_mr";
  const bool has_encryption = bool(dir().attributes().encryption());

  irs::flush_state state{
    .name = meta.name,
    .doc_count = MAX,
  };
  TestResourceManager mem;
  mem.cached_columns.result_ = false;
  {
    irs::columnstore2::writer writer(version(), irs::IResourceManager::kNoop,
                                     consolidation());
    writer.prepare(dir(), meta);

    {
      auto [id, column] = writer.push_column(
        {irs::type<irs::compression::none>::get(), {}, has_encryption},
        [](irs::bstring& out) {
          EXPECT_TRUE(out.empty());
          out += 42;
          return std::string_view{};
        });

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= MAX; ++doc) {
        auto& stream = column(doc);
        stream.write_bytes(reinterpret_cast<const irs::byte_type*>(&doc),
                           sizeof doc);
      }
    }

    {
      auto [id, column] = writer.push_column(
        {irs::type<irs::compression::none>::get(), {}, has_encryption},
        [](irs::bstring& out) {
          EXPECT_TRUE(out.empty());
          out += 43;
          return std::string_view{};
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
    auto options = reader_options(mem);
    ASSERT_TRUE(reader.prepare(dir(), meta, options));
    ASSERT_EQ(2, reader.size());
    if (this->buffered()) {
      // we still record an attempt of allocating
      ASSERT_GT(mem.cached_columns.counter_, 0);
    } else {
      ASSERT_EQ(0, mem.cached_columns.counter_);
    }
    auto* header = reader.header(0);
    ASSERT_NE(nullptr, header);
    ASSERT_EQ(MAX, header->docs_count);
    ASSERT_EQ(0, header->docs_index);
    ASSERT_EQ(irs::doc_limits::min(), header->min);
    ASSERT_EQ(consolidation() ? ColumnType::kDenseFixed : ColumnType::kFixed,
              header->type);
    ASSERT_EQ(has_encryption
                ? (ColumnProperty::kEncrypt | ColumnProperty::kNoName)
                : ColumnProperty::kNoName,
              header->props);
  }
  // should not be a deallocation
  if (this->buffered()) {
    ASSERT_GT(mem.cached_columns.counter_, 0);
  } else {
    ASSERT_EQ(0, mem.cached_columns.counter_);
  }
}

TEST_P(columnstore2_test_case, dense_fixed_length_column) {
  constexpr irs::doc_id_t kMax = 1000000;
  irs::SegmentMeta meta;
  meta.name = "test";

  irs::flush_state state{
    .name = meta.name,
    .doc_count = kMax,
  };

  {
    irs::columnstore2::writer writer(version(), irs::IResourceManager::kNoop,
                                     consolidation());
    writer.prepare(dir(), meta);

    {
      auto [id, column] =
        writer.push_column(column_info(), [](irs::bstring& out) {
          EXPECT_TRUE(out.empty());
          out += 42;
          return std::string_view{};
        });

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
        auto& stream = column(doc);
        stream.write_bytes(reinterpret_cast<const irs::byte_type*>(&doc),
                           sizeof doc);
      }
    }

    {
      auto [id, column] =
        writer.push_column(column_info(), [](irs::bstring& out) {
          EXPECT_TRUE(out.empty());
          out += 43;
          return std::string_view{};
        });

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
        auto& stream = column(doc);
        stream.write_byte(static_cast<irs::byte_type>(doc & 0xFF));
      }
    }

    ASSERT_TRUE(writer.commit(state));
  }

  {
    irs::columnstore2::reader reader;
    ASSERT_TRUE(reader.prepare(dir(), meta, reader_options()));
    ASSERT_EQ(2, reader.size());

    {
      auto assert_payload = [this](irs::doc_id_t doc,
                                   const irs::payload& payload) {
        if (has_payload()) {
          ASSERT_EQ(sizeof doc, payload.value.size());
          const irs::doc_id_t actual_doc =
            *reinterpret_cast<const irs::doc_id_t*>(payload.value.data());
          EXPECT_EQ(doc, actual_doc);
        } else {
          ASSERT_TRUE(payload.value.empty());
        }
      };

      constexpr irs::field_id kColumnId = 0;

      auto* header = reader.header(kColumnId);
      ASSERT_NE(nullptr, header);
      ASSERT_EQ(kMax, header->docs_count);
      ASSERT_EQ(0, header->docs_index);
      ASSERT_EQ(irs::doc_limits::min(), header->min);
      ASSERT_EQ(consolidation() ? ColumnType::kDenseFixed : ColumnType::kFixed,
                header->type);
      ASSERT_EQ(column_property(ColumnProperty::kNoName), header->props);

      auto* column = reader.column(kColumnId);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(kMax, column->size());

      ASSERT_EQ(0, column->id());
      ASSERT_TRUE(irs::IsNull(column->name()));

      const auto header_payload = column->payload();
      ASSERT_EQ(1, header_payload.size());
      ASSERT_EQ(42, header_payload[0]);

      {
        auto it = column->iterator(hint());
        auto* document = irs::get<irs::document>(*it);
        ASSERT_NE(nullptr, document);
        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_NE(nullptr, payload);
        auto* cost = irs::get<irs::cost>(*it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(column->size(), cost->estimate());
        auto* score = irs::get<irs::score>(*it);
        ASSERT_NE(nullptr, score);
        ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

        for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
          ASSERT_EQ(doc, it->seek(doc));
          ASSERT_EQ(doc, it->seek(doc));
          assert_payload(doc, *payload);
        }
      }

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
        auto it = column->iterator(hint());
        auto* document = irs::get<irs::document>(*it);
        ASSERT_NE(nullptr, document);
        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_NE(nullptr, payload);
        auto* cost = irs::get<irs::cost>(*it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(column->size(), cost->estimate());
        auto* score = irs::get<irs::score>(*it);
        ASSERT_NE(nullptr, score);
        ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(doc, it->seek(doc));
        assert_payload(doc, *payload);
      }

      // seek + next
      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax;
           doc += 10000) {
        auto prev_it = column->iterator(hint());
        auto it = column->iterator(hint());
        auto* document = irs::get<irs::document>(*it);
        ASSERT_NE(nullptr, document);
        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_NE(nullptr, payload);
        auto* cost = irs::get<irs::cost>(*it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(column->size(), cost->estimate());
        auto* score = irs::get<irs::score>(*it);
        ASSERT_NE(nullptr, score);
        ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

        ASSERT_EQ(doc, it->seek(doc));
        assert_payload(doc, *payload);
        ASSERT_EQ(doc, it->seek(doc));
        assert_payload(doc, *payload);
        ASSERT_EQ(doc, it->seek(doc - 1));
        assert_payload(doc, *payload);
        assert_prev_doc(*it, *prev_it);

        auto next_it = column->iterator(hint());
        auto* next_payload = irs::get<irs::payload>(*next_it);
        ASSERT_NE(nullptr, next_payload);
        ASSERT_EQ(doc, next_it->seek(doc));
        assert_payload(doc, *next_payload);
        for (auto next_doc = doc + 1; next_doc <= kMax; ++next_doc) {
          ASSERT_TRUE(next_it->next());
          ASSERT_EQ(next_doc, next_it->value());
          assert_payload(next_doc, *next_payload);
          assert_prev_doc(*next_it, *prev_it);
        }
      }

      // next + seek
      {
        auto prev_it = column->iterator(hint());
        auto it = column->iterator(hint());
        auto* document = irs::get<irs::document>(*it);
        ASSERT_NE(nullptr, document);
        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_NE(nullptr, payload);
        auto* cost = irs::get<irs::cost>(*it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(column->size(), cost->estimate());
        ASSERT_TRUE(it->next());
        ASSERT_EQ(irs::doc_limits::min(), document->value);
        assert_prev_doc(*it, *prev_it);
        ASSERT_EQ(118774, it->seek(118774));
        assert_payload(118774, *payload);
        assert_prev_doc(*it, *prev_it);
        ASSERT_TRUE(irs::doc_limits::eof(it->seek(kMax + 1)));
      }
    }

    {
      auto assert_payload = [this](irs::doc_id_t doc,
                                   const irs::payload& payload) {
        if (has_payload()) {
          ASSERT_EQ(1, payload.value.size());
          EXPECT_EQ(static_cast<irs::byte_type>(doc & 0xFF), payload.value[0]);
        } else {
          ASSERT_TRUE(payload.value.empty());
        }
      };

      constexpr irs::field_id kColumnId = 1;

      auto* header = reader.header(kColumnId);
      ASSERT_NE(nullptr, header);
      ASSERT_EQ(kMax, header->docs_count);
      ASSERT_EQ(0, header->docs_index);
      ASSERT_EQ(irs::doc_limits::min(), header->min);
      ASSERT_EQ(consolidation() ? ColumnType::kDenseFixed : ColumnType::kFixed,
                header->type);
      ASSERT_EQ(column_property(ColumnProperty::kNoName), header->props);

      auto* column = reader.column(kColumnId);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(kMax, column->size());

      ASSERT_EQ(1, column->id());
      ASSERT_TRUE(irs::IsNull(column->name()));

      const auto header_payload = column->payload();
      ASSERT_EQ(1, header_payload.size());
      ASSERT_EQ(43, header_payload[0]);

      {
        auto prev_it = column->iterator(hint());
        auto it = column->iterator(hint());
        auto* document = irs::get<irs::document>(*it);
        ASSERT_NE(nullptr, document);
        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_NE(nullptr, payload);
        auto* cost = irs::get<irs::cost>(*it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(column->size(), cost->estimate());
        auto* score = irs::get<irs::score>(*it);
        ASSERT_NE(nullptr, score);
        ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

        for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
          ASSERT_EQ(doc, it->seek(doc));
          ASSERT_EQ(doc, it->seek(doc));
          assert_payload(doc, *payload);
          assert_prev_doc(*it, *prev_it);
        }
      }

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
        auto it = column->iterator(hint());
        auto* document = irs::get<irs::document>(*it);
        ASSERT_NE(nullptr, document);
        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_NE(nullptr, payload);
        auto* cost = irs::get<irs::cost>(*it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(column->size(), cost->estimate());
        auto* score = irs::get<irs::score>(*it);
        ASSERT_NE(nullptr, score);
        ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(doc, it->seek(doc));
        assert_payload(doc, *payload);
      }

      // seek + next
      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax;
           doc += 10000) {
        auto prev_it = column->iterator(hint());
        auto it = column->iterator(hint());
        auto* document = irs::get<irs::document>(*it);
        ASSERT_NE(nullptr, document);
        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_NE(nullptr, payload);
        auto* cost = irs::get<irs::cost>(*it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(column->size(), cost->estimate());
        auto* score = irs::get<irs::score>(*it);
        ASSERT_NE(nullptr, score);
        ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

        ASSERT_EQ(doc, it->seek(doc));
        assert_payload(doc, *payload);
        ASSERT_EQ(doc, it->seek(doc));
        assert_payload(doc, *payload);
        ASSERT_EQ(doc, it->seek(doc - 1));
        assert_payload(doc, *payload);
        assert_prev_doc(*it, *prev_it);

        auto next_it = column->iterator(hint());
        auto* next_payload = irs::get<irs::payload>(*next_it);
        ASSERT_NE(nullptr, next_payload);
        ASSERT_EQ(doc, next_it->seek(doc));
        assert_payload(doc, *next_payload);
        for (auto next_doc = doc + 1; next_doc <= kMax; ++next_doc) {
          ASSERT_TRUE(next_it->next());
          ASSERT_EQ(next_doc, next_it->value());
          assert_payload(next_doc, *next_payload);
          assert_prev_doc(*next_it, *prev_it);
        }
      }

      // next + seek
      {
        auto prev_it = column->iterator(hint());
        auto it = column->iterator(hint());
        auto* document = irs::get<irs::document>(*it);
        ASSERT_NE(nullptr, document);
        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_NE(nullptr, payload);
        auto* cost = irs::get<irs::cost>(*it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(column->size(), cost->estimate());
        ASSERT_TRUE(it->next());
        ASSERT_EQ(irs::doc_limits::min(), document->value);
        assert_prev_doc(*it, *prev_it);
        ASSERT_EQ(118774, it->seek(118774));
        assert_payload(static_cast<irs::byte_type>(118774 & 0xFF), *payload);
        assert_prev_doc(*it, *prev_it);
        ASSERT_TRUE(irs::doc_limits::eof(it->seek(kMax + 1)));
      }
    }
  }
}

TEST_P(columnstore2_test_case, dense_fixed_length_column_empty_tail) {
  constexpr irs::doc_id_t kMax = 1000000;
  irs::SegmentMeta meta;
  meta.name = "test";

  irs::flush_state state{
    .name = meta.name,
    .doc_count = kMax,
  };

  {
    irs::columnstore2::writer writer(version(), irs::IResourceManager::kNoop,
                                     consolidation());
    writer.prepare(dir(), meta);

    {
      auto [id, column] =
        writer.push_column(column_info(), [](irs::bstring& out) {
          EXPECT_TRUE(out.empty());
          out += 42;
          return std::string_view{};
        });

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
        auto& stream = column(doc);
        stream.write_bytes(reinterpret_cast<const irs::byte_type*>(&doc),
                           sizeof doc);
      }
    }

    {
      // empty column has to be removed
      [[maybe_unused]] auto [id, column] =
        writer.push_column(column_info(), [](auto&) {
          // Must not be called
          EXPECT_FALSE(true);
          return std::string_view{};
        });
    }

    ASSERT_TRUE(writer.commit(state));
  }

  {
    irs::columnstore2::reader reader;
    ASSERT_TRUE(reader.prepare(dir(), meta, reader_options()));
    ASSERT_EQ(1, reader.size());

    {
      auto assert_payload = [this](irs::doc_id_t doc,
                                   const irs::payload& payload) {
        if (has_payload()) {
          ASSERT_EQ(sizeof doc, payload.value.size());
          const irs::doc_id_t actual_doc =
            *reinterpret_cast<const irs::doc_id_t*>(payload.value.data());
          EXPECT_EQ(doc, actual_doc);
        } else {
          ASSERT_TRUE(payload.value.empty());
        }
      };

      constexpr irs::field_id kColumnId = 0;

      auto* header = reader.header(kColumnId);
      ASSERT_NE(nullptr, header);
      ASSERT_EQ(kMax, header->docs_count);
      ASSERT_EQ(0, header->docs_index);
      ASSERT_EQ(irs::doc_limits::min(), header->min);
      ASSERT_EQ(consolidation() ? ColumnType::kDenseFixed : ColumnType::kFixed,
                header->type);
      ASSERT_EQ(column_property(ColumnProperty::kNoName), header->props);

      auto* column = reader.column(kColumnId);
      ASSERT_NE(nullptr, column);
      ASSERT_EQ(kMax, column->size());

      ASSERT_EQ(0, column->id());
      ASSERT_TRUE(irs::IsNull(column->name()));

      const auto header_payload = column->payload();
      ASSERT_EQ(1, header_payload.size());
      ASSERT_EQ(42, header_payload[0]);

      {
        auto prev_it = column->iterator(hint());
        auto it = column->iterator(hint());
        auto* document = irs::get<irs::document>(*it);
        ASSERT_NE(nullptr, document);
        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_NE(nullptr, payload);
        auto* cost = irs::get<irs::cost>(*it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(column->size(), cost->estimate());
        auto* score = irs::get<irs::score>(*it);
        ASSERT_NE(nullptr, score);
        ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

        for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
          ASSERT_EQ(doc, it->seek(doc));
          ASSERT_EQ(doc, it->seek(doc));
          assert_payload(doc, *payload);
          assert_prev_doc(*it, *prev_it);
        }
      }

      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax; ++doc) {
        auto it = column->iterator(hint());
        auto* document = irs::get<irs::document>(*it);
        ASSERT_NE(nullptr, document);
        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_NE(nullptr, payload);
        auto* cost = irs::get<irs::cost>(*it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(column->size(), cost->estimate());
        auto* score = irs::get<irs::score>(*it);
        ASSERT_NE(nullptr, score);
        ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

        ASSERT_EQ(doc, it->seek(doc));
        ASSERT_EQ(doc, it->seek(doc));
        assert_payload(doc, *payload);
      }

      // seek + next
      for (irs::doc_id_t doc = irs::doc_limits::min(); doc <= kMax;
           doc += 10000) {
        auto prev_it = column->iterator(hint());
        auto it = column->iterator(hint());
        auto* document = irs::get<irs::document>(*it);
        ASSERT_NE(nullptr, document);
        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_NE(nullptr, payload);
        auto* cost = irs::get<irs::cost>(*it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(column->size(), cost->estimate());
        auto* score = irs::get<irs::score>(*it);
        ASSERT_NE(nullptr, score);
        ASSERT_TRUE(score->Func() == &irs::ScoreFunction::DefaultScore);

        ASSERT_EQ(doc, it->seek(doc));
        assert_payload(doc, *payload);
        ASSERT_EQ(doc, it->seek(doc));
        assert_payload(doc, *payload);
        ASSERT_EQ(doc, it->seek(doc - 1));
        assert_payload(doc, *payload);
        assert_prev_doc(*it, *prev_it);

        auto next_it = column->iterator(hint());
        auto* next_payload = irs::get<irs::payload>(*next_it);
        ASSERT_NE(nullptr, next_payload);
        ASSERT_EQ(doc, next_it->seek(doc));
        assert_payload(doc, *next_payload);
        for (auto next_doc = doc + 1; next_doc <= kMax; ++next_doc) {
          ASSERT_TRUE(next_it->next());
          ASSERT_EQ(next_doc, next_it->value());
          assert_payload(next_doc, *next_payload);
          assert_prev_doc(*next_it, *prev_it);
        }
      }

      // next + seek
      {
        auto prev_it = column->iterator(hint());
        auto it = column->iterator(hint());
        auto* document = irs::get<irs::document>(*it);
        ASSERT_NE(nullptr, document);
        auto* payload = irs::get<irs::payload>(*it);
        ASSERT_NE(nullptr, payload);
        auto* cost = irs::get<irs::cost>(*it);
        ASSERT_NE(nullptr, cost);
        ASSERT_EQ(column->size(), cost->estimate());
        ASSERT_TRUE(it->next());
        ASSERT_EQ(irs::doc_limits::min(), document->value);
        assert_prev_doc(*it, *prev_it);
        ASSERT_EQ(118774, it->seek(118774));
        assert_payload(118774, *payload);
        assert_prev_doc(*it, *prev_it);
        ASSERT_TRUE(irs::doc_limits::eof(it->seek(kMax + 1)));
      }
    }
  }
}

TEST_P(columnstore2_test_case, empty_columns) {
  constexpr irs::doc_id_t kMax = 1000000;
  irs::SegmentMeta meta;
  meta.name = "test";

  irs::flush_state state{
    .name = meta.name,
    .doc_count = kMax,
  };

  {
    irs::columnstore2::writer writer(version(), irs::IResourceManager::kNoop,
                                     consolidation());
    writer.prepare(dir(), meta);

    {
      // empty column must be removed
      [[maybe_unused]] auto [id, column] =
        writer.push_column(column_info(), [](auto&) {
          // Must not be called
          EXPECT_FALSE(true);
          return std::string_view{};
        });
    }

    {
      // empty column must be removed
      [[maybe_unused]] auto [id, column] =
        writer.push_column(column_info(), [](auto&) {
          // Must not be called
          EXPECT_FALSE(true);
          return std::string_view{};
        });
    }

    ASSERT_FALSE(writer.commit(state));
  }

  size_t count = 0;
  ASSERT_TRUE(dir().visit([&count](auto) {
    ++count;
    return false;
  }));

  ASSERT_EQ(0, count);
}

static constexpr auto kTestDirs =
  tests::getDirectories<tests::kTypesDefault | tests::kTypesRot13_16>();

INSTANTIATE_TEST_SUITE_P(
  columnstore2_test, columnstore2_test_case,
  ::testing::Combine(::testing::ValuesIn(kTestDirs),
                     ::testing::Values(irs::ColumnHint::kNormal,
                                       irs::ColumnHint::kConsolidation,
                                       irs::ColumnHint::kMask,
                                       irs::ColumnHint::kPrevDoc),
                     ::testing::Values(irs::columnstore2::Version::kMin),
                     ::testing::Values(true, false)),
  &columnstore2_test_case::to_string);
