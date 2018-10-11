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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "BlockFetcherHelper.h"
#include "catch.hpp"

#include "Aql/AqlItemBlock.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/TraversalExecutor.h"
#include "Aql/ResourceUsage.h"
#include "Aql/SingleRowFetcher.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

SCENARIO("TraversalExecutor", "[AQL][EXECUTOR][TRAVEXE]") {
  ExecutionState state;

  ResourceMonitor monitor;
  auto block = std::make_unique<AqlItemBlock>(&monitor, 1000, 2);

  TraversalExecutorInfos infos({0}, {1}, 1, 2, {});

  GIVEN("there are no rows upstream") {
    VPackBuilder input;

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper fetcher(input.steal(), false);
      TraversalExecutor testee(fetcher, infos);
      TraversalStats stats{};

      THEN("the executor should return DONE with nullptr") {
        OutputAqlItemRow result(std::move(block), infos);
        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());
      }
    }

    WHEN("the producer waits") {
      SingleRowFetcherHelper fetcher(input.steal(), true);
      TraversalExecutor testee(fetcher, infos);
      TraversalStats stats{};

      THEN("the executor should first return WAIT with nullptr") {
        OutputAqlItemRow result(std::move(block), infos);
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

  /*
  GIVEN("there are rows in the upstream") {
    auto input =
        VPackParser::fromJson("[ [true], [false], [true], [false], [false], [true] ]");

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper fetcher(input->steal(), false);
      FilterExecutor testee(fetcher, infos);
      FilterStats stats{};

      THEN("the executor should return the rows") {
        OutputAqlItemRow row(std::move(block), infos);

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
      SingleRowFetcherHelper fetcher(input->steal(), true);
      FilterExecutor testee(fetcher, infos);
      FilterStats stats{};

      THEN("the executor should return the rows") {
        OutputAqlItemRow row(std::move(block), infos);

        //1
        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());
        REQUIRE(stats.getFiltered() == 0);

        //2
        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(row.produced());
        row.advanceRow();
        REQUIRE(stats.getFiltered() == 0);

        //3
        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());
        REQUIRE(stats.getFiltered() == 0);

        //4
        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());
        // We have one filter here
        REQUIRE(stats.getFiltered() == 1);

        //5
        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(row.produced());
        row.advanceRow();
        REQUIRE(stats.getFiltered() == 0);

        //6
        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());
        REQUIRE(stats.getFiltered() == 0);

        //7
        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());
        REQUIRE(stats.getFiltered() == 1);

        //7
        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());
        REQUIRE(stats.getFiltered() == 1);

        //8
        std::tie(state, stats) = testee.produceRow(row);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(row.produced());
        row.advanceRow();
        REQUIRE(stats.getFiltered() == 0);
      }
    }
  }
  */
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
