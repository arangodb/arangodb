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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "BlockFetcherHelper.h"
#include "catch.hpp"

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemBlockShell.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/LimitExecutor.h"
#include "Aql/ResourceUsage.h"
#include "Aql/SingleRowFetcher.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

SCENARIO("LimitExecutor", "[AQL][EXECUTOR][COPYPASTEPANZER]") {
  ExecutionState state;

  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager(&monitor);
  auto block = std::make_unique<AqlItemBlock>(&monitor, 1000, 1);
  auto outputRegisters = std::make_shared<const std::unordered_set<RegisterId>>(0);
  auto& registersToKeep = outputRegisters;
  auto outputBlockShell =
      std::make_unique<OutputAqlItemBlockShell>(itemBlockManager, std::move(block),
                                                outputRegisters, registersToKeep);

  // Special parameters:
  // 4th offset
  // 5th limit
  // 6th fullCount
  // 7th queryDepth

  // TODO: This test is currently broken.
  // Waiting for fake query server merge

  GIVEN("there are no rows upstream") {
    LimitExecutorInfos infos(0, 1, {}, 0, 1, true, 0);
    VPackBuilder input;

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper fetcher(input.steal(), false);
      LimitExecutor testee(fetcher, infos);
      LimitStats stats{};

      THEN("the executor should return DONE with nullptr") {
        OutputAqlItemRow result(std::move(outputBlockShell));
        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());
        REQUIRE(stats.getFullCount() == 0);
      }
    }

    WHEN("the producer waits") {
      SingleRowFetcherHelper fetcher(input.steal(), true);
      LimitExecutor testee(fetcher, infos);
      LimitStats stats{};

      THEN("the executor should first return WAIT with nullptr") {
        OutputAqlItemRow result(std::move(outputBlockShell));
        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());
        REQUIRE(stats.getFullCount() == 0);

        AND_THEN("the executor should return DONE with nullptr") {
          std::tie(state, stats) = testee.produceRow(result);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!result.produced());
          REQUIRE(stats.getFullCount() == 0);
        }
      }
    }
  }

  GIVEN("there are rows in the upstream, no filter or offset defined") {
    auto input = VPackParser::fromJson(
        "[ [true], [false], [true], [false], [false], [true] ]");

    WHEN("the producer does not wait: limit 1, offset 0, fullcount false") {
      LimitExecutorInfos infos(0, 1, {}, 0, 1, false, 0);
      SingleRowFetcherHelper fetcher(input->steal(), false);
      LimitExecutor testee(fetcher, infos);
      LimitStats stats{};

      THEN("the executor should return one row") {
        OutputAqlItemRow row(std::move(outputBlockShell));

        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(row.produced());
        row.advanceRow();

        AND_THEN("The output should stay stable") {
          std::tie(state, stats) = testee.produceRow(row);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!row.produced());
        }
      }
    }

    /*
    WHEN("the producer does wait: limit 1, offset 0, fullcount false") {
      LimitExecutorInfos infos(0, 1, {}, 0, 1, false, 0);
      SingleRowFetcherHelper fetcher(input->steal(), true);
      LimitExecutor testee(fetcher, infos);
      LimitStats stats{};

      THEN("the executor should return one row") {
        OutputAqlItemRow row(std::move(outputBlockShell));

        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());

        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(row.produced());

        row.advanceRow();

        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());

        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());

        AND_THEN("The output should stay stable") {
          std::tie(state, stats) = testee.produceRow(row);
          // TODO DONE NOT RETURNED ...... but running into the part where done is returned. what is wrong?
          // Code: LimitExecutor.cpp:89 :(
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!row.produced());
        }
      }
    }
     */
  }
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
