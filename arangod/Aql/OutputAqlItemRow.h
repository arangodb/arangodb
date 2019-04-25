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

#ifndef ARANGOD_AQL_OUTPUT_AQL_ITEM_ROW_H
#define ARANGOD_AQL_OUTPUT_AQL_ITEM_ROW_H

#include "Aql/InputAqlItemRow.h"
#include "Aql/types.h"
#include "Basics/Common.h"

namespace arangodb {
namespace aql {

struct AqlValue;

/**
 * @brief One row within an AqlItemBlock, for writing.
 *
 * Does not keep a reference to the data.
 * Caller needs to make sure that the underlying
 * AqlItemBlock is not going out of scope.
 */
class OutputAqlItemRow {
 public:
  // TODO Implement this behaviour via a template parameter instead?
  enum class CopyRowBehaviour { CopyInputRows, DoNotCopyInputRows };

  explicit OutputAqlItemRow(SharedAqlItemBlockPtr block,
                            std::shared_ptr<std::unordered_set<RegisterId> const> outputRegisters,
                            std::shared_ptr<std::unordered_set<RegisterId> const> registersToKeep,
                            std::shared_ptr<std::unordered_set<RegisterId> const> registersToClear,
                            CopyRowBehaviour = CopyRowBehaviour::CopyInputRows);

  OutputAqlItemRow(OutputAqlItemRow const&) = delete;
  OutputAqlItemRow& operator=(OutputAqlItemRow const&) = delete;
  OutputAqlItemRow(OutputAqlItemRow&&) = delete;
  OutputAqlItemRow& operator=(OutputAqlItemRow&&) = delete;

  // Clones the given AqlValue
  void cloneValueInto(RegisterId registerId, InputAqlItemRow const& sourceRow,
                      AqlValue const& value) {
    bool mustDestroy = true;
    AqlValue clonedValue = value.clone();
    AqlValueGuard guard{clonedValue, mustDestroy};
    moveValueInto(registerId, sourceRow, guard);
  }

  // Copies the given AqlValue. If it holds external memory, it will be
  // destroyed when the block is destroyed.
  // Note that there is no real move happening here, just a trivial copy of
  // the passed AqlValue. However, that means the output block will take
  // responsibility of possibly referenced external memory.
  void moveValueInto(RegisterId registerId, InputAqlItemRow const& sourceRow,
                     AqlValueGuard& guard) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
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
#endif

    block().setValue(_baseIndex, registerId, guard.value());
    guard.steal();
    _numValuesWritten++;
    // allValuesWritten() must be called only *after* _numValuesWritten was
    // increased.
    if (allValuesWritten()) {
      copyRow(sourceRow);
    }
  }

  // Reuses the value of the given register that has been inserted in the output
  // row before. This call cannot be used on the first row of this output block.
  // If the reusing does not work this call will return `false` caller needs to
  // react accordingly.
  bool reuseLastStoredValue(RegisterId registerId, InputAqlItemRow const& sourceRow) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    if (!isOutputRegister(registerId)) {
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION(TRI_ERROR_WROTE_IN_WRONG_REGISTER);
    }
#endif
    if (_lastBaseIndex == _baseIndex) {
      return false;
    }
    // Do not clone the value, we explicitly want recycle it.
    AqlValue ref = block().getValue(_lastBaseIndex, registerId);
    // The initial row is still responsible
    AqlValueGuard guard{ref, false};
    moveValueInto(registerId, sourceRow, guard);
    return true;
  }

  void copyRow(InputAqlItemRow const& sourceRow, bool ignoreMissing = false) {
    // While violating the following asserted states would do no harm, the
    // implementation as planned should only copy a row after all values have
    // been set, and copyRow should only be called once.
    TRI_ASSERT(!_inputRowCopied);
    TRI_ASSERT(allValuesWritten());
    if (_inputRowCopied) {
      _lastBaseIndex = _baseIndex;
      return;
    }

    // This may only be set if the input block is the same as the output block,
    // because it is passed through.
    if (_doNotCopyInputRow) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      TRI_ASSERT(sourceRow.isInitialized());
      TRI_ASSERT(sourceRow.internalBlockIs(_block));
#endif
      _inputRowCopied = true;
      _lastSourceRow = sourceRow;
      _lastBaseIndex = _baseIndex;
      return;
    }

