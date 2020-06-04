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

#include <iostream>
#include <typeinfo>

#include "gtest/gtest.h"

#include <boost/core/demangle.hpp>
#include <boost/date_time.hpp>
#include <boost/range/combine.hpp>

#include "Mocks/LogLevels.h"

#include "Agency/AddFollower.h"
#include "Agency/FailedLeader.h"
#include "Agency/MoveShard.h"
#include "ApplicationFeatures/ApplicationFeature.h"
#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/ScopeGuard.h"
#include "Cluster/ClusterRepairs.h"
#include "Cluster/ResultT.h"
#include "Cluster/ServerState.h"
#include "Random/RandomGenerator.h"

using namespace arangodb;
using namespace arangodb::consensus;
using namespace arangodb::cluster_repairs;

namespace arangodb {

bool operator==(AgencyWriteTransaction const& left, AgencyWriteTransaction const& right) {
  VPackBuilder leftBuilder;
  VPackBuilder rightBuilder;

  left.toVelocyPack(leftBuilder);
  right.toVelocyPack(rightBuilder);

  return velocypack::NormalizedCompare::equals(leftBuilder.slice(), rightBuilder.slice());
}

std::ostream& operator<<(std::ostream& ostream, AgencyWriteTransaction const& trx) {
  Options optPretty = VPackOptions::Defaults;
  optPretty.prettyPrint = true;

  VPackBuilder builder;

  trx.toVelocyPack(builder);

  ostream << builder.slice().toJson(&optPretty);

  return ostream;
}

std::ostream& operator<<(std::ostream& ostream, std::list<RepairOperation> const& list) {
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

std::ostream& operator<<(std::ostream& ostream, std::vector<RepairOperation> const& vector) {
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
  // clang-format off
  std::string const typeName =
      typeid(T) == typeid(RepairOperation) ?
        "RepairOperation" :
      typeid(T) == typeid(std::list<RepairOperation>) ?
        "std::list<RepairOperation>" :
      typeid(T) == typeid(std::vector<RepairOperation>) ?
        "std::vector<RepairOperation>" :
      boost::core::demangle(typeid(T).name());
  // clang-format on

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

class ClusterRepairsTest : public ::testing::Test {
 protected:
  void checkAgainstExpectedOperations(
      VPackBufferPtr const& planCollections, VPackBufferPtr const& supervisionHealth,
      std::map<CollectionID, ResultT<std::vector<RepairOperation>>> expectedRepairOperationsByCollection) {
    ResultT<std::map<CollectionID, ResultT<std::list<RepairOperation>>>> repairOperationsByCollectionResult =
        DistributeShardsLikeRepairer::repairDistributeShardsLike(
            VPackSlice(planCollections->data()), VPackSlice(supervisionHealth->data()));

    ASSERT_TRUE(repairOperationsByCollectionResult.ok());
    std::map<CollectionID, ResultT<std::list<RepairOperation>>>& repairOperationsByCollection =
        repairOperationsByCollectionResult.get();

    {
      std::stringstream expectedOperationsStringStream;
      expectedOperationsStringStream << "{" << std::endl;
      for (auto const& it : expectedRepairOperationsByCollection) {
        std::string const& collection = it.first;
        ResultT<std::vector<RepairOperation>> const& expectedRepairResult = it.second;

        expectedOperationsStringStream << "\"" << collection << "\": \n";
        expectedOperationsStringStream << expectedRepairResult << std::endl;
      }
      expectedOperationsStringStream << "}";

      std::stringstream repairOperationsStringStream;
      repairOperationsStringStream << "{" << std::endl;
      for (auto const& it : repairOperationsByCollection) {
        std::string const& collection = it.first;
        ResultT<std::list<RepairOperation>> const repairOperationsResult = it.second;
        repairOperationsStringStream << "\"" << collection << "\": \n";
        repairOperationsStringStream << repairOperationsResult << std::endl;
      }
      repairOperationsStringStream << "}";

      ASSERT_TRUE(repairOperationsByCollection.size() ==
                  expectedRepairOperationsByCollection.size());
      for (auto const& it : boost::combine(expectedRepairOperationsByCollection,
                                           repairOperationsByCollection)) {
        auto const& expectedResult = it.get<0>().second;
        auto const& actualResult = it.get<1>().second;

        ASSERT_EQ(expectedResult.ok(), actualResult.ok());
        if (expectedResult.ok()) {
          ASSERT_EQ(expectedResult.get().size(), actualResult.get().size());
        }
      }
    }

    for (auto const& it : boost::combine(repairOperationsByCollection,
                                         expectedRepairOperationsByCollection)) {
      std::string const& collection = it.get<0>().first;
      ResultT<std::list<RepairOperation>> const repairResult = it.get<0>().second;

      std::string const& expectedCollection = it.get<1>().first;
      ResultT<std::vector<RepairOperation>> const& expectedResult = it.get<1>().second;

      ASSERT_EQ(collection, expectedCollection);
      ASSERT_EQ(repairResult.ok(), expectedResult.ok());
      if (expectedResult.ok()) {
        std::list<RepairOperation> const& repairOperations = repairResult.get();
        std::vector<RepairOperation> const& expectedOperations = expectedResult.get();

        for (auto const& it : boost::combine(repairOperations, expectedOperations)) {
          auto const& repairOpIt = it.get<0>();
          auto const& expectedRepairOpIt = it.get<1>();

          ASSERT_EQ(repairOpIt, expectedRepairOpIt);
        }
      } else {
        ASSERT_EQ(repairResult, expectedResult);
      }
    }
  }
};

class ClusterRepairsTestBrokenDistribution
    : public ClusterRepairsTest,
      public tests::LogSuppressor<Logger::CLUSTER, LogLevel::FATAL>,
      public tests::LogSuppressor<Logger::FIXME, LogLevel::FATAL> {
};

TEST_F(ClusterRepairsTestBrokenDistribution,
       an_agency_where_on_two_shards_the_dbservers_are_swapped_one_unused_dbserver_is_free_to_exchange_the_leader) {
#include "ClusterRepairsTest.swapWithLeader.cpp"

  checkAgainstExpectedOperations(planCollections, supervisionHealth3Healthy0Bad,
                                 expectedResultsWithTwoSwappedDBServers);
}

TEST_F(ClusterRepairsTestBrokenDistribution,
       an_agency_where_on_two_shards_the_dbservers_are_swapped_the_unused_dbserver_is_marked_as_nonhealthy) {
#include "ClusterRepairsTest.unusedServerUnhealthy.cpp"

  auto result = DistributeShardsLikeRepairer::repairDistributeShardsLike(
      VPackSlice(planCollections->data()),
      VPackSlice(supervisionHealth2Healthy1Bad->data()));

  ASSERT_TRUE(result.ok());
  std::map<CollectionID, ResultT<std::list<RepairOperation>>> operationResultByCollectionId =
      result.get();
  ASSERT_EQ(operationResultByCollectionId.size(), 1);
  ASSERT_TRUE(operationResultByCollectionId.find("11111111") !=
              operationResultByCollectionId.end());
  ResultT<std::list<RepairOperation>> collectionResult =
      operationResultByCollectionId.at("11111111");

  ASSERT_EQ(collectionResult.errorNumber(), TRI_ERROR_CLUSTER_REPAIRS_NOT_ENOUGH_HEALTHY);
  ASSERT_TRUE(0 == strcmp(TRI_errno_string(collectionResult.errorNumber()),
                          "not enough (healthy) db servers"));
  ASSERT_TRUE(collectionResult.fail());
}

TEST_F(ClusterRepairsTestBrokenDistribution,
       an_agency_where_on_two_shards_the_dbservers_are_swapped_the_replicationfactor_equals_the_number_of_dbservers) {
#include "ClusterRepairsTest.replicationFactorTooHigh.cpp"

  auto result = DistributeShardsLikeRepairer::repairDistributeShardsLike(
      VPackSlice(planCollections->data()),
      VPackSlice(supervisionHealth2Healthy0Bad->data()));

  ASSERT_TRUE(result.ok());
  std::map<CollectionID, ResultT<std::list<RepairOperation>>> operationResultByCollectionId =
      result.get();
  ASSERT_EQ(operationResultByCollectionId.size(), 1);
  ASSERT_TRUE(operationResultByCollectionId.find("11111111") !=
              operationResultByCollectionId.end());
  ResultT<std::list<RepairOperation>> collectionResult =
      operationResultByCollectionId.at("11111111");

  ASSERT_EQ(collectionResult.errorNumber(), TRI_ERROR_CLUSTER_REPAIRS_NOT_ENOUGH_HEALTHY);
  ASSERT_TRUE(0 == strcmp(TRI_errno_string(collectionResult.errorNumber()),
                          "not enough (healthy) db servers"));
  ASSERT_TRUE(collectionResult.fail());
}

TEST_F(ClusterRepairsTestBrokenDistribution,
       an_agency_where_differently_ordered_followers_have_to_be_moved) {
#include "ClusterRepairsTest.moveFollower.cpp"
  // This test should ensure that the (internal) order in the repairer
  // after a shard move resembles the one after a real shard move.
  // i.e., moving a follower puts it to the end of the list, e.g., given
  // [a, b, c, d] (where a is the leader), moving b to e results in
  // [a, c, d, e] rather than [a, e, c, d].
  checkAgainstExpectedOperations(planCollections, supervisionHealth4Healthy0Bad,
                                 expectedResultsWithFollowerOrder);
}

TEST_F(ClusterRepairsTestBrokenDistribution,
       an_agency_where_a_follower_shard_has_erroneously_ordered_dbservers) {
#include "ClusterRepairsTest.unorderedFollowers.cpp"
  checkAgainstExpectedOperations(planCollections, supervisionHealth4Healthy0Bad,
                                 expectedResultsWithWronglyOrderedFollowers);
}

TEST_F(ClusterRepairsTestBrokenDistribution,
       an_agency_where_a_collection_has_repairing_distributshardslike_but_nothing_else_is_broken) {
#include "ClusterRepairsTest.repairingDistributeShardsLike.cpp"
  checkAgainstExpectedOperations(planCollections, supervisionHealth4Healthy0Bad,
                                 expectedResultsWithRepairingDistributeShardsLike);
}

TEST_F(ClusterRepairsTestBrokenDistribution,
       an_agency_where_a_collection_has_repairing_distributshardslike_but_the_replicationfactor_differs) {
#include "ClusterRepairsTest.repairingDslChangedRf.cpp"
  checkAgainstExpectedOperations(planCollections, supervisionHealth4Healthy0Bad,
                                 expectedResultsWithRepairingDistributeShardsLike);
}

TEST_F(ClusterRepairsTestBrokenDistribution, an_agency_with_multiple_collections) {
#include "ClusterRepairsTest.multipleCollections.cpp"
  checkAgainstExpectedOperations(planCollections, supervisionHealth4Healthy0Bad,
                                 expectedResultsWithMultipleCollections);
}

TEST_F(ClusterRepairsTestBrokenDistribution, a_collection_with_multiple_shards) {
#include "ClusterRepairsTest.multipleShards.cpp"
  checkAgainstExpectedOperations(planCollections, supervisionHealth3Healthy0Bad,
                                 expectedResultsWithMultipleShards);
}

TEST_F(ClusterRepairsTestBrokenDistribution,
       a_collection_where_the_replicationfactor_doesnt_conform_with_its_prototype) {
#include "ClusterRepairsTest.unequalReplicationFactor.cpp"
  checkAgainstExpectedOperations(planCollections, supervisionHealth3Healthy0Bad,
                                 expectedResultsWithUnequalReplicationFactor);
}

TEST_F(ClusterRepairsTestBrokenDistribution, a_smart_graph_with_some_broken_collections) {
#include "ClusterRepairsTest.smartCollections.cpp"
  checkAgainstExpectedOperations(planCollections, supervisionHealth3Healthy0Bad,
                                 expectedResultsWithSmartGraph);
}

TEST_F(ClusterRepairsTestBrokenDistribution, a_satellite_collection) {
#include "ClusterRepairsTest.satelliteCollection.cpp"
  checkAgainstExpectedOperations(planCollections, supervisionHealth3Healthy0Bad,
                                 expectedResultsWithSatelliteCollection);
}

TEST_F(ClusterRepairsTestBrokenDistribution,
       a_collection_that_should_usually_be_fixed_but_is_deleted) {
#include "ClusterRepairsTest.deletedCollection.cpp"
  checkAgainstExpectedOperations(planCollections, supervisionHealth3Healthy0Bad,
                                 expectedResultsWithDeletedCollection);
}

#ifdef ARANGODB_ENABLE_FAILURE_TESTS
TEST_F(ClusterRepairsTestBrokenDistribution, collections_with_triggered_failures) {
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
    checkAgainstExpectedOperations(planCollections, supervisionHealth2Healthy0Bad,
                                   expectedResultsWithTriggeredFailures);
    TRI_ClearFailurePointsDebugging();
  } catch (...) {
    TRI_ClearFailurePointsDebugging();
    throw;
  }
}
#endif

TEST(ClusterRepairsTestVersionSort, different_version_strings) {
  // Disable cluster logging
  arangodb::tests::LogSuppressor<Logger::CLUSTER, LogLevel::FATAL> suppressor;

  // General functionality check
  EXPECT_TRUE(VersionSort()("s2", "s10"));
  EXPECT_FALSE(VersionSort()("s10", "s2"));

  EXPECT_TRUE(VersionSort()("s5", "s7"));
  EXPECT_FALSE(VersionSort()("s7", "s5"));

  // Make sure sorting by the last char works
  EXPECT_TRUE(VersionSort()("s100a", "s0100b"));
  EXPECT_FALSE(VersionSort()("s0100b", "s100a"));

  // Make sure the ints aren't casted into signed chars and overflow
  EXPECT_TRUE(VersionSort()("s126", "s129"));
  EXPECT_FALSE(VersionSort()("s129", "s126"));

  // Make sure the ints aren't casted into unsigned chars and overflow
  EXPECT_TRUE(VersionSort()("s254", "s257"));
  EXPECT_FALSE(VersionSort()("s257", "s254"));

  // Regression test
  EXPECT_TRUE(VersionSort()("s1000057", "s1000065"));
  EXPECT_FALSE(VersionSort()("s1000065", "s1000057"));

  EXPECT_TRUE(VersionSort()("s1000050", "s1000064"));
  EXPECT_FALSE(VersionSort()("s1000064", "s1000050"));
}

class ClusterRepairsTestOperations
    : public ClusterRepairsTest,
      public tests::LogSuppressor<Logger::CLUSTER, LogLevel::FATAL> {
 protected:
  std::string const oldServerId;
  RepairOperationToTransactionVisitor conversionVisitor;
  arangodb::application_features::ApplicationServer server;

  static uint64_t mockJobIdGenerator() {
    EXPECT_TRUE(false);
    return 0ul;
  }

  static std::chrono::system_clock::time_point mockJobCreationTimestampGenerator() {
    EXPECT_TRUE(false);
    return std::chrono::system_clock::now();
  }

  ClusterRepairsTestOperations() :
        oldServerId(ServerState::instance()->getId()),
        conversionVisitor(mockJobIdGenerator, mockJobCreationTimestampGenerator),
        server{nullptr, nullptr} {}

  ~ClusterRepairsTestOperations() = default;
};

TEST_F(ClusterRepairsTestOperations,
       a_beginrepairsoperation_with_equal_replicationfactors_and_rename_true_converted_into_an_agencytransaction) {
  BeginRepairsOperation operation{_database = "myDbName",
                                  _collectionId = "123456",
                                  _collectionName = "myCollection",
                                  _protoCollectionId = "789876",
                                  _protoCollectionName = "myProtoCollection",
                                  _collectionReplicationFactor = 3,
                                  _protoReplicationFactor = 3,
                                  _renameDistributeShardsLike = true};
  auto [trx, jobid] = conversionVisitor(operation);

  ASSERT_FALSE(jobid.has_value());

  VPackBufferPtr protoCollIdVPack = R"=("789876")="_vpack;
  Slice protoCollIdSlice = Slice(protoCollIdVPack->data());
  VPackBufferPtr replicationFactorVPack = R"=(3)="_vpack;
  Slice replicationFactorSlice = Slice(replicationFactorVPack->data());

  AgencyWriteTransaction expectedTrx{
      std::vector<AgencyOperation>{
          AgencyOperation{
              "Plan/Collections/myDbName/123456/distributeShardsLike",
              AgencySimpleOperationType::DELETE_OP},
          AgencyOperation{"Plan/Collections/myDbName/123456/"
                          "repairingDistributeShardsLike",
                          AgencyValueOperationType::SET, protoCollIdSlice},
          AgencyOperation{"Plan/Collections/myDbName/123456/replicationFactor",
                          AgencyValueOperationType::SET, replicationFactorSlice},
          AgencyOperation{"Plan/Version", AgencySimpleOperationType::INCREMENT_OP}},
      std::vector<AgencyPrecondition>{
          AgencyPrecondition{"Plan/Collections/myDbName/123456/"
                             "repairingDistributeShardsLike",
                             AgencyPrecondition::Type::EMPTY, true},
          AgencyPrecondition{
              "Plan/Collections/myDbName/123456/distributeShardsLike",
              AgencyPrecondition::Type::VALUE, protoCollIdSlice},
          AgencyPrecondition{
              "Plan/Collections/myDbName/123456/replicationFactor",
              AgencyPrecondition::Type::VALUE, replicationFactorSlice},
          AgencyPrecondition{
              "Plan/Collections/myDbName/789876/replicationFactor",
              AgencyPrecondition::Type::VALUE, replicationFactorSlice}}};

  trx.clientId = expectedTrx.clientId = "dummy-client-id";

  ASSERT_EQ(trx, expectedTrx);
}

TEST_F(ClusterRepairsTestOperations,
       a_beginrepairsoperation_with_equal_replicationfactors_and_rename_true_compared_via_eqeq) {
  BeginRepairsOperation operation{_database = "myDbName",
                                  _collectionId = "123456",
                                  _collectionName = "myCollection",
                                  _protoCollectionId = "789876",
                                  _protoCollectionName = "myProtoCollection",
                                  _collectionReplicationFactor = 3,
                                  _protoReplicationFactor = 3,
                                  _renameDistributeShardsLike = true};
  BeginRepairsOperation other = operation;

  ASSERT_EQ(operation, other);

  (other = operation).database = "differing database";
  ASSERT_FALSE(operation == other);
  (other = operation).collectionId = "differing collectionId";
  ASSERT_FALSE(operation == other);
  (other = operation).collectionName = "differing collectionName";
  ASSERT_FALSE(operation == other);
  (other = operation).protoCollectionId = "differing protoCollectionId";
  ASSERT_FALSE(operation == other);
  (other = operation).protoCollectionName = "differing protoCollectionName";
  ASSERT_FALSE(operation == other);
  (other = operation).collectionReplicationFactor = 42;
  ASSERT_FALSE(operation == other);
  (other = operation).protoReplicationFactor = 23;
  ASSERT_FALSE(operation == other);
  (other = operation).renameDistributeShardsLike = !operation.renameDistributeShardsLike;
  ASSERT_FALSE(operation == other);
}

TEST_F(ClusterRepairsTestOperations,
       a_beginrepairsoperation_with_differing_replicationfactors_and_rename_false_converted_into_an_agencytransaction) {
  BeginRepairsOperation operation{_database = "myDbName",
                                  _collectionId = "123456",
                                  _collectionName = "myCollection",
                                  _protoCollectionId = "789876",
                                  _protoCollectionName = "myProtoCollection",
                                  _collectionReplicationFactor = 5,
                                  _protoReplicationFactor = 4,
                                  _renameDistributeShardsLike = false};

  auto [trx, jobid] = conversionVisitor(operation);

  ASSERT_FALSE(jobid.has_value());

  VPackBufferPtr protoCollIdVPack = R"=("789876")="_vpack;
  Slice protoCollIdSlice = Slice(protoCollIdVPack->data());
  VPackBufferPtr replicationFactorVPack = R"=(4)="_vpack;
  Slice replicationFactorSlice = Slice(replicationFactorVPack->data());

  AgencyWriteTransaction expectedTrx{
      {AgencyOperation{"Plan/Version", AgencySimpleOperationType::INCREMENT_OP}},
      std::vector<AgencyPrecondition>{
          AgencyPrecondition{
              "Plan/Collections/myDbName/123456/distributeShardsLike",
              AgencyPrecondition::Type::EMPTY, true},
          AgencyPrecondition{"Plan/Collections/myDbName/123456/"
                             "repairingDistributeShardsLike",
                             AgencyPrecondition::Type::VALUE, protoCollIdSlice},
          AgencyPrecondition{
              "Plan/Collections/myDbName/123456/replicationFactor",
              AgencyPrecondition::Type::VALUE, replicationFactorSlice},
          AgencyPrecondition{
              "Plan/Collections/myDbName/789876/replicationFactor",
              AgencyPrecondition::Type::VALUE, replicationFactorSlice}}};

  trx.clientId = expectedTrx.clientId = "dummy-client-id";

  ASSERT_EQ(trx, expectedTrx);
}

TEST_F(ClusterRepairsTestOperations,
       a_beginrepairsoperation_with_differing_replicationfactors_and_rename_true_converted_into_an_agency_transaction) {
  BeginRepairsOperation operation{_database = "myDbName",
                                  _collectionId = "123456",
                                  _collectionName = "myCollection",
                                  _protoCollectionId = "789876",
                                  _protoCollectionName = "myProtoCollection",
                                  _collectionReplicationFactor = 2,
                                  _protoReplicationFactor = 5,
                                  _renameDistributeShardsLike = true};

  auto [trx, jobid] = conversionVisitor(operation);

  ASSERT_FALSE(jobid.has_value());

  VPackBufferPtr protoCollIdVPack = R"=("789876")="_vpack;
  Slice protoCollIdSlice = Slice(protoCollIdVPack->data());
  VPackBufferPtr replicationFactorVPack = R"=(5)="_vpack;
  Slice replicationFactorSlice = Slice(replicationFactorVPack->data());
  VPackBufferPtr prevReplicationFactorVPack = R"=(2)="_vpack;
  Slice prevReplicationFactorSlice = Slice(prevReplicationFactorVPack->data());

  AgencyWriteTransaction expectedTrx{
      std::vector<AgencyOperation>{
          AgencyOperation{
              "Plan/Collections/myDbName/123456/distributeShardsLike",
              AgencySimpleOperationType::DELETE_OP},
          AgencyOperation{"Plan/Collections/myDbName/123456/"
                          "repairingDistributeShardsLike",
                          AgencyValueOperationType::SET, protoCollIdSlice},
          AgencyOperation{"Plan/Collections/myDbName/123456/replicationFactor",
                          AgencyValueOperationType::SET, replicationFactorSlice},
          AgencyOperation{"Plan/Version", AgencySimpleOperationType::INCREMENT_OP}},
      std::vector<AgencyPrecondition>{
          AgencyPrecondition{"Plan/Collections/myDbName/123456/"
                             "repairingDistributeShardsLike",
                             AgencyPrecondition::Type::EMPTY, true},
          AgencyPrecondition{
              "Plan/Collections/myDbName/123456/distributeShardsLike",
              AgencyPrecondition::Type::VALUE, protoCollIdSlice},
          AgencyPrecondition{
              "Plan/Collections/myDbName/123456/replicationFactor",
              AgencyPrecondition::Type::VALUE, prevReplicationFactorSlice},
          AgencyPrecondition{
              "Plan/Collections/myDbName/789876/replicationFactor",
              AgencyPrecondition::Type::VALUE, replicationFactorSlice}}};

  trx.clientId = expectedTrx.clientId = "dummy-client-id";

  ASSERT_EQ(trx, expectedTrx);
}

TEST_F(ClusterRepairsTestOperations, a_finishrepairsoperation_converted_into_an_agencytransaction) {
  FinishRepairsOperation operation{
      _database = "myDbName",
      _collectionId = "123456",
      _collectionName = "myCollection",
      _protoCollectionId = "789876",
      _protoCollectionName = "myProtoCollection",
      _shards =
          std::vector<ShardWithProtoAndDbServers>{
              std::make_tuple<ShardID, ShardID, DBServers>(
                  "shard1", "protoShard1", {"dbServer1", "dbServer2"}),
              std::make_tuple<ShardID, ShardID, DBServers>(
                  "shard2", "protoShard2", {"dbServer2", "dbServer3"})},
      _replicationFactor = 3};

  auto [trx, jobid] = conversionVisitor(operation);

  ASSERT_FALSE(jobid.has_value());

  VPackBufferPtr protoIdVPack = R"=("789876")="_vpack;
  Slice protoIdSlice = Slice(protoIdVPack->data());
  VPackBufferPtr replicationFactorVPack = R"=(3)="_vpack;
  Slice replicationFactorSlice = Slice(replicationFactorVPack->data());

  VPackBufferPtr serverOrderVPack1 = R"=(["dbServer1", "dbServer2"])="_vpack;
  VPackBufferPtr serverOrderVPack2 = R"=(["dbServer2", "dbServer3"])="_vpack;
  Slice serverOrderSlice1 = Slice(serverOrderVPack1->data());
  Slice serverOrderSlice2 = Slice(serverOrderVPack2->data());

  AgencyWriteTransaction expectedTrx{
      std::vector<AgencyOperation>{
          AgencyOperation{"Plan/Collections/myDbName/123456/"
                          "repairingDistributeShardsLike",
                          AgencySimpleOperationType::DELETE_OP},
          AgencyOperation{
              "Plan/Collections/myDbName/123456/distributeShardsLike",
              AgencyValueOperationType::SET, protoIdSlice},
          AgencyOperation{"Plan/Version", AgencySimpleOperationType::INCREMENT_OP}},
      std::vector<AgencyPrecondition>{
          AgencyPrecondition{
              "Plan/Collections/myDbName/123456/distributeShardsLike",
              AgencyPrecondition::Type::EMPTY, true},
          AgencyPrecondition{"Plan/Collections/myDbName/123456/"
                             "repairingDistributeShardsLike",
                             AgencyPrecondition::Type::VALUE, protoIdSlice},
          AgencyPrecondition{
              "Plan/Collections/myDbName/123456/replicationFactor",
              AgencyPrecondition::Type::VALUE, replicationFactorSlice},
          AgencyPrecondition{
              "Plan/Collections/myDbName/789876/replicationFactor",
              AgencyPrecondition::Type::VALUE, replicationFactorSlice},
          AgencyPrecondition{"Plan/Collections/myDbName/123456/shards/shard1",
                             AgencyPrecondition::Type::VALUE, serverOrderSlice1},
          AgencyPrecondition{
              "Plan/Collections/myDbName/789876/shards/protoShard1",
              AgencyPrecondition::Type::VALUE, serverOrderSlice1},
          AgencyPrecondition{"Plan/Collections/myDbName/123456/shards/shard2",
                             AgencyPrecondition::Type::VALUE, serverOrderSlice2},
          AgencyPrecondition{
              "Plan/Collections/myDbName/789876/shards/protoShard2",
              AgencyPrecondition::Type::VALUE, serverOrderSlice2}}};

  trx.clientId = expectedTrx.clientId = "dummy-client-id";

  ASSERT_EQ(trx, expectedTrx);
}

TEST_F(ClusterRepairsTestOperations, a_finishrepairsoperation_compared_via_eqeq) {
  FinishRepairsOperation operation{
      _database = "myDbName",
      _collectionId = "123456",
      _collectionName = "myCollection",
      _protoCollectionId = "789876",
      _protoCollectionName = "myProtoCollection",
      _shards =
          std::vector<ShardWithProtoAndDbServers>{
              std::make_tuple<ShardID, ShardID, DBServers>(
                  "shard1", "protoShard1", {"dbServer1", "dbServer2"}),
              std::make_tuple<ShardID, ShardID, DBServers>(
                  "shard2", "protoShard2", {"dbServer2", "dbServer3"})},
      _replicationFactor = 3};

  FinishRepairsOperation other = operation;

  ASSERT_EQ(operation, other);

  (other = operation).database = "differing database";
  ASSERT_FALSE(operation == other);
  (other = operation).collectionId = "differing collectionId";
  ASSERT_FALSE(operation == other);
  (other = operation).collectionName = "differing collectionName";
  ASSERT_FALSE(operation == other);
  (other = operation).protoCollectionId = "differing protoCollectionId";
  ASSERT_FALSE(operation == other);
  (other = operation).protoCollectionName = "differing protoCollectionName";
  ASSERT_FALSE(operation == other);
  (other = operation).shards = {
      std::make_tuple<ShardID, ShardID, DBServers>("differing", "shards", {"vector"})};
  ASSERT_FALSE(operation == other);
  (other = operation).replicationFactor = 42;
  ASSERT_FALSE(operation == other);
}

TEST_F(ClusterRepairsTestOperations, a_moveshardoperation_converted_into_an_agencytransaction) {
  ServerState::instance()->setId("CurrentCoordinatorServerId");

  MoveShardOperation operation{_database = "myDbName",
                               _collectionId = "123456",
                               _collectionName = "myCollection",
                               _shard = "s1",
                               _from = "db-from-server",
                               _to = "db-to-server",
                               _isLeader = true};

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

  conversionVisitor =
      RepairOperationToTransactionVisitor(jobIdGenerator, jobCreationTimestampGenerator);

  auto [trx, jobId] = conversionVisitor(operation);

  ASSERT_TRUE(jobId.has_value());
  // "timeCreated": "2018-03-07T15:20:01.284Z",
  VPackBufferPtr todoVPack = R"=(
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
  Slice todoSlice = Slice(todoVPack->data());

  AgencyWriteTransaction expectedTrx{
      AgencyOperation{"Target/ToDo/" + std::to_string(jobId.value()),
                      AgencyValueOperationType::SET, todoSlice},
      AgencyPrecondition{"Target/ToDo/" + std::to_string(jobId.value()),
                         AgencyPrecondition::Type::EMPTY, true}};

  trx.clientId = expectedTrx.clientId = "dummy-client-id";

  ASSERT_EQ(trx, expectedTrx);
}

TEST_F(ClusterRepairsTestOperations, a_moveshardoperation_compared_via_eqeq) {
  ServerState::instance()->setId("CurrentCoordinatorServerId");

  MoveShardOperation operation{_database = "myDbName",
                               _collectionId = "123456",
                               _collectionName = "myCollection",
                               _shard = "s1",
                               _from = "db-from-server",
                               _to = "db-to-server",
                               _isLeader = true};

  MoveShardOperation other = operation;

  ASSERT_EQ(operation, other);

  (other = operation).database = "differing database";
  ASSERT_FALSE(operation == other);
  (other = operation).collectionId = "differing collectionId";
  ASSERT_FALSE(operation == other);
  (other = operation).collectionName = "differing collectionName";
  ASSERT_FALSE(operation == other);
  (other = operation).shard = "differing shard";
  ASSERT_FALSE(operation == other);
  (other = operation).from = "differing from";
  ASSERT_FALSE(operation == other);
  (other = operation).to = "differing to";
  ASSERT_FALSE(operation == other);
  (other = operation).isLeader = !operation.isLeader;
  ASSERT_FALSE(operation == other);
}

TEST_F(ClusterRepairsTestOperations,
       a_fixserverorderoperation_converted_into_an_agencytransaction) {
  FixServerOrderOperation operation{
      _database = "myDbName",
      _collectionId = "123456",
      _collectionName = "myCollection",
      _protoCollectionId = "789876",
      _protoCollectionName = "myProtoCollection",
      _shard = "s1",
      _protoShard = "s7",
      _leader = "db-leader-server",
      _followers = {"db-follower-3-server", "db-follower-2-server",
                    "db-follower-4-server", "db-follower-1-server"},
      _protoFollowers = {"db-follower-1-server", "db-follower-2-server",
                         "db-follower-3-server", "db-follower-4-server"}};

  VPackBufferPtr previousServerOrderVPack = R"=([
          "db-leader-server",
          "db-follower-3-server",
          "db-follower-2-server",
          "db-follower-4-server",
          "db-follower-1-server"
        ])="_vpack;
  VPackBufferPtr correctServerOrderVPack = R"=([
          "db-leader-server",
          "db-follower-1-server",
          "db-follower-2-server",
          "db-follower-3-server",
          "db-follower-4-server"
        ])="_vpack;
  Slice previousServerOrderSlice = Slice(previousServerOrderVPack->data());
  Slice correctServerOrderSlice = Slice(correctServerOrderVPack->data());

  auto [trx, jobId] = conversionVisitor(operation);

  ASSERT_FALSE(jobId.has_value());

  AgencyWriteTransaction expectedTrx{
      AgencyOperation{"Plan/Collections/myDbName/123456/shards/s1",
                      AgencyValueOperationType::SET, correctServerOrderSlice},
      {AgencyPrecondition{"Plan/Collections/myDbName/123456/shards/s1",
                          AgencyPrecondition::Type::VALUE, previousServerOrderSlice},
       AgencyPrecondition{"Plan/Collections/myDbName/789876/shards/s7",
                          AgencyPrecondition::Type::VALUE, correctServerOrderSlice}}};

  trx.clientId = expectedTrx.clientId = "dummy-client-id";

  ASSERT_EQ(trx, expectedTrx);
}

