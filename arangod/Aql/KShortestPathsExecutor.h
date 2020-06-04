////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2019 ArangoDB GmbH, Cologne, Germany
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

#ifndef ARANGOD_AQL_KSHORTEST_PATHS_EXECUTOR_H
#define ARANGOD_AQL_KSHORTEST_PATHS_EXECUTOR_H

#include "Aql/AqlCall.h"
#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/RegisterInfos.h"
#include "Graph/KShortestPathsFinder.h"

#include <velocypack/Builder.h>

namespace arangodb {

class Result;

namespace velocypack {
class Slice;
}

namespace graph {
class KShortestPathsFinder;
class ShortestPathFinder;
class ShortestPathResult;
class TraverserCache;
}  // namespace graph

namespace aql {

template <BlockPassthrough>
class SingleRowFetcher;
class OutputAqlItemRow;
class NoStats;

class KShortestPathsExecutorInfos {
 public:
  struct InputVertex {
    enum class Type { CONSTANT, REGISTER };
    Type type;
    // TODO make the following two a union instead
    RegisterId reg;
    std::string value;

    // cppcheck-suppress passedByValue
    explicit InputVertex(std::string value)
        : type(Type::CONSTANT), reg(0), value(std::move(value)) {}
    explicit InputVertex(RegisterId reg)
        : type(Type::REGISTER), reg(reg), value("") {}
  };

  KShortestPathsExecutorInfos(RegisterId outputRegister,
                              std::unique_ptr<graph::KShortestPathsFinder>&& finder,
                              InputVertex&& source, InputVertex&& target);

  KShortestPathsExecutorInfos() = delete;

  KShortestPathsExecutorInfos(KShortestPathsExecutorInfos&&) = default;
  KShortestPathsExecutorInfos(KShortestPathsExecutorInfos const&) = delete;
  ~KShortestPathsExecutorInfos() = default;

  [[nodiscard]] auto finder() const -> arangodb::graph::KShortestPathsFinder&;

  /**
   * @brief test if we use a register or a constant input
   *
   * @param isTarget defines if we look for target(true) or source(false)
   */
  [[nodiscard]] auto useRegisterForSourceInput() const -> bool;
  [[nodiscard]] auto useRegisterForTargetInput() const -> bool;

  /**
   * @brief get the register used for the input
   *
   * @param isTarget defines if we look for target(true) or source(false)
   */
  [[nodiscard]] auto getSourceInputRegister() const -> RegisterId;
  [[nodiscard]] auto getTargetInputRegister() const -> RegisterId;

  /**
   * @brief get the const value for the input
   *
   * @param isTarget defines if we look for target(true) or source(false)
   */
  [[nodiscard]] auto getSourceInputValue() const -> std::string const&;
  [[nodiscard]] auto getTargetInputValue() const -> std::string const&;

  /**
   * @brief get the output register for the given type
   */
  [[nodiscard]] auto getOutputRegister() const -> RegisterId;

  [[nodiscard]] auto cache() const -> graph::TraverserCache*;

  [[nodiscard]] auto getSourceVertex() const noexcept -> InputVertex;
  [[nodiscard]] auto getTargetVertex() const noexcept -> InputVertex;

 private:
  /// @brief the shortest path finder.
  std::unique_ptr<arangodb::graph::KShortestPathsFinder> _finder;

  /// @brief Information about the source vertex
  InputVertex _source;

  /// @brief Information about the target vertex
  InputVertex _target;

  RegisterId _outputRegister;
};

/**
 * @brief Implementation of ShortestPath Node
 */
class KShortestPathsExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough = BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = false;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = KShortestPathsExecutorInfos;
  using Stats = NoStats;

  KShortestPathsExecutor() = delete;
  KShortestPathsExecutor(KShortestPathsExecutor&&) = default;

  KShortestPathsExecutor(Fetcher& fetcher, Infos&);
  ~KShortestPathsExecutor() = default;

  /**
   * @brief Shutdown will be called once for every query
   *
   * @return ExecutionState and no error.
   */
  [[nodiscard]] auto shutdown(int errorCode) -> std::pair<ExecutionState, Result>;

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState, and if successful exactly one new Row of AqlItems.
   */
  [[nodiscard]] auto produceRows(AqlItemBlockInputRange& input, OutputAqlItemRow& output)
      -> std::tuple<ExecutorState, Stats, AqlCall>;
  [[nodiscard]] auto skipRowsRange(AqlItemBlockInputRange& input, AqlCall& call)
      -> std::tuple<ExecutorState, Stats, size_t, AqlCall>;

 private:
  /**
   * @brief Fetch input row(s) and compute path
   *
   * @return false if we are done and no path could be found.
   */
  [[nodiscard]] auto fetchPaths(AqlItemBlockInputRange& input) -> bool;
  auto doOutputPath(OutputAqlItemRow& output) -> void;

  /**
   * @brief get the id of a input vertex
   */
  [[nodiscard]] auto getVertexId(bool isTarget, arangodb::velocypack::Slice& id) -> bool;

  [[nodiscard]] auto getVertexId(KShortestPathsExecutorInfos::InputVertex const& vertex,
                                 InputAqlItemRow& row, arangodb::velocypack::Builder& builder, arangodb::velocypack::Slice& id) -> bool;

 private:
  Infos& _infos;
  InputAqlItemRow _inputRow;
  ExecutionState _rowState;
  /// @brief the shortest path finder.
  arangodb::graph::KShortestPathsFinder& _finder;

  /// @brief temporary memory mangement for source id
  arangodb::velocypack::Builder _sourceBuilder;
  /// @brief temporary memory mangement for target id
  arangodb::velocypack::Builder _targetBuilder;
};
}  // namespace aql
}  // namespace arangodb

#endif
