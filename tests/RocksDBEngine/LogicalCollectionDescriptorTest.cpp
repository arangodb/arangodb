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
#include "Inspection/VPack.h"
#include "VocBase/KeyGenerator.h"
#include "VocBase/LogicalCollection.h"
#include "VocBase/Properties/ClusteringConstantProperties.h"
#include "VocBase/Properties/CollectionDescriptor.h"
#include "VocBase/Properties/CreateCollectionRequest.h"
#include "VocBase/Properties/DatabaseConfiguration.h"
#include "VocBase/Properties/KeyGeneratorProperties.h"
#include "VocBase/voc-types.h"
#include "VocBase/vocbase.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>

#include <set>

// Tests for `CollectionDescriptor` and LogicalCollection ctor taking one:
// Section 1: descriptor ctor creates a collection correctly
// Section 2: create path and load path both work correctly
// Section 3: serializing a collection back to velocypack works correctly
// Section 4: validation differs correctly between contexts
//
// Single-server fixture: cluster branches are covered by the JS suites.

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
  {
    VPackObjectBuilder keyOpts(&builder, StaticStrings::KeyOptions);
    builder.add(StaticStrings::AllowUserKeys, VPackValue(false));
    builder.add("type", VPackValue("traditional"));
  }
  builder.close();
  return builder;
}

CollectionDescriptor representativeCreateDescriptor() {
  CollectionDescriptor d;
  d.identity.id = DataSourceId{9988488};
  d.mutableProps.name = "books";
  d.constant.type = TRI_COL_TYPE_DOCUMENT;
  d.constant.keyOptions =
      TraditionalKeyGeneratorProperties{.allowUserKeys = false};
  d.mutableProps.cacheEnabled = true;
  d.clusteringMutable.waitForSync = true;
  d.clusteringMutable.replicationFactor = 1;
  d.clusteringMutable.writeConcern = 1;
  d.clusteringConstant.numberOfShards = 1;
  d.clusteringConstant.shardKeys = std::vector<std::string>{"_key"};
  d.clusteringConstant.shardingStrategy = "hash";
  return d;
}

// Identity and index keys are assigned per collection, so they can neither be
// compared between two collections nor pinned.
std::unordered_set<std::string> const& volatileKeys() {
  static std::unordered_set<std::string> const keys{
      StaticStrings::ObjectId,         StaticStrings::DataSourceGuid,
      StaticStrings::DataSourceId,     StaticStrings::DataSourceCid,
      StaticStrings::DataSourcePlanId, StaticStrings::Indexes};
  return keys;
}

VPackBuilder oneKeyObject(std::string_view key, VPackValue value) {
  VPackBuilder builder;
  {
    VPackObjectBuilder guard(&builder);
    builder.add(key, value);
  }
  return builder;
}

}  // namespace

// StorageEngineDataTest under this file's name, so every test here reports
// under one suite.
class LogicalCollectionDescriptorTest : public StorageEngineDataTest {};

//////////////////////////////////////////////////////////////////////////////////
// Section 1: descriptor ctor creates a collection correctly
//////////////////////////////////////////////////////////////////////////////////

TEST_F(LogicalCollectionDescriptorTest, DescriptorCtor_appliesDescriptor) {
  auto database = makeDatabase("testDatabase", 42);
  auto collection =
      database->createCollection(representativeCreateDescriptor());
  engine().createCollection(*database, *collection);

  EXPECT_EQ(collection->id(), DataSourceId{9988488});
  EXPECT_EQ(collection->name(), "books");
  EXPECT_EQ(collection->type(), TRI_COL_TYPE_DOCUMENT);
  EXPECT_FALSE(collection->system());
  EXPECT_TRUE(collection->waitForSync());
  EXPECT_FALSE(collection->allowUserKeys());
}

