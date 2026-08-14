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

#include "Basics/StaticStrings.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

#include <string>

using namespace arangodb;
using namespace arangodb::tests;

TEST_F(StorageEngineDataTest, CreatedCollectionIsListedInInventory) {
  auto database = makeDatabase("testDatabase", 42);
  auto collection = makeCollection(*database, "testCollection");

  VPackBuilder builder;
  auto err = engine().getCollectionsAndIndexes(*database, builder,
                                               /*wasCleanShutdown*/ true,
                                               /*isUpgrade*/ false);
  ASSERT_EQ(err, TRI_ERROR_NO_ERROR);

  auto slice = builder.slice();
  ASSERT_TRUE(slice.isArray());

  bool found = false;
  for (auto c : VPackArrayIterator(slice)) {
    if (c.get(StaticStrings::DataSourceName).stringView() == "testCollection") {
      found = true;
      EXPECT_EQ(c.get(StaticStrings::DataSourceId).stringView(),
                std::to_string(collection->id().id()));
    }
  }
  EXPECT_TRUE(found)
      << "created collection not reported by getCollectionsAndIndexes()";
}
