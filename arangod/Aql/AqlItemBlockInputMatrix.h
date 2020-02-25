////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#include "Aql/AqlItemMatrix.h"
#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/SharedAqlItemBlockPtr.h"

namespace arangodb::aql {

class ShadowAqlItemRow;

class AqlItemBlockInputMatrix {
 public:
  explicit AqlItemBlockInputMatrix(ExecutorState state, std::size_t skipped = 0);

  AqlItemBlockInputMatrix(arangodb::aql::SharedAqlItemBlockPtr const&);

  AqlItemBlockInputMatrix(ExecutorState state, std::size_t skipped, AqlItemMatrix* aqlItemMatrix);

  std::pair<ExecutorState, ShadowAqlItemRow> nextShadowRow();
  bool hasShadowRow() const noexcept;
  bool hasDataRow() const noexcept;

  arangodb::aql::SharedAqlItemBlockPtr getBlock() const noexcept;
  std::pair<ExecutorState, AqlItemMatrix const*> getMatrix() noexcept;

  ExecutorState upstreamState() const noexcept;
  bool upstreamHasMore() const noexcept;

  ExecutorState skipAllRemainingDataRows();

  // Subtract up to this many rows from the local `_skipped` state; return
  // the number actually skipped. Does not skip data rows.
  [[nodiscard]] auto skip(std::size_t) noexcept -> std::size_t;
  [[nodiscard]] auto skipAll() noexcept -> std::size_t;
  [[nodiscard]] auto skippedInFlight() const noexcept -> std::size_t;

 private:
  arangodb::aql::SharedAqlItemBlockPtr _block{nullptr};
  ExecutorState _finalState{ExecutorState::HASMORE};
  // How many rows were skipped upstream
  std::size_t _skipped{};
  AqlItemMatrix* _aqlItemMatrix;

  // if isRelevant is set to false, we're only having a single data block
  bool _isRelevant;
  ShadowAqlItemRow _shadowRow{CreateInvalidShadowRowHint{}};
};

}  // namespace arangodb::aql

#endif  // ARANGOD_AQL_AQLITEMBLOCKINPUTITERATOR_H
