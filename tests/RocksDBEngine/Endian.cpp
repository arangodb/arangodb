////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Endian.h"
#include "RocksDBEngine/RocksDBFormat.h"

#include <date/date.h>
#include "gtest/gtest.h"

using namespace arangodb;
using namespace arangodb::basics;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

/// @brief test RocksDBFormat functions class
class RocksDBFormatTest : public ::testing::Test {
 protected:
  std::string out;
};

TEST_F(RocksDBFormatTest, little_endian) {
  rocksutils::uintToPersistentLittleEndian<uint64_t>(out, 1);
  EXPECT_EQ(out.size(), 8);
  EXPECT_EQ(rocksutils::uintFromPersistentLittleEndian<uint64_t>(out.data()), 1);
  out.clear();

  rocksutils::uintToPersistentLittleEndian<uint64_t>(out, 1337);
  EXPECT_EQ(out.size(), 8);
  EXPECT_EQ(rocksutils::uintFromPersistentLittleEndian<uint64_t>(out.data()), 1337);
  out.clear();

  rocksutils::uintToPersistentLittleEndian<uint64_t>(out, 1212321);
  EXPECT_EQ(out.size(), 8);
  EXPECT_EQ(rocksutils::uintFromPersistentLittleEndian<uint64_t>(out.data()), 1212321);
  out.clear();

  rocksutils::uintToPersistentLittleEndian<uint32_t>(out, 88888);
  EXPECT_EQ(out.size(), 4);
  EXPECT_EQ(rocksutils::uintFromPersistentLittleEndian<uint32_t>(out.data()), 88888);
  out.clear();
}

TEST_F(RocksDBFormatTest, big_endian) {
  rocksutils::uintToPersistentBigEndian<uint64_t>(out, 1);
  EXPECT_EQ(out.size(), 8);
  EXPECT_EQ(rocksutils::uintFromPersistentBigEndian<uint64_t>(out.data()), 1);
  out.clear();

  rocksutils::uintToPersistentBigEndian<uint64_t>(out, 1337);
  EXPECT_EQ(out.size(), 8);
  EXPECT_EQ(rocksutils::uintFromPersistentBigEndian<uint64_t>(out.data()), 1337);
  out.clear();

  rocksutils::uintToPersistentBigEndian<uint64_t>(out, 1212321);
  EXPECT_EQ(out.size(), 8);
  EXPECT_EQ(rocksutils::uintFromPersistentBigEndian<uint64_t>(out.data()), 1212321);
  out.clear();

  rocksutils::uintToPersistentBigEndian<uint32_t>(out, 88888);
  EXPECT_EQ(out.size(), 4);
  EXPECT_EQ(rocksutils::uintFromPersistentBigEndian<uint32_t>(out.data()), 88888);
  out.clear();
}

TEST_F(RocksDBFormatTest, specialized_little_endian) {
  rocksutils::setRocksDBKeyFormatEndianess(RocksDBEndianness::Little);

  rocksutils::uint32ToPersistent(out, 1);
  EXPECT_EQ(out.size(), 4);
  EXPECT_EQ(rocksutils::uint32FromPersistent(out.data()), 1);
  out.clear();

  rocksutils::uint16ToPersistent(out, 1337);
  EXPECT_EQ(out.size(), 2);
  EXPECT_EQ(rocksutils::uint16FromPersistent(out.data()), 1337);
  out.clear();

  rocksutils::uint64ToPersistent(out, 1212321);
  EXPECT_EQ(out.size(), 8);
  EXPECT_EQ(rocksutils::uint64FromPersistent(out.data()), 1212321);
  out.clear();
}

TEST_F(RocksDBFormatTest, specialized_big_endian) {
  rocksutils::setRocksDBKeyFormatEndianess(RocksDBEndianness::Big);

  rocksutils::uint32ToPersistent(out, 1);
  EXPECT_EQ(out.size(), 4);
  EXPECT_EQ(rocksutils::uint32FromPersistent(out.data()), 1);
  out.clear();

  rocksutils::uint16ToPersistent(out, 1337);
  EXPECT_EQ(out.size(), 2);
  EXPECT_EQ(rocksutils::uint16FromPersistent(out.data()), 1337);
  out.clear();

  rocksutils::uint64ToPersistent(out, 1212321);
  EXPECT_EQ(out.size(), 8);
  EXPECT_EQ(rocksutils::uint64FromPersistent(out.data()), 1212321);
  out.clear();
}
