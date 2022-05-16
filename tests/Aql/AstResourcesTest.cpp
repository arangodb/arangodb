////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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
////////////////////////////////////////////////////////////////////////////////

#include "Aql/AstResources.h"
#include "Aql/ShortStringStorage.h"
#include "Basics/GlobalResourceMonitor.h"
#include "Basics/ResourceUsage.h"

#include "gtest/gtest.h"

#include <string_view>

namespace {

TEST(ShortStringStorageTest, testEmpty) {
  arangodb::GlobalResourceMonitor global;
  arangodb::ResourceMonitor resourceMonitor(global);
  arangodb::aql::ShortStringStorage sss(resourceMonitor, /*block size*/ 4096);

  EXPECT_EQ(0, resourceMonitor.current());
  EXPECT_EQ(0, resourceMonitor.peak());
}

TEST(ShortStringStorageTest, testFillAndClear) {
  constexpr size_t kBlockSize = 4096;
  arangodb::GlobalResourceMonitor global;
  arangodb::ResourceMonitor resourceMonitor(global);
  arangodb::aql::ShortStringStorage sss(resourceMonitor,
                                        /*block size*/ kBlockSize);

  constexpr std::string_view payload("der-otto-mag-keine-pilze");

  size_t blocksUsed = 0;
  size_t memoryLeftInOpenBlock = 0;
  for (size_t i = 0; i < 1000; ++i) {
    if (payload.size() + 1 > memoryLeftInOpenBlock) {
      ++blocksUsed;
      memoryLeftInOpenBlock = kBlockSize;
    }
    memoryLeftInOpenBlock -= payload.size() + 1;
    char const* p = sss.registerString(payload.data(), payload.size());
    EXPECT_EQ(std::string_view(p, payload.size()), payload);
    EXPECT_EQ(blocksUsed, sss.usedBlocks());
    EXPECT_EQ(blocksUsed * kBlockSize, resourceMonitor.current());
  }

  sss.clear();
  // no blocks left after clear
  EXPECT_EQ(0, sss.usedBlocks());
  EXPECT_EQ(0, resourceMonitor.current());

  // adding a string again will lead a new block being allocated
  char const* p = sss.registerString(payload.data(), payload.size());
  EXPECT_EQ(std::string_view(p, payload.size()), payload);
  EXPECT_EQ(1, sss.usedBlocks());
  EXPECT_EQ(kBlockSize, resourceMonitor.current());
}

TEST(ShortStringStorageTest, testFillAndClearMost) {
  constexpr size_t kBlockSize = 2048;
  arangodb::GlobalResourceMonitor global;
  arangodb::ResourceMonitor resourceMonitor(global);
  arangodb::aql::ShortStringStorage sss(resourceMonitor,
                                        /*block size*/ kBlockSize);

  constexpr std::string_view payload("ein-hund-ein-hund-der-treibt-es-bunt");

  size_t blocksUsed = 0;
  size_t memoryLeftInOpenBlock = 0;
  for (size_t i = 0; i < 1000; ++i) {
    if (payload.size() + 1 > memoryLeftInOpenBlock) {
      ++blocksUsed;
      memoryLeftInOpenBlock = kBlockSize;
    }
    memoryLeftInOpenBlock -= payload.size() + 1;
    char const* p = sss.registerString(payload.data(), payload.size());
    EXPECT_EQ(std::string_view(p, payload.size()), payload);
    EXPECT_EQ(blocksUsed, sss.usedBlocks());
    EXPECT_EQ(blocksUsed * kBlockSize, resourceMonitor.current());
  }

  // one block will remain after clearMost
  sss.clearMost();
  EXPECT_EQ(1, sss.usedBlocks());
  EXPECT_EQ(kBlockSize, resourceMonitor.current());

  // adding a string again will lead to the one block being recycled
  char const* p = sss.registerString(payload.data(), payload.size());
  EXPECT_EQ(std::string_view(p, payload.size()), payload);
  EXPECT_EQ(1, sss.usedBlocks());
  EXPECT_EQ(kBlockSize, resourceMonitor.current());
}

TEST(AstResourcesTest, testLongStringsFillAndClear) {
  arangodb::GlobalResourceMonitor global;
  arangodb::ResourceMonitor resourceMonitor(global);
  arangodb::aql::AstResources resources(resourceMonitor);

  constexpr size_t overheadPerString =
      arangodb::aql::AstResources::memoryUsageForStringBlock();

  // this tests long string storage (too long for short string storage)
  constexpr std::string_view payload(
      "der-otto-mag-pilze-denn-er-bevorzugt-schmackhafte-nahrhafte-gesunde-"
      "natuerliche-kost-aus-dem-wald-denn-er-ist-ja-ein-otto-wer-auch-sonst");
  ASSERT_GT(payload.size(), arangodb::aql::ShortStringStorage::maxStringLength);

  size_t overhead = 0;
  size_t capacity = 0;
  for (size_t i = 0; i < 10; ++i) {
    if (i + 1 > capacity) {
      size_t newCapacity =
          std::max(arangodb::aql::AstResources::kMinCapacityForLongStrings,
                   capacity * 2);
      overhead += (newCapacity - capacity) * overheadPerString;
      capacity = newCapacity;
    }
    char const* p = resources.registerString(payload.data(), payload.size());
    EXPECT_EQ(std::string_view(p, payload.size()), payload);
    EXPECT_EQ((i + 1) * payload.size() + overhead, resourceMonitor.current());
  }

  resources.clear();
  EXPECT_EQ(capacity * overheadPerString, resourceMonitor.current());

  char const* p = resources.registerString(payload.data(), payload.size());
  EXPECT_EQ(std::string_view(p, payload.size()), payload);
  EXPECT_EQ(payload.size() + (capacity * overheadPerString),
            resourceMonitor.current());

  resources.clear();
  EXPECT_EQ(capacity * overheadPerString, resourceMonitor.current());
}

TEST(AstResourcesTest, testLongStringsFillAndClearMost) {
  arangodb::GlobalResourceMonitor global;
  arangodb::ResourceMonitor resourceMonitor(global);
  arangodb::aql::AstResources resources(resourceMonitor);

  constexpr size_t overheadPerString =
      arangodb::aql::AstResources::memoryUsageForStringBlock();

  // this tests long string storage (too long for short string storage)
  constexpr std::string_view payload(
      "der-otto-mag-pilze-denn-er-bevorzugt-schmackhafte-nahrhafte-gesunde-"
      "natuerliche-kost-aus-dem-wald-denn-er-ist-ja-ein-otto-wer-auch-sonst");
  ASSERT_GT(payload.size(), arangodb::aql::ShortStringStorage::maxStringLength);

  size_t overhead = 0;
  size_t capacity = 0;
  for (size_t i = 0; i < 10; ++i) {
    if (i + 1 > capacity) {
      size_t newCapacity =
          std::max(arangodb::aql::AstResources::kMinCapacityForLongStrings,
                   capacity * 2);
      overhead += (newCapacity - capacity) * overheadPerString;
      capacity = newCapacity;
    }
    char const* p = resources.registerString(payload.data(), payload.size());
    EXPECT_EQ(std::string_view(p, payload.size()), payload);
    EXPECT_EQ((i + 1) * payload.size() + overhead, resourceMonitor.current());
  }

  resources.clearMost();
  EXPECT_EQ(capacity * overheadPerString, resourceMonitor.current());

  char const* p = resources.registerString(payload.data(), payload.size());
  EXPECT_EQ(std::string_view(p, payload.size()), payload);
  EXPECT_EQ(payload.size() + (capacity * overheadPerString),
            resourceMonitor.current());

  resources.clearMost();
  EXPECT_EQ(capacity * overheadPerString, resourceMonitor.current());
}

}  // namespace
