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

#include "DependencyProxy.h"

#include "Aql/BlocksWithClients.h"
#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"

using namespace arangodb;
using namespace arangodb::aql;

template <bool passBlocksThrough>
ExecutionState DependencyProxy<passBlocksThrough>::prefetchBlock(size_t atMost) {
  TRI_ASSERT(atMost > 0);
  ExecutionState state;
  SharedAqlItemBlockPtr block;
  do {
    // Note: upstreamBlock will return next dependency
    // if we need to loop here
    if (_distributeId.empty()) {
      std::tie(state, block) = upstreamBlock().getSome(atMost);
    } else {
      auto upstreamWithClient = dynamic_cast<BlocksWithClients*>(&upstreamBlock());
      std::tie(state, block) = upstreamWithClient->getSomeForShard(atMost, _distributeId);
    }
    TRI_IF_FAILURE("ExecutionBlock::getBlock") {
      THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    }

    if (state == ExecutionState::WAITING) {
      TRI_ASSERT(block == nullptr);
      return state;
    }

    if (block == nullptr) {
      // We're not waiting and didn't get a block, so we have to be done.
      TRI_ASSERT(state == ExecutionState::DONE);
      if (!advanceDependency()) {
        return state;
      }
    }
  } while (block == nullptr);

  // Now we definitely have a block.
  TRI_ASSERT(block != nullptr);

  if (state == ExecutionState::DONE) {
    // We need to modify the state here s.t. on next call we fetch from
    // next dependency and do not return DONE on first dependency
    if (advanceDependency()) {
      state = ExecutionState::HASMORE;
    }
  }
  if /* constexpr */ (passBlocksThrough) {
    // Reposit block for pass-through executors.
    _blockPassThroughQueue.push_back({state, block});
  }
  _blockQueue.push_back({state, std::move(block)});

  return ExecutionState::HASMORE;
}

template <bool passBlocksThrough>
std::pair<ExecutionState, SharedAqlItemBlockPtr>
// NOLINTNEXTLINE google-default-arguments
DependencyProxy<passBlocksThrough>::fetchBlock(size_t atMost) {
  if (_blockQueue.empty()) {
    ExecutionState state = prefetchBlock(atMost);
    // prefetchBlock returns HASMORE iff it pushed a block onto _blockQueue.
    // If it didn't, it got either WAITING from upstream, or DONE + nullptr.
    if (state == ExecutionState::WAITING || state == ExecutionState::DONE) {
      return {state, nullptr};
    }
    TRI_ASSERT(state == ExecutionState::HASMORE);
  }

  TRI_ASSERT(!_blockQueue.empty());

  ExecutionState state;
  SharedAqlItemBlockPtr block;
  std::tie(state, block) = _blockQueue.front();
  _blockQueue.pop_front();

  return {state, std::move(block)};
}

template <bool passBlocksThrough>
std::pair<ExecutionState, SharedAqlItemBlockPtr>
// NOLINTNEXTLINE google-default-arguments
DependencyProxy<passBlocksThrough>::fetchBlockForDependency(size_t dependency, size_t atMost) {
  TRI_ASSERT(!passBlocksThrough);
  ExecutionBlock& upstream = upstreamBlockForDependency(dependency);

  TRI_ASSERT(atMost > 0);
  ExecutionState state;
  SharedAqlItemBlockPtr block;
  if (_distributeId.empty()) {
    std::tie(state, block) = upstream.getSome(atMost);
  } else {
    auto upstreamWithClient = dynamic_cast<BlocksWithClients*>(&upstream);
    std::tie(state, block) = upstreamWithClient->getSomeForShard(atMost, _distributeId);
  }

  TRI_IF_FAILURE("ExecutionBlock::getBlock") {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
  }

  if (state == ExecutionState::WAITING) {
    TRI_ASSERT(block == nullptr);
    return {state, nullptr};
  }

  if (block == nullptr) {
    // We're not waiting and didn't get a block, so we have to be done.
    TRI_ASSERT(state == ExecutionState::DONE);
    return {state, nullptr};
  }

  // Now we definitely have a block.
  TRI_ASSERT(block != nullptr);

  return {state, block};
}

