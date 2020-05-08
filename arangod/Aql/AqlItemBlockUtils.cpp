////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include "AqlItemBlockUtils.h"

#include "Aql/AqlItemBlockManager.h"
#include "Aql/AqlValue.h"
#include "Aql/InputAqlItemRow.h"

using namespace arangodb;
using namespace arangodb::aql;

/// @brief concatenate multiple blocks, note that the new block now owns all
/// AqlValue pointers in the old blocks, therefore, the latter are all
/// set to nullptr, just to be sure.
SharedAqlItemBlockPtr itemBlock::concatenate(AqlItemBlockManager& manager,
                                             std::vector<SharedAqlItemBlockPtr>& blocks) {
  TRI_ASSERT(!blocks.empty());

  size_t totalSize = 0;
  RegisterCount nrRegs = 0;
  for (auto& it : blocks) {
    totalSize += it->size();
    if (nrRegs == 0) {
      nrRegs = it->getNrRegs();
    } else {
      TRI_ASSERT(it->getNrRegs() == nrRegs);
    }
  }

  TRI_ASSERT(totalSize > 0);
  TRI_ASSERT(nrRegs > 0);

  auto resultBlock = manager.requestBlock(totalSize, nrRegs);

  size_t nextFreeRow = 0;
  for (auto& inputBlock : blocks) {
    nextFreeRow = resultBlock->moveOtherBlockHere(nextFreeRow, *inputBlock);
  }

  return resultBlock;
}
