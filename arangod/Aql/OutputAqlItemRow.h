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

#ifndef ARANGOD_AQL_OUTPUT_AQL_ITEM_ROW_H
#define ARANGOD_AQL_OUTPUT_AQL_ITEM_ROW_H

#include "Aql/AqlCall.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/SharedAqlItemBlockPtr.h"
#include "Aql/types.h"

#include <Containers/HashSet.h>

#include <memory>

namespace arangodb::aql {

struct AqlValue;
class InputAqlItemRow;
class ShadowAqlItemRow;

/**
 * @brief One row within an AqlItemBlock, for writing.
 *
 * Does not keep a reference to the data.
 * Caller needs to make sure that the underlying
 * AqlItemBlock is not going out of scope.
 */
class OutputAqlItemRow {
 public:
  // TODO Implement this behavior via a template parameter instead?
  enum class CopyRowBehavior { CopyInputRows, DoNotCopyInputRows };

  OutputAqlItemRow(SharedAqlItemBlockPtr block, RegIdSet const& outputRegisters,
                   RegIdFlatSetStack const& registersToKeep,
                   RegIdFlatSet const& registersToClear, AqlCall clientCall = AqlCall{},
                   CopyRowBehavior = CopyRowBehavior::CopyInputRows);

  ~OutputAqlItemRow() = default;
  OutputAqlItemRow(OutputAqlItemRow const&) = delete;
  OutputAqlItemRow& operator=(OutputAqlItemRow const&) = delete;
  OutputAqlItemRow(OutputAqlItemRow&&) = delete;
  OutputAqlItemRow& operator=(OutputAqlItemRow&&) = delete;

  bool isInitialized() const noexcept;

  // Clones the given AqlValue
  template <class ItemRowType>
  void cloneValueInto(RegisterId registerId, ItemRowType const& sourceRow,
                      AqlValue const& value);

  // Copies the given AqlValue. If it holds external memory, it will be
  // destroyed when the block is destroyed.
  // Note that there is no real move happening here, just a trivial copy of
  // the passed AqlValue. However, that means the output block will take
  // responsibility of possibly referenced external memory.
  template <class ItemRowType, class ValueType>
  void moveValueInto(RegisterId registerId, ItemRowType const& sourceRow, ValueType& value);

  // Consume the given shadow row and transform it into a InputAqlItemRow
  // for the next consumer of this block.
  // Requires that sourceRow.isRelevant() holds.
  // Also requires that we still have the next row "free" to write to.
  void consumeShadowRow(RegisterId registerId, ShadowAqlItemRow& sourceRow,
                        AqlValueGuard& guard);

  // Transform the given input AqlItemRow into a relevant ShadowRow.
  // The data of this row will be copied.
  void createShadowRow(InputAqlItemRow const& sourceRow);

  // Increase the depth of the given shadowRow. This needs to be called
  // whenever you start a nested subquery on every outer subquery shadowrow
  // The data of this row will be copied.
  void increaseShadowRowDepth(ShadowAqlItemRow& sourceRow);

  // Decrease the depth of the given shadowRow. This needs to be called
  // whenever you finish a nested subquery on every outer subquery shadowrow
  // The data of this row will be copied.
  void decreaseShadowRowDepth(ShadowAqlItemRow& sourceRow);

  // Reuses the value of the given register that has been inserted in the output
  // row before. This call cannot be used on the first row of this output block.
  // If the reusing does not work this call will return `false` caller needs to
  // react accordingly.
  bool reuseLastStoredValue(RegisterId registerId, InputAqlItemRow const& sourceRow);

  void moveRow(ShadowAqlItemRow& sourceRow, bool ignoreMissing = false);

  template <class ItemRowType>
  void copyRow(ItemRowType const& sourceRow, bool ignoreMissing = false);

  void copyBlockInternalRegister(InputAqlItemRow const& sourceRow,
                                 RegisterId input, RegisterId output);

  /**
   * @brief This function will only work if input and output
   *        have the same block.
   *        We will "copy" all rows in the input block into the output.
   *        No Registers will be modified.
   *
   * @param sourceRow The input source row.
   *                  Requirements:
   *                    - Input and Output blocks are the same
   *                    - Input and Output row position are identical
   */
  auto fastForwardAllRows(InputAqlItemRow const& sourceRow, size_t rows) -> void;

  [[nodiscard]] RegisterCount getNumRegisters() const;

  /**
   * @brief May only be called after all output values in the current row have
   * been set, or in case there are zero output registers, after copyRow has
   * been called.
   */
  void advanceRow();

  // returns true if row was produced
  [[nodiscard]] bool produced() const {
    return _inputRowCopied && allValuesWritten();
  }

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

  /**
   * @brief Test if the data-Output is full. This contains checks against
   *        the client call as well. We are considered full as soon as
   *        hard or softLimit are reached.
   */
  [[nodiscard]] bool isFull() const { return numRowsLeft() == 0; }

  /**
   * @brief Test if all allocated rows are used.
   *        this does not consider the client call and allows to use
   *        the left-over space for ShadowRows.
   */
  [[nodiscard]] bool allRowsUsed() const {
    return _block == nullptr || block().numRows() <= _baseIndex;
  }

