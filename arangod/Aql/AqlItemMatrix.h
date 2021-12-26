////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2021 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "Aql/ShadowAqlItemRow.h"

#include <cstdint>
#include <utility>
#include <vector>

namespace arangodb::aql {

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
  RegisterCount getNumRegisters() const noexcept;

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
  std::pair<AqlItemBlock const*, size_t> getBlockRef(size_t index) const noexcept;

  bool stoppedOnShadowRow() const noexcept;

  ShadowAqlItemRow popShadowRow();

  ShadowAqlItemRow peekShadowRow() const;

  [[nodiscard]] auto hasMoreAfterShadowRow() const noexcept -> bool;

  [[nodiscard]] auto countDataRows() const noexcept -> std::size_t;

  [[nodiscard]] auto countShadowRows() const noexcept -> std::size_t;

  /**
   * @brief Skip over all shadowRows with a Subquery-depth <= depth
   *
   * @return firstValue: The number of ShadowRows in depth Skipped
   *         secondValue: The first shadowRow of Subquery-depth > depth, or not-initialize if no such row exists.
   * Invariant: secondValue.initialized() <=> this.numberBlocks() > 0
   */
  [[nodiscard]] auto skipAllShadowRowsOfDepth(size_t depth)
      -> std::tuple<size_t, ShadowAqlItemRow>;

  class RowIterator {
   public:
    using value_type = InputAqlItemRow;

    RowIterator() = default;
    RowIterator(AqlItemMatrix const* matrix, size_t blockIndex, size_t rowIndex);

    // Returns the current value, and move the iterator to the next value
    value_type next() noexcept;

    auto isInitialized() const noexcept -> bool;

    // Returns whether the current value is valid, i.e. whether next() may be
    // called
    auto hasMore() const noexcept -> bool;

    value_type operator*() const noexcept;

    // This can't be implemented, as we can only create the InputAqlItemRow
    // on-the-fly.
    // pointer operator->();

    // Prefix increment
    RowIterator& operator++() noexcept;

    // Postfix increment.
    auto operator++(int) & noexcept -> RowIterator;

    explicit operator bool() const noexcept;

    friend bool operator==(RowIterator const& a, RowIterator const& b);
    friend bool operator!=(RowIterator const& a, RowIterator const& b);

   private:
    AqlItemMatrix const* _matrix{};
    std::size_t _blockIndex{};
    // Invariant: _rowIndex is valid iff _blockIndex is valid.
    std::size_t _rowIndex{};
  };

  [[nodiscard]] RowIterator begin() const;
  [[nodiscard]] RowIterator end() const;

 private:
  std::vector<SharedAqlItemBlockPtr> _blocks;

  uint64_t _numDataRows;

  RegisterCount _nrRegs;
  size_t _startIndexInFirstBlock{0};
  size_t _stopIndexInLastBlock;
};

bool operator==(AqlItemMatrix::RowIterator const& a, AqlItemMatrix::RowIterator const& b);
bool operator!=(AqlItemMatrix::RowIterator const& a, AqlItemMatrix::RowIterator const& b);

}  // namespace arangodb::aql
