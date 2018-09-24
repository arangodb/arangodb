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

OutputAqlItemRow::OutputAqlItemRow(
    std::unique_ptr<OutputAqlItemBlockShell> blockShell)
    : _blockShell(std::move(blockShell)),
      _baseIndex(0),
      _inputRowCopied(false),
      _lastSourceRow{CreateInvalidInputRowHint{}},
      _numValuesWritten(0) {
  TRI_ASSERT(_blockShell != nullptr);
}

void OutputAqlItemRow::setValue(RegisterId registerId,
                                InputAqlItemRow const& sourceRow,
                                AqlValue const& value) {
  bool mustDestroy = true;
  AqlValue clonedValue = value.clone();
  AqlValueGuard guard{clonedValue, mustDestroy};
  // Use copy-implementation of setValue()
  // NOLINTNEXTLINE(performance-move-const-arg)
  setValue(registerId, sourceRow, std::move(clonedValue));
  guard.steal();
}

void OutputAqlItemRow::setValue(RegisterId registerId,
                                InputAqlItemRow const& sourceRow,
                                AqlValue&& value) {
  if (!isOutputRegister(registerId)) {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_WROTE_IN_WRONG_REGISTER);
  }
  // This is already implicitly asserted by isOutputRegister:
  TRI_ASSERT(registerId < getNrRegisters());
  if (_numValuesWritten >= numRegistersToWrite()) {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_WROTE_TOO_MANY_OUTPUT_REGISTERS);
  }
  if (!block().getValueReference(_baseIndex, registerId).isNone()) {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_WROTE_OUTPUT_REGISTER_TWICE);
  }

  block().setValue(_baseIndex, registerId, value);
  _numValuesWritten++;
  // allValuesWritten() must be called only *after* _numValuesWritten was
  // increased.
  if (allValuesWritten()) {
    copyRow(sourceRow);
  }
}

void OutputAqlItemRow::copyRow(InputAqlItemRow const& sourceRow) {
  TRI_ASSERT(sourceRow.isInitialized());
  // While violating the following asserted states would do no harm, the
  // implementation as planned should only copy a row after all values have been
  // set, and copyRow should only be called once.
  TRI_ASSERT(!_inputRowCopied);
  TRI_ASSERT(allValuesWritten());
  if (_inputRowCopied) {
    return;
  }

  // Note that _lastSourceRow is invalid right after construction. However, when
  // _baseIndex > 0, then we must have seen one row already.
  TRI_ASSERT(_baseIndex == 0 || _lastSourceRow.isInitialized());
  bool mustClone = _baseIndex == 0 || _lastSourceRow != sourceRow;

  if (mustClone) {
    for (auto itemId : _blockShell->registersToKeep()) {
      auto const& value = sourceRow.getValue(itemId);
      if (!value.isEmpty()) {
        AqlValue clonedValue = value.clone();
        AqlValueGuard guard(clonedValue, true);

        TRI_IF_FAILURE("OutputAqlItemRow::copyRow") {
          THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
        }

        block().setValue(_baseIndex, itemId, clonedValue);
        guard.steal();
      }
    }
  } else {
    TRI_ASSERT(_baseIndex > 0);
    block().copyValuesFromRow(_baseIndex, _blockShell->registersToKeep(),
                              _baseIndex - 1);
  }

  _inputRowCopied = true;
  _lastSourceRow = sourceRow;
}

void OutputAqlItemRow::advanceRow() {
  TRI_ASSERT(produced());
  if (!allValuesWritten()) {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_WROTE_TOO_FEW_OUTPUT_REGISTERS);
  }
  if (!_inputRowCopied) {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_INPUT_REGISTERS_NOT_COPIED);
  }
  ++_baseIndex;
  _inputRowCopied = false;
  _numValuesWritten = 0;
}

std::unique_ptr<AqlItemBlock> OutputAqlItemRow::stealBlock() {
  auto block = _blockShell->stealBlockCompat();
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

size_t OutputAqlItemRow::numRegistersToWrite() const {
  return _blockShell->outputRegisters().size();
}

bool OutputAqlItemRow::isOutputRegister(RegisterId regId) {
  return _blockShell->isOutputRegister(regId);
}

AqlItemBlock const& OutputAqlItemRow::block() const {
  return _blockShell->block();
}

AqlItemBlock& OutputAqlItemRow::block() { return _blockShell->block(); }

bool OutputAqlItemRow::isFull() { return numRowsWritten() >= block().size(); }

std::size_t OutputAqlItemRow::getNrRegisters() const { return block().getNrRegs(); }
