////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Dan Larkin-York
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include <cstdint>
#include <memory>

#include "Cache/FrequencyBuffer.h"

using namespace arangodb::cache;

TEST(CacheFrequencyBufferTest, test_buffer_with_uint8_entries) {
  std::uint8_t zero = 0;
  std::uint8_t one = 1;
  std::uint8_t two = 2;

  // check that default construction is as expected
  ASSERT_EQ(std::uint8_t(), zero);

  FrequencyBuffer<std::uint8_t> buffer(1024);
  ASSERT_EQ(buffer.memoryUsage(), sizeof(FrequencyBuffer<std::uint8_t>) + 1024);

  for (std::size_t i = 0; i < 512; i++) {
    buffer.insertRecord(two);
  }
  for (std::size_t i = 0; i < 256; i++) {
    buffer.insertRecord(one);
  }

  auto frequencies = buffer.getFrequencies();
  ASSERT_EQ(static_cast<std::uint64_t>(2), frequencies.size());
  ASSERT_EQ(one, frequencies[0].first);
  ASSERT_TRUE(static_cast<std::uint64_t>(150) <= frequencies[0].second);
  ASSERT_TRUE(static_cast<std::uint64_t>(256) >= frequencies[0].second);
  ASSERT_EQ(two, frequencies[1].first);
  ASSERT_TRUE(static_cast<std::uint64_t>(300) <= frequencies[1].second);
  ASSERT_TRUE(static_cast<std::uint64_t>(512) >= frequencies[1].second);

  for (std::size_t i = 0; i < 8192; i++) {
    buffer.insertRecord(one);
  }

  frequencies = buffer.getFrequencies();
  if (frequencies.size() == 1) {
    ASSERT_EQ(static_cast<std::size_t>(1), frequencies.size());
    ASSERT_EQ(one, frequencies[0].first);
    ASSERT_TRUE(static_cast<std::uint64_t>(800) <= frequencies[0].second);
  } else {
    ASSERT_EQ(static_cast<std::uint64_t>(2), frequencies.size());
    ASSERT_EQ(two, frequencies[0].first);
    ASSERT_TRUE(static_cast<std::uint64_t>(100) >= frequencies[0].second);
    ASSERT_EQ(one, frequencies[1].first);
    ASSERT_TRUE(static_cast<std::uint64_t>(800) <= frequencies[1].second);
  }
}
