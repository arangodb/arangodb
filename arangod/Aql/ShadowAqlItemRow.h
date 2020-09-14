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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_SHADOW_AQL_ITEM_ROW_H
#define ARANGOD_AQL_SHADOW_AQL_ITEM_ROW_H 1

#include "Aql/SharedAqlItemBlockPtr.h"

#include <cstddef>

namespace arangodb {
namespace velocypack {
struct Options;
}
namespace aql {

struct CreateInvalidShadowRowHint {
  // Forbid creating this via `{}`
  constexpr explicit CreateInvalidShadowRowHint() = default;
};

/**
 * @brief One shadow row within an AqlItemBlock.
 *
 * Does not keep a reference to the data.
 * Caller needs to make sure that the underlying
 * AqlItemBlock is not going out of scope.
 *
 * Note that this class will be copied a lot, and therefore should be small
 * and not do too complex things when copied!
 *
 * This row is used to indicate a separator between different executions of a subquery.
 * It will contain the data of the subquery input (former used in initializeCursor).
 * We can never write to ShadowRow again, only SubqueryEnd nodes are allowed transform
 * a ShadowRow into an AqlOutputRow again, and add the result of the subquery to it.
 */

class ShadowAqlItemRow {
 public:
  constexpr explicit ShadowAqlItemRow(CreateInvalidShadowRowHint)
      : _block(nullptr), _baseIndex(0) {}

  explicit ShadowAqlItemRow(
      // cppcheck-suppress passedByValue
      SharedAqlItemBlockPtr block, size_t baseIndex);

  /// @brief get the number of data registers in the underlying block.
  ///        Not all of these registers are necessarily filled by this
  ///        ShadowRow. There might be empty registers on deeper levels.
  [[nodiscard]] RegisterCount getNumRegisters() const noexcept;

  /// @brief a ShadowRow is relevant iff it indicates an end of subquery block on the subquery context
  ///        we are in right now. This will only be of importance on nested subqueries.
  ///        Within the inner subquery all shadowrows of this inner are relevant. All shadowRows
  ///        of the outer subquery are NOT relevant
  ///        Also note: There is a guarantee that a non-relevant shadowrow, can only be encountered
  ///        right after a shadowrow. And only in descending nesting level. (eg 1. inner most, 2. inner, 3. outer most)
  [[nodiscard]] bool isRelevant() const noexcept;

  /// @brief Test if this shadow row is initialized, eg has a block and has a valid depth.
  [[nodiscard]] bool isInitialized() const;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  /**
   * @brief Compare the underlying block. Only for assertions.
   */
  [[nodiscard]] bool internalBlockIs(SharedAqlItemBlockPtr const& other, size_t index) const;
#endif

  /**
   * @brief Get a reference to the value of the given Variable Nr
   *
   * @param registerId The register ID of the variable to read.
   *
   * @return Reference to the AqlValue stored in that variable.
   */
  [[nodiscard]] AqlValue const& getValue(RegisterId registerId) const;

  [[nodiscard]] AqlValue stealAndEraseValue(RegisterId registerId);

  /// @brief get the depthValue of the shadow row
  [[nodiscard]] size_t getShadowDepthValue() const;

  /// @brief get the depthValue of the shadow row as int64_t >= 0
  ///        NOTE: Innermost query will have depth 0. Outermost query wil have highest depth.
  [[nodiscard]] uint64_t getDepth() const;

  // Check whether the rows are *identical*, that is,
  // the same row in the same block.
  [[nodiscard]] bool isSameBlockAndIndex(ShadowAqlItemRow const& other) const noexcept;

#ifdef ARANGODB_USE_GOOGLE_TESTS
  // This checks whether the rows are equivalent, in the sense that they hold
  // the same number of registers and their entry-AqlValues compare equal,
  // plus their shadow-depth is the same.
  // In maintainer mode, it also asserts that the number of registers of the
  // blocks are equal, because comparing rows of blocks with different layouts
  // does not make sense.
  // Invalid rows are considered equivalent.
  [[nodiscard]] bool equates(ShadowAqlItemRow const& other,
                             velocypack::Options const* option) const noexcept;
#endif

 private:
  [[nodiscard]] AqlItemBlock& block() noexcept;

  [[nodiscard]] AqlItemBlock const& block() const noexcept;

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
