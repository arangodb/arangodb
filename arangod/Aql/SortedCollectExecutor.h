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

#ifndef ARANGOD_AQL_SORTED_COLLECT_EXECUTOR_H
#define ARANGOD_AQL_SORTED_COLLECT_EXECUTOR_H

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

class SortedCollectExecutorInfos : public ExecutorInfos {
 public:
  SortedCollectExecutorInfos(
      RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
      std::unordered_set<RegisterId> registersToClear,
      std::unordered_set<RegisterId> registersToKeep,
      std::unordered_set<RegisterId>&& readableInputRegisters,
      std::unordered_set<RegisterId>&& writeableOutputRegisters,
      std::vector<std::pair<RegisterId, RegisterId>>&& groupRegisters,
      RegisterId collectRegister, RegisterId expressionRegister,
      Variable const* expressionVariable, std::vector<std::string>&& aggregateTypes,
      std::vector<std::pair<std::string, RegisterId>>&& variables,
      std::vector<std::pair<RegisterId, RegisterId>>&& aggregateRegisters,
      transaction::Methods* trxPtr, bool count);

  SortedCollectExecutorInfos() = delete;
  SortedCollectExecutorInfos(SortedCollectExecutorInfos&&) = default;
  SortedCollectExecutorInfos(SortedCollectExecutorInfos const&) = delete;
  ~SortedCollectExecutorInfos() = default;

 public:
  std::vector<std::pair<RegisterId, RegisterId>> const& getGroupRegisters() const {
    return _groupRegisters;
  }
  std::vector<std::pair<RegisterId, RegisterId>> const& getAggregatedRegisters() const {
    return _aggregateRegisters;
  }
  std::vector<std::string> const& getAggregateTypes() const {
    return _aggregateTypes;
  }
  bool getCount() const noexcept { return _count; };
  transaction::Methods* getTransaction() const { return _trxPtr; }
  RegisterId getCollectRegister() const noexcept { return _collectRegister; };
  RegisterId getExpressionRegister() const noexcept {
    return _expressionRegister;
  };
  Variable const* getExpressionVariable() const { return _expressionVariable; }
  std::vector<std::pair<std::string, RegisterId>> const& getVariables() const {
    return _variables;
  }

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

  /// @brief the optional register that contains the input expression values for
  /// each group
  RegisterId _expressionRegister;

  /// @brief list of variables names for the registers
  std::vector<std::pair<std::string, RegisterId>> _variables;

  /// @brief input expression variable (might be null)
  Variable const* _expressionVariable;

  /// @brief COUNTing node?
  bool _count;

  /// @brief the transaction for this query
  transaction::Methods* _trxPtr;
};

typedef std::vector<std::unique_ptr<Aggregator>> AggregateValuesType;

/**
 * @brief Implementation of Sorted Collect Executor
 */

class SortedCollectExecutor {
 private:
  struct CollectGroup {
    using Infos = SortedCollectExecutorInfos;

    std::vector<AqlValue> groupValues;
    AggregateValuesType aggregators;
    size_t groupLength;
    bool const count;
    Infos& infos;
    InputAqlItemRow _lastInputRow;
    arangodb::velocypack::Builder _builder;
    bool _shouldDeleteBuilderBuffer;

    CollectGroup() = delete;
    CollectGroup(CollectGroup&&) = default;
    CollectGroup(CollectGroup const&) = delete;
    CollectGroup& operator=(CollectGroup const&) = delete;

    explicit CollectGroup(bool count, Infos& infos);
    ~CollectGroup();

    void initialize(size_t capacity);
    void reset(InputAqlItemRow& input);

    bool isValid() const { return _lastInputRow.isInitialized(); }

    void addLine(InputAqlItemRow& input);
    bool isSameGroup(InputAqlItemRow& input);
    void groupValuesToArray(VPackBuilder& builder);
    void writeToOutput(OutputAqlItemRow& output);
  };

 public:
  struct Properties {
    static const bool preservesOrder = false;
    static const bool allowsBlockPassthrough = false;
    static const bool inputSizeRestrictsOutputSize = true;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = SortedCollectExecutorInfos;
  using Stats = NoStats;

  SortedCollectExecutor() = delete;
  SortedCollectExecutor(SortedCollectExecutor&&) = default;
  SortedCollectExecutor(SortedCollectExecutor const&) = delete;
  SortedCollectExecutor(Fetcher& fetcher, Infos&);

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState, and if successful exactly one new Row of AqlItems.
   */
  std::pair<ExecutionState, Stats> produceRows(OutputAqlItemRow& output);

  /**
   * This executor has no chance to estimate how many rows
   * it will produce exactly. It can however only
   * overestimate never underestimate.
   */
  std::pair<ExecutionState, size_t> expectedNumberOfRows(size_t atMost) const;

 private:
  Infos const& infos() const noexcept { return _infos; };

 private:
  Infos const& _infos;

  Fetcher& _fetcher;

  /// @brief details about the current group
  CollectGroup _currentGroup;

  bool _fetcherDone;  // Flag if fetcher is done
};

}  // namespace aql
}  // namespace arangodb

#endif
