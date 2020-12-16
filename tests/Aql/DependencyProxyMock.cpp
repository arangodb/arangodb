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
#include <Logger/LogMacros.h>

#include "gtest/gtest.h"

#include "Aql/SkipResult.h"

#include <velocypack/Options.h>

namespace arangodb::tests::aql {

using namespace arangodb::aql;

/* * * * *
 * Mocks
 * * * * */

// Note that _itemBlockManager gets passed first to the parent constructor,
// and only then gets instantiated. That is okay, however, because the
// constructor will not access it.
template <BlockPassthrough passBlocksThrough>
DependencyProxyMock<passBlocksThrough>::DependencyProxyMock(
    arangodb::ResourceMonitor& monitor,
    ::arangodb::aql::RegIdSet const& inputRegisters, ::arangodb::aql::RegisterId nrRegisters)
    : DependencyProxy<passBlocksThrough>({}, _itemBlockManager, inputRegisters,
                                         nrRegisters, &velocypack::Options::Defaults),
      _itemsToReturn(),
      _numFetchBlockCalls(0),
      _monitor(monitor),
      _itemBlockManager(_monitor, SerializationFormat::SHADOWROWS) {}

template <BlockPassthrough passBlocksThrough>
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

template <BlockPassthrough passBlocksThrough>
DependencyProxyMock<passBlocksThrough>& DependencyProxyMock<passBlocksThrough>::shouldReturn(
    ExecutionState state, SharedAqlItemBlockPtr const& block) {
  // Should only be called once on each instance
  TRI_ASSERT(_itemsToReturn.empty());

  return andThenReturn(state, block);
}

template <BlockPassthrough passBlocksThrough>
DependencyProxyMock<passBlocksThrough>& DependencyProxyMock<passBlocksThrough>::shouldReturn(
    std::pair<ExecutionState, SharedAqlItemBlockPtr> firstReturnValue) {
  // Should only be called once on each instance
  TRI_ASSERT(_itemsToReturn.empty());

  return andThenReturn(std::move(firstReturnValue));
}

template <BlockPassthrough passBlocksThrough>
DependencyProxyMock<passBlocksThrough>& DependencyProxyMock<passBlocksThrough>::shouldReturn(
    std::vector<std::pair<ExecutionState, SharedAqlItemBlockPtr>> firstReturnValues) {
  // Should only be called once on each instance
  TRI_ASSERT(_itemsToReturn.empty());

  return andThenReturn(std::move(firstReturnValues));
}

template <BlockPassthrough passBlocksThrough>
DependencyProxyMock<passBlocksThrough>& DependencyProxyMock<passBlocksThrough>::andThenReturn(
    ExecutionState state, SharedAqlItemBlockPtr const& block) {
  auto inputRegisters = std::make_shared<std::unordered_set<RegisterId>>();
  // add all registers as input
  for (RegisterId i = 0; i < this->getNrInputRegisters(); i++) {
    inputRegisters->emplace(i);
  }
  // keep the block address
  _block = block;

  return andThenReturn({state, block});
}

template <BlockPassthrough passBlocksThrough>
DependencyProxyMock<passBlocksThrough>& DependencyProxyMock<passBlocksThrough>::andThenReturn(
    std::pair<ExecutionState, SharedAqlItemBlockPtr> additionalReturnValue) {
  _itemsToReturn.push(std::move(additionalReturnValue));

  return *this;
}

template <BlockPassthrough passBlocksThrough>
DependencyProxyMock<passBlocksThrough>& DependencyProxyMock<passBlocksThrough>::andThenReturn(
    std::vector<std::pair<ExecutionState, SharedAqlItemBlockPtr>> additionalReturnValues) {
  for (auto& it : additionalReturnValues) {
    andThenReturn(std::move(it));
  }

  return *this;
}

template <BlockPassthrough passBlocksThrough>
std::tuple<ExecutionState, SkipResult, SharedAqlItemBlockPtr>
DependencyProxyMock<passBlocksThrough>::execute(AqlCallStack& stack) {
  TRI_ASSERT(_block != nullptr);
  SkipResult res{};
  return {arangodb::aql::ExecutionState::DONE, res, _block};
}

template <BlockPassthrough passBlocksThrough>
bool DependencyProxyMock<passBlocksThrough>::allBlocksFetched() const {
  return _itemsToReturn.empty();
}

template <BlockPassthrough passBlocksThrough>
size_t DependencyProxyMock<passBlocksThrough>::numFetchBlockCalls() const {
  return _numFetchBlockCalls;
}

template <BlockPassthrough passBlocksThrough>
MultiDependencyProxyMock<passBlocksThrough>::MultiDependencyProxyMock(
    arangodb::ResourceMonitor& monitor,
    RegIdSet const& inputRegisters,
    ::arangodb::aql::RegisterId nrRegisters, size_t nrDeps)
    : DependencyProxy<passBlocksThrough>({}, _itemBlockManager,
                                         inputRegisters,
                                         nrRegisters, &velocypack::Options::Defaults),
      _itemBlockManager(monitor, SerializationFormat::SHADOWROWS) {
  _dependencyMocks.reserve(nrDeps);
  for (size_t i = 0; i < nrDeps; ++i) {
    _dependencyMocks.emplace_back(std::make_unique<DependencyProxyMock<passBlocksThrough>>(monitor, inputRegisters, nrRegisters));
  }
}

template <BlockPassthrough passBlocksThrough>
std::pair<arangodb::aql::ExecutionState, SharedAqlItemBlockPtr>
MultiDependencyProxyMock<passBlocksThrough>::fetchBlockForDependency(size_t dependency,
                                                                     size_t atMost) {
  return getDependencyMock(dependency).fetchBlock(atMost);
}

template <BlockPassthrough passBlocksThrough>
bool MultiDependencyProxyMock<passBlocksThrough>::allBlocksFetched() const {
  for (auto& dep : _dependencyMocks) {
    if (!dep->allBlocksFetched()) {
      return false;
    }
  }
  return true;
}

template <BlockPassthrough passBlocksThrough>
size_t MultiDependencyProxyMock<passBlocksThrough>::numFetchBlockCalls() const {
  size_t res = 0;
  for (auto& dep : _dependencyMocks) {
    res += dep->numFetchBlockCalls();
  }
  return res;
}

}  // namespace arangodb::tests::aql

template class ::arangodb::tests::aql::DependencyProxyMock<::arangodb::aql::BlockPassthrough::Enable>;
template class ::arangodb::tests::aql::DependencyProxyMock<::arangodb::aql::BlockPassthrough::Disable>;

// Multiblock does not pass through
template class ::arangodb::tests::aql::MultiDependencyProxyMock<::arangodb::aql::BlockPassthrough::Disable>;
