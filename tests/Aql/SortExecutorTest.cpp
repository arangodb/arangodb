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
#include "fakeit.hpp"

#include "RowFetcherHelper.h"

#include "Aql/AllRowsFetcher.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/ResourceUsage.h"
#include "Aql/SortExecutor.h"
#include "Aql/SortRegister.h"
#include "Aql/Variable.h"
#include "Transaction/Context.h"
#include "Transaction/Methods.h"

#include "search/sort.hpp"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

int compareAqlValues(irs::sort::prepared const*, arangodb::transaction::Methods* trx,
                     arangodb::aql::AqlValue const& lhs,
                     arangodb::aql::AqlValue const& rhs) {
  return arangodb::aql::AqlValue::Compare(trx, lhs, rhs, true);
}

SCENARIO("SortExecutor", "[AQL][EXECUTOR]") {
  ExecutionState state;

  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor};
  SharedAqlItemBlockPtr block{new AqlItemBlock(itemBlockManager, 1000, 1)};

  // Mock of the Transaction
  // Enough for this test, will only be passed through and accessed
  // on documents alone.
  fakeit::Mock<transaction::Methods> mockTrx;
  transaction::Methods& trx = mockTrx.get();

  fakeit::Mock<transaction::Context> mockContext;
  transaction::Context& ctxt = mockContext.get();

  fakeit::When(Method(mockTrx, transactionContextPtr)).AlwaysReturn(&ctxt);
  fakeit::When(Method(mockContext, getVPackOptions)).AlwaysReturn(&arangodb::velocypack::Options::Defaults);

  Variable sortVar("mySortVar", 0);
  std::vector<SortRegister> sortRegisters;
  SortElement sl{&sortVar, true};

  SortRegister sortReg(0, sl);

  sortRegisters.emplace_back(std::move(sortReg));

  SortExecutorInfos infos(std::move(sortRegisters),
                          /*limit (ignored for default sort)*/ 0,
                          itemBlockManager, 1, 1, {}, {0}, &trx, false);

  GIVEN("there are no rows upstream") {
    VPackBuilder input;

    WHEN("the producer does not wait") {
      AllRowsFetcherHelper fetcher(input.steal(), false);
      SortExecutor testee(fetcher, infos);
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
      AllRowsFetcherHelper fetcher(input.steal(), true);
      SortExecutor testee(fetcher, infos);
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

  GIVEN("there are rows from upstream, and we are waiting") {
    std::shared_ptr<VPackBuilder> input;

    WHEN("it is a simple list of numbers") {
      input = VPackParser::fromJson("[[5],[3],[1],[2],[4]]");
      AllRowsFetcherHelper fetcher(input->steal(), true);
      SortExecutor testee(fetcher, infos);
      // Use this instead of std::ignore, so the tests will be noticed and
      // updated when someone changes the stats type in the return value of
      // EnumerateListExecutor::produceRows().
      NoStats stats{};

      THEN("we will hit waiting 5 times") {
        OutputAqlItemRow result{std::move(block), infos.getOutputRegisters(),
                                infos.registersToKeep(), infos.registersToClear()};
        // Wait, 5, Wait, 3, Wait, 1, Wait, 2, Wait, 4, HASMORE
        for (size_t i = 0; i < 5; ++i) {
          std::tie(state, stats) = testee.produceRows(result);
          REQUIRE(state == ExecutionState::WAITING);
          REQUIRE(!result.produced());
        }

        AND_THEN("we produce the rows in order") {
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

          result.advanceRow();

          std::tie(state, stats) = testee.produceRows(result);
          REQUIRE(state == ExecutionState::HASMORE);
          REQUIRE(result.produced());

          result.advanceRow();

          std::tie(state, stats) = testee.produceRows(result);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(result.produced());

          block = result.stealBlock();
          AqlValue v = block->getValue(0, 0);
          REQUIRE(v.isNumber());
          int64_t number = v.toInt64();
          REQUIRE(number == 1);

          v = block->getValue(1, 0);
          REQUIRE(v.isNumber());
          number = v.toInt64();
          REQUIRE(number == 2);

          v = block->getValue(2, 0);
          REQUIRE(v.isNumber());
          number = v.toInt64();
          REQUIRE(number == 3);

          v = block->getValue(3, 0);
          REQUIRE(v.isNumber());
          number = v.toInt64();
          REQUIRE(number == 4);

          v = block->getValue(4, 0);
          REQUIRE(v.isNumber());
          number = v.toInt64();
          REQUIRE(number == 5);
        }
      }
    }
  }
}
}  // namespace aql
}  // namespace tests
}  // namespace arangodb
