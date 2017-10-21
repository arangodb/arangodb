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

  SECTION("it should create an ExecutionEngine for the first snippet") {
    EngineInfoContainerCoordinator testee;

    // Query: RETURN 1
    // Singleton <- Calc <- Return
    Mock<ExecutionNode> singletonMock;
    ExecutionNode& sNode = singletonMock.get();
    When(Method(singletonMock, getType)).AlwaysReturn(ExecutionNode::SINGLETON);

    Mock<ExecutionBlock> singletonBlockMock;
    ExecutionBlock& sBlock = singletonBlockMock.get();

    Mock<ExecutionNode> calculationMock;
    ExecutionNode& cNode = calculationMock.get();
    When(Method(calculationMock, getType)).AlwaysReturn(ExecutionNode::CALCULATION);

    Mock<ExecutionBlock> calculationBlockMock;
    ExecutionBlock& cBlock = calculationBlockMock.get();

    Mock<ExecutionNode> returnMock;
    ExecutionNode& rNode = returnMock.get();
    When(Method(returnMock, getType)).AlwaysReturn(ExecutionNode::RETURN);

    Mock<ExecutionBlock> returnBlockMock;
    ExecutionBlock& rBlock = returnBlockMock.get();

    testee.addNode(&rNode);
    testee.addNode(&cNode);
    testee.addNode(&sNode);

    ExecutionEngine* linkedToQuery = nullptr;
    Mock<Query> mockQuery;
    Query& query = mockQuery.get();
    When(Method(mockQuery, setEngine)).Do([&](ExecutionEngine* eng) -> void {
      linkedToQuery = eng;
    });
    When(Method(mockQuery, engine)).Return(linkedToQuery);

    Mock<ExecutionEngine> mockEngine;
    ExecutionEngine& myEngine = mockEngine.get();
    When(Method(mockEngine, createBlock).Using(&rNode,_,_)).Return(&rBlock);
    When(Method(mockEngine, createBlock).Using(&cNode,_,_)).Return(&cBlock);
    When(Method(mockEngine, createBlock).Using(&sNode,_,_)).Return(&sBlock);
    When(Method(mockEngine, addBlock)).Return().Return().Return();
    When(Method(mockEngine, setLockedShards)).Return();

    When(Method(mockQuery, engine)).Return(linkedToQuery);
    
    Mock<QueryRegistry> mockRegistry;
    QueryRegistry& registry = mockRegistry.get();

    std::unordered_set<std::string> const restrictToShards;
    std::unordered_map<std::string, std::string> queryIds;
    auto lockedShards = std::make_unique<std::unordered_set<ShardID> const>();

    ExecutionEngine* engine = testee.buildEngines(
      &query, &registry, restrictToShards, queryIds, lockedShards.get() 
    );
    VerifyNoOtherInvocations(mockQuery);

    REQUIRE(engine != nullptr);
    REQUIRE(engine == &myEngine);

  }

}
} // test
} // aql
} // arangodb
