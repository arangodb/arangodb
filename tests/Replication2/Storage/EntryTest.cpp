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

#include "Replication2/Storage/WAL/Entry.h"
#include "Replication2/Storage/WAL/EntryType.h"

namespace arangodb::replication2::storage::wal::test {

TEST(WalEntryTest, index_compress_decompress_roundtrip) {
  Entry::Header header{};
  header.index = (1ul << Entry::CompressedHeader::indexBits) - 1;

  EXPECT_EQ(header.index,
            Entry::Header::fromCompressed(header.compress()).index);
  EXPECT_EQ(0, header.term);
  EXPECT_EQ(EntryType(0), header.type);
  EXPECT_EQ(0, header.size);
}

TEST(WalEntryTest, term_compress_decompress_roundtrip) {
  Entry::Header header{};
  header.term = (1ul << Entry::CompressedHeader::termBits) - 1;

  EXPECT_EQ(header.term, Entry::Header::fromCompressed(header.compress()).term);
  EXPECT_EQ(0, header.index);
  EXPECT_EQ(EntryType(0), header.type);
  EXPECT_EQ(0, header.size);
}

TEST(WalEntryTest, type_compress_decompress_roundtrip) {
  Entry::Header header{};
  header.type = EntryType((1ul << Entry::CompressedHeader::typeBits) - 1);

  EXPECT_EQ(header.type, Entry::Header::fromCompressed(header.compress()).type);
  EXPECT_EQ(0, header.index);
  EXPECT_EQ(0, header.term);
  EXPECT_EQ(0, header.size);
}

TEST(WalEntryTest, size_compress_decompress_roundtrip) {
  Entry::Header header{};
  header.size = (1ul << Entry::CompressedHeader::sizeBits) - 1;

  EXPECT_EQ(header.size, Entry::Header::fromCompressed(header.compress()).size);
  EXPECT_EQ(0, header.index);
  EXPECT_EQ(0, header.term);
  EXPECT_EQ(EntryType(0), header.type);
}

TEST(WalEntryTest, full_compress_decompress_roundtrip) {
  Entry::Header header{};
  header.index = (1ul << (Entry::CompressedHeader::indexBits - 1)) + 1;
  header.term = (1ul << (Entry::CompressedHeader::termBits - 1)) + 1;
  header.type = EntryType((1ul << (Entry::CompressedHeader::typeBits - 1)) + 1);
  header.size = (1ul << (Entry::CompressedHeader::sizeBits - 1)) + 1;

  auto res = Entry::Header::fromCompressed(header.compress());
  EXPECT_EQ(header.index, res.index);
  EXPECT_EQ(header.term, res.term);
  EXPECT_EQ(header.type, res.type);
  EXPECT_EQ(header.size, res.size);
}

}  // namespace arangodb::replication2::storage::wal::test