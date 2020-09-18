////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

/*
The following conditions need to hold true, we need to add c++ tests for this.

  output.isFull() == output.numRowsLeft() > 0;
  output.numRowsLeft() <= output.allocatedRows() - output.numRowsWritten();
  output.numRowsLeft() <= output.softLimit();
  output.softLimit() <= output.hardLimit();
*/

#include "OutputAqlItemRow.h"

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlValue.h"
#include "Aql/RegisterInfos.h"
#include "Aql/ShadowAqlItemRow.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Logger/LogMacros.h"

using namespace arangodb;
using namespace arangodb::aql;

OutputAqlItemRow::OutputAqlItemRow(SharedAqlItemBlockPtr block, RegIdSet const& outputRegisters,
                                   RegIdFlatSetStack const& registersToKeep,
                                   RegIdFlatSet const& registersToClear,
                                   AqlCall clientCall, CopyRowBehavior copyRowBehavior)
    : _block(std::move(block)),
      _baseIndex(0),
      _lastBaseIndex(0),
      _inputRowCopied(false),
      _doNotCopyInputRow(copyRowBehavior == CopyRowBehavior::DoNotCopyInputRows),
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      _setBaseIndexNotUsed(true),
#endif
      _allowSourceRowUninitialized(false),
      _lastSourceRow{CreateInvalidInputRowHint{}},
      _numValuesWritten(0),
      _call(clientCall),
      _outputRegisters(outputRegisters),
      _registersToKeep(registersToKeep),
      _registersToClear(registersToClear) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  if (_block != nullptr) {
    for (auto const& reg : _outputRegisters) {
      TRI_ASSERT(reg < _block->numRegisters());
    }
    // the block must have enough columns for the registers of both data rows,
    // and all the different shadow row depths
    if (_doNotCopyInputRow) {
      // pass-through case, we won't use _registersToKeep
      for (auto const& reg : _registersToClear) {
        TRI_ASSERT(reg < _block->numRegisters());
      }
    } else {
      // copying (non-pass-through) case, we won't use _registersToClear
      for (auto const& stackEntry : _registersToKeep) {
        for (auto const& reg : stackEntry) {
          TRI_ASSERT(reg < _block->numRegisters());
        }
      }
    }
  }
  // We cannot create an output row if we still have unreported skipCount
  // in the call.
  TRI_ASSERT(_call.getSkipCount() == 0);
#endif
}

bool OutputAqlItemRow::isInitialized() const noexcept {
  return _block != nullptr;
}

template <class ItemRowType>
void OutputAqlItemRow::cloneValueInto(RegisterId registerId, ItemRowType const& sourceRow,
                                      AqlValue const& value) {
  bool mustDestroy = true;
  AqlValue clonedValue = value.clone();
  AqlValueGuard guard{clonedValue, mustDestroy};
  moveValueInto(registerId, sourceRow, guard);
}

template <class ItemRowType, class ValueType>
void OutputAqlItemRow::moveValueWithoutRowCopy(RegisterId registerId, ValueType& value) {
  TRI_ASSERT(isOutputRegister(registerId));
  // This is already implicitly asserted by isOutputRegister:
  TRI_ASSERT(registerId < getNumRegisters());
  TRI_ASSERT(_numValuesWritten < numRegistersToWrite());
  TRI_ASSERT(block().getValueReference(_baseIndex, registerId).isNone());

  if constexpr (std::is_same_v<std::decay_t<ValueType>, AqlValueGuard>) {
    block().setValue(_baseIndex, registerId, value.value());
    value.steal();
  } else {
    static_assert(std::is_same_v<std::decay_t<ValueType>, VPackSlice>);
    block().emplaceValue(_baseIndex, registerId, value);
  }
  _numValuesWritten++;
}

template <class ItemRowType, class ValueType>
void OutputAqlItemRow::moveValueInto(RegisterId registerId,
                                     ItemRowType const& sourceRow, ValueType& value) {
  moveValueWithoutRowCopy<ItemRowType, ValueType>(registerId, value);

  // allValuesWritten() must be called only *after* moveValueWithoutRowCopy(),
  // because it increases _numValuesWritten.
  if (allValuesWritten()) {
    copyRow(sourceRow);
  }
}

