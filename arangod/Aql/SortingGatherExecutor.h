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

#ifndef ARANGOD_AQL_SORTING_GATHER_EXECUTOR_H
#define ARANGOD_AQL_SORTING_GATHER_EXECUTOR_H

#include "Aql/ClusterNodes.h"
#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"

namespace arangodb {

namespace transaction {
class Methods;
}

namespace aql {

class MultiDependencySingleRowFetcher;
class NoStats;
class OutputAqlItemRow;
struct SortRegister;

class SortingGatherExecutorInfos : public ExecutorInfos {
 public:
  SortingGatherExecutorInfos(std::shared_ptr<std::unordered_set<RegisterId>> inputRegisters,
                             std::shared_ptr<std::unordered_set<RegisterId>> outputRegisters,
                             RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
                             std::unordered_set<RegisterId> registersToClear,
                             std::unordered_set<RegisterId> registersToKeep,
                             std::vector<SortRegister>&& sortRegister,
                             arangodb::transaction::Methods* trx,
                             GatherNode::SortMode sortMode, size_t limit,
                             GatherNode::Parallelism p);
  SortingGatherExecutorInfos() = delete;
  SortingGatherExecutorInfos(SortingGatherExecutorInfos&&);
  SortingGatherExecutorInfos(SortingGatherExecutorInfos const&) = delete;
  ~SortingGatherExecutorInfos();

  std::vector<SortRegister>& sortRegister() { return _sortRegister; }

  arangodb::transaction::Methods* trx() { return _trx; }

  GatherNode::SortMode sortMode() const noexcept { return _sortMode; }
  
  GatherNode::Parallelism parallelism() const noexcept { return _parallelism; }

  size_t limit() const noexcept { return _limit; }

 private:
  std::vector<SortRegister> _sortRegister;
  arangodb::transaction::Methods* _trx;
  GatherNode::SortMode _sortMode;
  GatherNode::Parallelism _parallelism;
  size_t _limit;
};

class SortingGatherExecutor {
 public:
  struct ValueType {
    size_t dependencyIndex;
    InputAqlItemRow row;
    ExecutionState state;

    explicit ValueType(size_t index);
  };

  ////////////////////////////////////////////////////////////////////////////////
  /// @struct SortingStrategy
  ////////////////////////////////////////////////////////////////////////////////
  struct SortingStrategy {
    virtual ~SortingStrategy() = default;

    /// @brief returns next value
    virtual ValueType nextValue() = 0;

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

  SortingGatherExecutor(Fetcher& fetcher, Infos& infos);
  ~SortingGatherExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState,
   *         if something was written output.hasValue() == true
   */
  std::pair<ExecutionState, Stats> produceRows(OutputAqlItemRow& output);

  void adjustNrDone(size_t dependency);

  std::pair<ExecutionState, size_t> expectedNumberOfRows(size_t atMost) const;

  std::tuple<ExecutionState, Stats, size_t> skipRows(size_t atMost);

 private:
  void initNumDepsIfNecessary();

  ExecutionState init(size_t atMost);

  std::pair<ExecutionState, InputAqlItemRow> produceNextRow(size_t atMost);

  bool constrainedSort() const noexcept;

  size_t rowsLeftToWrite() const noexcept;

  void assertConstrainedDoesntOverfetch(size_t atMost) const noexcept;

  // This is interesting in case this is a constrained sort and fullCount is
  // enabled. Then, after the limit is reached, we may pass skipSome through
  // to our dependencies, and not sort any more.
  // This also means that we may not produce rows anymore after that point.
  bool maySkip() const noexcept;

 private:
  Fetcher& _fetcher;

  // Flag if we are past the initialize phase (fetched one block for every dependency).
  bool _initialized;

  // Total Number of dependencies
  size_t _numberDependencies;

  // The Dependency we have to fetch next
  size_t _dependencyToFetch;

  // Input data to process
  std::vector<ValueType> _inputRows;

  // Counter for DONE states
  size_t _nrDone;
  
  /// @brief If we do a constrained sort, it holds the limit > 0. Otherwise, it's 0.
  size_t _limit;

  /// @brief Number of rows we've already written or skipped, up to _limit.
  /// Only after _rowsReturned == _limit we may pass skip through to
  /// dependencies.
  size_t _rowsReturned;

  /// @brief When we reached the limit, we once count the rows that are left in
  /// the heap (in _rowsLeftInHeap), so we can count them for skipping.
  bool _heapCounted;

  /// @brief See comment for _heapCounted first. At the first real skip, this
  /// is set to the number of rows left in the heap. It will be reduced while
  /// skipping.
  size_t _rowsLeftInHeap;

  size_t _skipped;

  /// @brief sorting strategy
  std::unique_ptr<SortingStrategy> _strategy;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  std::vector<bool> _flaggedAsDone;
#endif
  std::tuple<ExecutionState, SortingGatherExecutor::Stats, size_t> reallySkipRows(size_t atMost);
  std::tuple<ExecutionState, SortingGatherExecutor::Stats, size_t> produceAndSkipRows(size_t atMost);
  
  const bool _fetchParallel;
};

}  // namespace aql
}  // namespace arangodb

#endif
