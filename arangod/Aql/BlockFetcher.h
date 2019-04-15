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

#ifndef ARANGOD_AQL_BLOCK_FETCHER_H
#define ARANGOD_AQL_BLOCK_FETCHER_H

#include "Aql/AqlItemBlock.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionEngine.h"
#include "Aql/ExecutionState.h"
#include "Basics/Exceptions.h"

#include <memory>
#include <queue>
#include <utility>

namespace arangodb {
namespace aql {

/**
 * @brief Thin interface to access the methods of ExecutionBlock that are
 * necessary for the row Fetchers. Makes it easier to test the Fetchers.
 */
template <bool allowBlockPassthrough>
class BlockFetcher {
 public:
  /**
   * @brief Interface to fetch AqlItemBlocks from upstream with getSome.
   * @param dependencies Dependencies of the current ExecutionBlock. Must
   *                     contain EXACTLY ONE element. Otherwise, BlockFetcher
   *                     may be instantiated, but never used. It is allowed to
   *                     pass a reference to an empty vector, but as soon as
   *                     the BlockFetcher is used, the condition must be
   *                     satisfied.
   * @param itemBlockManager All blocks fetched via dependencies[0]->getSome()
   *                         will later be returned to this AqlItemBlockManager.
   * @param inputRegisters Set of registers the current ExecutionBlock is
   *                       allowed to read.
   * @param nrInputRegisters Total number of registers of the AqlItemBlocks
   *                         here. Called nrInputRegisters to discern between
   *                         the widths of input and output blocks.
   *
   * The constructor MAY NOT access the dependencies, nor the itemBlockManager.
   * This is because the dependencies will be added to the ExecutionBlock only
   * after construction, and to allow derived subclasses for testing (read
   * BlockFetcherMock) to create them *after* the parent class was constructed.
   */
  BlockFetcher(std::vector<ExecutionBlock*> const& dependencies,
               AqlItemBlockManager& itemBlockManager,
               std::shared_ptr<std::unordered_set<RegisterId> const> inputRegisters,
               RegisterId nrInputRegisters)
      : _dependencies(dependencies),
        _itemBlockManager(itemBlockManager),
        _inputRegisters(std::move(inputRegisters)),
        _nrInputRegisters(nrInputRegisters),
        _blockQueue(),
        _blockPassThroughQueue(),
        _currentDependency(0) {}

  TEST_VIRTUAL ~BlockFetcher() = default;

  // This is only TEST_VIRTUAL, so we ignore this lint warning:
  // NOLINTNEXTLINE google-default-arguments
  TEST_VIRTUAL std::pair<ExecutionState, SharedAqlItemBlockPtr> fetchBlock(
      size_t atMost = ExecutionBlock::DefaultBatchSize());

  // This fetches a block from the given dependency.
  // NOTE: It is not allowed to be used in conjunction with prefetching
  // of blocks and will work around the blockQueue
  // This is only TEST_VIRTUAL, so we ignore this lint warning:
  // NOLINTNEXTLINE google-default-arguments
  TEST_VIRTUAL std::pair<ExecutionState, SharedAqlItemBlockPtr> fetchBlockForDependency(
      size_t dependency, size_t atMost = ExecutionBlock::DefaultBatchSize());

  // TODO enable_if<allowBlockPassthrough>
  std::pair<ExecutionState, SharedAqlItemBlockPtr> fetchBlockForPassthrough(size_t atMost);

  TEST_VIRTUAL inline RegisterId getNrInputRegisters() const {
    return _nrInputRegisters;
  }

  // Tries to fetch a block from upstream and push it, wrapped, onto
  // _blockQueue. If it succeeds, it returns HASMORE (the returned state
  // regards the _blockQueue). If it doesn't it's either because
  //  - upstream returned WAITING - then so does prefetchBlock().
  //  - or upstream returned a nullptr with DONE - then so does prefetchBlock().
  ExecutionState prefetchBlock(size_t atMost = ExecutionBlock::DefaultBatchSize());

  TEST_VIRTUAL inline size_t numberDependencies() const {
    return _dependencies.size();
  }

 protected:
  inline AqlItemBlockManager& itemBlockManager() { return _itemBlockManager; }
  inline AqlItemBlockManager const& itemBlockManager() const {
    return _itemBlockManager;
  }

  inline ExecutionBlock& upstreamBlock() {
    return upstreamBlockForDependency(_currentDependency);
  }

  inline ExecutionBlock& upstreamBlockForDependency(size_t index) {
    TRI_ASSERT(_dependencies.size() > index);
    return *_dependencies[index];
  }

 private:
  inline bool advanceDependency() {
    if (_currentDependency + 1 >= _dependencies.size()) {
      return false;
    }
    _currentDependency++;
    return true;
  }

 private:
  std::vector<ExecutionBlock*> const& _dependencies;
  AqlItemBlockManager& _itemBlockManager;
  std::shared_ptr<std::unordered_set<RegisterId> const> const _inputRegisters;
  RegisterId const _nrInputRegisters;
  std::queue<std::pair<ExecutionState, SharedAqlItemBlockPtr>> _blockQueue;
  // only used in case of allowBlockPassthrough:
  std::queue<std::pair<ExecutionState, SharedAqlItemBlockPtr>> _blockPassThroughQueue;
  // only modified in case of multiple dependencies + Passthrough otherwise always 0
  size_t _currentDependency;
};

}  // namespace aql
}  // namespace arangodb

#endif  // ARANGOD_AQL_BLOCK_FETCHER_H