void OutputAqlItemRow::consumeShadowRow(RegisterId registerId, ShadowAqlItemRow& sourceRow,
                                        AqlValueGuard& guard) {
  TRI_ASSERT(sourceRow.isRelevant());

  moveValueWithoutRowCopy<ShadowAqlItemRow>(registerId, guard);
  TRI_ASSERT(allValuesWritten());
  copyOrMoveRow<ShadowAqlItemRow, CopyOrMove::MOVE, AdaptRowDepth::DecreaseDepth>(sourceRow, false);
  TRI_ASSERT(produced());
  block().makeDataRow(_baseIndex);
}

bool OutputAqlItemRow::reuseLastStoredValue(RegisterId registerId,
                                            InputAqlItemRow const& sourceRow) {
  TRI_ASSERT(isOutputRegister(registerId));
  if (_lastBaseIndex == _baseIndex) {
    return false;
  }

  // Do not clone the value, we explicitly want to recycle it.
  AqlValue ref = block().getValue(_lastBaseIndex, registerId);
  // The initial row is still responsible
  AqlValueGuard guard{ref, false};
  moveValueInto(registerId, sourceRow, guard);
  return true;
}

void OutputAqlItemRow::moveRow(ShadowAqlItemRow& sourceRow, bool ignoreMissing) {
  copyOrMoveRow<ShadowAqlItemRow, CopyOrMove::MOVE>(sourceRow, ignoreMissing);
}

template <class ItemRowType>
void OutputAqlItemRow::copyRow(ItemRowType const& sourceRow, bool ignoreMissing) {
  copyOrMoveRow<ItemRowType const, CopyOrMove::COPY>(sourceRow, ignoreMissing);
}

template <class ItemRowType, OutputAqlItemRow::CopyOrMove copyOrMove, OutputAqlItemRow::AdaptRowDepth adaptRowDepth>
void OutputAqlItemRow::copyOrMoveRow(ItemRowType& sourceRow, bool ignoreMissing) {
  // While violating the following asserted states would do no harm, the
  // implementation as planned should only copy a row after all values have
  // been set, and copyRow should only be called once.
  TRI_ASSERT(!_inputRowCopied);
  // We either have a shadowRow, or we need to have all values written
  TRI_ASSERT((std::is_same_v<ItemRowType, ShadowAqlItemRow>) || allValuesWritten());
  if (_inputRowCopied) {
    _lastBaseIndex = _baseIndex;
    return;
  }

  // This may only be set if the input block is the same as the output block,
  // because it is passed through.
  if (_doNotCopyInputRow) {
    TRI_ASSERT(sourceRow.isInitialized());
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(sourceRow.internalBlockIs(_block, _baseIndex));
#endif
    _inputRowCopied = true;
    memorizeRow(sourceRow);
    return;
  }

  doCopyOrMoveRow<ItemRowType, copyOrMove, adaptRowDepth>(sourceRow, ignoreMissing);
}

auto OutputAqlItemRow::fastForwardAllRows(InputAqlItemRow const& sourceRow, size_t rows)
    -> void {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(sourceRow.internalBlockIs(_block, _baseIndex));
#endif
  TRI_ASSERT(_doNotCopyInputRow);
  TRI_ASSERT(_call.getLimit() >= rows);
  TRI_ASSERT(rows > 0);
  // We have the guarantee that we have all data in our block.
  // We only need to adjust internal indexes.
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  // Safely assert that the API is not missused.
  TRI_ASSERT(_baseIndex + rows <= _block->numRows());
  for (size_t i = _baseIndex; i < _baseIndex + rows; ++i) {
    TRI_ASSERT(!_block->isShadowRow(i));
  }
#endif
  _baseIndex += rows;
  TRI_ASSERT(_baseIndex > 0);
  _lastBaseIndex = _baseIndex - 1;
  _lastSourceRow = InputAqlItemRow{_block, _lastBaseIndex};
  _call.didProduce(rows);
}

