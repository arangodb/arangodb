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
#include <iostream>
#include <velocypack/Parser.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>
#include <velocypack/Compare.h>

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

SCENARIO("Broken distributeShardsLike collections", "[cluster][shards]") {
  #include "ClusterRepairsTest.TestData.cpp"

  GIVEN("") {
    AgencyWriteTransaction transaction
      = ClusterRepairs::repairDistributeShardsLike(planBuilder->slice());

    VPackBuilder transactionBuilder = Builder();

    transaction.toVelocyPack(transactionBuilder);

    std::shared_ptr<VPackBuilder> expectedTransaction
      = R"=([{"myTrx": "foo"}])="_vpack;

    REQUIRE(transactionBuilder.slice().toJson() == expectedTransaction->slice().toJson());
    REQUIRE(NormalizedCompare::equals(transactionBuilder.slice(), expectedTransaction->slice()));
  }
}

}
}
}
