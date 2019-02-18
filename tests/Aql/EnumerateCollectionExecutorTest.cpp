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

#include "BlockFetcherHelper.h"
#include "catch.hpp"
#include "fakeit.hpp"

#include "Aql/AqlItemBlock.h"
#include "Aql/Collection.h"
#include "Aql/EnumerateCollectionExecutor.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/ResourceUsage.h"
#include "Aql/SingleRowFetcher.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"
#include "VocBase/AccessMode.h"
#include "VocBase/LogicalCollection.h"

#include "arangod/Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>
#include <functional>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

using CursorType = arangodb::transaction::Methods::CursorType;

SCENARIO("EnumerateCollectionExecutor",
         "[AQL][EXECUTOR][ENUMERATECOLLECTIONEXECUTOR]") {
  ExecutionState state;

  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor};

  GIVEN("there are no rows upstream") {
    fakeit::Mock<ExecutionEngine> mockEngine;

    // fake transaction::Methods
    fakeit::Mock<transaction::Methods> mockTrx;
    // fake indexScan
    fakeit::Mock<OperationCursor> mockCursor;
    fakeit::Fake(Dtor(mockCursor));
    fakeit::Fake(Method(mockCursor, reset));
    OperationCursor& cursor = mockCursor.get();  // required as return value for index scan
    fakeit::When(Method(mockTrx, indexScan))
        .AlwaysDo(std::function<std::unique_ptr<OperationCursor>(std::string const&, CursorType&)>(
            [&cursor](std::string const&, CursorType&) -> std::unique_ptr<OperationCursor> {
              return std::unique_ptr<OperationCursor>(&cursor);
            }));
    // fake transaction::Methods - end

    fakeit::Mock<TRI_vocbase_t> vocbaseMock;
    TRI_vocbase_t& vocbase = vocbaseMock.get();  // required to create collection

    // parameters for infos in order of ctor
    Variable outVariable("name", 1);
    bool varUsedLater = false;
    std::unordered_set<RegisterId> const regToClear{};
    std::unordered_set<RegisterId> const regToKeep{};
    ExecutionEngine& engine = mockEngine.get();
    Collection const abc("blabli", &vocbase, arangodb::AccessMode::Type::READ);
    std::vector<std::string> const projections;
    transaction::Methods& trx = mockTrx.get();
    std::vector<size_t> const coveringIndexAttributePositions;
    bool allowCoveringIndexOptimization = false;
    bool useRawPointers = false;
    bool random = false;

    EnumerateCollectionExecutorInfos infos(0 /*outReg*/, 1 /*nrIn*/, 1 /*nrOut*/,
                                           regToClear, regToKeep, &engine, &abc,
                                           &outVariable, varUsedLater, projections,
                                           &trx, coveringIndexAttributePositions,
                                           allowCoveringIndexOptimization,
                                           useRawPointers, random);

    auto block = std::make_unique<AqlItemBlock>(&monitor, 1000, 2);
    auto outputBlockShell =
        std::make_shared<AqlItemBlockShell>(itemBlockManager, std::move(block));
    VPackBuilder input;

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper<false> fetcher(input.steal(), false);
      EnumerateCollectionExecutor testee(fetcher, infos);
      // Use this instead of std::ignore, so the tests will be noticed and
      // updated when someone changes the stats type in the return value of
      // EnumerateCollectionExecutor::produceRow().
      EnumerateCollectionStats stats{};

      THEN("the executor should return DONE") {
        OutputAqlItemRow result(std::move(outputBlockShell), infos.getOutputRegisters(),
                                infos.registersToKeep(), infos.registersToClear());
        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());
      }
    }

    WHEN("the producer waits") {
      SingleRowFetcherHelper<false> fetcher(input.steal(), true);
      EnumerateCollectionExecutor testee(fetcher, infos);
      // Use this instead of std::ignore, so the tests will be noticed and
      // updated when someone changes the stats type in the return value of
      // EnumerateCollectionExecutor::produceRow().
      EnumerateCollectionStats stats{};

      THEN("the executor should first return WAIT") {
        OutputAqlItemRow result(std::move(outputBlockShell), infos.getOutputRegisters(),
                                infos.registersToKeep(), infos.registersToClear());
        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        AND_THEN("the executor should return DONE") {
          std::tie(state, stats) = testee.produceRow(result);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!result.produced());
        }
      }
    }
  }
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
