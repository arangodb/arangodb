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

#include "catch.hpp"
#include "BlockFetcherHelper.h"

#include "Aql/AllRowsFetcher.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemRow.h"
#include "Aql/BlockFetcherInterfaces.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/SortExecutor.h"
#include "Aql/ResourceUsage.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

SCENARIO("SortExecutor", "[AQL][EXECUTOR]") {
  ExecutionState state;

  ResourceMonitor monitor;
  AqlItemBlock block(&monitor, 1000, 1);

  ExecutorInfos infos(0, 0);

  GIVEN("there are no rows upstream") {
    VPackBuilder input;

    WHEN("the producer does not wait") {
      AllRowsFetcherHelper fetcher(input.steal(), false);
      SortExecutor testee(fetcher, infos);

      THEN("the executor should return DONE with nullptr") {
        AqlItemRow result(&block, 0, 1);
        state = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.hasValue());
      }
    }

    WHEN("the producer waits") {
      AllRowsFetcherHelper fetcher(input.steal(), true);
      SortExecutor testee(fetcher, infos);

      THEN("the executor should first return WAIT with nullptr") {
        AqlItemRow result(&block, 0, 1);
        state = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.hasValue());

        AND_THEN("the executor should return DONE with nullptr") {
          state = testee.produceRow(result);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!result.hasValue());
        }
      }

    }
  }
  
  GIVEN("there are rows from upstream, and we are waiting") {
    std::shared_ptr<VPackBuilder> input;

    WHEN("it is a simple list of numbers") {
      input = VPackParser::fromJson("[[5],[3],[1],[2],[4]]");
      AllRowsFetcherHelper fetcher(input->steal(), true);
      SortExecutor testee(fetcher, infos);

      THEN("we will hit waiting 5 times") {
        AqlItemRow firstResult(&block, 0, 1);
        // Wait, 5, Wait, 3, Wait, 1, Wait, 2, Wait, 4, HASMORE
        for (size_t i = 0; i < 5; ++i) {
          state = testee.produceRow(firstResult);
          REQUIRE(state == ExecutionState::WAITING);
          REQUIRE(!firstResult.hasValue());
        }

        AND_THEN("we procude the rows in order") {
          state = testee.produceRow(firstResult);
          REQUIRE(state == ExecutionState::HASMORE);
          REQUIRE(firstResult.hasValue());

          AqlItemRow secondResult(&block, 1, 1);
          state = testee.produceRow(secondResult);
          REQUIRE(state == ExecutionState::HASMORE);
          REQUIRE(secondResult.hasValue());

          AqlItemRow thirdResult(&block, 2, 1);
          state = testee.produceRow(thirdResult);
          REQUIRE(state == ExecutionState::HASMORE);
          REQUIRE(thirdResult.hasValue());

          AqlItemRow fourthResult(&block, 3, 1);
          state = testee.produceRow(fourthResult);
          REQUIRE(state == ExecutionState::HASMORE);
          REQUIRE(fourthResult.hasValue());

          AqlItemRow fifthResult(&block, 4, 1);
          state = testee.produceRow(fifthResult);
          REQUIRE(state == ExecutionState::HASMORE);
          REQUIRE(fifthResult.hasValue());

          AqlValue v = firstResult.getValue(0);
          REQUIRE(v.isNumber());
          int64_t number = v.toInt64(nullptr);
          REQUIRE(number == 1);

          v = secondResult.getValue(0);
          REQUIRE(v.isNumber());
          number = v.toInt64(nullptr);
          REQUIRE(number == 1);

          v = thirdResult.getValue(0);
          REQUIRE(v.isNumber());
          number = v.toInt64(nullptr);

          v = fourthResult.getValue(0);
          REQUIRE(v.isNumber());
          number = v.toInt64(nullptr);

          v = fifthResult.getValue(0);
          REQUIRE(v.isNumber());
          number = v.toInt64(nullptr);
        }
      }
    }
  }
}
} // aql
} // tests
} // arangodb
