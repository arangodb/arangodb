////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Copyright 2017, ArangoDB GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "gtest/gtest.h"

#include "fakeit.hpp"

#include "Aql/AqlItemBlock.h"
#include "Aql/EngineInfoContainerCoordinator.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode.h"
#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"
#include "Cluster/RebootTracker.h"
#include "Mocks/Servers.h"
#include "Transaction/Methods.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace engine_info_container_coordinator_test {

TEST(EngineInfoContainerTest, it_should_always_start_with_an_open_snippet) {
  EngineInfoContainerCoordinator testee;
  QueryId res = testee.closeSnippet();
  ASSERT_EQ(res, 0);
}

TEST(EngineInfoContainerTest, it_should_be_able_to_add_more_snippets) {
  EngineInfoContainerCoordinator testee;

  auto remote = ExecutionNodeId{1};
  testee.openSnippet(remote);
  testee.openSnippet(remote);

  QueryId res1 = testee.closeSnippet();
  ASSERT_NE(res1, 0);

  QueryId res2 = testee.closeSnippet();
  ASSERT_NE(res2, res1);
  ASSERT_NE(res2, 0);

  QueryId res3 = testee.closeSnippet();
  ASSERT_EQ(res3, 0);
}

///////////////////////////////////////////
// SECTION buildEngines
///////////////////////////////////////////

// Flow:
// 1. Clone the query for every snippet but the first
// 2. For every snippet:
//   1. create new Engine (e)
//   2. query->setEngine(e)
//   3. query->engine() -> e
//   5. engine->createBlocks()
//   6. Assert (engine->root() != nullptr)
//   7. For all but the first:
//     1. queryRegistry->insert(_id, query, 600.0);
// 3. query->engine();

TEST(EngineInfoContainerTest, it_should_create_an_executionengine_for_the_first_snippet) {
  MapRemoteToSnippet queryIds;
  std::string const dbname = "TestDB";
  
  mocks::MockAqlServer server;

  // ------------------------------
  // Section: Create Mock Instances
  // ------------------------------
//  fakeit::Mock<ExecutionNode> singletonMock;
//  ExecutionNode& sNode = singletonMock.get();
//  fakeit::When(Method(singletonMock, getType)).AlwaysReturn(ExecutionNode::SINGLETON);
//
//  fakeit::Mock<ExecutionEngine> mockEngine;
//  // ExecutionEngine& myEngine = mockEngine.get();
//
//  fakeit::Mock<ExecutionBlock> rootBlockMock;
//  ExecutionBlock& rootBlock = rootBlockMock.get();
//
//  fakeit::Mock<Query> mockQuery;
//  Query& query = mockQuery.get();
//
//  fakeit::Mock<transaction::Methods> mockTrx;
//   transaction::Methods& trx = mockTrx.get();

  // ------------------------------
  // Section: Mock Functions
  // ------------------------------
/*
  fakeit::When(OverloadedMethod(mockQuery, void(ExecutionEngine * ))).Do(
    [](ExecutionEngine *eng) -> void {
      // We expect that the snippet injects a new engine into our
      // query.
      // However we have to return a mocked engine later
      ASSERT_NE(eng, nullptr);
      // Throw it away
      delete eng;
    }
  );
  //fakeit::When(Method(mockQuery, trx)).Return(&trx);
  /// fakeit::When(Method(mockQuery, engine)).Return(&myEngine).Return(&myEngine);
  fakeit::When(Method(mockEngine, createBlocks)).Return(Result{TRI_ERROR_NO_ERROR});
  fakeit::When(ConstOverloadedMethod(mockEngine, root, ExecutionBlock * ())).AlwaysReturn(&rootBlock);
 */

  // ------------------------------
  // Section: Run the test
  // ------------------------------
  
  auto oldRole = ServerState::instance()->getRole();
  ServerState::instance()->setRole(ServerState::RoleEnum::ROLE_COORDINATOR);
  auto guard = scopeGuard([=] {
    ServerState::instance()->setRole(oldRole);
  });
  
  // simon: we only use this query for the API
  auto q = server.createFakeQuery("RETURN 1");
  ASSERT_EQ(q->rootEngine()->blocksForTesting().size(), 3);
  
  ExecutionBlock* block = q->rootEngine()->blocksForTesting()[2].get();
  ASSERT_EQ(block->getPlanNode()->getType(), ExecutionNode::RETURN);
  
  ASSERT_EQ(q->snippets().size(), 1);
  
  // The last engine should not be stored
  // It is not added to the registry
  ASSERT_TRUE(queryIds.empty());
  
}

