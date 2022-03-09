////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#pragma once

#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/AqlItemMatrix.h"
#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/SharedAqlItemBlockPtr.h"

namespace arangodb::aql {

class ShadowAqlItemRow;

class AqlItemBlockInputMatrix {
 public:
  explicit AqlItemBlockInputMatrix(MainQueryState state);

  AqlItemBlockInputMatrix(MainQueryState state, AqlItemMatrix* aqlItemMatrix);

  std::pair<ExecutorState, ShadowAqlItemRow> nextShadowRow();
  ShadowAqlItemRow peekShadowRow() const;

  void reset() noexcept {
    // nothing to do here.
  }

  bool hasShadowRow() const noexcept;
  bool hasDataRow() const noexcept;
  bool hasValidRow() const noexcept;

  arangodb::aql::SharedAqlItemBlockPtr getBlock() const noexcept;

  std::pair<ExecutorState, AqlItemMatrix const*> getMatrix() noexcept;

  ExecutorState upstreamState() const noexcept;

  size_t skipAllRemainingDataRows();

  size_t skipAllShadowRowsOfDepth(size_t depth);

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

 private:
  void advanceBlockIndexAndShadowRow() noexcept;

 private:
  MainQueryState _finalState{MainQueryState::HASMORE};

  // Only if _aqlItemMatrix is set (and NOT a nullptr), we have a valid and
  // usable DataRange object available to work with.
  AqlItemMatrix* _aqlItemMatrix;
  ShadowAqlItemRow _shadowRow{CreateInvalidShadowRowHint{}};
};

}  // namespace arangodb::aql
