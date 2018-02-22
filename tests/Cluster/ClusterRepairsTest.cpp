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

namespace arangodb {
namespace tests {
namespace cluster_repairs_test {

std::shared_ptr<VPackBuffer<uint8_t>> vpackFromJsonString(char const *c) {
  Options options;
  options.checkAttributeUniqueness = true;
  VPackParser parser(&options);
  parser.parse(c);

  std::shared_ptr<VPackBuilder> builder = parser.steal();

  return builder->steal();
}

std::shared_ptr<VPackBuffer<uint8_t>> operator "" _vpack(const char* json, size_t) {
  return vpackFromJsonString(json);
}

SCENARIO("Broken distributeShardsLike collections", "[cluster][shards][!mayfail]") {
  #include "ClusterRepairsTest.TestData.cpp"

  // save old manager (maybe null)
  std::unique_ptr<AgencyCommManager> old_manager = std::move(AgencyCommManager::MANAGER);

  try {
    // get a new manager
    AgencyCommManager::initialize("testArangoAgencyPrefix");

    GIVEN("An agency where on two shards the DBServers are swapped") {

      std::vector<AgencyWriteTransaction> transactions
        = ClusterRepairs::repairDistributeShardsLike(VPackSlice(plan->data()));

      // TODO there are more values that might be needed in the preconditions,
      // like distributeShardsLike / repairingDistributeShardsLike,
      // or maybe replicationFactor

      std::vector< std::shared_ptr<VPackBuffer<uint8_t>> > const& expectedTransactions
        = expectedTransactionsWithTwoSwappedDBServers;

      REQUIRE(transactions.size() == expectedTransactions.size());

      VPackBuilder transactionBuilder = Builder();
      for (auto const& it : boost::combine(transactions, expectedTransactions)) {
        auto const& transactionIt = it.get<0>();
        auto const& expectedTransactionIt = it.get<1>();

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
        // TODO should this fail, or reduce the replicationFactor to 1 to move
        // the leader?
      }
    }

    GIVEN("An agency where the replicationFactor equals the number of DBServers") {
      // TODO
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
