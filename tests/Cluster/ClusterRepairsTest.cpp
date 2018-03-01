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

namespace tests {
namespace cluster_repairs_test {

VPackBufferPtr
vpackFromJsonString(char const *c) {
  Options options;
  options.checkAttributeUniqueness = true;
  VPackParser parser(&options);
  parser.parse(c);

  std::shared_ptr<VPackBuilder> builder = parser.steal();

  return builder->steal();
}

VPackBufferPtr
operator "" _vpack(const char* json, size_t) {
  return vpackFromJsonString(json);
}

void checkAgainstExpectedOperations(
  VPackBufferPtr const& planCollections,
  VPackBufferPtr const& supervisionHealth,
  std::vector<RepairOperation> expectedRepairOperations
) {
  DistributeShardsLikeRepairer repairer;

  // TODO there are more values that might be needed in the preconditions,
  // like distributeShardsLike / repairingDistributeShardsLike,
  // or maybe replicationFactor

  std::list<RepairOperation> repairOperations
    = repairer.repairDistributeShardsLike(
      VPackSlice(planCollections->data()),
      VPackSlice(supervisionHealth->data())
    );

  {
    std::stringstream expectedOperationsStringStream;
    expectedOperationsStringStream << "[" << std::endl;
    for(auto const& it : expectedRepairOperations) {
      expectedOperationsStringStream << it << "," << std::endl;
    }
    expectedOperationsStringStream << "]";

    std::stringstream repairOperationsStringStream;
    repairOperationsStringStream << "[" << std::endl;
    for(auto const& it : repairOperations) {
      repairOperationsStringStream << it << "," << std::endl;
    }
    repairOperationsStringStream << "]";

    INFO("Expected transactions are:\n" << expectedOperationsStringStream.str());
    INFO("Actual transactions are:\n" << repairOperationsStringStream.str());

    REQUIRE(repairOperations.size() == expectedRepairOperations.size());
  }

  { // Transaction IDs shall be unique.
    std::set<std::string> transactionClientIds;
    for (auto &it : repairOperations) {
      if (it.type() != typeid(AgencyWriteTransaction)) {
        continue;
      }

      bool inserted;
      AgencyWriteTransaction& transaction = boost::get<AgencyWriteTransaction>(it);
      std::tie(std::ignore, inserted) = transactionClientIds.insert(transaction.clientId);
      REQUIRE(inserted);

      // Overwrite the client ID for the following comparisons to work
      transaction.clientId = "dummy-client-id";
    }

    // Overwrite expected client IDs as well
    for (auto &it : expectedRepairOperations) {
      if (it.type() != typeid(AgencyWriteTransaction)) {
        continue;
      }
      AgencyWriteTransaction& transaction = boost::get<AgencyWriteTransaction>(it);

      transaction.clientId = "dummy-client-id";
    }
  }

  for (auto const &it : boost::combine(repairOperations, expectedRepairOperations)) {
    auto const &repairOpIt = it.get<0>();
    auto const &expectedRepairOpIt = it.get<1>();

    REQUIRE(repairOpIt == expectedRepairOpIt);
  }
}

// TODO Add a test with a smart collections (i.e. with {"isSmart": true, "shards": []})

SCENARIO("Broken distributeShardsLike collections", "[cluster][shards][repairs][!throws]") {

  // save old manager (may be null)
  std::unique_ptr<AgencyCommManager> old_manager = std::move(AgencyCommManager::MANAGER);

  try {
    // get a new manager
    AgencyCommManager::initialize("testArangoAgencyPrefix");

    GIVEN("An agency where on two shards the DBServers are swapped") {

      WHEN("One unused DBServer is free to exchange the leader") {
#include "ClusterRepairsTest.swapWithLeader.cpp"
        checkAgainstExpectedOperations(
          planCollections,
          supervisionHealth3Healthy0Bad,
          expectedOperationsWithTwoSwappedDBServers
        );
      }

      // TODO This should not throw, but return a fail-Result
      WHEN("The unused DBServer is marked as non-healthy") {
#include "ClusterRepairsTest.unusedServerUnhealthy.cpp"

        DistributeShardsLikeRepairer repairer;
        REQUIRE_THROWS_WITH(
          repairer.repairDistributeShardsLike(
            VPackSlice(planCollections->data()),
            VPackSlice(supervisionHealth2Healthy1Bad->data())
          ),
          "not enough healthy db servers"
        );
      }

      // TODO This should not throw, but return a fail-Result
      WHEN("The replicationFactor equals the number of DBServers") {
#include "ClusterRepairsTest.replicationFactorTooHigh.cpp"

        DistributeShardsLikeRepairer repairer;
        REQUIRE_THROWS_WITH(
          repairer.repairDistributeShardsLike(
            VPackSlice(planCollections->data()),
            VPackSlice(supervisionHealth2Healthy0Bad->data())
          ),
          "not enough healthy db servers"
        );
      }
    }

    GIVEN("An agency where a follower-shard has erroneously ordered DBServers") {
#include "ClusterRepairsTest.unorderedFollowers.cpp"
      checkAgainstExpectedOperations(
        planCollections,
        supervisionHealth4Healthy0Bad,
        expectedOperationsWithWronglyOrderedFollowers
      );
    }

    GIVEN("An agency where a collection has repairingDistributeShardsLike, but nothing else is broken") {
#include "ClusterRepairsTest.repairingDistributeShardsLike.cpp"
      checkAgainstExpectedOperations(
        planCollections,
        supervisionHealth4Healthy0Bad,
        expectedOperationsWithRepairingDistributeShardsLike
      );
    }

  } catch (...) {
    // restore old manager
    AgencyCommManager::MANAGER = std::move(old_manager);
    throw;
  }
  // restore old manager
  AgencyCommManager::MANAGER = std::move(old_manager);

  GIVEN("Different version strings") {
    REQUIRE(VersionSort()("s2", "s10"));
    REQUIRE(! VersionSort()("s10", "s2"));

    REQUIRE(VersionSort()("s5", "s7"));
    REQUIRE(! VersionSort()("s7", "s5"));
  }
}

}
}
}
