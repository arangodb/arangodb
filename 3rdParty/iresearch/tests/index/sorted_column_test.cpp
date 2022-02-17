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

#ifndef IRESEARCH_DLL

#include "tests_shared.hpp"
#include "index/sorted_column.hpp"
#include "index/comparer.hpp"
#include "analysis/token_attributes.hpp"
#include "store/memory_directory.hpp"
#include "utils/bitvector.hpp"
#include "utils/bytes_utils.hpp"
#include "utils/lz4compression.hpp"
#include "utils/type_limits.hpp"

// FIXME check gaps && deleted docs

struct sorted_column_test_case
    : public virtual test_param_base<std::string_view> {
  bool supports_columnstore_headers() const noexcept {
    // old formats don't support columnstore headers
    constexpr std::string_view kOldFormats[] {
      "1_0", "1_1", "1_2", "1_3", "1_3simd" };

    const auto it = std::find(std::begin(kOldFormats),
                              std::end(kOldFormats),
                              GetParam());
    return std::end(kOldFormats) == it;
  }
};

TEST_P(sorted_column_test_case, ctor) {
  irs::sorted_column col({ irs::type<irs::compression::lz4>::get(), {}, false });
  ASSERT_TRUE(col.empty());
  ASSERT_EQ(0, col.size());
  ASSERT_EQ(0, col.memory_active());
  ASSERT_GE(col.memory_reserved(), 0);
}

TEST_P(sorted_column_test_case, flush_empty) {
  irs::sorted_column col({ irs::type<irs::compression::lz4>::get(), {}, false });
  ASSERT_TRUE(col.empty());
  ASSERT_EQ(0, col.size());
  ASSERT_EQ(0, col.memory_active());
  ASSERT_GE(col.memory_reserved(), 0);

  irs::field_id column_id;
  irs::doc_map order;
  irs::memory_directory dir;
  irs::segment_meta segment;
  segment.name = "123";
  auto codec = irs::formats::get(GetParam());
  ASSERT_NE(nullptr, codec);

  struct comparator final : irs::comparer {
    virtual bool less(const irs::bytes_ref& lhs, const irs::bytes_ref& rhs) const noexcept override {
      const auto* plhs = lhs.c_str();
      const auto* prhs = rhs.c_str();

      if (!plhs && !prhs) {
        return false;
      }

      if (!plhs) {
        return true;
      }

      if (!prhs) {
        return false;
      }

      const auto lhs_value = irs::vread<uint32_t>(plhs);
      const auto rhs_value = irs::vread<uint32_t>(prhs);

      return lhs_value < rhs_value;
    }
  } less;

  // write sorted column
  {
    auto writer = codec->get_columnstore_writer(false);
    ASSERT_NE(nullptr, writer);

    writer->prepare(dir, segment);

    std::tie(order, column_id) = col.flush(
        *writer,
        [](auto&){
          // Must not be called
          EXPECT_TRUE(false);
          return irs::string_ref::NIL;
        },
        0, less);
    ASSERT_TRUE(col.empty());
    ASSERT_EQ(0, col.size());
    ASSERT_TRUE(col.empty());
    ASSERT_EQ(0, col.memory_active());
    ASSERT_GE(col.memory_reserved(), 0);
    ASSERT_EQ(0, order.size());
    ASSERT_TRUE(irs::type_limits<irs::type_t::field_id_t>::valid(column_id));

    irs::flush_state state;
    state.dir = &dir;
    state.doc_count = 0;
    state.name = segment.name;

    ASSERT_FALSE(writer->commit(state)); // nothing to commit
  }

  // read sorted column
  {
    auto reader = codec->get_columnstore_reader();
    ASSERT_NE(nullptr, reader);
    ASSERT_FALSE(reader->prepare(dir, segment));
  }
}

