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
#include "Aql/InputAqlItemRow.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/FilterExecutor.h"
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
  AqlItemBlock block(&monitor, 1000, 1);

  ExecutorInfos infos(0, 0, 1, 1, {});

  GIVEN("there are no rows upstream") {
    VPackBuilder input;

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper fetcher(input.steal(), false);
      FilterExecutor testee(fetcher, infos);

      THEN("the executor should return DONE with nullptr") {
        OutputAqlItemRow result(&block, infos.registersToKeep());
        state = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());
      }
    }

    WHEN("the producer waits") {
      SingleRowFetcherHelper fetcher(input.steal(), true);
      FilterExecutor testee(fetcher, infos);

      THEN("the executor should first return WAIT with nullptr") {
        OutputAqlItemRow result(&block, infos.registersToKeep());
        state = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        AND_THEN("the executor should return DONE with nullptr") {
          state = testee.produceRow(result);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!result.produced());
        }
      }
    }
  }

  GIVEN("there are rows in the upstream") {
    auto input =
        VPackParser::fromJson("[ [true], [false], [true], [false], [true] ]");

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper fetcher(input->steal(), true);
      FilterExecutor testee(fetcher, infos);

      THEN("the executor should return DONE with nullptr") {
        std::size_t current = 0;
        OutputAqlItemRow row(&block, infos.registersToKeep());

        /*
        1  produce => WAIT                 RES1
        2  produce => HASMORE, Row 1     RES1
        3  => WAIT                         RES2
        4  => WAIT                         RES2
        5   => HASMORE, Row 3            RES2
        6   => WAIT,                       RES3
        7   => WAIT,                       RES3
        8   => DONE, Row 5               RES3
        */

        //1
        state = testee.produceRow(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());

        //2
        state = testee.produceRow(row);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(row.produced());
        row.changeRow(++current);

        //3
        state = testee.produceRow(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());

        //4
        state = testee.produceRow(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());

        //5
        state = testee.produceRow(row);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(row.produced());
        row.changeRow(++current);

        //6
        state = testee.produceRow(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());

        //7
        state = testee.produceRow(row);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!row.produced());

        //8
        state = testee.produceRow(row);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(row.produced());
        row.changeRow(++current);
      }
    }
  }

  GIVEN("there are rows in the upstream") {
    auto input = VPackParser::fromJson(
        "[ [true], [false], [true], [false], [true], [false] ]");

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper fetcher(input->steal(), true);
      FilterExecutor testee(fetcher, infos);

      THEN("the executor should return DONE with nullptr") {
        OutputAqlItemRow result(&block, infos.registersToKeep());

        /*
        produce => WAIT                  RES1
        produce => HASMORE, Row 1        RES1
        => WAIT                          RES2
        => WAIT                          RES2
         => HASMORE, Row 3               RES2
         => WAIT,                        RES3
         => WAIT,                        RES3
         => HASMORE, Row 5               RES3
         => WAITING, Row 6               RES3
         => DONE, no output!             RES3
          */

        state = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        state = testee.produceRow(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());

        result.advanceRow();

        state = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        state = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        state = testee.produceRow(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());

        result.advanceRow();

        state = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        state = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        state = testee.produceRow(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());

        result.advanceRow();

        state = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        state = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());
      }
    }
  }
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
