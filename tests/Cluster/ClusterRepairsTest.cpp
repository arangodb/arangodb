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


using namespace arangodb;
using namespace arangodb::consensus;
using namespace arangodb::cluster_repairs;

namespace arangodb {
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

SCENARIO("Broken distributeShardsLike collections", "[cluster][shards][repairs]") {
  #include "ClusterRepairsTest.TestData.cpp"

  // save old manager (may be null)
  std::unique_ptr<AgencyCommManager> old_manager = std::move(AgencyCommManager::MANAGER);

  try {
    // get a new manager
    AgencyCommManager::initialize("testArangoAgencyPrefix");

    GIVEN("An agency where on two shards the DBServers are swapped") {

      DistributeShardsLikeRepairer repairer;

      std::list<AgencyWriteTransaction> transactions
        = repairer.repairDistributeShardsLike(
          VPackSlice(planCollections->data()),
          VPackSlice(supervisionHealth3Healthy0Bad->data())
        );

      // TODO there are more values that might be needed in the preconditions,
      // like distributeShardsLike / repairingDistributeShardsLike,
      // waitForSync, or maybe replicationFactor

      std::vector< std::shared_ptr<VPackBuffer<uint8_t>> > const& expectedTransactions
        = expectedTransactionsWithTwoSwappedDBServers;

      VPackBuilder transactionBuilder;

      Options optPretty = VPackOptions::Defaults;
      optPretty.prettyPrint = true;

      INFO("Expected transactions are:" << std::accumulate(
        expectedTransactions.begin(), expectedTransactions.end(),
        std::string(),
        [&optPretty](std::string const& left, std::shared_ptr<VPackBuffer<uint8_t>> const& right) {
          std::string result = left + "\n"
                               + VPackSlice(right->data()).toJson(&optPretty);
          return result;
        }
      ));
      INFO("Actual transactions are (clientIds are ignored in the comparison):"
        << std::accumulate(
        transactions.begin(), transactions.end(),
        std::string(),
        [&transactionBuilder,&optPretty](std::string const& left, AgencyWriteTransaction const& right) {
          transactionBuilder.clear();
          right.toVelocyPack(transactionBuilder);
          return left + "\n"
                 + transactionBuilder.slice().toJson(&optPretty);
        }
      ));

      REQUIRE(transactions.size() == expectedTransactions.size());

      { // Transaction IDs shall be unique.
        std::set<std::string> transactionClientIds;
        for (auto& it : transactions) {
          bool inserted;
          std::tie(std::ignore, inserted) = transactionClientIds.insert(it.clientId);
          REQUIRE(inserted);

          // Overwrite the client ID for the following comparisons to work
          it.clientId = "dummy-client-id";
        }
      }

      for (auto const& it : boost::combine(transactions, expectedTransactions)) {
        auto const& transactionIt = it.get<0>();
        auto const& expectedTransactionIt = it.get<1>();

        transactionBuilder.clear();
        transactionIt.toVelocyPack(transactionBuilder);

        Slice const& transactionSlice = transactionBuilder.slice();
        Slice const& expectedTransactionSlice = VPackSlice(expectedTransactionIt->data());

        REQUIRE(
          transactionSlice.toJson() == expectedTransactionSlice.toJson()
        ); // either, or:
        REQUIRE(NormalizedCompare::equals(
          transactionSlice,
          expectedTransactionSlice
        ));
      }

      WHEN("The unused DBServer is marked as non-healthy") {
        // TODO this should fail
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
