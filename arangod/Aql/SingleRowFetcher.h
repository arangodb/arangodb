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

/**
 * @brief Interface for all AqlExecutors that do only need one
 *        row at a time in order to make progress.
 *        The guarantee is the following:
 *        If fetchRow returns a row the pointer to
 *        this row stays valid until the next call
 *        of fetchRow.
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
  // TODO implement and document
  std::tuple<ExecutionState, size_t, DataRange> execute(AqlCallStack& stack);

  /**
   * @brief Fetch one new AqlItemRow from upstream.
   *
   * @param atMost may be passed if a block knows the maximum it might want to
   *        fetch from upstream (should apply only to the LimitExecutor). Will
   *        not fetch more than the default batch size, so passing something
   *        greater than it will not have any effect.
   *
   * @return A pair with the following properties:
   *         ExecutionState:
   *           WAITING => IO going on, immediately return to caller.
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
  [[nodiscard]] TEST_VIRTUAL std::pair<ExecutionState, InputAqlItemRow> fetchRow(
      size_t atMost = ExecutionBlock::DefaultBatchSize());

  // NOLINTNEXTLINE google-default-arguments
  [[nodiscard]] TEST_VIRTUAL std::pair<ExecutionState, ShadowAqlItemRow> fetchShadowRow(
      size_t atMost = ExecutionBlock::DefaultBatchSize());

  [[nodiscard]] TEST_VIRTUAL std::pair<ExecutionState, size_t> skipRows(size_t atMost);

  // template methods may not be virtual, thus this #ifdef.
#ifndef ARANGODB_USE_GOOGLE_TESTS
  template <BlockPassthrough allowPass = blockPassthrough, typename = std::enable_if_t<allowPass == BlockPassthrough::Enable>>
  [[nodiscard]]
#else
  [[nodiscard]] TEST_VIRTUAL
#endif
  std::pair<ExecutionState, SharedAqlItemBlockPtr>
  fetchBlockForPassthrough(size_t atMost);

  [[nodiscard]] std::pair<ExecutionState, size_t> preFetchNumberOfRows(size_t atMost);

  void setDistributeId(std::string const& id);

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  [[nodiscard]] bool hasRowsLeftInBlock() const;
  [[nodiscard]] bool isAtShadowRow() const;
#endif

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
   * @brief Index of the row to be returned next by fetchRow(). This is valid
   *        iff _currentBlock != nullptr and it's smaller or equal than
   *        _currentBlock->size(). May be moved if the Fetcher implementations
   *        are moved into separate classes.
   */
  size_t _rowIndex;

  /**
   * @brief The current row, as returned last by fetchRow(). Must stay valid
   *        until the next fetchRow() call.
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
  [[nodiscard]] RegisterId getNrInputRegisters() const;

  [[nodiscard]] bool indexIsValid() const;

  [[nodiscard]] ExecutionState returnState(bool isShadowRow) const;
};
}  // namespace arangodb::aql

#endif  // ARANGOD_AQL_SINGLE_ROW_FETCHER_H
