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

#include "Aql/AqlCallStack.h"
#include "Aql/BlocksWithClients.h"
#include "Aql/types.h"
#include "Basics/Exceptions.h"
#include "Basics/voc-errors.h"

using namespace arangodb;
using namespace arangodb::aql;

template <BlockPassthrough blockPassthrough>
std::tuple<ExecutionState, size_t, SharedAqlItemBlockPtr>
DependencyProxy<blockPassthrough>::execute(AqlCallStack& stack) {
  // TODO: Test this, especially if upstreamBlock is done etc.
  // We do not modify any local state here.
  return upstreamBlock().execute(stack);
}

template <BlockPassthrough blockPassthrough>
ExecutionState DependencyProxy<blockPassthrough>::prefetchBlock(size_t atMost) {
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
  if /* constexpr */ (blockPassthrough == BlockPassthrough::Enable) {
    // Reposit block for pass-through executors.
    _blockPassThroughQueue.push_back({state, block});
  }
  _blockQueue.push_back({state, std::move(block)});

  return ExecutionState::HASMORE;
}

template <BlockPassthrough blockPassthrough>
std::pair<ExecutionState, SharedAqlItemBlockPtr>
// NOLINTNEXTLINE google-default-arguments
DependencyProxy<blockPassthrough>::fetchBlock(size_t atMost) {
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

template <BlockPassthrough blockPassthrough>
std::pair<ExecutionState, SharedAqlItemBlockPtr>
// NOLINTNEXTLINE google-default-arguments
DependencyProxy<blockPassthrough>::fetchBlockForDependency(size_t dependency, size_t atMost) {
  TRI_ASSERT(blockPassthrough == BlockPassthrough::Disable);
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

template <BlockPassthrough blockPassthrough>
std::pair<ExecutionState, size_t> DependencyProxy<blockPassthrough>::skipSomeForDependency(
    size_t const dependency, size_t const atMost) {
  TRI_ASSERT(blockPassthrough == BlockPassthrough::Disable);

  TRI_ASSERT(_blockPassThroughQueue.empty());
  TRI_ASSERT(_blockQueue.empty());

  TRI_ASSERT(atMost > 0);
  TRI_ASSERT(_skipped <= atMost);

  ExecutionBlock& upstream = upstreamBlockForDependency(dependency);

  ExecutionState state = ExecutionState::HASMORE;

  while (state == ExecutionState::HASMORE && _skipped < atMost) {
    size_t skippedNow;
    TRI_ASSERT(_skipped <= atMost);
    std::tie(state, skippedNow) = upstream.skipSome(atMost - _skipped);
    if (state == ExecutionState::WAITING) {
      TRI_ASSERT(skippedNow == 0);
      return {state, 0};
    }

    _skipped += skippedNow;
    TRI_ASSERT(_skipped <= atMost);
  }
  TRI_ASSERT(state != ExecutionState::WAITING);

  size_t skipped = _skipped;
  _skipped = 0;
  TRI_ASSERT(skipped <= atMost);
  return {state, skipped};
}

template <BlockPassthrough blockPassthrough>
std::pair<ExecutionState, size_t> DependencyProxy<blockPassthrough>::skipSome(size_t const toSkip) {
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
    if (state == ExecutionState::DONE) {
      if (!advanceDependency()) {
        break;
      } else {
        state = ExecutionState::HASMORE;
      }
    }
  }

  size_t skipped = _skipped;
  _skipped = 0;

  TRI_ASSERT(skipped <= toSkip);
  return {state, skipped};
}

template <BlockPassthrough blockPassthrough>
std::pair<ExecutionState, SharedAqlItemBlockPtr> DependencyProxy<blockPassthrough>::fetchBlockForPassthrough(
    size_t atMost) {
  TRI_ASSERT(blockPassthrough == BlockPassthrough::Enable);  // TODO check this with enable_if in the header already

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

template <BlockPassthrough blockPassthrough>
DependencyProxy<blockPassthrough>::DependencyProxy(
    std::vector<ExecutionBlock*> const& dependencies, AqlItemBlockManager& itemBlockManager,
    std::shared_ptr<std::unordered_set<RegisterId> const> inputRegisters,
    RegisterId nrInputRegisters,
    velocypack::Options const* const options)
    : _dependencies(dependencies),
      _itemBlockManager(itemBlockManager),
      _inputRegisters(std::move(inputRegisters)),
      _nrInputRegisters(nrInputRegisters),
      _distributeId(),
      _blockQueue(),
      _blockPassThroughQueue(),
      _currentDependency(0),
      _skipped(0),
      _vpackOptions(options) {}

template <BlockPassthrough blockPassthrough>
RegisterId DependencyProxy<blockPassthrough>::getNrInputRegisters() const {
  return _nrInputRegisters;
}

template <BlockPassthrough blockPassthrough>
size_t DependencyProxy<blockPassthrough>::numberDependencies() const {
  return _dependencies.size();
}

template <BlockPassthrough blockPassthrough>
void DependencyProxy<blockPassthrough>::reset() {
  _blockQueue.clear();
  _blockPassThroughQueue.clear();
  _currentDependency = 0;
  // We shouldn't be in a half-skipped state when reset is called
  TRI_ASSERT(_skipped == 0);
  _skipped = 0;
}

template <BlockPassthrough blockPassthrough>
AqlItemBlockManager& DependencyProxy<blockPassthrough>::itemBlockManager() {
  return _itemBlockManager;
}

template <BlockPassthrough blockPassthrough>
AqlItemBlockManager const& DependencyProxy<blockPassthrough>::itemBlockManager() const {
  return _itemBlockManager;
}

template <BlockPassthrough blockPassthrough>
ExecutionBlock& DependencyProxy<blockPassthrough>::upstreamBlock() {
  return upstreamBlockForDependency(_currentDependency);
}

template <BlockPassthrough blockPassthrough>
ExecutionBlock& DependencyProxy<blockPassthrough>::upstreamBlockForDependency(size_t index) {
  TRI_ASSERT(_dependencies.size() > index);
  return *_dependencies[index];
}

template <BlockPassthrough blockPassthrough>
bool DependencyProxy<blockPassthrough>::advanceDependency() {
  if (_currentDependency + 1 >= _dependencies.size()) {
    return false;
  }
  _currentDependency++;
  return true;
}

template <BlockPassthrough allowBlockPassthrough>
velocypack::Options const* DependencyProxy<allowBlockPassthrough>::velocypackOptions() const
    noexcept {
  return _vpackOptions;
}

template class ::arangodb::aql::DependencyProxy<BlockPassthrough::Enable>;
template class ::arangodb::aql::DependencyProxy<BlockPassthrough::Disable>;