#if 0
TEST(EngineInfoContainerTest,
     it_should_create_a_new_engine_and_register_it_for_the_second_snippet) {
  std::unordered_set<std::string> const restrictToShards;
  MapRemoteToSnippet queryIds;

  auto const remoteId = ExecutionNodeId{1337};
  QueryId secondId = 0;
  std::string dbname = "TestDB";

  // ------------------------------
  // Section: Create Mock Instances
  // ------------------------------
  fakeit::Mock<ExecutionNode> firstNodeMock;
  ExecutionNode& fNode = firstNodeMock.get();
  fakeit::When(Method(firstNodeMock, getType)).AlwaysReturn(ExecutionNode::SINGLETON);

  fakeit::Mock<ExecutionNode> secondNodeMock;
  ExecutionNode& sNode = secondNodeMock.get();
  fakeit::When(Method(secondNodeMock, getType)).AlwaysReturn(ExecutionNode::SINGLETON);

  // We need a block only for assertion
  fakeit::Mock<ExecutionBlock> blockMock;
  ExecutionBlock& block = blockMock.get();

  // Mock engine for first snippet
  fakeit::Mock<ExecutionEngine> mockEngine;
  // ExecutionEngine& myEngine = mockEngine.get();

  // Mock engine for second snippet
  fakeit::Mock<ExecutionEngine> mockSecondEngine;
  //ExecutionEngine& mySecondEngine = mockSecondEngine.get();

  fakeit::Mock<QueryRegistry> mockRegistry;
  fakeit::When(Method(mockRegistry, defaultTTL)).AlwaysReturn(600.0);
  //  QueryRegistry& registry = mockRegistry.get();

  fakeit::Mock<QueryOptions> mockQueryOptions;
  QueryOptions& lqueryOptions = mockQueryOptions.get();
  lqueryOptions.ttl = 600;

  fakeit::Mock<Query> mockQuery;
  Query& query = mockQuery.get();
  fakeit::When(ConstOverloadedMethod(mockQuery, queryOptions, QueryOptions const&()))
      .AlwaysDo([&]() -> QueryOptions const& { return lqueryOptions; });
  fakeit::When(OverloadedMethod(mockQuery, queryOptions, QueryOptions & ()))
      .AlwaysDo([&]() -> QueryOptions& { return lqueryOptions; });

  fakeit::Mock<Query> mockQueryClone;
  //Query& queryClone = mockQueryClone.get();
  fakeit::When(ConstOverloadedMethod(mockQueryClone, queryOptions, QueryOptions const&()))
      .AlwaysDo([&]() -> QueryOptions const& { return lqueryOptions; });
  fakeit::When(OverloadedMethod(mockQueryClone, queryOptions, QueryOptions & ()))
      .AlwaysDo([&]() -> QueryOptions& { return lqueryOptions; });

  fakeit::Mock<transaction::Methods> mockTrx;
  //transaction::Methods& trx = mockTrx.get();

  fakeit::Mock<transaction::Methods> mockSecondTrx;
  //transaction::Methods& secondTrx = mockSecondTrx.get();

  // ------------------------------
  // Section: Mock Functions
  // ------------------------------
/* gone...
  fakeit::When(OverloadedMethod(mockQuery, setEngine, void(ExecutionEngine*))).Do([&](ExecutionEngine* eng) -> void {
    // We expect that the snippet injects a new engine into our
    // query.
    // However we have to return a mocked engine later
    ASSERT_NE(eng, nullptr);
    // Throw it away
    delete eng;
  });
*/
  // fakeit::When(Method(mockQuery, trx)).Return(&trx);
  // TODO fakeit::When(Method(mockQuery, engine)).Return(&myEngine).Return(&myEngine);
  fakeit::When(Method(mockEngine, createBlocks))
      .Do([&](std::vector<ExecutionNode*> const& nodes,
              std::unordered_set<std::string> const&, MapRemoteToSnippet const&) {
        EXPECT_EQ(nodes.size(), 1);
        EXPECT_EQ(nodes[0], &fNode);
        return Result{TRI_ERROR_NO_ERROR};
      });
  fakeit::When(ConstOverloadedMethod(mockEngine, root, ExecutionBlock * ())).AlwaysReturn(&block);

  /*/ Mock query clone
  fakeit::When(Method(mockQuery, clone)).Do([&](QueryPart part, bool withPlan) -> Query* {
    EXPECT_EQ(part, PART_DEPENDENT);
    EXPECT_FALSE(withPlan);
    return &queryClone;
  });
  */
  /* 
  fakeit::When(OverloadedMethod(mockQueryClone, setEngine, void(ExecutionEngine * ))).Do(
    [&](ExecutionEngine *eng) -> void {
    // We expect that the snippet injects a new engine into our
    // query.
    // However we have to return a mocked engine later
    ASSERT_NE(eng, nullptr);
    // Throw it away
    delete eng;
  });
  */
  //fakeit::When(Method(mockQueryClone, trx)).Return(&secondTrx);
  // fakeit::When(Method(mockQueryClone, engine)).Return(&mySecondEngine);

  fakeit::When(Method(mockSecondEngine, createBlocks))
      .Do([&](std::vector<ExecutionNode*> const& nodes,
              std::unordered_set<std::string> const&, MapRemoteToSnippet const&) {
        EXPECT_EQ(nodes.size(), 1);
        EXPECT_EQ(nodes[0], &sNode);
        return Result{TRI_ERROR_NO_ERROR};
      });
  fakeit::When(ConstOverloadedMethod(mockSecondEngine, root, ExecutionBlock * ()))
      .AlwaysReturn(&block);

  /*/ Mock the Registry
  fakeit::When(Method(mockRegistry, insert))
    .Do([&](QueryId id, Query* query, double timeout, bool isPrepared, bool keepLease,
            std::unique_ptr<arangodb::cluster::CallbackGuard>&) {
        ASSERT_NE(id, 0);
        ASSERT_NE(query, nullptr);
        ASSERT_TRUE(isPrepared);
        ASSERT_FALSE(keepLease);
        ASSERT_EQ(timeout, 600.0);
        ASSERT_EQ(query, &queryClone);
        secondId = id;
      });
  */
  // ------------------------------
  // Section: Run the test
  // ------------------------------

  EngineInfoContainerCoordinator testee;
  testee.addNode(&fNode);

  // Open the Second Snippet
  testee.openSnippet(remoteId);
  // Inject a node
  testee.addNode(&sNode);
  // Close the second snippet
  testee.closeSnippet();
  ResourceMonitor monitor;
  AqlItemBlockManager mgr(&monitor, SerializationFormat::SHADOWROWS); /// TODO

  std::vector<uint64_t> coordinatorQueryIds{};
  SnippetList coordSnippets;
  auto result =
      testee.buildEngines(query,
                          // &registry,
                          mgr,
                          // dbname,
                          restrictToShards,
                          queryIds,
                          coordSnippets
                          // coordinatorQueryIds
        );
  ASSERT_TRUE(result.ok());

/*
  ExecutionEngine* engine = result.engine();

  ASSERT_NE(engine, nullptr);
  ASSERT_EQ(engine, &myEngine);
*/
  // The second engine needs a generated id
  ASSERT_NE(secondId, 0);
  // We do not add anything to the ids
  ASSERT_TRUE(queryIds.empty());

  // Validate that the query is wired up with the engine
  // fakeit::Verify(OverloadedMethod(mockQuery, setEngine, void(ExecutionEngine*))).Exactly(1);
  // Validate that createBlocks has been called!
  fakeit::Verify(Method(mockEngine, createBlocks)).Exactly(1);

  // Validate that the second query is wired up with the second engine
  // fakeit::Verify(OverloadedMethod(mockQueryClone, setEngine, void(ExecutionEngine*))).Exactly(1);
  // Validate that createBlocks has been called!
  fakeit::Verify(Method(mockSecondEngine, createBlocks)).Exactly(1);
  // fakeit::Verify(Method(mockRegistry, insert)).Exactly(1);
}

