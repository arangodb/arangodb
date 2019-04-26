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

#include "AqlItemBlockHelper.h"
#include "DependencyProxyMock.h"
#include "RowFetcherHelper.h"
#include "catch.hpp"

#include "Aql/AqlItemBlock.h"
#include "Aql/DependencyProxy.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/MultiDependencySingleRowFetcher.h"
#include "Aql/ResourceUsage.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

SCENARIO("MultiDependencySingleRowFetcher", "[AQL][EXECUTOR][FETCHER]") {
  ResourceMonitor monitor;
  AqlItemBlockManager itemBlockManager{&monitor};
  ExecutionState state;

  GIVEN("there are no blocks upstream, single dependency") {
    VPackBuilder input;
    MultiDependencyProxyMock<false> dependencyProxyMock{monitor, 0, 1};
    InputAqlItemRow row{CreateInvalidInputRowHint{}};
    WHEN("the producer does not wait") {
      dependencyProxyMock.getDependencyMock(0).shouldReturn(ExecutionState::DONE, nullptr);

      {
        MultiDependencySingleRowFetcher testee(dependencyProxyMock);

        THEN("the fetcher should return DONE with nullptr") {
          std::tie(state, row) = testee.fetchRowForDependency(0);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!row);
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(dependencyProxyMock.allBlocksFetched());
      REQUIRE(dependencyProxyMock.numFetchBlockCalls() == 1);
    }

    WHEN("the producer waits") {
      dependencyProxyMock.getDependencyMock(0)
          .shouldReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::DONE, nullptr);

      {
        MultiDependencySingleRowFetcher testee(dependencyProxyMock);

        THEN("the fetcher should first return WAIT with nullptr") {
          std::tie(state, row) = testee.fetchRowForDependency(0);
          REQUIRE(state == ExecutionState::WAITING);
          REQUIRE(!row);

          AND_THEN("the fetcher should return DONE with nullptr") {
            std::tie(state, row) = testee.fetchRowForDependency(0);
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(!row);
          }
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(dependencyProxyMock.allBlocksFetched());
      REQUIRE(dependencyProxyMock.numFetchBlockCalls() == 2);
    }
  }

  GIVEN("A single upstream block with a single row, single dependency") {
    VPackBuilder input;
    MultiDependencyProxyMock<false> dependencyProxyMock{monitor, 1, 1};
    InputAqlItemRow row{CreateInvalidInputRowHint{}};

    SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});

    WHEN("the producer returns DONE immediately") {
      dependencyProxyMock.getDependencyMock(0).shouldReturn(ExecutionState::DONE,
                                                            std::move(block));

      {
        MultiDependencySingleRowFetcher testee(dependencyProxyMock);

        THEN("the fetcher should return the row with DONE") {
          std::tie(state, row) = testee.fetchRowForDependency(0);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(row);
          REQUIRE(row.getNrRegisters() == 1);
          REQUIRE(row.getValue(0).slice().getInt() == 42);
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(dependencyProxyMock.allBlocksFetched());
      REQUIRE(dependencyProxyMock.numFetchBlockCalls() == 1);
    }

    WHEN("the producer returns HASMORE, then DONE with a nullptr") {
      dependencyProxyMock.getDependencyMock(0)
          .shouldReturn(ExecutionState::HASMORE, std::move(block))
          .andThenReturn(ExecutionState::DONE, nullptr);

      {
        MultiDependencySingleRowFetcher testee(dependencyProxyMock);

        THEN("the fetcher should return the row with HASMORE") {
          std::tie(state, row) = testee.fetchRowForDependency(0);
          REQUIRE(state == ExecutionState::HASMORE);
          REQUIRE(row);
          REQUIRE(row.getNrRegisters() == 1);
          REQUIRE(row.getValue(0).slice().getInt() == 42);

          AND_THEN("the fetcher shall return DONE") {
            std::tie(state, row) = testee.fetchRowForDependency(0);
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(!row);
          }
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(dependencyProxyMock.allBlocksFetched());
      REQUIRE(dependencyProxyMock.numFetchBlockCalls() == 2);
    }

    WHEN("the producer WAITs, then returns DONE") {
      dependencyProxyMock.getDependencyMock(0)
          .shouldReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::DONE, std::move(block));

      {
        MultiDependencySingleRowFetcher testee(dependencyProxyMock);

        THEN("the fetcher should first return WAIT with nullptr") {
          std::tie(state, row) = testee.fetchRowForDependency(0);
          REQUIRE(state == ExecutionState::WAITING);
          REQUIRE(!row);

          AND_THEN("the fetcher should return the row with DONE") {
            std::tie(state, row) = testee.fetchRowForDependency(0);
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(row);
            REQUIRE(row.getNrRegisters() == 1);
            REQUIRE(row.getValue(0).slice().getInt() == 42);
          }
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(dependencyProxyMock.allBlocksFetched());
      REQUIRE(dependencyProxyMock.numFetchBlockCalls() == 2);
    }

    WHEN("the producer WAITs, returns HASMORE, then DONE") {
      dependencyProxyMock.getDependencyMock(0)
          .shouldReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::HASMORE, std::move(block))
          .andThenReturn(ExecutionState::DONE, nullptr);

      {
        MultiDependencySingleRowFetcher testee(dependencyProxyMock);

        THEN("the fetcher should first return WAIT with nullptr") {
          std::tie(state, row) = testee.fetchRowForDependency(0);
          REQUIRE(state == ExecutionState::WAITING);
          REQUIRE(!row);

          AND_THEN("the fetcher should return the row with HASMORE") {
            std::tie(state, row) = testee.fetchRowForDependency(0);
            REQUIRE(state == ExecutionState::HASMORE);
            REQUIRE(row);
            REQUIRE(row.getNrRegisters() == 1);
            REQUIRE(row.getValue(0).slice().getInt() == 42);

            AND_THEN("the fetcher shall return DONE") {
              std::tie(state, row) = testee.fetchRowForDependency(0);
              REQUIRE(state == ExecutionState::DONE);
              REQUIRE(!row);
            }
          }
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(dependencyProxyMock.allBlocksFetched());
      REQUIRE(dependencyProxyMock.numFetchBlockCalls() == 3);
    }
  }

  // TODO the following tests should be simplified, a simple output
  // specification should be compared with the actual output.

  GIVEN("there are multiple blocks upstream, single dependency") {
    MultiDependencyProxyMock<false> dependencyProxyMock{monitor, 1, 1};
    InputAqlItemRow row{CreateInvalidInputRowHint{}};

    // three 1-column matrices with 3, 2 and 1 rows, respectively
    SharedAqlItemBlockPtr block1 =
                              buildBlock<1>(itemBlockManager, {{{1}}, {{2}}, {{3}}}),
                          block2 = buildBlock<1>(itemBlockManager, {{{4}}, {{5}}}),
                          block3 = buildBlock<1>(itemBlockManager, {{{6}}});

    WHEN("the producer does not wait") {
      dependencyProxyMock.getDependencyMock(0)
          .shouldReturn(ExecutionState::HASMORE, std::move(block1))
          .andThenReturn(ExecutionState::HASMORE, std::move(block2))
          .andThenReturn(ExecutionState::DONE, std::move(block3));

      {
        MultiDependencySingleRowFetcher testee(dependencyProxyMock);

        THEN("the fetcher should return all rows and DONE with the last") {
          int64_t rowIdxAndValue;
          for (rowIdxAndValue = 1; rowIdxAndValue <= 5; rowIdxAndValue++) {
            std::tie(state, row) = testee.fetchRowForDependency(0);
            REQUIRE(state == ExecutionState::HASMORE);
            REQUIRE(row);
            REQUIRE(row.getNrRegisters() == 1);
            REQUIRE(row.getValue(0).slice().getInt() == rowIdxAndValue);
          }
          rowIdxAndValue = 6;
          std::tie(state, row) = testee.fetchRowForDependency(0);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(row);
          REQUIRE(row.getNrRegisters() == 1);
          REQUIRE(row.getValue(0).slice().getInt() == rowIdxAndValue);
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(dependencyProxyMock.allBlocksFetched());
      REQUIRE(dependencyProxyMock.numFetchBlockCalls() == 3);
    }

    WHEN("the producer waits") {
      dependencyProxyMock.getDependencyMock(0)
          .shouldReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::HASMORE, std::move(block1))
          .andThenReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::HASMORE, std::move(block2))
          .andThenReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::DONE, std::move(block3));

      {
        MultiDependencySingleRowFetcher testee(dependencyProxyMock);

        THEN("the fetcher should return all rows and DONE with the last") {
          int64_t rowIdxAndValue;
          for (rowIdxAndValue = 1; rowIdxAndValue <= 5; rowIdxAndValue++) {
            if (rowIdxAndValue == 1 || rowIdxAndValue == 4) {
              // wait at the beginning of the 1st and 2nd block
              std::tie(state, row) = testee.fetchRowForDependency(0);
              REQUIRE(state == ExecutionState::WAITING);
              REQUIRE(!row);
            }
            std::tie(state, row) = testee.fetchRowForDependency(0);
            REQUIRE(state == ExecutionState::HASMORE);
            REQUIRE(row);
            REQUIRE(row.getNrRegisters() == 1);
            REQUIRE(row.getValue(0).slice().getInt() == rowIdxAndValue);
          }
          rowIdxAndValue = 6;
          // wait at the beginning of the 3rd block
          std::tie(state, row) = testee.fetchRowForDependency(0);
          REQUIRE(state == ExecutionState::WAITING);
          REQUIRE(!row);
          // last row and DONE
          std::tie(state, row) = testee.fetchRowForDependency(0);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(row);
          REQUIRE(row.getNrRegisters() == 1);
          REQUIRE(row.getValue(0).slice().getInt() == rowIdxAndValue);
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(dependencyProxyMock.allBlocksFetched());
      REQUIRE(dependencyProxyMock.numFetchBlockCalls() == 6);
    }

    WHEN("the producer waits and does not return DONE asap") {
      dependencyProxyMock.getDependencyMock(0)
          .shouldReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::HASMORE, std::move(block1))
          .andThenReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::HASMORE, std::move(block2))
          .andThenReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::HASMORE, std::move(block3))
          .andThenReturn(ExecutionState::DONE, nullptr);

      {
        MultiDependencySingleRowFetcher testee(dependencyProxyMock);

        THEN("the fetcher should return all rows and DONE with the last") {
          for (int64_t rowIdxAndValue = 1; rowIdxAndValue <= 6; rowIdxAndValue++) {
            if (rowIdxAndValue == 1 || rowIdxAndValue == 4 || rowIdxAndValue == 6) {
              // wait at the beginning of the 1st, 2nd and 3rd block
              std::tie(state, row) = testee.fetchRowForDependency(0);
              REQUIRE(state == ExecutionState::WAITING);
              REQUIRE(!row);
            }
            std::tie(state, row) = testee.fetchRowForDependency(0);
            REQUIRE(state == ExecutionState::HASMORE);
            REQUIRE(row);
            REQUIRE(row.getNrRegisters() == 1);
            REQUIRE(row.getValue(0).slice().getInt() == rowIdxAndValue);
          }
          std::tie(state, row) = testee.fetchRowForDependency(0);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!row);
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(dependencyProxyMock.allBlocksFetched());
      REQUIRE(dependencyProxyMock.numFetchBlockCalls() == 7);
    }
  }

  /*********************
   *  Multi Dependencies
   *********************/

  GIVEN("there are no blocks upstream, multiple dependencies") {
    VPackBuilder input;
    size_t numDeps = 3;
    MultiDependencyProxyMock<false> dependencyProxyMock{monitor, 0, numDeps};
    InputAqlItemRow row{CreateInvalidInputRowHint{}};

    WHEN("the producers do not wait") {
      for (size_t i = 0; i < numDeps; ++i) {
        dependencyProxyMock.getDependencyMock(i).shouldReturn(ExecutionState::DONE, nullptr);
      }

      {
        MultiDependencySingleRowFetcher testee(dependencyProxyMock);

        THEN("the fetcher should return DONE with nullptr") {
          for (size_t i = 0; i < numDeps; ++i) {
            std::tie(state, row) = testee.fetchRowForDependency(i);
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(!row);
          }
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(dependencyProxyMock.allBlocksFetched());
      REQUIRE(dependencyProxyMock.numFetchBlockCalls() == numDeps);
    }

    WHEN("the producer waits") {
      for (size_t i = 0; i < numDeps; ++i) {
        dependencyProxyMock.getDependencyMock(i)
            .shouldReturn(ExecutionState::WAITING, nullptr)
            .andThenReturn(ExecutionState::DONE, nullptr);
      }

      {
        MultiDependencySingleRowFetcher testee(dependencyProxyMock);

        THEN("the fetcher should first return WAIT with nullptr") {
          for (size_t i = 0; i < numDeps; ++i) {
            std::tie(state, row) = testee.fetchRowForDependency(i);
            REQUIRE(state == ExecutionState::WAITING);
            REQUIRE(!row);
          }

          AND_THEN("the fetcher should return DONE with nullptr") {
            for (size_t i = 0; i < numDeps; ++i) {
              std::tie(state, row) = testee.fetchRowForDependency(i);
              REQUIRE(state == ExecutionState::DONE);
              REQUIRE(!row);
            }
          }
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(dependencyProxyMock.allBlocksFetched());
      REQUIRE(dependencyProxyMock.numFetchBlockCalls() == 2 * numDeps);
    }
  }

  GIVEN("A single upstream block with a single row, multi dependency") {
    VPackBuilder input;
    size_t numDeps = 3;
    MultiDependencyProxyMock<false> dependencyProxyMock{monitor, 1, numDeps};
    InputAqlItemRow row{CreateInvalidInputRowHint{}};

    SharedAqlItemBlockPtr blockDep1 = buildBlock<1>(itemBlockManager, {{42}});
    SharedAqlItemBlockPtr blockDep2 = buildBlock<1>(itemBlockManager, {{23}});
    SharedAqlItemBlockPtr blockDep3 = buildBlock<1>(itemBlockManager, {{1337}});

    WHEN("the producer returns DONE immediately") {
      dependencyProxyMock.getDependencyMock(0).shouldReturn(ExecutionState::DONE,
                                                            std::move(blockDep1));
      dependencyProxyMock.getDependencyMock(1).shouldReturn(ExecutionState::DONE,
                                                            std::move(blockDep2));
      dependencyProxyMock.getDependencyMock(2).shouldReturn(ExecutionState::DONE,
                                                            std::move(blockDep3));

      {
        MultiDependencySingleRowFetcher testee(dependencyProxyMock);

        THEN("the fetcher should return the row with DONE") {
          for (size_t i = 0; i < numDeps; ++i) {
            std::tie(state, row) = testee.fetchRowForDependency(i);
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(row);
            REQUIRE(row.getNrRegisters() == 1);
            if (i == 0) {
              REQUIRE(row.getValue(0).slice().getInt() == 42);
            } else if (i == 1) {
              REQUIRE(row.getValue(0).slice().getInt() == 23);
            } else {
              REQUIRE(row.getValue(0).slice().getInt() == 1337);
            }
          }
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(dependencyProxyMock.allBlocksFetched());
      REQUIRE(dependencyProxyMock.numFetchBlockCalls() == numDeps);
    }

    WHEN("the producer returns HASMORE, then DONE with a nullptr") {
      dependencyProxyMock.getDependencyMock(0)
          .shouldReturn(ExecutionState::HASMORE, std::move(blockDep1))
          .andThenReturn(ExecutionState::DONE, nullptr);
      dependencyProxyMock.getDependencyMock(1)
          .shouldReturn(ExecutionState::HASMORE, std::move(blockDep2))
          .andThenReturn(ExecutionState::DONE, nullptr);
      dependencyProxyMock.getDependencyMock(2)
          .shouldReturn(ExecutionState::HASMORE, std::move(blockDep3))
          .andThenReturn(ExecutionState::DONE, nullptr);

      {
        MultiDependencySingleRowFetcher testee(dependencyProxyMock);

        THEN("the fetcher should return the row with HASMORE") {
          for (size_t i = 0; i < numDeps; ++i) {
            std::tie(state, row) = testee.fetchRowForDependency(i);
            REQUIRE(state == ExecutionState::HASMORE);
            REQUIRE(row);
            REQUIRE(row.getNrRegisters() == 1);
            if (i == 0) {
              REQUIRE(row.getValue(0).slice().getInt() == 42);
            } else if (i == 1) {
              REQUIRE(row.getValue(0).slice().getInt() == 23);
            } else {
              REQUIRE(row.getValue(0).slice().getInt() == 1337);
            }
          }

          AND_THEN("the fetcher shall return DONE") {
            for (size_t i = 0; i < numDeps; ++i) {
              std::tie(state, row) = testee.fetchRowForDependency(i);
              REQUIRE(state == ExecutionState::DONE);
              REQUIRE(!row);
            }
          }
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(dependencyProxyMock.allBlocksFetched());
      REQUIRE(dependencyProxyMock.numFetchBlockCalls() == 2 * numDeps);
    }

    WHEN("the producer WAITs, then returns DONE") {
      dependencyProxyMock.getDependencyMock(0)
          .shouldReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::DONE, std::move(blockDep1));
      dependencyProxyMock.getDependencyMock(1)
          .shouldReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::DONE, std::move(blockDep2));
      dependencyProxyMock.getDependencyMock(2)
          .shouldReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::DONE, std::move(blockDep3));

      {
        MultiDependencySingleRowFetcher testee(dependencyProxyMock);

        THEN("the fetcher should first return WAIT with nullptr") {
          for (size_t i = 0; i < numDeps; ++i) {
            std::tie(state, row) = testee.fetchRowForDependency(i);
            REQUIRE(state == ExecutionState::WAITING);
            REQUIRE(!row);
          }

          AND_THEN("the fetcher should return the row with DONE") {
            for (size_t i = 0; i < numDeps; ++i) {
              std::tie(state, row) = testee.fetchRowForDependency(i);
              REQUIRE(state == ExecutionState::DONE);
              REQUIRE(row);
              REQUIRE(row.getNrRegisters() == 1);
              if (i == 0) {
                REQUIRE(row.getValue(0).slice().getInt() == 42);
              } else if (i == 1) {
                REQUIRE(row.getValue(0).slice().getInt() == 23);
              } else {
                REQUIRE(row.getValue(0).slice().getInt() == 1337);
              }
            }
          }
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(dependencyProxyMock.allBlocksFetched());
      REQUIRE(dependencyProxyMock.numFetchBlockCalls() == 2 * numDeps);
    }

    WHEN("the producer WAITs, returns HASMORE, then DONE") {
      dependencyProxyMock.getDependencyMock(0)
          .shouldReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::HASMORE, std::move(blockDep1))
          .andThenReturn(ExecutionState::DONE, nullptr);
      dependencyProxyMock.getDependencyMock(1)
          .shouldReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::HASMORE, std::move(blockDep2))
          .andThenReturn(ExecutionState::DONE, nullptr);
      dependencyProxyMock.getDependencyMock(2)
          .shouldReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::HASMORE, std::move(blockDep3))
          .andThenReturn(ExecutionState::DONE, nullptr);

      {
        MultiDependencySingleRowFetcher testee(dependencyProxyMock);

        THEN("the fetcher should first return WAIT with nullptr") {
          for (size_t i = 0; i < numDeps; ++i) {
            std::tie(state, row) = testee.fetchRowForDependency(i);
            REQUIRE(state == ExecutionState::WAITING);
            REQUIRE(!row);
          }

          AND_THEN("the fetcher should return the row with HASMORE") {
            for (size_t i = 0; i < numDeps; ++i) {
              std::tie(state, row) = testee.fetchRowForDependency(i);
              REQUIRE(state == ExecutionState::HASMORE);
              REQUIRE(row);
              REQUIRE(row.getNrRegisters() == 1);
              if (i == 0) {
                REQUIRE(row.getValue(0).slice().getInt() == 42);
              } else if (i == 1) {
                REQUIRE(row.getValue(0).slice().getInt() == 23);
              } else {
                REQUIRE(row.getValue(0).slice().getInt() == 1337);
              }
            }

            AND_THEN("the fetcher shall return DONE") {
              for (size_t i = 0; i < numDeps; ++i) {
                std::tie(state, row) = testee.fetchRowForDependency(i);
                REQUIRE(state == ExecutionState::DONE);
                REQUIRE(!row);
              }
            }
          }
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(dependencyProxyMock.allBlocksFetched());
      REQUIRE(dependencyProxyMock.numFetchBlockCalls() == 3 * numDeps);
    }
  }

  // TODO the following tests should be simplified, a simple output
  // specification should be compared with the actual output.

  GIVEN("there are multiple blocks upstream, multiple dependencies") {
    size_t numDeps = 3;
    MultiDependencyProxyMock<false> dependencyProxyMock{monitor, 1, numDeps};
    InputAqlItemRow row{CreateInvalidInputRowHint{}};

    // three 1-column matrices with 3, 2 and 1 rows, respectively
    SharedAqlItemBlockPtr block1Dep1 =
                              buildBlock<1>(itemBlockManager, {{{1}}, {{2}}, {{3}}}),
                          block2Dep1 = buildBlock<1>(itemBlockManager, {{{4}}, {{5}}}),
                          block3Dep1 = buildBlock<1>(itemBlockManager, {{{6}}});

    // two 1-column matrices with 1 and 2 rows, respectively
    SharedAqlItemBlockPtr block1Dep2 = buildBlock<1>(itemBlockManager, {{{7}}}),
                          block2Dep2 = buildBlock<1>(itemBlockManager, {{{8}}, {{9}}});

    // single 1-column matrices with 2 rows
    SharedAqlItemBlockPtr block1Dep3 =
        buildBlock<1>(itemBlockManager, {{{10}}, {{11}}});

    WHEN("the producer does not wait") {
      dependencyProxyMock.getDependencyMock(0)
          .shouldReturn(ExecutionState::HASMORE, std::move(block1Dep1))
          .andThenReturn(ExecutionState::HASMORE, std::move(block2Dep1))
          .andThenReturn(ExecutionState::DONE, std::move(block3Dep1));
      dependencyProxyMock.getDependencyMock(1)
          .shouldReturn(ExecutionState::HASMORE, std::move(block1Dep2))
          .andThenReturn(ExecutionState::DONE, std::move(block2Dep2));
      dependencyProxyMock.getDependencyMock(2).shouldReturn(ExecutionState::DONE,
                                                            std::move(block1Dep3));

      {
        MultiDependencySingleRowFetcher testee(dependencyProxyMock);

        THEN(
            "the fetcher should return all rows and DONE with the last for "
            "dependency 0") {
          int64_t rowIdxAndValue;
          for (rowIdxAndValue = 1; rowIdxAndValue <= 5; rowIdxAndValue++) {
            std::tie(state, row) = testee.fetchRowForDependency(0);
            REQUIRE(state == ExecutionState::HASMORE);
            REQUIRE(row);
            REQUIRE(row.getNrRegisters() == 1);
            REQUIRE(row.getValue(0).slice().getInt() == rowIdxAndValue);
          }
          rowIdxAndValue = 6;
          std::tie(state, row) = testee.fetchRowForDependency(0);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(row);
          REQUIRE(row.getNrRegisters() == 1);
          REQUIRE(row.getValue(0).slice().getInt() == rowIdxAndValue);

          AND_THEN("for dependency 1") {
            for (rowIdxAndValue = 7; rowIdxAndValue <= 8; rowIdxAndValue++) {
              std::tie(state, row) = testee.fetchRowForDependency(1);
              REQUIRE(state == ExecutionState::HASMORE);
              REQUIRE(row);
              REQUIRE(row.getNrRegisters() == 1);
              REQUIRE(row.getValue(0).slice().getInt() == rowIdxAndValue);
            }
            rowIdxAndValue = 9;
            std::tie(state, row) = testee.fetchRowForDependency(1);
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(row);
            REQUIRE(row.getNrRegisters() == 1);
            REQUIRE(row.getValue(0).slice().getInt() == rowIdxAndValue);

            AND_THEN("for dependency 2") {
              for (rowIdxAndValue = 10; rowIdxAndValue <= 10; rowIdxAndValue++) {
                std::tie(state, row) = testee.fetchRowForDependency(2);
                REQUIRE(state == ExecutionState::HASMORE);
                REQUIRE(row);
                REQUIRE(row.getNrRegisters() == 1);
                REQUIRE(row.getValue(0).slice().getInt() == rowIdxAndValue);
              }
              rowIdxAndValue = 11;
              std::tie(state, row) = testee.fetchRowForDependency(2);
              REQUIRE(state == ExecutionState::DONE);
              REQUIRE(row);
              REQUIRE(row.getNrRegisters() == 1);
              REQUIRE(row.getValue(0).slice().getInt() == rowIdxAndValue);
            }
          }
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(dependencyProxyMock.allBlocksFetched());
      REQUIRE(dependencyProxyMock.numFetchBlockCalls() == 3 + 2 + 1);
    }

    WHEN("the producer waits") {
      dependencyProxyMock.getDependencyMock(0)
          .shouldReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::HASMORE, std::move(block1Dep1))
          .andThenReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::HASMORE, std::move(block2Dep1))
          .andThenReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::DONE, std::move(block3Dep1));
      dependencyProxyMock.getDependencyMock(1)
          .shouldReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::HASMORE, std::move(block1Dep2))
          .andThenReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::DONE, std::move(block2Dep2));
      dependencyProxyMock.getDependencyMock(2)
          .shouldReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::DONE, std::move(block1Dep3));

      {
        MultiDependencySingleRowFetcher testee(dependencyProxyMock);

        THEN("the fetcher should return all rows and DONE with the last") {
          int64_t rowIdxAndValue;
          for (rowIdxAndValue = 1; rowIdxAndValue <= 5; rowIdxAndValue++) {
            if (rowIdxAndValue == 1 || rowIdxAndValue == 4) {
              // wait at the beginning of the 1st and 2nd block
              std::tie(state, row) = testee.fetchRowForDependency(0);
              REQUIRE(state == ExecutionState::WAITING);
              REQUIRE(!row);
            }
            std::tie(state, row) = testee.fetchRowForDependency(0);
            REQUIRE(state == ExecutionState::HASMORE);
            REQUIRE(row);
            REQUIRE(row.getNrRegisters() == 1);
            REQUIRE(row.getValue(0).slice().getInt() == rowIdxAndValue);
          }
          rowIdxAndValue = 6;
          // wait at the beginning of the 3rd block
          std::tie(state, row) = testee.fetchRowForDependency(0);
          REQUIRE(state == ExecutionState::WAITING);
          REQUIRE(!row);
          // last row and DONE
          std::tie(state, row) = testee.fetchRowForDependency(0);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(row);
          REQUIRE(row.getNrRegisters() == 1);
          REQUIRE(row.getValue(0).slice().getInt() == rowIdxAndValue);

          AND_THEN("for dependency 1") {
            for (rowIdxAndValue = 7; rowIdxAndValue <= 8; rowIdxAndValue++) {
              if (rowIdxAndValue == 7 || rowIdxAndValue == 8) {
                // wait at the beginning of the 1st and 2nd block
                std::tie(state, row) = testee.fetchRowForDependency(1);
                REQUIRE(state == ExecutionState::WAITING);
                REQUIRE(!row);
              }
              std::tie(state, row) = testee.fetchRowForDependency(1);
              REQUIRE(state == ExecutionState::HASMORE);
              REQUIRE(row);
              REQUIRE(row.getNrRegisters() == 1);
              REQUIRE(row.getValue(0).slice().getInt() == rowIdxAndValue);
            }
            rowIdxAndValue = 9;
            std::tie(state, row) = testee.fetchRowForDependency(1);
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(row);
            REQUIRE(row.getNrRegisters() == 1);
            REQUIRE(row.getValue(0).slice().getInt() == rowIdxAndValue);

            AND_THEN("for dependency 2") {
              for (rowIdxAndValue = 10; rowIdxAndValue <= 10; rowIdxAndValue++) {
                if (rowIdxAndValue == 10) {
                  // wait at the beginning of the 1st and 2nd block
                  std::tie(state, row) = testee.fetchRowForDependency(2);
                  REQUIRE(state == ExecutionState::WAITING);
                  REQUIRE(!row);
                }
                std::tie(state, row) = testee.fetchRowForDependency(2);
                REQUIRE(state == ExecutionState::HASMORE);
                REQUIRE(row);
                REQUIRE(row.getNrRegisters() == 1);
                REQUIRE(row.getValue(0).slice().getInt() == rowIdxAndValue);
              }
              rowIdxAndValue = 11;
              std::tie(state, row) = testee.fetchRowForDependency(2);
              REQUIRE(state == ExecutionState::DONE);
              REQUIRE(row);
              REQUIRE(row.getNrRegisters() == 1);
              REQUIRE(row.getValue(0).slice().getInt() == rowIdxAndValue);
            }
          }
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(dependencyProxyMock.allBlocksFetched());
      REQUIRE(dependencyProxyMock.numFetchBlockCalls() == 12);
    }

    WHEN("the producer waits and does not return DONE asap") {
      dependencyProxyMock.getDependencyMock(0)
          .shouldReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::HASMORE, std::move(block1Dep1))
          .andThenReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::HASMORE, std::move(block2Dep1))
          .andThenReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::HASMORE, std::move(block3Dep1))
          .andThenReturn(ExecutionState::DONE, nullptr);
      dependencyProxyMock.getDependencyMock(1)
          .shouldReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::HASMORE, std::move(block1Dep2))
          .andThenReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::HASMORE, std::move(block2Dep2))
          .andThenReturn(ExecutionState::DONE, nullptr);
      dependencyProxyMock.getDependencyMock(2)
          .shouldReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::HASMORE, std::move(block1Dep3))
          .andThenReturn(ExecutionState::DONE, nullptr);

      {
        MultiDependencySingleRowFetcher testee(dependencyProxyMock);

        THEN("the fetcher should return all rows and DONE with the last") {
          int64_t rowIdxAndValue;
          for (rowIdxAndValue = 1; rowIdxAndValue <= 5; rowIdxAndValue++) {
            if (rowIdxAndValue == 1 || rowIdxAndValue == 4) {
              // wait at the beginning of the 1st and 2nd block
              std::tie(state, row) = testee.fetchRowForDependency(0);
              REQUIRE(state == ExecutionState::WAITING);
              REQUIRE(!row);
            }
            std::tie(state, row) = testee.fetchRowForDependency(0);
            REQUIRE(state == ExecutionState::HASMORE);
            REQUIRE(row);
            REQUIRE(row.getNrRegisters() == 1);
            REQUIRE(row.getValue(0).slice().getInt() == rowIdxAndValue);
          }
          rowIdxAndValue = 6;
          // wait at the beginning of the 3rd block
          std::tie(state, row) = testee.fetchRowForDependency(0);
          REQUIRE(state == ExecutionState::WAITING);
          REQUIRE(!row);
          // last row and DONE
          std::tie(state, row) = testee.fetchRowForDependency(0);
          REQUIRE(state == ExecutionState::HASMORE);
          REQUIRE(row);
          REQUIRE(row.getNrRegisters() == 1);
          REQUIRE(row.getValue(0).slice().getInt() == rowIdxAndValue);
          std::tie(state, row) = testee.fetchRowForDependency(0);
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(!row);

          AND_THEN("for dependency 1") {
            for (rowIdxAndValue = 7; rowIdxAndValue <= 8; rowIdxAndValue++) {
              if (rowIdxAndValue == 7 || rowIdxAndValue == 8) {
                // wait at the beginning of the 1st and 2nd block
                std::tie(state, row) = testee.fetchRowForDependency(1);
                REQUIRE(state == ExecutionState::WAITING);
                REQUIRE(!row);
              }
              std::tie(state, row) = testee.fetchRowForDependency(1);
              REQUIRE(state == ExecutionState::HASMORE);
              REQUIRE(row);
              REQUIRE(row.getNrRegisters() == 1);
              REQUIRE(row.getValue(0).slice().getInt() == rowIdxAndValue);
            }
            rowIdxAndValue = 9;
            std::tie(state, row) = testee.fetchRowForDependency(1);
            REQUIRE(state == ExecutionState::HASMORE);
            REQUIRE(row);
            REQUIRE(row.getNrRegisters() == 1);
            REQUIRE(row.getValue(0).slice().getInt() == rowIdxAndValue);
            std::tie(state, row) = testee.fetchRowForDependency(1);
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(!row);

            AND_THEN("for dependency 2") {
              for (rowIdxAndValue = 10; rowIdxAndValue <= 10; rowIdxAndValue++) {
                if (rowIdxAndValue == 10) {
                  // wait at the beginning of the 1st and 2nd block
                  std::tie(state, row) = testee.fetchRowForDependency(2);
                  REQUIRE(state == ExecutionState::WAITING);
                  REQUIRE(!row);
                }
                std::tie(state, row) = testee.fetchRowForDependency(2);
                REQUIRE(state == ExecutionState::HASMORE);
                REQUIRE(row);
                REQUIRE(row.getNrRegisters() == 1);
                REQUIRE(row.getValue(0).slice().getInt() == rowIdxAndValue);
              }
              rowIdxAndValue = 11;
              std::tie(state, row) = testee.fetchRowForDependency(2);
              REQUIRE(state == ExecutionState::HASMORE);
              REQUIRE(row);
              REQUIRE(row.getNrRegisters() == 1);
              REQUIRE(row.getValue(0).slice().getInt() == rowIdxAndValue);
              std::tie(state, row) = testee.fetchRowForDependency(2);
              REQUIRE(state == ExecutionState::DONE);
              REQUIRE(!row);
            }
          }
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(dependencyProxyMock.allBlocksFetched());
      REQUIRE(dependencyProxyMock.numFetchBlockCalls() == 15);
    }
  }
}
}  // namespace aql
}  // namespace tests
}  // namespace arangodb
