////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2022-2022 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "Agency/AgencyComm.h"
#include "Agency/AgencyPaths.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/Utils/IShardDistributionFactory.h"
#include "Cluster/Utils/EvenDistribution.h"
#include "Cluster/Utils/PlanCollectionEntry.h"
#include "Cluster/Utils/PlanCollectionToAgencyWriter.h"
#include "VocBase/Properties/CreateCollectionBody.h"

#include "Logger/LogMacros.h"
#include "Inspection/VPack.h"
#include "Cluster/Utils/ShardDistribution.h"

#include <velocypack/Builder.h>

namespace arangodb::tests {

namespace {}  // namespace

class PlanCollectionToAgencyWriterTest : public ::testing::Test {
 protected:
  std::string const& dbName() { return databaseName; }

  std::string collectionPlanPath(CreateCollectionBody const& col) {
    return cluster::paths::root()
        ->arango()
        ->plan()
        ->collections()
        ->database(dbName())
        ->collection(std::to_string(col.id.id()))
        ->str();
  }

  void hasIsBuildingFlag(AgencyOperation const& operation) {
    VPackBuilder b;
    {
      VPackObjectBuilder bodyBuilder{&b};
      operation.toVelocyPack(b);
    }
    auto isBuilding = b.slice().get(
        std::vector<std::string>{operation.key(), "new", "isBuilding"});
    ASSERT_TRUE(isBuilding.isBoolean());
    EXPECT_TRUE(isBuilding.getBoolean());
  }

  std::vector<ServerID> generateServerNames(uint64_t numberOfServers) {
    std::vector<ServerID> result;
    result.reserve(numberOfServers);
    for (uint64_t i = 0; i < numberOfServers; ++i) {
      result.emplace_back("PRMR_" + std::to_string(i));
    }
    return result;
  }

  std::vector<ShardID> generateShardNames(uint64_t numberOfShards,
                                          uint64_t idOffset = 0) {
    std::vector<ShardID> result;
    result.reserve(numberOfShards);
    for (uint64_t i = 0; i < numberOfShards; ++i) {
      result.emplace_back("s" + std::to_string(numberOfShards + idOffset));
    }
    return result;
  }

  PlanCollectionToAgencyWriter createWriterWithTestSharding(
      CreateCollectionBody col) {
    auto numberOfShards = col.numberOfShards.value();
    auto distribution = std::make_shared<EvenDistribution>(
        numberOfShards, col.replicationFactor.value(), std::vector<ServerID>{},
        false);
    auto shards = generateShardNames(numberOfShards);

    std::unordered_map<std::string, std::shared_ptr<IShardDistributionFactory>>
        shardDistributionsUsed;
    shardDistributionsUsed.emplace(col.name, distribution);

    ShardDistribution dist{shards, distribution};
    AgencyIsBuildingFlags buildingFlags;
    buildingFlags.rebootId = RebootId{42};
    buildingFlags.coordinatorName = "CRDN_123";
    return PlanCollectionToAgencyWriter{
        {PlanCollectionEntry{std::move(col), std::move(dist),
                             std::move(buildingFlags)}},
        std::move(shardDistributionsUsed)};
  }

  static CreateCollectionBody prepareMinimalCollection(
      uint64_t nrShards, uint64_t replicationFactor) {
    CreateCollectionBody col{};
    col.name = "test";
    col.id = DataSourceId(123);
    col.numberOfShards = nrShards;
    col.replicationFactor = replicationFactor;
    return col;
  }

