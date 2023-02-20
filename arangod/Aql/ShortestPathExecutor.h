////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2023 ArangoDB GmbH, Cologne, Germany
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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/AqlCall.h"
#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/RegisterInfos.h"

#include "GraphNode.h"
#include "Transaction/Helpers.h"

#include <velocypack/Builder.h>

namespace arangodb {

class Result;

namespace velocypack {
class Slice;
}

namespace graph {
class TraverserCache;
}  // namespace graph

namespace aql {

template<BlockPassthrough>
class SingleRowFetcher;
class OutputAqlItemRow;
class TraversalStats;
class NoStats;

template<class FinderType>
class ShortestPathExecutorInfos {
 public:
  enum OutputName { VERTEX, EDGE };
  struct OutputNameHash {
    size_t operator()(OutputName v) const noexcept { return size_t(v); }
  };
  using RegisterMapping =
      std::unordered_map<OutputName, RegisterId, OutputNameHash>;

  ShortestPathExecutorInfos(QueryContext& query,
                            std::unique_ptr<FinderType>&& finder,
                            RegisterMapping&& registerMapping,
                            arangodb::aql::GraphNode::InputVertex&& source,
                            GraphNode::InputVertex&& target);

  ShortestPathExecutorInfos() = delete;

  ShortestPathExecutorInfos(ShortestPathExecutorInfos&&) = default;
  ShortestPathExecutorInfos(ShortestPathExecutorInfos const&) = delete;
  ~ShortestPathExecutorInfos() = default;

  [[nodiscard]] auto finder() const -> FinderType&;

  aql::QueryContext& query() noexcept;

  /**
   * @brief test if we use a register or a constant input
   *
   * @param isTarget defines if we look for target(true) or source(false)
   */
  [[nodiscard]] bool useRegisterForSourceInput() const;
  [[nodiscard]] bool useRegisterForTargetInput() const;

  /**
   * @brief get the register used for the input
   *
   * @param isTarget defines if we look for target(true) or source(false)
   */
  [[nodiscard]] RegisterId getSourceInputRegister() const;
  [[nodiscard]] RegisterId getTargetInputRegister() const;

  /**
   * @brief get the const value for the input
   *
   * @param isTarget defines if we look for target(true) or source(false)
   */
  [[nodiscard]] std::string const& getSourceInputValue() const;
  [[nodiscard]] std::string const& getTargetInputValue() const;

  /**
   * @brief test if we have an output register for this type
   *
   * @param type: Either VERTEX or EDGE
   */
  [[nodiscard]] bool usesOutputRegister(OutputName type) const;

  /**
   * @brief get the output register for the given type
   */
  [[nodiscard]] RegisterId getOutputRegister(OutputName type) const;

  [[nodiscard]] graph::TraverserCache* cache() const;

  [[nodiscard]] GraphNode::InputVertex getSourceVertex() const noexcept;
  [[nodiscard]] GraphNode::InputVertex getTargetVertex() const noexcept;

 private:
  [[nodiscard]] RegisterId findRegisterChecked(OutputName type) const;

 private:
  QueryContext& _query;

  /// @brief the shortest path finder.
  std::unique_ptr<FinderType> _finder;

  /// @brief Mapping outputType => register
  std::unordered_map<OutputName, RegisterId, OutputNameHash> _registerMapping;

  /// @brief Information about the source vertex
  GraphNode::InputVertex _source;

  /// @brief Information about the target vertex
  GraphNode::InputVertex _target;
};

/**
 * @brief Implementation of ShortestPath Node
 */
template<class FinderType>
class ShortestPathExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough =
        BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = false;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = ShortestPathExecutorInfos<FinderType>;
  using Stats = TraversalStats;

  ShortestPathExecutor() = delete;
  ShortestPathExecutor(ShortestPathExecutor&&) = default;

  ShortestPathExecutor(Fetcher& fetcher, Infos&);
  ~ShortestPathExecutor() = default;

  /**
   * @brief produce the next Row of Aql Values.
   */
  [[nodiscard]] auto produceRows(AqlItemBlockInputRange& input,
                                 OutputAqlItemRow& output)
      -> std::tuple<ExecutorState, Stats, AqlCall>;
  [[nodiscard]] auto skipRowsRange(AqlItemBlockInputRange& input, AqlCall& call)
      -> std::tuple<ExecutorState, Stats, size_t, AqlCall>;

 private:
  /**
   *  @brief fetches a path given the current row in input.
   *  a flag indicating whether we found a path, put it into the
   *  internal state.
   */
  [[nodiscard]] auto fetchPath(AqlItemBlockInputRange& input) -> bool;
  [[nodiscard]] auto pathLengthAvailable() -> size_t;

  /**
   *  @brief produce the output from the currently stored path until either
   *  the path is exhausted or there is no output space left.
   */
  auto doOutputPath(OutputAqlItemRow& output) -> void;
  auto doSkipPath(AqlCall& call) -> size_t;

  /**
   * @brief get the id of a input vertex
   * Result will be written into the given Slice.
   * This is either managed by the handed in builder (might be overwritten),
   * or by the handed in row, or a constant value in the options.
   * In any case it will stay valid at least until the reference to the input
   * row is lost, or the builder is resetted.
   */
  [[nodiscard]] auto getVertexId(GraphNode::InputVertex const& vertex,
                                 InputAqlItemRow& row,
                                 arangodb::velocypack::Builder& builder,
                                 arangodb::velocypack::Slice& id) -> bool;

  [[nodiscard]] auto getPathLength() const -> size_t;

 private:
  Infos& _infos;
  transaction::Methods _trx;
  InputAqlItemRow _inputRow;

  /// @brief the shortest path finder.
  FinderType& _finder;

  /// @brief builder we tmp. store the path in
  transaction::BuilderLeaser _pathBuilder;
  /// @brief current computed path.
  size_t _posInPath;
  /// @brief path length based on amount of vertices
  size_t _pathLength;

  /// @brief temporary memory management for source id
  arangodb::velocypack::Builder _sourceBuilder;
  /// @brief temporary memory management for target id
  arangodb::velocypack::Builder _targetBuilder;
};
}  // namespace aql
}  // namespace arangodb
