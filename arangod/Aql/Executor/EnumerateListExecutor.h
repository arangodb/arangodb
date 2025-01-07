////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2024 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Business Source License 1.1 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     https://github.com/arangodb/arangodb/blob/devel/LICENSE
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

#pragma once

#include "Aql/AqlFunctionsInternalCache.h"
#include "Aql/AqlValue.h"
#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/RegisterInfos.h"
#include "Aql/SingleRowFetcher.h"
#include "Aql/Stats.h"
#include "Aql/Variable.h"
#include "Aql/types.h"
#include "Transaction/Methods.h"
#include "Aql/ExecutionNode/EnumerateListNode.h"

#include <memory>
#include <utility>
#include <vector>

namespace arangodb::aql {

struct AqlCall;
class AqlItemBlockInputRange;
class EnumerateListExpressionContext;
class Expression;
class OutputAqlItemRow;
class QueryContext;
class RegisterInfos;
template<BlockPassthrough>
class SingleRowFetcher;

class EnumerateListExecutorInfos {
 public:
  EnumerateListExecutorInfos(
      RegisterId inputRegister, const std::vector<RegisterId>&& outputRegisters,
      QueryContext& query, Expression* filter,
      std::vector<const Variable*>&& outputVariables,
      std::vector<std::pair<VariableId, RegisterId>>&& varsToRegs,
      EnumerateListNode::Mode mode = EnumerateListNode::Mode::kEnumerateArray);

  EnumerateListExecutorInfos() = delete;
  EnumerateListExecutorInfos(EnumerateListExecutorInfos&&) = default;
  EnumerateListExecutorInfos(EnumerateListExecutorInfos const&) = delete;
  ~EnumerateListExecutorInfos() = default;

  QueryContext& getQuery() const noexcept;
  RegisterId getInputRegister() const noexcept;
  const std::vector<RegisterId>& getOutputRegister() const noexcept;
  std::vector<const Variable*> const& getOutputVariables() const noexcept;
  bool hasFilter() const noexcept;
  Expression* getFilter() const noexcept;
  std::vector<std::pair<VariableId, RegisterId>> const& getVarsToRegs()
      const noexcept;

  EnumerateListNode::Mode getMode() const noexcept;

 private:
  QueryContext& _query;
  // These two are exactly the values in the parent members
  // ExecutorInfo::_inRegs and ExecutorInfo::_outRegs, respectively
  // getInputRegisters() and getOutputRegisters().
  RegisterId const _inputRegister;
  std::vector<RegisterId> const _outputRegisters;
  std::vector<const Variable*> _outputVariables;
  Expression* _filter;
  // Input variable and register pairs required for the filter
  std::vector<std::pair<VariableId, RegisterId>> _varsToRegs;
  EnumerateListNode::Mode _mode;
};

/**
 * @brief Implementation of EnumerateList Node
 */
class EnumerateListExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough =
        BlockPassthrough::Disable;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = EnumerateListExecutorInfos;
  using Stats = FilterStats;

  EnumerateListExecutor(Fetcher&, EnumerateListExecutorInfos&);
  ~EnumerateListExecutor();

  /**
   * @brief Will fetch a new InputRow if necessary and store their local state
   *
   * @return bool done in case we do not have any input and upstreamState is
   * done
   */
  void initializeNewRow(AqlItemBlockInputRange& inputRange);

  /**
   * @brief Will process a found array element
   */
  bool processArrayElement(OutputAqlItemRow& output);

  /**
   * @brief Will skip a maximum of n-elements inside the current array
   */
  size_t skipArrayElement(size_t skip);

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to
   * upstream
   */
  [[nodiscard]] std::tuple<ExecutorState, Stats, AqlCall> produceRows(
      AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output);

  /**
   * @brief skip the next Row of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to
   * upstream
   */
  [[nodiscard]] std::tuple<ExecutorState, Stats, size_t, AqlCall> skipRowsRange(
      AqlItemBlockInputRange& inputRange, AqlCall& call);

 private:
  AqlValue getAqlValue(AqlValue const& inVarReg, size_t const& pos,
                       bool& mustDestroy);

  bool checkFilter(AqlValue const& currentValue);

  EnumerateListExecutorInfos& _infos;
  transaction::Methods _trx;
  aql::AqlFunctionsInternalCache _aqlFunctionsInternalCache;
  InputAqlItemRow _currentRow;
  ExecutorState _currentRowState;
  size_t _inputArrayPosition;
  size_t _inputArrayLength;
  std::unique_ptr<EnumerateListExpressionContext> _expressionContext;

  // note: it is fine if this counter overflows
  uint_fast16_t _killCheckCounter = 0;
};

class EnumerateListObjectExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough =
        BlockPassthrough::Disable;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = EnumerateListExecutorInfos;
  using Stats = FilterStats;

  EnumerateListObjectExecutor(Fetcher&, EnumerateListExecutorInfos&);
  ~EnumerateListObjectExecutor();

  /**
   * @brief Will fetch a new InputRow if necessary and store their local state
   *
   * @return bool done in case we do not have any input and upstreamState is
   * done
   */
  void initializeNewRow(AqlItemBlockInputRange& inputRange);

  /**
   * @brief Will process a found array element
   */
  bool processElement(OutputAqlItemRow& output);

  /**
   * @brief Will skip a maximum of n-elements inside the current array
   */
  size_t skipElement(size_t skip);

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to
   * upstream
   */
  [[nodiscard]] std::tuple<ExecutorState, Stats, AqlCall> produceRows(
      AqlItemBlockInputRange& inputRange, OutputAqlItemRow& output);

  /**
   * @brief skip the next Row of Aql Values.
   *
   * @return ExecutorState, the stats, and a new Call that needs to be send to
   * upstream
   */
  [[nodiscard]] std::tuple<ExecutorState, Stats, size_t, AqlCall> skipRowsRange(
      AqlItemBlockInputRange& inputRange, AqlCall& call);

 private:
  AqlValue getAqlValue(AqlValue const& inVarReg, size_t const& pos,
                       bool& mustDestroy);

  bool checkFilter(AqlValue const& currentK, AqlValue const& currentV);

  std::tuple<AqlValue, AqlValue> keyValueExtractor(
      velocypack::ObjectIteratorPair innerValue);

  EnumerateListExecutorInfos& _infos;
  transaction::Methods _trx;
  aql::AqlFunctionsInternalCache _aqlFunctionsInternalCache;
  InputAqlItemRow _currentRow;
  ExecutorState _currentRowState;
  VPackObjectIterator _objIterator;
  std::unique_ptr<EnumerateListExpressionContext> _expressionContext;

  // note: it is fine if this counter overflows
  uint_fast16_t _killCheckCounter = 0;
};

}  // namespace arangodb::aql