template <bool allowBlockPassthrough>
std::pair<ExecutionState, size_t> DependencyProxy<allowBlockPassthrough>::skipSome(size_t const toSkip) {
  TRI_ASSERT(_blockPassThroughQueue.empty());
  TRI_ASSERT(_blockQueue.empty());

  TRI_ASSERT(toSkip > 0);
  TRI_ASSERT(_skipped <= toSkip);
  ExecutionState state = ExecutionState::HASMORE;

  while (_skipped < toSkip) {
    size_t skippedNow;
    // Note: upstreamBlock will return next dependency
    // if we need to loop here
    TRI_ASSERT(_skipped <= toSkip);
    if (_distributeId.empty()) {
      std::tie(state, skippedNow) = upstreamBlock().skipSome(toSkip - _skipped);
    } else {
      auto upstreamWithClient = dynamic_cast<BlocksWithClients*>(&upstreamBlock());
      std::tie(state, skippedNow) =
          upstreamWithClient->skipSomeForShard(toSkip - _skipped, _distributeId);
    }

    TRI_ASSERT(skippedNow <= toSkip - _skipped);

    if (state == ExecutionState::WAITING) {
      TRI_ASSERT(skippedNow == 0);
      return {state, 0};
    }

    _skipped += skippedNow;

    // When the current dependency is done, advance.
    if (state == ExecutionState::DONE && !advanceDependency()) {
      size_t skipped = _skipped;
      _skipped = 0;
      TRI_ASSERT(skipped <= toSkip);
      return {state, skipped};
    }
  }

  size_t skipped = _skipped;
  _skipped = 0;

  TRI_ASSERT(skipped <= toSkip);
  return {state, skipped};
}

template <bool allowBlockPassthrough>
std::pair<ExecutionState, SharedAqlItemBlockPtr>
DependencyProxy<allowBlockPassthrough>::fetchBlockForPassthrough(size_t atMost) {
  TRI_ASSERT(allowBlockPassthrough);  // TODO check this with enable_if in the header already

  if (_blockPassThroughQueue.empty()) {
    ExecutionState state = prefetchBlock(atMost);
    // prefetchBlock returns HASMORE iff it pushed a block onto _blockPassThroughQueue.
    // If it didn't, it got either WAITING from upstream, or DONE + nullptr.
    if (state == ExecutionState::WAITING || state == ExecutionState::DONE) {
      return {state, nullptr};
    }
    TRI_ASSERT(state == ExecutionState::HASMORE);
  }

  TRI_ASSERT(!_blockPassThroughQueue.empty());

  ExecutionState state;
  SharedAqlItemBlockPtr block;
  std::tie(state, block) = _blockPassThroughQueue.front();
  _blockPassThroughQueue.pop_front();

  return {state, std::move(block)};
}

template <bool allowBlockPassthrough>
DependencyProxy<allowBlockPassthrough>::DependencyProxy(
    std::vector<ExecutionBlock*> const& dependencies, AqlItemBlockManager& itemBlockManager,
    std::shared_ptr<std::unordered_set<RegisterId> const> inputRegisters,
    RegisterId nrInputRegisters)
    : _dependencies(dependencies),
      _itemBlockManager(itemBlockManager),
      _inputRegisters(std::move(inputRegisters)),
      _nrInputRegisters(nrInputRegisters),
      _distributeId(),
      _blockQueue(),
      _blockPassThroughQueue(),
      _currentDependency(0),
      _skipped(0) {}

template <bool allowBlockPassthrough>
RegisterId DependencyProxy<allowBlockPassthrough>::getNrInputRegisters() const {
  return _nrInputRegisters;
}

template <bool allowBlockPassthrough>
size_t DependencyProxy<allowBlockPassthrough>::numberDependencies() const {
  return _dependencies.size();
}

template <bool allowBlockPassthrough>
void DependencyProxy<allowBlockPassthrough>::reset() {
  _blockQueue.clear();
  _blockPassThroughQueue.clear();
  _currentDependency = 0;
  // We shouldn't be in a half-skipped state when reset is called
  TRI_ASSERT(_skipped == 0);
  _skipped = 0;
}

template <bool allowBlockPassthrough>
AqlItemBlockManager& DependencyProxy<allowBlockPassthrough>::itemBlockManager() {
  return _itemBlockManager;
}

template <bool allowBlockPassthrough>
AqlItemBlockManager const& DependencyProxy<allowBlockPassthrough>::itemBlockManager() const {
  return _itemBlockManager;
}

template <bool allowBlockPassthrough>
ExecutionBlock& DependencyProxy<allowBlockPassthrough>::upstreamBlock() {
  return upstreamBlockForDependency(_currentDependency);
}

template <bool allowBlockPassthrough>
ExecutionBlock& DependencyProxy<allowBlockPassthrough>::upstreamBlockForDependency(size_t index) {
  TRI_ASSERT(_dependencies.size() > index);
  return *_dependencies[index];
}

template <bool allowBlockPassthrough>
bool DependencyProxy<allowBlockPassthrough>::advanceDependency() {
  if (_currentDependency + 1 >= _dependencies.size()) {
    return false;
  }
  _currentDependency++;
  return true;
}

template class ::arangodb::aql::DependencyProxy<true>;
template class ::arangodb::aql::DependencyProxy<false>;
