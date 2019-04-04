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
#include "Aql/CountCollectExecutor.h"
#include "Aql/ExecutorInfos.h"
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

SCENARIO("CountCollectExecutor", "[AQL][EXECUTOR][COUNTCOLLECTEXECUTOR]") {
  ExecutionState state;

  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager(&monitor);

  RegisterId nrOutputReg = 2;

  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, nrOutputReg)};
  auto outputRegisters = std::make_shared<const std::unordered_set<RegisterId>>(
      std::initializer_list<RegisterId>{1});

  GIVEN("there are no rows upstream") {
    CountCollectExecutorInfos infos(1 /* outputRegId */, 1 /* nrIn */,
                                    nrOutputReg, {}, {});
    VPackBuilder input;

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper<false> fetcher(input.steal(), false);
      CountCollectExecutor testee(fetcher, infos);
      NoStats stats{};

      THEN("the executor should return 0") {
        OutputAqlItemRow result{std::move(block), outputRegisters,
                                infos.registersToKeep(), infos.registersToClear()};
        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(result.produced());

        auto block = result.stealBlock();
        AqlValue x = block->getValue(0, 1);
        REQUIRE(x.isNumber());
        REQUIRE(x.toInt64() == 0);
      }
    }

    WHEN("the producer does wait") {
      SingleRowFetcherHelper<false> fetcher(input.steal(), true);
      CountCollectExecutor testee(fetcher, infos);
      NoStats stats{};

      THEN("the executor should return 0") {
        OutputAqlItemRow result{std::move(block), outputRegisters,
                                infos.registersToKeep(), infos.registersToClear()};
        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(result.produced());

        auto block = result.stealBlock();
        AqlValue x = block->getValue(0, 1);
        REQUIRE(x.isNumber());
        REQUIRE(x.toInt64() == 0);
      }
    }
  }

  GIVEN("there are rows in the upstream") {
    CountCollectExecutorInfos infos(1 /* outputRegId */, 1 /* nrIn */,
                                    nrOutputReg, {}, {});

    WHEN("the producer does not wait") {
      auto input = VPackParser::fromJson("[ [1], [2], [3] ]");
      SingleRowFetcherHelper<false> fetcher(input->steal(), false);
      CountCollectExecutor testee(fetcher, infos);
      NoStats stats{};

      THEN("the executor should return 3") {
        OutputAqlItemRow result{std::move(block), outputRegisters,
                                infos.registersToKeep(), infos.registersToClear()};
        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(result.produced());

        auto block = result.stealBlock();
        AqlValue x = block->getValue(0, 1);
        REQUIRE(x.isNumber());
        REQUIRE(x.toInt64() == 3);
      }
    }

    WHEN("the producer does wait") {
      auto input = VPackParser::fromJson("[ [1], [2], [3] ]");
      SingleRowFetcherHelper<false> fetcher(input->steal(), true);
      CountCollectExecutor testee(fetcher, infos);
      NoStats stats{};
      THEN("the executor should return 3") {
        OutputAqlItemRow result{std::move(block), outputRegisters,
                                infos.registersToKeep(), infos.registersToClear()};

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        std::tie(state, stats) = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(result.produced());

        auto block = result.stealBlock();
        AqlValue x = block->getValue(0, 1);
        REQUIRE(x.isNumber());
        REQUIRE(x.toInt64() == 3);
      }
    }
  }
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
