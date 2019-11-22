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
#include "utils/fst_string_weight.h"
#include "utils/fst_decl.hpp"
#include "utils/fst.hpp"
#include "utils/fst_matcher.hpp"
#include "utils/fst_utils.hpp"

#include <fst/vector-fst.h>
#include <fst/matcher.h>

#include <fstream>

#include "store/mmap_directory.hpp"
#include "store/memory_directory.hpp"
#include "index/directory_reader.hpp"
#include "index/index_writer.hpp"
#include "utils/numeric_utils.hpp"

NS_LOCAL

// reads input data to build fst
// first - prefix
// second - payload
std::vector<std::pair<irs::bstring, irs::bstring>> read_fst_input(
    const std::string& filename
) {
  auto read_size = [](std::istream& stream) {
    size_t size;
    stream.read(reinterpret_cast<char*>(&size), sizeof(size_t));
    if (irs::numeric_utils::is_big_endian()) {
      size = irs::numeric_utils::numeric_traits<size_t>::hton(size);
    }
    return size;
  };

  std::vector<std::pair<irs::bstring, irs::bstring>> data;

  std::ifstream in;
  in.open(filename, std::ios_base::in | std::ios_base::binary);

  data.resize(read_size(in));

  for (size_t i = 0; i < data.size(); ++i) {
    auto& entry = data[i];
    auto& prefix = entry.first;
    auto& payload = entry.second;

    prefix.resize(read_size(in));
    in.read(reinterpret_cast<char*>(&prefix[0]), prefix.size());

    payload.resize(read_size(in));
    in.read(reinterpret_cast<char*>(&payload[0]), payload.size());
  }

  return data;
}

TEST(fst_builder_test, static_const) {
  ASSERT_EQ(0, irs::fst_byte_builder::stateid_t(irs::fst_byte_builder::final));
}

TEST(fst_builder_test, build_fst) {
  auto expected_data = read_fst_input(test_base::resource("fst"));
  ASSERT_FALSE(expected_data.empty());

  ASSERT_TRUE(
    std::is_sorted(
      expected_data.begin(), expected_data.end(),
      [](const std::pair<irs::bstring, irs::bstring>& lhs,
         const std::pair<irs::bstring, irs::bstring>& rhs) {
        return lhs.first < rhs.first;
    })
  );

  irs::fst_byte_builder::fst_t fst;

  // build fst
  {
    irs::fst_byte_builder builder(fst);
    builder.reset();

    for (auto& data : expected_data) {
      builder.add(data.first, irs::byte_weight(data.second.begin(), data.second.end()));
    }

    builder.finish();
  }

  // check fst
  {
    typedef fst::SortedMatcher<irs::fst_byte_builder::fst_t> sorted_matcher_t;
    typedef fst::explicit_matcher<sorted_matcher_t> matcher_t; // avoid implicit loops

    ASSERT_EQ(fst::kILabelSorted, fst.Properties(fst::kILabelSorted, true));
    ASSERT_TRUE(fst.Final(irs::fst_byte_builder::final).Empty());

    for (auto& data : expected_data) {
      irs::byte_weight actual_weight;

      auto state = fst.Start(); // root node

      matcher_t matcher(fst, fst::MATCH_INPUT);
      for (irs::byte_type c : data.first) {
        matcher.SetState(state);
        ASSERT_TRUE(matcher.Find(c));

        const auto& arc = matcher.Value();
        ASSERT_EQ(c, arc.ilabel);
        actual_weight.PushBack(arc.weight);
        state = arc.nextstate;
      }

      actual_weight = fst::Times(actual_weight, fst.Final(state));

      ASSERT_EQ(irs::bytes_ref(actual_weight), irs::bytes_ref(data.second));
    }
  }
}

NS_END

#endif // IRESEARCH_DLL