    doCopyRow(sourceRow, ignoreMissing);
  }

  std::size_t getNrRegisters() const { return block().getNrRegs(); }

  /**
   * @brief May only be called after all output values in the current row have
   * been set, or in case there are zero output registers, after copyRow has
   * been called.
   */
  void advanceRow() {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    TRI_ASSERT(produced());
    if (!allValuesWritten()) {
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION(TRI_ERROR_WROTE_TOO_FEW_OUTPUT_REGISTERS);
    }
    if (!_inputRowCopied) {
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION(TRI_ERROR_INPUT_REGISTERS_NOT_COPIED);
    }
#endif
    _lastBaseIndex = _baseIndex++;
    _inputRowCopied = false;
    _numValuesWritten = 0;
  }

  // returns true if row was produced
  bool produced() const { return allValuesWritten() && _inputRowCopied; }

  /**
   * @brief Steal the AqlItemBlock held by the OutputAqlItemRow. The returned
   *        block will contain exactly the number of written rows. e.g., if 42
   *        rows were written, block->size() will be 42, even if the original
   *        block was larger.
   *        The block will never be empty. If no rows were written, this will
   *        return a nullptr.
   *        After stealBlock(), the OutputAqlItemRow is unusable!
   */
  SharedAqlItemBlockPtr stealBlock();

  bool isFull() const { return numRowsWritten() >= block().size(); }

  /**
   * @brief Returns the number of rows that were fully written.
   */
  size_t numRowsWritten() const noexcept {
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

  /*
   * @brief Returns the number of rows left. *Always* includes the current row,
   *        whether it was already written or not!
   *        NOTE that we later want to replace this with some "atMost" value
   *        passed from ExecutionBlockImpl.
   */
  size_t numRowsLeft() const { return block().size() - _baseIndex; }

  // Use this function with caution! We need it only for the ConstrainedSortExecutor
  void setBaseIndex(std::size_t index) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    _setBaseIndexNotUsed = false;
#endif
    _baseIndex = index;
  }
  // Use this function with caution! We need it only for the SortedCollectExecutor
  void setAllowSourceRowUninitialized() {
    _allowSourceRowUninitialized = true;
  }

  // This function can be used to restore the row's invariant.
  // After setting this value numRowsWritten() rather returns
  // the number of written rows contained in the block than
  // the number of written rows, that could potentially be more.
  void setMaxBaseIndex(std::size_t index) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
    _setBaseIndexNotUsed = true;
#endif
    _baseIndex = index;
  }

 private:

  std::unordered_set<RegisterId> const& outputRegisters() const {
    return *_outputRegisters;
  }

  std::unordered_set<RegisterId> const& registersToKeep() const {
    return *_registersToKeep;
  }

  std::unordered_set<RegisterId> const& registersToClear() const {
    return *_registersToClear;
  }

  bool isOutputRegister(RegisterId registerId) const {
    return outputRegisters().find(registerId) != outputRegisters().end();
  }

 private:
  /**
   * @brief Underlying AqlItemBlock storing the data.
   */
  SharedAqlItemBlockPtr _block;

  /**
   * @brief The offset into the AqlItemBlock. In other words, the row's index.
   */
  size_t _baseIndex;
  size_t _lastBaseIndex;

  /**
   * @brief Whether the input registers were copied from a source row.
   */
  bool _inputRowCopied;

  /**
   * @brief The last source row seen. Note that this is invalid before the first
   *        source row is seen.
   */
  InputAqlItemRow _lastSourceRow;

  /**
   * @brief Number of setValue() calls. Each entry may be written at most once,
   * so this can be used to check when all values are written.
   */
  size_t _numValuesWritten;

  /**
   * @brief Set if and only if the current ExecutionBlock passes the
   * AqlItemBlocks through.
   */
  bool const _doNotCopyInputRow;

  std::shared_ptr<std::unordered_set<RegisterId> const> _outputRegisters;
  std::shared_ptr<std::unordered_set<RegisterId> const> _registersToKeep;
  std::shared_ptr<std::unordered_set<RegisterId> const> _registersToClear;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  bool _setBaseIndexNotUsed;
#endif
  // need this special bool for allowing an empty AqlValue inside the SortedCollectExecutor
  bool _allowSourceRowUninitialized;

 private:
  size_t nextUnwrittenIndex() const noexcept { return numRowsWritten(); }

  size_t numRegistersToWrite() const { return outputRegisters().size(); }

  bool allValuesWritten() const {
    return _numValuesWritten == numRegistersToWrite();
  }

  inline AqlItemBlock const& block() const {
    TRI_ASSERT(_block != nullptr);
    return *_block;
  }

  inline AqlItemBlock& block() {
    TRI_ASSERT(_block != nullptr);
    return *_block;
  }

  void doCopyRow(InputAqlItemRow const& sourceRow, bool ignoreMissing);
};

}  // namespace aql

}  // namespace arangodb

#endif  // ARANGOD_AQL_OUTPUT_AQL_ITEM_ROW_H
