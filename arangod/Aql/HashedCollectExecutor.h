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
/// @author Tobias Goedderz
/// @author Michael Hackstein
/// @author Heiko Kernbach
/// @author Jan Christoph Uhde
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_HASHED_COLLECT_EXECUTOR_H
#define ARANGOD_AQL_HASHED_COLLECT_EXECUTOR_H

#include "Aql/AqlValueGroup.h"
#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/Stats.h"
#include "Aql/types.h"

#include <memory>
#include <unordered_map>

namespace arangodb {
class ResourceMonitor;

namespace aql {
class OutputAqlItemRow;
class ExecutorInfos;
template <BlockPassthrough>
class SingleRowFetcher;
struct Aggregator;

class HashedCollectExecutorInfos : public ExecutorInfos {
 public:
  HashedCollectExecutorInfos(RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
                             std::unordered_set<RegisterId> registersToClear,
                             std::unordered_set<RegisterId> registersToKeep,
                             std::unordered_set<RegisterId>&& readableInputRegisters,
                             std::unordered_set<RegisterId>&& writeableOutputRegisters,
                             std::vector<std::pair<RegisterId, RegisterId>>&& groupRegisters,
                             RegisterId collectRegister, std::vector<std::string>&& aggregateTypes,
                             std::vector<std::pair<RegisterId, RegisterId>>&& aggregateRegisters,
                             transaction::Methods* trxPtr, 
                             arangodb::ResourceMonitor* resourceMonitor, bool count);

  HashedCollectExecutorInfos() = delete;
  HashedCollectExecutorInfos(HashedCollectExecutorInfos&&) = default;
  HashedCollectExecutorInfos(HashedCollectExecutorInfos const&) = delete;
  ~HashedCollectExecutorInfos() = default;

 public:
  std::vector<std::pair<RegisterId, RegisterId>> getGroupRegisters() const;
  std::vector<std::pair<RegisterId, RegisterId>> getAggregatedRegisters() const;
  std::vector<std::string> getAggregateTypes() const;
  bool getCount() const noexcept;
  transaction::Methods* getTransaction() const;
  RegisterId getCollectRegister() const noexcept;
  arangodb::ResourceMonitor* getResourceMonitor() const;

 private:
  /// @brief aggregate types
  std::vector<std::string> _aggregateTypes;

  /// @brief pairs, consisting of out register and in register
  std::vector<std::pair<RegisterId, RegisterId>> _aggregateRegisters;

  /// @brief pairs, consisting of out register and in register
  std::vector<std::pair<RegisterId, RegisterId>> _groupRegisters;

  /// @brief the optional register that contains the values for each group
  /// if no values should be returned, then this has a value of MaxRegisterId
  /// this register is also used for counting in case WITH COUNT INTO var is
  /// used
  RegisterId _collectRegister;

  /// @brief COUNTing node?
  bool _count;

  /// @brief the transaction for this query
  transaction::Methods* _trxPtr;
  
  /// @brief resource manager
  arangodb::ResourceMonitor* _resourceMonitor;
};

/**
 * @brief Implementation of Hashed Collect Executor
 */

class HashedCollectExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = false;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = true;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = HashedCollectExecutorInfos;
  using Stats = NoStats;

  HashedCollectExecutor() = delete;
  HashedCollectExecutor(HashedCollectExecutor&&) = default;
  HashedCollectExecutor(HashedCollectExecutor const&) = delete;
  HashedCollectExecutor(Fetcher& fetcher, Infos&);
  ~HashedCollectExecutor();

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState, and if successful exactly one new Row of AqlItems.
   */
  std::pair<ExecutionState, Stats> produceRows(OutputAqlItemRow& output);

  /**
   * @brief This Executor does not know how many distinct rows will be fetched
   * from upstream, it can only report how many it has found by itself, plus
   * it knows that it can only create as many new rows as pulled from upstream.
   * So it will overestimate.
   */
  std::pair<ExecutionState, size_t> expectedNumberOfRows(size_t atMost) const;

 private:
  using AggregateValuesType = std::vector<std::unique_ptr<Aggregator>>;
  using GroupKeyType = std::vector<AqlValue>;
  using GroupValueType = std::unique_ptr<AggregateValuesType>;
  using GroupMapType =
      std::unordered_map<GroupKeyType, GroupValueType, AqlValueGroupHash, AqlValueGroupEqual>;

  Infos const& infos() const noexcept;

  /**
   * @brief Shall be executed until it returns DONE, then never again.
   * Consumes all input, writes groups and calculates aggregates, and
   * initializes _currentGroup to _allGroups.begin().
   *
   * @return DONE or WAITING
   */
  ExecutionState init();

  void destroyAllGroupsAqlValues();

  static std::vector<std::function<std::unique_ptr<Aggregator>(transaction::Methods*)> const*>
  createAggregatorFactories(HashedCollectExecutor::Infos const& infos);

  GroupMapType::iterator findOrEmplaceGroup(InputAqlItemRow& input);

  void consumeInputRow(InputAqlItemRow& input);

  void writeCurrentGroupToOutput(OutputAqlItemRow& output);

  size_t calculateMemoryUsage(GroupKeyType const& values) const;

 private:
  Infos const& _infos;
  Fetcher& _fetcher;
  ExecutionState _upstreamState;

  /// @brief We need to save any input row (it really doesn't matter, except for
  /// when input blocks are freed - thus the last), so we can produce output
  /// rows later.
  InputAqlItemRow _lastInitializedInputRow;

  /// @brief hashmap of all encountered groups
  GroupMapType _allGroups;
  GroupMapType::iterator _currentGroup;

  bool _isInitialized;  // init() was called successfully (e.g. it returned DONE)

  std::vector<std::function<std::unique_ptr<Aggregator>(transaction::Methods*)> const*> _aggregatorFactories;

  size_t _returnedGroups;

  GroupKeyType _nextGroupValues;
};

}  // namespace aql
}  // namespace arangodb

#endif
