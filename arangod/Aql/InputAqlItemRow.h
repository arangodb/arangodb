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

#include "Aql/SharedAqlItemBlockPtr.h"
#include "Aql/types.h"

#include <cstddef>
#include <unordered_set>

namespace arangodb {
namespace transaction {
class Methods;
}
namespace velocypack {
class Builder;
}
namespace aql {

class AqlItemBlock;
class AqlItemBlockManager;
struct AqlValue;

struct CreateInvalidInputRowHint {
  // Forbid creating this via `{}`
  constexpr explicit CreateInvalidInputRowHint() = default;
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
  constexpr explicit InputAqlItemRow(CreateInvalidInputRowHint)
      : _block(nullptr), _baseIndex(0) {}

  InputAqlItemRow(SharedAqlItemBlockPtr const& block, size_t baseIndex);

  InputAqlItemRow(SharedAqlItemBlockPtr&& block, size_t baseIndex) noexcept;

  /**
   * @brief Get a reference to the value of the given Variable Nr
   *
   * @param registerId The register ID of the variable to read.
   *
   * @return Reference to the AqlValue stored in that variable.
   */
  AqlValue const& getValue(RegisterId registerId) const;

  /**
   * @brief Get a reference to the value of the given Variable Nr
   *
   * @param registerId The register ID of the variable to read.
   *
   * @return The AqlValue stored in that variable. It is invalidated in source.
   */
  AqlValue stealValue(RegisterId registerId);

  RegisterCount getNrRegisters() const noexcept;

  // Note that == and != here check whether the rows are *identical*, that is,
  // the same row in the same block.
  // TODO Make this a named method
  bool operator==(InputAqlItemRow const& other) const noexcept;

  bool operator!=(InputAqlItemRow const& other) const noexcept;

  // This checks whether the rows are equivalent, in the sense that they hold
  // the same number of registers and their entry-AqlValues compare equal.
  // In maintainer mode, it also asserts that the number of registers of the
  // blocks are equal, because comparing rows of blocks with different layouts
  // does not make sense.
  // Invalid rows are considered equivalent.
  bool equates(InputAqlItemRow const& other) const noexcept;

  bool isInitialized() const noexcept;

  explicit operator bool() const noexcept;

  bool isFirstDataRowInBlock() const noexcept;

  bool isLastRowInBlock() const noexcept;

  bool blockHasMoreDataRowsAfterThis() const noexcept;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  /**
   * @brief Compare the underlying block. Only for assertions.
   */
  bool internalBlockIs(SharedAqlItemBlockPtr const& other) const;
#endif

  /**
   * @brief Clone a new ItemBlock from this row
   */
  SharedAqlItemBlockPtr cloneToBlock(AqlItemBlockManager& manager,
                                     std::unordered_set<RegisterId> const& registers,
                                     size_t newNrRegs) const;

  /// @brief toVelocyPack, transfer a single AqlItemRow to Json, the result can
  /// be used to recreate the AqlItemBlock via the Json constructor
  /// Uses the same API as an AqlItemBlock with only a single row
  void toVelocyPack(transaction::Methods* trx, arangodb::velocypack::Builder&) const;

 private:
  AqlItemBlock& block() noexcept;

  AqlItemBlock const& block() const noexcept;

 private:
  /**
   * @brief Underlying AqlItemBlock storing the data.
   */
  SharedAqlItemBlockPtr _block;

  /**
   * @brief The offset into the AqlItemBlock. In other words, the row's index.
   */
  size_t _baseIndex;
};

}  // namespace aql
}  // namespace arangodb

#endif
