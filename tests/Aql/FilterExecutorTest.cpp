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
#include "Aql/AqlItemRow.h"
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

  ExecutorInfos infos(0, 0);

  GIVEN("there are no rows upstream") {
    VPackBuilder input;

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper fetcher(input.steal(), false);
      FilterExecutor testee(fetcher, infos);

      THEN("the executor should return DONE with nullptr") {
        AqlItemRow result(block, 0, RegInfo{});
        state = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());
      }
    }

    WHEN("the producer waits") {
      SingleRowFetcherHelper fetcher(input.steal(), true);
      FilterExecutor testee(fetcher, infos);

      THEN("the executor should first return WAIT with nullptr") {
        AqlItemRow result(block, 0, RegInfo{});
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
        AqlItemRow result1(block, 0);
        AqlItemRow result2(block, 1);
        AqlItemRow result3(block, 2);
        /*
        produce => WAIT                 RES1
        produce => HASMORE, Row 1     RES1
        => WAIT                         RES2
        => WAIT                         RES2
         => HASMORE, Row 3            RES2
         => WAIT,                       RES3
         => WAIT,                       RES3
         => DONE, Row 5               RES3
          */

        state = testee.produceRow(result1);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result1.hasValue());

        state = testee.produceRow(result1);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result1.hasValue());

        state = testee.produceRow(result2);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result2.hasValue());

        state = testee.produceRow(result2);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result2.hasValue());

        state = testee.produceRow(result2);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result2.hasValue());

        state = testee.produceRow(result3);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result3.hasValue());

        state = testee.produceRow(result3);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result3.hasValue());

        state = testee.produceRow(result3);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(result3.hasValue());
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
        AqlItemRow result1(block, 0);
        AqlItemRow result2(block, 1);
        AqlItemRow result3(block, 2);
        AqlItemRow result4(block, 3);

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

        state = testee.produceRow(result1);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result1.hasValue());

        state = testee.produceRow(result1);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result1.hasValue());

        state = testee.produceRow(result2);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result2.hasValue());

        state = testee.produceRow(result2);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result2.hasValue());

        state = testee.produceRow(result2);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result2.hasValue());

        state = testee.produceRow(result3);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result3.hasValue());

        state = testee.produceRow(result3);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result3.hasValue());

        state = testee.produceRow(result3);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result3.hasValue());

        state = testee.produceRow(result4);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result4.hasValue());

        state = testee.produceRow(result4);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result4.hasValue());
      }
    }
  }
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