TEST_F(LogicalCollectionDescriptorTest,
       DescriptorCtor_buildsKeyGeneratorFromKeyOptions) {
  auto database = makeDatabase("testDatabase", 42);
  auto descriptor = representativeCreateDescriptor();
  descriptor.constant.keyOptions =
      PaddedKeyGeneratorProperties{.allowUserKeys = true};
  auto collection = database->createCollection(std::move(descriptor));

  VPackBuilder out;
  {
    VPackObjectBuilder guard(&out);
    collection->keyGenerator().toVelocyPack(out);
  }

  EXPECT_EQ(out.slice().get("type").copyString(), "padded");
  EXPECT_TRUE(collection->allowUserKeys());
}

TEST_F(LogicalCollectionDescriptorTest, DescriptorCtor_appliesSharding) {
  auto database = makeDatabase("testDatabase", 42);
  auto descriptor = representativeCreateDescriptor();
  descriptor.clusteringConstant.numberOfShards = 3;
  descriptor.clusteringMutable.replicationFactor = 2;
  descriptor.clusteringMutable.writeConcern = 2;
  auto collection = database->createCollection(std::move(descriptor));

  EXPECT_EQ(collection->numberOfShards(), 3u);
  EXPECT_EQ(collection->replicationFactor(), 2u);
  EXPECT_EQ(collection->writeConcern(), 2u);
  EXPECT_EQ(collection->shardKeys(), std::vector<std::string>({"_key"}));
}

TEST_F(LogicalCollectionDescriptorTest,
       DescriptorCtor_keepsRequestedCacheEnabled) {
  auto database = makeDatabase("testDatabase", 42);
  auto collection =
      database->createCollection(representativeCreateDescriptor());

  // The descriptor keeps what was requested, the physical collection reports
  // what is in effect. They differ here because the fixture has no cache
  // manager. Reporting the requested value from cacheEnabled() was a bug.
  EXPECT_EQ(collection->properties().mutableProps.cacheEnabled,
            collection->cacheEnabled());
  EXPECT_FALSE(collection->cacheEnabled());
}

TEST_F(LogicalCollectionDescriptorTest,
       DescriptorCtor_receivesEngineAssignedObjectId) {
  auto database = makeDatabase("testDatabase", 42);
  auto collection =
      database->createCollection(representativeCreateDescriptor());
  engine().createCollection(*database, *collection);

  // createCollectionObject asks the engine to fill in the storage properties
  EXPECT_NE(collection->properties().storage.objectId, 0u);
  EXPECT_EQ(collection->properties().storage.objectId,
            static_cast<RocksDBMetaCollection*>(collection->getPhysical())
                ->objectId());
}

TEST_F(LogicalCollectionDescriptorTest, Properties_isALiveSnapshot) {
  auto database = makeDatabase("testDatabase", 42);
  auto collection =
      database->createCollection(representativeCreateDescriptor());

  auto d = collection->properties();
  // the stored descriptor has no id on the load path; properties() fills it in
  EXPECT_EQ(d.identity.id, collection->id());
  EXPECT_EQ(d.mutableProps.name, collection->name());
  EXPECT_EQ(d.clusteringConstant.numberOfShards, collection->numberOfShards());
  EXPECT_EQ(d.clusteringConstant.shardKeys, collection->shardKeys());
  EXPECT_EQ(d.clusteringMutable.replicationFactor,
            collection->replicationFactor());
  EXPECT_EQ(d.clusteringMutable.writeConcern, collection->writeConcern());
}

//////////////////////////////////////////////////////////////////////////////////
// Section 2: create path and load path both work correctly
//////////////////////////////////////////////////////////////////////////////////

