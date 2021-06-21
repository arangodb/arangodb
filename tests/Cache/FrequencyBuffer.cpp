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

#include "ApplicationFeatures/SharedPRNGFeature.h"
#include "Cache/FrequencyBuffer.h"

#include "Mocks/Servers.h"

using namespace arangodb;
using namespace arangodb::cache;
using namespace arangodb::tests::mocks;

TEST(CacheFrequencyBufferTest, test_buffer_with_uint8_entries) {
  std::uint8_t zero = 0;
  std::uint8_t one = 1;
  std::uint8_t two = 2;

  // check that default construction is as expected
  ASSERT_EQ(std::uint8_t(), zero);

  MockMetricsServer server;
  SharedPRNGFeature& sharedPRNG = server.getFeature<SharedPRNGFeature>();
  FrequencyBuffer<std::uint8_t> buffer(sharedPRNG, 1024);
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
  // the lower bound compare values here depend a lot on the quality of the
  // PRNG values used by the FrequencyBuffer.
  // Values are recorded in a random slot in the FrequencyBuffer, and the
  // slot is determined by a PRNG value. So when we inserted 256 values into
  // a FrequencyBuffer with 1024 slots, we may see only 1 slot used (worst
  // case), or 256 different slots (perfect distribution). In reality, we
  // will get something in between.
  // Apparently not only does this depend on the quality of the PRNG values,
  // but also on the _state_ that the PRNG has when we get here. The state
  // may have been influenced by any tests that ran before, so it is not
  // really possible to predict the exact outcomes here.
  ASSERT_TRUE(static_cast<std::uint64_t>(150) <= frequencies[0].second);
  ASSERT_TRUE(static_cast<std::uint64_t>(256) >= frequencies[0].second);
  ASSERT_EQ(two, frequencies[1].first);

  // same here. we inserted a value 512 times, and these values can be all
  // in one slot or in 512 different ones.
  ASSERT_TRUE(static_cast<std::uint64_t>(275) <= frequencies[1].second);
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
