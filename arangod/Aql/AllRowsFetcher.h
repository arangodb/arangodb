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
#include "Aql/ExecutionBlock.h"
#include "Aql/types.h"

#include <Basics/Common.h>
#include <Basics/Exceptions.h>

#include <cstddef>
#include <memory>

#include "Aql/AqlItemBlockInputMatrix.h"

namespace arangodb {
namespace aql {

class AqlItemBlock;
class SharedAqlItemBlockPtr;
enum class ExecutionState;
template <BlockPassthrough>
class DependencyProxy;
class ShadowAqlItemRow;
class SkipResult;

/**
 * @brief Interface for all AqlExecutors that do need all
 *        rows at a time in order to make progress.
 *
 *
 * Class Description and Guarantees
 * - Will have a single DependencyProxy only. This DependencyProxy cannot be PassThrough.
 * - It will offer the following APIs:
 *   - fetchAllRows()
 *     => Will fetch all Rows from the DependencyProxy until a ShadowRow is fetched
 *     => If any of the requests to DependencyProxy returns WAITING, this will be forwarded.
 *     => If all rows have been Fetched, it will return DONE and an AqlItemMatrix, the Matrix will return results
 *     => Any later call will return DONE and a nullptr. So make sure you keep the Matrix.
 *     => This state can be left only if the shadowRow is fetched.
 *   - preFetchNumberOfRows()
 *     => Will do the same as fetchAllRows, but NOT give out the data, it will only hold it internally.
 *     => On response it will inform the caller on exactly how many Rows will be returned until the next ShadowRow appears.
 *   - upstreamState()
 *     => Returns the last state of the dependencyProxy.
 *   - fetchShadowRow()
 *     => Can only be called after fetchAllRows()
 *     => It is supposed to pop the next shadowRow, namely the reason why fetchAllRows() was done.
 *     => You should continue to call fetchShadowRow() until you get either DONE or an invalid ShadowRow, as it could be possible that a higher level ShadowRow is next
 *     => If this call returns DONE your query is DONE and you can be sure no further Rows will be produced.
 *     => After this is called fetchAllRows() can return a new AqlMatrix again.
 *     => NOTE: If you have releveant ShadowRows in consecutive order, you are required to call fetchAllRows() in between of them. (This is required for COLLECT which needs to produce a row on 0 input).
 *
 *
 * - This class should be used if the Executor requires that ALL input is produced before it can start to work, e.g. it gives guarantees on side effects or needs to do Sorting.
 */
class AllRowsFetcher {
 private:
  enum FetchState {
    NONE,
    DATA_FETCH_ONGOING,
    ALL_DATA_FETCHED,
    SHADOW_ROW_FETCHED
  };

 public:
  explicit AllRowsFetcher(DependencyProxy<BlockPassthrough::Disable>& executionBlock);
  TEST_VIRTUAL ~AllRowsFetcher() = default;

  using DataRange = AqlItemBlockInputMatrix;

 protected:
  // only for testing! Does not initialize _dependencyProxy!
  AllRowsFetcher() = default;

 public:
  /**
   * @brief Execute the given call stack
   *
   * @param stack Call stack, on top of stack there is current subquery, bottom is the main query.
   * @return std::tuple<ExecutionState, size_t, DataRange>
   *   ExecutionState => DONE, all queries are done, there will be no more
   *   ExecutionState => HASMORE, there are more results for queries, might be on other subqueries
   *   ExecutionState => WAITING, we need to do I/O to solve the request, save local state and return WAITING to caller immediately
   *
   *   size_t => Amount of documents skipped
   *   DataRange => Resulting data
   */
  std::tuple<ExecutionState, SkipResult, DataRange> execute(AqlCallStack& stack);

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

  /**
   * @brief Fetch one new AqlItemRow from upstream.
   *        **Guarantee**: the row returned is valid only
   *        until the next call to fetchRow.
   *        **Guarantee**: All input rows have been produced from upstream before the first row is returned here
   *
   * @param atMost may be passed if a block knows the maximum it might want to
   *        fetch from upstream (should apply only to the LimitExecutor). Will
   *        not fetch more than the default batch size, so passing something
   *        greater than it will not have any effect.
   *
   * @return A pair with the following properties:
   *         ExecutionState:
   *           WAITING => IO going on, immediatly return to caller.
   *           DONE => No more to expect from Upstream, if you are done with
   *                   this row return DONE to caller.
   *           HASMORE => There is potentially more from above, call again if
   *                      you need more input.
   *         AqlItemRow:
   *           If WAITING => Do not use this Row, it is a nullptr.
   *           If HASMORE => The Row is guaranteed to not be a nullptr.
   *           If DONE => Row can be a nullptr (nothing received) or valid.
   */
  // This is only TEST_VIRTUAL, so we ignore this lint warning:
  // NOLINTNEXTLINE google-default-arguments
  std::pair<ExecutionState, InputAqlItemRow> fetchRow(size_t atMost = ExecutionBlock::DefaultBatchSize);

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
  ExecutionState upstreamState();

  // NOLINTNEXTLINE google-default-arguments
  std::pair<ExecutionState, ShadowAqlItemRow> fetchShadowRow(size_t atMost = ExecutionBlock::DefaultBatchSize);

  //@deprecated
  auto useStack(AqlCallStack const& stack) -> void;

 private:
  DependencyProxy<BlockPassthrough::Disable>* _dependencyProxy;

  std::unique_ptr<AqlItemMatrix> _aqlItemMatrix;
  ExecutionState _upstreamState;
  std::size_t _blockToReturnNext;

  FetchState _dataFetchedState;

  std::vector<AqlItemMatrix::RowIndex> _rowIndexes;
  std::size_t _nextReturn;

 private:
  /**
   * @brief Delegates to ExecutionBlock::getNrInputRegisters()
   */
  RegisterCount getNrInputRegisters() const;

  /**
   * @brief Delegates to ExecutionBlock::fetchBlock()
   */
  std::pair<ExecutionState, SharedAqlItemBlockPtr> fetchBlock();

  /**
   * @brief intermediate function to fetch data from
   *        upstream and does upstream state checking
   */
  ExecutionState fetchData();

  /**
   * @brief Fetch blocks from upstream until done
   */
  ExecutionState fetchUntilDone();
};

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_ALL_ROWS_FETCHER_H
