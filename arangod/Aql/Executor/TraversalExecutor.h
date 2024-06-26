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
/// @author Michael Hackstein
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/AqlCall.h"
#include "Aql/AqlItemBlockInputRange.h"
#include "Aql/Ast.h"
#include "Aql/ExecutionState.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/RegisterInfos.h"
#include "Aql/TraversalStats.h"
#include "Aql/Variable.h"
#include "Graph/Enumerators/OneSidedEnumeratorInterface.h"
#include "Graph/Options/OneSidedEnumeratorOptions.h"
#include "Graph/PathManagement/PathValidatorOptions.h"
#include "Graph/Providers/BaseProviderOptions.h"
#include "Graph/TraverserOptions.h"
#include "Graph/Types/UniquenessLevel.h"

namespace arangodb {
class Result;

namespace transaction {
class Methods;
}
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
  // for the coordinator
  TraversalExecutorInfos(
      std::unordered_map<TraversalExecutorInfosHelper::OutputName, RegisterId,
                         TraversalExecutorInfosHelper::OutputNameHash>
          registerMapping,
      std::string fixedSource, RegisterId inputRegister, Ast* ast,
      traverser::TraverserOptions::UniquenessLevel vertexUniqueness,
      traverser::TraverserOptions::UniquenessLevel edgeUniqueness,
      traverser::TraverserOptions::Order order, double defaultWeight,
      std::string weightAttribute, arangodb::aql::QueryContext& query,
      arangodb::graph::PathValidatorOptions&& pathValidatorOptions,
      arangodb::graph::OneSidedEnumeratorOptions&& enumeratorOptions,
      graph::ClusterBaseProviderOptions&& clusterBaseProviderOptions,
      bool isSmart = false);
  // TODO [GraphRefactor]: Tidy-up input parameter "mess" after refactor is
  // done.
  // TODO [GraphRefactor]: Thinking about a new class / struct for passing /
  // encapsulating those properties.

  // for non-coordinator (single server or DB-server)
  TraversalExecutorInfos(
      std::unordered_map<TraversalExecutorInfosHelper::OutputName, RegisterId,
                         TraversalExecutorInfosHelper::OutputNameHash>
          registerMapping,
      std::string fixedSource, RegisterId inputRegister, Ast* ast,
      traverser::TraverserOptions::UniquenessLevel vertexUniqueness,
      traverser::TraverserOptions::UniquenessLevel edgeUniqueness,
      traverser::TraverserOptions::Order order, double defaultWeight,
      std::string weightAttribute, arangodb::aql::QueryContext& query,
      arangodb::graph::PathValidatorOptions&& pathValidatorOptions,
      arangodb::graph::OneSidedEnumeratorOptions&& enumeratorOptions,
      graph::SingleServerBaseProviderOptions&& singleServerBaseProviderOptions,
      bool isSmart = false);

  TraversalExecutorInfos() = delete;

  TraversalExecutorInfos(TraversalExecutorInfos&&) = default;
  TraversalExecutorInfos(TraversalExecutorInfos const&) = delete;
  ~TraversalExecutorInfos() = default;

  [[nodiscard]] auto traversalEnumerator() const
      -> arangodb::graph::TraversalEnumerator*;

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

  auto parseTraversalEnumeratorSingleServer(
      traverser::TraverserOptions::Order order,
      traverser::TraverserOptions::UniquenessLevel uniqueVertices,
      traverser::TraverserOptions::UniquenessLevel uniqueEdges,
      double defaultWeight, std::string const& weightAttribute,
      arangodb::aql::QueryContext& query,
      arangodb::graph::SingleServerBaseProviderOptions&& baseProviderOptions,
      arangodb::graph::PathValidatorOptions&& pathValidatorOptions,
      arangodb::graph::OneSidedEnumeratorOptions&& enumeratorOptions,
      bool isSmart = false) -> void;

  auto parseTraversalEnumeratorCluster(
      traverser::TraverserOptions::Order order,
      traverser::TraverserOptions::UniquenessLevel uniqueVertices,
      traverser::TraverserOptions::UniquenessLevel uniqueEdges,
      double defaultWeight, std::string const& weightAttribute,
      arangodb::aql::QueryContext& query,
      arangodb::graph::ClusterBaseProviderOptions&& baseProviderOptions,
      arangodb::graph::PathValidatorOptions&& pathValidatorOptions,
      arangodb::graph::OneSidedEnumeratorOptions&& enumeratorOptions,
      bool isSmart = false) -> void;

  traverser::TraverserOptions::UniquenessLevel getUniqueVertices() const;
  traverser::TraverserOptions::UniquenessLevel getUniqueEdges() const;
  traverser::TraverserOptions::Order getOrder() const;
  transaction::Methods* getTrx();
  arangodb::aql::QueryContext& getQuery();
  arangodb::aql::QueryWarnings& getWarnings();

 private:
  static std::string typeToString(
      TraversalExecutorInfosHelper::OutputName type);
  RegisterId findRegisterChecked(
      TraversalExecutorInfosHelper::OutputName type) const;

 private:
  std::unique_ptr<arangodb::graph::TraversalEnumerator> _traversalEnumerator =
      nullptr;

  std::unordered_map<TraversalExecutorInfosHelper::OutputName, RegisterId,
                     TraversalExecutorInfosHelper::OutputNameHash>
      _registerMapping;
  std::string _fixedSource;
  RegisterId _inputRegister;
  traverser::TraverserOptions::UniquenessLevel _uniqueVertices;
  traverser::TraverserOptions::UniquenessLevel _uniqueEdges;
  traverser::TraverserOptions::Order _order;
  double _defaultWeight;
  std::string _weightAttribute;
  arangodb::aql::QueryContext& _query;
  transaction::Methods _trx;
};

/**
 * @brief Implementation of traversal executor
 */
class TraversalExecutor {
 public:
  struct Properties {
    static constexpr bool preservesOrder = true;
    static constexpr BlockPassthrough allowsBlockPassthrough =
        BlockPassthrough::Disable;
  };
  using Fetcher = SingleRowFetcher<Properties::allowsBlockPassthrough>;
  using Infos = TraversalExecutorInfos;
  using Stats = TraversalStats;

  TraversalExecutor() = delete;
  TraversalExecutor(TraversalExecutor&&) = delete;
  TraversalExecutor(TraversalExecutor const&) = delete;
  TraversalExecutor(Fetcher& fetcher, Infos&);
  ~TraversalExecutor();

  [[nodiscard]] auto produceRows(AqlItemBlockInputRange& input,
                                 OutputAqlItemRow& output)
      -> std::tuple<ExecutorState, Stats, AqlCall>;
  [[nodiscard]] auto skipRowsRange(AqlItemBlockInputRange& input, AqlCall& call)
      -> std::tuple<ExecutorState, Stats, size_t, AqlCall>;

 private:
  auto doOutput(OutputAqlItemRow& output) -> void;

  [[nodiscard]] bool initTraverser(AqlItemBlockInputRange& input);

  [[nodiscard]] auto stats() -> Stats;

  auto traversalEnumerator() -> arangodb::graph::TraversalEnumerator*;

  Infos& _infos;

  // an AST owned by the TraversalExecutor, used to store data of index
  // expressions
  Ast _ast;
  InputAqlItemRow _inputRow;

  /// @brief the finder variant.
  arangodb::graph::TraversalEnumerator* _traversalEnumerator;
};

}  // namespace aql
}  // namespace arangodb
