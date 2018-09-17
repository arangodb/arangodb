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
#include "fakeit.hpp"

#include "Aql/AqlItemBlock.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/EnumerateListExecutor.h"
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

SCENARIO("EnumerateListExecutor", "[AQL][EXXECUTOR]") {
  ExecutionState state;

  ResourceMonitor monitor;
  // Mock of the Transaction
  // Enough for this test, will only be passed through and accessed
  // on documents alone.
  fakeit::Mock<transaction::Methods> mockTrx;
  transaction::Methods& trx = mockTrx.get();

  fakeit::Mock<transaction::Context> mockContext;
  transaction::Context& ctxt = mockContext.get();

  fakeit::When(Method(mockTrx, transactionContextPtr)).AlwaysReturn(&ctxt);
  fakeit::When(Method(mockContext, getVPackOptions)).AlwaysReturn(&arangodb::velocypack::Options::Defaults);

  EnumerateListExecutorInfos infos(0, 1, 2, 1, {}, &trx);

  GIVEN("there are no rows upstream") {
    auto block = std::make_unique<AqlItemBlock>(&monitor, 1000, 1);
    VPackBuilder input;

    WHEN("the producer does not wait") {
      SingleRowFetcherHelper fetcher(input.steal(), false);
      EnumerateListExecutor testee(fetcher, infos);

      THEN("the executor should return DONE with nullptr") {
        OutputAqlItemRow result(std::move(block), infos);
        state = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());
      }
    }

    WHEN("the producer waits") {
      SingleRowFetcherHelper fetcher(input.steal(), true);
      EnumerateListExecutor testee(fetcher, infos);

      THEN("the executor should first return WAIT with nullptr") {
        OutputAqlItemRow result(std::move(block), infos);
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
    EnumerateListExecutorInfos infos(3, 4, 5, 4, {}, &trx);
    auto block = std::make_unique<AqlItemBlock>(&monitor, 1000, 5);
    auto input =
        VPackParser::fromJson("[ [1, 2, 3, [true, true, true]] ]");

    WHEN("the producer does wait") {
      SingleRowFetcherHelper fetcher(input->steal(), true);
      EnumerateListExecutor testee(fetcher, infos);

      THEN("the executor should return DONE with nullptr") {
        OutputAqlItemRow result(std::move(block), infos);

        /*
       1  produce => WAIT                 RES1
       2  produce => HASMORE              RES1
       3  produce => WAIT                 RES2
       4  produce => HASMORE              RES2
       5  produce => WAIT                 RES3
       6  produce => DONE                 RES3
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
        REQUIRE(state == ExecutionState::HASMORE);
        REQUIRE(result.produced());

        result.advanceRow();

        state = testee.produceRow(result);
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(!result.produced());

        state = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(result.produced());

        result.advanceRow();
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.produced());
      }
    }
  }
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
