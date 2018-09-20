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

#include "Aql/types.h"
#include "Basics/Common.h"

namespace arangodb {
namespace aql {

struct AqlValue;

struct CreateInvalidInputRowHint{
  // Forbid creating this via `{}`
  explicit CreateInvalidInputRowHint() = default;
};

/**
 * @brief One row within an AqlItemBlock, for reading.
 *
 * Does not keep a reference to the data.
 * Caller needs to make sure that the underlying
 * AqlItemBlock is not going out of scope.
 */
class InputAqlItemRow {
 public:
  /**
  * @brief ID type for AqlItemBlocks. Positive values are allowed, -1 means
  *        invalid/uninitialized.
  */
  using AqlItemBlockId = int64_t;

  // The default constructor contains an invalid item row
  explicit InputAqlItemRow(CreateInvalidInputRowHint);

  InputAqlItemRow(AqlItemBlock* block, size_t baseIndex,
                  AqlItemBlockId blockId);

  /**
   * @brief Get a reference to the value of the given Variable Nr
   *
   * @param registerId The register ID of the variable to read.
   *
   * @return Reference to the AqlValue stored in that variable.
   */
  const AqlValue& getValue(RegisterId registerId) const;

  std::size_t getNrRegisters() const { return _block->getNrRegs(); }

  bool operator==(InputAqlItemRow const& other) const noexcept;
  bool operator!=(InputAqlItemRow const& other) const noexcept;

  bool isInitialized() const noexcept {
    return _block != nullptr && _blockId >= 0;
  }

  explicit operator bool() const noexcept { return isInitialized(); }

 private:
  /**
   * @brief Underlying AqlItemBlock storing the data.
   */
  AqlItemBlock* _block;

  /**
   * @brief The offset into the AqlItemBlock. In other words, the row's index.
   */
  size_t _baseIndex;

  /**
   * @brief Block ID. Is assumed to biuniquely identify an AqlItemBlock. The
   *        Fetcher is responsible for assigning these.
   *        It may not be set to a negative value from the outside.
   */
  AqlItemBlockId _blockId;
};

// If you want to relax this requirement, make sure its necessary and that it
// doesn't affect the performance.
static_assert(
    std::is_trivially_copyable<InputAqlItemRow>(),
    "InputAqlItemRow is created and copied very often, so keep fast to copy.");

}  // namespace aql
}  // namespace arangodb
#endif
