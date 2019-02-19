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
class BlockFetcher;

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
  explicit SingleRowFetcher(BlockFetcher<passBlocksThrough>& executionBlock);
  TEST_VIRTUAL ~SingleRowFetcher() = default;

 protected:
  // only for testing! Does not initialize _blockFetcher!
  SingleRowFetcher();

 public:
  /**
   * @brief Fetch one new AqlItemRow from upstream.
   *        **Guarantee**: the pointer returned is valid only
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

  // TODO enable_if<passBlocksThrough>
  std::pair<ExecutionState, std::shared_ptr<AqlItemBlockShell>> fetchBlockForPassthrough(size_t atMost);

  std::pair<ExecutionState, size_t> preFetchNumberOfRows() {
    // This is not implemented for this fetcher
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INTERNAL);
  }

 private:
  BlockFetcher<passBlocksThrough>* _blockFetcher;

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
  std::shared_ptr<AqlItemBlockShell> _currentBlock;

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
  std::pair<ExecutionState, std::shared_ptr<AqlItemBlockShell>> fetchBlock(size_t atMost);

  /**
   * @brief Delegates to ExecutionBlock::getNrInputRegisters()
   */
  RegisterId getNrInputRegisters() const {
    return _blockFetcher->getNrInputRegisters();
  }
  bool indexIsValid() const;

  bool isLastRowInBlock() const {
    TRI_ASSERT(indexIsValid());
    return _rowIndex + 1 == _currentBlock->block().size();
  }

  size_t getRowIndex() const {
    TRI_ASSERT(indexIsValid());
    return _rowIndex;
  }
};

template <bool passBlocksThrough>
// NOLINTNEXTLINE google-default-arguments
std::pair<ExecutionState, InputAqlItemRow> SingleRowFetcher<passBlocksThrough>::fetchRow(size_t atMost) {
  // Fetch a new block iff necessary
  if (!indexIsValid()) {
    // This returns the AqlItemBlock to the ItemBlockManager before fetching a
    // new one, so we might reuse it immediately!
    _currentBlock = nullptr;

    ExecutionState state;
    std::shared_ptr<AqlItemBlockShell> newBlock;
    std::tie(state, newBlock) = fetchBlock(atMost);
    if (state == ExecutionState::WAITING) {
      return {ExecutionState::WAITING, InputAqlItemRow{CreateInvalidInputRowHint{}}};
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
    TRI_ASSERT(_currentBlock);
    _currentRow = InputAqlItemRow{_currentBlock, _rowIndex};

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
  // TODO Hopefully we can get rid of this distinction later. When there are no
  // more old blocks, we can replace the old getSome interface easily,
  // specifically replace std::unique_ptr<AqlItemBlock> with a shared_ptr
  // with a custom deleter (like AqlItemBlockShell), or a shared_ptr to an
  // AqlItemBlockShell. Then we will never lose the block.
  if /* constexpr */ (passBlocksThrough) {
    return _currentBlock != nullptr && _currentBlock->hasBlock() &&
           _rowIndex < _currentBlock->block().size();
  } else {
    // The current block must never become invalid (i.e. !hasBlock()), unless
    // it's passed through and therefore the output block steals it.
    TRI_ASSERT(_currentBlock == nullptr || _currentBlock->hasBlock());
    return _currentBlock != nullptr && _rowIndex < _currentBlock->block().size();
  }
}

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_SINGLE_ROW_FETCHER_H
