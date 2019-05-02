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

#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"

#include <memory>

namespace arangodb {
namespace aql {

class AqlItemBlock;
template <bool>
class DependencyProxy;

/**
 * @brief Interface for all AqlExecutors that do only need one
 *        row at a time in order to make progress.
 *        The guarantee is the following:
 *        If fetchRow returns a row the pointer to
 *        this row stays valid until the next call
 *        of fetchRow.
 */
template <bool passBlocksThrough>
class SingleRowFetcher {
 public:
  explicit SingleRowFetcher(DependencyProxy<passBlocksThrough>& executionBlock);
  TEST_VIRTUAL ~SingleRowFetcher() = default;

 protected:
  // only for testing! Does not initialize _dependencyProxy!
  SingleRowFetcher();

 public:
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
  TEST_VIRTUAL std::pair<ExecutionState, InputRowRef> fetchRow(
      size_t atMost = ExecutionBlock::DefaultBatchSize());

  // TODO enable_if<passBlocksThrough>
  std::pair<ExecutionState, SharedAqlItemBlockPtr> fetchBlockForPassthrough(size_t atMost);

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

 private:
  DependencyProxy<passBlocksThrough>* _dependencyProxy;

  /**
   * @brief Holds state returned by the last fetchBlock() call.
   *        This is similar to ExecutionBlock::_upstreamState, but can also be
   *        WAITING.
   *        Part of the Fetcher, and may be moved if the Fetcher implementations
   *        are moved into separate classes.
   */
  ExecutionState _upstreamState;

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

 private:
  /**
   * @brief Delegates to ExecutionBlock::fetchBlock()
   */
  std::pair<ExecutionState, SharedAqlItemBlockPtr> fetchBlock(size_t atMost);

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
};

template <bool passBlocksThrough>
// NOLINTNEXTLINE google-default-arguments
std::pair<ExecutionState, InputRowRef> SingleRowFetcher<passBlocksThrough>::fetchRow(size_t atMost) {
  // Fetch a new block iff necessary
  if (!indexIsValid()) {
    // This returns the AqlItemBlock to the ItemBlockManager before fetching a
    // new one, so we might reuse it immediately!
    _currentBlock = nullptr;

    ExecutionState state;
    SharedAqlItemBlockPtr newBlock;
    std::tie(state, newBlock) = fetchBlock(atMost);
    if (state == ExecutionState::WAITING) {
      return {ExecutionState::WAITING, InvalidInputAqlItemRow};
    }

    _currentBlock = std::move(newBlock);
    _rowIndex = 0;
  }

  ExecutionState rowState;

  if (_currentBlock == nullptr) {
    TRI_ASSERT(_upstreamState == ExecutionState::DONE);
    _currentRow = InputAqlItemRow{CreateInvalidInputRowHint{}};
    rowState = ExecutionState::DONE;
  } else {
    TRI_ASSERT(_currentBlock != nullptr);
    // When _rowIndex is 0, we got a new block - either just now (from the
    // `if (!indexIsValid())` branch) or from a preFetchNumberOfRows() call.
    // Otherwise we can just increase the rows index instead of recreating it.
    // Because of preFetchNumberOfRows() we sadly cannot avoid the check here.
    if (_rowIndex > 0) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      TRI_ASSERT(_currentRow.internalBlockIs(_currentBlock));
      TRI_ASSERT(!_currentRow.isLastRowInBlock());
#endif
      _currentRow.moveToNextRow();
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      TRI_ASSERT(_currentRow.internalIndexIs(_rowIndex));
#endif
    } else {
      _currentRow = InputAqlItemRow{_currentBlock, _rowIndex};
    }

    TRI_ASSERT(_upstreamState != ExecutionState::WAITING);
    if (isLastRowInBlock() && _upstreamState == ExecutionState::DONE) {
      rowState = ExecutionState::DONE;
    } else {
      rowState = ExecutionState::HASMORE;
    }

    _rowIndex++;
  }
  return {rowState, _currentRow};
}

template <bool passBlocksThrough>
bool SingleRowFetcher<passBlocksThrough>::indexIsValid() const {
  return _currentBlock != nullptr && _rowIndex < _currentBlock->size();
}

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_SINGLE_ROW_FETCHER_H