TEST_P(sorted_column_test_case, insert_duplicates) {
  const uint32_t values[] = {
    19,45,27,1,73,98,46,48,38,20,60,91,61,80,44,53,88,
    75,63,39,68,20,11,78,21,100,87,8,9,63,41,35,82,69,
    56,49,6,46,59,19,16,58,15,21,46,23,99,78,18,89,77,
    7,2,15,97,10,5,75,13,7,77,12,15,70,95,42,29,26,81,
    82,74,53,84,13,95,84,51,9,19,18,21,82,22,91,70,68,
    14,73,30,70,38,85,98,79,75,38,79,85,85,100,91
  };

  struct comparator final : irs::comparer {
    virtual bool less(const irs::bytes_ref& lhs, const irs::bytes_ref& rhs) const noexcept override {
      const auto* plhs = lhs.c_str();
      const auto* prhs = rhs.c_str();

      if (!plhs && !prhs) {
        return false;
      }

      if (!plhs) {
        return true;
      }

      if (!prhs) {
        return false;
      }

      const auto lhs_value = irs::vread<uint32_t>(plhs);
      const auto rhs_value = irs::vread<uint32_t>(prhs);

      return lhs_value < rhs_value;
    }
  } less;

  irs::segment_meta segment;
  segment.name = "123";

  irs::memory_directory dir;
  irs::field_id column_id;
  irs::doc_map order;

  auto codec = irs::formats::get(GetParam());
  ASSERT_NE(nullptr, codec);

  // write sorted column
  {
    auto writer = codec->get_columnstore_writer(false);
    ASSERT_NE(nullptr, writer);

    writer->prepare(dir, segment);

    irs::sorted_column col({ irs::type<irs::compression::none>::get(), {}, true });
    ASSERT_TRUE(col.empty());
    ASSERT_EQ(0, col.size());
    ASSERT_EQ(0, col.memory_active());
    ASSERT_GE(col.memory_reserved(), 0);

    irs::doc_id_t doc = irs::type_limits<irs::type_t::doc_id_t>::min();
    for (const auto value : values) {
      // write value
      col.prepare(doc);
      col.write_vint(value);

      // write and rollback
      col.prepare(doc);
      col.write_vint(value);
      col.reset();

      ++doc;
    }
    ASSERT_EQ(0, col.size());
    ASSERT_TRUE(col.empty());

    ASSERT_GE(col.memory_active(), 0);
    ASSERT_GE(col.memory_reserved(), 0);

    std::tie(order, column_id) = col.flush(
        *writer,
        [](irs::bstring& out) {
          EXPECT_TRUE(out.empty());
          out += 42;
          return irs::string_ref::NIL;
        },
        IRESEARCH_COUNTOF(values), less);
    ASSERT_TRUE(col.empty());
    ASSERT_EQ(0, col.size());
    ASSERT_TRUE(col.empty());
    ASSERT_EQ(0, col.memory_active());
    ASSERT_GE(col.memory_reserved(), 0);
    ASSERT_EQ(0, order.size()); // already sorted
    ASSERT_TRUE(irs::type_limits<irs::type_t::field_id_t>::valid(column_id));

    irs::flush_state state;
    state.dir = &dir;
    state.doc_count = IRESEARCH_COUNTOF(values);
    state.name = segment.name;

    ASSERT_TRUE(writer->commit(state));
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
      ASSERT_TRUE(header_payload.null());
    }

    auto it = column->iterator(false);
    auto* payload = irs::get<irs::payload>(*it);
    ASSERT_TRUE(!payload || payload->value.null());

    irs::doc_id_t doc = irs::type_limits<irs::type_t::doc_id_t>::min();
    while (it->next()) {
      ASSERT_EQ(doc, it->value());
      ++doc;
    }
    ASSERT_FALSE(it->next());
  }
}

