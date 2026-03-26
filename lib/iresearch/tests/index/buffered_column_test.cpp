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
/// @author Vasiliy Nabatchikov
////////////////////////////////////////////////////////////////////////////////

#include "index/buffered_column.hpp"

#include "analysis/token_attributes.hpp"
#include "index/buffered_column_iterator.hpp"
#include "index/comparer.hpp"
#include "search/score.hpp"
#include "store/memory_directory.hpp"
#include "tests_shared.hpp"
#include "utils/bytes_utils.hpp"
#include "utils/lz4compression.hpp"
#include "utils/type_limits.hpp"

namespace {

class Comparator final : public irs::Comparer {
  int CompareImpl(irs::bytes_view lhs,
                  irs::bytes_view rhs) const noexcept final {
    const auto* plhs = lhs.data();
    const auto* prhs = rhs.data();

    if (!plhs && !prhs) {
      return 0;
    }

    if (!plhs) {
      return -1;
    }

    if (!prhs) {
      return 1;
    }

    const auto lhs_value = irs::vread<uint32_t>(plhs);
    const auto rhs_value = irs::vread<uint32_t>(prhs);

    if (lhs_value < rhs_value) {
      return -1;
    }

    if (rhs_value < lhs_value) {
      return 1;
    }

    return 0;
  }
};

void AssertIteratorNext(const irs::BufferedColumn& column,
                        std::span<const uint32_t> expected_values) {
  irs::BufferedColumnIterator it{column.Index(), column.Data()};
  ASSERT_EQ(expected_values.size(), irs::cost::extract(it));
  ASSERT_EQ(nullptr, irs::get<irs::score>(it));
  auto* doc = irs::get<irs::document>(it);
  ASSERT_NE(nullptr, doc);
  auto* payload = irs::get<irs::payload>(it);
  ASSERT_NE(nullptr, payload);

  ASSERT_FALSE(irs::doc_limits::valid(doc->value));
  ASSERT_EQ(it.value(), doc->value);
  for (const auto expected_value : expected_values) {
    ASSERT_TRUE(it.next());
    ASSERT_EQ(it.value(), doc->value);
    ASSERT_FALSE(payload->value.empty());
    const auto* data = payload->value.data();
    const auto value = irs::vread<uint32_t>(data);
    ASSERT_EQ(expected_value, value);
  }
  ASSERT_FALSE(it.next());
  ASSERT_TRUE(irs::doc_limits::eof(doc->value));
  ASSERT_FALSE(it.next());
  ASSERT_TRUE(irs::doc_limits::eof(doc->value));
}

void AssertIteratorSeekStateful(const irs::BufferedColumn& column,
                                std::span<const uint32_t> expected_values) {
  irs::BufferedColumnIterator it{column.Index(), column.Data()};
  ASSERT_EQ(expected_values.size(), irs::cost::extract(it));
  ASSERT_EQ(nullptr, irs::get<irs::score>(it));
  auto* doc = irs::get<irs::document>(it);
  ASSERT_NE(nullptr, doc);
  auto* payload = irs::get<irs::payload>(it);
  ASSERT_NE(nullptr, payload);

  ASSERT_FALSE(irs::doc_limits::valid(doc->value));
  ASSERT_EQ(it.value(), doc->value);
  irs::doc_id_t expected_doc = irs::doc_limits::min();
  for (const auto expected_value : expected_values) {
    ASSERT_EQ(expected_doc, it.seek(expected_doc));
    ASSERT_EQ(it.value(), doc->value);
    ASSERT_EQ(expected_doc, it.seek(expected_doc));
    ASSERT_EQ(it.value(), doc->value);
    ASSERT_FALSE(payload->value.empty());
    const auto* data = payload->value.data();
    const auto value = irs::vread<uint32_t>(data);
    ASSERT_EQ(expected_value, value);
    ++expected_doc;
  }
  ASSERT_FALSE(it.next());
  ASSERT_TRUE(irs::doc_limits::eof(doc->value));
  ASSERT_FALSE(it.next());
  ASSERT_TRUE(irs::doc_limits::eof(doc->value));
}

void AssertIteratorSeekStateles(const irs::BufferedColumn& column,
                                std::span<const uint32_t> expected_values) {
  for (irs::doc_id_t expected_doc = irs::doc_limits::min();
       const auto expected_value : expected_values) {
    irs::BufferedColumnIterator it{column.Index(), column.Data()};
    auto* payload = irs::get<irs::payload>(it);
    ASSERT_NE(nullptr, payload);
    ASSERT_EQ(expected_doc, it.seek(expected_doc));
    ASSERT_EQ(it.value(), expected_doc);
    ASSERT_EQ(expected_doc, it.seek(expected_doc));
    ASSERT_EQ(it.value(), expected_doc);
    ASSERT_FALSE(payload->value.empty());
    const auto* data = payload->value.data();
    const auto value = irs::vread<uint32_t>(data);
    ASSERT_EQ(expected_value, value);
    ++expected_doc;
  }
}

void AssertIteratorCornerCases(const irs::BufferedColumn& column,
                               std::span<const uint32_t> expected_values) {
  // next + seek to the last element
  {
    irs::BufferedColumnIterator it{column.Index(), column.Data()};
    ASSERT_FALSE(irs::doc_limits::valid(it.value()));
    if (!expected_values.empty()) {
      ASSERT_TRUE(it.next());
      ASSERT_EQ(1, it.value());
    } else {
      ASSERT_FALSE(it.next());
    }
    ASSERT_EQ(
      expected_values.empty() ? irs::doc_limits::eof() : expected_values.size(),
      it.seek(expected_values.size()));
    ASSERT_FALSE(it.next());
  }

  // next + seek to eof
  {
    irs::BufferedColumnIterator it{column.Index(), column.Data()};
    ASSERT_FALSE(irs::doc_limits::valid(it.value()));
    if (!expected_values.empty()) {
      ASSERT_TRUE(it.next());
      ASSERT_EQ(1, it.value());
    } else {
      ASSERT_FALSE(it.next());
    }
    ASSERT_TRUE(irs::doc_limits::eof(it.seek(expected_values.size() + 42)));
  }

  // Seek to irs::doc_limits::invalid()
  {
    irs::BufferedColumnIterator it{column.Index(), column.Data()};
    ASSERT_FALSE(irs::doc_limits::valid(it.value()));
    const auto value = it.seek(irs::doc_limits::invalid());
    ASSERT_EQ(value, it.value());
    if (!expected_values.empty()) {
      ASSERT_EQ(value, 1);
    } else {
      ASSERT_EQ(value, irs::doc_limits::eof());
    }
  }

  // Seek to irs::doc_limits::eof()
  {
    irs::BufferedColumnIterator it{column.Index(), column.Data()};
    ASSERT_FALSE(irs::doc_limits::valid(it.value()));
    ASSERT_TRUE(irs::doc_limits::eof(it.seek(irs::doc_limits::eof())));
    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }
}

void AssertIteratorBackwardsNextStateless(
  const irs::BufferedColumn& column,
  std::span<const uint32_t> expected_values) {
  for (auto expected_value = expected_values.rbegin(),
            end = expected_values.rend();
       expected_value != end; ++expected_value) {
    auto expected_doc =
      static_cast<irs::doc_id_t>(std::distance(expected_value, end));
    irs::BufferedColumnIterator it{column.Index(), column.Data()};
    ASSERT_FALSE(irs::doc_limits::valid(it.value()));
    ASSERT_EQ(expected_doc, it.seek(expected_doc));

    for (auto expected_it = expected_values.begin() + expected_doc;
         expected_it != expected_values.end(); ++expected_it) {
      ++expected_doc;
      ASSERT_TRUE(it.next());
      auto* payload = irs::get<irs::payload>(it);
      ASSERT_NE(nullptr, payload);
      ASSERT_EQ(expected_doc, it.seek(expected_doc));
      ASSERT_EQ(it.value(), expected_doc);
      const auto* data = payload->value.data();
      const auto value = irs::vread<uint32_t>(data);
      ASSERT_EQ(*expected_it, value);
    }

    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }
}

void AssertIteratorBackwardsNextStateful(
  const irs::BufferedColumn& column,
  std::span<const uint32_t> expected_values) {
  irs::BufferedColumnIterator it{column.Index(), column.Data()};

  for (auto expected_value = expected_values.rbegin(),
            end = expected_values.rend();
       expected_value != end; ++expected_value) {
    auto expected_doc =
      static_cast<irs::doc_id_t>(std::distance(expected_value, end));
    ASSERT_EQ(expected_doc, it.seek(expected_doc));

    for (auto expected_it = expected_values.begin() + expected_doc;
         expected_it != expected_values.end(); ++expected_it) {
      ++expected_doc;
      ASSERT_TRUE(it.next());
      auto* payload = irs::get<irs::payload>(it);
      ASSERT_NE(nullptr, payload);
      ASSERT_EQ(expected_doc, it.seek(expected_doc));
      ASSERT_EQ(it.value(), expected_doc);
      const auto* data = payload->value.data();
      const auto value = irs::vread<uint32_t>(data);
      ASSERT_EQ(*expected_it, value);
    }

    ASSERT_FALSE(it.next());
    ASSERT_TRUE(irs::doc_limits::eof(it.value()));
  }
}

void AssertIterator(const irs::BufferedColumn& column,
                    std::span<const uint32_t> expected_values) {
  AssertIteratorNext(column, expected_values);
  AssertIteratorSeekStateful(column, expected_values);
  AssertIteratorSeekStateles(column, expected_values);
  AssertIteratorCornerCases(column, expected_values);
  AssertIteratorBackwardsNextStateful(column, expected_values);
  AssertIteratorBackwardsNextStateless(column, expected_values);
}

const Comparator kLess;

}  // namespace