 private:
  // On purpose private and accisbale via above methods to be easily
  // exchangable without rewriting the tests.
  std::string databaseName = "testDB";
};

TEST_F(PlanCollectionToAgencyWriterTest, can_produce_agency_precondition) {}

TEST_F(PlanCollectionToAgencyWriterTest, can_produce_agency_operation) {
  auto col = prepareMinimalCollection(1, 1);

  auto writer = createWriterWithTestSharding(col);

  auto serversAvailable = generateServerNames(3);
  auto res =
      writer.prepareStartBuildingTransaction(dbName(), 2, serversAvailable);
  ASSERT_TRUE(res.ok());
  auto transaction = res.get();
  LOG_DEVEL << transaction.toJson();
  /*
// We have a write operation
ASSERT_EQ(transaction.type().type, AgencyOperationType::Type::VALUE);
EXPECT_EQ(transaction.type().value, AgencyValueOperationType::SET);
EXPECT_EQ(transaction.key(), collectionPlanPath(col));
hasIsBuildingFlag(transaction);

VPackBuilder b;
{
 VPackObjectBuilder bodyBuilder{&b};
 transaction.toVelocyPack(b);
}
LOG_DEVEL << b.toJson();
   */
}

/**
 * SECTION - Basic tests to make sure at least one property per component is
 * serialized
 */
#if false
TEST_F(PlanCollectionToAgencyWriterTest,
       default_agency_operation_has_a_constant_property_entry) {
  PlanCollection col{};
  col.mutableProperties.name = "test";
  auto writer = createWriterWithTestSharding(col);
  AgencyOperation operation = writer.prepareOperation(dbName());

  VPackBuilder b;
  {
    VPackObjectBuilder bodyBuilder{&b};
    operation.toVelocyPack(b);
  }
  auto entry = extractCollectionEntry(b.slice(), collectionPlanPath(col));
  ASSERT_TRUE(entry.hasKey(StaticStrings::ShardKeys));
  auto keys = entry.get(StaticStrings::ShardKeys);
  ASSERT_TRUE(keys.isArray());
  EXPECT_EQ(keys.length(), 1);
  ASSERT_TRUE(keys.at(0).isString());
  EXPECT_EQ(keys.at(0).copyString(), StaticStrings::KeyString);
}

TEST_F(PlanCollectionToAgencyWriterTest,
       default_agency_operation_has_a_mutable_property_entry) {
  PlanCollection col{};
  col.mutableProperties.name = "test";
  auto writer = createWriterWithTestSharding(col);
  AgencyOperation operation = writer.prepareOperation(dbName());

  VPackBuilder b;
  {
    VPackObjectBuilder bodyBuilder{&b};
    operation.toVelocyPack(b);
  }
  auto entry = extractCollectionEntry(b.slice(), collectionPlanPath(col));
  EXPECT_EQ(basics::VelocyPackHelper::getStringValue(
                entry, StaticStrings::DataSourceName, "notFound"),
            "test");
  EXPECT_EQ(basics::VelocyPackHelper::getBooleanValue(
                entry, StaticStrings::WaitForSyncString, true),
            false);
}

TEST_F(PlanCollectionToAgencyWriterTest, default_agency_operation_has_indexes) {
  PlanCollection col{};
  col.mutableProperties.name = "test";
  auto writer = createWriterWithTestSharding(col);
  AgencyOperation operation = writer.prepareOperation(dbName());

  VPackBuilder b;
  {
    VPackObjectBuilder bodyBuilder{&b};
    operation.toVelocyPack(b);
  }

  auto entry = extractCollectionEntry(b.slice(), collectionPlanPath(col));
  ASSERT_TRUE(entry.hasKey(StaticStrings::Indexes));
  auto indexes = entry.get(StaticStrings::Indexes);
  ASSERT_TRUE(indexes.isArray());
  EXPECT_EQ(indexes.length(), 1);
  auto prim = indexes.at(0);
  ASSERT_TRUE(prim.isObject());
  EXPECT_EQ(basics::VelocyPackHelper::getStringValue(
                prim, StaticStrings::IndexName, "notFound"),
            "primary");
}

TEST_F(PlanCollectionToAgencyWriterTest, default_agency_operation_has_shards) {
  PlanCollection col{};
  col.mutableProperties.name = "test";
  auto writer = createWriterWithTestSharding(col);
  AgencyOperation operation = writer.prepareOperation(dbName());

  VPackBuilder b;
  {
    VPackObjectBuilder bodyBuilder{&b};
    operation.toVelocyPack(b);
  }
  auto entry = extractCollectionEntry(b.slice(), collectionPlanPath(col));
  ASSERT_TRUE(entry.hasKey("shards"));
  auto shards = entry.get("shards");
  ASSERT_TRUE(shards.isObject());
  auto numberOfShards = col.constantProperties.numberOfShards;
  auto replicationFactor = col.mutableProperties.replicationFactor;
  ASSERT_EQ(shards.length(), numberOfShards);
  auto expectedShards = generateShardNames(numberOfShards);
  ASSERT_EQ(expectedShards.size(), numberOfShards);
  for (uint64_t s = 0; s < numberOfShards; ++s) {
    ASSERT_TRUE(shards.hasKey(expectedShards[s]))
        << "Searching for " << expectedShards[s] << " in " << shards.toJson();
    auto list = shards.get(expectedShards[s]);
    ASSERT_TRUE(list.isArray());
    ASSERT_EQ(list.length(), replicationFactor);
    for (uint64_t r = 0; r < replicationFactor; ++r) {
      EXPECT_TRUE(list.at(r).isEqualString(generateServerName(s, r, 0)));
    }
  }
}

/**
 * SECTION - Some selected tests that are not straight-forward with inspect
 */
TEST_F(PlanCollectionToAgencyWriterTest,
       default_agency_operation_has_no_distributeShardsLike) {
  PlanCollection col{};
  col.mutableProperties.name = "test";
  auto writer = createWriterWithTestSharding(col);
  AgencyOperation operation = writer.prepareOperation(dbName());

  VPackBuilder b;
  {
    VPackObjectBuilder bodyBuilder{&b};
    operation.toVelocyPack(b);
  }
  auto entry = extractCollectionEntry(b.slice(), collectionPlanPath(col));
  // Default of Inspect would be to set it to `null`
  EXPECT_FALSE(entry.hasKey(StaticStrings::DistributeShardsLike));
}

TEST_F(PlanCollectionToAgencyWriterTest,
       default_agency_operation_has_no_smartGraphAttribute) {
  PlanCollection col{};
  col.mutableProperties.name = "test";
  auto writer = createWriterWithTestSharding(col);
  AgencyOperation operation = writer.prepareOperation(dbName());

  VPackBuilder b;
  {
    VPackObjectBuilder bodyBuilder{&b};
    operation.toVelocyPack(b);
  }
  auto entry = extractCollectionEntry(b.slice(), collectionPlanPath(col));
  // Default of Inspect would be to set it to `null`
  EXPECT_FALSE(entry.hasKey(StaticStrings::GraphSmartGraphAttribute));
}

TEST_F(PlanCollectionToAgencyWriterTest,
       default_agency_operation_has_no_smartJoinAttribute) {
  PlanCollection col{};
  col.mutableProperties.name = "test";
  auto writer = createWriterWithTestSharding(col);
  AgencyOperation operation = writer.prepareOperation(dbName());

  VPackBuilder b;
  {
    VPackObjectBuilder bodyBuilder{&b};
    operation.toVelocyPack(b);
  }
  auto entry = extractCollectionEntry(b.slice(), collectionPlanPath(col));
  // Default of Inspect would be to set it to `null`
  EXPECT_FALSE(entry.hasKey(StaticStrings::SmartJoinAttribute));
}

TEST_F(PlanCollectionToAgencyWriterTest,
       default_agency_operation_has_status_and_statusString) {
  PlanCollection col{};
  col.mutableProperties.name = "test";
  auto writer = createWriterWithTestSharding(col);
  AgencyOperation operation = writer.prepareOperation(dbName());

  VPackBuilder b;
  {
    VPackObjectBuilder bodyBuilder{&b};
    operation.toVelocyPack(b);
  }
  auto entry = extractCollectionEntry(b.slice(), collectionPlanPath(col));
  // Default of Inspect would be to set it to `null`
  EXPECT_EQ(basics::VelocyPackHelper::getStringValue(entry, "statusString",
                                                     "notFound"),
            "loaded");
  EXPECT_EQ(basics::VelocyPackHelper::getNumericValue<TRI_vocbase_col_status_e>(
                entry, "status", TRI_VOC_COL_STATUS_CORRUPTED),
            TRI_VOC_COL_STATUS_LOADED);
}

TEST_F(PlanCollectionToAgencyWriterTest, can_produce_agency_callback) {}

#endif

}  // namespace arangodb::tests