TEST_F(LogicalCollectionDescriptorTest, LoadPath_acceptsInternalOnlyValues) {
  auto database = makeDatabase("testDatabase", 42);

  // A SmartGraph edge collection in a cluster is stored with numberOfShards: 0,
  // and a database upgrade stores the "upgrade" key generator. The create API
  // rejects both, so the invariants must not run when loading a marker or a
  // plan entry.
  VPackBuilder builder;
  {
    VPackObjectBuilder obj(&builder);
    builder.add(StaticStrings::DataSourceName, VPackValue("edges"));
    builder.add(StaticStrings::DataSourceType,
                VPackValue(static_cast<int>(TRI_COL_TYPE_EDGE)));
    builder.add(StaticStrings::NumberOfShards, VPackValue(0));
    // markers store the id as a number, not a string
    builder.add(StaticStrings::Id, VPackValue(9988488));
    {
      VPackObjectBuilder keyOpts(&builder, StaticStrings::KeyOptions);
      builder.add("type", VPackValue("upgrade"));
    }
  }

  std::shared_ptr<LogicalCollection> collection;
  ASSERT_NO_THROW(collection = database->createCollection(builder.slice()));
  ASSERT_NE(collection, nullptr);
  EXPECT_EQ(collection->name(), "edges");
  EXPECT_EQ(collection->type(), TRI_COL_TYPE_EDGE);
}

//////////////////////////////////////////////////////////////////////////////////
// Section 3: serializing a collection back to velocypack works correctly
//////////////////////////////////////////////////////////////////////////////////

TEST_F(LogicalCollectionDescriptorTest,
       Serialization_emitsEveryKeyExactlyOnce) {
  auto database = makeDatabase("testDatabase", 42);
  auto collection =
      database->createCollection(representativeCreateDescriptor());
  engine().createCollection(*database, *collection);

  auto out = collection->toVelocyPackIgnore(
      volatileKeys(), LogicalDataSource::Serialization::Persistence);

  std::set<std::string> keys;
  for (auto it : VPackObjectIterator(out.slice())) {
    EXPECT_TRUE(keys.insert(it.key.copyString()).second)
        << "duplicate key " << it.key.stringView();
  }

  for (auto const& key :
       {StaticStrings::DataSourceName, StaticStrings::KeyOptions,
        StaticStrings::CacheEnabled, StaticStrings::NumberOfShards,
        StaticStrings::ShardKeys, StaticStrings::ReplicationFactor,
        StaticStrings::WriteConcern}) {
    EXPECT_TRUE(keys.contains(key)) << "missing key " << key;
  }

  EXPECT_EQ(out.slice().get(StaticStrings::DataSourceName).copyString(),
            "books");
  EXPECT_TRUE(out.slice().get(StaticStrings::WaitForSyncString).getBool());
  EXPECT_FALSE(out.slice()
                   .get(StaticStrings::KeyOptions)
                   .get(StaticStrings::AllowUserKeys)
                   .getBool());
}

TEST_F(LogicalCollectionDescriptorTest,
       Serialization_descriptorRoundTripIsStable) {
  auto sliceBuilder = representativeCreateSlice();

  CollectionDescriptor first;
  auto status = velocypack::deserializeWithStatus(sliceBuilder.slice(), first,
                                                  {.ignoreUnknownFields = true},
                                                  InspectInternalContext{});
  ASSERT_TRUE(status.ok()) << status.error();

  VPackBuilder out;
  velocypack::serializeWithContext(out, first, InspectInternalContext{});

  CollectionDescriptor second;
  status = velocypack::deserializeWithStatus(out.slice(), second,
                                             {.ignoreUnknownFields = true},
                                             InspectInternalContext{});
  ASSERT_TRUE(status.ok()) << status.error();

  EXPECT_EQ(first, second) << out.slice().toJson();
}

//////////////////////////////////////////////////////////////////////////////////
// Section 4: validation differs correctly between contexts
//////////////////////////////////////////////////////////////////////////////////

TEST_F(LogicalCollectionDescriptorTest,
       Context_numberOfShardsZeroIsInternalOnly) {
  auto body = oneKeyObject(StaticStrings::NumberOfShards, VPackValue(0));

  ClusteringConstantProperties internalProps;
  EXPECT_TRUE(velocypack::deserializeWithStatus(body.slice(), internalProps, {},
                                                InspectInternalContext{})
                  .ok());
  EXPECT_EQ(internalProps.numberOfShards, 0u);

  ClusteringConstantProperties userProps;
  EXPECT_FALSE(velocypack::deserializeWithStatus(body.slice(), userProps, {},
                                                 InspectUserContext{})
                   .ok());
}