TEST(EngineInfoContainerTest, snippets_are_a_stack_insert_node_always_into_top_snippet) {
  std::unordered_set<std::string> const restrictToShards;
  MapRemoteToSnippet queryIds;

  auto const remoteId = ExecutionNodeId{1337};
  auto const secondRemoteId = ExecutionNodeId{42};
  //QueryId secondId = 0;
  // QueryId thirdId = 0;
  std::string dbname = "TestDB";

  /*
  auto setEngineCallback = [](ExecutionEngine* eng) -> void {
    // We expect that the snippet injects a new engine into our
    // query.
    // However we have to return a mocked engine later
    ASSERT_NE(eng, nullptr);
    // Throw it away
    delete eng;
  };
  */
  // We test the following:
  // Base Snippet insert node
  // New Snippet (A)
  // Insert Node -> (A)
  // Close (A)
  // Insert Node -> Base
  // New Snippet (B)
  // Insert Node -> (B)
  // Close (B)
  // Insert Node -> Base
  // Verfiy on Engines

  // ------------------------------
  // Section: Create Mock Instances
  // ------------------------------

  fakeit::Mock<ExecutionNode> firstBaseNodeMock;
  ExecutionNode& fbNode = firstBaseNodeMock.get();
  fakeit::When(Method(firstBaseNodeMock, getType)).AlwaysReturn(ExecutionNode::SINGLETON);

  fakeit::Mock<ExecutionNode> snipANodeMock;
  ExecutionNode& aNode = snipANodeMock.get();
  fakeit::When(Method(snipANodeMock, getType)).AlwaysReturn(ExecutionNode::SINGLETON);

  fakeit::Mock<ExecutionNode> secondBaseNodeMock;
  ExecutionNode& sbNode = secondBaseNodeMock.get();
  fakeit::When(Method(secondBaseNodeMock, getType)).AlwaysReturn(ExecutionNode::SINGLETON);

  fakeit::Mock<ExecutionNode> snipBNodeMock;
  ExecutionNode& bNode = snipBNodeMock.get();
  fakeit::When(Method(snipBNodeMock, getType)).AlwaysReturn(ExecutionNode::SINGLETON);

  fakeit::Mock<ExecutionNode> thirdBaseNodeMock;
  ExecutionNode& tbNode = thirdBaseNodeMock.get();
  fakeit::When(Method(thirdBaseNodeMock, getType)).AlwaysReturn(ExecutionNode::SINGLETON);

  // We need a block only for assertion
  fakeit::Mock<ExecutionBlock> blockMock;
  ExecutionBlock& block = blockMock.get();

  // Mock engine for first snippet
  fakeit::Mock<ExecutionEngine> mockEngine;
  // ExecutionEngine& myEngine = mockEngine.get();

  // Mock engine for second snippet
  fakeit::Mock<ExecutionEngine> mockSecondEngine;
  // ExecutionEngine& mySecondEngine = mockSecondEngine.get();

  // Mock engine for second snippet
  fakeit::Mock<ExecutionEngine> mockThirdEngine;
  //ExecutionEngine& myThirdEngine = mockThirdEngine.get();

  fakeit::Mock<QueryOptions> mockQueryOptions;
  QueryOptions& lqueryOptions = mockQueryOptions.get();
  lqueryOptions.ttl = 600;

  fakeit::Mock<Query> mockQuery;
  Query& query = mockQuery.get();
  fakeit::When(ConstOverloadedMethod(mockQuery, queryOptions, QueryOptions const&()))
      .AlwaysDo([&]() -> QueryOptions const& { return lqueryOptions; });
  fakeit::When(OverloadedMethod(mockQuery, queryOptions, QueryOptions & ()))
      .AlwaysDo([&]() -> QueryOptions& { return lqueryOptions; });

  // We need two query clones
  fakeit::Mock<Query> mockQueryClone;
  //Query& queryClone = mockQueryClone.get();
  fakeit::When(ConstOverloadedMethod(mockQueryClone, queryOptions, QueryOptions const&()))
      .AlwaysDo([&]() -> QueryOptions const& { return lqueryOptions; });
  fakeit::When(OverloadedMethod(mockQueryClone, queryOptions, QueryOptions & ()))
      .AlwaysDo([&]() -> QueryOptions& { return lqueryOptions; });

  fakeit::Mock<Query> mockQuerySecondClone;
  //Query& querySecondClone = mockQuerySecondClone.get();
  fakeit::When(ConstOverloadedMethod(mockQuerySecondClone, queryOptions,
                                     QueryOptions const&()))
      .AlwaysDo([&]() -> QueryOptions const& { return lqueryOptions; });
  fakeit::When(OverloadedMethod(mockQuerySecondClone, queryOptions, QueryOptions & ()))
      .AlwaysDo([&]() -> QueryOptions& { return lqueryOptions; });

  fakeit::Mock<QueryRegistry> mockRegistry;
  fakeit::When(Method(mockRegistry, defaultTTL)).AlwaysReturn(600.0);
  // QueryRegistry& registry = mockRegistry.get();

  fakeit::Mock<transaction::Methods> mockTrx;
  //transaction::Methods& trx = mockTrx.get();

  fakeit::Mock<transaction::Methods> mockSecondTrx;
  //transaction::Methods& secondTrx = mockSecondTrx.get();

  fakeit::Mock<transaction::Methods> mockThirdTrx;
  //transaction::Methods& thirdTrx = mockThirdTrx.get();

  // ------------------------------
  // Section: Mock Functions
  // ------------------------------

  // fakeit::When(OverloadedMethod(mockQuery, setEngine, void(ExecutionEngine*))).Do(setEngineCallback);
  // fakeit::When(Method(mockQuery, trx)).Return(&trx);
  //fakeit::When(Method(mockQuery, engine)).Return(&myEngine).Return(&myEngine);
  fakeit::When(Method(mockEngine, createBlocks))
      .Do([&](std::vector<ExecutionNode*> const& nodes,
              std::unordered_set<std::string> const&, MapRemoteToSnippet const&) {
        EXPECT_EQ(nodes.size(), 3);
        EXPECT_EQ(nodes[0], &fbNode);
        EXPECT_EQ(nodes[1], &sbNode);
        EXPECT_EQ(nodes[2], &tbNode);
        return Result{TRI_ERROR_NO_ERROR};
      });
  fakeit::When(ConstOverloadedMethod(mockEngine, root, ExecutionBlock * ())).AlwaysReturn(&block);

  /*
  fakeit::When(Method(mockQuery, clone))
      .Do([&](QueryPart part, bool withPlan) -> Query* {
        EXPECT_EQ(part, PART_DEPENDENT);
        EXPECT_FALSE(withPlan);
        return &queryClone;
      })
      .Do([&](QueryPart part, bool withPlan) -> Query* {
        EXPECT_EQ(part, PART_DEPENDENT);
        EXPECT_FALSE(withPlan);
        return &querySecondClone;
      });
  */
  // Mock first clone
  // fakeit::When(OverloadedMethod(mockQueryClone, setEngine, void(ExecutionEngine*))).Do(setEngineCallback);
  //fakeit::When(Method(mockQueryClone, engine)).Return(&mySecondEngine);
  //fakeit::When(Method(mockQueryClone, trx)).Return(&secondTrx);
  fakeit::When(Method(mockSecondEngine, createBlocks))
      .Do([&](std::vector<ExecutionNode*> const& nodes,
              std::unordered_set<std::string> const&, MapRemoteToSnippet const&) {
        EXPECT_EQ(nodes.size(), 1);
        EXPECT_EQ(nodes[0], &aNode);
        return Result{TRI_ERROR_NO_ERROR};
      });
  fakeit::When(ConstOverloadedMethod(mockSecondEngine, root, ExecutionBlock * ()))
      .AlwaysReturn(&block);

  // Mock second clone
  // fakeit::When(OverloadedMethod(mockQuerySecondClone, setEngine, void(ExecutionEngine*))).Do(setEngineCallback);
  // fakeit::When(Method(mockQuerySecondClone, engine)).Return(&myThirdEngine);
  //fakeit::When(Method(mockQuerySecondClone, trx)).Return(&thirdTrx);
  fakeit::When(Method(mockThirdEngine, createBlocks))
      .Do([&](std::vector<ExecutionNode*> const& nodes,
              std::unordered_set<std::string> const&, MapRemoteToSnippet const&) {
        EXPECT_EQ(nodes.size(), 1);
        EXPECT_EQ(nodes[0], &bNode);
        return Result{TRI_ERROR_NO_ERROR};
      });
  fakeit::When(ConstOverloadedMethod(mockThirdEngine, root, ExecutionBlock * ()))
      .AlwaysReturn(&block);

  // Mock the Registry
  // NOTE: This expects an ordering of the engines first of the stack will be
  // handled first. With same fakeit magic we could make this ordering
  // independent which is is fine as well for the production code.
  /*fakeit::When(Method(mockRegistry, insert))
    .Do([&](QueryId id, Query* query, double timeout, bool isPrepared, bool keepLease,
          std::unique_ptr<arangodb::cluster::CallbackGuard>&) {
        ASSERT_NE(id, 0);
        ASSERT_NE(query, nullptr);
        ASSERT_TRUE(isPrepared);
        ASSERT_FALSE(keepLease);
        ASSERT_EQ(timeout, 600.0);
        ASSERT_EQ(query, &queryClone);
        secondId = id;
      })
    .Do([&](QueryId id, Query* query, double timeout, bool isPrepared, bool keepLease,
          std::unique_ptr<arangodb::cluster::CallbackGuard>&) {
        ASSERT_NE(id, 0);
        ASSERT_NE(query, nullptr);
        ASSERT_EQ(timeout, 600.0);
        ASSERT_FALSE(keepLease);
        ASSERT_EQ(query, &querySecondClone);
        thirdId = id;
      });
  */
  // ------------------------------
  // Section: Run the test
  // ------------------------------
  EngineInfoContainerCoordinator testee;

  testee.addNode(&fbNode);

  testee.openSnippet(remoteId);
  testee.addNode(&aNode);
  testee.closeSnippet();

  testee.addNode(&sbNode);

  testee.openSnippet(secondRemoteId);
  testee.addNode(&bNode);
  testee.closeSnippet();

  testee.addNode(&tbNode);
  ResourceMonitor monitor;
  AqlItemBlockManager mgr(&monitor, SerializationFormat::SHADOWROWS); /// TODO
  SnippetList coordSnippets;
  std::vector<uint64_t> coordinatorQueryIds{};
  auto result =
      testee.buildEngines(query,
                          // &registry,
                          mgr,
                          // dbname,
                          restrictToShards,
                          queryIds,
                          coordSnippets
                          // coordinatorQueryIds
        );
  ASSERT_TRUE(result.ok());
  /*                
  ExecutionEngine* engine = result.engine();
  ASSERT_NE(engine, nullptr);
  ASSERT_EQ(engine, &myEngine);
  */
  // We do not add anything to the ids
  ASSERT_TRUE(queryIds.empty());

  // Validate that the query is wired up with the engine
  //fakeit::Verify(OverloadedMethod(mockQuery, setEngine, void(ExecutionEngine*))).Exactly(1);
  // Validate that createBlocks has been called!
  fakeit::Verify(Method(mockEngine, createBlocks)).Exactly(1);

  // Validate that the second query is wired up with the second engine
  // fakeit::Verify(OverloadedMethod(mockQueryClone, setEngine, void(ExecutionEngine*))).Exactly(1);
  // Validate that createBlocks has been called!
  fakeit::Verify(Method(mockSecondEngine, createBlocks)).Exactly(1);

  // Validate that the second query is wired up with the second engine
  // fakeit::Verify(OverloadedMethod(mockQuerySecondClone, setEngine, void(ExecutionEngine*))).Exactly(1);
  // Validate that createBlocks has been called!
  fakeit::Verify(Method(mockThirdEngine, createBlocks)).Exactly(1);

  // Validate two queries are registered correctly
  // fakeit::Verify(Method(mockRegistry, insert)).Exactly(2);
}

