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

#include "Aql/ExecutionState.h"
#include "Aql/ExecutorInfos.h"
#include "Aql/InputAqlItemRow.h"

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

template <bool>
class SingleRowFetcher;
class OutputAqlItemRow;
class NoStats;

class KShortestPathsExecutorInfos : public ExecutorInfos {
 public:
  struct InputVertex {
    enum { CONSTANT, REGISTER } type;
    // TODO make the following two a union instead
    RegisterId reg;
    std::string value;

    // cppcheck-suppress passedByValue
    explicit InputVertex(std::string value)
        : type(CONSTANT), reg(0), value(std::move(value)) {}
    explicit InputVertex(RegisterId reg)
        : type(REGISTER), reg(reg), value("") {}
  };

  KShortestPathsExecutorInfos(std::shared_ptr<std::unordered_set<RegisterId>> inputRegisters,
                              std::shared_ptr<std::unordered_set<RegisterId>> outputRegisters,
                              RegisterId nrInputRegisters, RegisterId nrOutputRegisters,
                              std::unordered_set<RegisterId> registersToClear,
                              std::unordered_set<RegisterId> registersToKeep,
                              std::unique_ptr<graph::KShortestPathsFinder>&& finder,
                              InputVertex&& source, InputVertex&& target);

  KShortestPathsExecutorInfos() = delete;

  KShortestPathsExecutorInfos(KShortestPathsExecutorInfos&&);
  KShortestPathsExecutorInfos(KShortestPathsExecutorInfos const&) = delete;
  ~KShortestPathsExecutorInfos();

  arangodb::graph::KShortestPathsFinder& finder() const;

  /**
   * @brief test if we use a register or a constant input
   *
   * @param isTarget defines if we look for target(true) or source(false)
   */
  bool useRegisterForInput(bool isTarget) const;

  /**
   * @brief get the register used for the input
   *
   * @param isTarget defines if we look for target(true) or source(false)
   */
  RegisterId getInputRegister(bool isTarget) const;

  /**
   * @brief get the const value for the input
   *
   * @param isTarget defines if we look for target(true) or source(false)
   */
  std::string const& getInputValue(bool isTarget) const;

  /**
   * @brief get the output register for the given type
   */
  RegisterId getOutputRegister() const;

  graph::TraverserCache* cache() const;

 private:
  /// @brief the shortest path finder.
  std::unique_ptr<arangodb::graph::KShortestPathsFinder> _finder;

  /// @brief Information about the source vertex
  InputVertex const _source;

  /// @brief Information about the target vertex
  InputVertex const _target;

  RegisterId const _outputRegister;
};

/**
 * @brief Implementation of ShortestPath Node
 */
class KShortestPathsExecutor {
 public:
  struct Properties {
    static const bool preservesOrder = true;
    static const bool allowsBlockPassthrough = false;
    static const bool inputSizeRestrictsOutputSize = false;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = KShortestPathsExecutorInfos;
  using Stats = NoStats;

  KShortestPathsExecutor() = delete;
  KShortestPathsExecutor(KShortestPathsExecutor&&) = default;

  KShortestPathsExecutor(Fetcher& fetcher, Infos&);
  ~KShortestPathsExecutor();

  /**
   * @brief Shutdown will be called once for every query
   *
   * @return ExecutionState and no error.
   */
  std::pair<ExecutionState, Result> shutdown(int errorCode);

  /**
   * @brief produce the next Row of Aql Values.
   *
   * @return ExecutionState, and if successful exactly one new Row of AqlItems.
   */
  std::pair<ExecutionState, Stats> produceRow(OutputAqlItemRow& output);
  inline std::pair<ExecutionState, size_t> expectedNumberOfRows(size_t) const {
    TRI_ASSERT(false);
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_INTERNAL,
        "Logic_error, prefetching number fo rows not supported");
  }

 private:
  /**
   * @brief Fetch input row(s) and compute path
   *
   * @return false if we are done and no path could be found.
   */
  bool fetchPaths();

  /**
   * @brief compute the correct return state
   *
   * @return DONE if no more is expected
   */

  ExecutionState computeState() const;

  /**
   * @brief get the id of a input vertex
   */
  bool getVertexId(bool isTarget, arangodb::velocypack::Slice& id);

 private:
  Infos& _infos;
  Fetcher& _fetcher;
  InputAqlItemRow _input;
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
