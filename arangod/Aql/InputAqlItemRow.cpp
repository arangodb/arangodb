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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#include "Aql/InputAqlItemRow.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/AqlValue.h"

using namespace arangodb;
using namespace arangodb::aql;

InputAqlItemRow::InputAqlItemRow(AqlItemBlock* block, size_t baseIndex)
    : _block(block), _baseIndex(baseIndex), _regsToKeep(), _produced(true) {
  // Using this constructor we are not allowed to write
  TRI_ASSERT(block != nullptr);
}

InputAqlItemRow::InputAqlItemRow(AqlItemBlock* block, size_t baseIndex, std::unordered_set<RegisterId> const& regsToKeep)
    : _block(block)
    , _baseIndex(baseIndex)
    , _regsToKeep(regsToKeep)
    , _produced(false)
    {
      TRI_ASSERT(block != nullptr);
    }

const AqlValue& InputAqlItemRow::getValue(RegisterId variableNr) const {
  TRI_ASSERT(variableNr < getNrRegisters());
  return _block->getValueReference(_baseIndex, variableNr);
}

void InputAqlItemRow::setValue(RegisterId variableNr, InputAqlItemRow const& sourceRow, AqlValue const& value) {
  TRI_ASSERT(variableNr < getNrRegisters());
  _block->emplaceValue(_baseIndex, variableNr, value);
  copyRow(sourceRow);
}

void InputAqlItemRow::copyRow(InputAqlItemRow const& sourceRow) {
  if (_produced) {
    return;
  }

  for (auto itemId : _regsToKeep) {
    // copy entries to keep
    _block->emplaceValue(_baseIndex, itemId, sourceRow.getValue(itemId));

    // auto const& value = sourceRow.getValue(itemId);
    // if (!value.isEmpty()) {
    //   AqlValue clonedValue = value.clone();
    //   AqlValueGuard guard(clonedValue, true);

    //   TRI_IF_FAILURE("InputAqlItemRow::copyRow") {
    //     THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
    //   }

    //   _block.setValue(_baseIndex, itemId, clonedValue);
    //   guard.steal();
    // }
  }

  _produced = true;
}
