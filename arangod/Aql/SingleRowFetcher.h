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

#ifndef ARANGOD_AQL_SINGLE_ROW_FETCHER_H
#define ARANGOD_AQL_SINGLE_ROW_FETCHER_H

#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/ShadowAqlItemRow.h"
#include "Aql/types.h"

#include <memory>

namespace arangodb::aql {

class AqlItemBlock;
template <BlockPassthrough>
class DependencyProxy;
class SkipResult;

/**
 * @brief Interface for all AqlExecutors that do only need one
 *        row at a time in order to make progress.
 */
template <BlockPassthrough blockPassthrough>
class SingleRowFetcher {
 public:
  explicit SingleRowFetcher(DependencyProxy<blockPassthrough>& executionBlock);
  TEST_VIRTUAL ~SingleRowFetcher() = default;

  using DataRange = AqlItemBlockInputRange;

 protected:
  // only for testing! Does not initialize _dependencyProxy!
  SingleRowFetcher();

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

  void setDistributeId(std::string const& id);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  [[nodiscard]] bool hasRowsLeftInBlock() const;
  [[nodiscard]] bool isAtShadowRow() const;
#endif

  //@deprecated
  auto useStack(AqlCallStack const& stack) -> void;

 private:
  DependencyProxy<blockPassthrough>* _dependencyProxy;

  /**
   * @brief Holds state returned by the last fetchBlock() call.
   *        This is similar to ExecutionBlock::_upstreamState, but can also be
   *        WAITING.
   *        Part of the Fetcher, and may be moved if the Fetcher implementations
   *        are moved into separate classes.
   */
#ifdef ARANGODB_USE_GOOGLE_TESTS
 protected:
#endif
  // NOLINTNEXTLINE(cppcoreguidelines-non-private-member-variables-in-classes, misc-non-private-member-variables-in-classes)
  ExecutionState _upstreamState;
#ifdef ARANGODB_USE_GOOGLE_TESTS
 private:
#endif
  /**
   * @brief Input block currently in use. Used for memory management by the
   *        SingleRowFetcher. May be moved if the Fetcher implementations
   *        are moved into separate classes.
   */
  SharedAqlItemBlockPtr _currentBlock;

  /**
   * @brief This is valid iff _currentBlock != nullptr and it's smaller or equal
   *        than _currentBlock->size(). May be moved if the Fetcher implementations
   *        are moved into separate classes.
   */
  size_t _rowIndex;

  /**
   * @brief
   */
  InputAqlItemRow _currentRow;
  ShadowAqlItemRow _currentShadowRow;

 private:
  /**
   * @brief Delegates to ExecutionBlock::fetchBlock()
   */
  [[nodiscard]] TEST_VIRTUAL std::pair<ExecutionState, SharedAqlItemBlockPtr> fetchBlock(size_t atMost);

  [[nodiscard]] bool fetchBlockIfNecessary(size_t atMost);

  /**
   * @brief Delegates to ExecutionBlock::getNrInputRegisters()
   */
  [[nodiscard]] RegisterCount getNrInputRegisters() const;

  [[nodiscard]] bool indexIsValid() const;

  [[nodiscard]] ExecutionState returnState(bool isShadowRow) const;
};
}  // namespace arangodb::aql

#endif  // ARANGOD_AQL_SINGLE_ROW_FETCHER_H
