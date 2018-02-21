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
using namespace arangodb::velocypack;

namespace arangodb {
namespace tests {
namespace cluster_repairs_test {

std::shared_ptr<VPackBuilder> createBuilder(char const *c) {
  Options options;
  options.checkAttributeUniqueness = true;
  VPackParser parser(&options);
  parser.parse(c);

  std::shared_ptr<VPackBuilder> builder = parser.steal();

  return builder;
}

std::shared_ptr<VPackBuilder> operator "" _vpack(const char* json, size_t) {
  return createBuilder(json);
}

SCENARIO("Broken distributeShardsLike collections", "[cluster][shards][!mayfail]") {
  #include "ClusterRepairsTest.TestData.cpp"

  GIVEN("") {
    // save old manager (maybe null)
    std::unique_ptr<AgencyCommManager> old_manager = std::move(AgencyCommManager::MANAGER);

    try {
      // get a new manager
      AgencyCommManager::initialize("testArangoAgencyPrefix");

      std::vector<AgencyWriteTransaction> transactions
        = ClusterRepairs::repairDistributeShardsLike(planBuilder->slice());

      // TODO there are more values that might be needed in the preconditions,
      // like distributeShardsLike / repairingDistributeShardsLike,
      // or maybe replicationFactor

      std::vector< std::shared_ptr<VPackBuilder> > expectedTransactions {
        R"=([
          { "/testArangoAgencyPrefix/Plan/Collections/someDB/27050122/shards/s27050123": {
              "op": "set",
              "new": [
                "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a",
                "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9"
              ]
            }
          },
          { "/testArangoAgencyPrefix/Plan/Collections/someDB/27050122/shards/s27050123": {
              "old": [
                "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9",
                "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a"
              ]
            }
          }
        ])="_vpack,
        R"=([
          { "/testArangoAgencyPrefix/Plan/Collections/someDB/27050217/shards/s27050238": {
              "op": "set",
              "new": [
                "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a",
                "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9"
              ]
            }
          },
          { "/testArangoAgencyPrefix/Plan/Collections/someDB/27050217/shards/s27050238": {
              "old": [
                "PRMR-98de05bc-bfa8-4211-906d-b0b8f704f9a9",
                "PRMR-edec30ad-1540-4346-b79b-a0cbe6ac6f6a"
              ]
            }
          }
          ])="_vpack
      };

      REQUIRE(transactions.size() == expectedTransactions.size());

      VPackBuilder transactionBuilder = Builder();
      for (auto const& it : boost::combine(transactions, expectedTransactions)) {
        auto const& transactionIt = it.get<0>();
        auto const& expectedTransactionIt = it.get<1>();

        transactionIt.toVelocyPack(transactionBuilder);

        REQUIRE(
          transactionBuilder.slice().toJson() == expectedTransactionIt->slice().toJson()
        ); // either, or:
        REQUIRE(NormalizedCompare::equals(
          transactionBuilder.slice(),
          expectedTransactionIt->slice()
        ));
      }

    } catch(...) {
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
}
