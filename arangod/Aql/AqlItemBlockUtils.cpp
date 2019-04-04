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
#include "Aql/BlockCollector.h"
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
  RegisterId nrRegs = 0;
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

  auto res = manager.requestBlock(totalSize, nrRegs);

  size_t pos = 0;
  for (auto& it : blocks) {
    size_t const n = it->size();
    for (size_t row = 0; row < n; ++row) {
      for (RegisterId col = 0; col < nrRegs; ++col) {
        // copy over value
        AqlValue const& a = it->getValueReference(row, col);
        if (!a.isEmpty()) {
          res->setValue(pos + row, col, a);
        }
      }
    }
    it->eraseAll();
    pos += n;
  }

  return res;
}

void itemBlock::forRowInBlock(SharedAqlItemBlockPtr const& block,
                              std::function<void(InputAqlItemRow&&)> const& callback) {
  TRI_ASSERT(block != nullptr);
  for (std::size_t index = 0; index < block->size(); ++index) {
    callback(InputAqlItemRow{block, index});
  }
}
