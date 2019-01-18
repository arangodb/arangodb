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
#include "Aql/AqlItemBlockShell.h"
#include "Aql/AqlValue.h"

#include <utility>

using namespace arangodb;
using namespace arangodb::aql;

InputAqlItemRow::InputAqlItemRow(CreateInvalidInputRowHint)
    : _blockShell(nullptr), _baseIndex(0) {}

InputAqlItemRow::InputAqlItemRow(
    std::shared_ptr<InputAqlItemBlockShell> blockShell, size_t baseIndex)
    : _blockShell(std::move(blockShell)), _baseIndex(baseIndex) {
  TRI_ASSERT(_blockShell != nullptr);
}

const AqlValue& InputAqlItemRow::getValue(RegisterId registerId) const {
  TRI_ASSERT(isInitialized());
  TRI_ASSERT(registerId < getNrRegisters());
  return block().getValueReference(_baseIndex, registerId);
}

bool InputAqlItemRow::operator==(InputAqlItemRow const& other) const noexcept {
  TRI_ASSERT(isInitialized());
  return this->_blockShell == other._blockShell &&
         this->_baseIndex == other._baseIndex;
}

bool InputAqlItemRow::operator!=(InputAqlItemRow const& other) const noexcept {
  TRI_ASSERT(isInitialized());
  return !(*this == other);
}

AqlItemBlock& InputAqlItemRow::block() { return _blockShell->block(); }

AqlItemBlock const& InputAqlItemRow::block() const {
  return _blockShell->block();
}

std::size_t InputAqlItemRow::getNrRegisters() const {
  return block().getNrRegs();
}
