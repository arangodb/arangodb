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
#include "Aql/DistinctCollectExecutor.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/SingleRowFetcher.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>
#include <functional>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

SCENARIO("DistinctCollectExecutor", "[AQL][EXECUTOR][DISTINCTCOLLETEXECUTOR]") {
  ExecutionState state;
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor};

  GIVEN("there are no rows upstream") {
    // fake transaction::Methods
    fakeit::Mock<transaction::Methods> mockTrx;
    // fake transaction::Methods - end

    fakeit::Mock<TRI_vocbase_t> vocbaseMock;
    TRI_vocbase_t& vocbase = vocbaseMock.get();  // required to create collection

    std::unordered_set<RegisterId> const regToClear;
    std::vector<std::pair<RegisterId, RegisterId>> groupRegisters;
    groupRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 2));

    std::unordered_set<RegisterId> readableInputRegisters;
    std::unordered_set<RegisterId> writeableOutputRegisters;

    transaction::Methods& trx = mockTrx.get();
    DistinctCollectExecutorInfos infos(2 /*nrIn*/, 2 /*nrOut*/, {},
                                       std::move(readableInputRegisters),
                                       std::move(writeableOutputRegisters),
                                       std::move(groupRegisters), &trx);

    auto block = std::make_unique<AqlItemBlock>(&monitor, 1000, 2);
    auto outputBlockShell =
        std::make_shared<AqlItemBlockShell>(itemBlockManager, std::move(block));
    VPackBuilder input;
    NoStats stats{};

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper<false> fetcher(input.steal(), false);
      DistinctCollectExecutor testee(fetcher, infos);

      THEN("the executor should return DONE") {
        OutputAqlItemRow result(std::move(outputBlockShell),
                                infos.getOutputRegisters(), infos.registersToKeep());
        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());
      }
    }

    WHEN("the producer waits") {
      SingleRowFetcherHelper<false> fetcher(input.steal(), true);
      DistinctCollectExecutor testee(fetcher, infos);

      THEN("the executor should first return WAIT") {
        OutputAqlItemRow result(std::move(outputBlockShell),
                                infos.getOutputRegisters(), infos.registersToKeep());
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

  GIVEN("there are rows in the upstream") {
    // fake transaction::Methods
    fakeit::Mock<transaction::Methods> mockTrx;
    // fake transaction::Methods - end

    fakeit::Mock<TRI_vocbase_t> vocbaseMock;
    TRI_vocbase_t& vocbase = vocbaseMock.get();  // required to create collection

    std::unordered_set<RegisterId> regToClear;
    std::vector<std::pair<RegisterId, RegisterId>> groupRegisters;
    groupRegisters.emplace_back(std::make_pair<RegisterId, RegisterId>(1, 0));

    std::unordered_set<RegisterId> readableInputRegisters;
    readableInputRegisters.insert(0);

    std::unordered_set<RegisterId> writeableOutputRegisters;
    writeableOutputRegisters.insert(1);

    RegisterId nrOutputRegister = 2;

    transaction::Methods& trx = mockTrx.get();
    DistinctCollectExecutorInfos infos(1 /*nrInputReg*/, nrOutputRegister /*nrOutputReg*/, regToClear,
                                       std::move(readableInputRegisters),
                                       std::move(writeableOutputRegisters),
                                       std::move(groupRegisters), &trx);

    auto block = std::make_unique<AqlItemBlock>(&monitor, 1000, nrOutputRegister);
    auto outputBlockShell =
        std::make_shared<AqlItemBlockShell>(itemBlockManager, std::move(block));
    NoStats stats{};

    WHEN("the producer does not wait") {
      auto input = VPackParser::fromJson("[ [1], [2], [1] ]");
      SingleRowFetcherHelper<false> fetcher(input->steal(), false);
      DistinctCollectExecutor testee(fetcher, infos);

      THEN("the executor should return DONE") {
        OutputAqlItemRow result(std::move(outputBlockShell),
                                infos.getOutputRegisters(), infos.registersToKeep());

        LOG_DEVEL << 1;
        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());
        result.advanceRow();

        LOG_DEVEL << 2;
        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());
        result.advanceRow();

        LOG_DEVEL << 3;
        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(result.produced());
        result.advanceRow();

        LOG_DEVEL << 4;
        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());
        LOG_DEVEL << 5;
      }
    }

  }
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
