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
/// @author Tobias Gödderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////


#include "catch.hpp"
#include "fakeit.hpp"
#include "BlockFetcherHelper.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemRow.h"
#include "Aql/BlockFetcher.h"
#include "Aql/ExecutionBlock.h"
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

SCENARIO("SingleRowFetcher", "[AQL][EXECUTOR]") {
  ExecutionState state;
  AqlItemRow const* row;

  GIVEN("there are no blocks upstream") {
    VPackBuilder input;
    fakeit::Mock<BlockFetcher> blockFetcherMock;

    // TODO this should not be called, throw an exception if it is
    fakeit::When(Method(blockFetcherMock,getNrInputRegisters)).Return(0);

    WHEN("the producer does not wait") {
      // Using .Return doesn't work here, as unique_ptr is not
      // copy-constructible.
      fakeit::When(Method(blockFetcherMock,fetchBlock))
        .Do([]{ return std::make_pair(ExecutionState::DONE, nullptr); });
      ;

      SingleRowFetcher testee(blockFetcherMock.get());

      THEN("the fetcher should return DONE with nullptr") {
        std::tie(state, row) = testee.fetchRow();
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(row == nullptr);
      }
    }

    WHEN("the producer waits") {
      fakeit::When(Method(blockFetcherMock,fetchBlock))
        .Do([]{ return std::make_pair(ExecutionState::WAITING, nullptr); })
        .Do([]{ return std::make_pair(ExecutionState::DONE, nullptr); });

      SingleRowFetcher testee(blockFetcherMock.get());

      THEN("the fetcher should first return WAIT with nullptr") {
        std::tie(state, row) = testee.fetchRow();
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(row == nullptr);

        AND_THEN("the fetcher should return DONE with nullptr") {
          std::tie(state, row) = testee.fetchRow();
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(row == nullptr);

        }
      }

    }
  }
}

} // aql
} // tests
} // arangodb
