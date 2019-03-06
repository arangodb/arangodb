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

#include "Aql/Aggregator.h"
#include "Aql/AqlValueGroup.h"
#include "Aql/ExecutionBlock.h"
#include "Aql/ExecutionBlockImpl.h"
#include "Aql/ExecutionNode.h"
#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/LimitStats.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/types.h"

#include <memory>

namespace arangodb {
namespace aql {

class InputAqlItemRow;
class ExecutorInfos;
template <bool>
class SingleRowFetcher;

class HashedCollectExecutorInfos : public ExecutorInfos {
 public:
  HashedCollectExecutorInfos(
      RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
      std::unordered_set<RegisterId> registersToClear,
      std::unordered_set<RegisterId> registersToKeep,
      std::unordered_set<RegisterId>&& readableInputRegisters,
      std::unordered_set<RegisterId>&& writeableOutputRegisters,
      std::vector<std::pair<RegisterId, RegisterId>>&& groupRegisters, RegisterId collectRegister,
      std::vector<std::pair<Variable const*, std::pair<Variable const*, std::string>>> const& aggregateVariables,
      std::vector<std::pair<RegisterId, RegisterId>>&& aggregateRegisters,
      std::vector<std::pair<Variable const*, Variable const*>> groupVariables,
      transaction::Methods* trxPtr, bool count);

  HashedCollectExecutorInfos() = delete;
  HashedCollectExecutorInfos(HashedCollectExecutorInfos&&) = default;
  HashedCollectExecutorInfos(HashedCollectExecutorInfos const&) = delete;
  ~HashedCollectExecutorInfos() = default;

 public:
  std::vector<std::pair<RegisterId, RegisterId>> getGroupRegisters() const {
    return _groupRegisters;
  }
  std::vector<std::pair<Variable const*, Variable const*>> getGroupVariables() const {
    return _groupVariables;
  }
  std::vector<std::pair<RegisterId, RegisterId>> getAggregatedRegisters() const {
    return _aggregateRegisters;
  }
  std::vector<std::pair<Variable const*, std::pair<Variable const*, std::string>>> getAggregateVariables() const {
    return _aggregateVariables;
  }
  bool getCount() const noexcept { return _count; };
  transaction::Methods* getTransaction() const { return _trxPtr; }
  RegisterId getInputRegister() const noexcept { return _inputRegister; };
  RegisterId getCollectRegister() const noexcept { return _collectRegister; };

 private:
  // This is exactly the value in the parent member ExecutorInfo::_inRegs,
  // respectively getInputRegisters().
  RegisterId _inputRegister;

  /// @brief input/output variables for the aggregation (out, in)
  std::vector<std::pair<Variable const*, std::pair<Variable const*, std::string>>> _aggregateVariables;

  /// @brief pairs, consisting of out register and in register
  std::vector<std::pair<RegisterId, RegisterId>> _aggregateRegisters;

  /// @brief pairs, consisting of out register and in register
  std::vector<std::pair<RegisterId, RegisterId>> _groupRegisters;

  /// @brief the optional register that contains the values for each group
  /// if no values should be returned, then this has a value of MaxRegisterId
  /// this register is also used for counting in case WITH COUNT INTO var is
  /// used
  RegisterId _collectRegister;

  /// @brief input/output variables for the collection (out, in)
  std::vector<std::pair<Variable const*, Variable const*>> _groupVariables;

  /// @brief COUNTing node?
  bool _count;

  /// @brief the transaction for this query
  transaction::Methods* _trxPtr;
};

/**
 * @brief Implementation of Hashed Collect Executor
 */

typedef std::vector<std::unique_ptr<Aggregator>> AggregateValuesType;

class HashedCollectExecutor {
 public:
  struct Properties {
    static const bool preservesOrder = false;
    static const bool allowsBlockPassthrough = false;
    static const bool inputSizeRestrictsOutputSize = true;
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
  std::pair<ExecutionState, Stats> produceRow(OutputAqlItemRow& output);

 private:
  Infos const& infos() const noexcept { return _infos; };

  void _destroyAllGroupsAqlValues();

 private:
  Infos const& _infos;
  Fetcher& _fetcher;
  ExecutionState _state;
  InputAqlItemRow _input;

  /// @brief hashmap of all encountered groups
  std::unordered_map<std::vector<AqlValue>, std::unique_ptr<AggregateValuesType>, AqlValueGroupHash, AqlValueGroupEqual> _allGroups;
  std::unordered_map<std::vector<AqlValue>, std::unique_ptr<AggregateValuesType>,
                     AqlValueGroupHash, AqlValueGroupEqual>::iterator _currentGroup;
  bool _done;  // wrote last output row
  bool _init;  // done aggregating groups
};

}  // namespace aql
}  // namespace arangodb

#endif
