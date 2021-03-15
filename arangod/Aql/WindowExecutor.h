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
/// @author Simon Grätzer
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_AQL_WINDOW_EXECUTOR_H
#define ARANGOD_AQL_WINDOW_EXECUTOR_H

#include "Aql/Aggregator.h"
#include "Aql/AqlValueGroup.h"
#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/RegisterInfos.h"
#include "Aql/Stats.h"
#include "Aql/WindowNode.h"
#include "Aql/types.h"

#include <deque>
#include <memory>

namespace arangodb {
namespace aql {

struct AqlCall;
class AqlItemBlockInputRange;
class OutputAqlItemRow;
class RegisterInfos;
template <BlockPassthrough>
class SingleRowFetcher;

class WindowExecutorInfos {
 public:
  /**
   * @brief Construct a new Hashed Collect Executor Infos object
   *
   * @param aggregateTypes Aggregation methods used
   * @param aggregateRegisters Input and output Register for Aggregation
   * @param options The AQL transaction, as it might be needed for aggregates
   */
  WindowExecutorInfos(WindowBounds const& b, RegisterId rangeRegister,
                      std::vector<std::string>&& aggregateTypes,
                      std::vector<std::pair<RegisterId, RegisterId>>&& aggregateRegisters,
                      QueryWarnings& warnings, velocypack::Options const* options);

  WindowExecutorInfos() = delete;
  WindowExecutorInfos(WindowExecutorInfos&&) = default;
  WindowExecutorInfos(WindowExecutorInfos const&) = delete;
  ~WindowExecutorInfos() = default;

 public:
  WindowBounds const& bounds() const;
  RegisterId rangeRegister() const;
  std::vector<std::pair<RegisterId, RegisterId>> getAggregatedRegisters() const;
  std::vector<std::string> getAggregateTypes() const;
  QueryWarnings& warnings() const;
  velocypack::Options const* getVPackOptions() const;

 private:
  WindowBounds const& _bounds;

  RegisterId _rangeRegister;

  /// @brief aggregate types
  std::vector<std::string> _aggregateTypes;

  /// @brief pairs, consisting of out register and in register
  std::vector<std::pair<RegisterId, RegisterId>> _aggregateRegisters;

  QueryWarnings& _warnings;

  /// @brief the transaction for this query
  velocypack::Options const* _vpackOptions;
};

class BaseWindowExecutor {
 public:
  using Infos = WindowExecutorInfos;

  BaseWindowExecutor() = delete;
  BaseWindowExecutor(BaseWindowExecutor&&) = default;
  BaseWindowExecutor(BaseWindowExecutor const&) = delete;
  BaseWindowExecutor(Infos&);
  ~BaseWindowExecutor();

 protected:
  using AggregatorList = std::vector<std::unique_ptr<Aggregator>>;

  Infos const& infos() const noexcept;

  static AggregatorList createAggregators(BaseWindowExecutor::Infos const& infos);

  void applyAggregators(InputAqlItemRow& input);
  void resetAggregators();
  void produceOutputRow(InputAqlItemRow& input, OutputAqlItemRow& output, bool reset);
  void produceInvalidOutputRow(InputAqlItemRow& input, OutputAqlItemRow& output);

 protected:
  Infos const& _infos;
  AggregatorList _aggregators;
};

/**
 * @brief Implementation of Window Executor, accumulates all rows it sees can be passthrough
 */
class AccuWindowExecutor : public BaseWindowExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Enable;
    static constexpr bool inputSizeRestrictsOutputSize = true;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = WindowExecutorInfos;
  using Stats = NoStats;

  AccuWindowExecutor() = delete;
  AccuWindowExecutor(AccuWindowExecutor&&) = default;
  AccuWindowExecutor(AccuWindowExecutor const&) = delete;
  AccuWindowExecutor(Fetcher& fetcher, Infos&);
  ~AccuWindowExecutor();

  
  void initializeCursor();
  
  /**
   * @brief produce the next Row of Aql Values.
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
   * @brief This Executor does not know how many distinct rows will be fetched
   * from upstream, it can only report how many it has found by itself, plus
   * it knows that it can only create as many new rows as pulled from upstream.
   * So it will overestimate.
   */
  [[nodiscard]] auto expectedNumberOfRowsNew(AqlItemBlockInputRange const& input,
                                             AqlCall const& call) const noexcept -> size_t;
};

/**
 * @brief Implementation of Window Executor
 */
class WindowExecutor : public BaseWindowExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = true;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = WindowExecutorInfos;
  using Stats = NoStats;

  WindowExecutor() = delete;
  WindowExecutor(WindowExecutor&&) = default;
  WindowExecutor(WindowExecutor const&) = delete;
  WindowExecutor(Fetcher& fetcher, Infos&);
  ~WindowExecutor();

  /**
   * @brief produce the next Row of Aql Values.
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
   * @brief This Executor does not know how many distinct rows will be fetched
   * from upstream, it can only report how many it has found by itself, plus
   * it knows that it can only create as many new rows as pulled from upstream.
   * So it will overestimate.
   */
  [[nodiscard]] auto expectedNumberOfRowsNew(AqlItemBlockInputRange const& input,
                                             AqlCall const& call) const noexcept -> size_t;

 private:
  ExecutorState consumeInputRange(AqlItemBlockInputRange& input);
  void trimBounds();

 private:
  /// @brief consumed rows that we need to keep track of
  std::deque<InputAqlItemRow> _rows;
  /// @brief cached row value, used for range based windowing
  std::deque<WindowBounds::Row> _windowRows;
  /// @brief index of row we need to copy to output next
  size_t _currentIdx = 0;
};

}  // namespace aql
}  // namespace arangodb

#endif
