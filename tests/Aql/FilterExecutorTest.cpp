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

#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/FilterExecutor.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/ResourceUsage.h"
#include "Aql/SingleRowFetcher.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

SCENARIO("FilterExecutor", "[AQL][EXECUTOR]") {
  ExecutionState state;

  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager(&monitor);
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 1)};
  auto outputRegisters = make_shared_unordered_set();
  auto& registersToKeep = outputRegisters;

  FilterExecutorInfos infos(0, 1, 1, {}, {});

  GIVEN("there are no rows upstream") {
    VPackBuilder input;

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper<false> fetcher(input.steal(), false);
      FilterExecutor testee(fetcher, infos);
      FilterStats stats{};

      THEN("the executor should return DONE with nullptr") {
        OutputAqlItemRow result(std::move(block), outputRegisters,
                                registersToKeep, infos.registersToClear());
        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());
      }
    }

    WHEN("the producer waits") {
      SingleRowFetcherHelper<false> fetcher(input.steal(), true);
      FilterExecutor testee(fetcher, infos);
      FilterStats stats{};

      THEN("the executor should first return WAIT with nullptr") {
        OutputAqlItemRow result(std::move(block), outputRegisters,
                                registersToKeep, infos.registersToClear());
        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());
        REQUIRE(stats.getFiltered() == 0);

        AND_THEN("the executor should return DONE with nullptr") {
          std::tie(state, stats) = testee.produceRow(result);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!result.produced());
          REQUIRE(stats.getFiltered() == 0);
        }
      }
    }
  }

  GIVEN("there are rows in the upstream") {
    auto input = VPackParser::fromJson(
        "[ [true], [false], [true], [false], [false], [true] ]");

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper<false> fetcher(input->steal(), false);
      FilterExecutor testee(fetcher, infos);
      FilterStats stats{};

      THEN("the executor should return the rows") {
        OutputAqlItemRow row(std::move(block), outputRegisters, registersToKeep,
                             infos.registersToClear());

        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(stats.getFiltered() == 0);
        REQUIRE(row.produced());

        row.advanceRow();

        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(stats.getFiltered() == 1);
        REQUIRE(row.produced());

        row.advanceRow();

        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(stats.getFiltered() == 2);
        REQUIRE(row.produced());

        row.advanceRow();

        AND_THEN("The output should stay stable") {
          std::tie(state, stats) = testee.produceRow(row);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(stats.getFiltered() == 0);
          REQUIRE(!row.produced());
        }
      }
    }

    WHEN("the producer waits") {
      SingleRowFetcherHelper<false> fetcher(input->steal(), true);
      FilterExecutor testee(fetcher, infos);
      FilterStats stats{};

      THEN("the executor should return the rows") {
        OutputAqlItemRow row(std::move(block), outputRegisters, registersToKeep,
                             infos.registersToClear());

        /*
        1  produce => WAIT                 RES1
        2  produce => HASMORE, Row 1     RES1
        3  => WAIT                         RES2
        4  => WAIT                         RES2
        5   => HASMORE, Row 3            RES2
        6   => WAIT,                       RES3
        7   => WAIT,                       RES3
        8   => WAIT,                       RES3
        9   => DONE, Row 6               RES3
        */

        // 1
        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());
        REQUIRE(stats.getFiltered() == 0);

        // 2
        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(row.produced());
        row.advanceRow();
        REQUIRE(stats.getFiltered() == 0);

        // 3
        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());
        REQUIRE(stats.getFiltered() == 0);

        // 4
        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());
        // We have one filter here
        REQUIRE(stats.getFiltered() == 1);

        // 5
        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(row.produced());
        row.advanceRow();
        REQUIRE(stats.getFiltered() == 0);

        // 6
        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());
        REQUIRE(stats.getFiltered() == 0);

        // 7
        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());
        REQUIRE(stats.getFiltered() == 1);

        // 7
        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());
        REQUIRE(stats.getFiltered() == 1);

        // 8
        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(row.produced());
        row.advanceRow();
        REQUIRE(stats.getFiltered() == 0);
      }
    }
  }

  GIVEN("there are rows in the upstream, and the last one has to be filtered") {
    auto input = VPackParser::fromJson(
        "[ [true], [false], [true], [false], [false], [true], [false] ]");

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper<false> fetcher(input->steal(), false);
      FilterExecutor testee(fetcher, infos);
      FilterStats stats{};

      THEN("the executor should return the rows") {
        OutputAqlItemRow row(std::move(block), outputRegisters, registersToKeep,
                             infos.registersToClear());

        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(stats.getFiltered() == 0);
        REQUIRE(row.produced());

        row.advanceRow();

        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(stats.getFiltered() == 1);
        REQUIRE(row.produced());

        row.advanceRow();

        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(stats.getFiltered() == 2);
        REQUIRE(row.produced());

        row.advanceRow();

        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(stats.getFiltered() == 1);
        REQUIRE(!row.produced());

        AND_THEN("The output should stay stable") {
          std::tie(state, stats) = testee.produceRow(row);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(stats.getFiltered() == 0);
          REQUIRE(!row.produced());
        }
      }
    }

    WHEN("the producer waits") {
      SingleRowFetcherHelper<false> fetcher(input->steal(), true);
      FilterExecutor testee(fetcher, infos);
      FilterStats stats{};

      THEN("the executor should return the rows") {
        OutputAqlItemRow result(std::move(block), outputRegisters,
                                registersToKeep, infos.registersToClear());

        /*
        produce => WAIT                  RES1
        produce => HASMORE, Row 1        RES1
        => WAIT                          RES2
        => WAIT                          RES2
         => HASMORE, Row 3               RES2
         => WAIT,                        RES3
         => WAIT,                        RES3
         => WAIT,                        RES3
         => HASMORE, Row 6               RES3
         => WAITING,                     RES3
         => DONE, no output!             RES3
          */

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());
        REQUIRE(stats.getFiltered() == 0);

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());
        REQUIRE(stats.getFiltered() == 0);

        result.advanceRow();

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());
        REQUIRE(stats.getFiltered() == 0);

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());
        REQUIRE(stats.getFiltered() == 1);

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());
        REQUIRE(stats.getFiltered() == 0);

        result.advanceRow();

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());
        REQUIRE(stats.getFiltered() == 0);

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());
        REQUIRE(stats.getFiltered() == 1);

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());
        REQUIRE(stats.getFiltered() == 1);

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());
        REQUIRE(stats.getFiltered() == 0);

        result.advanceRow();

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());
        REQUIRE(stats.getFiltered() == 0);

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());
        REQUIRE(stats.getFiltered() == 1);
      }
    }
  }
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
