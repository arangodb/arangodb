////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2020 ArangoDB GmbH, Cologne, Germany
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
/// @author Markus Pfeiffer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_MULTIAQLITEMBLOCKINPUTRANGE_H
#define ARANGOD_AQL_MULTIAQLITEMBLOCKINPUTRANGE_H

#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/SharedAqlItemBlockPtr.h"

namespace arangodb::aql {

class MultiAqlItemBlockInputRange {
 public:
  explicit MultiAqlItemBlockInputRange(ExecutorState state, std::size_t skipped = 0,
                                       std::size_t nrInputRanges = 1);

  MultiAqlItemBlockInputRange(ExecutorState, std::size_t skipped,
                              arangodb::aql::SharedAqlItemBlockPtr const&,
                              std::size_t startIndex);

  MultiAqlItemBlockInputRange(ExecutorState, std::size_t skipped,
                              arangodb::aql::SharedAqlItemBlockPtr&&,
                              std::size_t startIndex) noexcept;

  ExecutorState upstreamState(size_t const dependency = 0) const noexcept;
  bool upstreamHasMore(size_t const dependency = 0) const noexcept;

  bool hasDataRow(size_t const dependency = 0) const noexcept;

  std::pair<ExecutorState, arangodb::aql::InputAqlItemRow> peekDataRow(size_t const dependency = 0) const;
  std::pair<ExecutorState, arangodb::aql::InputAqlItemRow> nextDataRow(size_t const dependency = 0);

  bool hasShadowRow() const noexcept;

  std::pair<ExecutorState, arangodb::aql::ShadowAqlItemRow> peekShadowRow(size_t const dependency = 0) const;
  std::pair<ExecutorState, arangodb::aql::ShadowAqlItemRow> nextShadowRow(size_t const dependency = 0);

  std::size_t getRowIndex(size_t const dependency = 0) const noexcept;
  // Subtract up to this many rows from the local `_skipped` state; return
  // the number actually skipped. Does not skip data rows.
  [[nodiscard]] auto skip(std::size_t) noexcept -> std::size_t;

  [[nodiscard]] auto skipAll() noexcept -> std::size_t;

  [[nodiscard]] auto skippedInFlight() const noexcept -> std::size_t;

  auto resize(ExecutorState state, size_t skipped, size_t nrInputRanges) -> void;

  auto getBlock(size_t const dependency = 0) const noexcept -> SharedAqlItemBlockPtr;

  auto setDependency(size_t const dependency, AqlItemBlockInputRange& range) -> void;

 private:
  bool isIndexValid(std::size_t index) const noexcept;

  bool isShadowRowAtIndex(std::size_t index) const noexcept;

  enum LookAhead { NOW, NEXT };
  enum RowType { DATA, SHADOW };
  template <LookAhead doPeek, RowType type>
  ExecutorState nextState() const noexcept;

 private:
  ExecutorState _finalState{ExecutorState::HASMORE};

  std::vector<AqlItemBlockInputRange> _inputs;
};

}  // namespace arangodb::aql

#endif