struct BufferedColumnTestCase
  : public virtual test_param_base<std::string_view> {
  bool supports_columnstore_headers() const noexcept {
    // old formats don't support columnstore headers
    constexpr std::string_view kOldFormats[]{"1_0", "1_1", "1_2", "1_3",
                                             "1_3simd"};

    const auto it =
      std::find(std::begin(kOldFormats), std::end(kOldFormats), GetParam());
    return std::end(kOldFormats) == it;
  }
};

TEST_P(BufferedColumnTestCase, Ctor) {
  SimpleMemoryAccounter memory;
  irs::BufferedColumn col({irs::type<irs::compression::lz4>::get(), {}, false},
                          memory);
  ASSERT_TRUE(col.Empty());
  ASSERT_EQ(0, col.Size());
  ASSERT_EQ(0, col.MemoryActive());
  ASSERT_GE(col.MemoryReserved(), 0);
#if defined(_MSC_VER) && defined(IRESEARCH_DEBUG)
  ASSERT_GT(memory.counter_, 0);
#else
  ASSERT_EQ(0, memory.counter_);
#endif
}

TEST_P(BufferedColumnTestCase, FlushEmpty) {
  TestResourceManager memory;
  irs::BufferedColumn col({irs::type<irs::compression::lz4>::get(), {}, false},
                          memory.cached_columns);
  ASSERT_TRUE(col.Empty());
  ASSERT_EQ(0, col.Size());
  ASSERT_EQ(0, col.MemoryActive());
  ASSERT_GE(col.MemoryReserved(), 0);
#if defined(_MSC_VER) && defined(IRESEARCH_DEBUG)
  ASSERT_GT(memory.cached_columns.counter_, 0);
#else
  ASSERT_EQ(0, memory.cached_columns.counter_);
#endif

  irs::field_id column_id;
  irs::DocMap order;
  irs::memory_directory dir;
  irs::SegmentMeta segment;
  segment.name = "123";
  auto codec = irs::formats::get(GetParam());
  ASSERT_NE(nullptr, codec);

  // write sorted column
  {
    auto writer =
      codec->get_columnstore_writer(false, *memory.options.transactions);
    ASSERT_NE(nullptr, writer);

    writer->prepare(dir, segment);

    std::tie(order, column_id) = col.Flush(
      *writer,
      [](auto&) {
        // Must not be called
        EXPECT_TRUE(false);
        return std::string_view{};
      },
      0, kLess);
    ASSERT_TRUE(col.Empty());
    ASSERT_EQ(0, col.Size());
    ASSERT_TRUE(col.Empty());
    ASSERT_EQ(0, col.MemoryActive());
    ASSERT_GE(col.MemoryReserved(), 0);
    ASSERT_EQ(0, order.size());
    ASSERT_FALSE(irs::field_limits::valid(column_id));

    const irs::flush_state state{
      .dir = &dir, .name = segment.name, .doc_count = 0};

    ASSERT_FALSE(writer->commit(state));  // nothing to commit
  }
  ASSERT_EQ(0, memory.transactions.counter_);
  // read sorted column
  {
    auto reader = codec->get_columnstore_reader();
    ASSERT_NE(nullptr, reader);
    ASSERT_FALSE(reader->prepare(dir, segment));
  }
}