TEST(EngineInfoContainerTest, error_cases_cloning_of_a_query_fails_throws_an_error) {
  std::unordered_set<std::string> const restrictToShards;
  MapRemoteToSnippet queryIds;

  auto const remoteId = ExecutionNodeId{1337};
  // QueryId secondId = 0;
  std::string dbname = "TestDB";

  // ------------------------------
  // Section: Create Mock Instances
  // ------------------------------
  fakeit::Mock<ExecutionNode> firstNodeMock;
  ExecutionNode& fNode = firstNodeMock.get();
  fakeit::When(Method(firstNodeMock, getType)).AlwaysReturn(ExecutionNode::SINGLETON);

  // We need a block only for assertion
  fakeit::Mock<ExecutionBlock> blockMock;
  ExecutionBlock& block = blockMock.get();

  // Mock engine for first snippet
  fakeit::Mock<ExecutionEngine> mockEngine;
  //ExecutionEngine& myEngine = mockEngine.get();

  // Mock engine for second snippet
  fakeit::Mock<ExecutionEngine> mockSecondEngine;
  //ExecutionEngine& mySecondEngine = mockSecondEngine.get();

  fakeit::Mock<QueryOptions> mockQueryOptions;
  QueryOptions& lqueryOptions = mockQueryOptions.get();
  lqueryOptions.ttl = 600;

  fakeit::Mock<Query> mockQuery;
  Query& query = mockQuery.get();
  fakeit::When(ConstOverloadedMethod(mockQuery, queryOptions, QueryOptions const&()))
      .AlwaysDo([&]() -> QueryOptions const& { return lqueryOptions; });
  fakeit::When(OverloadedMethod(mockQuery, queryOptions, QueryOptions & ()))
      .AlwaysDo([&]() -> QueryOptions& { return lqueryOptions; });

  fakeit::Mock<Query> mockQueryClone;
  //Query& queryClone = mockQueryClone.get();
  fakeit::When(ConstOverloadedMethod(mockQueryClone, queryOptions, QueryOptions const&()))
      .AlwaysDo([&]() -> QueryOptions const& { return lqueryOptions; });
  fakeit::When(OverloadedMethod(mockQueryClone, queryOptions, QueryOptions & ()))
      .AlwaysDo([&]() -> QueryOptions& { return lqueryOptions; });

  fakeit::Mock<QueryRegistry> mockRegistry;
  fakeit::When(Method(mockRegistry, defaultTTL)).AlwaysReturn(600.0);
  // QueryRegistry& registry = mockRegistry.get();

  fakeit::Mock<transaction::Methods> mockTrx;
  // transaction::Methods& trx = mockTrx.get();

  fakeit::Mock<transaction::Methods> mockSecondTrx;
  //transaction::Methods& secondTrx = mockSecondTrx.get();

  // ------------------------------
  // Section: Mock Functions
  // ------------------------------
/*
  fakeit::When(OverloadedMethod(mockQuery, setEngine, void(ExecutionEngine*))).Do([&](ExecutionEngine* eng) -> void {
    // We expect that the snippet injects a new engine into our
    // query.
    // However we have to return a mocked engine later
    ASSERT_NE(eng, nullptr);
    // Throw it away
    delete eng;
  });
  fakeit::When(Method(mockQuery, engine)).Return(&myEngine).Return(&myEngine);
  fakeit::When(Method(mockQuery, trx)).Return(&trx);
*/
  fakeit::When(Method(mockEngine, createBlocks)).AlwaysReturn(Result{TRI_ERROR_NO_ERROR});
  fakeit::When(ConstOverloadedMethod(mockEngine, root, ExecutionBlock * ())).AlwaysReturn(&block);
/*
  fakeit::When(OverloadedMethod(mockQueryClone, setEngine, void(ExecutionEngine*))).Do([&](ExecutionEngine* eng) -> void {
    // We expect that the snippet injects a new engine into our
    // query.
    // However we have to return a mocked engine later
    ASSERT_NE(eng, nullptr);
    // Throw it away
    delete eng;
  });

  fakeit::When(Method(mockQueryClone, engine)).Return(&mySecondEngine);
*/
  //fakeit::When(Method(mockQueryClone, trx)).Return(&secondTrx);
  fakeit::When(Method(mockSecondEngine, createBlocks)).AlwaysReturn(Result{TRI_ERROR_NO_ERROR});
  fakeit::When(ConstOverloadedMethod(mockSecondEngine, root, ExecutionBlock * ()))
      .AlwaysReturn(&block);
/*
  fakeit::When(OverloadedMethod(mockRegistry, destroy,
                                void(std::string const&, QueryId, int, bool)))
      .Do([&](std::string const& vocbase, QueryId id, int errorCode, bool ignoreOpened) {
        ASSERT_EQ(vocbase, dbname);
        ASSERT_EQ(id, secondId);
        ASSERT_EQ(errorCode, TRI_ERROR_INTERNAL);
      });
*/
  // ------------------------------
  // Section: Run the test
  // ------------------------------

  EngineInfoContainerCoordinator testee;
  testee.addNode(&fNode);

  // Open the Second Snippet
  testee.openSnippet(remoteId);
  // Inject a node
  testee.addNode(&fNode);

  testee.openSnippet(remoteId);
  // Inject a node
  testee.addNode(&fNode);

  // Close the third snippet
  testee.closeSnippet();

  // Close the second snippet
  testee.closeSnippet();

  /*/ Mock the Registry
  fakeit::When(Method(mockRegistry, insert))
    .Do([&](QueryId id, Query* query, double timeout, bool isPrepared, bool keepLease,
          std::unique_ptr<arangodb::cluster::CallbackGuard>&) {
        ASSERT_NE(id, 0);
        ASSERT_NE(query, nullptr);
        ASSERT_EQ(timeout, 600.0);
        ASSERT_TRUE(isPrepared);
        ASSERT_FALSE(keepLease);
        ASSERT_EQ(query, &queryClone);
        secondId = id;
      });

  // Mock query clone
  fakeit::When(Method(mockQuery, clone))
      .Do([&](QueryPart part, bool withPlan) -> Query* {
        EXPECT_EQ(part, PART_DEPENDENT);
        EXPECT_FALSE(withPlan);
        return &queryClone;
      })
      .Throw(arangodb::basics::Exception(TRI_ERROR_DEBUG, __FILE__, __LINE__));
  */
  std::vector<uint64_t> coordinatorQueryIds{};
  ResourceMonitor monitor;
  AqlItemBlockManager mgr(&monitor, SerializationFormat::SHADOWROWS); /// TODO
  SnippetList coordSnippets;
  auto result =
      testee.buildEngines(query,
                          mgr,
                          restrictToShards,
                          queryIds,
                          coordSnippets
        );
  ASSERT_FALSE(result.ok());
  // Make sure we check the right thing here
  ASSERT_EQ(result.errorNumber(), TRI_ERROR_DEBUG);

  // Validate that the path up to intended error was taken

  // Validate that the query is wired up with the engine
  // fakeit::Verify(OverloadedMethod(mockQuery, setEngine, void(ExecutionEngine*))).Exactly(1);
  // Validate that createBlocks has been called!
  fakeit::Verify(Method(mockEngine, createBlocks)).Exactly(1);

  // Validate that the second query is wired up with the second engine
  // fakeit::Verify(OverloadedMethod(mockQueryClone, setEngine, void(ExecutionEngine*))).Exactly(1);
  // Validate that createBlocks has been called!
  fakeit::Verify(Method(mockSecondEngine, createBlocks)).Exactly(1);
  //fakeit::Verify(Method(mockRegistry, insert)).Exactly(1);

  // Assert unregister of second engine.
  // fakeit::Verify(OverloadedMethod(mockRegistry, destroy,
  //                                 void(std::string const&, QueryId, int, bool)))
  //     .Exactly(1);
}

