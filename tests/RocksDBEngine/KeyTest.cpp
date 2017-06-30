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
    RocksDBKey key1 = RocksDBKey::Database(0);
    auto const& s1 = key1.string();

    CHECK(s1.size() == sizeof(char) + sizeof(uint64_t));
    CHECK(s1 == std::string("0\0\0\0\0\0\0\0\0\0", 9));

    RocksDBKey key2 = RocksDBKey::Database(1);
    auto const& s2 = key2.string();

    CHECK(s2.size() == sizeof(char) + sizeof(uint64_t));
    CHECK(s2 == std::string("0\1\0\0\0\0\0\0\0\0", 9));

    RocksDBKey key3 = RocksDBKey::Database(255);
    auto const& s3 = key3.string();

    CHECK(s3.size() == sizeof(char) + sizeof(uint64_t));
    CHECK(s3 == std::string("0\xff\0\0\0\0\0\0\0\0", 9));

    RocksDBKey key4 = RocksDBKey::Database(256);
    auto const& s4 = key4.string();

    CHECK(s4.size() == sizeof(char) + sizeof(uint64_t));
    CHECK(s4 == std::string("0\0\x01\0\0\0\0\0\0\0", 9));

    RocksDBKey key5 = RocksDBKey::Database(49152);
    auto const& s5 = key5.string();

    CHECK(s5.size() == sizeof(char) + sizeof(uint64_t));
    CHECK(s5 == std::string("0\0\xc0\0\0\0\0\0\0\0", 9));

    RocksDBKey key6 = RocksDBKey::Database(12345678901);
    auto const& s6 = key6.string();

    CHECK(s6.size() == sizeof(char) + sizeof(uint64_t));
    CHECK(s6 == std::string("0\x35\x1c\xdc\xdf\x02\0\0\0", 9));

    RocksDBKey key7 = RocksDBKey::Database(0xf0f1f2f3f4f5f6f7ULL);
    auto const& s7 = key7.string();

    CHECK(s7.size() == sizeof(char) + sizeof(uint64_t));
    CHECK(s7 == std::string("0\xf7\xf6\xf5\xf4\xf3\xf2\xf1\xf0", 9));
  }

  /// @brief test collection
  SECTION("test_collection") {
    RocksDBKey key1 = RocksDBKey::Collection(0, 0);
    auto const& s1 = key1.string();

    CHECK(s1.size() == sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
    CHECK(s1 == std::string("1\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 17));

    RocksDBKey key2 = RocksDBKey::Collection(23, 42);
    auto const& s2 = key2.string();

    CHECK(s2.size() == sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
    CHECK(s2 == std::string("1\x17\0\0\0\0\0\0\0\x2a\0\0\0\0\0\0\0", 17));

    RocksDBKey key3 = RocksDBKey::Collection(255, 255);
    auto const& s3 = key3.string();

    CHECK(s3.size() == sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
    CHECK(s3 == std::string("1\xff\0\0\0\0\0\0\0\xff\0\0\0\0\0\0\0", 17));

    RocksDBKey key4 = RocksDBKey::Collection(256, 257);
    auto const& s4 = key4.string();

    CHECK(s4.size() == sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
    CHECK(s4 == std::string("1\0\x01\0\0\0\0\0\0\x01\x01\0\0\0\0\0\0", 17));

    RocksDBKey key5 = RocksDBKey::Collection(49152, 16384);
    auto const& s5 = key5.string();

    CHECK(s5.size() == sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
    CHECK(s5 == std::string("1\0\xc0\0\0\0\0\0\0\0\x40\0\0\0\0\0\0", 17));

    RocksDBKey key6 = RocksDBKey::Collection(12345678901, 987654321);
    auto const& s6 = key6.string();

    CHECK(s6.size() == sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
    CHECK(s6 == std::string(
                    "1\x35\x1c\xdc\xdf\x02\0\0\0\xb1\x68\xde\x3a\0\0\0\0", 17));

    RocksDBKey key7 =
        RocksDBKey::Collection(0xf0f1f2f3f4f5f6f7ULL, 0xf0f1f2f3f4f5f6f7ULL);
    auto const& s7 = key7.string();

    CHECK(s7.size() == sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
    CHECK(
        s7 ==
        std::string(
            "1\xf7\xf6\xf5\xf4\xf3\xf2\xf1\xf0\xf7\xf6\xf5\xf4\xf3\xf2\xf1\xf0",
            17));
  }

  /// @brief test document
  SECTION("test_document") {
    RocksDBKey key1 = RocksDBKey::Document(0, 0);
    auto const& s1 = key1.string();

    CHECK(s1.size() == +sizeof(uint64_t) + sizeof(uint64_t));
    CHECK(s1 == std::string("\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 16));

    RocksDBKey key2 = RocksDBKey::Document(23, 42);
    auto const& s2 = key2.string();

    CHECK(s2.size() == +sizeof(uint64_t) + sizeof(uint64_t));
    CHECK(s2 == std::string("\x17\0\0\0\0\0\0\0\x2a\0\0\0\0\0\0\0", 16));

    RocksDBKey key3 = RocksDBKey::Document(255, 255);
    auto const& s3 = key3.string();

    CHECK(s3.size() == sizeof(uint64_t) + sizeof(uint64_t));
    CHECK(s3 == std::string("\xff\0\0\0\0\0\0\0\xff\0\0\0\0\0\0\0", 16));

    RocksDBKey key4 = RocksDBKey::Document(256, 257);
    auto const& s4 = key4.string();

    CHECK(s4.size() == sizeof(uint64_t) + sizeof(uint64_t));
    CHECK(s4 == std::string("\0\x01\0\0\0\0\0\0\x01\x01\0\0\0\0\0\0", 16));

    RocksDBKey key5 = RocksDBKey::Document(49152, 16384);
    auto const& s5 = key5.string();

    CHECK(s5.size() == sizeof(uint64_t) + sizeof(uint64_t));
    CHECK(s5 == std::string("\0\xc0\0\0\0\0\0\0\0\x40\0\0\0\0\0\0", 16));

    RocksDBKey key6 = RocksDBKey::Document(12345678901, 987654321);
    auto const& s6 = key6.string();

    CHECK(s6.size() == sizeof(uint64_t) + sizeof(uint64_t));
    CHECK(s6 == std::string(
                    "\x35\x1c\xdc\xdf\x02\0\0\0\xb1\x68\xde\x3a\0\0\0\0", 16));

    RocksDBKey key7 =
        RocksDBKey::Document(0xf0f1f2f3f4f5f6f7ULL, 0xf0f1f2f3f4f5f6f7ULL);
    auto const& s7 = key7.string();

    CHECK(s7.size() == sizeof(uint64_t) + sizeof(uint64_t));
    CHECK(
        s7 ==
        std::string(
            "\xf7\xf6\xf5\xf4\xf3\xf2\xf1\xf0\xf7\xf6\xf5\xf4\xf3\xf2\xf1\xf0",
            16));
  }

  /// @brief test primary index
  SECTION("test_primary_index") {
    RocksDBKey key1 = RocksDBKey::PrimaryIndexValue(0, StringRef(""));
    auto const& s1 = key1.string();

    CHECK(s1.size() == sizeof(uint64_t) + strlen(""));
    CHECK(s1 == std::string("\0\0\0\0\0\0\0\0", 8));

    RocksDBKey key2 = RocksDBKey::PrimaryIndexValue(0, StringRef("abc"));
    auto const& s2 = key2.string();

    CHECK(s2.size() == sizeof(uint64_t) + strlen("abc"));
    CHECK(s2 == std::string("\0\0\0\0\0\0\0\0abc", 11));

    RocksDBKey key3 = RocksDBKey::PrimaryIndexValue(0, StringRef(" "));
    auto const& s3 = key3.string();

    CHECK(s3.size() == sizeof(uint64_t) + strlen(" "));
    CHECK(s3 == std::string("\0\0\0\0\0\0\0\0 ", 9));

    RocksDBKey key4 =
        RocksDBKey::PrimaryIndexValue(0, StringRef("this is a key"));
    auto const& s4 = key4.string();

    CHECK(s4.size() == sizeof(uint64_t) + strlen("this is a key"));
    CHECK(s4 == std::string("\0\0\0\0\0\0\0\0this is a key", 21));

    // 254 bytes
    char const* longKey =
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
        "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa";
    RocksDBKey key5 = RocksDBKey::PrimaryIndexValue(0, StringRef(longKey));
    auto const& s5 = key5.string();

    CHECK(s5.size() == sizeof(uint64_t) + strlen(longKey));
    CHECK(s5 == std::string("\0\0\0\0\0\0\0\0", 8) + longKey);

    RocksDBKey key6 =
        RocksDBKey::PrimaryIndexValue(123456789, StringRef("this is a key"));
    auto const& s6 = key6.string();

    CHECK(s6.size() == sizeof(uint64_t) + strlen("this is a key"));
    CHECK(s6 == std::string("\x15\xcd\x5b\x07\0\0\0\0this is a key", 21));
  }

  /// @brief test edge index
  SECTION("test_edge_index") {
    RocksDBKey key1 = RocksDBKey::EdgeIndexValue(0, StringRef("a/1"), 33);
    RocksDBKey key2 = RocksDBKey::EdgeIndexValue(0, StringRef("b/1"), 33);
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

    RocksDBKey k1 = RocksDBKey::GeoIndexValue(256, 128, false);
    RocksDBKeyBounds bb1 = RocksDBKeyBounds::GeoIndex(256, false);

    CHECK(cmp->Compare(k1.string(), bb1.start()) > 0);
    CHECK(cmp->Compare(k1.string(), bb1.end()) < 0);

    RocksDBKey k2 = RocksDBKey::GeoIndexValue(256, 128, true);
    RocksDBKeyBounds bb2 = RocksDBKeyBounds::GeoIndex(256, true);
    CHECK(cmp->Compare(k2.string(), bb2.start()) > 0);
    CHECK(cmp->Compare(k2.string(), bb2.end()) < 0);
  }

  /// @brief test edge index with dynamic prefix extractor
  SECTION("test_edge_index") {
    RocksDBKey key1 = RocksDBKey::EdgeIndexValue(0, StringRef("a/1"), 33);
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

    RocksDBKey key2 = RocksDBKey::EdgeIndexValue(0, StringRef("c/1000"), 33);
    CHECK(cmp->Compare(prefixBegin, key2.string()) < 0);
    CHECK(cmp->Compare(prefixEnd, key2.string()) > 0);

    // test higher prefix
    RocksDBKey key3 = RocksDBKey::EdgeIndexValue(1, StringRef("c/1000"), 33);
    CHECK(cmp->Compare(prefixBegin, key3.string()) < 0);
    CHECK(cmp->Compare(prefixEnd, key3.string()) < 0);
  }

  /// @brief test hash index with prefix over indexed slice
  SECTION("test_hash_index") {

    VPackBuilder t;
    t(VPackValue(VPackValueType::Array))(VPackSlice::trueSlice())();
    VPackBuilder f;
    f(VPackValue(VPackValueType::Array))(VPackSlice::falseSlice())();

    RocksDBKey key1 = RocksDBKey::VPackHashIndexValue(0, f.slice(), 33);
    RocksDBKey key2 = RocksDBKey::VPackHashIndexValue(0, t.slice(), 34);
    RocksDBKey key3 = RocksDBKey::VPackHashIndexValue(1, f.slice(), 35);
    
    std::string s1 = key1.string();
    std::string s2 = key1.string();
    CHECK(static_cast<uint8_t>(s1.data()[s1.size()-1]) == 0xFFU);
    CHECK(static_cast<uint8_t>(s2.data()[s1.size()-1]) == 0xFFU);

    // check the variable length edge prefix
    auto pe = std::make_unique<RocksDBPrefixExtractor>();
    CHECK(pe->InDomain(key1.string()));

    // check the correct key bounds comparisons
    RocksDBKeyBounds bounds = RocksDBKeyBounds::VPackHashIndex(0);
    CHECK_FALSE(pe->InDomain(bounds.start()));
    CHECK_FALSE(pe->InDomain(bounds.end()));
    rocksdb::Slice prefixBegin = pe->Transform(bounds.start());
    rocksdb::Slice prefixEnd = pe->Transform(bounds.end());
    CHECK_FALSE(pe->InDomain(prefixBegin));
    CHECK_FALSE(pe->InDomain(prefixEnd));
    CHECK(memcmp(bounds.start().data(), prefixBegin.data(),
                 prefixBegin.size()) == 0);
    CHECK(memcmp(bounds.end().data(), prefixEnd.data(), prefixEnd.size()) == 0);
    CHECK(prefixBegin.data()[prefixBegin.size()-1] == '\0');
    CHECK(prefixEnd.data()[prefixBegin.size()-1] == '\0');
    
    // check our assumptions about bound construction
    auto cmp = std::make_unique<RocksDBVPackComparator>();
    CHECK(cmp->Compare(prefixBegin, prefixEnd) < 0);
    CHECK(cmp->Compare(prefixBegin, key1.string()) < 0);
    CHECK(cmp->Compare(prefixEnd, key1.string()) > 0);

    CHECK(cmp->Compare(prefixBegin, key2.string()) < 0);
    CHECK(cmp->Compare(prefixEnd, key2.string()) > 0);

    CHECK(cmp->Compare(key1.string(), key2.string()) < 0);
    CHECK(cmp->Compare(key2.string(), key3.string()) < 0);
    CHECK(cmp->Compare(key1.string(), key3.string()) < 0);

    // test higher prefix
    CHECK(cmp->Compare(prefixBegin, key3.string()) < 0);
    CHECK(cmp->Compare(prefixEnd, key3.string()) < 0);
    
    
    bounds = RocksDBKeyBounds::VPackHashIndex(0, f.slice());
    CHECK(cmp->Compare(bounds.start(), key1.string()) < 0);
    CHECK(cmp->Compare(bounds.end(), key1.string()) > 0);
    // check prefix ordering
    prefixBegin = pe->Transform(bounds.start());
    prefixEnd = pe->Transform(bounds.end());
    CHECK(memcmp(bounds.start().data(), prefixBegin.data(),
                 prefixBegin.size()) == 0);
    CHECK(memcmp(bounds.end().data(), prefixEnd.data(), prefixEnd.size()) == 0);
    CHECK(cmp->Compare(prefixBegin, key1.string()) < 0);
    CHECK(cmp->Compare(prefixEnd, key1.string()) < 0);
    CHECK(prefixBegin == prefixEnd);


    // prefixes are different
    CHECK(cmp->Compare(bounds.start(), key2.string()) < 0);
    CHECK(cmp->Compare(bounds.end(), key2.string()) < 0);
    CHECK(cmp->Compare(prefixBegin, pe->Transform(key2.string())) < 0);
    CHECK(cmp->Compare(prefixEnd, pe->Transform(key2.string())) < 0);
  }
}
