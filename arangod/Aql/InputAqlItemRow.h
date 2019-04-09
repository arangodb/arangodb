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

#ifndef ARANGOD_AQL_INPUT_AQL_ITEM_ROW_H
#define ARANGOD_AQL_INPUT_AQL_ITEM_ROW_H 1

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlItemBlockShell.h"
#include "Aql/types.h"
#include "Basics/Common.h"

namespace arangodb {
namespace aql {

class AqlItemBlock;
struct AqlValue;

struct CreateInvalidInputRowHint {
  // Forbid creating this via `{}`
  explicit CreateInvalidInputRowHint() = default;
};

/**
 * @brief One row within an AqlItemBlock, for reading.
 *
 * Does not keep a reference to the data.
 * Caller needs to make sure that the underlying
 * AqlItemBlock is not going out of scope.
 *
 * Note that this class will be copied a lot, and therefore should be small
 * and not do too complex things when copied!
 */
class InputAqlItemRow {
 public:
  // The default constructor contains an invalid item row
  explicit InputAqlItemRow(CreateInvalidInputRowHint)
      : _blockShell(nullptr), _baseIndex(0) {}

  InputAqlItemRow(
      // cppcheck-suppress passedByValue
      std::shared_ptr<AqlItemBlockShell> blockShell, size_t baseIndex)
      : _blockShell(std::move(blockShell)), _baseIndex(baseIndex) {
    TRI_ASSERT(_blockShell != nullptr);
  }

  /**
   * @brief Get a reference to the value of the given Variable Nr
   *
   * @param registerId The register ID of the variable to read.
   *
   * @return Reference to the AqlValue stored in that variable.
   */
  inline const AqlValue& getValue(RegisterId registerId) const {
    TRI_ASSERT(isInitialized());
    TRI_ASSERT(registerId < getNrRegisters());
    return block().getValueReference(_baseIndex, registerId);
  }

  /**
   * @brief Get a reference to the value of the given Variable Nr
   *
   * @param registerId The register ID of the variable to read.
   *
   * @return The AqlValue stored in that variable. It is invalidated in source.
   */
  inline AqlValue stealValue(RegisterId registerId) {
    TRI_ASSERT(isInitialized());
    TRI_ASSERT(registerId < getNrRegisters());
    AqlValue const& a = block().getValueReference(_baseIndex, registerId);
    if (!a.isEmpty() && a.requiresDestruction()) {
      // Now no one is responsible for AqlValue a
      block().steal(a);
    }
    // This cannot fail, caller needs to take immediate ownership.
    return a;
  }

  std::size_t getNrRegisters() const { return block().getNrRegs(); }

  bool operator==(InputAqlItemRow const& other) const noexcept {
    TRI_ASSERT(isInitialized());
    return this->_blockShell == other._blockShell && this->_baseIndex == other._baseIndex;
  }

  bool operator!=(InputAqlItemRow const& other) const noexcept {
    TRI_ASSERT(isInitialized());
    return !(*this == other);
  }

  bool isInitialized() const noexcept { return _blockShell != nullptr; }

  explicit operator bool() const noexcept { return isInitialized(); }

  inline bool isFirstRowInBlock() const noexcept {
    TRI_ASSERT(isInitialized());
    TRI_ASSERT(blockShell().hasBlock());
    TRI_ASSERT(_baseIndex < block().size());
    return _baseIndex == 0;
  }

  inline bool isLastRowInBlock() const noexcept {
    TRI_ASSERT(isInitialized());
    TRI_ASSERT(blockShell().hasBlock());
    TRI_ASSERT(_baseIndex < block().size());
    return _baseIndex + 1 == block().size();
  }

  inline bool blockHasMoreRows() const noexcept { return !isLastRowInBlock(); }


#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  /**
   * @brief Compare the underlying block. Only for assertions.
   */
  bool internalBlockIs(AqlItemBlockShell const& other) const;
#endif

  /**
   * @brief Clone a new ItemBlock from this row
   */
  std::unique_ptr<AqlItemBlock> cloneToBlock(AqlItemBlockManager& manager,
                                             std::unordered_set<RegisterId> const& registers,
                                             size_t newNrRegs) const;

  /// @brief toVelocyPack, transfer a single AqlItemRow to Json, the result can
  /// be used to recreate the AqlItemBlock via the Json constructor
  /// Uses the same API as an AqlItemBlock with only a single row
  void toVelocyPack(transaction::Methods* trx, arangodb::velocypack::Builder&) const;

 private:
  inline AqlItemBlockShell& blockShell() {
    TRI_ASSERT(_blockShell != nullptr);
    return *_blockShell;
  }

  inline AqlItemBlockShell const& blockShell() const {
    TRI_ASSERT(_blockShell != nullptr);
    return *_blockShell;
  }
  inline AqlItemBlock& block() { return blockShell().block(); }
  inline AqlItemBlock const& block() const { return blockShell().block(); }

 private:
  /**
   * @brief Underlying AqlItemBlock storing the data.
   */
  std::shared_ptr<AqlItemBlockShell> _blockShell;

  /**
   * @brief The offset into the AqlItemBlock. In other words, the row's index.
   */
  size_t _baseIndex;
};

}  // namespace aql

}  // namespace arangodb
#endif
