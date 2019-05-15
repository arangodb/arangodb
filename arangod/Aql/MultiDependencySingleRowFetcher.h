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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_MULTI_DEPENDENCY_SINGLE_ROW_FETCHER_H
#define ARANGOD_AQL_MULTI_DEPENDENCY_SINGLE_ROW_FETCHER_H

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
 * @brief Interface for all AqlExecutors that do need one
 *        row at a time from every dependency in order to make progress.
 *        The guarantee is the following:
 *        If fetchRowForDependency returns a row of the given dependency,
 *        the pointer to
 *        this row stays valid until the next call
 *        of fetchRowForDependency.
 *        So we can have one Row per dependency in flight.
 */
class MultiDependencySingleRowFetcher {
 private:
  /**
   * @brief helper struct to contain all information about dependency-states
   */
  struct DependencyInfo {
   public:
    DependencyInfo();
    ~DependencyInfo() = default;
    /**
     * @brief Holds state returned by the last fetchBlock() call.
     *        This is similar to ExecutionBlock::_upstreamState, but can also be
     *        WAITING.
     *        Part of the Fetcher, and may be moved if the Fetcher
     * implementations are moved into separate classes.
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
  };

 public:
  explicit MultiDependencySingleRowFetcher(DependencyProxy<false>& executionBlock);
  TEST_VIRTUAL ~MultiDependencySingleRowFetcher() = default;

 protected:
  // only for testing! Does not initialize _dependencyProxy!
  MultiDependencySingleRowFetcher();

 public:
  std::pair<ExecutionState, InputAqlItemRow> fetchRow(size_t atMost = ExecutionBlock::DefaultBatchSize()) {
    // This is not implemented for this fetcher
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  std::pair<ExecutionState, SharedAqlItemBlockPtr> fetchBlockForPassthrough(size_t atMost) {
    // This is not implemented for this fetcher
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  }

  std::pair<ExecutionState, size_t> preFetchNumberOfRows(size_t atMost) {
    ExecutionState state = ExecutionState::DONE;
    size_t available = 0;
    for (size_t i = 0; i < numberDependencies(); ++i) {
      auto res = preFetchNumberOfRowsForDependency(i, atMost);
      if (res.first == ExecutionState::WAITING) {
        return {ExecutionState::WAITING, 0};
      }
      available += res.second;
      if (res.first == ExecutionState::HASMORE) {
        state = ExecutionState::HASMORE;
      }
    }
    return {state, available};
  }

  size_t numberDependencies();

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
  TEST_VIRTUAL std::pair<ExecutionState, InputAqlItemRow> fetchRowForDependency(
      size_t dependency, size_t atMost = ExecutionBlock::DefaultBatchSize()) {
    TRI_ASSERT(dependency < numberDependencies());
    auto& depInfo = _dependencyInfos[dependency];
    // Fetch a new block iff necessary
    if (!indexIsValid(depInfo) && !isDone(depInfo)) {
      // This returns the AqlItemBlock to the ItemBlockManager before fetching a
      // new one, so we might reuse it immediately!
      depInfo._currentBlock = nullptr;

      ExecutionState state;
      SharedAqlItemBlockPtr newBlock;
      std::tie(state, newBlock) = fetchBlockForDependency(dependency, atMost);
      if (state == ExecutionState::WAITING) {
        return {ExecutionState::WAITING, InputAqlItemRow{CreateInvalidInputRowHint{}}};
      }

      depInfo._currentBlock = std::move(newBlock);
      depInfo._rowIndex = 0;
    }

    ExecutionState rowState;
    InputAqlItemRow row = InputAqlItemRow{CreateInvalidInputRowHint{}};
    if (depInfo._currentBlock == nullptr) {
      TRI_ASSERT(depInfo._upstreamState == ExecutionState::DONE);
      rowState = ExecutionState::DONE;
    } else {
      TRI_ASSERT(depInfo._currentBlock != nullptr);
      row = InputAqlItemRow{depInfo._currentBlock, depInfo._rowIndex};

      TRI_ASSERT(depInfo._upstreamState != ExecutionState::WAITING);
      if (isLastRowInBlock(depInfo) && depInfo._upstreamState == ExecutionState::DONE) {
        depInfo._currentBlock = nullptr;
        depInfo._rowIndex = 0;
        rowState = ExecutionState::DONE;
      } else {
        depInfo._rowIndex++;
        rowState = ExecutionState::HASMORE;
      }

    }

    return {rowState, row};
  }

 private:
  DependencyProxy<false>* _dependencyProxy;

  /**
   * @brief Holds the information for all dependencies
   */
  std::vector<DependencyInfo> _dependencyInfos;

 private:
  /**
   * @brief Delegates to ExecutionBlock::fetchBlock()
   */
  std::pair<ExecutionState, SharedAqlItemBlockPtr> fetchBlockForDependency(size_t dependency,
                                                                           size_t atMost);

  /**
   * @brief Delegates to ExecutionBlock::getNrInputRegisters()
   */
  RegisterId getNrInputRegisters() const;

  bool indexIsValid(DependencyInfo const& info) const {
    return info._currentBlock != nullptr && info._rowIndex < info._currentBlock->size();
  }

  bool isDone(DependencyInfo const& info) const {
    return info._upstreamState == ExecutionState::DONE;
  }

  bool isLastRowInBlock(DependencyInfo const& info) const {
    TRI_ASSERT(indexIsValid(info));
    return info._rowIndex + 1 == info._currentBlock->size();
  }

  size_t getRowIndex(DependencyInfo const& info) const {
    TRI_ASSERT(indexIsValid(info));
    return info._rowIndex;
  }
  std::pair<ExecutionState, size_t> preFetchNumberOfRowsForDependency(size_t dependency,
                                                                      size_t atMost) {
    TRI_ASSERT(dependency < _dependencyInfos.size());
    auto& depInfo = _dependencyInfos[dependency];
    // Fetch a new block iff necessary
    if (!indexIsValid(depInfo) && !isDone(depInfo)) {
      // This returns the AqlItemBlock to the ItemBlockManager before fetching a
      // new one, so we might reuse it immediately!
      depInfo._currentBlock = nullptr;

      ExecutionState state;
      SharedAqlItemBlockPtr newBlock;
      std::tie(state, newBlock) = fetchBlockForDependency(dependency, atMost);
      if (state == ExecutionState::WAITING) {
        return {ExecutionState::WAITING, 0};
      }

      depInfo._currentBlock = std::move(newBlock);
      depInfo._rowIndex = 0;
    }

    if (!indexIsValid(depInfo)) {
      TRI_ASSERT(depInfo._upstreamState == ExecutionState::DONE);
      return {ExecutionState::DONE, 0};
    } else {
      if (isDone(depInfo)) {
        TRI_ASSERT(depInfo._currentBlock != nullptr);
        TRI_ASSERT(depInfo._currentBlock->size() > depInfo._rowIndex);
        return {depInfo._upstreamState, depInfo._currentBlock->size() - depInfo._rowIndex};
      }
      // In the HAS_MORE case we do not know exactly how many rows there are.
      // So we need to return an uppter bound (atMost) here.
      return {depInfo._upstreamState, atMost};
    }
  }
};

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_SINGLE_ROW_FETCHER_H
