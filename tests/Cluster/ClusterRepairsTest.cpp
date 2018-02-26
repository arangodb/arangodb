////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "catch.hpp"

#include "Cluster/ClusterRepairs.h"

#include "Agency/AddFollower.h"
#include "Agency/FailedLeader.h"
#include "Agency/MoveShard.h"
#include "lib/Random/RandomGenerator.h"
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include <velocypack/Compare.h>

#include <boost/range/combine.hpp>

#include <iostream>
#include <memory>
#include <sstream>


using namespace arangodb;
using namespace arangodb::consensus;
using namespace arangodb::cluster_repairs;

namespace arangodb {

bool operator==(AgencyWriteTransaction const& left, AgencyWriteTransaction const& right) {
  VPackBuilder leftBuilder;
  VPackBuilder rightBuilder;

  left.toVelocyPack(leftBuilder);
  right.toVelocyPack(rightBuilder);

  return velocypack::NormalizedCompare::equals(
    leftBuilder.slice(),
    rightBuilder.slice()
  );
}

std::ostream& operator<<(std::ostream& ostream, AgencyWriteTransaction const& trx) {
  Options optPretty = VPackOptions::Defaults;
  optPretty.prettyPrint = true;

  VPackBuilder builder;

  trx.toVelocyPack(builder);

  ostream << builder.slice().toJson(&optPretty);

  return ostream;
}

namespace cluster_repairs {
bool operator==(MoveShardOperation const &left, MoveShardOperation const &other) {
  return
    left.database == other.database
    && left.collection == other.collection
    && left.shard == other.shard
    && left.from == other.from
    && left.isLeader == other.isLeader;
}

std::ostream& operator<<(std::ostream& ostream, MoveShardOperation const& operation) {
  ostream << "MoveShardOperation" << std::endl
          << "{ database: " << operation.database << std::endl
          << ", collection: " << operation.collection << std::endl
          << ", shard: " << operation.shard << std::endl
          << ", from: " << operation.from << std::endl
          << ", to: " << operation.to << std::endl
          << ", isLeader: " << operation.isLeader << std::endl
          << "}";

  return ostream;
}
}

namespace tests {
namespace cluster_repairs_test {

std::shared_ptr<VPackBuffer<uint8_t>>
vpackFromJsonString(char const *c) {
  Options options;
  options.checkAttributeUniqueness = true;
  VPackParser parser(&options);
  parser.parse(c);

  std::shared_ptr<VPackBuilder> builder = parser.steal();

  return builder->steal();
}

std::shared_ptr<VPackBuffer<uint8_t>>
operator "" _vpack(const char* json, size_t) {
  return vpackFromJsonString(json);
}

SCENARIO("Broken distributeShardsLike collections", "[cluster][shards][repairs][!throws]") {

  #include "ClusterRepairsTest.TestData.cpp"

  // save old manager (may be null)
  std::unique_ptr<AgencyCommManager> old_manager = std::move(AgencyCommManager::MANAGER);

  try {
    // get a new manager
    AgencyCommManager::initialize("testArangoAgencyPrefix");

    GIVEN("An agency where on two shards the DBServers are swapped") {

      DistributeShardsLikeRepairer repairer;

      WHEN("One unused DBServer is free to exchange the leader") {

        std::list<RepairOperation> repairOperations
          = repairer.repairDistributeShardsLike(
            VPackSlice(planCollections->data()),
            VPackSlice(supervisionHealth3Healthy0Bad->data())
          );

        // TODO there are more values that might be needed in the preconditions,
        // like distributeShardsLike / repairingDistributeShardsLike,
        // waitForSync, or maybe replicationFactor

        std::vector<RepairOperation> const &expectedRepairOperations
          = expectedOperationsWithTwoSwappedDBServers;

        {

          std::stringstream expectedOperationsStringStream;
          expectedOperationsStringStream << "[" << std::endl;
          for(auto const& it : expectedRepairOperations) {
            expectedOperationsStringStream << it << "," << std::endl;
          }
          expectedOperationsStringStream << "]";

          std::stringstream repairOperationsStringStream;
          repairOperationsStringStream << "[" << std::endl;
          for(auto const& it : expectedRepairOperations) {
            repairOperationsStringStream << it << "," << std::endl;
          }
          repairOperationsStringStream << "]";

          INFO("Expected transactions are:\n" << expectedOperationsStringStream.str());
          INFO("Actual transactions are:\n" << repairOperationsStringStream.str());

          REQUIRE(repairOperations.size() == expectedRepairOperations.size());
        }

        for (auto const &it : boost::combine(repairOperations, expectedRepairOperations)) {
          auto const &repairOpIt = it.get<0>();
          auto const &expectedRepairOpIt = it.get<1>();

           REQUIRE(repairOpIt == expectedRepairOpIt);
        }
      }

      WHEN("The unused DBServer is marked as non-healthy") {
        REQUIRE_THROWS_WITH(
          repairer.repairDistributeShardsLike(
            VPackSlice(planCollections->data()),
            VPackSlice(supervisionHealth2Healthy1Bad->data())
          ),
          "not enough healthy db servers"
        );
      }
    }

    GIVEN("An agency where the replicationFactor equals the number of DBServers") {
      // TODO this should reduce the replicationFactor for the leader-move
    }

  } catch (...) {
    // restore old manager
    AgencyCommManager::MANAGER = std::move(old_manager);
    throw;
  }
  // restore old manager
  AgencyCommManager::MANAGER = std::move(old_manager);
}

}
}
}