  /**
   * @brief Returns the number of rows that were fully written.
   */
  [[nodiscard]] size_t numRowsWritten() const noexcept;

  /*
   * @brief Returns the number of rows left. *Always* includes the current row,
   *        whether it was already written or not!
   *        NOTE that we later want to replace this with some "atMost" value
   *        passed from ExecutionBlockImpl.
   */
  [[nodiscard]] size_t numRowsLeft() const {
    if (_block == nullptr) {
      return 0;
    }
    return (std::min)(block().numRows() - _baseIndex, _call.getLimit());
  }

  // Use this function with caution! We need it only for the ConstrainedSortExecutor
  void setBaseIndex(std::size_t index);
  // Use this function with caution! We need it for the SortedCollectExecutor,
  // CountCollectExecutor, and the ConstrainedSortExecutor.
  void setAllowSourceRowUninitialized() { _allowSourceRowUninitialized = true; }

  // This function can be used to restore the row's invariant.
  // After setting this value numRowsWritten() rather returns
  // the number of written rows contained in the block than
  // the number of written rows, that could potentially be more.
  void setMaxBaseIndex(std::size_t index);

  void toVelocyPack(velocypack::Options const* options, velocypack::Builder& builder);

  AqlCall::Limit softLimit() const;

  AqlCall::Limit hardLimit() const;

  AqlCall const& getClientCall() const;

  AqlCall& getModifiableClientCall();

  AqlCall&& stealClientCall();

  void setCall(AqlCall call);

 private:
  [[nodiscard]] RegIdSet const& outputRegisters() const {
    return _outputRegisters;
  }

  [[nodiscard]] RegIdFlatSetStack const& registersToKeep() const {
    return _registersToKeep;
  }

  [[nodiscard]] RegIdFlatSet const& registersToClear() const {
    return _registersToClear;
  }

  [[nodiscard]] bool isOutputRegister(RegisterId registerId) const {
    return outputRegisters().find(registerId) != outputRegisters().end();
  }

  [[nodiscard]] size_t nextUnwrittenIndex() const noexcept {
    return numRowsWritten();
  }

  [[nodiscard]] size_t numRegistersToWrite() const {
    return outputRegisters().size();
  }

  [[nodiscard]] bool allValuesWritten() const {
    // If we have a shadowRow in the output, it counts as written
    // if not it only counts is written, if we have all registers filled.
    return block().isShadowRow(_baseIndex) || _numValuesWritten == numRegistersToWrite();
  }

  [[nodiscard]] inline AqlItemBlock const& block() const {
    TRI_ASSERT(_block != nullptr);
    return *_block;
  }

  inline AqlItemBlock& block() {
    TRI_ASSERT(_block != nullptr);
    return *_block;
  }

  enum class CopyOrMove { COPY, MOVE };

  enum class AdaptRowDepth : signed int {
    DecreaseDepth = -1,
    Unchanged = 0,
    IncreaseDepth = 1
  };

  static auto constexpr depthDelta(AdaptRowDepth) -> std::underlying_type_t<AdaptRowDepth>;

  template <class ItemRowType, CopyOrMove, AdaptRowDepth = AdaptRowDepth::Unchanged>
  void copyOrMoveRow(ItemRowType& sourceRow, bool ignoreMissing);

  template <class ItemRowType, CopyOrMove, AdaptRowDepth = AdaptRowDepth::Unchanged>
  void doCopyOrMoveRow(ItemRowType& sourceRow, bool ignoreMissing);

  template <class ItemRowType, CopyOrMove>
  void doCopyOrMoveValue(ItemRowType& sourceRow, RegisterId);

  /// @brief move the value into the given output registers and count the value
  /// as written in _numValuesWritten.
  template <class ItemRowType, class ValueType>
  void moveValueWithoutRowCopy(RegisterId registerId, ValueType& value);

  template <class ItemRowType>
  void memorizeRow(ItemRowType const& sourceRow);

  template <class ItemRowType>
  bool testIfWeMustClone(ItemRowType const& sourceRow) const;

  template <class ItemRowType>
  void adjustShadowRowDepth(ItemRowType const& sourceRow);

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
   * @brief Set if and only if the current ExecutionBlock passes the
   * AqlItemBlocks through.
   */
  bool const _doNotCopyInputRow;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  bool _setBaseIndexNotUsed;
#endif
  // Need this special bool for allowing an empty AqlValue inside the
  // SortedCollectExecutor, CountCollectExecutor and ConstrainedSortExecutor.
  bool _allowSourceRowUninitialized;

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
   * @brief Call received by the client to produce this outputblock
   *        It is used for accounting of produced rows and number
   *        of rows requested by client.
   */
  AqlCall _call;

  RegIdSet const& _outputRegisters;
  RegIdFlatSetStack const& _registersToKeep;
  RegIdFlatSet const& _registersToClear;
};
}  // namespace arangodb::aql

#endif  // ARANGOD_AQL_OUTPUT_AQL_ITEM_ROW_H
