////////////////////////////////////////////////////////////////////////////////
/// @brief test suite for PathEnumerator class
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2004-2012 triagens GmbH, Cologne, Germany
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
/// Copyright holder is triAGENS GmbH, Cologne, Germany
///
/// @author Jan Steemann
/// @author Copyright 2015, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Basics/Common.h"

#include "catch.hpp"

#include "Basics/Exceptions.h"
#include "Basics/VelocyPackHelper.h"
#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBPrefixExtractor.h"
#include "RocksDBEngine/RocksDBTypes.h"

using namespace arangodb;

// -----------------------------------------------------------------------------
// --SECTION--                                                        test suite
// -----------------------------------------------------------------------------

/// @brief test RocksDBKey class
TEST_CASE("RocksDBKeyTest", "[rocksdbkeytest]") {
  /// @brief test database
  SECTION("test_database") {
    RocksDBKey key;
    key.constructDatabase(0);
    auto const& s1 = key.string();

    CHECK(s1.size() == sizeof(char) + sizeof(uint64_t));
    CHECK(s1 == std::string("0\0\0\0\0\0\0\0\0\0", 9));

    key.constructDatabase(1);
    auto const& s2 = key.string();

    CHECK(s2.size() == sizeof(char) + sizeof(uint64_t));
    CHECK(s2 == std::string("0\1\0\0\0\0\0\0\0\0", 9));

    key.constructDatabase(255);
    auto const& s3 = key.string();

    CHECK(s3.size() == sizeof(char) + sizeof(uint64_t));
    CHECK(s3 == std::string("0\xff\0\0\0\0\0\0\0\0", 9));

    key.constructDatabase(256);
    auto const& s4 = key.string();

    CHECK(s4.size() == sizeof(char) + sizeof(uint64_t));
    CHECK(s4 == std::string("0\0\x01\0\0\0\0\0\0\0", 9));

    key.constructDatabase(49152);
    auto const& s5 = key.string();

    CHECK(s5.size() == sizeof(char) + sizeof(uint64_t));
    CHECK(s5 == std::string("0\0\xc0\0\0\0\0\0\0\0", 9));

    key.constructDatabase(12345678901);
    auto const& s6 = key.string();

    CHECK(s6.size() == sizeof(char) + sizeof(uint64_t));
    CHECK(s6 == std::string("0\x35\x1c\xdc\xdf\x02\0\0\0", 9));

    key.constructDatabase(0xf0f1f2f3f4f5f6f7ULL);
    auto const& s7 = key.string();

    CHECK(s7.size() == sizeof(char) + sizeof(uint64_t));
    CHECK(s7 == std::string("0\xf7\xf6\xf5\xf4\xf3\xf2\xf1\xf0", 9));
  }

  /// @brief test collection
  SECTION("test_collection") {
    RocksDBKey key;
    key.constructCollection(0, 0);
    auto const& s1 = key.string();

    CHECK(s1.size() == sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
    CHECK(s1 == std::string("1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 17));

    key.constructCollection(23, 42);
    auto const& s2 = key.string();

    CHECK(s2.size() == sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
    CHECK(s2 == std::string("1\x17\0\0\0\0\0\0\0\x2a\0\0\0\0\0\0\0", 17));

    key.constructCollection(255, 255);
    auto const& s3 = key.string();

    CHECK(s3.size() == sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
    CHECK(s3 == std::string("1\xff\0\0\0\0\0\0\0\xff\0\0\0\0\0\0\0", 17));

    key.constructCollection(256, 257);
    auto const& s4 = key.string();

    CHECK(s4.size() == sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
    CHECK(s4 == std::string("1\0\x01\0\0\0\0\0\0\x01\x01\0\0\0\0\0\0", 17));

    key.constructCollection(49152, 16384);
    auto const& s5 = key.string();

    CHECK(s5.size() == sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
    CHECK(s5 == std::string("1\0\xc0\0\0\0\0\0\0\0\x40\0\0\0\0\0\0", 17));

    key.constructCollection(12345678901, 987654321);
    auto const& s6 = key.string();

    CHECK(s6.size() == sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
    CHECK(s6 == std::string(
                    "1\x35\x1c\xdc\xdf\x02\0\0\0\xb1\x68\xde\x3a\0\0\0\0", 17));

    key.constructCollection(0xf0f1f2f3f4f5f6f7ULL, 0xf0f1f2f3f4f5f6f7ULL);
    auto const& s7 = key.string();

    CHECK(s7.size() == sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
    CHECK(
        s7 ==
        std::string(
            "1\xf7\xf6\xf5\xf4\xf3\xf2\xf1\xf0\xf7\xf6\xf5\xf4\xf3\xf2\xf1\xf0",
            17));
  }

  /// @brief test document
  SECTION("test_document") {
    RocksDBKey key;
    key.constructDocument(0, 0);
    auto const& s1 = key.string();

    CHECK(s1.size() == +sizeof(uint64_t) + sizeof(uint64_t));
    CHECK(s1 == std::string("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16));

    key.constructDocument(23, 42);
    auto const& s2 = key.string();

    CHECK(s2.size() == +sizeof(uint64_t) + sizeof(uint64_t));
    CHECK(s2 == std::string("\x17\0\0\0\0\0\0\0\x2a\0\0\0\0\0\0\0", 16));

    key.constructDocument(255, 255);
    auto const& s3 = key.string();

    CHECK(s3.size() == sizeof(uint64_t) + sizeof(uint64_t));
    CHECK(s3 == std::string("\xff\0\0\0\0\0\0\0\xff\0\0\0\0\0\0\0", 16));

    key.constructDocument(256, 257);
    auto const& s4 = key.string();

    CHECK(s4.size() == sizeof(uint64_t) + sizeof(uint64_t));
    CHECK(s4 == std::string("\0\x01\0\0\0\0\0\0\x01\x01\0\0\0\0\0\0", 16));

    key.constructDocument(49152, 16384);
    auto const& s5 = key.string();

    CHECK(s5.size() == sizeof(uint64_t) + sizeof(uint64_t));
    CHECK(s5 == std::string("\0\xc0\0\0\0\0\0\0\0\x40\0\0\0\0\0\0", 16));

    key.constructDocument(12345678901, 987654321);
    auto const& s6 = key.string();

    CHECK(s6.size() == sizeof(uint64_t) + sizeof(uint64_t));
    CHECK(s6 == std::string(
                    "\x35\x1c\xdc\xdf\x02\0\0\0\xb1\x68\xde\x3a\0\0\0\0", 16));

    key.constructDocument(0xf0f1f2f3f4f5f6f7ULL, 0xf0f1f2f3f4f5f6f7ULL);
    auto const& s7 = key.string();

    CHECK(s7.size() == sizeof(uint64_t) + sizeof(uint64_t));
    CHECK(
        s7 ==
        std::string(
            "\xf7\xf6\xf5\xf4\xf3\xf2\xf1\xf0\xf7\xf6\xf5\xf4\xf3\xf2\xf1\xf0",
            16));
  }

  /// @brief test primary index
  SECTION("test_primary_index") {
    RocksDBKey key;
    key.constructPrimaryIndexValue(0, StringRef(""));
    auto const& s1 = key.string();

    CHECK(s1.size() == sizeof(uint64_t) + strlen(""));
    CHECK(s1 == std::string("\0\0\0\0\0\0\0\0", 8));

    key.constructPrimaryIndexValue(0, StringRef("abc"));
    auto const& s2 = key.string();

    CHECK(s2.size() == sizeof(uint64_t) + strlen("abc"));
    CHECK(s2 == std::string("\0\0\0\0\0\0\0\0abc", 11));

    key.constructPrimaryIndexValue(0, StringRef(" "));
    auto const& s3 = key.string();

    CHECK(s3.size() == sizeof(uint64_t) + strlen(" "));
    CHECK(s3 == std::string("\0\0\0\0\0\0\0\0 ", 9));

    key.constructPrimaryIndexValue(0, StringRef("this is a key"));
    auto const& s4 = key.string();

    CHECK(s4.size() == sizeof(uint64_t) + strlen("this is a key"));
    CHECK(s4 == std::string("\0\0\0\0\0\0\0\0this is a key", 21));

    // 254 bytes
    char const* longKey =
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    key.constructPrimaryIndexValue(0, StringRef(longKey));
    auto const& s5 = key.string();

    CHECK(s5.size() == sizeof(uint64_t) + strlen(longKey));
    CHECK(s5 == std::string("\0\0\0\0\0\0\0\0", 8) + longKey);

    key.constructPrimaryIndexValue(123456789, StringRef("this is a key"));
    auto const& s6 = key.string();

    CHECK(s6.size() == sizeof(uint64_t) + strlen("this is a key"));
    CHECK(s6 == std::string("\x15\xcd\x5b\x07\0\0\0\0this is a key", 21));
  }

  /// @brief test edge index
  SECTION("test_edge_index") {
    RocksDBKey key1;
    key1.constructEdgeIndexValue(0, StringRef("a/1"), 33);
    RocksDBKey key2;
    key2.constructEdgeIndexValue(0, StringRef("b/1"), 33);
    auto const& s1 = key1.string();

    CHECK(s1.size() ==
          sizeof(uint64_t) + strlen("a/1") + sizeof(char) + sizeof(uint64_t) +
              sizeof(char));
    CHECK(s1 == std::string("\0\0\0\0\0\0\0\0a/1\0!\0\0\0\0\0\0\0\xff", 21));
    CHECK(key2.string().size() ==
          sizeof(uint64_t) + strlen("b/1") + sizeof(char) + sizeof(uint64_t) +
              sizeof(char));
    CHECK(key2.string() ==
          std::string("\0\0\0\0\0\0\0\0b/1\0!\0\0\0\0\0\0\0\xff", 21));
    
    CHECK(RocksDBKey::vertexId(key1).compare("a/1") == 0);
    CHECK(RocksDBKey::vertexId(key2).compare("b/1") == 0);

    // check the variable length edge prefix
    auto pe = std::make_unique<RocksDBPrefixExtractor>();
    CHECK(pe->InDomain(key1.string()));

    rocksdb::Slice prefix = pe->Transform(key1.string());
    CHECK(prefix.size() == sizeof(uint64_t) + strlen("a/1") + sizeof(char));
    CHECK(memcmp(s1.data(), prefix.data(), prefix.size()) == 0);

    rocksdb::Comparator const* cmp = rocksdb::BytewiseComparator();
    CHECK(cmp->Compare(key1.string(), key2.string()) < 0);
  }
}

/// @brief test RocksDBKeyBounds class
TEST_CASE("RocksDBKeyBoundsTest", "[rocksdbkeybounds]") {
  /// @brief test geo index key and bounds consistency
  SECTION("test_geo_index") {
    rocksdb::Comparator const* cmp = rocksdb::BytewiseComparator();

    RocksDBKey key;
    key.constructGeoIndexValue(256, 128, false);
    RocksDBKeyBounds bb1 = RocksDBKeyBounds::GeoIndex(256, false);

    CHECK(cmp->Compare(key.string(), bb1.start()) > 0);
    CHECK(cmp->Compare(key.string(), bb1.end()) < 0);

    key.constructGeoIndexValue(256, 128, true);
    RocksDBKeyBounds bb2 = RocksDBKeyBounds::GeoIndex(256, true);
    CHECK(cmp->Compare(key.string(), bb2.start()) > 0);
    CHECK(cmp->Compare(key.string(), bb2.end()) < 0);
  }

  /// @brief test edge index with dynamic prefix extractor
  SECTION("test_edge_index") {
    RocksDBKey key1;
    key1.constructEdgeIndexValue(0, StringRef("a/1"), 33);
    // check the variable length edge prefix
    auto pe = std::make_unique<RocksDBPrefixExtractor>();
    REQUIRE(pe->InDomain(key1.string()));

    // check the correct key bounds comparisons
    RocksDBKeyBounds bounds = RocksDBKeyBounds::EdgeIndex(0);
    REQUIRE_FALSE(pe->InDomain(bounds.start()));
    REQUIRE_FALSE(pe->InDomain(bounds.end()));
    rocksdb::Slice prefixBegin = pe->Transform(bounds.start());
    rocksdb::Slice prefixEnd = pe->Transform(bounds.end());
    REQUIRE_FALSE(pe->InDomain(prefixBegin));
    REQUIRE_FALSE(pe->InDomain(prefixEnd));
    REQUIRE(memcmp(bounds.start().data(), prefixBegin.data(),
                   prefixBegin.size()) == 0);
    REQUIRE(memcmp(bounds.end().data(), prefixEnd.data(), prefixEnd.size()) ==
            0);

    // check our assumptions about bound construction
    rocksdb::Comparator const* cmp = rocksdb::BytewiseComparator();
    CHECK(cmp->Compare(prefixBegin, prefixEnd) < 0);
    CHECK(cmp->Compare(prefixBegin, key1.string()) < 0);
    CHECK(cmp->Compare(prefixEnd, key1.string()) > 0);

    RocksDBKey key2;
    key2.constructEdgeIndexValue(0, StringRef("c/1000"), 33);
    CHECK(cmp->Compare(prefixBegin, key2.string()) < 0);
    CHECK(cmp->Compare(prefixEnd, key2.string()) > 0);

    // test higher prefix
    RocksDBKey key3;
    key3.constructEdgeIndexValue(1, StringRef("c/1000"), 33);
    CHECK(cmp->Compare(prefixBegin, key3.string()) < 0);
    CHECK(cmp->Compare(prefixEnd, key3.string()) < 0);
  }
  
  
  /// @brief test hash index with prefix over indexed slice
  SECTION("test_hash_index") {
    
    VPackBuilder lower;
    lower(VPackValue(VPackValueType::Array))(VPackValue("a"))();
    VPackBuilder higher;
    higher(VPackValue(VPackValueType::Array))(VPackValue("b"))();
    
    RocksDBKey key1, key2, key3;
    key1.constructVPackIndexValue(1, lower.slice(), 33);
    key2.constructVPackIndexValue(1, higher.slice(), 33);
    key3.constructVPackIndexValue(2, lower.slice(), 16);

    // check the variable length edge prefix
    std::unique_ptr<rocksdb::SliceTransform const> pe(rocksdb::NewFixedPrefixTransform(RocksDBKey::objectIdSize()));
    
    CHECK(pe->InDomain(key1.string()));
    
    // check the correct key bounds comparisons
    RocksDBKeyBounds bounds = RocksDBKeyBounds::VPackIndex(1);
    CHECK(pe->InDomain(bounds.start()));
    CHECK(pe->InDomain(bounds.end()));
    rocksdb::Slice prefixBegin = pe->Transform(bounds.start());
    rocksdb::Slice prefixEnd = pe->Transform(bounds.end());
    CHECK(pe->InDomain(prefixBegin));
    CHECK(pe->InDomain(prefixEnd));
    CHECK(memcmp(bounds.start().data(), prefixBegin.data(),
                 prefixBegin.size()) == 0);
    CHECK(memcmp(bounds.end().data(), prefixEnd.data(), prefixEnd.size()) == 0);
    CHECK(prefixBegin.data()[prefixBegin.size()-1] == '\0');
    CHECK(prefixEnd.data()[prefixBegin.size()-1] == '\0');
    
    // prefix is just object id
    auto cmp = std::make_unique<RocksDBVPackComparator>();
    CHECK(cmp->Compare(prefixBegin, prefixEnd) == 0);
    CHECK(cmp->Compare(prefixBegin, key1.string()) < 0);
    CHECK(cmp->Compare(prefixEnd, key1.string()) < 0);
    
    CHECK(cmp->Compare(key1.string(), key2.string()) < 0);
    CHECK(cmp->Compare(key2.string(), key3.string()) < 0);
    CHECK(cmp->Compare(key1.string(), key3.string()) < 0);
    
    CHECK(cmp->Compare(prefixEnd, key3.string()) < 0);
    
    
    VPackBuilder a;
    a(VPackValue(VPackValueType::Array))(VPackValue(1))();
    VPackBuilder b;
    b(VPackValue(VPackValueType::Array))(VPackValue(3))();
    VPackBuilder c;
    c(VPackValue(VPackValueType::Array))(VPackValue(5))();
    
    RocksDBKey key4, key5, key6, key7;
    key4.constructVPackIndexValue(1, a.slice(), 18);
    key5.constructVPackIndexValue(1, b.slice(), 60);
    key6.constructVPackIndexValue(1, b.slice(), 90);
    key7.constructVPackIndexValue(1, c.slice(), 12);
    
    bounds = RocksDBKeyBounds::VPackIndex(1, a.slice(), c.slice());
    CHECK(cmp->Compare(bounds.start(), key4.string()) < 0);
    CHECK(cmp->Compare(key4.string(), bounds.end()) < 0);
    CHECK(cmp->Compare(bounds.start(), key5.string()) < 0);
    CHECK(cmp->Compare(key5.string(), bounds.end()) < 0);
    CHECK(cmp->Compare(bounds.start(), key6.string()) < 0);
    CHECK(cmp->Compare(key6.string(), bounds.end()) < 0);
    CHECK(cmp->Compare(bounds.start(), key7.string()) < 0);
    CHECK(cmp->Compare(key7.string(), bounds.end()) < 0);

    CHECK(cmp->Compare(key4.string(), key5.string()) < 0);
    CHECK(cmp->Compare(key5.string(), key6.string()) < 0);
    CHECK(cmp->Compare(key4.string(), key6.string()) < 0);
    CHECK(cmp->Compare(key6.string(), key7.string()) < 0);
    CHECK(cmp->Compare(key4.string(), key7.string()) < 0);
  }
}