TEST_F(LogicalCollectionDescriptorTest,
       Context_upgradeKeyGeneratorIsInternalOnly) {
  auto body = oneKeyObject("type", VPackValue("upgrade"));

  KeyGeneratorProperties internalProps;
  EXPECT_TRUE(velocypack::deserializeWithStatus(body.slice(), internalProps, {},
                                                InspectInternalContext{})
                  .ok());
  EXPECT_TRUE(
      std::holds_alternative<UpgradeKeyGeneratorProperties>(internalProps));

  KeyGeneratorProperties userProps;
  EXPECT_FALSE(velocypack::deserializeWithStatus(body.slice(), userProps, {},
                                                 InspectUserContext{})
                   .ok());
}

// objectId is user-rejected, internal-accepted.
TEST_F(LogicalCollectionDescriptorTest, Context_objectIdIsInternalOnly) {
  auto body = oneKeyObject(StaticStrings::ObjectId, VPackValue("1234"));

  CollectionDescriptor internalProps;
  EXPECT_TRUE(velocypack::deserializeWithStatus(body.slice(), internalProps, {},
                                                InspectInternalContext{})
                  .ok());
  EXPECT_EQ(internalProps.storage.objectId, 1234u);

  CollectionDescriptor userProps;
  EXPECT_FALSE(velocypack::deserializeWithStatus(body.slice(), userProps, {},
                                                 InspectUserContext{})
                   .ok());
}

// The slice ctor goes away in COR-885. Until then, one test keeps it honest:
// the same input through either ctor must produce the same collection. Delete
// this block together with the ctor.

TEST_F(LogicalCollectionDescriptorTest, SliceCtor_matchesDescriptorCtor) {
  VPackBuilder sliceBuilder;
  sliceBuilder.openObject();
  sliceBuilder.add(VPackObjectIterator(representativeCreateSlice().slice()));
  // Collections::create injects this for every user create on a single server
  // or coordinator; without it the two paths legitimately disagree
  sliceBuilder.add(StaticStrings::UsesRevisionsAsDocumentIds, VPackValue(true));
  sliceBuilder.close();

  // baseline: the existing slice path
  auto viaSlice = makeDatabase("viaSlice", 42);
  auto expected = viaSlice->createCollection(sliceBuilder.slice());
  engine().createCollection(*viaSlice, *expected);

  // same input, taken through the descriptor factory
  DatabaseConfiguration config{
      []() { return DataSourceId(42); },
      [](std::string const&) -> ResultT<CollectionDescriptor> {
        return {TRI_ERROR_INTERNAL};
      }};
  auto request = CreateCollectionRequest::fromCreateAPIBody(
      sliceBuilder.slice(), config, /*backwardsCompatibility*/ false);
  ASSERT_TRUE(request.ok()) << request.errorMessage();

  auto viaDescriptor = makeDatabase("viaDescriptor", 43);
  auto actual = viaDescriptor->createCollection(std::move(request->descriptor));
  engine().createCollection(*viaDescriptor, *actual);

  EXPECT_EQ(
      actual
          ->toVelocyPackIgnore(volatileKeys(),
                               LogicalDataSource::Serialization::Persistence)
          .slice()
          .toJson(),
      expected
          ->toVelocyPackIgnore(volatileKeys(),
                               LogicalDataSource::Serialization::Persistence)
          .slice()
          .toJson());
}

TEST_F(LogicalCollectionDescriptorTest,
       SliceCtor_usesRevisionsAsDocumentIdsDefaults) {
  // The descriptor defaults to true, matching what Collections::create injects
  // for a user create. The slice ctor defaults to false, which is what the
  // agency's own collections and pre-v37 markers rely on.
  CollectionDescriptor d;
  EXPECT_TRUE(d.internal.usesRevisionsAsDocumentIds);

  auto database = makeDatabase("testDatabase", 42);
  auto collection =
      database->createCollection(representativeCreateSlice().slice());
  EXPECT_FALSE(collection->usesRevisionsAsDocumentIds());
}
