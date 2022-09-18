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
#include "Cluster/Utils/PlanCollectionToAgencyWriter.h"
#include "VocBase/Properties/PlanCollection.h"

#include "Logger/LogMacros.h"
#include "Inspection/VPack.h"

#include <velocypack/Builder.h>

namespace arangodb::tests {

class PlanCollectionToAgencyWriterTest : public ::testing::Test {
 protected:
  std::string const& dbName() {
    return databaseName;
  }

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

TEST_F(PlanCollectionToAgencyWriterTest, can_produce_agency_callback) {}

}
