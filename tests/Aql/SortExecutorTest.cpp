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

#include "Aql/AqlItemRow.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/SortExecutor.h"
#include "Aql/BlockFetcherInterfaces.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

SCENARIO("SortExecutor", "[AQL][EXECUTOR]") {
  ExecutionState state;
  std::unique_ptr<AqlItemRow> result;
  ExecutorInfos infos(0, 0);

  GIVEN("there are no rows upstream") {
    VPackBuilder input;

    WHEN("the producer does not wait") {
      AllRowsFetcherHelper fetcher(input.steal(), false);
      SortExecutor testee(fetcher, infos);

      THEN("the executor should return DONE with nullptr") {
        std::tie(state, result) = testee.produceRow();
        REQUIRE(state == ExecutionState::DONE);
        REQUIRE(result == nullptr);
      }
    }

    WHEN("the producer waits") {
      AllRowsFetcherHelper fetcher(input.steal(), true);
      SortExecutor testee(fetcher, infos);

      THEN("the executor should first return WAIT with nullptr") {
        std::tie(state, result) = testee.produceRow();
        REQUIRE(state == ExecutionState::WAITING);
        REQUIRE(result == nullptr);

        AND_THEN("the executor should return DONE with nullptr") {
          std::tie(state, result) = testee.produceRow();
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(result == nullptr);
        }
      }

    }
  }
}



} // aql
} // tests
} // arangodb
