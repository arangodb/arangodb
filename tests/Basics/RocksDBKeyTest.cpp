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

#include "RocksDBEngine/RocksDBComparator.h"
#include "RocksDBEngine/RocksDBKey.h"
#include "RocksDBEngine/RocksDBKeyBounds.h"
#include "RocksDBEngine/RocksDBTypes.h"
#include "Basics/Exceptions.h"

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
  
  CHECK(s6.size() == sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t)) ;
  CHECK(s6 == std::string("1\x35\x1c\xdc\xdf\x02\0\0\0\xb1\x68\xde\x3a\0\0\0\0", 17));
  
  RocksDBKey key7 = RocksDBKey::Collection(0xf0f1f2f3f4f5f6f7ULL, 0xf0f1f2f3f4f5f6f7ULL); 
  auto const& s7 = key7.string();
  
  CHECK(s7.size() == sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
  CHECK(s7 == std::string("1\xf7\xf6\xf5\xf4\xf3\xf2\xf1\xf0\xf7\xf6\xf5\xf4\xf3\xf2\xf1\xf0", 17));
}

/// @brief test document
SECTION("test_document") {
  RocksDBKey key1 = RocksDBKey::Document(0, 0); 
  auto const& s1 = key1.string();
  
  CHECK(s1.size() == sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
  CHECK(s1 == std::string("3\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0\0", 17));
  
  RocksDBKey key2 = RocksDBKey::Document(23, 42); 
  auto const& s2 = key2.string();
  
  CHECK(s2.size() == sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
  CHECK(s2 == std::string("3\x17\0\0\0\0\0\0\0\x2a\0\0\0\0\0\0\0", 17));
  
  RocksDBKey key3 = RocksDBKey::Document(255, 255); 
  auto const& s3 = key3.string();
  
  CHECK(s3.size() == sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
  CHECK(s3 == std::string("3\xff\0\0\0\0\0\0\0\xff\0\0\0\0\0\0\0", 17));
  
  RocksDBKey key4 = RocksDBKey::Document(256, 257); 
  auto const& s4 = key4.string();
  
  CHECK(s4.size() == sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
  CHECK(s4 == std::string("3\0\x01\0\0\0\0\0\0\x01\x01\0\0\0\0\0\0", 17));
  
  RocksDBKey key5 = RocksDBKey::Document(49152, 16384); 
  auto const& s5 = key5.string();
  
  CHECK(s5.size() == sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
  CHECK(s5 == std::string("3\0\xc0\0\0\0\0\0\0\0\x40\0\0\0\0\0\0", 17));
  
  RocksDBKey key6 = RocksDBKey::Document(12345678901, 987654321); 
  auto const& s6 = key6.string();
  
  CHECK(s6.size() == sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t)) ;
  CHECK(s6 == std::string("3\x35\x1c\xdc\xdf\x02\0\0\0\xb1\x68\xde\x3a\0\0\0\0", 17));
  
  RocksDBKey key7 = RocksDBKey::Document(0xf0f1f2f3f4f5f6f7ULL, 0xf0f1f2f3f4f5f6f7ULL); 
  auto const& s7 = key7.string();
  
  CHECK(s7.size() == sizeof(char) + sizeof(uint64_t) + sizeof(uint64_t));
  CHECK(s7 == std::string("3\xf7\xf6\xf5\xf4\xf3\xf2\xf1\xf0\xf7\xf6\xf5\xf4\xf3\xf2\xf1\xf0", 17));
}

/// @brief test primary index
SECTION("test_primary_index") {
  RocksDBKey key1 = RocksDBKey::PrimaryIndexValue(0, StringRef("")); 
  auto const& s1 = key1.string();
  
  CHECK(s1.size() == sizeof(char) + sizeof(uint64_t) + strlen(""));
  CHECK(s1 == std::string("4\0\0\0\0\0\0\0\0", 9));
  
  RocksDBKey key2 = RocksDBKey::PrimaryIndexValue(0, StringRef("abc")); 
  auto const& s2 = key2.string();
  
  CHECK(s2.size() == sizeof(char) + sizeof(uint64_t) + strlen("abc"));
  CHECK(s2 == std::string("4\0\0\0\0\0\0\0\0abc", 12));
  
  RocksDBKey key3 = RocksDBKey::PrimaryIndexValue(0, StringRef(" ")); 
  auto const& s3 = key3.string();
  
  CHECK(s3.size() == sizeof(char) + sizeof(uint64_t) + strlen(" "));
  CHECK(s3 == std::string("4\0\0\0\0\0\0\0\0 ", 10));
  
  RocksDBKey key4 = RocksDBKey::PrimaryIndexValue(0, StringRef("this is a key")); 
  auto const& s4 = key4.string();
  
  CHECK(s4.size() == sizeof(char) + sizeof(uint64_t) + strlen("this is a key"));
  CHECK(s4 == std::string("4\0\0\0\0\0\0\0\0this is a key", 22));

  // 254 bytes
  char const* longKey = "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"; 
  RocksDBKey key5 = RocksDBKey::PrimaryIndexValue(0, StringRef(longKey)); 
  auto const& s5 = key5.string();
  
  CHECK(s5.size() == sizeof(char) + sizeof(uint64_t) + strlen(longKey));
  CHECK(s5 == std::string("4\0\0\0\0\0\0\0\0", 9) + longKey);
  
  RocksDBKey key6 = RocksDBKey::PrimaryIndexValue(123456789, StringRef("this is a key")); 
  auto const& s6 = key6.string();
  
  CHECK(s6.size() == sizeof(char) + sizeof(uint64_t) + strlen("this is a key"));
  CHECK(s6 == std::string("4\x15\xcd\x5b\x07\0\0\0\0this is a key", 22));
}

/// @brief test edge index
SECTION("test_edge_index") {
  RocksDBKey key1 = RocksDBKey::EdgeIndexValue(0, StringRef("a/1"), 33);
  auto const& s1 = key1.string();

  CHECK(s1.size() == sizeof(char) + sizeof(uint64_t) + strlen("a/1") + sizeof(char) + sizeof(uint64_t));
  CHECK(s1 == std::string("5\0\0\0\0\0\0\0\0a/1\0!\0\0\0\0\0\0\0", 21));
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
  
}
