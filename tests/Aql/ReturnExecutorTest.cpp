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
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "BlockFetcherHelper.h"
#include "catch.hpp"

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemBlockShell.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/ResourceUsage.h"
#include "Aql/ReturnExecutor.h"
#include "Aql/SingleRowFetcher.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

SCENARIO("ReturnExecutor", "[AQL][EXECUTOR][RETURN]") {
  ExecutionState state;

  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager(&monitor);
  auto block = std::make_unique<AqlItemBlock>(&monitor, 1000, 1);
  auto outputRegisters = make_shared_unordered_set({0});
  auto registersToKeep = make_shared_unordered_set();
  auto outputBlockShell =
      std::make_unique<OutputAqlItemBlockShell>(itemBlockManager, std::move(block),
                                                outputRegisters, registersToKeep);

  REQUIRE(outputRegisters->size()  == 1);
  REQUIRE(( *(outputRegisters->begin())  == 0 ));

  RegisterId inputRegister(0);
  RegisterId outputRegister(*(outputRegisters->begin()));

  ReturnExecutorInfos infos(inputRegister, outputRegister, 1 /*nr in*/ , 1 /*nr out*/,
                            std::unordered_set<RegisterId>{} /*to clear*/, false /*return inherit*/);

  GIVEN("there are no rows upstream") {
    VPackBuilder input;

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper fetcher(input.steal(), false);
      ReturnExecutor testee(fetcher, infos);
      NoStats stats{};

      THEN("the executor should return DONE with nullptr") {
        OutputAqlItemRow result(std::move(outputBlockShell));
        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());
      }
    }

    WHEN("the producer waits") {
      SingleRowFetcherHelper fetcher(input.steal(), true);
      ReturnExecutor testee(fetcher, infos);
      NoStats stats{};

      THEN("the executor should first return WAIT with nullptr") {
        OutputAqlItemRow result(std::move(outputBlockShell));
        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        AND_THEN("the executor should return DONE with nullptr") {
          std::tie(state, stats) = testee.produceRow(result);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!result.produced());
        }
      }
    }
  }

  GIVEN("there are rows in the upstream") {
    auto input = VPackParser::fromJson(
        "[ [true], [false], [true] ]");

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper fetcher(input->steal(), false);
      ReturnExecutor testee(fetcher, infos);
      NoStats stats{};

      THEN("the executor should return the rows") {
        OutputAqlItemRow row(std::move(outputBlockShell));

        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(row.produced());
        row.advanceRow();

        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(row.produced());
        row.advanceRow();

        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(row.produced());
        row.advanceRow();

        AND_THEN("The output should stay stable") {
          std::tie(state, stats) = testee.produceRow(row);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!row.produced());
        }
      }
    }

    WHEN("the producer waits") {
      SingleRowFetcherHelper fetcher(input->steal(), true);
      ReturnExecutor testee(fetcher, infos);
      NoStats stats{};

      THEN("the executor should return the rows") {
        OutputAqlItemRow row(std::move(outputBlockShell));

        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());

        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(row.produced());
        row.advanceRow();

        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());

        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(row.produced());
        row.advanceRow();

        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());

        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(row.produced());
        row.advanceRow();

        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!row.produced());
      }
    }
  } // GIVERN
} // SCENARIO

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
