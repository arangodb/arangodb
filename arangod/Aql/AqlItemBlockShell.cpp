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

#include "AqlItemBlockShell.h"
#include "InputAqlItemRow.h"

using namespace arangodb::aql;

AqlItemBlockShell::AqlItemBlockShell(AqlItemBlockManager& manager,
                                     std::unique_ptr<AqlItemBlock> block)
    : _block(block.release(), AqlItemBlockDeleter{manager}) {
  // An AqlItemBlockShell instance is assumed to be responsible for *exactly*
  // one AqlItemBlock. _block may never be null!
  TRI_ASSERT(_block != nullptr);
}

void AqlItemBlockShell::forRowInBlock(std::function<void(InputAqlItemRow&&)> callback) {
    TRI_ASSERT(_block);
    for (std::size_t index = 0; index < block().size(); ++index) {
      callback(InputAqlItemRow{ shared_from_this(), index});
    }
}
