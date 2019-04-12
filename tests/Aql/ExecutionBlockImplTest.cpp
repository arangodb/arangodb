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
#include "BlockFetcherHelper.h"
#include "BlockFetcherMock.h"
#include "TestEmptyExecutorHelper.h"
#include "TestExecutorHelper.h"
#include "WaitingExecutionBlockMock.h"
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
  SharedAqlItemBlockPtr result;

  // Mock of the ExecutionEngine
  fakeit::Mock<ExecutionEngine> mockEngine;
  ExecutionEngine& engine = mockEngine.get();

  // Mock of the AqlItemBlockManager
  fakeit::Mock<AqlItemBlockManager> mockBlockManager;
  AqlItemBlockManager& itemBlockManager = mockBlockManager.get();

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

  fakeit::When(Method(mockBlockManager, requestBlock)).AlwaysDo([&](size_t nrItems, RegisterId nrRegs) -> SharedAqlItemBlockPtr {
    return SharedAqlItemBlockPtr{new AqlItemBlock(itemBlockManager, nrItems, nrRegs)};
  });

  fakeit::When(Method(mockEngine, itemBlockManager)).AlwaysReturn(itemBlockManager);
  fakeit::When(Method(mockEngine, getQuery)).AlwaysReturn(&query);
  fakeit::When(OverloadedMethod(mockBlockManager, returnBlock, void(AqlItemBlock*&)))
      .AlwaysDo([&](AqlItemBlock*& block) -> void { AqlItemBlockManager::deleteBlock(block); block = nullptr; });
  fakeit::When(Method(mockBlockManager, resourceMonitor)).AlwaysReturn(&monitor);
  fakeit::When(ConstOverloadedMethod(mockQuery, queryOptions, QueryOptions const&()))
      .AlwaysDo([&]() -> QueryOptions const& { return lqueryOptions; });
  fakeit::When(OverloadedMethod(mockQuery, queryOptions, QueryOptions & ()))
      .AlwaysDo([&]() -> QueryOptions& { return lqueryOptions; });
  fakeit::When(Method(mockQuery, trx)).AlwaysReturn(&trx);

  fakeit::When(Method(mockQueryOptions, getProfileLevel)).AlwaysReturn(profile);

  // This is not used thus far in Base-Clase
  ExecutionNode const* node = nullptr;

  // Executor Infos
  TestExecutorHelperInfos infos(0, 1, 1, {}, {0});
  TestEmptyExecutorHelperInfos emptyInfos(0, 1, 1, {}, {0});

  GIVEN("there is a block in the upstream with no rows inside") {
    VPackBuilder input;
    BlockFetcherMock<false> blockFetcherMock{monitor, 0};

    SharedAqlItemBlockPtr block = nullptr;

    WHEN("the executor does wait, using getSome") {
      std::deque<SharedAqlItemBlockPtr> blockDeque;
      SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});
      blockDeque.push_back(std::move(block));

      WaitingExecutionBlockMock dependency{&engine, node, std::move(blockDeque)};

      ExecutionBlockImpl<TestExecutorHelper> testee(&engine, node, std::move(infos));
      testee.addDependency(&dependency);

      size_t atMost = 1000;
      std::tie(state, block) = testee.getSome(atMost);
      REQUIRE(state == ExecutionState::WAITING);
      std::tie(state, block) = testee.getSome(atMost);
      REQUIRE(block != nullptr);
      REQUIRE(block->size() == 1);
      REQUIRE(state == ExecutionState::DONE);

      // done should stay done!
      std::tie(state, block) = testee.getSome(atMost);
      REQUIRE(block == nullptr);
      REQUIRE(state == ExecutionState::DONE);
    }

    WHEN("the executor does wait, using skipSome") {
      std::deque<SharedAqlItemBlockPtr> blockDeque;
      SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});
      blockDeque.push_back(std::move(block));

      WaitingExecutionBlockMock dependency{&engine, node, std::move(blockDeque)};

      ExecutionBlockImpl<TestExecutorHelper> testee(&engine, node, std::move(infos));
      testee.addDependency(&dependency);

      size_t atMost = 1;
      size_t skipped = 0;

      std::tie(state, skipped) = testee.skipSome(atMost);
      REQUIRE(state == ExecutionState::WAITING);
      REQUIRE(skipped == 0);

      std::tie(state, skipped) = testee.skipSome(atMost);
      REQUIRE(state == ExecutionState::DONE);
      REQUIRE(skipped == 1);

      // done should stay done!
      std::tie(state, skipped) = testee.skipSome(atMost);
      REQUIRE(state == ExecutionState::DONE);
      REQUIRE(skipped == 0);
    }
  }

  GIVEN("there are multiple blocks in the upstream with no rows inside") {
    VPackBuilder input;
    BlockFetcherMock<false> blockFetcherMock{monitor, 0};

    SharedAqlItemBlockPtr block = nullptr;

    WHEN("the executor does wait - using getSome - one block") {
      // we are checking multiple input blocks
      // we are only fetching 1 row each (atMost = 1)
      // after a DONE is returned, it must stay done!

      std::deque<SharedAqlItemBlockPtr> blockDeque;
      SharedAqlItemBlockPtr blocka = buildBlock<1>(itemBlockManager, {{42}});
      SharedAqlItemBlockPtr blockb = buildBlock<1>(itemBlockManager, {{42}});
      SharedAqlItemBlockPtr blockc = buildBlock<1>(itemBlockManager, {{42}});
      SharedAqlItemBlockPtr blockd = buildBlock<1>(itemBlockManager, {{42}});
      SharedAqlItemBlockPtr blocke = buildBlock<1>(itemBlockManager, {{42}});
      blockDeque.push_back(std::move(blocka));
      blockDeque.push_back(std::move(blockb));
      blockDeque.push_back(std::move(blockc));
      blockDeque.push_back(std::move(blockd));
      blockDeque.push_back(std::move(blocke));

      WaitingExecutionBlockMock dependency{&engine, node, std::move(blockDeque)};

      ExecutionBlockImpl<TestExecutorHelper> testee(&engine, node, std::move(infos));
      testee.addDependency(&dependency);
      size_t atMost = 1;
      size_t total = 0;

      std::tie(state, block) = testee.getSome(atMost);
      REQUIRE(state == ExecutionState::WAITING);

      std::tie(state, block) = testee.getSome(atMost);
      REQUIRE(state == ExecutionState::HASMORE);
      total = total + block->size();

      std::tie(state, block) = testee.getSome(atMost);
      REQUIRE(state == ExecutionState::WAITING);

      std::tie(state, block) = testee.getSome(atMost);
      REQUIRE(state == ExecutionState::HASMORE);
      total = total + block->size();

      std::tie(state, block) = testee.getSome(atMost);
      REQUIRE(state == ExecutionState::WAITING);

      std::tie(state, block) = testee.getSome(atMost);
      REQUIRE(state == ExecutionState::HASMORE);
      total = total + block->size();

      std::tie(state, block) = testee.getSome(atMost);
      REQUIRE(state == ExecutionState::WAITING);

      std::tie(state, block) = testee.getSome(atMost);
      REQUIRE(state == ExecutionState::HASMORE);
      total = total + block->size();

      std::tie(state, block) = testee.getSome(atMost);
      REQUIRE(state == ExecutionState::WAITING);

      std::tie(state, block) = testee.getSome(atMost);
      REQUIRE(state == ExecutionState::DONE);
      total = total + block->size();

      std::tie(state, block) = testee.getSome(atMost);
      REQUIRE(state == ExecutionState::DONE);

      REQUIRE(total == 5);
    }

    WHEN("the executor does wait - using getSome - multiple blocks") {
      // as test above, BUT with a higher atMost value.

      std::deque<SharedAqlItemBlockPtr> blockDeque;
      SharedAqlItemBlockPtr blocka = buildBlock<1>(itemBlockManager, {{42}});
      SharedAqlItemBlockPtr blockb = buildBlock<1>(itemBlockManager, {{42}});
      SharedAqlItemBlockPtr blockc = buildBlock<1>(itemBlockManager, {{42}});
      SharedAqlItemBlockPtr blockd = buildBlock<1>(itemBlockManager, {{42}});
      SharedAqlItemBlockPtr blocke = buildBlock<1>(itemBlockManager, {{42}});
      blockDeque.push_back(std::move(blocka));
      blockDeque.push_back(std::move(blockb));
      blockDeque.push_back(std::move(blockc));
      blockDeque.push_back(std::move(blockd));
      blockDeque.push_back(std::move(blocke));

      WaitingExecutionBlockMock dependency{&engine, node, std::move(blockDeque)};

      ExecutionBlockImpl<TestExecutorHelper> testee(&engine, node, std::move(infos));
      testee.addDependency(&dependency);
      size_t atMost = 2;
      size_t total = 0;

      std::tie(state, block) = testee.getSome(atMost);
      REQUIRE(state == ExecutionState::WAITING);

      std::tie(state, block) = testee.getSome(atMost);
      REQUIRE(state == ExecutionState::WAITING);

      std::tie(state, block) = testee.getSome(atMost);
      REQUIRE(state == ExecutionState::HASMORE);
      total = total + block->size();

      std::tie(state, block) = testee.getSome(atMost);
      REQUIRE(state == ExecutionState::WAITING);

      std::tie(state, block) = testee.getSome(atMost);
      REQUIRE(state == ExecutionState::WAITING);

      std::tie(state, block) = testee.getSome(atMost);
      REQUIRE(state == ExecutionState::HASMORE);
      total = total + block->size();

      std::tie(state, block) = testee.getSome(atMost);
      REQUIRE(state == ExecutionState::WAITING);

      std::tie(state, block) = testee.getSome(atMost);
      REQUIRE(state == ExecutionState::DONE);
      total = total + block->size();

      std::tie(state, block) = testee.getSome(atMost);
      REQUIRE(state == ExecutionState::DONE);
      REQUIRE(block == nullptr);

      REQUIRE(total == 5);
    }

    WHEN("the executor does wait - using skipSome") {
      // we are checking multiple input blocks
      // we are only fetching 1 row each (atMost = 1)
      // after a DONE is returned, it must stay done!

      std::deque<SharedAqlItemBlockPtr> blockDeque;
      SharedAqlItemBlockPtr blocka = buildBlock<1>(itemBlockManager, {{42}});
      SharedAqlItemBlockPtr blockb = buildBlock<1>(itemBlockManager, {{42}});
      SharedAqlItemBlockPtr blockc = buildBlock<1>(itemBlockManager, {{42}});
      SharedAqlItemBlockPtr blockd = buildBlock<1>(itemBlockManager, {{42}});
      SharedAqlItemBlockPtr blocke = buildBlock<1>(itemBlockManager, {{42}});
      blockDeque.push_back(std::move(blocka));
      blockDeque.push_back(std::move(blockb));
      blockDeque.push_back(std::move(blockc));
      blockDeque.push_back(std::move(blockd));
      blockDeque.push_back(std::move(blocke));

      WaitingExecutionBlockMock dependency{&engine, node, std::move(blockDeque)};

      ExecutionBlockImpl<TestExecutorHelper> testee(&engine, node, std::move(infos));
      testee.addDependency(&dependency);
      size_t atMost = 1;
      size_t skipped = 0;

      std::tie(state, skipped) = testee.skipSome(atMost);
      REQUIRE(state == ExecutionState::WAITING);
      REQUIRE(skipped == 0);

      std::tie(state, skipped) = testee.skipSome(atMost);
      REQUIRE(state == ExecutionState::HASMORE);
      REQUIRE(skipped == 1);

      std::tie(state, skipped) = testee.skipSome(atMost);
      REQUIRE(state == ExecutionState::WAITING);
      REQUIRE(skipped == 0);

      std::tie(state, skipped) = testee.skipSome(atMost);
      REQUIRE(state == ExecutionState::HASMORE);
      REQUIRE(skipped == 1);

      std::tie(state, skipped) = testee.skipSome(atMost);
      REQUIRE(state == ExecutionState::WAITING);
      REQUIRE(skipped == 0);

      std::tie(state, skipped) = testee.skipSome(atMost);
      REQUIRE(state == ExecutionState::HASMORE);
      REQUIRE(skipped == 1);

      std::tie(state, skipped) = testee.skipSome(atMost);
      REQUIRE(state == ExecutionState::WAITING);
      REQUIRE(skipped == 0);

      std::tie(state, skipped) = testee.skipSome(atMost);
      REQUIRE(state == ExecutionState::HASMORE);
      REQUIRE(skipped == 1);

      std::tie(state, skipped) = testee.skipSome(atMost);
      REQUIRE(state == ExecutionState::WAITING);
      REQUIRE(skipped == 0);

      std::tie(state, skipped) = testee.skipSome(atMost);
      REQUIRE(state == ExecutionState::DONE);
      REQUIRE(skipped == 1);

      std::tie(state, skipped) = testee.skipSome(atMost);
      REQUIRE(state == ExecutionState::DONE);
      REQUIRE(skipped == 0);
    }
  }

  GIVEN("there is an invalid/empty block in the upstream") {
    VPackBuilder input;
    BlockFetcherMock<false> blockFetcherMock{monitor, 0};

    WHEN("the executor does wait, using getSome") {
      std::deque<SharedAqlItemBlockPtr> blockDeque;
      SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});
      blockDeque.push_back(std::move(block));

      WaitingExecutionBlockMock dependency{&engine, node, std::move(blockDeque)};

      ExecutionBlockImpl<TestEmptyExecutorHelper> testee(&engine, node,
                                                         std::move(emptyInfos));
      testee.addDependency(&dependency);

      size_t atMost = 1000;
      std::tie(state, block) = testee.getSome(atMost);
      REQUIRE(state == ExecutionState::DONE);
      REQUIRE(block == nullptr);
    }
  }
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
