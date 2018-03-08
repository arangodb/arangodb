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
#include "Cluster/ServerState.h"

#include "Agency/AddFollower.h"
#include "Agency/FailedLeader.h"
#include "Agency/MoveShard.h"
#include "lib/Random/RandomGenerator.h"

#include <boost/date_time.hpp>
#include <boost/range/combine.hpp>

#include <iostream>

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

  ResultT<std::list<RepairOperation>> repairOperationsResult
    = repairer.repairDistributeShardsLike(
      VPackSlice(planCollections->data()),
      VPackSlice(supervisionHealth->data())
    );

  REQUIRE(repairOperationsResult);
  std::list<RepairOperation> &repairOperations = repairOperationsResult.get();

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

  for (auto const &it : boost::combine(repairOperations, expectedRepairOperations)) {
    auto const &repairOpIt = it.get<0>();
    auto const &expectedRepairOpIt = it.get<1>();

    REQUIRE(repairOpIt == expectedRepairOpIt);
  }
}

void requireTransactionIdsToBeUnique(
  std::vector<AgencyWriteTransaction> const& transactions
) {
  std::set<std::__cxx11::string> transactionClientIds;
  for (auto const& it : transactions) {
    bool inserted;
    tie(std::ignore, inserted) = transactionClientIds.insert(it.clientId);
    REQUIRE(inserted);
  }
}

void overwriteTransactionIdsWith(
  std::vector<AgencyWriteTransaction>& transactions,
  std::string const& id
) {
  std::set<std::__cxx11::string> transactionClientIds;
  for (auto& it : transactions) {
    it.clientId = id;
  }
}

// TODO Add a test with a smart collections (i.e. with {"isSmart": true, "shards": []})
// TODO Add a test with a deleted collection
// TODO Add a test with different replicationFactors on leader and follower

SCENARIO("Broken distributeShardsLike collections", "[cluster][shards][repairs]") {

  // save old manager (may be null)
  std::unique_ptr<AgencyCommManager> oldManager = std::move(AgencyCommManager::MANAGER);

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

      WHEN("The unused DBServer is marked as non-healthy") {
#include "ClusterRepairsTest.unusedServerUnhealthy.cpp"

        DistributeShardsLikeRepairer repairer;

        auto result = repairer.repairDistributeShardsLike(
          VPackSlice(planCollections->data()),
          VPackSlice(supervisionHealth2Healthy1Bad->data())
        );

        REQUIRE(result.errorNumber() == TRI_ERROR_CLUSTER_REPAIRS_NOT_ENOUGH_HEALTHY);
        REQUIRE(0 == strcmp(TRI_errno_string(result.errorNumber()), "not enough healthy db servers"));
        REQUIRE(result.fail());
      }

      WHEN("The replicationFactor equals the number of DBServers") {
#include "ClusterRepairsTest.replicationFactorTooHigh.cpp"

        DistributeShardsLikeRepairer repairer;

        auto result = repairer.repairDistributeShardsLike(
          VPackSlice(planCollections->data()),
          VPackSlice(supervisionHealth2Healthy0Bad->data())
        );

        REQUIRE(result.errorNumber() == TRI_ERROR_CLUSTER_REPAIRS_NOT_ENOUGH_HEALTHY);
        REQUIRE(0 == strcmp(TRI_errno_string(result.errorNumber()), "not enough healthy db servers"));
        REQUIRE(result.fail());
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
    AgencyCommManager::MANAGER = std::move(oldManager);
    throw;
  }
  // restore old manager
  AgencyCommManager::MANAGER = std::move(oldManager);
}

SCENARIO("VersionSort", "[cluster][shards][repairs]") {
  GIVEN("Different version strings") {
    REQUIRE(VersionSort()("s2", "s10"));
    REQUIRE(! VersionSort()("s10", "s2"));

    REQUIRE(VersionSort()("s5", "s7"));
    REQUIRE(! VersionSort()("s7", "s5"));
  }
}

// TODO write more tests for RepairOperation to Transaction conversion

