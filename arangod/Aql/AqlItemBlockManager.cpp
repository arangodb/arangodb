////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
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
/// @author Jan Steemann
////////////////////////////////////////////////////////////////////////////////

#include "AqlItemBlockManager.h"
#include "Aql/AqlItemBlock.h"

using namespace arangodb::aql;

/// @brief create the manager
AqlItemBlockManager::AqlItemBlockManager() : _last(nullptr) {}

/// @brief destroy the manager
AqlItemBlockManager::~AqlItemBlockManager() { delete _last; }

/// @brief request a block with the specified size
AqlItemBlock* AqlItemBlockManager::requestBlock(size_t nrItems,
                                                RegisterId nrRegs) {
  if (_last != nullptr && _last->size() == nrItems &&
      _last->getNrRegs() == nrRegs) {
    auto block = _last;
    // don't hand out the same block next time
    _last = nullptr;
    block->eraseAll();

    return block;
  }

  return new AqlItemBlock(nrItems, nrRegs);
}

/// @brief return a block to the manager
void AqlItemBlockManager::returnBlock(AqlItemBlock*& block) {
  TRI_ASSERT(block != nullptr);
  block->destroy();

  delete _last;
  _last = block;
  block = nullptr;
}
