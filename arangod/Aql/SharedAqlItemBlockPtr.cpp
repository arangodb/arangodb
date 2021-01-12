////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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
/// @author Tobias Gödderz
////////////////////////////////////////////////////////////////////////////////

#include "SharedAqlItemBlockPtr.h"

#include "Aql/AqlItemBlockManager.h"

using namespace arangodb;
using namespace arangodb::aql;

/*
 * returnBlock() and itemBlockManager() cannot be moved into the header file due
 * to the circular dependency between SharedAqlItemBlockPtr and
 * AqlItemBlockManager. However, by extracting returnBlock(), at least the often
 * called part of decrRefCount() can by inlined.
 */

void SharedAqlItemBlockPtr::returnBlock() noexcept {
  itemBlockManager().returnBlock(_aqlItemBlock);
  TRI_ASSERT(_aqlItemBlock == nullptr);
}

AqlItemBlockManager& SharedAqlItemBlockPtr::itemBlockManager() const noexcept {
  return _aqlItemBlock->aqlItemBlockManager();
}
