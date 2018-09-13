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

#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"

#include <memory>

namespace arangodb {
namespace aql {

class AqlItemBlock;
class BlockFetcher;

/**
 * @brief Interface for all AqlExecutors that do only need one
 *        row at a time in order to make progress.
 *        The guarantee is the following:
 *        If fetchRow returns a row the pointer to
 *        this row stays valid until the next call
 *        of fetchRow.
 */
class SingleRowFetcher {
 public:
  explicit SingleRowFetcher(BlockFetcher& executionBlock);
  TEST_VIRTUAL ~SingleRowFetcher();

 protected:
  // only for testing! Does not initialize _blockFetcher!
  SingleRowFetcher();

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
   *                   this row return DONE to caller.
   *           HASMORE => There is potentially more from above, call again if
   *                      you need more input.
   *         AqlItemRow:
   *           If WAITING => Do not use this Row, it is a nullptr.
   *           If HASMORE => The Row is guaranteed to not be a nullptr.
   *           If DONE => Row can be a nullptr (nothing received) or valid.
   */
  TEST_VIRTUAL std::pair<ExecutionState, InputAqlItemRow> fetchRow();

 private:
  BlockFetcher* _blockFetcher;

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
  std::unique_ptr<AqlItemBlock> _currentBlock;

  /**
  * @brief Unique block ID, given by this class to every AqlItemBlock in
  *        fetchBlock().
  *
  *        Internally, blocks are numbered consecutively starting from 0, but
  *        this is not guaranteed.
  */
  InputAqlItemRow::AqlItemBlockId _blockId;

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
  std::pair<ExecutionState, std::unique_ptr<AqlItemBlock>> fetchBlock();

  /**
   * @brief Delegates to ExecutionBlock::getNrInputRegisters()
   */
  RegisterId getNrInputRegisters() const;

  bool indexIsValid();

  bool isLastRowInBlock();

  size_t getRowIndex();

  /**
  * @brief return block to the BlockFetcher (and, by extension, to the
  *        AqlItemBlockManager)
  */
  void returnCurrentBlock() noexcept;
};

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_SINGLE_ROW_FETCHER_H
