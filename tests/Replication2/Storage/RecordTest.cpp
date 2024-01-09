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

#define RECORD_UNIT_TEST
#include "Replication2/Storage/WAL/Record.h"

namespace arangodb::replication2::storage::wal::test {

void compareHeaders(Record::Header const& expected,
                    Record::Header const& actual) {
  EXPECT_EQ(expected.index, actual.index);
  EXPECT_EQ(expected.term, actual.term);
  EXPECT_EQ(expected.reserved, actual.reserved);
  EXPECT_EQ(expected.type, actual.type);
  EXPECT_EQ(expected.payloadSize, actual.payloadSize);
}

TEST(WalRecordTest, paddedPayloadSize) {
  EXPECT_EQ(0, Record::paddedPayloadSize(0));
  EXPECT_EQ(Record::alignment, Record::paddedPayloadSize(1));
  EXPECT_EQ(Record::alignment, Record::paddedPayloadSize(2));
  EXPECT_EQ(Record::alignment, Record::paddedPayloadSize(3));
  EXPECT_EQ(Record::alignment, Record::paddedPayloadSize(4));
  EXPECT_EQ(Record::alignment, Record::paddedPayloadSize(5));
  EXPECT_EQ(Record::alignment, Record::paddedPayloadSize(6));
  EXPECT_EQ(Record::alignment, Record::paddedPayloadSize(7));
  EXPECT_EQ(Record::alignment, Record::paddedPayloadSize(8));
  EXPECT_EQ(2 * Record::alignment, Record::paddedPayloadSize(9));
  EXPECT_EQ(2 * Record::alignment, Record::paddedPayloadSize(15));
  EXPECT_EQ(2 * Record::alignment, Record::paddedPayloadSize(16));
  EXPECT_EQ(3 * Record::alignment, Record::paddedPayloadSize(17));
}

TEST(WalRecordTest, index_compress_decompress_roundtrip) {
  Record::Header expected{};
  expected.index = std::numeric_limits<decltype(expected.index)>::max();

  auto actual = Record::Header{Record::CompressedHeader{expected}};
  compareHeaders(expected, actual);
}

TEST(WalRecordTest, term_compress_decompress_roundtrip) {
  Record::Header expected{};
  expected.term = (std::uint64_t{1} << Record::CompressedHeader::termBits) - 1;

  auto actual = Record::Header{Record::CompressedHeader{expected}};
  compareHeaders(expected, actual);
}

TEST(WalRecordTest, reserved_compress_decompress_roundtrip) {
  Record::Header expected{};
  expected.reserved =
      (std::uint64_t{1} << Record::CompressedHeader::reservedBits) - 1;

  auto actual = Record::Header{Record::CompressedHeader{expected}};
  compareHeaders(expected, actual);
}

TEST(WalRecordTest, type_compress_decompress_roundtrip) {
  Record::Header expected{};
  expected.type =
      RecordType((std::uint64_t{1} << Record::CompressedHeader::typeBits) - 1);

  auto actual = Record::Header{Record::CompressedHeader{expected}};
  compareHeaders(expected, actual);
}

TEST(WalRecordTest, size_compress_decompress_roundtrip) {
  Record::Header expected{};
  expected.payloadSize =
      std::numeric_limits<decltype(expected.payloadSize)>::max();

  auto actual = Record::Header{Record::CompressedHeader{expected}};
  compareHeaders(expected, actual);
}

TEST(WalRecordTest, full_compress_decompress_roundtrip) {
  Record::Header expected{};
  expected.index =
      (std::uint64_t{1} << (Record::CompressedHeader::indexBits - 1)) + 1;
  expected.term =
      (std::uint64_t{1} << (Record::CompressedHeader::termBits - 1)) + 1;
  expected.type =
      RecordType((1ul << (Record::CompressedHeader::typeBits - 1)) + 1);
  expected.payloadSize =
      (std::uint64_t{1} << (Record::CompressedHeader::payloadSizeBits - 1)) + 1;

  auto actual = Record::Header{Record::CompressedHeader{expected}};
  compareHeaders(expected, actual);
}

}  // namespace arangodb::replication2::storage::wal::test
