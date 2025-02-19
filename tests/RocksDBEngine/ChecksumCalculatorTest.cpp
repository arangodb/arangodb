////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
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
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "RocksDBEngine/RocksDBChecksumEnv.h"

using namespace arangodb;

namespace {
constexpr std::string_view contents =
    "Ein männlein steht im Walde, ganz still und stumm";
}

class RocksDBChecksumCalculatorTest : public ::testing::Test {};

TEST_F(RocksDBChecksumCalculatorTest, test_empty) {
  checksum::ChecksumCalculator calc;

  ASSERT_EQ("", calc.getChecksum());
}

TEST_F(RocksDBChecksumCalculatorTest, test_simple_string) {
  checksum::ChecksumCalculator calc;
  calc.updateIncrementalChecksum(::contents.data(), ::contents.size());
  ASSERT_EQ("", calc.getChecksum());
  calc.computeFinalChecksum();
  ASSERT_EQ("18f6b39dc049d331f60fabb4d32223fe0dea0644defa51d4b53cf2d4bea63432",
            calc.getChecksum());
}

TEST_F(RocksDBChecksumCalculatorTest, test_simple_string_incremental) {
  checksum::ChecksumCalculator calc;

  // update one byte at a time
  for (size_t i = 0; i < ::contents.size(); ++i) {
    calc.updateIncrementalChecksum(::contents.data() + i, 1);
    ASSERT_EQ("", calc.getChecksum());
  }
  calc.computeFinalChecksum();
  ASSERT_EQ("18f6b39dc049d331f60fabb4d32223fe0dea0644defa51d4b53cf2d4bea63432",
            calc.getChecksum());
}

TEST_F(RocksDBChecksumCalculatorTest, test_long_string) {
  checksum::ChecksumCalculator calc;

  for (size_t i = 0; i < 1024; ++i) {
    calc.updateIncrementalChecksum(::contents.data(), ::contents.size());
    ASSERT_EQ("", calc.getChecksum());
  }
  calc.computeFinalChecksum();
  ASSERT_EQ("e12c3541e23c161d70367ac4a77b604d44ebf6d231356b2b2c8284d61dede2b2",
            calc.getChecksum());
}
