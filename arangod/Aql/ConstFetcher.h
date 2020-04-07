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

#ifndef ARANGOD_AQL_CONST_FETCHER_H
#define ARANGOD_AQL_CONST_FETCHER_H

#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"

#include <memory>

namespace arangodb {
namespace aql {

class AqlCallStack;
class AqlItemBlock;
template <BlockPassthrough>
class DependencyProxy;
class ShadowAqlItemRow;
class SkipResult;

/**
 * @brief Interface for all AqlExecutors that do only need one
 *        row at a time in order to make progress.
 *        The guarantee is the following:
 *        If fetchRow returns a row the pointer to
 *        this row stays valid until the next call
 *        of fetchRow.
 */
class ConstFetcher {
  using DependencyProxy = aql::DependencyProxy<BlockPassthrough::Enable>;

 public:
  using DataRange = AqlItemBlockInputRange;
  explicit ConstFetcher(DependencyProxy& executionBlock);
  TEST_VIRTUAL ~ConstFetcher() = default;

 protected:
  // only for testing! Does not initialize _dependencyProxy!
  ConstFetcher();

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
  auto execute(AqlCallStack& stack) -> std::tuple<ExecutionState, SkipResult, DataRange>;

  /**
   * @brief Fetch one new AqlItemRow from upstream.
   *        **Guarantee**: the pointer returned is valid only
   *        until the next call to fetchRow.
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
  TEST_VIRTUAL std::pair<ExecutionState, InputAqlItemRow> fetchRow(size_t atMost = 1);
  TEST_VIRTUAL std::pair<ExecutionState, size_t> skipRows(size_t);
  void injectBlock(SharedAqlItemBlockPtr block);

  void setDistributeId(std::string const&) {
    // This is not implemented for this fetcher
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  // At most does not matter for this fetcher. It will return DONE anyways
  // NOLINTNEXTLINE google-default-arguments
  std::pair<ExecutionState, ShadowAqlItemRow> fetchShadowRow(size_t atMost = 1) const;

  //@deprecated
  auto useStack(AqlCallStack const& stack) -> void{};

 private:
  /**
   * @brief Input block currently in use. Used for memory management by the
   *        ConstFetcher. May be moved if the Fetcher implementations
   *        are moved into separate classes.
   */
  SharedAqlItemBlockPtr _currentBlock;

  SharedAqlItemBlockPtr _blockForPassThrough;

  /**
   * @brief Index of the row to be returned next by fetchRow(). This is valid
   *        iff _currentBlock != nullptr and it's smaller or equal than
   *        _currentBlock->size(). May be moved if the Fetcher implementations
   *        are moved into separate classes.
   */
  size_t _rowIndex;

 private:
  auto indexIsValid() const noexcept -> bool;
  auto isLastRowInBlock() const noexcept -> bool;
  auto numRowsLeft() const noexcept -> size_t;
  auto canUseFullBlock(std::vector<std::pair<size_t, size_t>> const& ranges) const
      noexcept -> bool;
};

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_SINGLE_ROW_FETCHER_H
