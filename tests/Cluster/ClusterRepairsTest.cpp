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

#include <boost/core/demangle.hpp>
#include <boost/date_time.hpp>
#include <boost/range/combine.hpp>

#include <iostream>
#include <typeinfo>

using namespace arangodb;
using namespace arangodb::consensus;
using namespace arangodb::cluster_repairs;

namespace arangodb {

bool operator==(AgencyWriteTransaction const& left,
                AgencyWriteTransaction const& right) {
  VPackBuilder leftBuilder;
  VPackBuilder rightBuilder;

  left.toVelocyPack(leftBuilder);
  right.toVelocyPack(rightBuilder);

  return velocypack::NormalizedCompare::equals(leftBuilder.slice(),
                                               rightBuilder.slice());
}

std::ostream& operator<<(std::ostream& ostream,
                         AgencyWriteTransaction const& trx) {
  Options optPretty = VPackOptions::Defaults;
  optPretty.prettyPrint = true;

  VPackBuilder builder;

  trx.toVelocyPack(builder);

  ostream << builder.slice().toJson(&optPretty);

  return ostream;
}

std::ostream& operator<<(std::ostream& ostream,
                         std::list<RepairOperation> const& list) {
  ostream << "std::list<RepairOperation> {\n";
  if (!list.empty()) {
    auto it = list.begin();
    ostream << *it << "\n";
    for (++it; it != list.end(); ++it) {
      ostream << ", " << *it << "\n";
    }
  }
  ostream << "}" << std::endl;
  return ostream;
}

std::ostream& operator<<(std::ostream& ostream,
                         std::vector<RepairOperation> const& vector) {
  ostream << "std::vector<RepairOperation> {\n";
  if (!vector.empty()) {
    auto it = vector.begin();
    ostream << *it << "\n";
    for (++it; it != vector.end(); ++it) {
      ostream << ", " << *it << "\n";
    }
  }
  ostream << "}" << std::endl;
  return ostream;
}

template <typename K, typename V>
std::ostream& operator<<(std::ostream& ostream, std::map<K, V> const& map) {
  std::string const typeNameK = boost::core::demangle(typeid(K).name());
  std::string const typeNameV = boost::core::demangle(typeid(V).name());
  ostream << "std::map<" << typeNameK << ", " << typeNameV << "> {\n";
  if (!map.empty()) {
    auto it = map.begin();
    ostream << it->first << " => " << it->second << "\n";
    for (++it; it != map.end(); ++it) {
      ostream << ", " << it->first << " => " << it->second << "\n";
    }
  }
  ostream << "}" << std::endl;
  return ostream;
}

template <typename T>
std::ostream& operator<<(std::ostream& stream, ResultT<T> const& result) {
  std::string const typeName =
      typeid(T) == typeid(RepairOperation)
          ? "RepairOperation"
          : typeid(T) == typeid(std::list<RepairOperation>)
                ? "std::list<RepairOperation>"
                : typeid(T) == typeid(std::vector<RepairOperation>)
                      ? "std::vector<RepairOperation>"
                      : boost::core::demangle(typeid(T).name());

  if (result.ok()) {
    return stream << "ResultT<" << typeName << "> {"
                  << "val = " << result.get() << " }";
  }

  return stream << "ResultT<" << typeName << "> {"
                << "errorNumber = " << result.errorNumber() << ", "
                << "errorMessage = \"" << result.errorMessage() << "\" }";
}

namespace tests {
namespace cluster_repairs_test {

VPackBufferPtr vpackFromJsonString(char const* c) {
  Options options;
  options.checkAttributeUniqueness = true;
  VPackParser parser(&options);
  parser.parse(c);

  std::shared_ptr<VPackBuilder> builder = parser.steal();

  return builder->steal();
}

VPackBufferPtr operator"" _vpack(const char* json, size_t) {
  return vpackFromJsonString(json);
}

void checkAgainstExpectedOperations(
    VPackBufferPtr const& planCollections,
    VPackBufferPtr const& supervisionHealth,
    std::map<CollectionID, ResultT<std::vector<RepairOperation>>>
        expectedRepairOperationsByCollection) {
  DistributeShardsLikeRepairer repairer;

  ResultT<std::map<CollectionID, ResultT<std::list<RepairOperation>>>>
      repairOperationsByCollectionResult = repairer.repairDistributeShardsLike(
          VPackSlice(planCollections->data()),
          VPackSlice(supervisionHealth->data()));

  INFO(repairOperationsByCollectionResult);
  REQUIRE(repairOperationsByCollectionResult.ok());
  std::map<CollectionID, ResultT<std::list<RepairOperation>>>&
      repairOperationsByCollection = repairOperationsByCollectionResult.get();

  {
    std::stringstream expectedOperationsStringStream;
    expectedOperationsStringStream << "{" << std::endl;
    for (auto const& it : expectedRepairOperationsByCollection) {
      std::string const& collection = it.first;
      ResultT<std::vector<RepairOperation>> const& expectedRepairResult =
          it.second;

      expectedOperationsStringStream << "\"" << collection << "\": \n";
      expectedOperationsStringStream << expectedRepairResult << std::endl;
    }
    expectedOperationsStringStream << "}";

    std::stringstream repairOperationsStringStream;
    repairOperationsStringStream << "{" << std::endl;
    for (auto const& it : repairOperationsByCollection) {
      std::string const& collection = it.first;
      ResultT<std::list<RepairOperation>> const repairOperationsResult =
          it.second;
      repairOperationsStringStream << "\"" << collection << "\": \n";
      repairOperationsStringStream << repairOperationsResult << std::endl;
    }
    repairOperationsStringStream << "}";

    INFO("Expected operations are:\n" << expectedOperationsStringStream.str());
    INFO("Actual operations are:\n" << repairOperationsStringStream.str());

    REQUIRE(repairOperationsByCollection.size() ==
            expectedRepairOperationsByCollection.size());
    for (auto const& it : boost::combine(expectedRepairOperationsByCollection,
                                         repairOperationsByCollection)) {
      auto const& expectedResult = it.get<0>().second;
      auto const& actualResult = it.get<1>().second;

      INFO("Expected operations are:\n"
           << expectedOperationsStringStream.str());
      INFO("Actual operations are:\n" << repairOperationsStringStream.str());
      REQUIRE(expectedResult.ok() == actualResult.ok());
      if (expectedResult.ok()) {
        REQUIRE(expectedResult.get().size() == actualResult.get().size());
      }
    }
  }

  for (auto const& it : boost::combine(repairOperationsByCollection,
                                       expectedRepairOperationsByCollection)) {
    std::string const& collection = it.get<0>().first;
    ResultT<std::list<RepairOperation>> const repairResult = it.get<0>().second;

    std::string const& expectedCollection = it.get<1>().first;
    ResultT<std::vector<RepairOperation>> const& expectedResult =
        it.get<1>().second;

    REQUIRE(collection == expectedCollection);
    REQUIRE(repairResult.ok() == expectedResult.ok());
    if (expectedResult.ok()) {
      std::list<RepairOperation> const& repairOperations = repairResult.get();
      std::vector<RepairOperation> const& expectedOperations =
          expectedResult.get();

      for (auto const& it :
           boost::combine(repairOperations, expectedOperations)) {
        auto const& repairOpIt = it.get<0>();
        auto const& expectedRepairOpIt = it.get<1>();

        REQUIRE(repairOpIt == expectedRepairOpIt);
      }
    } else {
      REQUIRE(repairResult == expectedResult);
    }
  }
}

// TODO Add a test with a deleted collection

SCENARIO("Broken distributeShardsLike collections",
         "[cluster][shards][repairs]") {
  // save old manager (may be null)
  std::unique_ptr<AgencyCommManager> oldManager =
      std::move(AgencyCommManager::MANAGER);

  try {
    // get a new manager
    AgencyCommManager::initialize("testArangoAgencyPrefix");

    GIVEN("An agency where on two shards the DBServers are swapped") {
      WHEN("One unused DBServer is free to exchange the leader") {
#include "ClusterRepairsTest.swapWithLeader.cpp"

        checkAgainstExpectedOperations(planCollections,
                                       supervisionHealth3Healthy0Bad,
                                       expectedResultsWithTwoSwappedDBServers);
      }

      WHEN("The unused DBServer is marked as non-healthy") {
#include "ClusterRepairsTest.unusedServerUnhealthy.cpp"

        DistributeShardsLikeRepairer repairer;

        auto result = repairer.repairDistributeShardsLike(
            VPackSlice(planCollections->data()),
            VPackSlice(supervisionHealth2Healthy1Bad->data()));

        REQUIRE(result.ok());
        std::map<CollectionID, ResultT<std::list<RepairOperation>>>
            operationResultByCollectionId = result.get();
        REQUIRE(operationResultByCollectionId.size() == 1);
        REQUIRE(operationResultByCollectionId.find("11111111") !=
                operationResultByCollectionId.end());
        ResultT<std::list<RepairOperation>> collectionResult =
            operationResultByCollectionId.at("11111111");

        REQUIRE(collectionResult.errorNumber() ==
                TRI_ERROR_CLUSTER_REPAIRS_NOT_ENOUGH_HEALTHY);
        REQUIRE(0 == strcmp(TRI_errno_string(collectionResult.errorNumber()),
                            "not enough (healthy) db servers"));
        REQUIRE(collectionResult.fail());
      }

      WHEN("The replicationFactor equals the number of DBServers") {
#include "ClusterRepairsTest.replicationFactorTooHigh.cpp"

        DistributeShardsLikeRepairer repairer;

        auto result = repairer.repairDistributeShardsLike(
            VPackSlice(planCollections->data()),
            VPackSlice(supervisionHealth2Healthy0Bad->data()));

        REQUIRE(result.ok());
        std::map<CollectionID, ResultT<std::list<RepairOperation>>>
            operationResultByCollectionId = result.get();
        REQUIRE(operationResultByCollectionId.size() == 1);
        REQUIRE(operationResultByCollectionId.find("11111111") !=
                operationResultByCollectionId.end());
        ResultT<std::list<RepairOperation>> collectionResult =
            operationResultByCollectionId.at("11111111");

        REQUIRE(collectionResult.errorNumber() ==
                TRI_ERROR_CLUSTER_REPAIRS_NOT_ENOUGH_HEALTHY);
        REQUIRE(0 == strcmp(TRI_errno_string(collectionResult.errorNumber()),
                            "not enough (healthy) db servers"));
        REQUIRE(collectionResult.fail());
      }
    }

    GIVEN(
        "An agency where a follower-shard has erroneously ordered DBServers") {
#include "ClusterRepairsTest.unorderedFollowers.cpp"
      checkAgainstExpectedOperations(
          planCollections, supervisionHealth4Healthy0Bad,
          expectedResultsWithWronglyOrderedFollowers);
    }

    GIVEN(
        "An agency where a collection has repairingDistributeShardsLike, but "
        "nothing else is broken") {
#include "ClusterRepairsTest.repairingDistributeShardsLike.cpp"
      checkAgainstExpectedOperations(
          planCollections, supervisionHealth4Healthy0Bad,
          expectedResultsWithRepairingDistributeShardsLike);
    }

    GIVEN(
        "An agency where a collection has repairingDistributeShardsLike, but "
        "the replicationFactor differs") {
#include "ClusterRepairsTest.repairingDslChangedRf.cpp"
      checkAgainstExpectedOperations(
          planCollections, supervisionHealth4Healthy0Bad,
          expectedResultsWithRepairingDistributeShardsLike);
    }

    GIVEN("An agency with multiple collections") {
#include "ClusterRepairsTest.multipleCollections.cpp"
      checkAgainstExpectedOperations(planCollections,
                                     supervisionHealth4Healthy0Bad,
                                     expectedResultsWithMultipleCollections);
    }

    GIVEN("A collection with multiple shards") {
#include "ClusterRepairsTest.multipleShards.cpp"
      checkAgainstExpectedOperations(planCollections,
                                     supervisionHealth3Healthy0Bad,
                                     expectedResultsWithMultipleShards);
    }

    GIVEN(
        "A collection where the replicationFactor doesn't conform with its "
        "prototype") {
#include "ClusterRepairsTest.unequalReplicationFactor.cpp"
      checkAgainstExpectedOperations(
          planCollections, supervisionHealth3Healthy0Bad,
          expectedResultsWithUnequalReplicationFactor);
    }

    GIVEN(
        "A smart graph with some broken collections") {
#include "ClusterRepairsTest.smartCollections.cpp"
      checkAgainstExpectedOperations(
          planCollections, supervisionHealth3Healthy0Bad,
        expectedResultsWithSmartGraph);
    }

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
    GIVEN("Collections with triggered failures") {
// NOTE: Some of the collection names used in in the following file would
// usually be invalid because they are too long.
#include "ClusterRepairsTest.triggeredFailures.cpp"
      TRI_AddFailurePointDebugging(
          "DistributeShardsLikeRepairer::createFixServerOrderOperation/"
          "TRI_ERROR_CLUSTER_REPAIRS_MISMATCHING_LEADERS");
      TRI_AddFailurePointDebugging(
          "DistributeShardsLikeRepairer::createFixServerOrderOperation/"
          "TRI_ERROR_CLUSTER_REPAIRS_MISMATCHING_FOLLOWERS");
      TRI_AddFailurePointDebugging(
          "DistributeShardsLikeRepairer::repairDistributeShardsLike/"
          "TRI_ERROR_CLUSTER_REPAIRS_INCONSISTENT_ATTRIBUTES");
      TRI_AddFailurePointDebugging(
          "DistributeShardsLikeRepairer::createBeginRepairsOperation/"
          "TRI_ERROR_CLUSTER_REPAIRS_INCONSISTENT_ATTRIBUTES");
      TRI_AddFailurePointDebugging(
          "DistributeShardsLikeRepairer::createFinishRepairsOperation/"
          "TRI_ERROR_CLUSTER_REPAIRS_INCONSISTENT_ATTRIBUTES");
      TRI_AddFailurePointDebugging(
          "DistributeShardsLikeRepairer::repairDistributeShardsLike/"
          "TRI_ERROR_CLUSTER_REPAIRS_NO_DBSERVERS");
      try {
        checkAgainstExpectedOperations(planCollections,
                                       supervisionHealth2Healthy0Bad,
                                       expectedResultsWithTriggeredFailures);
        TRI_ClearFailurePointsDebugging();
      } catch (...) {
        TRI_ClearFailurePointsDebugging();
        throw;
      }
    }
#endif

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
    REQUIRE(!VersionSort()("s10", "s2"));

    REQUIRE(VersionSort()("s5", "s7"));
    REQUIRE(!VersionSort()("s7", "s5"));
  }
}

SCENARIO("Cluster RepairOperations", "[cluster][shards][repairs]") {
  // save old manager (may be null)
  std::unique_ptr<AgencyCommManager> oldManager =
      std::move(AgencyCommManager::MANAGER);
  std::string const oldServerId = ServerState::instance()->getId();

  try {
    // get a new manager
    AgencyCommManager::initialize("testArangoAgencyPrefix");

    uint64_t (*mockJobIdGenerator)() = []() {
      REQUIRE(false);
      return 0ul;
    };
    std::chrono::system_clock::time_point (
        *mockJobCreationTimestampGenerator)() = []() {
      REQUIRE(false);
      return std::chrono::system_clock::now();
    };

    RepairOperationToTransactionVisitor conversionVisitor(
        mockJobIdGenerator, mockJobCreationTimestampGenerator);

    GIVEN(
        "A BeginRepairsOperation with equal replicationFactors and "
        "rename=true") {
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
            std::vector<AgencyOperation>{
                AgencyOperation{
                    "Plan/Collections/myDbName/123456/distributeShardsLike",
                    AgencySimpleOperationType::DELETE_OP,
                },
                AgencyOperation{
                    "Plan/Collections/myDbName/123456/"
                    "repairingDistributeShardsLike",
                    AgencyValueOperationType::SET, protoCollIdSlice,
                },
                AgencyOperation{
                    "Plan/Collections/myDbName/123456/replicationFactor",
                    AgencyValueOperationType::SET, replicationFactorSlice,
                },
            },
            std::vector<AgencyPrecondition>{
                AgencyPrecondition{
                    "Plan/Collections/myDbName/123456/"
                    "repairingDistributeShardsLike",
                    AgencyPrecondition::Type::EMPTY, true,
                },
                AgencyPrecondition{
                    "Plan/Collections/myDbName/123456/distributeShardsLike",
                    AgencyPrecondition::Type::VALUE, protoCollIdSlice,
                },
                AgencyPrecondition{
                    "Plan/Collections/myDbName/123456/replicationFactor",
                    AgencyPrecondition::Type::VALUE, replicationFactorSlice,
                },
                AgencyPrecondition{
                    "Plan/Collections/myDbName/789876/replicationFactor",
                    AgencyPrecondition::Type::VALUE, replicationFactorSlice,
                },
            }};

        trx.clientId = expectedTrx.clientId = "dummy-client-id";

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
        (other = operation).protoCollectionName =
            "differing protoCollectionName";
        REQUIRE_FALSE(operation == other);
        (other = operation).collectionReplicationFactor = 42;
        REQUIRE_FALSE(operation == other);
        (other = operation).protoReplicationFactor = 23;
        REQUIRE_FALSE(operation == other);
        (other = operation).renameDistributeShardsLike =
            !operation.renameDistributeShardsLike;
        REQUIRE_FALSE(operation == other);
      }
    }

