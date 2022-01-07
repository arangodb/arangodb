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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/AqlCall.h"
#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/RegisterInfos.h"
#include "Aql/TraversalStats.h"
#include "Aql/Variable.h"
#include "Graph/Enumerators/OneSidedEnumeratorInterface.h"
#include "Graph/Options/OneSidedEnumeratorOptions.h"
#include "Graph/PathManagement/PathValidatorOptions.h"
#include "Graph/Providers/BaseProviderOptions.h"
#include "Graph/Traverser.h"
#include "Graph/TraverserOptions.h"
#include "Graph/Types/UniquenessLevel.h"
#include "Transaction/Methods.h"

namespace arangodb {
class Result;

namespace aql {

class Query;
class QueryWarnings;
class OutputAqlItemRow;
class RegisterInfos;
template<BlockPassthrough>
class SingleRowFetcher;

class TraversalExecutorInfosHelper {
 public:
  enum OutputName { VERTEX, EDGE, PATH };
  struct OutputNameHash {
    size_t operator()(OutputName v) const noexcept { return size_t(v); }
  };
};

class TraversalExecutorInfos {
 public:
  TraversalExecutorInfos(
      std::unique_ptr<traverser::Traverser>&& traverser,
      std::unordered_map<TraversalExecutorInfosHelper::OutputName, RegisterId,
                         TraversalExecutorInfosHelper::OutputNameHash>
          registerMapping,
      std::string fixedSource, RegisterId inputRegister,
      std::vector<std::pair<Variable const*, RegisterId>>
          filterConditionVariables,
      Ast* ast, traverser::TraverserOptions::UniquenessLevel vertexUniqueness,
      traverser::TraverserOptions::UniquenessLevel edgeUniqueness,
      traverser::TraverserOptions::Order order, bool refactor,
      double defaultWeight, std::string const& weightAttribute,
      transaction::Methods* trx, arangodb::aql::QueryContext& query,
      arangodb::graph::BaseProviderOptions&& baseProviderOptions,
      arangodb::graph::PathValidatorOptions&& pathValidatorOptions,
      arangodb::graph::OneSidedEnumeratorOptions&& enumeratorOptions);
  // TODO [GraphRefactor]: Tidy-up input parameter "mess" after refactor is
  // done.
  // TODO [GraphRefactor]: Thinking about a new class / struct for passing /
  // encapsulating those properties.

  TraversalExecutorInfos() = delete;

  TraversalExecutorInfos(TraversalExecutorInfos&&) = default;
  TraversalExecutorInfos(TraversalExecutorInfos const&) = delete;
  ~TraversalExecutorInfos() = default;

  [[nodiscard]] auto traversalEnumerator() const
      -> arangodb::graph::TraversalEnumerator&;
  traverser::Traverser& traverser();

  bool usesOutputRegister(TraversalExecutorInfosHelper::OutputName type) const;

  RegisterId getOutputRegister(
      TraversalExecutorInfosHelper::OutputName type) const;

  bool useVertexOutput() const;

  RegisterId vertexRegister() const;

  bool useEdgeOutput() const;

  RegisterId edgeRegister() const;

  bool usePathOutput() const;

  RegisterId pathRegister() const;

  bool usesFixedSource() const;

  std::string const& getFixedSource() const;

  RegisterId getInputRegister() const;

  std::vector<std::pair<Variable const*, RegisterId>> const&
  filterConditionVariables() const;

  Ast* getAst() const;

  std::pair<arangodb::graph::VertexUniquenessLevel,
            arangodb::graph::EdgeUniquenessLevel>
  convertUniquenessLevels() const;

  auto parseTraversalEnumerator(
      traverser::TraverserOptions::Order order,
      traverser::TraverserOptions::UniquenessLevel uniqueVertices,
      traverser::TraverserOptions::UniquenessLevel uniqueEdges,
      double defaultWeight, std::string const& weightAttribute,
      arangodb::aql::QueryContext& query,
      arangodb::graph::BaseProviderOptions&& baseProviderOptions,
      arangodb::graph::PathValidatorOptions&& pathValidatorOptions,
      arangodb::graph::OneSidedEnumeratorOptions&& enumeratorOptions) -> void;

  traverser::TraverserOptions::UniquenessLevel getUniqueVertices() const;
  traverser::TraverserOptions::UniquenessLevel getUniqueEdges() const;
  traverser::TraverserOptions::Order getOrder() const;
  void setOrder(traverser::TraverserOptions::Order order);
  bool isRefactor() const;
  transaction::Methods* getTrx();
  arangodb::aql::QueryWarnings& getWarnings();

 private:
  std::string typeToString(TraversalExecutorInfosHelper::OutputName type) const;
  RegisterId findRegisterChecked(
      TraversalExecutorInfosHelper::OutputName type) const;

 private:
  std::unique_ptr<traverser::Traverser>
      _traverser;  // TODO [GraphRefactor]: Old way, to be removed after
                   // refactor is done!
  std::unique_ptr<arangodb::graph::TraversalEnumerator> _traversalEnumerator =
      nullptr;

  std::unordered_map<TraversalExecutorInfosHelper::OutputName, RegisterId,
                     TraversalExecutorInfosHelper::OutputNameHash>
      _registerMapping;
  std::string _fixedSource;
  RegisterId _inputRegister;
  std::vector<std::pair<Variable const*, RegisterId>> _filterConditionVariables;
  Ast* _ast;
  traverser::TraverserOptions::UniquenessLevel _uniqueVertices;
  traverser::TraverserOptions::UniquenessLevel _uniqueEdges;
  traverser::TraverserOptions::Order _order;
  bool _refactor;
  double _defaultWeight;
  std::string _weightAttribute;
  transaction::Methods* _trx;
  arangodb::aql::QueryContext& _query;
};

/**
 * @brief Implementation of Traversal Node
 */
class TraversalExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough =
        BlockPassthrough::Disable;
    static constexpr bool inputSizeRestrictsOutputSize = false;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = TraversalExecutorInfos;
  using Stats = TraversalStats;

  TraversalExecutor() = delete;
  TraversalExecutor(TraversalExecutor&&) = default;
  TraversalExecutor(TraversalExecutor const&) = default;
  TraversalExecutor(Fetcher& fetcher, Infos&);
  ~TraversalExecutor();

  [[nodiscard]] auto produceRows(AqlItemBlockInputRange& input,
                                 OutputAqlItemRow& output)
      -> std::tuple<ExecutorState, Stats, AqlCall>;
  [[nodiscard]] auto skipRowsRange(AqlItemBlockInputRange& input, AqlCall& call)
      -> std::tuple<ExecutorState, Stats, size_t, AqlCall>;

 private:
  auto doOutput(OutputAqlItemRow& output) -> void;
  [[nodiscard]] auto doSkip(AqlCall& call) -> size_t;

  [[nodiscard]] bool initTraverser(AqlItemBlockInputRange& input);

  [[nodiscard]] auto stats() -> Stats;

 private:
  Infos& _infos;
  InputAqlItemRow _inputRow;

  traverser::Traverser& _traverser;  // TODO [GraphRefactor]: Old way, to be
                                     // removed after refactor is done!

  /// @brief the refactored finder variant.
  arangodb::graph::TraversalEnumerator& _traversalEnumerator;
};

}  // namespace aql
}  // namespace arangodb
