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
#include "BlockFetcherMock.h"
#include "catch.hpp"

#include "Aql/AllRowsFetcher.h"
#include "Aql/InputAqlItemRow.h"

using namespace arangodb;
using namespace arangodb::aql;

namespace arangodb {
namespace tests {
namespace aql {

SCENARIO("AllRowsFetcher", "[AQL][EXECUTOR][FETCHER]") {
  ExecutionState state;
  AqlItemMatrix const* matrix = nullptr;

  GIVEN("there are no blocks upstream") {
    VPackBuilder input;
    ResourceMonitor monitor;
    BlockFetcherMock<false> blockFetcherMock{monitor, 0};

    WHEN("the producer does not wait") {
      blockFetcherMock.shouldReturn(ExecutionState::DONE, nullptr);

      {
        AllRowsFetcher testee(blockFetcherMock);

        THEN("the fetcher should return DONE with an empty matrix") {
          std::tie(state, matrix) = testee.fetchAllRows();
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(matrix != nullptr);
          REQUIRE(matrix->empty());
          REQUIRE(matrix->size() == 0);

          AND_THEN("the same matrix should be returned") {
            AqlItemMatrix const* matrix2 = nullptr;
            std::tie(state, matrix2) = testee.fetchAllRows();
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(matrix2 == matrix);
          }
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(blockFetcherMock.allBlocksFetched());
      REQUIRE(blockFetcherMock.numFetchBlockCalls() == 1);
    }

    WHEN("the producer waits") {
      blockFetcherMock.shouldReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::DONE, nullptr);

      {
        AllRowsFetcher testee(blockFetcherMock);

        THEN("the fetcher should first return WAIT with nullptr") {
          std::tie(state, matrix) = testee.fetchAllRows();
          REQUIRE(state == ExecutionState::WAITING);
          REQUIRE(matrix == nullptr);

          AND_THEN("the fetcher should return DONE with an empty matrix") {
            std::tie(state, matrix) = testee.fetchAllRows();
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(matrix != nullptr);
            REQUIRE(matrix->empty());
            REQUIRE(matrix->size() == 0);

            AND_THEN("the same matrix should be returned") {
              AqlItemMatrix const* matrix2 = nullptr;
              std::tie(state, matrix2) = testee.fetchAllRows();
              REQUIRE(state == ExecutionState::DONE);
              REQUIRE(matrix2 == matrix);
            }
          }
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(blockFetcherMock.allBlocksFetched());
      REQUIRE(blockFetcherMock.numFetchBlockCalls() == 2);
    }
  }

  GIVEN("A single upstream block with a single row") {
    VPackBuilder input;
    ResourceMonitor monitor;
    AqlItemBlockManager itemBlockManager{&monitor};
    BlockFetcherMock<false> blockFetcherMock{monitor, 1};

    SharedAqlItemBlockPtr block = buildBlock<1>(itemBlockManager, {{42}});

    WHEN("the producer returns DONE immediately") {
      blockFetcherMock.shouldReturn(ExecutionState::DONE, std::move(block));

      {
        AllRowsFetcher testee(blockFetcherMock);

        THEN("the fetcher should return the matrix with DONE") {
          std::tie(state, matrix) = testee.fetchAllRows();
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(matrix != nullptr);
          REQUIRE(matrix->getNrRegisters() == 1);
          REQUIRE(!matrix->empty());
          REQUIRE(matrix->size() == 1);
          REQUIRE(matrix->getRow(0).getValue(0).slice().getInt() == 42);

          AND_THEN("the same matrix should be returned") {
            AqlItemMatrix const* matrix2 = nullptr;
            std::tie(state, matrix2) = testee.fetchAllRows();
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(matrix2 == matrix);
          }
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(blockFetcherMock.allBlocksFetched());
      REQUIRE(blockFetcherMock.numFetchBlockCalls() == 1);
    }

    WHEN("the producer returns HASMORE, then DONE with a nullptr") {
      blockFetcherMock.shouldReturn(ExecutionState::HASMORE, std::move(block))
          .andThenReturn(ExecutionState::DONE, nullptr);

      {
        AllRowsFetcher testee(blockFetcherMock);

        THEN("the fetcher should return the matrix with DONE") {
          std::tie(state, matrix) = testee.fetchAllRows();
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(matrix != nullptr);
          REQUIRE(matrix->getNrRegisters() == 1);
          REQUIRE(!matrix->empty());
          REQUIRE(matrix->size() == 1);
          REQUIRE(matrix->getRow(0).getValue(0).slice().getInt() == 42);

          AND_THEN("the same matrix should be returned") {
            AqlItemMatrix const* matrix2 = nullptr;
            std::tie(state, matrix2) = testee.fetchAllRows();
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(matrix2 == matrix);
          }
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(blockFetcherMock.allBlocksFetched());
      REQUIRE(blockFetcherMock.numFetchBlockCalls() == 2);
    }

    WHEN("the producer WAITs, then returns DONE") {
      blockFetcherMock.shouldReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::DONE, std::move(block));

      {
        AllRowsFetcher testee(blockFetcherMock);

        THEN("the fetcher should first return WAIT with nullptr") {
          std::tie(state, matrix) = testee.fetchAllRows();
          REQUIRE(state == ExecutionState::WAITING);
          REQUIRE(matrix == nullptr);

          AND_THEN("the fetcher should return the matrix with DONE") {
            std::tie(state, matrix) = testee.fetchAllRows();
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(matrix != nullptr);
            REQUIRE(!matrix->empty());
            REQUIRE(matrix->size() == 1);
            REQUIRE(matrix->getNrRegisters() == 1);
            REQUIRE(matrix->getRow(0).getValue(0).slice().getInt() == 42);

            AND_THEN("the same matrix should be returned") {
              AqlItemMatrix const* matrix2 = nullptr;
              std::tie(state, matrix2) = testee.fetchAllRows();
              REQUIRE(state == ExecutionState::DONE);
              REQUIRE(matrix2 == matrix);
            }
          }
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(blockFetcherMock.allBlocksFetched());
      REQUIRE(blockFetcherMock.numFetchBlockCalls() == 2);
    }

    WHEN("the producer WAITs, returns HASMORE, then DONE") {
      blockFetcherMock.shouldReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::HASMORE, std::move(block))
          .andThenReturn(ExecutionState::DONE, nullptr);

      {
        AllRowsFetcher testee(blockFetcherMock);

        THEN("the fetcher should first return WAIT with nullptr") {
          std::tie(state, matrix) = testee.fetchAllRows();
          REQUIRE(state == ExecutionState::WAITING);
          REQUIRE(matrix == nullptr);

          AND_THEN("the fetcher should return the matrix with DONE") {
            std::tie(state, matrix) = testee.fetchAllRows();
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(matrix != nullptr);
            REQUIRE(!matrix->empty());
            REQUIRE(matrix->size() == 1);
            REQUIRE(matrix->getNrRegisters() == 1);
            REQUIRE(matrix->getRow(0).getValue(0).slice().getInt() == 42);

            AND_THEN("the same matrix should be returned") {
              AqlItemMatrix const* matrix2 = nullptr;
              std::tie(state, matrix2) = testee.fetchAllRows();
              REQUIRE(state == ExecutionState::DONE);
              REQUIRE(matrix2 == matrix);
            }
          }
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(blockFetcherMock.allBlocksFetched());
      REQUIRE(blockFetcherMock.numFetchBlockCalls() == 3);
    }
  }

  // TODO the following tests should be simplified, a simple output
  // specification should be compared with the actual output.
  GIVEN("there are multiple blocks upstream") {
    ResourceMonitor monitor;
    AqlItemBlockManager itemBlockManager{&monitor};
    BlockFetcherMock<false> blockFetcherMock{monitor, 1};

    // three 1-column matrices with 3, 2 and 1 rows, respectively
    SharedAqlItemBlockPtr block1 =
                              buildBlock<1>(itemBlockManager, {{{1}}, {{2}}, {{3}}}),
                          block2 = buildBlock<1>(itemBlockManager, {{{4}}, {{5}}}),
                          block3 = buildBlock<1>(itemBlockManager, {{{6}}});

    WHEN("the producer does not wait") {
      blockFetcherMock.shouldReturn(ExecutionState::HASMORE, std::move(block1))
          .andThenReturn(ExecutionState::HASMORE, std::move(block2))
          .andThenReturn(ExecutionState::DONE, std::move(block3));

      {
        AllRowsFetcher testee(blockFetcherMock);

        THEN("the fetcher should return the matrix with DONE") {
          std::tie(state, matrix) = testee.fetchAllRows();
          REQUIRE(state == ExecutionState::DONE);
          REQUIRE(matrix != nullptr);
          REQUIRE(!matrix->empty());
          REQUIRE(matrix->size() == 6);
          REQUIRE(matrix->getNrRegisters() == 1);
          for (int64_t i = 0; i < 6; i++) {
            int64_t rowIdx = i;
            int64_t rowValue = i + 1;
            auto row = matrix->getRow(rowIdx);
            REQUIRE(row.isInitialized());
            REQUIRE(row.getValue(0).slice().getInt() == rowValue);
          }

          AND_THEN("the same matrix should be returned") {
            AqlItemMatrix const* matrix2 = nullptr;
            std::tie(state, matrix2) = testee.fetchAllRows();
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(matrix2 == matrix);
          }
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(blockFetcherMock.allBlocksFetched());
      REQUIRE(blockFetcherMock.numFetchBlockCalls() == 3);
    }

    WHEN("the producer waits") {
      blockFetcherMock.shouldReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::HASMORE, std::move(block1))
          .andThenReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::HASMORE, std::move(block2))
          .andThenReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::DONE, std::move(block3));

      {
        AllRowsFetcher testee(blockFetcherMock);

        THEN("the fetcher should return WAITING three times") {
          // wait when fetching the 1st and 2nd block
          std::tie(state, matrix) = testee.fetchAllRows();
          REQUIRE(state == ExecutionState::WAITING);
          REQUIRE(matrix == nullptr);
          std::tie(state, matrix) = testee.fetchAllRows();
          REQUIRE(state == ExecutionState::WAITING);
          REQUIRE(matrix == nullptr);
          std::tie(state, matrix) = testee.fetchAllRows();
          REQUIRE(state == ExecutionState::WAITING);
          REQUIRE(matrix == nullptr);

          AND_THEN("the fetcher should return the matrix and DONE") {
            // now get the matrix
            std::tie(state, matrix) = testee.fetchAllRows();
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(matrix != nullptr);
            REQUIRE(!matrix->empty());
            REQUIRE(matrix->size() == 6);
            REQUIRE(matrix->getNrRegisters() == 1);
            for (int64_t i = 0; i < 6; i++) {
              int64_t rowIdx = i;
              int64_t rowValue = i + 1;
              REQUIRE(matrix->getRow(rowIdx).getValue(0).slice().getInt() == rowValue);
            }

            AND_THEN("the same matrix should be returned") {
              AqlItemMatrix const* matrix2 = nullptr;
              std::tie(state, matrix2) = testee.fetchAllRows();
              REQUIRE(state == ExecutionState::DONE);
              REQUIRE(matrix2 == matrix);
            }
          }
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(blockFetcherMock.allBlocksFetched());
      REQUIRE(blockFetcherMock.numFetchBlockCalls() == 6);
    }

    WHEN("the producer waits and does not return DONE asap") {
      blockFetcherMock.shouldReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::HASMORE, std::move(block1))
          .andThenReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::HASMORE, std::move(block2))
          .andThenReturn(ExecutionState::WAITING, nullptr)
          .andThenReturn(ExecutionState::HASMORE, std::move(block3))
          .andThenReturn(ExecutionState::DONE, nullptr);

      {
        AllRowsFetcher testee(blockFetcherMock);

        THEN("the fetcher should return WAITING three times") {
          // wait when fetching the 1st and 2nd block
          std::tie(state, matrix) = testee.fetchAllRows();
          REQUIRE(state == ExecutionState::WAITING);
          REQUIRE(matrix == nullptr);
          std::tie(state, matrix) = testee.fetchAllRows();
          REQUIRE(state == ExecutionState::WAITING);
          REQUIRE(matrix == nullptr);
          std::tie(state, matrix) = testee.fetchAllRows();
          REQUIRE(state == ExecutionState::WAITING);
          REQUIRE(matrix == nullptr);

          AND_THEN("the fetcher should return the matrix and DONE") {
            // now get the matrix
            std::tie(state, matrix) = testee.fetchAllRows();
            REQUIRE(state == ExecutionState::DONE);
            REQUIRE(matrix != nullptr);
            REQUIRE(!matrix->empty());
            REQUIRE(matrix->size() == 6);
            REQUIRE(matrix->getNrRegisters() == 1);
            for (int64_t i = 0; i < 6; i++) {
              int64_t rowIdx = i;
              int64_t rowValue = i + 1;
              REQUIRE(matrix->getRow(rowIdx).getValue(0).slice().getInt() == rowValue);
            }

            AND_THEN("the same matrix should be returned") {
              AqlItemMatrix const* matrix2 = nullptr;
              std::tie(state, matrix2) = testee.fetchAllRows();
              REQUIRE(state == ExecutionState::DONE);
              REQUIRE(matrix2 == matrix);
            }
          }
        }
      }  // testee is destroyed here
      // testee must be destroyed before verify, because it may call returnBlock
      // in the destructor
      REQUIRE(blockFetcherMock.allBlocksFetched());
      REQUIRE(blockFetcherMock.numFetchBlockCalls() == 7);
    }
  }
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb
