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
#include "analysis/token_attributes.hpp"
#include "store/memory_directory.hpp"
#include "utils/bitvector.hpp"
#include "utils/bytes_utils.hpp"
#include "utils/type_limits.hpp"

// FIXME check gaps && deleted docs

TEST(sorted_colum_test, sort) {
  const uint32_t values[] = {
    19,45,27,1,73,98,46,48,38,20,60,91,61,80,44,53,88,
    75,63,39,68,20,11,78,21,100,87,8,9,63,41,35,82,69,
    56,49,6,46,59,19,16,58,15,21,46,23,99,78,18,89,77,
    7,2,15,97,10,5,75,13,7,77,12,15,70,95,42,29,26,81,
    82,74,53,84,13,95,84,51,9,19,18,21,82,22,91,70,68,
    14,73,30,70,38,85,98,79,75,38,79,85,85,100,91
  };

  struct comparator final : irs::comparer {
    virtual bool less(const irs::bytes_ref& lhs, const irs::bytes_ref& rhs) const NOEXCEPT override {
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
  irs::bitvector docs_mask;
  irs::field_id column_id;
  irs::doc_map order;

  auto codec = irs::formats::get("1_0");
  ASSERT_NE(nullptr, codec);

  // write sorted column
  {
    auto writer = codec->get_columnstore_writer();
    ASSERT_NE(nullptr, writer);

    writer->prepare(dir, segment);

    irs::sorted_column col;
    ASSERT_TRUE(col.empty());
    ASSERT_EQ(0, col.size());

    irs::doc_id_t doc = irs::type_limits<irs::type_t::doc_id_t>::min();
    for (const auto value : values) {
      col.prepare(doc++);
      col.write_vint(value);
    }
    ASSERT_EQ(IRESEARCH_COUNTOF(values), col.size());

    std::tie(order, column_id) = col.flush(*writer, IRESEARCH_COUNTOF(values), less);
    ASSERT_TRUE(col.empty());
    ASSERT_EQ(0, col.size());
    ASSERT_EQ(IRESEARCH_COUNTOF(values), order.size());
    ASSERT_TRUE(irs::type_limits<irs::type_t::field_id_t>::valid(column_id));

    ASSERT_TRUE(writer->commit());
  }

  std::vector<uint32_t> sorted_values(values, values + IRESEARCH_COUNTOF(values));
  std::sort(sorted_values.begin(), sorted_values.end());

  // check order
  {
    uint32_t values_by_order[IRESEARCH_COUNTOF(values)];
    for (size_t i = 0; i < IRESEARCH_COUNTOF(values); ++i) {
      values_by_order[order[i]] = values[i];
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

    auto it = column->iterator();
    auto& payload = it->attributes().get<irs::payload>();
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

#endif // IRESEARCH_DLL
