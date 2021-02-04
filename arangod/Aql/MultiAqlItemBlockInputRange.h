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

  bool hasValidRow() const noexcept;
  
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

  [[nodiscard]] auto skippedInFlight(size_t dependency) const noexcept -> std::size_t;

  bool hasShadowRow() const noexcept;

  arangodb::aql::ShadowAqlItemRow peekShadowRow() const;
  std::pair<ExecutorState, arangodb::aql::ShadowAqlItemRow> nextShadowRow();

  auto isDone() const -> bool;
  auto state() const -> ExecutorState;

  auto resizeOnce(ExecutorState state, size_t skipped, size_t nrInputRanges) -> void;

  [[nodiscard]] auto getBlock(size_t dependency = 0) const noexcept -> SharedAqlItemBlockPtr;

  auto setDependency(size_t dependency, AqlItemBlockInputRange const& range) -> void;

  // This discards all remaining data rows
  auto skipAllRemainingDataRows() -> size_t;

  // Skips all ShadowRows of lower or equal depth then given in all
  // locally known ranges. Reports the amount of skipped equal depth
  // ShadowRows per depth.
  auto skipAllShadowRowsOfDepth(size_t depth) -> std::vector<size_t>;

  // Subtract up to count rows from the local _skipped state
  auto skipForDependency(size_t const dependency, size_t count) -> size_t;
  // Skipp all that is available
  auto skipAllForDependency(size_t const dependency) -> size_t;

  auto numberDependencies() const noexcept -> size_t;

  auto reset() -> void;

  /**
   * @brief Count how many datarows are expected in this range
   *        Used to estimate amount of produced rows
   *        Combines output of all included Ranges
   * @return std::size_t
   */
  [[nodiscard]] auto countDataRows() const noexcept -> std::size_t;

  /**
   * @brief Count how many shadowRows are expected in this range
   *        Used to estimate amount of produced rows
   *        Combines output of all included Ranges
   * @return std::size_t
   */
  [[nodiscard]] auto countShadowRows() const noexcept -> std::size_t;

  /**
   * @brief The final State of all ranges. Combined
   *        Will be DONE only if all inputs are DONE
   *
   * @return ExecutorState
   */
  [[nodiscard]] auto finalState() const noexcept -> ExecutorState;

 private:
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  bool _dependenciesDontAgreeOnState{false};
#endif
  std::vector<AqlItemBlockInputRange> _inputs;
};

}  // namespace arangodb::aql

#endif