TEST_P(BufferedColumnTestCase, InsertDuplicates) {
  constexpr uint32_t values[] = {
    19, 45, 27, 1,  73, 98, 46, 48, 38,  20, 60, 91, 61, 80, 44,  53, 88,
    75, 63, 39, 68, 20, 11, 78, 21, 100, 87, 8,  9,  63, 41, 35,  82, 69,
    56, 49, 6,  46, 59, 19, 16, 58, 15,  21, 46, 23, 99, 78, 18,  89, 77,
    7,  2,  15, 97, 10, 5,  75, 13, 7,   77, 12, 15, 70, 95, 42,  29, 26,
    81, 82, 74, 53, 84, 13, 95, 84, 51,  9,  19, 18, 21, 82, 22,  91, 70,
    68, 14, 73, 30, 70, 38, 85, 98, 79,  75, 38, 79, 85, 85, 100, 91};

  irs::SegmentMeta segment;
  segment.name = "123";

  irs::memory_directory dir;
  irs::field_id column_id;
  irs::DocMap order;

  auto codec = irs::formats::get(GetParam());
  ASSERT_NE(nullptr, codec);
  TestResourceManager memory;
  // write sorted column
  {
    auto writer =
      codec->get_columnstore_writer(false, *memory.options.transactions);
    ASSERT_NE(nullptr, writer);

    writer->prepare(dir, segment);

    irs::BufferedColumn col(
      {irs::type<irs::compression::none>::get(), {}, true},
      memory.cached_columns);
    ASSERT_TRUE(col.Empty());
    ASSERT_EQ(0, col.Size());
    ASSERT_EQ(0, col.MemoryActive());
    ASSERT_GE(col.MemoryReserved(), 0);

    irs::doc_id_t doc = irs::doc_limits::min();
    for (const auto value : values) {
      // write value
      col.Prepare(doc);
      col.write_vint(value);

      // write and rollback
      col.Prepare(doc);
      col.write_vint(value);
      col.reset();

      ++doc;
    }
    ASSERT_EQ(0, col.Size());
    ASSERT_TRUE(col.Empty());

    ASSERT_GE(col.MemoryActive(), 0);
    ASSERT_GE(col.MemoryReserved(), 0);
    // SSO should do the trick without allocations
#if defined(_MSC_VER) && defined(IRESEARCH_DEBUG)
    ASSERT_GT(memory.cached_columns.counter_, 0);
#else
    ASSERT_EQ(0, memory.cached_columns.counter_);
#endif

    std::tie(order, column_id) = col.Flush(
      *writer,
      [](irs::bstring&) {
        // must not be called
        EXPECT_TRUE(false);
        return std::string_view{};
      },
      std::size(values), kLess);
    ASSERT_TRUE(col.Empty());
    ASSERT_EQ(0, col.Size());
    ASSERT_TRUE(col.Empty());
    ASSERT_EQ(0, col.MemoryActive());
    ASSERT_GE(col.MemoryReserved(), 0);
    ASSERT_EQ(0, order.size());  // already sorted
    ASSERT_FALSE(irs::field_limits::valid(column_id));

    const irs::flush_state state{
      .dir = &dir, .name = segment.name, .doc_count = std::size(values)};

    ASSERT_FALSE(writer->commit(state));
  }

  // read sorted column
  {
    auto reader = codec->get_columnstore_reader();
    ASSERT_NE(nullptr, reader);
    ASSERT_FALSE(reader->prepare(dir, segment));
  }
}

