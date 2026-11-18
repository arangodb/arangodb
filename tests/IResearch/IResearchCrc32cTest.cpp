////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
////////////////////////////////////////////////////////////////////////////////

#include <boost/crc.hpp>

#include <random>
#include <string>

#include "gtest/gtest.h"
#include "utils/crc.hpp"

namespace {

// irs::crc32c is persisted in iresearch files, so it must keep computing
// CRC32C without the usual initial/final xor, whatever abseil provides.
using ExpectedCrc32c = boost::crc_optimal<32, 0x1EDC6F41, 0, 0, true, true>;

std::string makeBuffer(size_t size, bool zeros) {
  std::mt19937 rng(static_cast<std::mt19937::result_type>(size));
  std::string buffer(size, '\0');
  if (!zeros) {
    for (auto& c : buffer) {
      c = static_cast<char>(rng());
    }
  }
  return buffer;
}

}  // namespace

TEST(IResearchCrc32cTest, empty_input_keeps_seed) {
  EXPECT_EQ(0U, irs::crc32c{}.checksum());
  EXPECT_EQ(0xDEADBEEFU, irs::crc32c{0xDEADBEEFU}.checksum());
}

// abseil uses different code paths for chunks up to 64 bytes and above
TEST(IResearchCrc32cTest, matches_crc_without_xor_for_all_chunk_sizes) {
  for (bool zeros : {false, true}) {
    for (size_t chunkSize = 1; chunkSize <= 200; ++chunkSize) {
      auto const buffer = makeBuffer(3 * chunkSize + 5, zeros);

      irs::crc32c actual;
      ExpectedCrc32c expected;
      for (size_t offset = 0; offset < buffer.size(); offset += chunkSize) {
        auto const size = std::min(chunkSize, buffer.size() - offset);
        actual.process_bytes(buffer.data() + offset, size);
        expected.process_bytes(buffer.data() + offset, size);
      }

      EXPECT_EQ(expected.checksum(), actual.checksum())
          << "chunkSize=" << chunkSize << " zeros=" << zeros;
    }
  }
}

TEST(IResearchCrc32cTest, seeding_with_checksum_resumes_computation) {
  auto const buffer = makeBuffer(1000, false);
  for (size_t split : {size_t{1}, size_t{64}, size_t{65}, size_t{500}}) {
    irs::crc32c whole;
    whole.process_bytes(buffer.data(), buffer.size());

    irs::crc32c head;
    head.process_bytes(buffer.data(), split);
    irs::crc32c resumed{head.checksum()};
    resumed.process_bytes(buffer.data() + split, buffer.size() - split);

    EXPECT_EQ(whole.checksum(), resumed.checksum()) << "split=" << split;
  }
}
