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

#include "RocksDBEngine/StorageEngineDataTest.h"

#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <string>

using namespace arangodb;
using namespace arangodb::tests;

// Static member definition for StorageEngineDataTest
std::shared_ptr<transaction::Manager>
    StorageEngineDataTest::_transactionManager;

TEST_F(StorageEngineDataTest, CreatedCollectionIsListedInInventory) {
  auto database = makeDatabase("testDatabase", 42);
  auto collection = makeCollection(*database, "testCollection");
  auto descriptors = engine().getCollectionsAndIndexes(*database);

  bool found = false;
  for (auto const& d : descriptors) {
    if (d.mutableProps.name == "testCollection") {
      found = true;
      EXPECT_EQ(d.identity.id, collection->id());
    }
  }
  EXPECT_TRUE(found)
      << "created collection not reported by getCollectionsAndIndexes()";
}

// A dropped collection's marker stays on disk with deleted: true until
// compaction removes it. Recovery must skip those, or the collection comes
// back after a restart.
TEST_F(StorageEngineDataTest, DeletedCollectionIsNotListedInInventory) {
  auto database = makeDatabase("deletedTestDatabase", 43);
  auto collection = makeCollection(*database, "deletedCollection");

  collection->setDeleted();
  engine().changeCollection(*database, *collection);

  for (auto const& d : engine().getCollectionsAndIndexes(*database)) {
    EXPECT_NE(d.mutableProps.name, "deletedCollection")
        << "deleted collection reported by getCollectionsAndIndexes()";
  }
}
