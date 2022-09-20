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
#include "Cluster/Utils/PlanCollectionToAgencyWriter.h"
#include "VocBase/Properties/PlanCollection.h"

#include "Logger/LogMacros.h"
#include "Inspection/VPack.h"

#include <velocypack/Builder.h>

namespace arangodb::tests {

namespace {
auto extractCollectionEntry(VPackSlice agencyOperation, std::string const& key)
    -> VPackSlice {
  return agencyOperation.get(std::vector<std::string>{key, "new"});
}
}  // namespace

class PlanCollectionToAgencyWriterTest : public ::testing::Test {
 protected:
  std::string const& dbName() { return databaseName; }

  std::string collectionPlanPath(PlanCollection const& col) {
    return cluster::paths::root()
        ->arango()
        ->plan()
        ->collections()
        ->database(dbName())
        ->collection(col.internalProperties.id)
        ->str();
  }

  void hasIsBuildingFlag(AgencyOperation const& operation) {
    VPackBuilder b;
    {
      VPackObjectBuilder bodyBuilder{&b};
      operation.toVelocyPack(b);
    }
    auto isBuilding = b.slice().get(std::vector<std::string>{operation.key(), "new", "isBuilding"});
    ASSERT_TRUE(isBuilding.isBoolean());
    EXPECT_TRUE(isBuilding.getBoolean());
  }

 private:
  // On purpose private and accisbale via above methods to be easily
  // exchangable without rewriting the tests.
  std::string databaseName = "testDB";
};

TEST_F(PlanCollectionToAgencyWriterTest, can_produce_agency_precondition) {}

TEST_F(PlanCollectionToAgencyWriterTest, can_produce_agency_operation) {
  PlanCollection col{};
  col.mutableProperties.name = "test";
  PlanCollectionToAgencyWriter writer{col};
  AgencyOperation operation = writer.prepareOperation(dbName());
  // We have a write operation
  ASSERT_EQ(operation.type().type, AgencyOperationType::Type::VALUE);
  EXPECT_EQ(operation.type().value, AgencyValueOperationType::SET);
  EXPECT_EQ(operation.key(), collectionPlanPath(col));
  hasIsBuildingFlag(operation);

  VPackBuilder b;
  {
    VPackObjectBuilder bodyBuilder{&b};
    operation.toVelocyPack(b);
  }
  LOG_DEVEL << b.toJson();
}

/**
 * SECTION - Basic tests to make sure at least one property per component is
 * serialized
 */

TEST_F(PlanCollectionToAgencyWriterTest,
       default_agency_operation_has_a_constant_property_entry) {
  PlanCollection col{};
  col.mutableProperties.name = "test";
  PlanCollectionToAgencyWriter writer{col};
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
  PlanCollectionToAgencyWriter writer{col};
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
  PlanCollectionToAgencyWriter writer{col};
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
  PlanCollectionToAgencyWriter writer{col};
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
  ASSERT_TRUE(false) << "Test is not implemented completely";
  // TODO: Need to assert some values inside the shards.
}

/**
 * SECTION - Some selected tests that are not straight-forward with inspect
 */
TEST_F(PlanCollectionToAgencyWriterTest,
       default_agency_operation_has_no_distributeShardsLike) {
  PlanCollection col{};
  col.mutableProperties.name = "test";
  PlanCollectionToAgencyWriter writer{col};
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
  PlanCollectionToAgencyWriter writer{col};
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
  PlanCollectionToAgencyWriter writer{col};
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
  PlanCollectionToAgencyWriter writer{col};
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
            "test");
  EXPECT_EQ(basics::VelocyPackHelper::getNumericValue<TRI_vocbase_col_status_e>(
                entry, "status", TRI_VOC_COL_STATUS_CORRUPTED),
            TRI_VOC_COL_STATUS_LOADED);
}

TEST_F(PlanCollectionToAgencyWriterTest, can_produce_agency_callback) {}

}  // namespace arangodb::tests
