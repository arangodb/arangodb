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
///
////////////////////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>

#include "RocksDBEngine/StorageEngineDataTest.h"

#include "Basics/StaticStrings.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Properties/UserInputCollectionProperties.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>

using namespace arangodb;
using namespace arangodb::tests;

namespace {

VPackBuilder representativeCreateSlice() {
  VPackBuilder builder;
  builder.openObject();
  builder.add(StaticStrings::DataSourceName, VPackValue("books"));
  builder.add(StaticStrings::DataSourceType,
              VPackValue(static_cast<int>(TRI_COL_TYPE_DOCUMENT)));
  builder.add(StaticStrings::WaitForSyncString, VPackValue(true));
  builder.add(StaticStrings::CacheEnabled, VPackValue(true));
  builder.add(StaticStrings::SupportsRBAC, VPackValue(false));
  {
    VPackObjectBuilder keyOpts(&builder, StaticStrings::KeyOptions);
    builder.add(StaticStrings::AllowUserKeys, VPackValue(false));
    builder.add("type", VPackValue("traditional"));
  }
  builder.close();
  return builder;
}

}  // namespace

TEST_F(StorageEngineDataTest,
       LogicalCollection_accessorsMatchRepresentativeCreateSlice) {
  auto database = makeDatabase("testDatabase", 42);
  auto sliceBuilder = representativeCreateSlice();
  auto collection = database->createCollection(sliceBuilder.slice());
  engine().createCollection(*database, *collection);

  EXPECT_EQ(collection->name(), "books");
  EXPECT_EQ(collection->type(), TRI_COL_TYPE_DOCUMENT);
  EXPECT_TRUE(collection->waitForSync());
  EXPECT_FALSE(collection->cacheEnabled());
  EXPECT_FALSE(collection->supportsRBAC());

  UserInputCollectionProperties props = collection->getCollectionProperties();
  EXPECT_EQ(props.name, "books");
  EXPECT_TRUE(props.waitForSync);
  EXPECT_FALSE(props.cacheEnabled);
  EXPECT_FALSE(props.supportsRBAC);
}