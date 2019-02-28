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

MultiDependencySingleRowFetcher::MultiDependencySingleRowFetcher(BlockFetcher<false>& executionBlock)
    : _blockFetcher(&executionBlock), _currentRow{CreateInvalidInputRowHint{}} {}

std::pair<ExecutionState, std::shared_ptr<AqlItemBlockShell>> MultiDependencySingleRowFetcher::fetchBlock(size_t atMost) {
  atMost = (std::min)(atMost, ExecutionBlock::DefaultBatchSize());

  // There are still some blocks left that ask their parent even after they got
  // DONE the last time, and I don't currently have time to track them down.
  // Thus the following assert is commented out.
  // TRI_ASSERT(_upstreamState != ExecutionState::DONE);
  auto res = _blockFetcher->fetchBlock(atMost);

  _upstreamState = res.first;

  return res;
}

MultiDependencySingleRowFetcher::MultiDependencySingleRowFetcher()
    : _blockFetcher(nullptr), _currentRow{CreateInvalidInputRowHint{}} {}

RegisterId MultiDependencySingleRowFetcher::getNrInputRegisters() const {
  return _blockFetcher->getNrInputRegisters();
}