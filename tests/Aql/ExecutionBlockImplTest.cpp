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
#include "catch.hpp"
#include "fakeit.hpp"

#include "Aql/ExecutionBlockImpl.h"
// #include "Aql/ExecutionBlockImpl.cpp"

#include "Aql/AllRowsFetcher.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/FilterExecutor.h"

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

  ResourceMonitor monitor;

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

  // Mock of the QueryOptions
  fakeit::Mock<QueryOptions> mockQueryOptions;
  QueryOptions& lqueryOptions = mockQueryOptions.get();

  fakeit::When(Method(mockEngine, itemBlockManager)).AlwaysReturn(blockManager);
  fakeit::When(
      ConstOverloadedMethod(mockQuery, queryOptions, QueryOptions const&()))
      .AlwaysDo([&]() -> QueryOptions const& { return lqueryOptions; });
  fakeit::When(Method(mockQuery, trx)).AlwaysReturn(&trx);
  fakeit::When(Method(mockEngine, getQuery)).AlwaysReturn(&query);

  // This is not used thus far in Base-Clase
  ExecutionNode const* node = nullptr;

  // Executor Infos
  // ExecutorInfos infos(0, 0, 1, 1, {});
  FilterExecutorInfos infos(0, 1, 1, {});

  ExecutionState state;

  GIVEN("there are no blocks upstream") {
    VPackBuilder input;
    BlockFetcherMock blockFetcherMock{0};

    auto block = std::make_unique<AqlItemBlock>(&monitor, 1000, 1);

    WHEN("the producer does not wait") {
      LOG_DEVEL << 1;
      ExecutionBlockImpl<FilterExecutor> testee(&engine, node,
                                                std::move(infos));

      LOG_DEVEL << 2;
      size_t atMost = 1000;
      LOG_DEVEL << 3;
      std::tie(state, block) = testee.getSome(atMost);
      LOG_DEVEL << state;
    }
  }
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
