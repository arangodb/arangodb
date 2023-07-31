////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Manuel Pöter
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <limits>

#include "Replication2/Storage/WAL/Record.h"

namespace arangodb::replication2::storage::wal::test {

void compareHeaders(Record::Header const& expected,
                    Record::Header const& actual) {
  EXPECT_EQ(expected.index, actual.index);
  EXPECT_EQ(expected.term, actual.term);
  EXPECT_EQ(expected.tag, actual.tag);
  EXPECT_EQ(expected.type, actual.type);
  EXPECT_EQ(expected.size, actual.size);
}

TEST(WalRecordTest, index_compress_decompress_roundtrip) {
  Record::Header expected{};
  expected.index = std::numeric_limits<decltype(expected.index)>::max();

  auto actual = Record::Header{Record::CompressedHeader{expected}};
  compareHeaders(expected, actual);
}

TEST(WalRecordTest, term_compress_decompress_roundtrip) {
  Record::Header expected{};
  expected.term = (1ul << Record::CompressedHeader::termBits) - 1;

  auto actual = Record::Header{Record::CompressedHeader{expected}};
  compareHeaders(expected, actual);
}

TEST(WalRecordTest, tag_compress_decompress_roundtrip) {
  Record::Header expected{};
  expected.tag = (1ul << Record::CompressedHeader::tagBits) - 1;

  auto actual = Record::Header{Record::CompressedHeader{expected}};
  compareHeaders(expected, actual);
}

TEST(WalRecordTest, type_compress_decompress_roundtrip) {
  Record::Header expected{};
  expected.type = RecordType((1ul << Record::CompressedHeader::typeBits) - 1);

  auto actual = Record::Header{Record::CompressedHeader{expected}};
  compareHeaders(expected, actual);
}

TEST(WalRecordTest, size_compress_decompress_roundtrip) {
  Record::Header expected{};
  expected.size = std::numeric_limits<decltype(expected.size)>::max();

  auto actual = Record::Header{Record::CompressedHeader{expected}};
  compareHeaders(expected, actual);
}

TEST(WalRecordTest, full_compress_decompress_roundtrip) {
  Record::Header expected{};
  expected.index = (1ul << (Record::CompressedHeader::indexBits - 1)) + 1;
  expected.term = (1ul << (Record::CompressedHeader::termBits - 1)) + 1;
  expected.type =
      RecordType((1ul << (Record::CompressedHeader::typeBits - 1)) + 1);
  expected.size = (1ul << (Record::CompressedHeader::sizeBits - 1)) + 1;

  auto actual = Record::Header{Record::CompressedHeader{expected}};
  compareHeaders(expected, actual);
}

}  // namespace arangodb::replication2::storage::wal::test