SCENARIO("Cluster RepairOperations", "[cluster][shards][repairs]") {

  // save old manager (may be null)
  std::unique_ptr<AgencyCommManager> oldManager = std::move(AgencyCommManager::MANAGER);
  std::string const oldServerId = ServerState::instance()->getId();

  try {
    // get a new manager
    AgencyCommManager::initialize("testArangoAgencyPrefix");

    uint64_t (*mockJobIdGenerator)() = []() {
      REQUIRE(false);
      return 0ul;
    };
    std::chrono::system_clock::time_point (*mockJobCreationTimestampGenerator)() = []() {
      REQUIRE(false);
      return std::chrono::system_clock::now();
    };

    RepairOperationToTransactionVisitor conversionVisitor(
      mockJobIdGenerator,
      mockJobCreationTimestampGenerator
    );

    GIVEN("A BeginRepairsOperation with equal replicationFactors and rename=true") {
      BeginRepairsOperation operation{
        .database = "myDbName",
        .collectionId = "123456",
        .collectionName = "myCollection",
        .protoCollectionId = "789876",
        .protoCollectionName = "myProtoCollection",
        .collectionReplicationFactor = 3,
        .protoReplicationFactor = 3,
        .renameDistributeShardsLike = true,
      };

      WHEN("Converted into an AgencyTransaction") {
        AgencyWriteTransaction trx;
        boost::optional<uint64_t> jobid;
        std::tie(trx, jobid) = conversionVisitor(operation);

        REQUIRE_FALSE(jobid.is_initialized());

        VPackBufferPtr protoCollIdVpack = R"=("789876")="_vpack;
        Slice protoCollIdSlice = Slice(protoCollIdVpack->data());
        VPackBufferPtr replicationFactorVpack = R"=(3)="_vpack;
        Slice replicationFactorSlice = Slice(replicationFactorVpack->data());

        AgencyWriteTransaction expectedTrx{
          std::vector<AgencyOperation> {
            AgencyOperation {
              "Plan/Collections/myDbName/123456/distributeShardsLike",
              AgencySimpleOperationType::DELETE_OP,
            },
            AgencyOperation {
              "Plan/Collections/myDbName/123456/repairingDistributeShardsLike",
              AgencyValueOperationType::SET,
              protoCollIdSlice,
            },
          },
          std::vector<AgencyPrecondition> {
            AgencyPrecondition {
              "Plan/Collections/myDbName/123456/repairingDistributeShardsLike",
              AgencyPrecondition::Type::EMPTY,
              true,
            },
            AgencyPrecondition {
              "Plan/Collections/myDbName/123456/distributeShardsLike",
              AgencyPrecondition::Type::VALUE,
              protoCollIdSlice,
            },
            AgencyPrecondition {
              "Plan/Collections/myDbName/123456/replicationFactor",
              AgencyPrecondition::Type::VALUE,
              replicationFactorSlice,
            },
            AgencyPrecondition {
              "Plan/Collections/myDbName/789876/replicationFactor",
              AgencyPrecondition::Type::VALUE,
              replicationFactorSlice,
            },
          }
        };

        trx.clientId
          = expectedTrx.clientId
          = "dummy-client-id";

        REQUIRE(trx == expectedTrx);
      }

      WHEN("Compared via ==") {
        BeginRepairsOperation other = operation;

        REQUIRE(operation == other);

        (other = operation).database = "differing database";
        REQUIRE_FALSE(operation == other);
        (other = operation).collectionId = "differing collectionId";
        REQUIRE_FALSE(operation == other);
        (other = operation).collectionName = "differing collectionName";
        REQUIRE_FALSE(operation == other);
        (other = operation).protoCollectionId = "differing protoCollectionId";
        REQUIRE_FALSE(operation == other);
        (other = operation).protoCollectionName = "differing protoCollectionName";
        REQUIRE_FALSE(operation == other);
        (other = operation).collectionReplicationFactor = 42;
        REQUIRE_FALSE(operation == other);
        (other = operation).protoReplicationFactor = 23;
        REQUIRE_FALSE(operation == other);
        (other = operation).renameDistributeShardsLike = ! operation.renameDistributeShardsLike;
        REQUIRE_FALSE(operation == other);

      }
    }

    GIVEN("A BeginRepairsOperation differing replicationFactors and rename=false") {
      BeginRepairsOperation operation{
        .database = "myDbName",
        .collectionId = "123456",
        .collectionName = "myCollection",
        .protoCollectionId = "789876",
        .protoCollectionName = "myProtoCollection",
        .collectionReplicationFactor = 5,
        .protoReplicationFactor = 4,
        .renameDistributeShardsLike = false,
      };

      WHEN("Converted into an AgencyTransaction") {
        AgencyWriteTransaction trx;
        boost::optional<uint64_t> jobid;
        std::tie(trx, jobid) = conversionVisitor(operation);

        REQUIRE_FALSE(jobid.is_initialized());

        VPackBufferPtr protoCollIdVpack = R"=("789876")="_vpack;
        Slice protoCollIdSlice = Slice(protoCollIdVpack->data());
        VPackBufferPtr oldReplicationFactorVpack = R"=(5)="_vpack;
        Slice oldReplicationFactorSlice = Slice(oldReplicationFactorVpack->data());
        VPackBufferPtr replicationFactorVpack = R"=(4)="_vpack;
        Slice replicationFactorSlice = Slice(replicationFactorVpack->data());

        AgencyWriteTransaction expectedTrx{
          std::vector<AgencyOperation> {
            AgencyOperation {
              "Plan/Collections/myDbName/123456/replicationFactor",
              AgencyValueOperationType::SET,
              replicationFactorSlice,
            },
          },
          std::vector<AgencyPrecondition> {
            AgencyPrecondition {
              "Plan/Collections/myDbName/123456/distributeShardsLike",
              AgencyPrecondition::Type::EMPTY,
              true,
            },
            AgencyPrecondition {
              "Plan/Collections/myDbName/123456/repairingDistributeShardsLike",
              AgencyPrecondition::Type::VALUE,
              protoCollIdSlice,
            },
            AgencyPrecondition {
              "Plan/Collections/myDbName/123456/replicationFactor",
              AgencyPrecondition::Type::VALUE,
              oldReplicationFactorSlice,
            },
            AgencyPrecondition {
              "Plan/Collections/myDbName/789876/replicationFactor",
              AgencyPrecondition::Type::VALUE,
              replicationFactorSlice,
            },
          }
        };

        trx.clientId
          = expectedTrx.clientId
          = "dummy-client-id";

        REQUIRE(trx == expectedTrx);
      }
    }

    GIVEN("A FinishRepairsOperation") {
      FinishRepairsOperation operation{
        .database = "myDbName",
        .collectionId = "123456",
        .collectionName = "myCollection",
        .protoCollectionId = "789876",
        .protoCollectionName = "myProtoCollection",
        .replicationFactor = 3,
      };

      WHEN("Converted into an AgencyTransaction") {
        AgencyWriteTransaction trx;
        boost::optional<uint64_t> jobid;
        std::tie(trx, jobid) = conversionVisitor(operation);

        REQUIRE_FALSE(jobid.is_initialized());

        VPackBufferPtr protoIdVpack = R"=("789876")="_vpack;
        Slice protoIdSlice = Slice(protoIdVpack->data());
        VPackBufferPtr replicationFactorVpack = R"=(3)="_vpack;
        Slice replicationFactorSlice = Slice(replicationFactorVpack->data());

        AgencyWriteTransaction expectedTrx{
          std::vector<AgencyOperation> {
            AgencyOperation {
              "Plan/Collections/myDbName/123456/repairingDistributeShardsLike",
              AgencySimpleOperationType::DELETE_OP,
            },
            AgencyOperation {
              "Plan/Collections/myDbName/123456/distributeShardsLike",
              AgencyValueOperationType::SET,
              protoIdSlice,
            },
          },
          std::vector<AgencyPrecondition> {
            AgencyPrecondition {
              "Plan/Collections/myDbName/123456/distributeShardsLike",
              AgencyPrecondition::Type::EMPTY,
              true,
            },
            AgencyPrecondition {
              "Plan/Collections/myDbName/123456/repairingDistributeShardsLike",
              AgencyPrecondition::Type::VALUE,
              protoIdSlice,
            },
            AgencyPrecondition {
              "Plan/Collections/myDbName/123456/replicationFactor",
              AgencyPrecondition::Type::VALUE,
              replicationFactorSlice,
            },
            AgencyPrecondition {
              "Plan/Collections/myDbName/789876/replicationFactor",
              AgencyPrecondition::Type::VALUE,
              replicationFactorSlice,
            },
          },
        };

        trx.clientId
          = expectedTrx.clientId
          = "dummy-client-id";

        REQUIRE(trx == expectedTrx);
      }

      WHEN("Compared via ==") {
        FinishRepairsOperation other = operation;

        REQUIRE(operation == other);

        (other = operation).database = "differing database";
        REQUIRE_FALSE(operation == other);
        (other = operation).collectionId = "differing collectionId";
        REQUIRE_FALSE(operation == other);
        (other = operation).collectionName = "differing collectionName";
        REQUIRE_FALSE(operation == other);
        (other = operation).protoCollectionId = "differing protoCollectionId";
        REQUIRE_FALSE(operation == other);
        (other = operation).protoCollectionName = "differing protoCollectionName";
        REQUIRE_FALSE(operation == other);
        (other = operation).replicationFactor = 42;
        REQUIRE_FALSE(operation == other);

      }
    }

    GIVEN("A MoveShardOperation") {
      ServerState::instance()->setId("CurrentCoordinatorServerId");

      MoveShardOperation operation{
        .database = "myDbName",
        .collectionId = "123456",
        .collectionName = "myCollection",
        .shard = "s1",
        .from = "db-from-server",
        .to = "db-to-server",
        .isLeader = true,
      };

      WHEN("Converted into an AgencyTransaction") {
        uint64_t nextJobId = 41;
        auto jobIdGenerator = [&nextJobId]() {
          return nextJobId++;
        };
        auto jobCreationTimestampGenerator = []() {
          std::tm tm = {};
          tm.tm_year = 2018 - 1900; // years since 1900
          tm.tm_mon = 3 - 1; // March, counted from january
          tm.tm_mday = 7;
          tm.tm_hour = 15;
          tm.tm_min = 20;
          tm.tm_sec = 1;
          tm.tm_isdst = 0;

          std::chrono::system_clock::time_point tp
            = std::chrono::system_clock::from_time_t(TRI_timegm(&tm));

          return tp;
        };

        conversionVisitor = RepairOperationToTransactionVisitor(
          jobIdGenerator,
          jobCreationTimestampGenerator
        );

        AgencyWriteTransaction trx;
        boost::optional<uint64_t> jobId;
        std::tie(trx, jobId) = conversionVisitor(operation);

        REQUIRE(jobId.is_initialized());
// "timeCreated": "2018-03-07T15:20:01.284Z",
        VPackBufferPtr todoVpack = R"=(
          {
            "type": "moveShard",
            "database": "myDbName",
            "collection": "123456",
            "shard": "s1",
            "fromServer": "db-from-server",
            "toServer": "db-to-server",
            "jobId": "41",
            "timeCreated": "2018-03-07T15:20:01Z",
            "creator": "CurrentCoordinatorServerId",
            "isLeader": true
          }
        )="_vpack;
        Slice todoSlice = Slice(todoVpack->data());

        AgencyWriteTransaction expectedTrx {
          AgencyOperation {
            "Target/ToDo/" + std::to_string(jobId.get()),
            AgencyValueOperationType::SET,
            todoSlice,
          },
          AgencyPrecondition {
            "Target/ToDo/" + std::to_string(jobId.get()),
            AgencyPrecondition::Type::EMPTY,
            true,
          },
        };

        trx.clientId
          = expectedTrx.clientId
          = "dummy-client-id";

        REQUIRE(trx == expectedTrx);
      }

      WHEN("Compared via ==") {
        MoveShardOperation other = operation;

        REQUIRE(operation == other);

        (other = operation).database = "differing database";
        REQUIRE_FALSE(operation == other);
        (other = operation).collectionId = "differing collectionId";
        REQUIRE_FALSE(operation == other);
        (other = operation).collectionName = "differing collectionName";
        REQUIRE_FALSE(operation == other);
        (other = operation).shard = "differing shard";
        REQUIRE_FALSE(operation == other);
        (other = operation).from = "differing from";
        REQUIRE_FALSE(operation == other);
        (other = operation).to = "differing to";
        REQUIRE_FALSE(operation == other);
        (other = operation).isLeader = ! operation.isLeader;
        REQUIRE_FALSE(operation == other);

      }
    }

    GIVEN("A FixServerOrderOperation") {
      FixServerOrderOperation operation{
        .database = "myDbName",
        .collectionId = "123456",
        .collectionName = "myCollection",
        .protoCollectionId = "789876",
        .protoCollectionName = "myProtoCollection",
        .shard = "s1",
        .protoShard = "s7",
        .leader = "db-leader-server",
        .followers = {
          "db-follower-3-server",
          "db-follower-2-server",
          "db-follower-4-server",
          "db-follower-1-server",
        },
        .protoFollowers = {
          "db-follower-1-server",
          "db-follower-2-server",
          "db-follower-3-server",
          "db-follower-4-server",
        },
      };

      WHEN("Converted into an AgencyTransaction") {
        VPackBufferPtr previousServerOrderVpack = R"=([
          "db-leader-server",
          "db-follower-3-server",
          "db-follower-2-server",
          "db-follower-4-server",
          "db-follower-1-server"
        ])="_vpack;
        VPackBufferPtr correctServerOrderVpack = R"=([
          "db-leader-server",
          "db-follower-1-server",
          "db-follower-2-server",
          "db-follower-3-server",
          "db-follower-4-server"
        ])="_vpack;
        Slice previousServerOrderSlice = Slice(previousServerOrderVpack->data());
        Slice correctServerOrderSlice = Slice(correctServerOrderVpack->data());

        AgencyWriteTransaction trx;
        boost::optional<uint64_t> jobid;
        std::tie(trx, jobid) = conversionVisitor(operation);

        REQUIRE_FALSE(jobid.is_initialized());

        AgencyWriteTransaction expectedTrx{
          AgencyOperation {
            "Plan/Collections/myDbName/123456/shards/s1",
            AgencyValueOperationType::SET,
            correctServerOrderSlice,
          },
          {
            AgencyPrecondition {
              "Plan/Collections/myDbName/123456/shards/s1",
              AgencyPrecondition::Type::VALUE,
              previousServerOrderSlice,
            },
            AgencyPrecondition {
              "Plan/Collections/myDbName/789876/shards/s7",
              AgencyPrecondition::Type::VALUE,
              correctServerOrderSlice,
            },
          },
        };

        trx.clientId
          = expectedTrx.clientId
          = "dummy-client-id";

        REQUIRE(trx == expectedTrx);
      }

      WHEN("Compared via ==") {
        FixServerOrderOperation other = operation;

        REQUIRE(operation == other);

        (other = operation).database = "differing database";
        REQUIRE_FALSE(operation == other);
        (other = operation).collectionId = "differing collectionId";
        REQUIRE_FALSE(operation == other);
        (other = operation).collectionName = "differing collectionName";
        REQUIRE_FALSE(operation == other);
        (other = operation).protoCollectionId = "differing protoCollectionId";
        REQUIRE_FALSE(operation == other);
        (other = operation).protoCollectionName = "differing protoCollectionName";
        REQUIRE_FALSE(operation == other);
        (other = operation).shard = "differing shard";
        REQUIRE_FALSE(operation == other);
        (other = operation).protoShard = "differing protoShard";
        REQUIRE_FALSE(operation == other);
        (other = operation).leader = "differing leader";
        REQUIRE_FALSE(operation == other);
        (other = operation).followers = {"differing", "followers"};
        REQUIRE_FALSE(operation == other);
        (other = operation).protoFollowers = {"differing", "protoFollowers"};
        REQUIRE_FALSE(operation == other);

      }
    }

  }
  catch (...) {
    // restore old manager
    AgencyCommManager::MANAGER = std::move(oldManager);
    throw;
  }
  // restore old manager
  AgencyCommManager::MANAGER = std::move(oldManager);
}


}
}
}