    GIVEN(
        "A BeginRepairsOperation with differing replicationFactors and "
        "rename=false") {
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
        VPackBufferPtr replicationFactorVpack = R"=(4)="_vpack;
        Slice replicationFactorSlice = Slice(replicationFactorVpack->data());

        AgencyWriteTransaction expectedTrx{
            {},
            std::vector<AgencyPrecondition>{
                AgencyPrecondition{
                    "Plan/Collections/myDbName/123456/distributeShardsLike",
                    AgencyPrecondition::Type::EMPTY, true,
                },
                AgencyPrecondition{
                    "Plan/Collections/myDbName/123456/"
                    "repairingDistributeShardsLike",
                    AgencyPrecondition::Type::VALUE, protoCollIdSlice,
                },
                AgencyPrecondition{
                    "Plan/Collections/myDbName/123456/replicationFactor",
                    AgencyPrecondition::Type::VALUE, replicationFactorSlice,
                },
                AgencyPrecondition{
                    "Plan/Collections/myDbName/789876/replicationFactor",
                    AgencyPrecondition::Type::VALUE, replicationFactorSlice,
                },
            }};

        trx.clientId = expectedTrx.clientId = "dummy-client-id";

        REQUIRE(trx == expectedTrx);
      }
    }

    GIVEN(
        "A BeginRepairsOperation with differing replicationFactors and "
        "rename=true") {
      BeginRepairsOperation operation{
          .database = "myDbName",
          .collectionId = "123456",
          .collectionName = "myCollection",
          .protoCollectionId = "789876",
          .protoCollectionName = "myProtoCollection",
          .collectionReplicationFactor = 2,
          .protoReplicationFactor = 5,
          .renameDistributeShardsLike = true,
      };

      WHEN("Converted into an AgencyTransaction") {
        AgencyWriteTransaction trx;
        boost::optional<uint64_t> jobid;
        std::tie(trx, jobid) = conversionVisitor(operation);

        REQUIRE_FALSE(jobid.is_initialized());

        VPackBufferPtr protoCollIdVpack = R"=("789876")="_vpack;
        Slice protoCollIdSlice = Slice(protoCollIdVpack->data());
        VPackBufferPtr replicationFactorVpack = R"=(5)="_vpack;
        Slice replicationFactorSlice = Slice(replicationFactorVpack->data());
        VPackBufferPtr prevReplicationFactorVpack = R"=(2)="_vpack;
        Slice prevReplicationFactorSlice =
            Slice(prevReplicationFactorVpack->data());

        AgencyWriteTransaction expectedTrx{
            std::vector<AgencyOperation>{
                AgencyOperation{
                    "Plan/Collections/myDbName/123456/distributeShardsLike",
                    AgencySimpleOperationType::DELETE_OP,
                },
                AgencyOperation{
                    "Plan/Collections/myDbName/123456/"
                    "repairingDistributeShardsLike",
                    AgencyValueOperationType::SET, protoCollIdSlice,
                },
                AgencyOperation{
                    "Plan/Collections/myDbName/123456/replicationFactor",
                    AgencyValueOperationType::SET, replicationFactorSlice,
                },
            },
            std::vector<AgencyPrecondition>{
                AgencyPrecondition{
                    "Plan/Collections/myDbName/123456/"
                    "repairingDistributeShardsLike",
                    AgencyPrecondition::Type::EMPTY, true,
                },
                AgencyPrecondition{
                    "Plan/Collections/myDbName/123456/distributeShardsLike",
                    AgencyPrecondition::Type::VALUE, protoCollIdSlice,
                },
                AgencyPrecondition{
                    "Plan/Collections/myDbName/123456/replicationFactor",
                    AgencyPrecondition::Type::VALUE, prevReplicationFactorSlice,
                },
                AgencyPrecondition{
                    "Plan/Collections/myDbName/789876/replicationFactor",
                    AgencyPrecondition::Type::VALUE, replicationFactorSlice,
                },
            }};

        trx.clientId = expectedTrx.clientId = "dummy-client-id";

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
          .shards =
              {
                  std::make_tuple<ShardID, ShardID, DBServers>(
                      "shard1", "protoShard1", {"dbServer1", "dbServer2"}),
                  std::make_tuple<ShardID, ShardID, DBServers>(
                      "shard2", "protoShard2", {"dbServer2", "dbServer3"}),
              },
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

        VPackBufferPtr serverOrderVpack1 =
            R"=(["dbServer1", "dbServer2"])="_vpack;
        VPackBufferPtr serverOrderVpack2 =
            R"=(["dbServer2", "dbServer3"])="_vpack;
        Slice serverOrderSlice1 = Slice(serverOrderVpack1->data());
        Slice serverOrderSlice2 = Slice(serverOrderVpack2->data());

        AgencyWriteTransaction expectedTrx{
            std::vector<AgencyOperation>{
                AgencyOperation{
                    "Plan/Collections/myDbName/123456/"
                    "repairingDistributeShardsLike",
                    AgencySimpleOperationType::DELETE_OP,
                },
                AgencyOperation{
                    "Plan/Collections/myDbName/123456/distributeShardsLike",
                    AgencyValueOperationType::SET, protoIdSlice,
                },
            },
            std::vector<AgencyPrecondition>{
                AgencyPrecondition{
                    "Plan/Collections/myDbName/123456/distributeShardsLike",
                    AgencyPrecondition::Type::EMPTY, true,
                },
                AgencyPrecondition{
                    "Plan/Collections/myDbName/123456/"
                    "repairingDistributeShardsLike",
                    AgencyPrecondition::Type::VALUE, protoIdSlice,
                },
                AgencyPrecondition{
                    "Plan/Collections/myDbName/123456/replicationFactor",
                    AgencyPrecondition::Type::VALUE, replicationFactorSlice,
                },
                AgencyPrecondition{
                    "Plan/Collections/myDbName/789876/replicationFactor",
                    AgencyPrecondition::Type::VALUE, replicationFactorSlice,
                },
                AgencyPrecondition{
                    "Plan/Collections/myDbName/123456/shards/shard1",
                    AgencyPrecondition::Type::VALUE, serverOrderSlice1,
                },
                AgencyPrecondition{
                    "Plan/Collections/myDbName/789876/shards/protoShard1",
                    AgencyPrecondition::Type::VALUE, serverOrderSlice1,
                },
                AgencyPrecondition{
                    "Plan/Collections/myDbName/123456/shards/shard2",
                    AgencyPrecondition::Type::VALUE, serverOrderSlice2,
                },
                AgencyPrecondition{
                    "Plan/Collections/myDbName/789876/shards/protoShard2",
                    AgencyPrecondition::Type::VALUE, serverOrderSlice2,
                },
            },
        };

        trx.clientId = expectedTrx.clientId = "dummy-client-id";

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
        (other = operation).protoCollectionName =
            "differing protoCollectionName";
        REQUIRE_FALSE(operation == other);
        (other = operation).shards = {
            std::make_tuple<ShardID, ShardID, DBServers>("differing", "shards",
                                                         {"vector"})};
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
        auto jobIdGenerator = [&nextJobId]() { return nextJobId++; };
        auto jobCreationTimestampGenerator = []() {
          std::tm tm = {};
          tm.tm_year = 2018 - 1900;  // years since 1900
          tm.tm_mon = 3 - 1;         // March, counted from january
          tm.tm_mday = 7;
          tm.tm_hour = 15;
          tm.tm_min = 20;
          tm.tm_sec = 1;
          tm.tm_isdst = 0;

          std::chrono::system_clock::time_point tp =
              std::chrono::system_clock::from_time_t(TRI_timegm(&tm));

          return tp;
        };

        conversionVisitor = RepairOperationToTransactionVisitor(
            jobIdGenerator, jobCreationTimestampGenerator);

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

        AgencyWriteTransaction expectedTrx{
            AgencyOperation{
                "Target/ToDo/" + std::to_string(jobId.get()),
                AgencyValueOperationType::SET, todoSlice,
            },
            AgencyPrecondition{
                "Target/ToDo/" + std::to_string(jobId.get()),
                AgencyPrecondition::Type::EMPTY, true,
            },
        };

        trx.clientId = expectedTrx.clientId = "dummy-client-id";

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
        (other = operation).isLeader = !operation.isLeader;
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
          .followers =
              {
                  "db-follower-3-server", "db-follower-2-server",
                  "db-follower-4-server", "db-follower-1-server",
              },
          .protoFollowers =
              {
                  "db-follower-1-server", "db-follower-2-server",
                  "db-follower-3-server", "db-follower-4-server",
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
        Slice previousServerOrderSlice =
            Slice(previousServerOrderVpack->data());
        Slice correctServerOrderSlice = Slice(correctServerOrderVpack->data());

        AgencyWriteTransaction trx;
        boost::optional<uint64_t> jobid;
        std::tie(trx, jobid) = conversionVisitor(operation);

        REQUIRE_FALSE(jobid.is_initialized());

        AgencyWriteTransaction expectedTrx{
            AgencyOperation{
                "Plan/Collections/myDbName/123456/shards/s1",
                AgencyValueOperationType::SET, correctServerOrderSlice,
            },
            {
                AgencyPrecondition{
                    "Plan/Collections/myDbName/123456/shards/s1",
                    AgencyPrecondition::Type::VALUE, previousServerOrderSlice,
                },
                AgencyPrecondition{
                    "Plan/Collections/myDbName/789876/shards/s7",
                    AgencyPrecondition::Type::VALUE, correctServerOrderSlice,
                },
            },
        };

        trx.clientId = expectedTrx.clientId = "dummy-client-id";

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
        (other = operation).protoCollectionName =
            "differing protoCollectionName";
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

  } catch (...) {
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
