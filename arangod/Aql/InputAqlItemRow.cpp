////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2018-2018 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "InputAqlItemRow.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlValue.h"

using namespace arangodb;
using namespace arangodb::aql;

InputAqlItemRow::InputAqlItemRow(AqlItemBlock* block, size_t baseIndex,
                                 AqlItemBlockId blockId_)
    : _block(block), _baseIndex(baseIndex), _blockId(blockId_) {
  // Using this constructor we are not allowed to write
  TRI_ASSERT(block != nullptr);
}

const AqlValue& InputAqlItemRow::getValue(RegisterId registerId) const {
  TRI_ASSERT(registerId < getNrRegisters());
  return _block->getValueReference(_baseIndex, registerId);
}

void InputAqlItemRow::changeRow(std::size_t baseIndex) {
  _baseIndex = baseIndex;
}

void InputAqlItemRow::changeRow(AqlItemBlock* block, std::size_t baseIndex) {
  TRI_ASSERT(block != nullptr);
  _block = block;
  _baseIndex = baseIndex;
}