void OutputAqlItemRow::copyBlockInternalRegister(InputAqlItemRow const& sourceRow,
                                                 RegisterId input, RegisterId output) {
  // This method is only allowed if the block of the input row is the same as
  // the block of the output row!
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(sourceRow.internalBlockIs(_block, _baseIndex));
#endif
  TRI_ASSERT(isOutputRegister(output));
  // This is already implicitly asserted by isOutputRegister:
  TRI_ASSERT(output < getNumRegisters());
  TRI_ASSERT(_numValuesWritten < numRegistersToWrite());
  TRI_ASSERT(block().getValueReference(_baseIndex, output).isNone());

  AqlValue const& value = sourceRow.getValue(input);

  block().setValue(_baseIndex, output, value);
  _numValuesWritten++;
  // allValuesWritten() must be called only *after* _numValuesWritten was
  // increased.
  if (allValuesWritten()) {
    copyRow(sourceRow);
  }
}

size_t OutputAqlItemRow::numRowsWritten() const noexcept {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  TRI_ASSERT(_setBaseIndexNotUsed);
#endif
  // If the current line was fully written, the number of fully written rows
  // is the index plus one.
  if (produced()) {
    return _baseIndex + 1;
  }

  // If the current line was not fully written, the last one was, so the
  // number of fully written rows is (_baseIndex - 1) + 1.
  return _baseIndex;

  // Disregarding unsignedness, we could also write:
  //   lastWrittenIndex = produced()
  //     ? _baseIndex
  //     : _baseIndex - 1;
  //   return lastWrittenIndex + 1;
}

void OutputAqlItemRow::advanceRow() {
  TRI_ASSERT(produced());
  TRI_ASSERT(allValuesWritten());
  TRI_ASSERT(_inputRowCopied);
  if (!_block->isShadowRow(_baseIndex)) {
    // We have written a data row into the output.
    // Need to count it.
    _call.didProduce(1);
  }

  ++_baseIndex;
  _inputRowCopied = false;
  _numValuesWritten = 0;
}

void OutputAqlItemRow::toVelocyPack(velocypack::Options const* options, VPackBuilder& builder) {
  TRI_ASSERT(produced());
  block().rowToSimpleVPack(_baseIndex, options, builder);
}

AqlCall::Limit OutputAqlItemRow::softLimit() const { return _call.softLimit; }

AqlCall::Limit OutputAqlItemRow::hardLimit() const { return _call.hardLimit; }

AqlCall const& OutputAqlItemRow::getClientCall() const { return _call; }

AqlCall& OutputAqlItemRow::getModifiableClientCall() { return _call; }

AqlCall&& OutputAqlItemRow::stealClientCall() { return std::move(_call); }

void OutputAqlItemRow::setCall(AqlCall call) {
  // We cannot create an output row if we still have unreported skipCount
  // in the call.
  TRI_ASSERT(_call.getSkipCount() == 0);
  _call = std::move(call);
}

SharedAqlItemBlockPtr OutputAqlItemRow::stealBlock() {
  // numRowsWritten() inspects _block, so save this before resetting it!
  auto const numRows = numRowsWritten();
  // Release our hold on the block now
  SharedAqlItemBlockPtr block = std::move(_block);
  if (numRows == 0) {
    // blocks may not be empty
    block = nullptr;
  } else {
    // numRowsWritten() returns the exact number of rows that were fully
    // written and takes into account whether the current row was written.
    block->shrink(numRows);

    if (!registersToClear().empty()) {
      block->clearRegisters(registersToClear());
    }
  }

  return block;
}

void OutputAqlItemRow::setBaseIndex(std::size_t index) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  _setBaseIndexNotUsed = false;
#endif
  _baseIndex = index;
}

void OutputAqlItemRow::setMaxBaseIndex(std::size_t index) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  _setBaseIndexNotUsed = true;
#endif
  _baseIndex = index;
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
  TRI_ASSERT(!sourceRow.internalBlockIs(_block, _baseIndex));
#endif
  block().makeShadowRow(_baseIndex, 0); 
  doCopyOrMoveRow<InputAqlItemRow const, CopyOrMove::COPY, AdaptRowDepth::IncreaseDepth>(sourceRow, true);
}

