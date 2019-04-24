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
#include "Aql/ConstFetcher.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/IdExecutor.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/ResourceUsage.h"

#include "tests/Aql/BlockFetcherHelper.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

SCENARIO("IdExecutor", "[AQL][EXECUTOR][ID]") {
  ExecutionState state;

  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager(&monitor);
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 1)};
  auto outputRegisters = make_shared_unordered_set();
  auto registersToKeep = make_shared_unordered_set({0});  // this must be set correctly

  IdExecutorInfos infos(1 /*nrRegs*/, *registersToKeep /*toKeep*/, {} /*toClear*/);
  OutputAqlItemRow row{std::move(block), outputRegisters, registersToKeep,
                       infos.registersToClear()};
  GIVEN("there are no rows upstream") {
    WHEN("the producer does not wait") {
      ConstFetcherHelper fetcher(itemBlockManager, nullptr);
      IdExecutor<ConstFetcher> testee(fetcher, infos);
      NoStats stats{};

      THEN("the executor should return DONE with no block produced") {
        std::tie(state, stats) = testee.produceRows(row);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!row.produced());
      }
    }
  }

  GIVEN("there are rows in the upstream") {
    auto input = VPackParser::fromJson("[ [true], [false], [true] ]");

    WHEN("the producer does not wait") {
      ConstFetcherHelper fetcher(itemBlockManager, input->buffer());
      IdExecutor<ConstFetcher> testee(fetcher, infos);
      NoStats stats{};

      THEN("the executor should return the rows") {
        std::tie(state, stats) = testee.produceRows(row);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(row.produced());
        row.advanceRow();

        std::tie(state, stats) = testee.produceRows(row);
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(row.produced());
        row.advanceRow();

        std::tie(state, stats) = testee.produceRows(row);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(row.produced());
        row.advanceRow();

        AND_THEN("The output should stay stable") {
          std::tie(state, stats) = testee.produceRows(row);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!row.produced());
        }

        // verify result
        AqlValue value;
        auto block = row.stealBlock();
        for (std::size_t index = 0; index < 3; index++) {
          value = block->getValue(index, 0);
          REQUIRE(value.isBoolean());
          REQUIRE(value.toBoolean() == input->slice().at(index).at(0).getBool());
        }

      }  // WHEN
    }    // GIVEN

  }  // GIVEN
}  // SCENARIO

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