TEST_P(BufferedColumnTestCase, Sort) {
  constexpr uint32_t values[] = {
    19, 45, 27, 1,  73, 98, 46, 48, 38,  20, 60, 91, 61, 80, 44,  53, 88,
    75, 63, 39, 68, 20, 11, 78, 21, 100, 87, 8,  9,  63, 41, 35,  82, 69,
    56, 49, 6,  46, 59, 19, 16, 58, 15,  21, 46, 23, 99, 78, 18,  89, 77,
    7,  2,  15, 97, 10, 5,  75, 13, 7,   77, 12, 15, 70, 95, 42,  29, 26,
    81, 82, 74, 53, 84, 13, 95, 84, 51,  9,  19, 18, 21, 82, 22,  91, 70,
    68, 14, 73, 30, 70, 38, 85, 98, 79,  75, 38, 79, 85, 85, 100, 91};

  irs::SegmentMeta segment;
  segment.name = "123";

  irs::memory_directory dir;
  irs::field_id column_id;
  irs::DocMap order;

  auto codec = irs::formats::get(GetParam());
  ASSERT_NE(nullptr, codec);
  TestResourceManager memory;
  // write sorted column
  {
    auto writer = codec->get_columnstore_writer(false, memory.transactions);
    ASSERT_NE(nullptr, writer);

    writer->prepare(dir, segment);

    irs::BufferedColumn col({irs::type<irs::compression::lz4>::get(), {}, true},
                            memory.cached_columns);
    ASSERT_TRUE(col.Empty());
    ASSERT_EQ(0, col.Size());
    ASSERT_EQ(0, col.MemoryActive());
    ASSERT_GE(col.MemoryReserved(), 0);

    irs::doc_id_t doc = irs::doc_limits::min();
    for (const auto value : values) {
      // write value
      col.Prepare(doc);
      col.write_vint(value);

      // write and rollback
      col.Prepare(++doc);
      col.write_vint(value);
      col.reset();
    }
    ASSERT_EQ(std::size(values), col.Size());
    ASSERT_EQ(col.Index().size(), col.Size());
    ASSERT_FALSE(col.Empty());

    ASSERT_GE(col.MemoryActive(), 0);
    ASSERT_GE(col.MemoryReserved(), 0);
    ASSERT_GE(memory.cached_columns.counter_, col.MemoryReserved());
    std::tie(order, column_id) = col.Flush(
      *writer,
      [](irs::bstring& out) {
        EXPECT_TRUE(out.empty());
        out += 42;
        return std::string_view{};
      },
      std::size(values), kLess);
    ASSERT_FALSE(col.Empty());

    AssertIterator(col, values);

    col.Clear();
    ASSERT_TRUE(col.Empty());
    ASSERT_EQ(0, col.Size());
    ASSERT_TRUE(col.Empty());
    ASSERT_EQ(0, col.MemoryActive());
    ASSERT_GE(col.MemoryReserved(), 0);
    ASSERT_EQ(1 + std::size(values), order.size());
    ASSERT_TRUE(irs::field_limits::valid(column_id));

    AssertIterator(col, {});

    const irs::flush_state state{
      .dir = &dir, .name = segment.name, .doc_count = std::size(values)};

    ASSERT_TRUE(writer->commit(state));
  }

  std::vector<uint32_t> sorted_values(std::begin(values), std::end(values));
  std::sort(sorted_values.begin(), sorted_values.end());

  // check order
  {
    uint32_t values_by_order[std::size(values)];
    for (size_t i = 0; i < std::size(values); ++i) {
      values_by_order[order[irs::doc_limits::min() + i] -
                      irs::doc_limits::min()] = values[i];
    }

    ASSERT_TRUE(
      std::is_sorted(std::begin(values_by_order), std::end(values_by_order)));
  }

  // read sorted column
  {
    auto reader = codec->get_columnstore_reader();
    ASSERT_NE(nullptr, reader);
    ASSERT_TRUE(reader->prepare(dir, segment));
    ASSERT_EQ(1, reader->size());

    auto column = reader->column(column_id);
    ASSERT_NE(nullptr, column);

    if (const auto header_payload = column->payload();
        supports_columnstore_headers()) {
      ASSERT_EQ(1, header_payload.size());
      ASSERT_EQ(42, header_payload[0]);
    } else {
      ASSERT_TRUE(irs::IsNull(header_payload));
    }

    auto it = column->iterator(irs::ColumnHint::kNormal);
    auto* payload = irs::get<irs::payload>(*it);
    ASSERT_TRUE(payload);

    auto begin = sorted_values.begin();
    irs::doc_id_t doc = irs::doc_limits::min();
    while (it->next()) {
      ASSERT_EQ(doc, it->value());
      const auto* pvalue = payload->value.data();
      ASSERT_NE(nullptr, pvalue);
      ASSERT_EQ(*begin, irs::vread<uint32_t>(pvalue));
      ++doc;
      ++begin;
    }
    ASSERT_EQ(sorted_values.end(), begin);
    ASSERT_FALSE(it->next());
  }
}

INSTANTIATE_TEST_SUITE_P(BufferedColumnTest, BufferedColumnTestCase,
                         ::testing::Values("1_0", "1_4"));
