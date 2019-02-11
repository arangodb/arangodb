#include <utility>

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

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemBlockShell.h"
#include "Aql/AqlValue.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"

using namespace arangodb;
using namespace arangodb::aql;

OutputAqlItemRow::OutputAqlItemRow(std::shared_ptr<AqlItemBlockShell> blockShell,
                                   std::shared_ptr<std::unordered_set<RegisterId> const> outputRegisters,
                                   std::shared_ptr<std::unordered_set<RegisterId> const> registersToKeep,
                                   CopyRowBehaviour copyRowBehaviour)
    : _blockShell(std::move(blockShell)),
      _baseIndex(0),
      _lastBaseIndex(0),
      _inputRowCopied(false),
      _lastSourceRow{CreateInvalidInputRowHint{}},
      _numValuesWritten(0),
      _doNotCopyInputRow(copyRowBehaviour == CopyRowBehaviour::DoNotCopyInputRows),
      _outputRegisters(std::move(outputRegisters)),
      _registersToKeep(std::move(registersToKeep)) {
  TRI_ASSERT(_blockShell != nullptr);
}

void OutputAqlItemRow::doCopyRow(const InputAqlItemRow& sourceRow, bool ignoreMissing) {
  // Note that _lastSourceRow is invalid right after construction. However, when
  // _baseIndex > 0, then we must have seen one row already.

  bool mustClone = _baseIndex == 0 || _lastSourceRow != sourceRow;

  if (mustClone) {
    for (auto itemId : registersToKeep()) {
      if (ignoreMissing && itemId >= sourceRow.getNrRegisters()) {
        continue;
      }
      auto const& value = sourceRow.getValue(itemId);
      if (!value.isEmpty()) {
        AqlValue clonedValue = value.clone();
        AqlValueGuard guard(clonedValue, true);

        TRI_IF_FAILURE("OutputAqlItemRow::copyRow") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }
        TRI_IF_FAILURE("ExecutionBlock::inheritRegisters") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        block().setValue(_baseIndex, itemId, clonedValue);
        guard.steal();
      }
    }
  } else {
    TRI_ASSERT(_baseIndex > 0);
    block().copyValuesFromRow(_baseIndex, registersToKeep(), _baseIndex - 1);
  }
  _lastBaseIndex = _baseIndex;
  _inputRowCopied = true;
  _lastSourceRow = sourceRow;
}

std::unique_ptr<AqlItemBlock> OutputAqlItemRow::stealBlock() {
  auto block = blockShell().stealBlockCompat();
  if (numRowsWritten() == 0) {
    // blocks may not be empty
    block.reset(nullptr);
  } else {
    // numRowsWritten() returns the exact number of rows that were fully
    // written and takes into account whether the current row was written.
    block->shrink(numRowsWritten());
  }
  return block;
}
