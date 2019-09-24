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

OutputAqlItemRow::OutputAqlItemRow(
    SharedAqlItemBlockPtr block,
    std::shared_ptr<std::unordered_set<RegisterId> const> outputRegisters,
    std::shared_ptr<std::unordered_set<RegisterId> const> registersToKeep,
    std::shared_ptr<std::unordered_set<RegisterId> const> registersToClear,
    CopyRowBehavior copyRowBehavior)
    : _block(std::move(block)),
      _baseIndex(0),
      _lastBaseIndex(0),
      _inputRowCopied(false),
      _lastSourceRow{CreateInvalidInputRowHint{}},
      _numValuesWritten(0),
      _doNotCopyInputRow(copyRowBehavior == CopyRowBehavior::DoNotCopyInputRows),
      _outputRegisters(std::move(outputRegisters)),
      _registersToKeep(std::move(registersToKeep)),
      _registersToClear(std::move(registersToClear)),
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      _setBaseIndexNotUsed(true),
#endif
      _allowSourceRowUninitialized(false) {
  TRI_ASSERT(_block != nullptr);
}

SharedAqlItemBlockPtr OutputAqlItemRow::stealBlock() {
  // Release our hold on the block now
  SharedAqlItemBlockPtr block = std::move(_block);
  if (numRowsWritten() == 0) {
    // blocks may not be empty
    block = nullptr;
  } else {
    // numRowsWritten() returns the exact number of rows that were fully
    // written and takes into account whether the current row was written.
    block->shrink(numRowsWritten());

    if (_doNotCopyInputRow) {
      block->clearRegisters(registersToClear());
    }
  }

  return block;
}

void OutputAqlItemRow::createShadowRow(InputAqlItemRow const& sourceRow) {
  TRI_ASSERT(!_inputRowCopied);
  // A shadow row can only be created on blocks that DO NOT write additional
  // output. This assertion is not a hard requirement and could be softened, but
  // it makes the logic much easier to understand, we have a shadow-row producer
  // that does not produce anything else.
  TRI_ASSERT(numRegistersToWrite() == 0);
  // This is the hard requirement, if we decide to remove the no-write policy.
  // This has te be in place. However no-write implies this.
  TRI_ASSERT(allValuesWritten());
  TRI_ASSERT(sourceRow.isInitialized());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // We can only add shadow rows if source and this are different blocks
  TRI_ASSERT(!sourceRow.internalBlockIs(_block));
#endif
  block().makeShadowRow(_baseIndex);
  doCopyRow(sourceRow, true);
}