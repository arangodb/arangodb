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

#include "DependencyProxyMock.h"

#include "gtest/gtest.h"

namespace arangodb {
namespace tests {
namespace aql {

using namespace arangodb::aql;

/* * * * *
 * Mocks
 * * * * */

// Note that _itemBlockManager gets passed first to the parent constructor,
// and only then gets instantiated. That is okay, however, because the
// constructor will not access it.
template <bool passBlocksThrough>
DependencyProxyMock<passBlocksThrough>::DependencyProxyMock(arangodb::aql::ResourceMonitor& monitor,
                                                            ::arangodb::aql::RegisterId nrRegisters)
    : DependencyProxy<passBlocksThrough>({}, _itemBlockManager,
                                         std::shared_ptr<std::unordered_set<RegisterId>>(),
                                         nrRegisters),
      _itemsToReturn(),
      _numFetchBlockCalls(0),
      _monitor(monitor),
      _itemBlockManager(&_monitor, SerializationFormat::SHADOWROWS) {}

template <bool passBlocksThrough>
std::pair<ExecutionState, SharedAqlItemBlockPtr>
// NOLINTNEXTLINE google-default-arguments
DependencyProxyMock<passBlocksThrough>::fetchBlock(size_t) {
  _numFetchBlockCalls++;

  if (_itemsToReturn.empty()) {
    return {ExecutionState::DONE, nullptr};
  }

  std::pair<ExecutionState, SharedAqlItemBlockPtr> returnValue =
      std::move(_itemsToReturn.front());
  _itemsToReturn.pop();

  return returnValue;
}

/* * * * * * * * * * * * *
 * Test helper functions
 * * * * * * * * * * * * */

template <bool passBlocksThrough>
DependencyProxyMock<passBlocksThrough>& DependencyProxyMock<passBlocksThrough>::shouldReturn(
    ExecutionState state, SharedAqlItemBlockPtr const& block) {
  // Should only be called once on each instance
  TRI_ASSERT(_itemsToReturn.empty());

  return andThenReturn(state, block);
}

template <bool passBlocksThrough>
DependencyProxyMock<passBlocksThrough>& DependencyProxyMock<passBlocksThrough>::shouldReturn(
    std::pair<ExecutionState, SharedAqlItemBlockPtr> firstReturnValue) {
  // Should only be called once on each instance
  TRI_ASSERT(_itemsToReturn.empty());

  return andThenReturn(std::move(firstReturnValue));
}

template <bool passBlocksThrough>
DependencyProxyMock<passBlocksThrough>& DependencyProxyMock<passBlocksThrough>::shouldReturn(
    std::vector<std::pair<ExecutionState, SharedAqlItemBlockPtr>> firstReturnValues) {
  // Should only be called once on each instance
  TRI_ASSERT(_itemsToReturn.empty());

  return andThenReturn(std::move(firstReturnValues));
}

template <bool passBlocksThrough>
DependencyProxyMock<passBlocksThrough>& DependencyProxyMock<passBlocksThrough>::andThenReturn(
    ExecutionState state, SharedAqlItemBlockPtr const& block) {
  auto inputRegisters = std::make_shared<std::unordered_set<RegisterId>>();
  // add all registers as input
  for (RegisterId i = 0; i < this->getNrInputRegisters(); i++) {
    inputRegisters->emplace(i);
  }
  return andThenReturn({state, block});
}

template <bool passBlocksThrough>
DependencyProxyMock<passBlocksThrough>& DependencyProxyMock<passBlocksThrough>::andThenReturn(
    std::pair<ExecutionState, SharedAqlItemBlockPtr> additionalReturnValue) {
  _itemsToReturn.push(std::move(additionalReturnValue));

  return *this;
}

template <bool passBlocksThrough>
DependencyProxyMock<passBlocksThrough>& DependencyProxyMock<passBlocksThrough>::andThenReturn(
    std::vector<std::pair<ExecutionState, SharedAqlItemBlockPtr>> additionalReturnValues) {
  for (auto& it : additionalReturnValues) {
    andThenReturn(std::move(it));
  }

  return *this;
}

template <bool passBlocksThrough>
bool DependencyProxyMock<passBlocksThrough>::allBlocksFetched() const {
  return _itemsToReturn.empty();
}

template <bool passBlocksThrough>
size_t DependencyProxyMock<passBlocksThrough>::numFetchBlockCalls() const {
  return _numFetchBlockCalls;
}

template <bool passBlocksThrough>
std::pair<ExecutionState, size_t> DependencyProxyMock<passBlocksThrough>::skipSome(size_t atMost) {
  ExecutionState state;
  SharedAqlItemBlockPtr block;

  std::tie(state, block) = _itemsToReturn.front();

  if (block == nullptr) {
    return {ExecutionState::DONE, 0};
  }

  size_t const firstShadowRow = [&]() {
    size_t row;
    for (row = 0; row < block->size(); ++row) {
      if (block->isShadowRow(row)) break;
    }
    return row;
  }();
  atMost = (std::min)(firstShadowRow, atMost);

  if (block->size() <= atMost) {
    // Return the whole block
    std::tie(state, block) = fetchBlock(atMost);
    return {state, block->size()};
  }
  TRI_ASSERT(block != nullptr);
  TRI_ASSERT(block->size() > atMost);
  SharedAqlItemBlockPtr rest = block->slice(atMost, block->size());
  _itemsToReturn.front().second = rest;

  return {ExecutionState::HASMORE, atMost};
}

template <bool passBlocksThrough>
MultiDependencyProxyMock<passBlocksThrough>::MultiDependencyProxyMock(
    arangodb::aql::ResourceMonitor& monitor,
    ::arangodb::aql::RegisterId nrRegisters, size_t nrDeps)
    : DependencyProxy<passBlocksThrough>({}, _itemBlockManager,
                                         std::shared_ptr<std::unordered_set<RegisterId>>(),
                                         nrRegisters),
      _itemBlockManager(&monitor, SerializationFormat::SHADOWROWS) {
  _dependencyMocks.reserve(nrDeps);
  for (size_t i = 0; i < nrDeps; ++i) {
    _dependencyMocks.emplace_back(DependencyProxyMock<passBlocksThrough>{monitor, nrRegisters});
  }
}

template <bool passBlocksThrough>
std::pair<arangodb::aql::ExecutionState, SharedAqlItemBlockPtr>
MultiDependencyProxyMock<passBlocksThrough>::fetchBlockForDependency(size_t dependency,
                                                                     size_t atMost) {
  return getDependencyMock(dependency).fetchBlock(atMost);
}

template <bool passBlocksThrough>
std::pair<arangodb::aql::ExecutionState, size_t> MultiDependencyProxyMock<passBlocksThrough>::skipSomeForDependency(
    size_t dependency, size_t atMost) {
  return getDependencyMock(dependency).skipSome(atMost);
}

template <bool passBlocksThrough>
bool MultiDependencyProxyMock<passBlocksThrough>::allBlocksFetched() const {
  for (auto& dep : _dependencyMocks) {
    if (!dep.allBlocksFetched()) {
      return false;
    }
  }
  return true;
}

template <bool passBlocksThrough>
size_t MultiDependencyProxyMock<passBlocksThrough>::numFetchBlockCalls() const {
  size_t res = 0;
  for (auto& dep : _dependencyMocks) {
    res += dep.numFetchBlockCalls();
  }
  return res;
}

}  // namespace aql
}  // namespace tests
}  // namespace arangodb

template class ::arangodb::tests::aql::DependencyProxyMock<true>;
template class ::arangodb::tests::aql::DependencyProxyMock<false>;

// Multiblock does not pass through
template class ::arangodb::tests::aql::MultiDependencyProxyMock<false>;