void OutputAqlItemRow::increaseShadowRowDepth(ShadowAqlItemRow& sourceRow) {
  size_t newDepth = sourceRow.getDepth() + 1;
  doCopyOrMoveRow<ShadowAqlItemRow, CopyOrMove::MOVE, AdaptRowDepth::IncreaseDepth>(sourceRow, false);
  block().makeShadowRow(_baseIndex, newDepth);
  // We need to fake produced state
  _numValuesWritten = numRegistersToWrite();
  TRI_ASSERT(produced());
}

void OutputAqlItemRow::decreaseShadowRowDepth(ShadowAqlItemRow& sourceRow) {
  doCopyOrMoveRow<ShadowAqlItemRow, CopyOrMove::MOVE, AdaptRowDepth::DecreaseDepth>(sourceRow, false);
  TRI_ASSERT(!sourceRow.isRelevant());
  TRI_ASSERT(sourceRow.getDepth() > 0);
  block().makeShadowRow(_baseIndex, sourceRow.getDepth() - 1);
  // We need to fake produced state
  _numValuesWritten = numRegistersToWrite();
  TRI_ASSERT(produced());
}

template <>
void OutputAqlItemRow::memorizeRow<InputAqlItemRow>(InputAqlItemRow const& sourceRow) {
  _lastSourceRow = sourceRow;
  _lastBaseIndex = _baseIndex;
}

template <>
void OutputAqlItemRow::memorizeRow<ShadowAqlItemRow>(ShadowAqlItemRow const& sourceRow) {}

template <>
bool OutputAqlItemRow::testIfWeMustClone<InputAqlItemRow>(InputAqlItemRow const& sourceRow) const {
  return _baseIndex == 0 || !_lastSourceRow.isSameBlockAndIndex(sourceRow);
}

template <>
bool OutputAqlItemRow::testIfWeMustClone<ShadowAqlItemRow>(ShadowAqlItemRow const& sourceRow) const {
  return true;
}

template <>
void OutputAqlItemRow::adjustShadowRowDepth<InputAqlItemRow>(InputAqlItemRow const& sourceRow) {
}

template <>
void OutputAqlItemRow::adjustShadowRowDepth<ShadowAqlItemRow>(ShadowAqlItemRow const& sourceRow) {
  block().makeShadowRow(_baseIndex, sourceRow.getShadowDepthValue());
}

template <class T>
struct dependent_false : std::false_type {};

template <class ItemRowType, OutputAqlItemRow::CopyOrMove copyOrMove, OutputAqlItemRow::AdaptRowDepth adaptRowDepth>
void OutputAqlItemRow::doCopyOrMoveRow(ItemRowType& sourceRow, bool ignoreMissing) {
  // Note that _lastSourceRow is invalid right after construction. However, when
  // _baseIndex > 0, then we must have seen one row already.
  TRI_ASSERT(!_doNotCopyInputRow);
  bool constexpr createShadowRow = adaptRowDepth == AdaptRowDepth::IncreaseDepth;
  bool mustClone = testIfWeMustClone(sourceRow) || createShadowRow;

  // Indexes (reversely) which entry in the registersToKeep() stack we need to
  // use.
  // Usually, for all InputAqlItemRows, it is zero, respectively the top of the
  // stack or the back of the vector.
  // Similarly, for all ShadowAqlItemRows, it is their respective shadow row
  // depth plus one.
  // The exceptions are SubqueryStart and SubqueryEnd nodes, where the depth
  // of all rows increases or decreases, respectively. In these cases, we have
  // to adapt the depth by plus one or minus one, respectively.
  static bool constexpr isShadowRow =
      std::is_same_v<std::decay_t<ItemRowType>, ShadowAqlItemRow>;
  size_t baseRowDepth = 0;
  if constexpr (isShadowRow) {
    baseRowDepth = sourceRow.getDepth() + 1;
  }
  auto constexpr delta = depthDelta(adaptRowDepth);
  size_t const rowDepth = baseRowDepth + delta;
    
  auto const roffset = rowDepth + 1;
  TRI_ASSERT(roffset <= registersToKeep().size());
  auto idx = registersToKeep().size() - roffset;
  auto const& regsToKeep = registersToKeep().at(idx);

  if (mustClone) {
    for (auto const& itemId : regsToKeep) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      if (!_allowSourceRowUninitialized) {
        TRI_ASSERT(sourceRow.isInitialized());
      }
#endif
      if (ignoreMissing && itemId >= sourceRow.getNumRegisters()) {
        continue;
      }
      if (ADB_LIKELY(!_allowSourceRowUninitialized || sourceRow.isInitialized())) {
        doCopyOrMoveValue<ItemRowType, copyOrMove>(sourceRow, itemId);
      }
    }
    adjustShadowRowDepth(sourceRow);
  } else {
    TRI_ASSERT(_baseIndex > 0);
    if (ADB_LIKELY(!_allowSourceRowUninitialized || sourceRow.isInitialized())) {
      block().referenceValuesFromRow(_baseIndex, regsToKeep, _lastBaseIndex);
    }
  }

  _inputRowCopied = true;
  memorizeRow(sourceRow);
}

