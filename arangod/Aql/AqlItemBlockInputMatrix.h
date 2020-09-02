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
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_AQLITEMBLOCKMATRIXITERATOR_H
#define ARANGOD_AQL_AQLITEMBLOCKMATRIXITERATOR_H

#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/AqlItemMatrix.h"
#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/SharedAqlItemBlockPtr.h"

namespace arangodb::aql {

class ShadowAqlItemRow;

class AqlItemBlockInputMatrix {
 public:
  explicit AqlItemBlockInputMatrix(ExecutorState state);

  AqlItemBlockInputMatrix(arangodb::aql::SharedAqlItemBlockPtr const&);

  AqlItemBlockInputMatrix(ExecutorState state, AqlItemMatrix* aqlItemMatrix);

  std::pair<ExecutorState, ShadowAqlItemRow> nextShadowRow();
  ShadowAqlItemRow peekShadowRow() const;

  bool hasShadowRow() const noexcept;
  bool hasDataRow() const noexcept;
  bool hasValidRow() const noexcept;

  arangodb::aql::SharedAqlItemBlockPtr getBlock() const noexcept;

  // Will provide access to the first block (from _aqlItemMatrix)
  // After a block has been delivered, the block index will be increased.
  // Next call then will deliver the next block etc.
  AqlItemBlockInputRange& getInputRange();
  std::pair<ExecutorState, AqlItemMatrix const*> getMatrix() noexcept;

  ExecutorState upstreamState() const noexcept;
  bool upstreamHasMore() const noexcept;
  size_t skipAllRemainingDataRows();

  // Will return HASMORE if we were able to increase the row index.
  // Otherwise will return DONE.
  ExecutorState incrBlockIndex();
  void resetBlockIndex() noexcept;

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

  [[nodiscard]] auto finalState() const noexcept -> ExecutorState;

 private:
  arangodb::aql::SharedAqlItemBlockPtr _block{nullptr};
  ExecutorState _finalState{ExecutorState::HASMORE};

  // Only if _aqlItemMatrix is set (and NOT a nullptr), we have a valid and
  // usable DataRange object available to work with.
  AqlItemMatrix* _aqlItemMatrix;
  AqlItemBlockInputRange _lastRange{ExecutorState::HASMORE};
  size_t _currentBlockRowIndex = 0;
  ShadowAqlItemRow _shadowRow{CreateInvalidShadowRowHint{}};
};

}  // namespace arangodb::aql

#endif  // ARANGOD_AQL_AQLITEMBLOCKINPUTITERATOR_H
