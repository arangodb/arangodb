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
#include "RocksDBEngine/RocksDBMetaCollection.h"
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
  EXPECT_TRUE(collection->properties().mutableProps.cacheEnabled);

  UserInputCollectionProperties props = collection->getCollectionProperties();
  EXPECT_EQ(props.name, "books");
  EXPECT_TRUE(props.waitForSync);
  EXPECT_FALSE(props.cacheEnabled);
  EXPECT_FALSE(props.supportsRBAC);
}

TEST_F(StorageEngineDataTest, LogicalCollection_acceptsInternalOnlyValues) {
  auto database = makeDatabase("testDatabase", 42);

  // A SmartGraph edge collection in a cluster is stored with
  // numberOfShards: 0. The create API rejects that value, so the invariant
  // must not run when loading a marker or a plan entry.
  VPackBuilder builder;
  {
    VPackObjectBuilder obj(&builder);
    builder.add(StaticStrings::DataSourceName, VPackValue("edges"));
    builder.add(StaticStrings::DataSourceType,
                VPackValue(static_cast<int>(TRI_COL_TYPE_EDGE)));
    builder.add(StaticStrings::NumberOfShards, VPackValue(0));
    // markers store the id as a number, not a string
    builder.add(StaticStrings::Id, VPackValue(9988488));
  }

  std::shared_ptr<LogicalCollection> collection;
  ASSERT_NO_THROW(collection = database->createCollection(builder.slice()));
  ASSERT_NE(collection, nullptr);
  EXPECT_EQ(collection->name(), "edges");
  EXPECT_EQ(collection->type(), TRI_COL_TYPE_EDGE);
}

TEST_F(StorageEngineDataTest, CollectionDescriptor_roundTripIsStable) {
  auto sliceBuilder = representativeCreateSlice();

  CollectionDescriptor first;
  auto status = velocypack::deserializeWithStatus(
      sliceBuilder.slice(), first,
      {.ignoreUnknownFields = true, .ignoreInvariants = true},
      InspectInternalContext{});
  ASSERT_TRUE(status.ok()) << status.error();

  VPackBuilder out;
  velocypack::serializeWithContext(out, first, InspectInternalContext{});

  CollectionDescriptor second;
  status = velocypack::deserializeWithStatus(
      out.slice(), second,
      {.ignoreUnknownFields = true, .ignoreInvariants = true},
      InspectInternalContext{});
  ASSERT_TRUE(status.ok()) << status.error();

  EXPECT_EQ(first, second) << out.slice().toJson();
}

TEST_F(StorageEngineDataTest, CollectionDescriptor_holdsObjectId) {
  auto database = makeDatabase("testDatabase", 42);
  auto sliceBuilder = representativeCreateSlice();
  auto collection = database->createCollection(sliceBuilder.slice());
  engine().createCollection(*database, *collection);

  // createCollectionObjectForStorage merges in an engine-assigned objectId
  EXPECT_NE(collection->properties().storage.objectId, 0u);
  EXPECT_EQ(collection->properties().storage.objectId,
            static_cast<RocksDBMetaCollection*>(collection->getPhysical())
                ->objectId());
}
