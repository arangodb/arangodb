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

namespace arangodb {
namespace aql {

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
  std::tuple<ExecutionState, size_t, DataRange> execute(AqlCallStack& stack) {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  /**
   * @brief Fetch one new AqlItemRow from upstream.
   *        **Guarantee**: the row returned is valid only
   *        until the next call to fetchRow.
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
  TEST_VIRTUAL std::pair<ExecutionState, InputAqlItemRow> fetchRow(
      size_t atMost = ExecutionBlock::DefaultBatchSize());

  // TODO: atMost?
  // NOLINTNEXTLINE google-default-arguments
  TEST_VIRTUAL std::pair<ExecutionState, ShadowAqlItemRow> fetchShadowRow(
      size_t atMost = ExecutionBlock::DefaultBatchSize());

  TEST_VIRTUAL std::pair<ExecutionState, size_t> skipRows(size_t atMost);

  // TODO enable_if<blockPassthrough>
  // std::enable_if<blockPassthrough == BlockPassthrough::Enable>
  TEST_VIRTUAL std::pair<ExecutionState, SharedAqlItemBlockPtr> fetchBlockForPassthrough(size_t atMost);

  std::pair<ExecutionState, size_t> preFetchNumberOfRows(size_t atMost) {
    if (_upstreamState != ExecutionState::DONE && !indexIsValid()) {
      // We have exhausted the current block and need to fetch a new one
      ExecutionState state;
      SharedAqlItemBlockPtr newBlock;
      std::tie(state, newBlock) = fetchBlock(atMost);
      // we de not need result as local members are modified
      if (state == ExecutionState::WAITING) {
        return {state, 0};
      }
      // The internal state should be in-line with the returned state.
      TRI_ASSERT(_upstreamState == state);
      _currentBlock = std::move(newBlock);
      _rowIndex = 0;
    }

    // The block before can put upstreamState to DONE.
    // So we cannot swap the blocks
    if (_upstreamState == ExecutionState::DONE) {
      if (!indexIsValid()) {
        // There is nothing more from upstream
        return {_upstreamState, 0};
      }
      // we only have the block in hand, so we can only return that
      // many additional rows
      TRI_ASSERT(_rowIndex < _currentBlock->size());
      return {_upstreamState, (std::min)(_currentBlock->size() - _rowIndex, atMost)};
    }

    TRI_ASSERT(_upstreamState == ExecutionState::HASMORE);
    TRI_ASSERT(_currentBlock != nullptr);
    // Here we can only assume that we have enough from upstream
    // We do not want to pull additional block
    return {_upstreamState, atMost};
  }

  void setDistributeId(std::string const& id) {
    _dependencyProxy->setDistributeId(id);
  }

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
  TEST_VIRTUAL std::pair<ExecutionState, SharedAqlItemBlockPtr> fetchBlock(size_t atMost);

  bool fetchBlockIfNecessary(size_t atMost);

  /**
   * @brief Delegates to ExecutionBlock::getNrInputRegisters()
   */
  RegisterId getNrInputRegisters() const {
    return _dependencyProxy->getNrInputRegisters();
  }
  bool indexIsValid() const;

  bool isLastRowInBlock() const {
    TRI_ASSERT(indexIsValid());
    return _rowIndex + 1 == _currentBlock->size();
  }

  size_t getRowIndex() const {
    TRI_ASSERT(indexIsValid());
    return _rowIndex;
  }

  ExecutionState returnState(bool isShadowRow) const;
};
}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_SINGLE_ROW_FETCHER_H
