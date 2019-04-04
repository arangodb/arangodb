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

#include "MultiDependencySingleRowFetcher.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/BlockFetcher.h"

using namespace arangodb;
using namespace arangodb::aql;

MultiDependencySingleRowFetcher::DependencyInfo::DependencyInfo()
    : _upstreamState{ExecutionState::HASMORE}, _rowIndex{0} {}

MultiDependencySingleRowFetcher::MultiDependencySingleRowFetcher(BlockFetcher<false>& executionBlock)
    : _blockFetcher(&executionBlock) {}

std::pair<ExecutionState, SharedAqlItemBlockPtr> MultiDependencySingleRowFetcher::fetchBlockForDependency(
    size_t dependency, size_t atMost) {
  TRI_ASSERT(!_dependencyInfos.empty());
  atMost = (std::min)(atMost, ExecutionBlock::DefaultBatchSize());
  TRI_ASSERT(dependency < _dependencyInfos.size());

  // There are still some blocks left that ask their parent even after they got
  // DONE the last time, and I don't currently have time to track them down.
  // Thus the following assert is commented out.
  // TRI_ASSERT(_upstreamState != ExecutionState::DONE);
  auto res = _blockFetcher->fetchBlockForDependency(dependency, atMost);
  auto& depInfo = _dependencyInfos[dependency];
  depInfo._upstreamState = res.first;

  return res;
}

MultiDependencySingleRowFetcher::MultiDependencySingleRowFetcher()
    : _blockFetcher(nullptr) {}

RegisterId MultiDependencySingleRowFetcher::getNrInputRegisters() const {
  return _blockFetcher->getNrInputRegisters();
}

size_t MultiDependencySingleRowFetcher::numberDependencies() {
  if (_dependencyInfos.empty()) {
    // Need to setup the dependencies, they are injected lazily.
    TRI_ASSERT(_blockFetcher->numberDependencies() > 0);
    _dependencyInfos.reserve(_blockFetcher->numberDependencies());
    for (size_t i = 0; i < _blockFetcher->numberDependencies(); ++i) {
      _dependencyInfos.emplace_back(DependencyInfo{});
    }
  }
  return _dependencyInfos.size();
}