template <class ItemRowType, OutputAqlItemRow::CopyOrMove copyOrMove>
void OutputAqlItemRow::doCopyOrMoveValue(ItemRowType& sourceRow, RegisterId itemId) {
  if constexpr (copyOrMove == CopyOrMove::COPY) {
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
  } else if constexpr (copyOrMove == CopyOrMove::MOVE) {
    // This is only compiled for ShadowRows
    auto stolenValue = sourceRow.stealAndEraseValue(itemId);
    if (!stolenValue.isEmpty()) {
      AqlValueGuard guard(stolenValue, true);

      TRI_IF_FAILURE("OutputAqlItemRow::copyRow") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }
      TRI_IF_FAILURE("ExecutionBlock::inheritRegisters") {
        THROW_ARANGO_EXCEPTION(TRI_ERROR_DEBUG);
      }

      block().setValue(_baseIndex, itemId, stolenValue);
      guard.steal();
    }
  } else {
    static_assert(dependent_false<ItemRowType>::value, "Unhandled value");
  }
}

auto constexpr OutputAqlItemRow::depthDelta(AdaptRowDepth adaptRowDepth)
    -> std::underlying_type_t<OutputAqlItemRow::AdaptRowDepth> {
  return static_cast<std::underlying_type_t<AdaptRowDepth>>(adaptRowDepth);
}

RegisterCount OutputAqlItemRow::getNumRegisters() const {
  return block().numRegisters();
}

template void OutputAqlItemRow::copyRow<InputAqlItemRow>(InputAqlItemRow const& sourceRow,
                                                         bool ignoreMissing);
template void OutputAqlItemRow::copyRow<ShadowAqlItemRow>(ShadowAqlItemRow const& sourceRow,
                                                          bool ignoreMissing);
template void OutputAqlItemRow::cloneValueInto<InputAqlItemRow>(
    RegisterId registerId, const InputAqlItemRow& sourceRow, AqlValue const& value);
template void OutputAqlItemRow::cloneValueInto<ShadowAqlItemRow>(
    RegisterId registerId, const ShadowAqlItemRow& sourceRow, AqlValue const& value);
template void OutputAqlItemRow::moveValueInto<InputAqlItemRow, AqlValueGuard>(
    RegisterId registerId, InputAqlItemRow const& sourceRow, AqlValueGuard& guard);
template void OutputAqlItemRow::moveValueInto<ShadowAqlItemRow, AqlValueGuard>(
    RegisterId registerId, ShadowAqlItemRow const& sourceRow, AqlValueGuard& guard);

template void OutputAqlItemRow::moveValueInto<InputAqlItemRow, VPackSlice const>(
    RegisterId registerId, InputAqlItemRow const& sourceRow, VPackSlice const& guard);
template void OutputAqlItemRow::moveValueInto<ShadowAqlItemRow, VPackSlice const>(
    RegisterId registerId, ShadowAqlItemRow const& sourceRow, VPackSlice const& guard);
