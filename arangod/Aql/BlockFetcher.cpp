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

#include "BlockFetcher.h"

using namespace arangodb::aql;

template <bool repositShells>
std::pair<ExecutionState, std::shared_ptr<InputAqlItemBlockShell>> BlockFetcher<repositShells>::fetchBlock() {
  ExecutionState state;
  std::unique_ptr<AqlItemBlock> block;
  std::tie(state, block) = upstreamBlock().getSome(ExecutionBlock::DefaultBatchSize());
  if (block != nullptr) {
    auto blockShell =
        std::make_shared<AqlItemBlockShell>(itemBlockManager(), std::move(block));
    if (repositShells) {
      _repositShell(blockShell);
    }
    auto inputBlockShell =
        std::make_shared<InputAqlItemBlockShell>(blockShell, _inputRegisters);
    return {state, inputBlockShell};
  } else {
    return {state, nullptr};
  }
}

template class ::arangodb::aql::BlockFetcher<true>;
template class ::arangodb::aql::BlockFetcher<false>;
