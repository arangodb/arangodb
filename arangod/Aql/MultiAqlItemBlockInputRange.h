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

  ExecutorState upstreamState(size_t const dependency) const noexcept;
  bool upstreamHasMore(size_t const dependency) const noexcept;

  bool hasDataRow() const noexcept;
  bool hasDataRow(size_t const dependency) const noexcept;

  /**
   * @brief Get a reference to the range of a given dependency
   * NOTE: Modifing this range will modify the state of this class as well
   *
   * @param dependency index of the dependency
   * @return AqlItemBlockInputRange& Modifyable reference to the input data stream
   */
  auto rangeForDependency(size_t const dependency) -> AqlItemBlockInputRange&;

  std::pair<ExecutorState, arangodb::aql::InputAqlItemRow> peekDataRow(size_t const dependency) const;
  std::pair<ExecutorState, arangodb::aql::InputAqlItemRow> nextDataRow(size_t const dependency);
  auto skipAll(size_t const dependency) noexcept -> std::size_t;

  bool hasShadowRow() const noexcept;

  arangodb::aql::ShadowAqlItemRow peekShadowRow() const;
  std::pair<ExecutorState, arangodb::aql::ShadowAqlItemRow> nextShadowRow();

  auto isDone() const -> bool;

  auto resizeIfNecessary(ExecutorState state, size_t skipped, size_t nrInputRanges) -> void;

  auto getBlock(size_t const dependency = 0) const noexcept -> SharedAqlItemBlockPtr;

  auto setDependency(size_t const dependency, AqlItemBlockInputRange& range) -> void;

  auto skipAllRemainingDataRows() -> size_t;

  auto numberDependencies() const noexcept -> size_t;

 private:
  ExecutorState _finalState{ExecutorState::HASMORE};

  std::vector<AqlItemBlockInputRange> _inputs;
};

}  // namespace arangodb::aql

#endif
