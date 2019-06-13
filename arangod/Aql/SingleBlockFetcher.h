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

#ifndef ARANGOD_AQL_SINGLE_BLOCK_FETCHER_H
#define ARANGOD_AQL_SINGLE_BLOCK_FETCHER_H

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemMatrix.h"
#include "Aql/DependencyProxy.h"
#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/SortExecutor.h"

#include <Basics/Exceptions.h>

#include <memory>

namespace arangodb {
namespace aql {

class AqlItemBlock;
template <bool>
class DependencyProxy;

/**
 * @brief Interface for all AqlExecutors that do need all
 *        rows at a time in order to make progress.
 */
template <bool pass>
class SingleBlockFetcher {
 public:
  explicit SingleBlockFetcher(DependencyProxy<pass>& executionBlock)
      : _prefetched(false),
        _dependencyProxy(&executionBlock),
        _currentBlock(nullptr),
        _upstreamState(ExecutionState::HASMORE) {}

  TEST_VIRTUAL ~SingleBlockFetcher() = default;

 protected:
  // only for testing! Does not initialize _dependencyProxy!
  SingleBlockFetcher() = default;

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

  // SingleBlockFetcher cannot pass through. Could be implemented, but currently
  // there are no executors that could use this and not better use
  // SingleRowFetcher instead.

  std::pair<ExecutionState, SharedAqlItemBlockPtr> fetchBlock(
      std::size_t limit = ExecutionBlock::DefaultBatchSize(), bool prefetch = false) {
    if (_prefetched) {
      TRI_ASSERT(!prefetch);
      _prefetched = false;
      return {_upstreamState, _currentBlock};
    }

    if (_upstreamState == ExecutionState::DONE) {
      TRI_ASSERT(_currentBlock == nullptr);
      return {_upstreamState, _currentBlock};
    }

    auto res = _dependencyProxy->fetchBlock(limit);
    _upstreamState = res.first;
    _currentBlock = res.second;

    if (prefetch && _currentBlock != nullptr) {
      _prefetched = prefetch;
    }

    return res;
  }

  std::pair<ExecutionState, SharedAqlItemBlockPtr> fetchBlockForModificationExecutor(
      std::size_t limit = ExecutionBlock::DefaultBatchSize()) {
    return fetchBlock(limit);
  }

  std::pair<ExecutionState, SharedAqlItemBlockPtr> fetchBlockForPassthrough(size_t) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_NOT_IMPLEMENTED);
  };

  std::pair<ExecutionState, std::size_t> preFetchNumberOfRows(std::size_t) {
    fetchBlock(true);
    return {_upstreamState, _currentBlock != nullptr ? _currentBlock->size() : 0};
  }

  InputAqlItemRow accessRow(std::size_t index) {
    TRI_ASSERT(_currentBlock != nullptr);
    TRI_ASSERT(index < _currentBlock->size());
    return InputAqlItemRow{_currentBlock, index};
  }

  ExecutionState upstreamState() const { return _upstreamState; }
  SharedAqlItemBlockPtr currentBlock() const { return _currentBlock; }

  bool _prefetched;

 private:
  DependencyProxy<pass>* _dependencyProxy;
  SharedAqlItemBlockPtr _currentBlock;
  ExecutionState _upstreamState;

 private:
  /**
   * @brief Delegates to ExecutionBlock::getNrInputRegisters()
   */
  RegisterId getNrInputRegisters() const {
    return _dependencyProxy->getNrInputRegisters();
  }
};

}  // namespace aql
}  // namespace arangodb
#endif  // ARANGOD_AQL_SINGLE_BLOCK_FETCHER_H
