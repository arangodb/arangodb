////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2026 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include "RocksDBEngine/StorageEngineIndexTest.h"

#include "Basics/ResultAssertions.h"
#include "VelocypackUtils/VelocyPackStringLiteral.h"

#include <algorithm>

using namespace arangodb;
using namespace arangodb::tests;
using namespace arangodb::velocypack;

// ===========================================================================
// Secondary index entries (physical storage identity, index level)
// ===========================================================================

TEST_F(StorageEngineIndexTest, SecondaryIndexReflectsInsertedDocuments) {
  auto index = makeIndex(R"({"type":"persistent","fields":["value"]})"_vpack);

  ASSERT_TRUE(IsOk(insertR(keyed("a", 1))));
  ASSERT_TRUE(IsOk(insertR(keyed("b", 2))));
  ASSERT_TRUE(IsOk(insertR(keyed("c", 3))));

  auto entries = scanIndex(*index);
  ASSERT_EQ(entries.size(), 3u);

  for (auto const* key : {"a", "b", "c"}) {
    auto expected = lookupKey(key).first;
    EXPECT_NE(std::find(entries.begin(), entries.end(), expected),
              entries.end());
  }
}

TEST_F(StorageEngineIndexTest, UpdateIndexedFieldReplacesIndexEntry) {
  auto index = makeIndex(R"({"type":"persistent","fields":["value"]})"_vpack);
  ASSERT_TRUE(IsOk(insertR(keyed("k", 1))));

  ASSERT_TRUE(IsOk(updateR(keyed("k", 2))));

  // A stale entry (keyed on value 1) would make this return two.
  auto entries = scanIndex(*index);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries.front(), lookupKey("k").first);
}

TEST_F(StorageEngineIndexTest, RemoveDeletesSecondaryIndexEntry) {
  auto index = makeIndex(R"({"type":"persistent","fields":["value"]})"_vpack);
  ASSERT_TRUE(IsOk(insertR(keyed("a", 1))));
  ASSERT_TRUE(IsOk(insertR(keyed("b", 2))));
  ASSERT_EQ(scanIndex(*index).size(), 2u);

  ASSERT_TRUE(IsOk(remove(keyOnly("a"))));

  auto entries = scanIndex(*index);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries.front(), lookupKey("b").first);
}
