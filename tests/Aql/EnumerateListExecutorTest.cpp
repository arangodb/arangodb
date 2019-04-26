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

#include "RowFetcherHelper.h"
#include "catch.hpp"
#include "fakeit.hpp"

#include "Aql/AqlItemBlock.h"
#include "Aql/EnumerateListExecutor.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/ResourceUsage.h"
#include "Aql/SingleRowFetcher.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

SCENARIO("EnumerateListExecutor", "[AQL][EXECUTOR]") {
  ExecutionState state;

  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor};

  GIVEN("there are no rows upstream") {
    EnumerateListExecutorInfos infos(0, 1, 1, 2, {}, {0});
    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 2)};
    VPackBuilder input;

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper<false> fetcher(input.steal(), false);
      EnumerateListExecutor testee(fetcher, infos);
      // Use this instead of std::ignore, so the tests will be noticed and
      // updated when someone changes the stats type in the return value of
      // EnumerateListExecutor::produceRows().
      NoStats stats{};

      THEN("the executor should return DONE with nullptr") {
        OutputAqlItemRow result{std::move(block), infos.getOutputRegisters(),
                                infos.registersToKeep(), infos.registersToClear()};
        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());
      }
    }

    WHEN("the producer waits") {
      SingleRowFetcherHelper<false> fetcher(input.steal(), true);
      EnumerateListExecutor testee(fetcher, infos);
      // Use this instead of std::ignore, so the tests will be noticed and
      // updated when someone changes the stats type in the return value of
      // EnumerateListExecutor::produceRows().
      NoStats stats{};

      THEN("the executor should first return WAIT with nullptr") {
        OutputAqlItemRow result{std::move(block), infos.getOutputRegisters(),
                                infos.registersToKeep(), infos.registersToClear()};
        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        AND_THEN("the executor should return DONE with nullptr") {
          std::tie(state, stats) = testee.produceRows(result);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!result.produced());
        }
      }
    }
  }

  GIVEN("there is one row in the upstream") {
    EnumerateListExecutorInfos infos(3, 4, 4, 5, {}, {0, 1, 2, 3});
    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 5)};
    auto input = VPackParser::fromJson("[ [1, 2, 3, [true, true, true]] ]");

    WHEN("the producer does wait") {
      SingleRowFetcherHelper<false> fetcher(input->steal(), true);
      EnumerateListExecutor testee(fetcher, infos);
      // Use this instead of std::ignore, so the tests will be noticed and
      // updated when someone changes the stats type in the return value of
      // EnumerateListExecutor::produceRows().
      NoStats stats{};

      THEN("the executor should return DONE with nullptr") {
        OutputAqlItemRow result{std::move(block), infos.getOutputRegisters(),
                                infos.registersToKeep(), infos.registersToClear()};

        /*
         * Here we are not waiting after every row produce, because the fetcher
         * does not need to refetch a new row to produce the next one.
         * 1. produce => WAIT                 RES1  - due true flag in
         * SingleRowFetcherHelper
         * 2. produce => HASMORE              RES1 - return a row
         * 3. produce => HASMORE              RES2 - return a row
         * 4. produce => HASMORE              RES3 - return a row
         * 5. produce => DONE                 RES4 - DONE - do not return a row
         */

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());

        result.advanceRow();

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());

        result.advanceRow();

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(result.produced());

        result.advanceRow();

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());

        block = result.stealBlock();
        AqlValue v = block->getValue(0, 0);
        REQUIRE(v.isNumber());
        int64_t number = v.toInt64();
        REQUIRE(number == 1);

        v = block->getValue(1, 0);
        REQUIRE(v.isNumber());
        number = v.toInt64();
        REQUIRE(number == 1);

        v = block->getValue(1, 1);
        REQUIRE(v.isNumber());
        number = v.toInt64();
        REQUIRE(number == 2);

        v = block->getValue(1, 2);
        REQUIRE(v.isNumber());
        number = v.toInt64();
        REQUIRE(number == 3);

        bool mustDestroy = false;
        v = block->getValue(1, 3);
        REQUIRE(v.isArray());
        REQUIRE(v.at(0, mustDestroy, false).toBoolean() == true);
        REQUIRE(v.at(1, mustDestroy, false).toBoolean() == true);
        REQUIRE(v.at(2, mustDestroy, false).toBoolean() == true);

        v = block->getValue(1, 4);
        REQUIRE(v.isBoolean());
        REQUIRE(v.toBoolean() == true);
      }
    }
  }

  GIVEN("there is one empty array row in the upstream") {
    EnumerateListExecutorInfos infos(3, 4, 4, 5, {}, {0, 1, 2, 3});
    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 5)};
    auto input = VPackParser::fromJson("[ [1, 2, 3, [] ] ]");

    WHEN("the producer does wait") {
      SingleRowFetcherHelper<false> fetcher(input->steal(), true);
      EnumerateListExecutor testee(fetcher, infos);
      // Use this instead of std::ignore, so the tests will be noticed and
      // updated when someone changes the stats type in the return value of
      // EnumerateListExecutor::produceRows().
      NoStats stats{};

      THEN("the executor should return DONE with nullptr") {
        OutputAqlItemRow result{std::move(block), infos.getOutputRegisters(),
                                infos.registersToKeep(), infos.registersToClear()};

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());

        block = result.stealBlock();
        REQUIRE(block == nullptr);
      }
    }
  }

  GIVEN("there are rows in the upstream") {
    EnumerateListExecutorInfos infos(3, 4, 4, 5, {}, {0, 1, 2, 3});
    SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 5)};
    auto input = VPackParser::fromJson(
        "[ [1, 2, 3, [true, true, true]], [1, 2, 3, [true, true, true]] ]");

    WHEN("the producer does wait") {
      SingleRowFetcherHelper<false> fetcher(input->steal(), true);
      EnumerateListExecutor testee(fetcher, infos);
      // Use this instead of std::ignore, so the tests will be noticed and
      // updated when someone changes the stats type in the return value of
      // EnumerateListExecutor::produceRows().
      NoStats stats{};

      THEN("the executor should return DONE with nullptr") {
        OutputAqlItemRow result{std::move(block), infos.getOutputRegisters(),
                                infos.registersToKeep(), infos.registersToClear()};

        // like the test above, except now two rows of input
        // are available

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());

        result.advanceRow();

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());

        result.advanceRow();

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(result.produced());

        result.advanceRow();

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());

        result.advanceRow();

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());

        result.advanceRow();

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(result.produced());

        result.advanceRow();

        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());

        block = result.stealBlock();
        bool mustDestroy = false;

        // first row
        AqlValue v = block->getValue(0, 0);
        REQUIRE(v.isNumber());
        int64_t number = v.toInt64();
        REQUIRE(number == 1);

        v = block->getValue(1, 0);
        REQUIRE(v.isNumber());
        number = v.toInt64();
        REQUIRE(number == 1);

        v = block->getValue(1, 1);
        REQUIRE(v.isNumber());
        number = v.toInt64();
        REQUIRE(number == 2);

        v = block->getValue(1, 2);
        REQUIRE(v.isNumber());
        number = v.toInt64();
        REQUIRE(number == 3);

        v = block->getValue(1, 3);
        REQUIRE(v.isArray());
        REQUIRE(v.at(0, mustDestroy, false).toBoolean() == true);
        REQUIRE(v.at(1, mustDestroy, false).toBoolean() == true);
        REQUIRE(v.at(2, mustDestroy, false).toBoolean() == true);

        v = block->getValue(1, 4);
        REQUIRE(v.isBoolean());
        REQUIRE(v.toBoolean() == true);

        // second row
        v = block->getValue(2, 0);
        REQUIRE(v.isNumber());
        number = v.toInt64();
        REQUIRE(number == 1);

        v = block->getValue(2, 1);
        REQUIRE(v.isNumber());
        number = v.toInt64();
        REQUIRE(number == 2);

        v = block->getValue(2, 2);
        REQUIRE(v.isNumber());
        number = v.toInt64();
        REQUIRE(number == 3);

        v = block->getValue(2, 3);
        REQUIRE(v.isArray());
        REQUIRE(v.at(0, mustDestroy, false).toBoolean() == true);
        REQUIRE(v.at(1, mustDestroy, false).toBoolean() == true);
        REQUIRE(v.at(2, mustDestroy, false).toBoolean() == true);

        v = block->getValue(2, 4);
        REQUIRE(v.isBoolean());
        REQUIRE(v.toBoolean() == true);
      }
    }
  }
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
