////////////////////////////////////////////////////////////////////////////////
/// @brief test case for EngineInfoContainerCoordinator
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2017 ArangoDB GmbH, Cologne, Germany
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


#include "catch.hpp"
#include "fakeit.hpp"

#include "Aql/EngineInfoContainerCoordinator.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionNode.h"
#include "Aql/Query.h"
#include "Aql/QueryRegistry.h"

using namespace arangodb;
using namespace arangodb::aql;
using namespace fakeit;

namespace arangodb {
namespace tests {
namespace engine_info_container_coordinator_test {

TEST_CASE("EngineInfoContainerCoordinator", "[aql][cluster][coordinator]") {

  SECTION("it should always start with an open snippet, with queryID 0") {
    EngineInfoContainerCoordinator testee;
    QueryId res = testee.closeSnippet();
    REQUIRE(res == 0);
  }

  SECTION("it should be able to add more snippets, all giving a different id") {
    EngineInfoContainerCoordinator testee;

    size_t remote = 1;
    testee.openSnippet(remote);
    testee.openSnippet(remote);

    QueryId res1 = testee.closeSnippet();
    REQUIRE(res1 != 0);

    QueryId res2 = testee.closeSnippet();
    REQUIRE(res2 != res1);
    REQUIRE(res2 != 0);

    QueryId res3 = testee.closeSnippet();
    REQUIRE(res3 == 0);
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
  //   4. engine->setLockedShards()
  //   5. For every node (n) in Snippet:
  //     1. If remote save as (r)
  //     2. Else: engine->createBlock(n, _, _) -> ExecutionBlock (eb)
  //     3. engine->addBlock(eb)
  //     4. n->getDependencies() -> []
  //     5. If gather node:
  //       1. // Skipped
  //     6. engine->root(eb)
  //   6. Assert (engine->root() != nullptr)
  //   7. For all but the first:
  //     1. queryRegistry->insert(_id, query, 600.0);
  //     2. queryIds.emplace(idOfRemote/dbname, _id);
  // 3. query->engine();

  SECTION("it should create an ExecutionEngine for the first snippet") {

    std::unordered_set<std::string> const restrictToShards;
    std::unordered_map<std::string, std::string> queryIds;
    auto lockedShards = std::make_unique<std::unordered_set<ShardID> const>();
    std::vector<ExecutionNode*> noDependencies;
    ExecutionBlock* lastRoot = nullptr;
    std::string const dbname = "TestDB";

    // ------------------------------
    // Section: Create Mock Instances
    // ------------------------------
    Mock<ExecutionNode> singletonMock;
    ExecutionNode& sNode = singletonMock.get();

    Mock<ExecutionEngine> mockEngine;
    ExecutionEngine& myEngine = mockEngine.get();

    Mock<ExecutionBlock> singletonBlockMock;
    ExecutionBlock& sBlock = singletonBlockMock.get();

    Mock<Query> mockQuery;
    Query& query = mockQuery.get();

    Mock<QueryRegistry> mockRegistry;
    QueryRegistry& registry = mockRegistry.get();

    // ------------------------------
    // Section: Faked Function bodies
    // ------------------------------

    auto createBlock =
        [&](ExecutionNode const* n,
            std::unordered_map<ExecutionNode*, ExecutionBlock*> const&,
            std::unordered_set<std::string> const&) -> ExecutionBlock* {
      REQUIRE(n->getType() == ExecutionNode::SINGLETON);
      return &sBlock;
    };

    auto setRoot = [&] (ExecutionBlock* newRoot) -> void {
      REQUIRE(newRoot != nullptr);
      lastRoot = newRoot;
    };

    // ------------------------------
    // Section: Mock Functions
    // ------------------------------

    When(Method(mockQuery, setEngine)).Do([&](ExecutionEngine* eng) -> void {
      // We expect that the snippet injects a new engine into our
      // query.
      // However we have to return a mocked engine later
      REQUIRE(eng != nullptr);
      // Throw it away
      delete eng;
    });
    When(Method(mockQuery, engine)).Return(&myEngine).Return(&myEngine);
    When(Method(mockEngine, setLockedShards)).Return();

    // For nodes in snippet:

    // Singleton
    When(Method(singletonMock, getType)).AlwaysReturn(ExecutionNode::SINGLETON);
    When(Method(mockEngine, createBlock)).AlwaysDo(createBlock);
    When(Method(mockEngine, addBlock).Using(&sBlock)).Return();
    When(Method(singletonMock, getDependencies)).Return(noDependencies);
    When(OverloadedMethod(mockEngine, root, void(ExecutionBlock*))).AlwaysDo(setRoot);

    When(ConstOverloadedMethod(mockEngine, root, ExecutionBlock*())).Do([&] () -> ExecutionBlock* {
      // We expect that set root had been called before
      REQUIRE(lastRoot != nullptr);
      return lastRoot;
    });

    // ------------------------------
    // Section: Run the test
    // ------------------------------

    EngineInfoContainerCoordinator testee;
    testee.addNode(&sNode);

    ExecutionEngine* engine = testee.buildEngines(
      &query, &registry, dbname, restrictToShards, queryIds, lockedShards.get() 
    );

    REQUIRE(engine != nullptr);
    REQUIRE(engine == &myEngine);

    // We need our block as root
    REQUIRE(lastRoot == &sBlock);
    // The last engine should not be stored
    // It is not added to the registry
    REQUIRE(queryIds.empty());
  }

  SECTION("it should wire the dependencies on the blocks") {

    std::unordered_set<std::string> const restrictToShards;
    std::unordered_map<std::string, std::string> queryIds;
    auto lockedShards = std::make_unique<std::unordered_set<ShardID> const>();
    std::vector<ExecutionNode*> noDependencies;
    ExecutionBlock* lastRoot = nullptr;
    std::string const dbname = "TestDB";

    // Simulate Query: RETURN 1
    // Singleton <- Calc <- Return
    //
    // ------------------------------
    // Section: Create Mock Instances
    // ------------------------------
    Mock<ExecutionNode> singletonMock;
    ExecutionNode& sNode = singletonMock.get();

    Mock<ExecutionEngine> mockEngine;
    ExecutionEngine& myEngine = mockEngine.get();

    Mock<ExecutionBlock> singletonBlockMock;
    ExecutionBlock& sBlock = singletonBlockMock.get();

    Mock<ExecutionNode> calculationMock;
    ExecutionNode& cNode = calculationMock.get();

    Mock<ExecutionBlock> calculationBlockMock;
    ExecutionBlock& cBlock = calculationBlockMock.get();

    Mock<ExecutionNode> returnMock;
    ExecutionNode& rNode = returnMock.get();

    Mock<ExecutionBlock> returnBlockMock;
    ExecutionBlock& rBlock = returnBlockMock.get();

    Mock<Query> mockQuery;
    Query& query = mockQuery.get();

    Mock<QueryRegistry> mockRegistry;
    QueryRegistry& registry = mockRegistry.get();

    // ------------------------------
    // Section: Faked Function bodies
    // ------------------------------

    auto createBlock =
        [&](ExecutionNode const* n,
            std::unordered_map<ExecutionNode*, ExecutionBlock*> const&,
            std::unordered_set<std::string> const&) -> ExecutionBlock* {
      auto type = n->getType();
      switch (type) {
        case ExecutionNode::SINGLETON:
          return &sBlock;
        case ExecutionNode::CALCULATION:
          return &cBlock;
        case ExecutionNode::RETURN:
          return &rBlock;
        default:
          REQUIRE(false == true);
      }
      return nullptr;
    };

    auto setRoot = [&] (ExecutionBlock* newRoot) -> void {
      REQUIRE(newRoot != nullptr);
      lastRoot = newRoot;
    };

    // ------------------------------
    // Section: Mock Functions
    // ------------------------------

    When(Method(mockQuery, setEngine)).Do([&](ExecutionEngine* eng) -> void {
      // We expect that the snippet injects a new engine into our
      // query.
      // However we have to return a mocked engine later
      REQUIRE(eng != nullptr);
      // Throw it away
      delete eng;
    });
    When(Method(mockQuery, engine)).Return(&myEngine).Return(&myEngine);
    When(Method(mockEngine, setLockedShards)).Return();

    // For nodes in snippet:

    // Singleton
    When(Method(singletonMock, getType)).AlwaysReturn(ExecutionNode::SINGLETON);
    When(Method(mockEngine, createBlock)).AlwaysDo(createBlock);

    When(Method(mockEngine, addBlock).Using(&sBlock)).Return();
    When(Method(singletonMock, getDependencies)).Return(noDependencies);
    When(OverloadedMethod(mockEngine, root, void(ExecutionBlock*))).AlwaysDo(setRoot);
    // Calculation
    When(Method(calculationMock, getType)).AlwaysReturn(ExecutionNode::CALCULATION);
    When(Method(mockEngine, addBlock).Using(&cBlock)).Return();
    When(Method(calculationMock, getDependencies)).Return({&sNode});
    When(Method(calculationBlockMock, addDependency)).Do([&] (ExecutionBlock const* block) -> void {
        REQUIRE(block == &sBlock);
    });

    // Return
    When(Method(returnMock, getType)).AlwaysReturn(ExecutionNode::RETURN);
    When(Method(mockEngine, addBlock).Using(&rBlock)).Return();
    When(Method(returnMock, getDependencies)).Return({&cNode});
    When(Method(returnBlockMock, addDependency)).Do([&] (ExecutionBlock const* block) -> void {
        REQUIRE(block == &cBlock);
    });

    When(ConstOverloadedMethod(mockEngine, root, ExecutionBlock*())).Do([&] () -> ExecutionBlock* {
      // We expect that set root had been called before
      REQUIRE(lastRoot != nullptr);
      return lastRoot;
    });

    // ------------------------------
    // Section: Run the test
    // ------------------------------

    EngineInfoContainerCoordinator testee;
    testee.addNode(&rNode);
    testee.addNode(&cNode);
    testee.addNode(&sNode);

    ExecutionEngine* engine = testee.buildEngines(
      &query, &registry, dbname, restrictToShards, queryIds, lockedShards.get() 
    );

    REQUIRE(engine != nullptr);
    REQUIRE(engine == &myEngine);

    // We need the first block added as root
    REQUIRE(lastRoot == &rBlock);

    // The fast engine should not be stored
    // It is not added to the registry
    REQUIRE(queryIds.empty());
  }

  SECTION("it should create an new engine and register it for second snippet") {
    std::unordered_set<std::string> const restrictToShards;
    std::unordered_map<std::string, std::string> queryIds;
    auto lockedShards = std::make_unique<std::unordered_set<ShardID> const>();
    std::vector<ExecutionNode*> noDependencies;
    ExecutionBlock* lastRoot = nullptr;
    ExecutionBlock* lastRootSecond = nullptr;
    size_t remoteId = 1337;
    QueryId secondId = 0;
    std::string dbname = "TestDB";

    // ------------------------------
    // Section: Create Mock Instances
    // ------------------------------
    Mock<ExecutionNode> singletonMock;
    ExecutionNode& sNode = singletonMock.get();

    Mock<ExecutionBlock> singletonBlockMock;
    ExecutionBlock& sBlock = singletonBlockMock.get();

    Mock<ExecutionEngine> mockEngine;
    ExecutionEngine& myEngine = mockEngine.get();

    Mock<ExecutionNode> calculationMock;
    ExecutionNode& cNode = calculationMock.get();

    Mock<ExecutionBlock> calculationBlockMock;
    ExecutionBlock& cBlock = calculationBlockMock.get();

    Mock<ExecutionEngine> mockSecondEngine;
    ExecutionEngine& mySecondEngine = mockSecondEngine.get();

    Mock<Query> mockQuery;
    Query& query = mockQuery.get();

    Mock<Query> mockQueryClone;
    Query& queryClone = mockQueryClone.get();

    Mock<QueryRegistry> mockRegistry;
    QueryRegistry& registry = mockRegistry.get();

    // ------------------------------
    // Section: Faked Function bodies
    // ------------------------------

    auto createBlock =
        [&](ExecutionNode const* n,
            std::unordered_map<ExecutionNode*, ExecutionBlock*> const&,
            std::unordered_set<std::string> const&) -> ExecutionBlock* {
      auto type = n->getType();
      switch (type) {
        case ExecutionNode::SINGLETON:
          return &sBlock;
        case ExecutionNode::CALCULATION:
          return &cBlock;
        default:
          REQUIRE(false == true);
      }
      return nullptr;
    };

    auto setRoot = [&] (ExecutionBlock* newRoot) -> void {
      REQUIRE(newRoot != nullptr);
      lastRoot = newRoot;
    };

    auto setSecondRoot = [&] (ExecutionBlock* newRoot) -> void {
      REQUIRE(newRoot != nullptr);
      lastRootSecond = newRoot;
    };


    // ------------------------------
    // Section: Mock Functions
    // ------------------------------

    When(Method(mockQuery, setEngine)).Do([&](ExecutionEngine* eng) -> void {
      // We expect that the snippet injects a new engine into our
      // query.
      // However we have to return a mocked engine later
      REQUIRE(eng != nullptr);
      // Throw it away
      delete eng;
    });
    When(Method(mockQuery, engine)).Return(&myEngine).Return(&myEngine);
    When(Method(mockEngine, setLockedShards)).Return();

    // Mock query clone
    When(Method(mockQuery, clone)).Do([&](QueryPart part, bool withPlan) -> Query* {
      REQUIRE(part == PART_DEPENDENT);
      REQUIRE(withPlan == false);
      return &queryClone;
    });

    When(Method(mockQueryClone, setEngine)).Do([&](ExecutionEngine* eng) -> void {
      // We expect that the snippet injects a new engine into our
      // query.
      // However we have to return a mocked engine later
      REQUIRE(eng != nullptr);
      // Throw it away
      delete eng;
    });
    When(Method(mockQueryClone, engine)).Return(&mySecondEngine);
    When(Method(mockSecondEngine, setLockedShards)).Return();



    // For nodes in snippet:

    // Singleton
    When(Method(singletonMock, getType)).AlwaysReturn(ExecutionNode::SINGLETON);
    When(Method(mockEngine, createBlock)).AlwaysDo(createBlock);
    When(Method(mockSecondEngine, createBlock)).AlwaysDo(createBlock);

    When(Method(mockEngine, addBlock).Using(&sBlock)).Return();
    When(Method(singletonMock, getDependencies)).Return(noDependencies);
    When(OverloadedMethod(mockEngine, root, void(ExecutionBlock*))).AlwaysDo(setRoot);
    // Calculation
    When(Method(calculationMock, getType)).AlwaysReturn(ExecutionNode::CALCULATION);
    When(Method(mockSecondEngine, addBlock).Using(&cBlock)).Return();
    When(Method(calculationMock, getDependencies)).Return(noDependencies);
    When(OverloadedMethod(mockSecondEngine, root, void(ExecutionBlock*))).AlwaysDo(setSecondRoot);

    When(ConstOverloadedMethod(mockEngine, root, ExecutionBlock*())).Do([&] () -> ExecutionBlock* {
      // We expect that set root had been called before
      REQUIRE(lastRoot != nullptr);
      return lastRoot;
    });

    When(ConstOverloadedMethod(mockSecondEngine, root, ExecutionBlock*())).Do([&] () -> ExecutionBlock* {
      // We expect that set root had been called before
      REQUIRE(lastRootSecond != nullptr);
      return lastRootSecond;
    });

    When(Method(mockRegistry, insert)).Do([&] (QueryId id, Query* query, double timeout) {
      REQUIRE(id != 0);
      REQUIRE(query != nullptr);
      REQUIRE(timeout == 600.0);
      REQUIRE(query == &queryClone);
      secondId = id;
    });


    // ------------------------------
    // Section: Run the test
    // ------------------------------

    EngineInfoContainerCoordinator testee;
    testee.addNode(&sNode);

    // Open the Second Snippet
    testee.openSnippet(remoteId);
    // Inject a node
    testee.addNode(&cNode);
    // Close the second snippet
    testee.closeSnippet();

    ExecutionEngine* engine = testee.buildEngines(
      &query, &registry, dbname, restrictToShards, queryIds, lockedShards.get() 
    );

    REQUIRE(engine != nullptr);
    REQUIRE(engine == &myEngine);

    // We need the first block added as root
    REQUIRE(lastRoot == &sBlock);

    // The second snippet needs the second root
    REQUIRE(lastRootSecond == &cBlock);

    // The fast engine should not be stored
    // It is not added to the registry
    REQUIRE(!queryIds.empty());
    // The second engine needs a generated id
    REQUIRE(secondId != 0);

    // It is stored in the mapping
    std::string secIdString = arangodb::basics::StringUtils::itoa(secondId);
    std::string remIdString = arangodb::basics::StringUtils::itoa(remoteId) + "/" + dbname;
    REQUIRE(queryIds.find(remIdString) != queryIds.end());
    REQUIRE(queryIds[remIdString] == secIdString);
  }
}
} // test
} // aql
} // arangodb
