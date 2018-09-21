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
#include "Aql/AqlValue.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"

using namespace arangodb;
using namespace arangodb::aql;

OutputAqlItemRow::OutputAqlItemRow(std::unique_ptr<AqlItemBlock> block,
                                   const ExecutorInfos& executorInfos)
    : _block(std::move(block)),
      _baseIndex(0),
      _executorInfos(executorInfos),
      _inputRowCopied(false),
      _lastSourceRow{CreateInvalidInputRowHint{}},
      _numValuesWritten(0) {
  TRI_ASSERT(_block != nullptr);
  TRI_ASSERT(_block->getNrRegs() == _executorInfos.numberOfOutputRegisters());
}

void OutputAqlItemRow::setValue(RegisterId registerId,
                                InputAqlItemRow const& sourceRow,
                                AqlValue const& value) {
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
  if (!_block->getValueReference(_baseIndex, registerId).isNone()) {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION(TRI_ERROR_WROTE_OUTPUT_REGISTER_TWICE);
  }

  _block->emplaceValue(_baseIndex, registerId, value);
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

  for (auto itemId : executorInfos().registersToKeep()) {
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
      _block->copyValuesFromRow(_baseIndex, executorInfos().registersToKeep(),
                                _baseIndex - 1);
    }
  }

  _inputRowCopied = true;
  _lastSourceRow = sourceRow;
}
