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

#include "RocksDBEngine/RocksDBEdgeIndex.h"

#include "gtest/gtest.h"

using namespace arangodb;

TEST(CachedCollectionNameTest, test_empty) {
  RocksDBEdgeIndex::CachedCollectionName testee;
  ASSERT_EQ(nullptr, testee.get());
}

TEST(CachedCollectionNameTest, test_set_invalid_values) {
  {
    RocksDBEdgeIndex::CachedCollectionName testee;
    std::string const* previous = nullptr;
    std::string_view result =
        testee.buildCompressedValue(previous, "foobar/123");
    ASSERT_NE(nullptr, previous);
    ASSERT_EQ("foobar", *previous);
    ASSERT_EQ("/123", result);
    ASSERT_NE(nullptr, testee.get());
    ASSERT_EQ("foobar", *testee.get());

    result = testee.buildCompressedValue(previous, "foobar/123/456");
    ASSERT_NE(nullptr, previous);
    ASSERT_EQ("foobar", *previous);
    ASSERT_EQ("/123/456", result);
    ASSERT_NE(nullptr, testee.get());
    ASSERT_EQ("foobar", *testee.get());
  }

  {
    RocksDBEdgeIndex::CachedCollectionName testee;
    std::string const* previous = nullptr;
    std::string_view result = testee.buildCompressedValue(previous, "/123");
    ASSERT_EQ(nullptr, previous);
    // expecting an empty result back
    ASSERT_EQ("", result);
  }

  {
    RocksDBEdgeIndex::CachedCollectionName testee;
    std::string const* previous = nullptr;
    std::string_view result = testee.buildCompressedValue(previous, "abc//123");
    ASSERT_EQ(nullptr, previous);
    // expecting an empty result back
    ASSERT_EQ("", result);
  }

  {
    RocksDBEdgeIndex::CachedCollectionName testee;
    std::string const* previous = nullptr;
    std::string_view result =
        testee.buildCompressedValue(previous, "der-fuchs");
    ASSERT_EQ(nullptr, previous);
    // expecting an empty result back
    ASSERT_EQ("", result);
  }

  {
    RocksDBEdgeIndex::CachedCollectionName testee;
    std::string const* previous = nullptr;
    std::string_view result = testee.buildCompressedValue(previous, "");
    ASSERT_EQ(nullptr, previous);
    // expecting an empty result back
    ASSERT_EQ("", result);
  }
}

TEST(CachedCollectionNameTest, test_set_once) {
  RocksDBEdgeIndex::CachedCollectionName testee;
  std::string const* previous = nullptr;
  std::string_view result = testee.buildCompressedValue(previous, "foobar/abc");
  ASSERT_NE(nullptr, previous);
  ASSERT_EQ("foobar", *previous);
  ASSERT_EQ("/abc", result);
  ASSERT_NE(nullptr, testee.get());
  ASSERT_EQ("foobar", *testee.get());
}

TEST(CachedCollectionNameTest, test_set_multiple_times_same_collection) {
  RocksDBEdgeIndex::CachedCollectionName testee;
  std::string const* previous = nullptr;
  std::string_view result = testee.buildCompressedValue(previous, "foobar/abc");
  ASSERT_NE(nullptr, previous);
  ASSERT_EQ("foobar", *previous);
  ASSERT_EQ("/abc", result);
  ASSERT_NE(nullptr, testee.get());
  ASSERT_EQ("foobar", *testee.get());

  result = testee.buildCompressedValue(previous, "foobar/def");
  ASSERT_NE(nullptr, previous);
  ASSERT_EQ("foobar", *previous);
  ASSERT_EQ("/def", result);
  ASSERT_NE(nullptr, testee.get());
  ASSERT_EQ("foobar", *testee.get());
}

TEST(CachedCollectionNameTest, test_set_multiple_times_different_collection) {
  RocksDBEdgeIndex::CachedCollectionName testee;
  std::string const* previous = nullptr;
  std::string_view result = testee.buildCompressedValue(previous, "foobar/abc");
  ASSERT_NE(nullptr, previous);
  ASSERT_EQ("foobar", *previous);
  ASSERT_EQ("/abc", result);
  ASSERT_NE(nullptr, testee.get());
  ASSERT_EQ("foobar", *testee.get());

  result = testee.buildCompressedValue(previous, "barbaz/123456");
  ASSERT_NE(nullptr, previous);
  ASSERT_EQ("foobar", *previous);
  ASSERT_EQ("barbaz/123456", result);
  ASSERT_NE(nullptr, testee.get());
  ASSERT_EQ("foobar", *testee.get());
}

TEST(CachedCollectionNameTest, test_wrong_previous) {
  RocksDBEdgeIndex::CachedCollectionName testee;
  std::string const* previous = nullptr;
  std::string_view result = testee.buildCompressedValue(previous, "foobar/abc");
  ASSERT_NE(nullptr, previous);
  ASSERT_EQ("foobar", *previous);
  ASSERT_EQ("/abc", result);
  ASSERT_NE(nullptr, testee.get());
  ASSERT_EQ("foobar", *testee.get());

  std::string other = "qux";
  previous = &other;

  result = testee.buildCompressedValue(previous, "foobar/123456");
  ASSERT_NE(nullptr, previous);
  ASSERT_EQ("qux", *previous);
  ASSERT_EQ("foobar/123456", result);
  ASSERT_NE(nullptr, testee.get());
  ASSERT_EQ("foobar", *testee.get());

  result = testee.buildCompressedValue(previous, "qux/123456");
  ASSERT_NE(nullptr, previous);
  ASSERT_EQ("qux", *previous);
  ASSERT_EQ("/123456", result);
  ASSERT_NE(nullptr, testee.get());
  ASSERT_EQ("foobar", *testee.get());
}
