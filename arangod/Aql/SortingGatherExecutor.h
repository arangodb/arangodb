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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_SORTING_GATHER_EXECUTOR_H
#define ARANGOD_AQL_SORTING_GATHER_EXECUTOR_H

#include "Aql/AqlCallSet.h"
#include "Aql/ClusterNodes.h"
#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/RegisterInfos.h"

#include <optional>

namespace arangodb {

namespace transaction {
class Methods;
}

namespace aql {

struct AqlCall;
class AqlCallList;
class MultiDependencySingleRowFetcher;
class MultiAqlItemBlockInputRange;
class NoStats;
class OutputAqlItemRow;
struct SortRegister;

class SortingGatherExecutorInfos {
 public:
  SortingGatherExecutorInfos(std::vector<SortRegister>&& sortRegister,
                             arangodb::aql::QueryContext& query, GatherNode::SortMode sortMode,
                             size_t limit, GatherNode::Parallelism p);
  SortingGatherExecutorInfos() = delete;
  SortingGatherExecutorInfos(SortingGatherExecutorInfos&&) = default;
  SortingGatherExecutorInfos(SortingGatherExecutorInfos const&) = delete;
  ~SortingGatherExecutorInfos() = default;

  std::vector<SortRegister>& sortRegister() { return _sortRegister; }

  arangodb::aql::QueryContext& query() { return _query; }

  GatherNode::SortMode sortMode() const noexcept { return _sortMode; }

  GatherNode::Parallelism parallelism() const noexcept { return _parallelism; }

  size_t limit() const noexcept { return _limit; }

 private:
  std::vector<SortRegister> _sortRegister;
  arangodb::aql::QueryContext& _query;
  GatherNode::SortMode _sortMode;
  GatherNode::Parallelism _parallelism;
  size_t _limit;
};

class SortingGatherExecutor {
 public:
  struct ValueType {
    size_t dependencyIndex;
    InputAqlItemRow row;
    ExecutorState state;

    explicit ValueType(size_t index);
    ValueType(size_t, InputAqlItemRow, ExecutorState);
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @struct SortingStrategy
  ////////////////////////////////////////////////////////////////////////////////
  struct SortingStrategy {
    virtual ~SortingStrategy() = default;

    /// @brief returns next value
    [[nodiscard]] virtual auto nextValue() -> ValueType = 0;

    /// @brief prepare strategy fetching values
    virtual void prepare(std::vector<ValueType>& /*blockPos*/) {}

    /// @brief resets strategy state
    virtual void reset() = 0;
  };  // SortingStrategy

 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = true;
  };

  using Fetcher = MultiDependencySingleRowFetcher;
  using Infos = SortingGatherExecutorInfos;
  using Stats = NoStats;

  SortingGatherExecutor(Fetcher&, Infos& infos);
  ~SortingGatherExecutor();

  /**
   * @brief Produce rows
   *
   * @param input DataRange delivered by the fetcher
   * @param output place to write rows to
   * @return std::tuple<ExecutorState, Stats, AqlCall, size_t>
   *   ExecutorState: DONE or HASMORE (only within a subquery)
   *   Stats: Stats gerenated here
   *   AqlCallSet: Request to specific upstream dependency
   */
  [[nodiscard]] auto produceRows(MultiAqlItemBlockInputRange& input, OutputAqlItemRow& output)
      -> std::tuple<ExecutorState, Stats, AqlCallSet>;

  /**
   * @brief Skip rows
   *
   * @param input DataRange delivered by the fetcher
   * @param call skip request form consumer
   * @return std::tuple<ExecutorState, Stats, AqlCallSet>
   *   ExecutorState: DONE or HASMORE (only within a subquery)
   *   Stats: Stats gerenated here
   *   size_t: Number of rows skipped
   *   AqlCallSet: Request to specific upstream dependency
   */
  [[nodiscard]] auto skipRowsRange(MultiAqlItemBlockInputRange& input, AqlCall& call)
      -> std::tuple<ExecutorState, Stats, size_t, AqlCallSet>;

 private:
  [[nodiscard]] auto constrainedSort() const noexcept -> bool;

  void assertConstrainedDoesntOverfetch(size_t atMost) const noexcept;

  // This is interesting in case this is a constrained sort and fullCount is
  // enabled. Then, after the limit is reached, we may pass skipSome through
  // to our dependencies, and not sort any more.
  // This also means that we may not produce rows anymore after that point.
  [[nodiscard]] auto maySkip() const noexcept -> bool;

  /**
   * @brief Function that checks if all dependencies are either
   *  done, or have a row.
   *  The first one that does not match the condition
   *  will produce an upstream call to be fulfilled.
   *
   * @param inputRange Range of all input dependencies
   * @return std::optional<std::tuple<AqlCall, size_t>>  optional call for the dependnecy requiring input
   */
  [[nodiscard]] auto requiresMoreInput(MultiAqlItemBlockInputRange const& inputRange,
                                       AqlCall const& clientCall) -> AqlCallSet;

  /**
   * @brief Get the next row matching the sorting strategy
   *
   * @return InputAqlItemRow best fit row. Might be invalid if all input is done.
   */
  [[nodiscard]] auto nextRow(MultiAqlItemBlockInputRange& input) -> InputAqlItemRow;

  /**
   * @brief Initialize the Sorting strategy with the given input.
   *        This is known to be empty, but all prepared at this point.
   * @param inputRange The input, no data included yet.
   */
  [[nodiscard]] auto initialize(MultiAqlItemBlockInputRange const& inputRange,
                                AqlCall const& clientCall) -> AqlCallSet;

  [[nodiscard]] auto rowsLeftToWrite() const noexcept -> size_t;

  [[nodiscard]] auto limitReached() const noexcept -> bool;

  [[nodiscard]] auto calculateUpstreamCall(AqlCall const& clientCall) const
      noexcept -> AqlCallList;

 private:
  // Flag if we are past the initialize phase (fetched one block for every dependency).
  bool _initialized = false;

  // Total Number of dependencies
  size_t _numberDependencies;

  // Input data to process, indexed by dependency, referenced by the SortingStrategy
  std::vector<ValueType> _inputRows;

  /// @brief If we do a constrained sort, it holds the limit > 0. Otherwise, it's 0.
  size_t _limit;

  /// @brief Number of rows we've already written or skipped, up to _limit.
  /// Only after _rowsReturned == _limit we may pass skip through to
  /// dependencies.
  size_t _rowsReturned;

  /// @brief sorting strategy
  std::unique_ptr<SortingStrategy> _strategy;

  const bool _fetchParallel;

  std::optional<size_t> _depToUpdate = std::nullopt;
};

}  // namespace aql
}  // namespace arangodb

#endif
