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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "AqlItemBlockHelper.h"
#include "TestExecutorHelper.h"
#include "BlockFetcherHelper.h"
#include "WaitingExecutionBlockMock.h"
#include "BlockFetcherMock.h"
#include "catch.hpp"
#include "fakeit.hpp"

#include "Aql/ExecutionBlockImpl.h"

#include "Aql/AllRowsFetcher.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionEngine.h"

#include "Transaction/Methods.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

// This test is supposed to only test getSome return values,
// it is not supposed to test the fetch logic!

SCENARIO("ExecutionBlockImpl", "[AQL][EXECUTOR][EXECBLOCKIMPL]") {
  // ExecutionState state
  std::unique_ptr<AqlItemBlock> result;

  // Mock of the ExecutionEngine
  fakeit::Mock<ExecutionEngine> mockEngine;
  ExecutionEngine& engine = mockEngine.get();

  // Mock of the AqlItemBlockManager
  fakeit::Mock<AqlItemBlockManager> mockBlockManager;
  AqlItemBlockManager& blockManager = mockBlockManager.get();

  // Mock of the transaction
  fakeit::Mock<transaction::Methods> mockTrx;
  transaction::Methods& trx = mockTrx.get();

  // Mock of the Query
  fakeit::Mock<Query> mockQuery;
  Query& query = mockQuery.get();

  ExecutionState state;
  ResourceMonitor monitor;

  // Mock of the QueryOptions
  fakeit::Mock<QueryOptions> mockQueryOptions;
  QueryOptions& lqueryOptions = mockQueryOptions.get();
  ProfileLevel profile = ProfileLevel(PROFILE_LEVEL_NONE);

  fakeit::When(Method(mockBlockManager, requestBlock))
      .AlwaysDo([&](size_t nrItems, RegisterId nrRegs) -> AqlItemBlock* {
        return new AqlItemBlock(&monitor, nrItems, nrRegs);
      });

  fakeit::When(Method(mockEngine, itemBlockManager)).AlwaysReturn(blockManager);
  fakeit::When(Method(mockEngine, getQuery)).AlwaysReturn(&query);
  fakeit::When(
      ConstOverloadedMethod(mockQuery, queryOptions, QueryOptions const&()))
      .AlwaysDo([&]() -> QueryOptions const& { return lqueryOptions; });
  fakeit::When(OverloadedMethod(mockQuery, queryOptions, QueryOptions & ()))
      .AlwaysDo([&]() -> QueryOptions& { return lqueryOptions; });
  fakeit::When(Method(mockQuery, trx)).AlwaysReturn(&trx);

  fakeit::When(Method(mockQueryOptions, getProfileLevel)).AlwaysReturn(profile);

  // This is not used thus far in Base-Clase
  ExecutionNode const* node = nullptr;

  // Executor Infos
  TestExecutorHelperInfos infos(0, 1, 1, {});

  std::deque<std::unique_ptr<AqlItemBlock>> blockDeque;
  std::unique_ptr<AqlItemBlock> block = buildBlock<1>(&monitor, {{42}});
  blockDeque.push_back(std::move(block));

  WaitingExecutionBlockMock dependency {&engine, node, std::move(blockDeque)};

  GIVEN("there are is a block in the upstream with an empty row") {
    VPackBuilder input;
    BlockFetcherMock blockFetcherMock{0};

    auto block = std::make_unique<AqlItemBlock>(&monitor, 1000, 1);

    WHEN("the executor does wait") {
      ExecutionBlockImpl<TestExecutorHelper> testee(&engine, node,
                                                std::move(infos));
      testee.addDependency(&dependency);

      size_t atMost = 1000;
      std::tie(state, block) = testee.getSome(atMost);
      REQUIRE(state == ExecutionState::WAITING);
      std::tie(state, block) = testee.getSome(atMost);
      REQUIRE(state == ExecutionState::DONE);

      // done should stay done!
      std::tie(state, block) = testee.getSome(atMost);
      REQUIRE(state == ExecutionState::WAITING);
    }
  }
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