TEST(EngineInfoContainerTest, error_cases_cloning_of_a_query_fails_returns_a_nullptr) {
  std::unordered_set<std::string> const restrictToShards;
  MapRemoteToSnippet queryIds;

  auto const remoteId = ExecutionNodeId{1337};
  //QueryId secondId = 0;
  std::string dbname = "TestDB";

  // ------------------------------
  // Section: Create Mock Instances
  // ------------------------------
  fakeit::Mock<ExecutionNode> firstNodeMock;
  ExecutionNode& fNode = firstNodeMock.get();
  fakeit::When(Method(firstNodeMock, getType)).AlwaysReturn(ExecutionNode::SINGLETON);

  // We need a block only for assertion
  fakeit::Mock<ExecutionBlock> blockMock;
  ExecutionBlock& block = blockMock.get();

  // Mock engine for first snippet
  fakeit::Mock<ExecutionEngine> mockEngine;
  //ExecutionEngine& myEngine = mockEngine.get();

  // Mock engine for second snippet
  fakeit::Mock<ExecutionEngine> mockSecondEngine;
  //ExecutionEngine& mySecondEngine = mockSecondEngine.get();

  fakeit::Mock<QueryOptions> mockQueryOptions;
  QueryOptions& lqueryOptions = mockQueryOptions.get();
  lqueryOptions.ttl = 600;

  fakeit::Mock<Query> mockQuery;
  Query& query = mockQuery.get();
  fakeit::When(ConstOverloadedMethod(mockQuery, queryOptions, QueryOptions const&()))
      .AlwaysDo([&]() -> QueryOptions const& { return lqueryOptions; });
  fakeit::When(OverloadedMethod(mockQuery, queryOptions, QueryOptions & ()))
      .AlwaysDo([&]() -> QueryOptions& { return lqueryOptions; });

  fakeit::Mock<Query> mockQueryClone;
  //Query& queryClone = mockQueryClone.get();
  fakeit::When(ConstOverloadedMethod(mockQueryClone, queryOptions, QueryOptions const&()))
      .AlwaysDo([&]() -> QueryOptions const& { return lqueryOptions; });
  fakeit::When(OverloadedMethod(mockQueryClone, queryOptions, QueryOptions & ()))
      .AlwaysDo([&]() -> QueryOptions& { return lqueryOptions; });

  fakeit::Mock<QueryRegistry> mockRegistry;
  fakeit::When(Method(mockRegistry, defaultTTL)).AlwaysReturn(600.0);
  //QueryRegistry& registry = mockRegistry.get();

  fakeit::Mock<transaction::Methods> mockTrx;
  //transaction::Methods& trx = mockTrx.get();

  fakeit::Mock<transaction::Methods> mockSecondTrx;
  //transaction::Methods& secondTrx = mockSecondTrx.get();

  // ------------------------------
  // Section: Mock Functions
  // ------------------------------
/*
  fakeit::When(OverloadedMethod(mockQuery, setEngine, void(ExecutionEngine*))).Do([&](ExecutionEngine* eng) -> void {
    // We expect that the snippet injects a new engine into our
    // query.
    // However we have to return a mocked engine later
    ASSERT_NE(eng, nullptr);
    // Throw it away
    delete eng;
  });
*/
  //fakeit::When(Method(mockQuery, engine)).Return(&myEngine).Return(&myEngine);
  //fakeit::When(Method(mockQuery, trx)).Return(&trx);
  fakeit::When(Method(mockEngine, createBlocks)).AlwaysReturn(Result{TRI_ERROR_NO_ERROR});
  fakeit::When(ConstOverloadedMethod(mockEngine, root, ExecutionBlock * ())).AlwaysReturn(&block);

  /*fakeit::When(OverloadedMethod(mockQueryClone, setEngine, void(ExecutionEngine*))).Do([&](ExecutionEngine* eng) -> void {
    // We expect that the snippet injects a new engine into our
    // query.
    // However we have to return a mocked engine later
    ASSERT_NE(eng, nullptr);
    // Throw it away
    delete eng;
    });*/

  // fakeit::When(Method(mockQueryClone, engine)).Return(&mySecondEngine);
  //fakeit::When(Method(mockQueryClone, trx)).Return(&secondTrx);
  fakeit::When(Method(mockSecondEngine, createBlocks)).AlwaysReturn(Result{TRI_ERROR_NO_ERROR});
  fakeit::When(ConstOverloadedMethod(mockSecondEngine, root, ExecutionBlock * ()))
      .AlwaysReturn(&block);

/*  fakeit::When(OverloadedMethod(mockRegistry, destroy,
                                void(std::string const&, QueryId, int, bool)))
      .Do([&](std::string const& vocbase, QueryId id, int errorCode, bool ignoreOpened) {
        ASSERT_EQ(vocbase, dbname);
        ASSERT_EQ(id, secondId);
        ASSERT_EQ(errorCode, TRI_ERROR_INTERNAL);
      });
*/
  // ------------------------------
  // Section: Run the test
  // ------------------------------

  EngineInfoContainerCoordinator testee;
  testee.addNode(&fNode);

  // Open the Second Snippet
  testee.openSnippet(remoteId);
  // Inject a node
  testee.addNode(&fNode);

  testee.openSnippet(remoteId);
  // Inject a node
  testee.addNode(&fNode);

  // Close the third snippet
  testee.closeSnippet();

  // Close the second snippet
  testee.closeSnippet();

  /*/ Mock the Registry
  fakeit::When(Method(mockRegistry, insert))
    .Do([&](QueryId id, Query* query, double timeout, bool isPrepared, bool keepLease,
          std::unique_ptr<arangodb::cluster::CallbackGuard>&) {
        ASSERT_NE(id, 0);
        ASSERT_NE(query, nullptr);
        ASSERT_EQ(timeout, 600.0);
        ASSERT_TRUE(isPrepared);
        ASSERT_FALSE(keepLease);
        ASSERT_EQ(query, &queryClone);
        secondId = id;
      });

  // Mock query clone
  fakeit::When(Method(mockQuery, clone))
      .Do([&](QueryPart part, bool withPlan) -> Query* {
        EXPECT_EQ(part, PART_DEPENDENT);
        EXPECT_FALSE(withPlan);
        return &queryClone;
      })
      .Do([&](QueryPart part, bool withPlan) -> Query* {
        EXPECT_EQ(part, PART_DEPENDENT);
        EXPECT_FALSE(withPlan);
        return nullptr;
      });
  */
  ResourceMonitor monitor;
  AqlItemBlockManager mgr(&monitor, SerializationFormat::SHADOWROWS); /// TODO
   std::vector<uint64_t> coordinatorQueryIds{};
  SnippetList coordSnippets;
  auto result =
      testee.buildEngines(query,
                          // &registry,
                          mgr,
                          // dbname,
                          restrictToShards,
                          queryIds,
                          coordSnippets
                          // coordinatorQueryIds
        );
  ASSERT_FALSE(result.ok());
  // Make sure we check the right thing here
  ASSERT_EQ(result.errorNumber(), TRI_ERROR_INTERNAL);

  // Validate that the path up to intended error was taken

  // Validate that the query is wired up with the engine
  //fakeit::Verify(OverloadedMethod(mockQuery, setEngine, void(ExecutionEngine*))).Exactly(1);
  // Validate that createBlocks has been called!
  fakeit::Verify(Method(mockEngine, createBlocks)).Exactly(1);

  // Validate that the second query is wired up with the second engine
  //fakeit::Verify(OverloadedMethod(mockQueryClone, setEngine, void(ExecutionEngine*))).Exactly(1);
  // Validate that createBlocks has been called!
  fakeit::Verify(Method(mockSecondEngine, createBlocks)).Exactly(1);
  //fakeit::Verify(Method(mockRegistry, insert)).Exactly(1);

  /*/ Assert unregister of second engine.
  fakeit::Verify(OverloadedMethod(mockRegistry, destroy,
                                  void(std::string const&, QueryId, int, bool)))
      .Exactly(1);
  */
}
#endif

}  // namespace engine_info_container_coordinator_test
}  // namespace tests
}  // namespace arangodb
