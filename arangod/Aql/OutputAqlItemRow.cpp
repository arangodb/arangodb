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
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#include "OutputAqlItemRow.h"

#include "Aql/InputAqlItemRow.h"
#include "Aql/AqlItemBlock.h"
#include "Aql/AqlValue.h"

using namespace arangodb;
using namespace arangodb::aql;

OutputAqlItemRow::OutputAqlItemRow(
    std::unique_ptr<AqlItemBlock> block,
    std::unordered_set<RegisterId> const& regsToKeep)
    : _block(std::move(block)),
      _baseIndex(0),
      _regsToKeep(regsToKeep),
      _currentRowIsComplete(false),
      _lastSourceRow{CreateInvalidInputRowHint{}} {
  TRI_ASSERT(_block != nullptr);
}

void OutputAqlItemRow::setValue(RegisterId variableNr, InputAqlItemRow const& sourceRow, AqlValue const& value) {
  TRI_ASSERT(variableNr < getNrRegisters());
  _block->emplaceValue(_baseIndex, variableNr, value);
  copyRow(sourceRow);
}

void OutputAqlItemRow::copyRow(InputAqlItemRow const& sourceRow) {
  TRI_ASSERT(sourceRow.isInitialized());
  if (_currentRowIsComplete) {
    return;
  }

  // Note that _lastSourceRow is invalid right after construction. However, when
  // _baseIndex > 0, then we must have seen one row already.
  TRI_ASSERT(_baseIndex == 0 || _lastSourceRow.isInitialized());
  bool mustClone = _baseIndex == 0 || _lastSourceRow != sourceRow;

  for (auto itemId : _regsToKeep) {
    // copy entries to keep
    //_block->emplaceValue(_baseIndex, itemId, sourceRow.getValue(itemId));

    if (mustClone) {
      auto const &value = sourceRow.getValue(itemId);
      if (!value.isEmpty()) {
        AqlValue clonedValue = value.clone();
        AqlValueGuard guard(clonedValue, true);

        TRI_IF_FAILURE("OutputAqlItemRow::copyRow") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        _block->setValue(_baseIndex, itemId, clonedValue);
        guard.steal();
      }
    } else {
      TRI_ASSERT(_baseIndex > 0);
      _block->copyValuesFromRow(_baseIndex, _regsToKeep, _baseIndex-1);
    }
  }

  _currentRowIsComplete = true;
  _lastSourceRow = sourceRow;
}
