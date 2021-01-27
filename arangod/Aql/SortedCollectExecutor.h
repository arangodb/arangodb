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
#include "Aql/LimitStats.h"
#include "Aql/OutputAqlItemRow.h"
#include "Aql/RegisterInfos.h"
#include "Aql/Stats.h"
#include "Aql/types.h"

#include <velocypack/Builder.h>

#include <memory>

namespace arangodb {
namespace aql {

class InputAqlItemRow;
class RegisterInfos;
template <BlockPassthrough>
class SingleRowFetcher;

class SortedCollectExecutorInfos {
 public:
  SortedCollectExecutorInfos(std::vector<std::pair<RegisterId, RegisterId>>&& groupRegisters,
                             RegisterId collectRegister, RegisterId expressionRegister,
                             Variable const* expressionVariable,
                             std::vector<std::string>&& aggregateTypes,
                             std::vector<std::pair<std::string, RegisterId>>&& variables,
                             std::vector<std::pair<RegisterId, RegisterId>>&& aggregateRegisters,
                             velocypack::Options const*);

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
  velocypack::Options const* getVPackOptions() const { return _vpackOptions; }
  RegisterId getCollectRegister() const noexcept { return _collectRegister; };
  RegisterId getExpressionRegister() const noexcept {
    return _expressionRegister;
  };
  Variable const* getExpressionVariable() const { return _expressionVariable; }

  std::vector<std::pair<std::string, RegisterId>> const& getInputVariables() const {
    return _inputVariables;
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

  std::vector<std::pair<std::string, RegisterId>> _inputVariables;

  /// @brief input expression variable (might be null)
  Variable const* _expressionVariable;
  
  /// @brief the transaction for this query
  velocypack::Options const* _vpackOptions;
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
    Infos& infos;
    InputAqlItemRow _lastInputRow;
    arangodb::velocypack::Buffer<uint8_t> _buffer;
    arangodb::velocypack::Builder _builder;

    CollectGroup() = delete;
    CollectGroup(CollectGroup&&) = default;
    CollectGroup(CollectGroup const&) = delete;
    CollectGroup& operator=(CollectGroup const&) = delete;

    explicit CollectGroup(Infos& infos);
    ~CollectGroup();

    void initialize(size_t capacity);
    void reset(InputAqlItemRow const& input);

    bool isValid() const { return _lastInputRow.isInitialized(); }

    void addLine(InputAqlItemRow const& input);
    bool isSameGroup(InputAqlItemRow const& input) const;
    void groupValuesToArray(velocypack::Builder& builder);
    void writeToOutput(OutputAqlItemRow& output, InputAqlItemRow const& input);
  };

 public:
  struct Properties {
    static constexpr bool preservesOrder = false;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = true;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = SortedCollectExecutorInfos;
  using Stats = NoStats;

  SortedCollectExecutor() = delete;
  SortedCollectExecutor(SortedCollectExecutor&&) = default;
  SortedCollectExecutor(SortedCollectExecutor const&) = delete;
  SortedCollectExecutor(Fetcher& fetcher, Infos&);

  /**
   * @brief produce the next Rows of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to upstream
   */
  [[nodiscard]] auto produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
      -> std::tuple<ExecutorState, Stats, AqlCall>;

  /**
   * @brief skip the next Row of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to upstream
   */
  [[nodiscard]] auto skipRowsRange(AqlItemBlockInputRange& inputRange, AqlCall& call)
      -> std::tuple<ExecutorState, Stats, size_t, AqlCall>;

  /**
   * This executor has no chance to estimate how many rows
   * it will produce exactly. It can however only
   * overestimate never underestimate.
   */
  [[nodiscard]] auto expectedNumberOfRowsNew(AqlItemBlockInputRange const& input,
                                             AqlCall const& call) const noexcept -> size_t;

 private:
  Infos const& infos() const noexcept { return _infos; };

 private:
  Infos const& _infos;

  /// @brief details about the current group
  CollectGroup _currentGroup;

  bool _haveSeenData = false;
};

}  // namespace aql
}  // namespace arangodb

#endif
