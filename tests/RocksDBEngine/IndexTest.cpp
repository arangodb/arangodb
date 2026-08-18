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

// persistent/hash/skiplist all instantiate RocksDBVPackIndex with only
// type()/typeName() overridden - parameterized instead of duplicated per type.

class StorageEngineIndexTypeTest
    : public StorageEngineIndexTest,
      public testing::WithParamInterface<std::string_view> {
 protected:
  static VPackString indexDefinition(std::string_view type) {
    VPackBuilder b;
    b.openObject();
    b.add("type", VPackValue(type));
    b.add("fields", VPackValue(VPackValueType::Array));
    b.add(VPackValue("value"));
    b.close();
    b.close();
    return VPackString{b.slice()};
  }
};

TEST_P(StorageEngineIndexTypeTest, ReflectsInsertedDocuments) {
  auto index = makeIndex(indexDefinition(GetParam()));
  ASSERT_TRUE(IsOk(insertR(keyed("a", 1))));

  auto entries = scanIndex(*index);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries.front(), lookupKey("a").first);
}

TEST_P(StorageEngineIndexTypeTest, UpdateReplacesIndexEntry) {
  auto index = makeIndex(indexDefinition(GetParam()));
  ASSERT_TRUE(IsOk(insertR(keyed("k", 1))));

  ASSERT_TRUE(IsOk(updateR(keyed("k", 2))));

  // A stale entry (keyed on value 1) would make this return two.
  auto entries = scanIndex(*index);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries.front(), lookupKey("k").first);
}

TEST_P(StorageEngineIndexTypeTest, RemoveDeletesIndexEntry) {
  auto index = makeIndex(indexDefinition(GetParam()));
  ASSERT_TRUE(IsOk(insertR(keyed("a", 1))));
  ASSERT_TRUE(IsOk(insertR(keyed("b", 2))));
  ASSERT_EQ(scanIndex(*index).size(), 2u);

  ASSERT_TRUE(IsOk(remove(keyOnly("a"))));

  auto entries = scanIndex(*index);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries.front(), lookupKey("b").first);
}

TEST_P(StorageEngineIndexTypeTest, ReturnsEntriesInAscendingOrder) {
  auto index = makeIndex(indexDefinition(GetParam()));
  ASSERT_TRUE(IsOk(insertR(keyed("a", 3))));
  ASSERT_TRUE(IsOk(insertR(keyed("b", 1))));
  ASSERT_TRUE(IsOk(insertR(keyed("c", 2))));

  std::vector<LocalDocumentId> expected{
      lookupKey("b").first, lookupKey("c").first, lookupKey("a").first};
  EXPECT_EQ(scanIndex(*index), expected);
}

INSTANTIATE_TEST_CASE_P(VPackIndexAliases, StorageEngineIndexTypeTest,
                        ::testing::Values("persistent", "hash", "skiplist"));

// RocksDBTtlIndex::insert converts the attribute to a timestamp, so a
// document is only indexed if that conversion succeeds.

namespace {
VPackString ttlIndexDefinition() {
  return R"({"type":"ttl","fields":["value"],"expireAfter":3600,)"
         R"("unique":false,"sparse":true})"_vpack;
}
}  // namespace

TEST_F(StorageEngineIndexTest, TtlIndexIndexesDocumentsWithNumericExpireValue) {
  auto index = makeIndex(ttlIndexDefinition());
  ASSERT_TRUE(IsOk(insertR(keyed("a", 1))));

  auto entries = scanIndex(*index);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries.front(), lookupKey("a").first);
}

TEST_F(StorageEngineIndexTest,
       TtlIndexExcludesDocumentsMissingExpireAttribute) {
  auto index = makeIndex(ttlIndexDefinition());
  ASSERT_TRUE(IsOk(insertR(keyOnly("a"))));

  EXPECT_TRUE(scanIndex(*index).empty());
}

TEST_F(StorageEngineIndexTest, TtlIndexReturnsEntriesInAscendingOrder) {
  auto index = makeIndex(ttlIndexDefinition());
  ASSERT_TRUE(IsOk(insertR(keyed("a", 3))));
  ASSERT_TRUE(IsOk(insertR(keyed("b", 1))));
  ASSERT_TRUE(IsOk(insertR(keyed("c", 2))));

  std::vector<LocalDocumentId> expected{
      lookupKey("b").first, lookupKey("c").first, lookupKey("a").first};
  EXPECT_EQ(scanIndex(*index), expected);
}

TEST_F(StorageEngineIndexTest, TtlIndexUpdateReplacesIndexEntry) {
  auto index = makeIndex(ttlIndexDefinition());
  ASSERT_TRUE(IsOk(insertR(keyed("k", 1))));

  ASSERT_TRUE(IsOk(updateR(keyed("k", 2))));

  // A stale entry (keyed on the old timestamp) would make this return two.
  auto entries = scanIndex(*index);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries.front(), lookupKey("k").first);
}

TEST_F(StorageEngineIndexTest, TtlIndexRemoveDeletesIndexEntry) {
  auto index = makeIndex(ttlIndexDefinition());
  ASSERT_TRUE(IsOk(insertR(keyed("a", 1))));
  ASSERT_TRUE(IsOk(insertR(keyed("b", 2))));
  ASSERT_EQ(scanIndex(*index).size(), 2u);

  ASSERT_TRUE(IsOk(remove(keyOnly("a"))));

  auto entries = scanIndex(*index);
  ASSERT_EQ(entries.size(), 1u);
  EXPECT_EQ(entries.front(), lookupKey("b").first);
}
