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
                             GatherNode::SortMode sortMode);
  SortingGatherExecutorInfos() = delete;
  SortingGatherExecutorInfos(SortingGatherExecutorInfos&&);
  SortingGatherExecutorInfos(SortingGatherExecutorInfos const&) = delete;
  ~SortingGatherExecutorInfos();

  std::vector<SortRegister>& sortRegister() { return _sortRegister; }

  arangodb::transaction::Methods* trx() { return _trx; }

  GatherNode::SortMode sortMode() { return _sortMode; }

 private:
  std::vector<SortRegister> _sortRegister;
  arangodb::transaction::Methods* _trx;
  GatherNode::SortMode _sortMode;
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
    static const bool preservesOrder = true;
    static const bool allowsBlockPassthrough = false;
    static const bool inputSizeRestrictsOutputSize = true;
  };

  using Fetcher = MultiDependencySingleRowFetcher;
  using Infos = SortingGatherExecutorInfos;
  using Stats = NoStats;

  SortingGatherExecutor(Fetcher& fetcher, Infos& unused);
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

 private:
  ExecutionState init();

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

  /// @brief sorting strategy
  std::unique_ptr<SortingStrategy> _strategy;

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  std::vector<bool> _flaggedAsDone;
#endif
};

}  // namespace aql
}  // namespace arangodb

#endif
