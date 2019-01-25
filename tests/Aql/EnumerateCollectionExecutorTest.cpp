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
#include "Aql/EnumerateCollectionExecutor.h"
#include "Aql/Collection.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/ResourceUsage.h"
#include "Aql/SingleRowFetcher.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"
#include "StorageEngine/EngineSelectorFeature.h"
#include "StorageEngine/StorageEngine.h"
#include "VocBase/AccessMode.h"
#include "VocBase/LogicalCollection.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

SCENARIO("EnumerateCollectionExecutor", "[AQL][EXECUTOR][ENUMERATECOLLECTIONEXECUTOR]") {
  ExecutionState state;

  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor};

  GIVEN("there are no rows upstream") {
    // EnumerateCollectionExecutorInfos infos(0, 1, 1, 2, {});
    fakeit::Mock<ExecutionEngine> mockEngine;
    ExecutionEngine& engine = mockEngine.get();

    fakeit::Mock<transaction::Methods> mockTrx;
    transaction::Methods& trx = mockTrx.get();

    // Slice = VPackSlice::emptyObjectSlice();
    Variable _outVariable("name", 1);
    std::vector<std::string> const projections;
    bool _varUsedLater = false;
    std::vector<size_t> const coveringIndexAttributePositions;
    bool allowCoveringIndexOptimization = false;
    bool _random = false;
    std::unordered_set<RegisterId> const regToClear;
    // TRI_vocbase_t vocbase(TRI_vocbase_type_e::TRI_VOCBASE_TYPE_NORMAL, 1, "testVocbase");

    fakeit::Mock<TRI_vocbase_t> vocbaseMock;
    TRI_vocbase_t& vocbase = vocbaseMock.get();

    fakeit::When(Method(mockTrx, indexScan)).AlwaysDo([](){return std::unique_ptr<OperationCursor>(nullptr);});

    // Collection(std::string const&, TRI_vocbase_t*, AccessMode::Type);
    Collection const abc("blabli", &vocbase, arangodb::AccessMode::Type::READ);
    bool useRawPointers = false;

    EnumerateCollectionExecutorInfos infos(
        0, 1, 1, regToClear, &engine, &abc, &_outVariable, _varUsedLater, projections,
        &trx, coveringIndexAttributePositions, allowCoveringIndexOptimization,
        useRawPointers, _random);

    auto block = std::make_unique<AqlItemBlock>(&monitor, 1000, 2);
    auto outputBlockShell =
        std::make_unique<OutputAqlItemBlockShell>(itemBlockManager, std::move(block),
                                                  infos.getOutputRegisters(),
                                                  infos.registersToKeep());
    VPackBuilder input;

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper fetcher(input.steal(), false);
      EnumerateCollectionExecutor testee(fetcher, infos);
      // Use this instead of std::ignore, so the tests will be noticed and
      // updated when someone changes the stats type in the return value of
      // EnumerateCollectionExecutor::produceRow().
      EnumerateCollectionStats stats{};

      THEN("the executor should return DONE") {
        OutputAqlItemRow result(std::move(outputBlockShell));
        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());
      }
    }

    WHEN("the producer waits") {
      SingleRowFetcherHelper fetcher(input.steal(), true);
      EnumerateCollectionExecutor testee(fetcher, infos);
      // Use this instead of std::ignore, so the tests will be noticed and
      // updated when someone changes the stats type in the return value of
      // EnumerateCollectionExecutor::produceRow().
      EnumerateCollectionStats stats{};

      THEN("the executor should first return WAIT") {
        OutputAqlItemRow result(std::move(outputBlockShell));
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