TEST_F(ClusterRepairsTestOperations, a_fixserverorderoperation_compared_via_eqeq) {
  FixServerOrderOperation operation{
      _database = "myDbName",
      _collectionId = "123456",
      _collectionName = "myCollection",
      _protoCollectionId = "789876",
      _protoCollectionName = "myProtoCollection",
      _shard = "s1",
      _protoShard = "s7",
      _leader = "db-leader-server",
      _followers = {"db-follower-3-server", "db-follower-2-server",
                    "db-follower-4-server", "db-follower-1-server"},
      _protoFollowers = {"db-follower-1-server", "db-follower-2-server",
                         "db-follower-3-server", "db-follower-4-server"}};

  FixServerOrderOperation other = operation;

  ASSERT_EQ(operation, other);

  (other = operation).database = "differing database";
  ASSERT_FALSE(operation == other);
  (other = operation).collectionId = "differing collectionId";
  ASSERT_FALSE(operation == other);
  (other = operation).collectionName = "differing collectionName";
  ASSERT_FALSE(operation == other);
  (other = operation).protoCollectionId = "differing protoCollectionId";
  ASSERT_FALSE(operation == other);
  (other = operation).protoCollectionName = "differing protoCollectionName";
  ASSERT_FALSE(operation == other);
  (other = operation).shard = "differing shard";
  ASSERT_FALSE(operation == other);
  (other = operation).protoShard = "differing protoShard";
  ASSERT_FALSE(operation == other);
  (other = operation).leader = "differing leader";
  ASSERT_FALSE(operation == other);
  (other = operation).followers = {"differing", "followers"};
  ASSERT_FALSE(operation == other);
  (other = operation).protoFollowers = {"differing", "protoFollowers"};
  ASSERT_FALSE(operation == other);
}

}  // namespace cluster_repairs_test
}  // namespace tests
}  // namespace arangodb
