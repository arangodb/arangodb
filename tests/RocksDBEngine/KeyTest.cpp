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
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "gtest/gtest.h"

#include "Basics/Exceptions.h"
#include "Basics/VelocyPackHelper.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBFormat.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBPrefixExtractor.h"
#include "RocksDBEngine/RocksDBTypes.h"

using namespace arangodb;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

/// @brief test RocksDBKey class
class RocksDBKeyTestLittleEndian : public ::testing::Test {
 protected:
  RocksDBKeyTestLittleEndian() {
    rocksutils::setRocksDBKeyFormatEndianess(RocksDBEndianness::Little);
  }
};

/// @brief test database
TEST_F(RocksDBKeyTestLittleEndian, test_database) {
  static_assert(static_cast<char>(RocksDBEntryType::Database) == '0', "");

  RocksDBKey key;
  key.constructDatabase(1);
  auto const& s2 = key.string();

  EXPECT_EQ(s2.size(), sizeof(char) + sizeof(uint64_t));
  EXPECT_EQ(s2, std::string("0\1\0\0\0\0\0\0\0", 9));

  key.constructDatabase(255);
  auto const& s3 = key.string();

  EXPECT_EQ(s3.size(), sizeof(char) + sizeof(uint64_t));
  EXPECT_EQ(s3, std::string("0\xff\0\0\0\0\0\0\0", 9));

  key.constructDatabase(256);
  auto const& s4 = key.string();

  EXPECT_EQ(s4.size(), sizeof(char) + sizeof(uint64_t));
  EXPECT_EQ(s4, std::string("0\0\x01\0\0\0\0\0\0", 9));

  key.constructDatabase(49152);
  auto const& s5 = key.string();

  EXPECT_EQ(s5.size(), sizeof(char) + sizeof(uint64_t));
  EXPECT_EQ(s5, std::string("0\0\xc0\0\0\0\0\0\0", 9));

  key.constructDatabase(12345678901);
  auto const& s6 = key.string();

  EXPECT_EQ(s6.size(), sizeof(char) + sizeof(uint64_t));
  EXPECT_EQ(s6, std::string("0\x35\x1c\xdc\xdf\x02\0\0\0", 9));

  key.constructDatabase(0xf0f1f2f3f4f5f6f7ULL);
  auto const& s7 = key.string();

  EXPECT_EQ(s7.size(), sizeof(char) + sizeof(uint64_t));
  EXPECT_EQ(s7, std::string("0\xf7\xf6\xf5\xf4\xf3\xf2\xf1\xf0", 9));
}

