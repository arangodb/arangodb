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

#include "RowFetcherHelper.h"
#include "catch.hpp"

#include "Aql/AqlItemBlock.h"
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

SCENARIO("LimitExecutor", "[AQL][EXECUTOR][LIMITEXECUTOR]") {
  ExecutionState state;

  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager(&monitor);
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 1)};
  auto outputRegisters = std::make_shared<const std::unordered_set<RegisterId>>(
      std::initializer_list<RegisterId>{});
  auto registersToKeep = std::make_shared<const std::unordered_set<RegisterId>>(
      std::initializer_list<RegisterId>{0});

  // Special parameters:
  // 4th offset
  // 5th limit
  // 6th fullCount
  // 7th queryDepth

  GIVEN("there are no rows upstream") {
    LimitExecutorInfos infos(1, 1, {}, {0}, 0, 1, true);
    VPackBuilder input;

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper<false> fetcher(input.steal(), false);
      LimitExecutor testee(fetcher, infos);
      LimitStats stats{};

      THEN("the executor should return DONE with nullptr") {
        OutputAqlItemRow result{std::move(block), outputRegisters,
                                registersToKeep, infos.registersToClear()};
        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());
        REQUIRE(stats.getFullCount() == 0);
      }
    }

    WHEN("the producer waits") {
      SingleRowFetcherHelper<false> fetcher(input.steal(), true);
      LimitExecutor testee(fetcher, infos);
      LimitStats stats{};

      THEN("the executor should first return WAIT") {
        OutputAqlItemRow result{std::move(block), outputRegisters,
                                registersToKeep, infos.registersToClear()};
        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());
        REQUIRE(stats.getFullCount() == 0);

        AND_THEN("the executor should return DONE") {
          std::tie(state, stats) = testee.produceRows(result);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!result.produced());
          REQUIRE(stats.getFullCount() == 0);
        }
      }
    }
  }

  GIVEN("there are rows in the upstream, no filter or offset defined") {
    WHEN("the producer does not wait: limit 1, offset 0, fullcount false") {
      auto input = VPackParser::fromJson("[ [1], [2], [3], [4] ]");
      LimitExecutorInfos infos(1, 1, {}, {0}, 0, 1, false);
      SingleRowFetcherHelper<false> fetcher(input->steal(), false);
      LimitExecutor testee(fetcher, infos);
      LimitStats stats{};

      THEN("the executor should return one row") {
        OutputAqlItemRow row{std::move(block), outputRegisters, registersToKeep,
                             infos.registersToClear()};

        std::tie(state, stats) = testee.produceRows(row);
        REQUIRE(row.produced());
        row.advanceRow();

        AND_THEN("The output should stay stable") {
          std::tie(state, stats) = testee.produceRows(row);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!row.produced());
        }
      }
    }

    WHEN("the producer does not wait: limit 1, offset 0, fullcount true") {
      auto input = VPackParser::fromJson("[ [1], [2], [3], [4] ]");
      LimitExecutorInfos infos(1, 1, {}, {0}, 0, 1, true);
      SingleRowFetcherHelper<false> fetcher(input->steal(), false);
      LimitExecutor testee(fetcher, infos);
      LimitStats stats{};

      THEN("the executor should return one row") {
        OutputAqlItemRow row{std::move(block), outputRegisters, registersToKeep,
                             infos.registersToClear()};

        std::tie(state, stats) = testee.produceRows(row);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(row.produced());

        row.advanceRow();

        AND_THEN("The output should stay stable") {
          std::tie(state, stats) = testee.produceRows(row);
          REQUIRE(!row.produced());
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(stats.getFullCount() == 3);
        }

        auto block = row.stealBlock();
        AqlValue value = block->getValue(0, 0);
        REQUIRE(value.isNumber());
        REQUIRE(value.toInt64() == 1);
      }
    }

    WHEN("the producer does not wait: limit 1, offset 1, fullcount true") {
      auto input = VPackParser::fromJson("[ [1], [2], [3], [4] ]");
      LimitExecutorInfos infos(1, 1, {}, {0}, 1, 1, true);
      SingleRowFetcherHelper<false> fetcher(input->steal(), false);
      LimitExecutor testee(fetcher, infos);
      LimitStats stats{};

      THEN("the executor should return one row") {
        OutputAqlItemRow row{std::move(block), outputRegisters, registersToKeep,
                             infos.registersToClear()};

        std::tie(state, stats) = testee.produceRows(row);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(row.produced());

        row.advanceRow();

        AND_THEN("The output should stay stable") {
          std::tie(state, stats) = testee.produceRows(row);
          REQUIRE(!row.produced());
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(stats.getFullCount() == 2);
        }

        auto block = row.stealBlock();
        AqlValue value = block->getValue(0, 0);
        REQUIRE(value.isNumber());
        REQUIRE(value.toInt64() == 2);
      }
    }

    WHEN("the producer does wait: limit 1, offset 0, fullcount false") {
      auto input = VPackParser::fromJson("[ [1], [2], [3], [4] ]");
      LimitExecutorInfos infos(1, 1, {}, {0}, 0, 1, false);
      SingleRowFetcherHelper<false> fetcher(input->steal(), true);
      LimitExecutor testee(fetcher, infos);
      LimitStats stats{};

      THEN("the executor should return one row") {
        OutputAqlItemRow row{std::move(block), outputRegisters, registersToKeep,
                             infos.registersToClear()};

        std::tie(state, stats) = testee.produceRows(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());

        std::tie(state, stats) = testee.produceRows(row);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(row.produced());

        row.advanceRow();

        AND_THEN("The output should stay stable") {
          std::tie(state, stats) = testee.produceRows(row);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!row.produced());
        }

        auto block = row.stealBlock();
        AqlValue value = block->getValue(0, 0);
        REQUIRE(value.isNumber());
        REQUIRE(value.toInt64() == 1);
      }
    }

    WHEN("the producer does wait: limit 1, offset 0, fullcount true") {
      auto input = VPackParser::fromJson("[ [1], [2], [3], [4] ]");
      LimitExecutorInfos infos(1, 1, {}, {0}, 0, 1, true);
      SingleRowFetcherHelper<false> fetcher(input->steal(), true);
      LimitExecutor testee(fetcher, infos);
      LimitStats stats{};

      THEN("the executor should return one row") {
        OutputAqlItemRow row{std::move(block), outputRegisters, registersToKeep,
                             infos.registersToClear()};

        std::tie(state, stats) = testee.produceRows(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());

        std::tie(state, stats) = testee.produceRows(row);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(row.produced());

        row.advanceRow();

        std::tie(state, stats) = testee.produceRows(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());

        std::tie(state, stats) = testee.produceRows(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());

        std::tie(state, stats) = testee.produceRows(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());

        AND_THEN("The output should stay stable") {
          std::tie(state, stats) = testee.produceRows(row);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(stats.getFullCount() == 1);
          REQUIRE(!row.produced());
        }

        auto block = row.stealBlock();
        AqlValue value = block->getValue(0, 0);
        REQUIRE(value.isNumber());
        REQUIRE(value.toInt64() == 1);
      }
    }
  }
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
