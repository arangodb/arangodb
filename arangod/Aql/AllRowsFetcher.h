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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_ALL_ROWS_FETCHER_H
#define ARANGOD_AQL_ALL_ROWS_FETCHER_H

#include "Aql/AqlItemMatrix.h"
#include "Aql/ExecutionState.h"

#include <Basics/Exceptions.h>

#include <memory>

namespace arangodb {
namespace aql {

class AqlItemBlock;
template <bool>
class BlockFetcher;

/**
 * @brief Interface for all AqlExecutors that do need all
 *        rows at a time in order to make progress.
 */
class AllRowsFetcher {
 public:
  explicit AllRowsFetcher(BlockFetcher<false>& executionBlock);

  TEST_VIRTUAL ~AllRowsFetcher() = default;

 protected:
  // only for testing! Does not initialize _blockFetcher!
  AllRowsFetcher() = default;

 public:
  /**
   * @brief Fetch one new AqlItemRow from upstream.
   *        **Guarantee**: the pointer returned is valid only
   *        until the next call to fetchRow.
   *
   * @return A pair with the following properties:
   *         ExecutionState:
   *           WAITING => IO going on, immediatly return to caller.
   *           DONE => No more to expect from Upstream, if you are done with
   *                   this Matrix return DONE to caller.
   *           HASMORE => Cannot be returned here
   *
   *         AqlItemRow:
   *           If WAITING => Do not use this Row, it is a nullptr.
   *           If HASMORE => impossible
   *           If DONE => Row can be a nullptr (nothing received) or valid.
   */
  TEST_VIRTUAL std::pair<ExecutionState, AqlItemMatrix const*> fetchAllRows();

  // AllRowsFetcher cannot pass through. Could be implemented, but currently
  // there are no executors that could use this and not better use
  // SingleRowFetcher instead.
  std::pair<ExecutionState, SharedAqlItemBlockPtr> fetchBlockForPassthrough(size_t) {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  };

  /**
   * @brief Prefetch the number of rows that will be returned from upstream.
   * calling this function will render the fetchAllRows() a noop function
   * as this function will already fill the local result caches.
   *
   * @return A pair with the following properties:
   *         ExecutionState:
   *           WAITING => IO going on, immediatly return to caller.
   *           DONE => No more to expect from Upstream, if you are done with
   *                   this Matrix return DONE to caller.
   *           HASMORE => Cannot be returned here
   *
   *         AqlItemRow:
   *           If WAITING => Do not use this number, it is 0.
   *           If HASMORE => impossible
   *           If DONE => Number contains the correct number of rows upstream.
   */
  TEST_VIRTUAL std::pair<ExecutionState, size_t> preFetchNumberOfRows(size_t);

  // only for ModificationNodes
  std::pair<ExecutionState, SharedAqlItemBlockPtr> fetchBlockForModificationExecutor(std::size_t);

  // only for ModificationNodes
  ExecutionState upstreamState();

 private:
  BlockFetcher<false>* _blockFetcher;

  std::unique_ptr<AqlItemMatrix> _aqlItemMatrix;
  ExecutionState _upstreamState;
  std::size_t _blockToReturnNext;

 private:
  /**
   * @brief Delegates to ExecutionBlock::getNrInputRegisters()
   */
  RegisterId getNrInputRegisters() const;

  /**
   * @brief Delegates to ExecutionBlock::fetchBlock()
   */
  std::pair<ExecutionState, SharedAqlItemBlockPtr> fetchBlock();

  /**
   * @brief Fetch blocks from upstream until done
   */
  ExecutionState fetchUntilDone();
};

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_ALL_ROWS_FETCHER_H
