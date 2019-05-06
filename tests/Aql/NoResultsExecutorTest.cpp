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

#include "RowFetcherHelper.h"
#include "catch.hpp"

#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/NoResultsExecutor.h"
#include "Aql/ResourceUsage.h"
#include "Aql/SingleRowFetcher.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

SCENARIO("NoResultsExecutor", "[AQL][EXECUTOR][NORESULTS]") {
  ExecutionState state;

  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager(&monitor);
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 1)};
  auto outputRegisters = make_shared_unordered_set();
  auto registersToClear = make_shared_unordered_set();
  auto registersToKeep = make_shared_unordered_set();

  RegisterId inputRegister(0);
  ExecutorInfos infos(make_shared_unordered_set({inputRegister}), outputRegisters,
                      1 /*nr in*/, 1 /*nr out*/, *registersToClear, *registersToKeep);
  OutputAqlItemRow result{std::move(block), outputRegisters, registersToKeep, registersToClear};

  GIVEN("there are no rows upstream") {
    VPackBuilder input;

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper<false> fetcher(input.steal(), false);
      NoResultsExecutor testee(fetcher, infos);
      NoStats stats{};

      THEN("the executor should return DONE and produce nothing") {
        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());
        REQUIRE(fetcher.nrCalled() == 0);
      }
    }

    WHEN("the producer waits") {
      SingleRowFetcherHelper<false> fetcher(input.steal(), true);
      NoResultsExecutor testee(fetcher, infos);
      NoStats stats{};

      THEN("the executor should return DONE and produce nothing") {
        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());
        REQUIRE(fetcher.nrCalled() == 0);

        AND_THEN("The output should stay stable") {
          std::tie(state, stats) = testee.produceRows(result);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!result.produced());
          REQUIRE(fetcher.nrCalled() == 0);
        }
      }
    }
  }

  GIVEN("there are rows in the upstream") {
    auto input = VPackParser::fromJson("[ [true], [false], [true] ]");

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper<false> fetcher(input->steal(), false);
      NoResultsExecutor testee(fetcher, infos);
      NoStats stats{};

      THEN("the executor should return DONE and produce nothing") {
        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());
        REQUIRE(fetcher.nrCalled() == 0);

        AND_THEN("The output should stay stable") {
          std::tie(state, stats) = testee.produceRows(result);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!result.produced());
          REQUIRE(fetcher.nrCalled() == 0);
        }
      }
    }

    WHEN("the producer waits") {
      SingleRowFetcherHelper<false> fetcher(input->steal(), true);
      NoResultsExecutor testee(fetcher, infos);
      NoStats stats{};

      THEN("the executor should return DONE and produce nothing") {
        std::tie(state, stats) = testee.produceRows(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());
        REQUIRE(fetcher.nrCalled() == 0);

        AND_THEN("The output should stay stable") {
          std::tie(state, stats) = testee.produceRows(result);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!result.produced());
          REQUIRE(fetcher.nrCalled() == 0);
        }
      }
    }
  }  // GIVERN
}  // SCENARIO

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
