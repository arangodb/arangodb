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

#include "SharedAqlItemBlockPtr.h"

#include "Aql/AqlItemBlockManager.h"

using namespace arangodb;
using namespace arangodb::aql;

void SharedAqlItemBlockPtr::decrRefCount() noexcept {
  if (_aqlItemBlock != nullptr) {
    _aqlItemBlock->decrRefCount();
    if (_aqlItemBlock->getRefCount() == 0) {
      itemBlockManager().returnBlock(_aqlItemBlock);
      TRI_ASSERT(_aqlItemBlock == nullptr);
    }
  }
}

AqlItemBlockManager& SharedAqlItemBlockPtr::itemBlockManager() const noexcept {
  return _aqlItemBlock->aqlItemBlockManager();
}
