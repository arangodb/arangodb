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
      SingleRowFetcherHelper<FilterExecutor> fetcher(input.steal(), false);
      FilterExecutor testee(fetcher, infos);

      THEN("the executor should return DONE with nullptr") {
        AqlItemRow result(&block, 0, 1);
        state = testee.produceRow(result);
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(!result.hasValue());
      }
    }

    WHEN("the producer waits") {
      SingleRowFetcherHelper<FilterExecutor> fetcher(input.steal(), true);
      FilterExecutor testee(fetcher, infos);

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
}

} // aql
} // tests
} // arangodb
