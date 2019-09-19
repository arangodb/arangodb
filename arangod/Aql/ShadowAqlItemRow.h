////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019-2019 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/AqlItemBlock.h"
#include "Aql/AqlValue.h"
#include "Aql/ExecutionNode.h"
#include "Aql/SharedAqlItemBlockPtr.h"
#include "Basics/Common.h"

namespace arangodb {
namespace aql {

struct CreateInvalidShadowRowHint {
  // Forbid creating this via `{}`
  explicit CreateInvalidShadowRowHint() = default;
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
  explicit ShadowAqlItemRow(CreateInvalidShadowRowHint)
    : _block(nullptr), _baseIndex(0) {}

  explicit ShadowAqlItemRow(
      // cppcheck-suppress passedByValue
      SharedAqlItemBlockPtr block, size_t baseIndex)
      : _block(std::move(block)), _baseIndex(baseIndex) {
    TRI_ASSERT(isInitialized());
  }

  std::size_t getNrRegisters() const noexcept { return block().getNrRegs(); }

  bool isRelevant() const noexcept { return getDepth() == 0; }

  inline bool isInitialized() const {
    TRI_ASSERT(_block != nullptr);
    // The value needs to always be a positive integer.
    auto depthVal = block().getShadowRowDepth(_baseIndex);
    TRI_ASSERT(depthVal.isNumber());
    TRI_ASSERT(depthVal.toInt64() >= 0);
    return true;
  }

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  /**
   * @brief Compare the underlying block. Only for assertions.
   */
  bool internalBlockIs(SharedAqlItemBlockPtr const& other) const {
    return _block == other;
  }
#endif

  /**
   * @brief Get a reference to the value of the given Variable Nr
   *
   * @param registerId The register ID of the variable to read.
   *
   * @return Reference to the AqlValue stored in that variable.
   */
  inline AqlValue const& getValue(RegisterId registerId) const {
    TRI_ASSERT(isInitialized());
    TRI_ASSERT(registerId < getNrRegisters());
    return block().getValueReference(_baseIndex, registerId);
  }

  inline AqlValue const& getShadowDepthValue() const {
    TRI_ASSERT(isInitialized());
    return block().getShadowRowDepth(_baseIndex);
  }

  inline int64_t getDepth() const {
    TRI_ASSERT(isInitialized());
    auto value = block().getShadowRowDepth(_baseIndex);
    TRI_ASSERT(value.toInt64() >= 0);
    return value.toInt64();
  }

 private:
  inline AqlItemBlock& block() noexcept {
    TRI_ASSERT(_block != nullptr);
    return *_block;
  }

  inline AqlItemBlock const& block() const noexcept {
    TRI_ASSERT(_block != nullptr);
    return *_block;
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
};

}  // namespace aql
}  // namespace arangodb

#endif