/// @brief test collection
TEST_F(RocksDBKeyTestLittleEndian, test_collection) {
  static_assert(static_cast<char>(RocksDBEntryType::Collection) == '1', "");

  RocksDBKey key;
  key.constructCollection(23, DataSourceId{42});
  auto const& s2 = key.string();

  EXPECT_EQ(s2.size(), sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
  EXPECT_EQ(s2, std::string("1\x17\0\0\0\0\0\0\0\x2a\0\0\0\0\0\0\0", 17));

  key.constructCollection(255, DataSourceId{255});
  auto const& s3 = key.string();

  EXPECT_EQ(s3.size(), sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
  EXPECT_EQ(s3, std::string("1\xff\0\0\0\0\0\0\0\xff\0\0\0\0\0\0\0", 17));

  key.constructCollection(256, DataSourceId{257});
  auto const& s4 = key.string();

  EXPECT_EQ(s4.size(), sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
  EXPECT_EQ(s4, std::string("1\0\x01\0\0\0\0\0\0\x01\x01\0\0\0\0\0\0", 17));

  key.constructCollection(49152, DataSourceId{16384});
  auto const& s5 = key.string();

  EXPECT_EQ(s5.size(), sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
  EXPECT_EQ(s5, std::string("1\0\xc0\0\0\0\0\0\0\0\x40\0\0\0\0\0\0", 17));

  key.constructCollection(12345678901, DataSourceId{987654321});
  auto const& s6 = key.string();

  EXPECT_EQ(s6.size(), sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
  EXPECT_TRUE(s6 ==
              std::string("1\x35\x1c\xdc\xdf\x02\0\0\0\xb1\x68\xde\x3a\0\0\0\0", 17));

  key.constructCollection(0xf0f1f2f3f4f5f6f7ULL, DataSourceId{0xf0f1f2f3f4f5f6f7ULL});
  auto const& s7 = key.string();

  EXPECT_EQ(s7.size(), sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
  EXPECT_TRUE(
      s7 ==
      std::string(
          "1\xf7\xf6\xf5\xf4\xf3\xf2\xf1\xf0\xf7\xf6\xf5\xf4\xf3\xf2\xf1\xf0", 17));
}

/// @brief test document little-endian
TEST_F(RocksDBKeyTestLittleEndian, test_document) {
  RocksDBKey key;
  key.constructDocument(1, LocalDocumentId(0));
  auto const& s1 = key.string();

  EXPECT_EQ(s1.size(), +sizeof(uint64_t) + sizeof(uint64_t));
  EXPECT_EQ(s1, std::string("\x01\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16));

  key.constructDocument(23, LocalDocumentId(42));
  auto const& s2 = key.string();

  EXPECT_EQ(s2.size(), +sizeof(uint64_t) + sizeof(uint64_t));
  EXPECT_EQ(s2, std::string("\x17\0\0\0\0\0\0\0\x2a\0\0\0\0\0\0\0", 16));

  key.constructDocument(255, LocalDocumentId(255));
  auto const& s3 = key.string();

  EXPECT_EQ(s3.size(), sizeof(uint64_t) + sizeof(uint64_t));
  EXPECT_EQ(s3, std::string("\xff\0\0\0\0\0\0\0\xff\0\0\0\0\0\0\0", 16));

  key.constructDocument(256, LocalDocumentId(257));
  auto const& s4 = key.string();

  EXPECT_EQ(s4.size(), sizeof(uint64_t) + sizeof(uint64_t));
  EXPECT_EQ(s4, std::string("\0\x01\0\0\0\0\0\0\x01\x01\0\0\0\0\0\0", 16));

  key.constructDocument(49152, LocalDocumentId(16384));
  auto const& s5 = key.string();

  EXPECT_EQ(s5.size(), sizeof(uint64_t) + sizeof(uint64_t));
  EXPECT_EQ(s5, std::string("\0\xc0\0\0\0\0\0\0\0\x40\0\0\0\0\0\0", 16));

  key.constructDocument(12345678901, LocalDocumentId(987654321));
  auto const& s6 = key.string();

  EXPECT_EQ(s6.size(), sizeof(uint64_t) + sizeof(uint64_t));
  EXPECT_TRUE(s6 ==
              std::string("\x35\x1c\xdc\xdf\x02\0\0\0\xb1\x68\xde\x3a\0\0\0\0", 16));

  key.constructDocument(0xf0f1f2f3f4f5f6f7ULL, LocalDocumentId(0xf0f1f2f3f4f5f6f7ULL));
  auto const& s7 = key.string();

  EXPECT_EQ(s7.size(), sizeof(uint64_t) + sizeof(uint64_t));
  EXPECT_TRUE(
      s7 ==
      std::string(
          "\xf7\xf6\xf5\xf4\xf3\xf2\xf1\xf0\xf7\xf6\xf5\xf4\xf3\xf2\xf1\xf0", 16));
}

/// @brief test primary index
TEST_F(RocksDBKeyTestLittleEndian, test_primary_index) {
  RocksDBKey key;
  key.constructPrimaryIndexValue(1, arangodb::velocypack::StringRef("abc"));
  auto const& s2 = key.string();

  EXPECT_EQ(s2.size(), sizeof(uint64_t) + strlen("abc"));
  EXPECT_EQ(s2, std::string("\1\0\0\0\0\0\0\0abc", 11));

  key.constructPrimaryIndexValue(1, arangodb::velocypack::StringRef(" "));
  auto const& s3 = key.string();

  EXPECT_EQ(s3.size(), sizeof(uint64_t) + strlen(" "));
  EXPECT_EQ(s3, std::string("\1\0\0\0\0\0\0\0 ", 9));

  key.constructPrimaryIndexValue(1, arangodb::velocypack::StringRef(
                                        "this is a key"));
  auto const& s4 = key.string();

  EXPECT_EQ(s4.size(), sizeof(uint64_t) + strlen("this is a key"));
  EXPECT_EQ(s4, std::string("\1\0\0\0\0\0\0\0this is a key", 21));

  // 254 bytes
  char const* longKey =
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  key.constructPrimaryIndexValue(1, arangodb::velocypack::StringRef(longKey));
  auto const& s5 = key.string();

  EXPECT_EQ(s5.size(), sizeof(uint64_t) + strlen(longKey));
  EXPECT_EQ(s5, std::string("\1\0\0\0\0\0\0\0", 8) + longKey);

  key.constructPrimaryIndexValue(123456789, arangodb::velocypack::StringRef(
                                                "this is a key"));
  auto const& s6 = key.string();

  EXPECT_EQ(s6.size(), sizeof(uint64_t) + strlen("this is a key"));
  EXPECT_EQ(s6, std::string("\x15\xcd\x5b\x07\0\0\0\0this is a key", 21));
}

/// @brief test edge index
TEST_F(RocksDBKeyTestLittleEndian, test_edge_index) {
  RocksDBKey key1;
  key1.constructEdgeIndexValue(1, arangodb::velocypack::StringRef("a/1"),
                               LocalDocumentId(33));
  RocksDBKey key2;
  key2.constructEdgeIndexValue(1, arangodb::velocypack::StringRef("b/1"),
                               LocalDocumentId(33));
  auto const& s1 = key1.string();

  EXPECT_TRUE(s1.size() == sizeof(uint64_t) + strlen("a/1") + sizeof(char) +
                               sizeof(uint64_t) + sizeof(char));
  EXPECT_EQ(s1, std::string("\1\0\0\0\0\0\0\0a/1\0!\0\0\0\0\0\0\0\xff", 21));
  EXPECT_TRUE(key2.string().size() == sizeof(uint64_t) + strlen("b/1") + sizeof(char) +
                                          sizeof(uint64_t) + sizeof(char));
  EXPECT_TRUE(key2.string() ==
              std::string("\1\0\0\0\0\0\0\0b/1\0!\0\0\0\0\0\0\0\xff", 21));

  EXPECT_EQ(RocksDBKey::vertexId(key1).compare("a/1"), 0);
  EXPECT_EQ(RocksDBKey::vertexId(key2).compare("b/1"), 0);

  // check the variable length edge prefix
  auto pe = std::make_unique<RocksDBPrefixExtractor>();
  EXPECT_TRUE(pe->InDomain(key1.string()));

  rocksdb::Slice prefix = pe->Transform(key1.string());
  EXPECT_EQ(prefix.size(), sizeof(uint64_t) + strlen("a/1") + sizeof(char));
  EXPECT_EQ(memcmp(s1.data(), prefix.data(), prefix.size()), 0);

  rocksdb::Comparator const* cmp = rocksdb::BytewiseComparator();
  EXPECT_LT(cmp->Compare(key1.string(), key2.string()), 0);
}

/// @brief test RocksDBKey class
class RocksDBKeyTestBigEndian : public ::testing::Test {
 protected:
  RocksDBKeyTestBigEndian() {
    rocksutils::setRocksDBKeyFormatEndianess(RocksDBEndianness::Big);
  }
};

/// @brief test database
TEST_F(RocksDBKeyTestBigEndian, test_database) {
  static_assert(static_cast<char>(RocksDBEntryType::Database) == '0', "");

  RocksDBKey key;
  key.constructDatabase(1);
  auto const& s2 = key.string();

  EXPECT_EQ(s2.size(), sizeof(char) + sizeof(uint64_t));
  EXPECT_EQ(s2, std::string("0\0\0\0\0\0\0\0\x01", 9));

  key.constructDatabase(255);
  auto const& s3 = key.string();

  EXPECT_EQ(s3.size(), sizeof(char) + sizeof(uint64_t));
  EXPECT_EQ(s3, std::string("0\0\0\0\0\0\0\0\xff", 9));

  key.constructDatabase(256);
  auto const& s4 = key.string();

  EXPECT_EQ(s4.size(), sizeof(char) + sizeof(uint64_t));
  EXPECT_EQ(s4, std::string("0\0\0\0\0\0\0\x01\0", 9));

  key.constructDatabase(49152);
  auto const& s5 = key.string();

  EXPECT_EQ(s5.size(), sizeof(char) + sizeof(uint64_t));
  EXPECT_EQ(s5, std::string("0\0\0\0\0\0\0\xc0\0", 9));

  key.constructDatabase(12345678901);
  auto const& s6 = key.string();

  EXPECT_EQ(s6.size(), sizeof(char) + sizeof(uint64_t));
  EXPECT_EQ(s6, std::string("0\0\0\0\x02\xdf\xdc\x1c\x35", 9));

  key.constructDatabase(0xf0f1f2f3f4f5f6f7ULL);
  auto const& s7 = key.string();

  EXPECT_EQ(s7.size(), sizeof(char) + sizeof(uint64_t));
  EXPECT_EQ(s7, std::string("0\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7", 9));
}

/// @brief test collection
TEST_F(RocksDBKeyTestBigEndian, test_collection) {
  static_assert(static_cast<char>(RocksDBEntryType::Collection) == '1', "");

  RocksDBKey key;
  key.constructCollection(23, DataSourceId{42});
  auto const& s2 = key.string();

  EXPECT_EQ(s2.size(), sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
  EXPECT_EQ(s2, std::string("1\0\0\0\0\0\0\0\x17\0\0\0\0\0\0\0\x2a", 17));

  key.constructCollection(255, DataSourceId{255});
  auto const& s3 = key.string();

  EXPECT_EQ(s3.size(), sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
  EXPECT_EQ(s3, std::string("1\0\0\0\0\0\0\0\xff\0\0\0\0\0\0\0\xff", 17));

  key.constructCollection(256, DataSourceId{257});
  auto const& s4 = key.string();

  EXPECT_EQ(s4.size(), sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
  EXPECT_EQ(s4, std::string("1\0\0\0\0\0\0\x01\0\0\0\0\0\0\0\x01\x01", 17));

  key.constructCollection(49152, DataSourceId{16384});
  auto const& s5 = key.string();

  EXPECT_EQ(s5.size(), sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
  EXPECT_EQ(s5, std::string("1\0\0\0\0\0\0\xc0\0\0\0\0\0\0\0\x40\0", 17));

  key.constructCollection(12345678901, DataSourceId{987654321});
  auto const& s6 = key.string();

  EXPECT_EQ(s6.size(), sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
  EXPECT_TRUE(s6 ==
              std::string("1\0\0\0\x02\xdf\xdc\x1c\x35\0\0\0\0\x3a\xde\x68\xb1", 17));

  key.constructCollection(0xf0f1f2f3f4f5f6f7ULL, DataSourceId{0xf0f1f2f3f4f5f6f7ULL});
  auto const& s7 = key.string();

  EXPECT_EQ(s7.size(), sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
  EXPECT_TRUE(
      s7 ==
      std::string(
          "1\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7", 17));
}

/// @brief test document
TEST_F(RocksDBKeyTestBigEndian, test_document) {
  RocksDBKey key;
  key.constructDocument(1, LocalDocumentId(0));
  auto const& s1 = key.string();

  EXPECT_EQ(s1.size(), +sizeof(uint64_t) + sizeof(uint64_t));
  EXPECT_EQ(s1, std::string("\0\0\0\0\0\0\0\x01\0\0\0\0\0\0\0\0\0", 16));

  key.constructDocument(23, LocalDocumentId(42));
  auto const& s2 = key.string();

  EXPECT_EQ(s2.size(), +sizeof(uint64_t) + sizeof(uint64_t));
  EXPECT_EQ(s2, std::string("\0\0\0\0\0\0\0\x17\0\0\0\0\0\0\0\x2a", 16));

  key.constructDocument(255, LocalDocumentId(255));
  auto const& s3 = key.string();

  EXPECT_EQ(s3.size(), sizeof(uint64_t) + sizeof(uint64_t));
  EXPECT_EQ(s3, std::string("\0\0\0\0\0\0\0\xff\0\0\0\0\0\0\0\xff", 16));

  key.constructDocument(256, LocalDocumentId(257));
  auto const& s4 = key.string();

  EXPECT_EQ(s4.size(), sizeof(uint64_t) + sizeof(uint64_t));
  EXPECT_EQ(s4, std::string("\0\0\0\0\0\0\x01\0\0\0\0\0\0\0\x01\x01", 16));

  key.constructDocument(49152, LocalDocumentId(16384));
  auto const& s5 = key.string();

  EXPECT_EQ(s5.size(), sizeof(uint64_t) + sizeof(uint64_t));
  EXPECT_EQ(s5, std::string("\0\0\0\0\0\0\xc0\0\0\0\0\0\0\0\x40\0", 16));

  key.constructDocument(12345678901, LocalDocumentId(987654321));
  auto const& s6 = key.string();

  EXPECT_EQ(s6.size(), sizeof(uint64_t) + sizeof(uint64_t));
  EXPECT_TRUE(s6 ==
              std::string("\0\0\0\x02\xdf\xdc\x1c\x35\0\0\0\0\x3a\xde\x68\xb1", 16));

  key.constructDocument(0xf0f1f2f3f4f5f6f7ULL, LocalDocumentId(0xf0f1f2f3f4f5f6f7ULL));
  auto const& s7 = key.string();

  EXPECT_EQ(s7.size(), sizeof(uint64_t) + sizeof(uint64_t));
  EXPECT_TRUE(
      s7 ==
      std::string(
          "\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7\xf0\xf1\xf2\xf3\xf4\xf5\xf6\xf7", 16));
}

/// @brief test primary index
TEST_F(RocksDBKeyTestBigEndian, test_primary_index) {
  RocksDBKey key;
  key.constructPrimaryIndexValue(1, arangodb::velocypack::StringRef("abc"));
  auto const& s2 = key.string();

  EXPECT_EQ(s2.size(), sizeof(uint64_t) + strlen("abc"));
  EXPECT_EQ(s2, std::string("\0\0\0\0\0\0\0\1abc", 11));

  key.constructPrimaryIndexValue(1, arangodb::velocypack::StringRef(" "));
  auto const& s3 = key.string();

  EXPECT_EQ(s3.size(), sizeof(uint64_t) + strlen(" "));
  EXPECT_EQ(s3, std::string("\0\0\0\0\0\0\0\1 ", 9));

  key.constructPrimaryIndexValue(1, arangodb::velocypack::StringRef(
                                        "this is a key"));
  auto const& s4 = key.string();

  EXPECT_EQ(s4.size(), sizeof(uint64_t) + strlen("this is a key"));
  EXPECT_EQ(s4, std::string("\0\0\0\0\0\0\0\1this is a key", 21));

  // 254 bytes
  char const* longKey =
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
      "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
  key.constructPrimaryIndexValue(1, arangodb::velocypack::StringRef(longKey));
  auto const& s5 = key.string();

  EXPECT_EQ(s5.size(), sizeof(uint64_t) + strlen(longKey));
  EXPECT_EQ(s5, std::string("\0\0\0\0\0\0\0\1", 8) + longKey);

  key.constructPrimaryIndexValue(123456789, arangodb::velocypack::StringRef(
                                                "this is a key"));
  auto const& s6 = key.string();

  EXPECT_EQ(s6.size(), sizeof(uint64_t) + strlen("this is a key"));
  EXPECT_EQ(s6, std::string("\0\0\0\0\x07\x5b\xcd\x15this is a key", 21));
}

/// @brief test edge index
TEST_F(RocksDBKeyTestBigEndian, test_edge_index) {
  RocksDBKey key1;
  key1.constructEdgeIndexValue(1, arangodb::velocypack::StringRef("a/1"),
                               LocalDocumentId(33));
  RocksDBKey key2;
  key2.constructEdgeIndexValue(1, arangodb::velocypack::StringRef("b/1"),
                               LocalDocumentId(33));
  auto const& s1 = key1.string();

  EXPECT_TRUE(s1.size() == sizeof(uint64_t) + strlen("a/1") + sizeof(char) +
                               sizeof(uint64_t) + sizeof(char));
  EXPECT_EQ(s1, std::string("\0\0\0\0\0\0\0\1a/1\0\0\0\0\0\0\0\0!\xff", 21));
  EXPECT_TRUE(key2.string().size() == sizeof(uint64_t) + strlen("b/1") + sizeof(char) +
                                          sizeof(uint64_t) + sizeof(char));
  EXPECT_TRUE(key2.string() ==
              std::string("\0\0\0\0\0\0\0\1b/1\0\0\0\0\0\0\0\0!\xff", 21));

  EXPECT_EQ(RocksDBKey::vertexId(key1).compare("a/1"), 0);
  EXPECT_EQ(RocksDBKey::vertexId(key2).compare("b/1"), 0);

  // check the variable length edge prefix
  auto pe = std::make_unique<RocksDBPrefixExtractor>();
  EXPECT_TRUE(pe->InDomain(key1.string()));

  rocksdb::Slice prefix = pe->Transform(key1.string());
  EXPECT_EQ(prefix.size(), sizeof(uint64_t) + strlen("a/1") + sizeof(char));
  EXPECT_EQ(memcmp(s1.data(), prefix.data(), prefix.size()), 0);

  rocksdb::Comparator const* cmp = rocksdb::BytewiseComparator();
  EXPECT_LT(cmp->Compare(key1.string(), key2.string()), 0);
}

/// @brief test RocksDBKeyBounds class
class RocksDBKeyBoundsTestLittleEndian : public ::testing::Test {
 protected:
  RocksDBKeyBoundsTestLittleEndian() {
    rocksutils::setRocksDBKeyFormatEndianess(RocksDBEndianness::Little);
  }
};

/// @brief test edge index with dynamic prefix extractor
TEST_F(RocksDBKeyBoundsTestLittleEndian, test_edge_index) {
  RocksDBKey key1;
  key1.constructEdgeIndexValue(1, arangodb::velocypack::StringRef("a/1"),
                               LocalDocumentId(33));
  // check the variable length edge prefix
  auto pe = std::make_unique<RocksDBPrefixExtractor>();
  ASSERT_TRUE(pe->InDomain(key1.string()));

  // check the correct key bounds comparisons
  RocksDBKeyBounds bounds = RocksDBKeyBounds::EdgeIndex(1);
  ASSERT_FALSE(pe->InDomain(bounds.start()));
  ASSERT_FALSE(pe->InDomain(bounds.end()));
  rocksdb::Slice prefixBegin = pe->Transform(bounds.start());
  rocksdb::Slice prefixEnd = pe->Transform(bounds.end());
  ASSERT_FALSE(pe->InDomain(prefixBegin));
  ASSERT_FALSE(pe->InDomain(prefixEnd));
  ASSERT_EQ(memcmp(bounds.start().data(), prefixBegin.data(), prefixBegin.size()), 0);
  ASSERT_EQ(memcmp(bounds.end().data(), prefixEnd.data(), prefixEnd.size()), 0);

  // check our assumptions about bound construction
  rocksdb::Comparator const* cmp = rocksdb::BytewiseComparator();
  EXPECT_LT(cmp->Compare(prefixBegin, prefixEnd), 0);
  EXPECT_LT(cmp->Compare(prefixBegin, key1.string()), 0);
  EXPECT_GT(cmp->Compare(prefixEnd, key1.string()), 0);

  RocksDBKey key2;
  key2.constructEdgeIndexValue(1, arangodb::velocypack::StringRef("c/1000"),
                               LocalDocumentId(33));
  EXPECT_LT(cmp->Compare(prefixBegin, key2.string()), 0);
  EXPECT_GT(cmp->Compare(prefixEnd, key2.string()), 0);

  // test higher prefix
  RocksDBKey key3;
  key3.constructEdgeIndexValue(1, arangodb::velocypack::StringRef("c/1000"),
                               LocalDocumentId(33));
  EXPECT_LT(cmp->Compare(prefixBegin, key3.string()), 0);
  EXPECT_GT(cmp->Compare(prefixEnd, key3.string()), 0);
}

/// @brief test hash index with prefix over indexed slice
TEST_F(RocksDBKeyBoundsTestLittleEndian, test_hash_index) {
  VPackBuilder lower;
  lower(VPackValue(VPackValueType::Array))(VPackValue("a"))();
  VPackBuilder higher;
  higher(VPackValue(VPackValueType::Array))(VPackValue("b"))();

  RocksDBKey key1, key2, key3;
  key1.constructVPackIndexValue(1, lower.slice(), LocalDocumentId(33));
  key2.constructVPackIndexValue(1, higher.slice(), LocalDocumentId(33));
  key3.constructVPackIndexValue(2, lower.slice(), LocalDocumentId(16));

  // check the variable length edge prefix
  std::unique_ptr<rocksdb::SliceTransform const> pe(
      rocksdb::NewFixedPrefixTransform(RocksDBKey::objectIdSize()));

  EXPECT_TRUE(pe->InDomain(key1.string()));

  // check the correct key bounds comparisons
  RocksDBKeyBounds bounds = RocksDBKeyBounds::VPackIndex(1, false);
  EXPECT_TRUE(pe->InDomain(bounds.start()));
  EXPECT_TRUE(pe->InDomain(bounds.end()));
  rocksdb::Slice prefixBegin = pe->Transform(bounds.start());
  rocksdb::Slice prefixEnd = pe->Transform(bounds.end());
  EXPECT_TRUE(pe->InDomain(prefixBegin));
  EXPECT_TRUE(pe->InDomain(prefixEnd));
  EXPECT_EQ(memcmp(bounds.start().data(), prefixBegin.data(), prefixBegin.size()), 0);
  EXPECT_EQ(memcmp(bounds.end().data(), prefixEnd.data(), prefixEnd.size()), 0);
  EXPECT_EQ(prefixBegin.data()[prefixBegin.size() - 1], '\0');
  EXPECT_EQ(prefixEnd.data()[prefixEnd.size() - 1], '\0');

  // prefix is just object id
  auto cmp = std::make_unique<RocksDBVPackComparator>();
  EXPECT_LT(cmp->Compare(prefixBegin, prefixEnd), 0);
  EXPECT_LT(cmp->Compare(prefixBegin, key1.string()), 0);
  EXPECT_GT(cmp->Compare(prefixEnd, key1.string()), 0);

  EXPECT_LT(cmp->Compare(key1.string(), key2.string()), 0);
  EXPECT_LT(cmp->Compare(key2.string(), key3.string()), 0);
  EXPECT_LT(cmp->Compare(key1.string(), key3.string()), 0);

  EXPECT_LT(cmp->Compare(prefixEnd, key3.string()), 0);

  // check again with reverse iteration bounds
  bounds = RocksDBKeyBounds::VPackIndex(1, true);
  EXPECT_TRUE(pe->InDomain(bounds.start()));
  EXPECT_TRUE(pe->InDomain(bounds.end()));
  prefixBegin = pe->Transform(bounds.start());
  prefixEnd = pe->Transform(bounds.end());
  EXPECT_TRUE(pe->InDomain(prefixBegin));
  EXPECT_TRUE(pe->InDomain(prefixEnd));
  EXPECT_EQ(memcmp(bounds.start().data(), prefixBegin.data(), prefixBegin.size()), 0);
  EXPECT_EQ(memcmp(bounds.end().data(), prefixEnd.data(), prefixEnd.size()), 0);
  EXPECT_EQ(prefixBegin.data()[prefixBegin.size() - 1], '\0');
  EXPECT_EQ(prefixEnd.data()[prefixEnd.size() - 1], '\0');

  EXPECT_EQ(cmp->Compare(prefixBegin, prefixEnd), 0);
  EXPECT_LT(cmp->Compare(prefixBegin, key1.string()), 0);
  EXPECT_LT(cmp->Compare(prefixEnd, key1.string()), 0);

  EXPECT_LT(cmp->Compare(key1.string(), key2.string()), 0);
  EXPECT_LT(cmp->Compare(key2.string(), key3.string()), 0);
  EXPECT_LT(cmp->Compare(key1.string(), key3.string()), 0);

  EXPECT_LT(cmp->Compare(prefixEnd, key3.string()), 0);

  VPackBuilder a;
  a(VPackValue(VPackValueType::Array))(VPackValue(1))();
  VPackBuilder b;
  b(VPackValue(VPackValueType::Array))(VPackValue(3))();
  VPackBuilder c;
  c(VPackValue(VPackValueType::Array))(VPackValue(5))();

  RocksDBKey key4, key5, key6, key7;
  key4.constructVPackIndexValue(1, a.slice(), LocalDocumentId(18));
  key5.constructVPackIndexValue(1, b.slice(), LocalDocumentId(60));
  key6.constructVPackIndexValue(1, b.slice(), LocalDocumentId(90));
  key7.constructVPackIndexValue(1, c.slice(), LocalDocumentId(12));

  bounds = RocksDBKeyBounds::VPackIndex(1, a.slice(), c.slice());
  EXPECT_LT(cmp->Compare(bounds.start(), key4.string()), 0);
  EXPECT_LT(cmp->Compare(key4.string(), bounds.end()), 0);
  EXPECT_LT(cmp->Compare(bounds.start(), key5.string()), 0);
  EXPECT_LT(cmp->Compare(key5.string(), bounds.end()), 0);
  EXPECT_LT(cmp->Compare(bounds.start(), key6.string()), 0);
  EXPECT_LT(cmp->Compare(key6.string(), bounds.end()), 0);
  EXPECT_LT(cmp->Compare(bounds.start(), key7.string()), 0);
  EXPECT_LT(cmp->Compare(key7.string(), bounds.end()), 0);

  EXPECT_LT(cmp->Compare(key4.string(), key5.string()), 0);
  EXPECT_LT(cmp->Compare(key5.string(), key6.string()), 0);
  EXPECT_LT(cmp->Compare(key4.string(), key6.string()), 0);
  EXPECT_LT(cmp->Compare(key6.string(), key7.string()), 0);
  EXPECT_LT(cmp->Compare(key4.string(), key7.string()), 0);
}

/// @brief test RocksDBKeyBounds class
class RocksDBKeyBoundsTestBigEndian : public ::testing::Test {
 protected:
  RocksDBKeyBoundsTestBigEndian() {
    rocksutils::setRocksDBKeyFormatEndianess(RocksDBEndianness::Big);
  }
};

/// @brief test edge index with dynamic prefix extractor
TEST_F(RocksDBKeyBoundsTestBigEndian, test_edge_index) {
  RocksDBKey key1;
  key1.constructEdgeIndexValue(1, arangodb::velocypack::StringRef("a/1"),
                               LocalDocumentId(33));
  // check the variable length edge prefix
  auto pe = std::make_unique<RocksDBPrefixExtractor>();
  ASSERT_TRUE(pe->InDomain(key1.string()));

  // check the correct key bounds comparisons
  RocksDBKeyBounds bounds = RocksDBKeyBounds::EdgeIndex(1);
  ASSERT_FALSE(pe->InDomain(bounds.start()));
  ASSERT_FALSE(pe->InDomain(bounds.end()));
  rocksdb::Slice prefixBegin = pe->Transform(bounds.start());
  rocksdb::Slice prefixEnd = pe->Transform(bounds.end());
  ASSERT_FALSE(pe->InDomain(prefixBegin));
  ASSERT_FALSE(pe->InDomain(prefixEnd));
  ASSERT_EQ(memcmp(bounds.start().data(), prefixBegin.data(), prefixBegin.size()), 0);
  ASSERT_EQ(memcmp(bounds.end().data(), prefixEnd.data(), prefixEnd.size()), 0);

  // check our assumptions about bound construction
  rocksdb::Comparator const* cmp = rocksdb::BytewiseComparator();
  EXPECT_LT(cmp->Compare(prefixBegin, prefixEnd), 0);
  EXPECT_LT(cmp->Compare(prefixBegin, key1.string()), 0);
  EXPECT_GT(cmp->Compare(prefixEnd, key1.string()), 0);

  RocksDBKey key2;
  key2.constructEdgeIndexValue(1, arangodb::velocypack::StringRef("c/1000"),
                               LocalDocumentId(33));
  EXPECT_LT(cmp->Compare(prefixBegin, key2.string()), 0);
  EXPECT_GT(cmp->Compare(prefixEnd, key2.string()), 0);

  // test higher prefix
  RocksDBKey key3;
  key3.constructEdgeIndexValue(1, arangodb::velocypack::StringRef("c/1000"),
                               LocalDocumentId(33));
  EXPECT_LT(cmp->Compare(prefixBegin, key3.string()), 0);
  EXPECT_GT(cmp->Compare(prefixEnd, key3.string()), 0);
}

/// @brief test hash index with prefix over indexed slice
TEST_F(RocksDBKeyBoundsTestBigEndian, test_hash_index) {
  VPackBuilder lower;
  lower(VPackValue(VPackValueType::Array))(VPackValue("a"))();
  VPackBuilder higher;
  higher(VPackValue(VPackValueType::Array))(VPackValue("b"))();

  RocksDBKey key1, key2, key3;
  key1.constructVPackIndexValue(1, lower.slice(), LocalDocumentId(33));
  key2.constructVPackIndexValue(1, higher.slice(), LocalDocumentId(33));
  key3.constructVPackIndexValue(2, lower.slice(), LocalDocumentId(16));

  // check the variable length edge prefix
  std::unique_ptr<rocksdb::SliceTransform const> pe(
      rocksdb::NewFixedPrefixTransform(RocksDBKey::objectIdSize()));

  EXPECT_TRUE(pe->InDomain(key1.string()));

  // check the correct key bounds comparisons
  RocksDBKeyBounds bounds = RocksDBKeyBounds::VPackIndex(1, false);
  EXPECT_TRUE(pe->InDomain(bounds.start()));
  EXPECT_TRUE(pe->InDomain(bounds.end()));
  rocksdb::Slice prefixBegin = pe->Transform(bounds.start());
  rocksdb::Slice prefixEnd = pe->Transform(bounds.end());
  EXPECT_TRUE(pe->InDomain(prefixBegin));
  EXPECT_TRUE(pe->InDomain(prefixEnd));
  EXPECT_EQ(memcmp(bounds.start().data(), prefixBegin.data(), prefixBegin.size()), 0);
  EXPECT_EQ(memcmp(bounds.end().data(), prefixEnd.data(), prefixEnd.size()), 0);
  EXPECT_EQ(prefixBegin.data()[0], '\0');
  EXPECT_EQ(prefixEnd.data()[0], '\0');
  EXPECT_EQ(prefixBegin.data()[prefixBegin.size() - 2], '\x00');
  EXPECT_EQ(prefixBegin.data()[prefixBegin.size() - 1], '\x01');
  EXPECT_EQ(prefixEnd.data()[prefixEnd.size() - 2], '\x00');
  EXPECT_EQ(prefixEnd.data()[prefixEnd.size() - 1], '\x02');

  // prefix is just object id
  auto cmp = std::make_unique<RocksDBVPackComparator>();
  EXPECT_LT(cmp->Compare(prefixBegin, prefixEnd), 0);
  EXPECT_LT(cmp->Compare(prefixBegin, key1.string()), 0);
  EXPECT_GT(cmp->Compare(prefixEnd, key1.string()), 0);

  EXPECT_LT(cmp->Compare(key1.string(), key2.string()), 0);
  EXPECT_LT(cmp->Compare(key2.string(), key3.string()), 0);
  EXPECT_LT(cmp->Compare(key1.string(), key3.string()), 0);

  EXPECT_LT(cmp->Compare(prefixEnd, key3.string()), 0);

  // check again with reverse full iteration bounds
  bounds = RocksDBKeyBounds::VPackIndex(1, true);
  EXPECT_TRUE(pe->InDomain(bounds.start()));
  EXPECT_TRUE(pe->InDomain(bounds.end()));
  prefixBegin = pe->Transform(bounds.start());
  prefixEnd = pe->Transform(bounds.end());
  EXPECT_TRUE(pe->InDomain(prefixBegin));
  EXPECT_TRUE(pe->InDomain(prefixEnd));
  EXPECT_EQ(memcmp(bounds.start().data(), prefixBegin.data(), prefixBegin.size()), 0);
  EXPECT_EQ(memcmp(bounds.end().data(), prefixEnd.data(), prefixEnd.size()), 0);
  EXPECT_EQ(prefixBegin.data()[0], '\0');
  EXPECT_EQ(prefixEnd.data()[0], '\0');
  EXPECT_EQ(prefixBegin.data()[prefixBegin.size() - 2], '\x00');
  EXPECT_EQ(prefixBegin.data()[prefixBegin.size() - 1], '\x01');
  EXPECT_EQ(prefixEnd.data()[prefixEnd.size() - 2], '\x00');
  EXPECT_EQ(prefixEnd.data()[prefixEnd.size() - 1], '\x01');

  EXPECT_EQ(cmp->Compare(prefixBegin, prefixEnd), 0);
  EXPECT_LT(cmp->Compare(prefixBegin, key1.string()), 0);
  EXPECT_LT(cmp->Compare(prefixEnd, key1.string()), 0);

  EXPECT_LT(cmp->Compare(key1.string(), key2.string()), 0);
  EXPECT_LT(cmp->Compare(key2.string(), key3.string()), 0);
  EXPECT_LT(cmp->Compare(key1.string(), key3.string()), 0);

  EXPECT_LT(cmp->Compare(prefixEnd, key3.string()), 0);

  VPackBuilder a;
  a(VPackValue(VPackValueType::Array))(VPackValue(1))();
  VPackBuilder b;
  b(VPackValue(VPackValueType::Array))(VPackValue(3))();
  VPackBuilder c;
  c(VPackValue(VPackValueType::Array))(VPackValue(5))();

  RocksDBKey key4, key5, key6, key7;
  key4.constructVPackIndexValue(1, a.slice(), LocalDocumentId(18));
  key5.constructVPackIndexValue(1, b.slice(), LocalDocumentId(60));
  key6.constructVPackIndexValue(1, b.slice(), LocalDocumentId(90));
  key7.constructVPackIndexValue(1, c.slice(), LocalDocumentId(12));

  bounds = RocksDBKeyBounds::VPackIndex(1, a.slice(), c.slice());
  EXPECT_LT(cmp->Compare(bounds.start(), key4.string()), 0);
  EXPECT_LT(cmp->Compare(key4.string(), bounds.end()), 0);
  EXPECT_LT(cmp->Compare(bounds.start(), key5.string()), 0);
  EXPECT_LT(cmp->Compare(key5.string(), bounds.end()), 0);
  EXPECT_LT(cmp->Compare(bounds.start(), key6.string()), 0);
  EXPECT_LT(cmp->Compare(key6.string(), bounds.end()), 0);
  EXPECT_LT(cmp->Compare(bounds.start(), key7.string()), 0);
  EXPECT_LT(cmp->Compare(key7.string(), bounds.end()), 0);

  EXPECT_LT(cmp->Compare(key4.string(), key5.string()), 0);
  EXPECT_LT(cmp->Compare(key5.string(), key6.string()), 0);
  EXPECT_LT(cmp->Compare(key4.string(), key6.string()), 0);
  EXPECT_LT(cmp->Compare(key6.string(), key7.string()), 0);
  EXPECT_LT(cmp->Compare(key4.string(), key7.string()), 0);
}
