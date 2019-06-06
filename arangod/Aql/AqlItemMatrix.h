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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_AQL_ITEM_MATRIX_H
#define ARANGOD_AQL_AQL_ITEM_MATRIX_H 1

#include <lib/Logger/Logger.h>
#include "Aql/AqlItemBlock.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/types.h"
#include "Basics/Common.h"

#include <math.h>

namespace arangodb {
namespace aql {

/**
 * @brief A Matrix of AqlItemRows
 */
class AqlItemMatrix {
 public:
  // uint32_t in this vector is a reasonable trade-off between performance and amount of data.
  // With this values we can sort up to ~ 4.000.000.000 times 1000 elements in memory.
  // Anything beyond that has a questionable runtime on nowadays hardware anyways.
  typedef std::pair<uint32_t, uint32_t> RowIndex;

  explicit AqlItemMatrix(size_t nrRegs) : _size(0), _nrRegs(nrRegs) {}
  ~AqlItemMatrix() = default;

  /**
   * @brief Add this block of rows into the Matrix
   *
   * @param blockPtr Block of rows to append in the matrix
   */
  void addBlock(SharedAqlItemBlockPtr blockPtr) {
    TRI_ASSERT(blockPtr->getNrRegs() == getNrRegisters());
    // Test if we have more than uint32_t many blocks
    if (ADB_UNLIKELY(_blocks.size() == std::numeric_limits<uint32_t>::max())) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_RESOURCE_LIMIT,
          "Reaching the limit of AqlItems to SORT, please consider using a "
          "limit after sorting.");
    }
    // Test if we have more than uint32_t many rows within a block
    if (ADB_UNLIKELY(blockPtr->size() > std::numeric_limits<uint32_t>::max())) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_RESOURCE_LIMIT,
          "Reaching the limit of AqlItems to SORT, please consider lowering "
          "the batch size.");
    }
    _size += blockPtr->size();
    _blocks.emplace_back(std::move(blockPtr));
  }

  /**
   * @brief Get the number of rows stored in this Matrix
   *
   * @return Number of Rows
   */
  uint64_t size() const noexcept { return _size; }

  /**
   * @brief Number of registers, i.e. width of the matrix.
   */
  size_t getNrRegisters() const noexcept { return _nrRegs; };

  /**
   * @brief Test if this matrix is empty
   *
   * @return True if empty
   */
  bool empty() const noexcept { return _blocks.empty(); }

  std::vector<RowIndex> produceRowIndexes() const {
    std::vector<RowIndex> result;
    if (!empty()) {
      result.reserve(size());
      uint32_t index = 0;
      for (auto const& block : _blocks) {
        for (uint32_t row = 0; row < block->size(); ++row) {
          result.emplace_back(index, row);
        }
        ++index;
      }
    }
    return result;
  }

  /**
   * @brief Get the AqlItemRow at the given index
   *
   * @param index The index of the Row to read inside the matrix
   *
   * @return A single row in the Matrix
   */
  InputAqlItemRow getRow(RowIndex index) const noexcept {
    auto const& block = getBlock(index.first);
    return InputAqlItemRow{block, index.second};
  }

  inline size_t numberOfBlocks() const noexcept { return _blocks.size(); }

  inline SharedAqlItemBlockPtr getBlock(size_t index) const noexcept {
    TRI_ASSERT(index < numberOfBlocks());
    return _blocks[index];
  }

 private:
  std::vector<SharedAqlItemBlockPtr> _blocks;

  uint64_t _size;

  size_t _nrRegs;
};

}  // namespace aql
}  // namespace arangodb

#endif
