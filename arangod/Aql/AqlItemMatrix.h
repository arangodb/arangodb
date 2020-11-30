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

#include "Aql/ShadowAqlItemRow.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace arangodb {
namespace aql {

class InputAqlItemRow;
class SharedAqlItemBlockPtr;

/**
 * @brief A Matrix of AqlItemRows
 */
class AqlItemMatrix {
 public:
  // uint32_t in this vector is a reasonable trade-off between performance and amount of data.
  // With this values we can sort up to ~ 4.000.000.000 times 1000 elements in memory.
  // Anything beyond that has a questionable runtime on nowadays hardware anyways.
  using RowIndex = std::pair<uint32_t, uint32_t>;

  explicit AqlItemMatrix(RegisterCount nrRegs);
  ~AqlItemMatrix() = default;

  /**
   * @brief Add this block of rows into the Matrix
   *
   * @param blockPtr Block of rows to append in the matrix
   */
  void addBlock(SharedAqlItemBlockPtr blockPtr);

  /**
   * @brief Get the number of rows stored in this Matrix
   *
   * @return Number of Rows
   */
  uint64_t size() const noexcept;

  /**
   * @brief Calculate the memory usage for the row indexes of the matrix
   */
  size_t memoryUsageForRowIndexes() const noexcept;

  /**
   * @brief Number of registers, i.e. width of the matrix.
   */
  RegisterCount getNrRegisters() const noexcept;

  /**
   * @brief Test if this matrix is empty
   *
   * @return True if empty
   */
  bool blocksEmpty() const noexcept;

  void clear();

  std::vector<RowIndex> produceRowIndexes() const;

  /**
   * @brief Get the AqlItemRow at the given index
   *
   * @param index The index of the Row to read inside the matrix
   *
   * @return A single row in the Matrix
   */
  InputAqlItemRow getRow(RowIndex index) const noexcept;

  size_t numberOfBlocks() const noexcept;

  std::pair<SharedAqlItemBlockPtr, size_t> getBlock(size_t index) const noexcept;

  bool stoppedOnShadowRow() const noexcept;

  ShadowAqlItemRow popShadowRow();

  ShadowAqlItemRow peekShadowRow() const;

  [[nodiscard]] auto hasMoreAfterShadowRow() const noexcept -> bool;

  [[nodiscard]] auto countDataRows() const noexcept -> std::size_t;

  [[nodiscard]] auto countShadowRows() const noexcept -> std::size_t;

 private:
  std::vector<SharedAqlItemBlockPtr> _blocks;

  uint64_t _numDataRows;

  RegisterCount _nrRegs;
  size_t _startIndexInFirstBlock{0};
  size_t _stopIndexInLastBlock;
};

}  // namespace aql
}  // namespace arangodb

#endif