TEST_P(sorted_column_test_case, sort) {
  const uint32_t values[] = {
    19,45,27,1,73,98,46,48,38,20,60,91,61,80,44,53,88,
    75,63,39,68,20,11,78,21,100,87,8,9,63,41,35,82,69,
    56,49,6,46,59,19,16,58,15,21,46,23,99,78,18,89,77,
    7,2,15,97,10,5,75,13,7,77,12,15,70,95,42,29,26,81,
    82,74,53,84,13,95,84,51,9,19,18,21,82,22,91,70,68,
    14,73,30,70,38,85,98,79,75,38,79,85,85,100,91
  };

  struct comparator final : irs::comparer {
    virtual bool less(const irs::bytes_ref& lhs, const irs::bytes_ref& rhs) const noexcept override {
      const auto* plhs = lhs.c_str();
      const auto* prhs = rhs.c_str();

      if (!plhs && !prhs) {
        return false;
      }

      if (!plhs) {
        return true;
      }

      if (!prhs) {
        return false;
      }

      const auto lhs_value = irs::vread<uint32_t>(plhs);
      const auto rhs_value = irs::vread<uint32_t>(prhs);

      return lhs_value < rhs_value;
    }
  } less;

  irs::segment_meta segment;
  segment.name = "123";

  irs::memory_directory dir;
  irs::field_id column_id;
  irs::doc_map order;

  auto codec = irs::formats::get(GetParam());
  ASSERT_NE(nullptr, codec);

  // write sorted column
  {
    auto writer = codec->get_columnstore_writer(false);
    ASSERT_NE(nullptr, writer);

    writer->prepare(dir, segment);

    irs::sorted_column col({ irs::type<irs::compression::lz4>::get(), {}, true });
    ASSERT_TRUE(col.empty());
    ASSERT_EQ(0, col.size());
    ASSERT_EQ(0, col.memory_active());
    ASSERT_GE(col.memory_reserved(), 0);

    irs::doc_id_t doc = irs::type_limits<irs::type_t::doc_id_t>::min();
    for (const auto value : values) {
      // write value
      col.prepare(doc);
      col.write_vint(value);

      // write and rollback
      col.prepare(++doc);
      col.write_vint(value);
      col.reset();
    }
    ASSERT_EQ(IRESEARCH_COUNTOF(values), col.size());
    ASSERT_FALSE(col.empty());

    ASSERT_GE(col.memory_active(), 0);
    ASSERT_GE(col.memory_reserved(), 0);

    std::tie(order, column_id) = col.flush(
        *writer,
        [](irs::bstring& out) {
          EXPECT_TRUE(out.empty());
          out += 42;
          return irs::string_ref::NIL;
        },
        IRESEARCH_COUNTOF(values), less);
    ASSERT_TRUE(col.empty());
    ASSERT_EQ(0, col.size());
    ASSERT_TRUE(col.empty());
    ASSERT_EQ(0, col.memory_active());
    ASSERT_GE(col.memory_reserved(), 0);
    ASSERT_EQ(1+IRESEARCH_COUNTOF(values), order.size());
    ASSERT_TRUE(irs::type_limits<irs::type_t::field_id_t>::valid(column_id));

    irs::flush_state state;
    state.dir = &dir;
    state.doc_count = IRESEARCH_COUNTOF(values);
    state.name = segment.name;

    ASSERT_TRUE(writer->commit(state));
  }

  std::vector<uint32_t> sorted_values(values, values + IRESEARCH_COUNTOF(values));
  std::sort(sorted_values.begin(), sorted_values.end());

  // check order
  {
    uint32_t values_by_order[IRESEARCH_COUNTOF(values)];
    for (size_t i = 0; i < IRESEARCH_COUNTOF(values); ++i) {
      values_by_order[order[irs::doc_limits::min() + i] - irs::doc_limits::min()] = values[i];
    }

    ASSERT_TRUE(std::is_sorted(std::begin(values_by_order), std::end(values_by_order)));
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
      ASSERT_TRUE(header_payload.null());
    }

    auto it = column->iterator(false);
    auto* payload = irs::get<irs::payload>(*it);
    ASSERT_TRUE(payload);

    auto begin = sorted_values.begin();
    irs::doc_id_t doc = irs::type_limits<irs::type_t::doc_id_t>::min();
    while (it->next()) {
      ASSERT_EQ(doc, it->value());
      const auto* pvalue = payload->value.c_str();
      ASSERT_NE(nullptr, pvalue);
      ASSERT_EQ(*begin, irs::vread<uint32_t>(pvalue));
      ++doc;
      ++begin;
    }
    ASSERT_EQ(sorted_values.end(), begin);
    ASSERT_FALSE(it->next());
  }
}

INSTANTIATE_TEST_SUITE_P(
    sorted_column_test,
    sorted_column_test_case,
    ::testing::Values("1_0", "1_4"));

#endif // IRESEARCH_DLL
