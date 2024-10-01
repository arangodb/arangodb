////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/SharedAqlItemBlockPtr.h"

namespace arangodb::aql {

class ShadowAqlItemRow;

class AqlItemBlockInputRange {
 public:
  /// @brief tag that can be used to speed up nextDataRow
  struct HasDataRow {};

 public:
  explicit AqlItemBlockInputRange(MainQueryState state,
                                  std::size_t skipped = 0);

  AqlItemBlockInputRange(MainQueryState, std::size_t skipped,
                         SharedAqlItemBlockPtr,
                         std::size_t startIndex) noexcept;

  void reset() noexcept { _block.reset(nullptr); }
  bool hasBlock() const noexcept { return _block.get() != nullptr; }

  arangodb::aql::SharedAqlItemBlockPtr getBlock() const noexcept;

  ExecutorState upstreamState() const noexcept;

  bool hasValidRow() const noexcept;

  bool hasDataRow() const noexcept;

  std::pair<ExecutorState, InputAqlItemRow> peekDataRow() const;

  /// @brief optimized version of peekDataRow, only to be used when it is known
  /// that there is a next data row (i.e. if a previous call to hasDataRow()
  /// returned true)
  std::pair<ExecutorState, InputAqlItemRow> peekDataRow(HasDataRow) const;

  std::pair<ExecutorState, InputAqlItemRow> nextDataRow();

  /// @brief optimized version of nextDataRow, only to be used when it is known
  /// that there is a next data row (i.e. if a previous call to hasDataRow()
  /// returned true)
  std::pair<ExecutorState, InputAqlItemRow> nextDataRow(HasDataRow);

  /// @brief moves the row index one forward if we are at a row right now
  void advanceDataRow() noexcept;

  std::size_t getRowIndex() const noexcept { return _rowIndex; };

  bool hasShadowRow() const noexcept;

  ShadowAqlItemRow peekShadowRow() const;

  std::pair<ExecutorState, ShadowAqlItemRow> nextShadowRow();

  size_t skipAllRemainingDataRows() noexcept;

  // depthOffset is added to depth, except it won't underflow.
  template<int depthOffset>
  requires(depthOffset == 0 || depthOffset == -1) size_t
      skipAllShadowRowsOfDepth(size_t depth);

  // Subtract up to this many rows from the local `_skipped` state; return
  // the number actually skipped. Does not skip data rows.
  [[nodiscard]] auto skip(std::size_t) noexcept -> std::size_t;

  [[nodiscard]] auto skipAll() noexcept -> std::size_t;

  [[nodiscard]] auto skippedInFlight() const noexcept -> std::size_t;

  /**
   * @brief Count how many datarows are expected in this range
   *        Used to estimate amount of produced rows
   * @return std::size_t
   */
  [[nodiscard]] auto countDataRows() const noexcept -> std::size_t;

  /**
   * @brief Count how many shadowRows are expected in this range
   *        Used to estimate amount of produced rows
   * @return std::size_t
   */
  [[nodiscard]] auto countShadowRows() const noexcept -> std::size_t;

  [[nodiscard]] auto finalState() const noexcept -> MainQueryState;

  /**
   * @brief Skip over all remaining data rows until the next shadow row.
   * Count how many rows are skipped
   */
  [[nodiscard]] auto countAndSkipAllRemainingDataRows() noexcept -> std::size_t;

  [[nodiscard]] auto numRowsLeft() const noexcept -> std::size_t;

 private:
  bool isIndexValid(std::size_t index) const noexcept;

  bool isShadowRowAtIndex(std::size_t index) const noexcept;

  enum LookAhead { NOW, NEXT };
  enum RowType { DATA, SHADOW };
  template<LookAhead doPeek, RowType type>
  ExecutorState nextState() const noexcept;

  SharedAqlItemBlockPtr _block{nullptr};
  std::size_t _rowIndex{};
  MainQueryState _finalState{MainQueryState::HASMORE};
  // How many rows were skipped upstream
  std::size_t _skipped{};
};

}  // namespace arangodb::aql
