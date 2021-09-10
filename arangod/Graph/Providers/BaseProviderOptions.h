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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#pragma once

#include "Aql/Expression.h"
#include "Aql/FixedVarExpressionContext.h"
#include "Cluster/ClusterInfo.h"
#include "Graph/Cache/RefactoredClusterTraverserCache.h"
#include "Transaction/Methods.h"

#include <optional>
#include <vector>

namespace arangodb {

namespace aql {
class QueryContext;
}

namespace graph {

struct IndexAccessor {
  IndexAccessor(transaction::Methods::IndexHandle idx, aql::AstNode* condition,
                std::optional<size_t> memberToUpdate,
                std::unique_ptr<arangodb::aql::Expression> expression, size_t cursorId);
  IndexAccessor(IndexAccessor const&) = delete;
  IndexAccessor(IndexAccessor&&) = default;

  aql::AstNode* getCondition() const;
  aql::Expression* getExpression() const;
  transaction::Methods::IndexHandle indexHandle() const;
  std::optional<size_t> getMemberToUpdate() const;
  size_t cursorId() const;

 private:
  transaction::Methods::IndexHandle _idx;
  aql::AstNode* _indexCondition;
  std::optional<size_t> _memberToUpdate;

  // Note: We would prefer to have this a unique_ptr here.
  // However the IndexAccessor is used in std::vector<IndexAccessor>
  // which then refuses to compile (deleted copy constructor)
  std::unique_ptr<arangodb::aql::Expression> _expression;
  size_t _cursorId;
};

struct BaseProviderOptions {
  using WeightCallback =
      std::function<double(double originalWeight, arangodb::velocypack::Slice edge)>;

 public:
  BaseProviderOptions(
      aql::Variable const* tmpVar,
      std::pair<std::vector<IndexAccessor>, std::unordered_map<uint64_t, std::vector<IndexAccessor>>>&& indexInfo,
      aql::FixedVarExpressionContext& expressionContext,
      std::unordered_map<std::string, std::vector<std::string>> const& collectionToShardMap);

  BaseProviderOptions(BaseProviderOptions const&) = delete;
  BaseProviderOptions(BaseProviderOptions&&) = default;

  aql::Variable const* tmpVar() const;
  std::pair<std::vector<IndexAccessor>, std::unordered_map<uint64_t, std::vector<IndexAccessor>>> const& indexInformations() const;

  std::unordered_map<std::string, std::vector<std::string>> const& collectionToShardMap() const;

  aql::FixedVarExpressionContext& expressionContext() const;

  bool hasWeightMethod() const;

  double weightEdge(double prefixWeight, arangodb::velocypack::Slice edge) const;

  void setWeightEdgeCallback(WeightCallback callback);

 private:
  // The temporary Variable used in the Indexes
  aql::Variable const* _temporaryVariable;
  // One entry per collection, ShardTranslation needs
  // to be done by Provider
  std::pair<std::vector<IndexAccessor>, std::unordered_map<uint64_t, std::vector<IndexAccessor>>> _indexInformation;

  // The context of AQL variables. These variables are set from the outside.
  // and the caller needs to make sure the reference stays valid
  aql::FixedVarExpressionContext& _expressionContext;

  // CollectionName to ShardMap, used if the Traversal is pushed down to DBServer
  std::unordered_map<std::string, std::vector<std::string>> const& _collectionToShardMap;

  // Optional callback to compute the weight of an edge.
  std::optional<WeightCallback> _weightCallback;
};

struct ClusterBaseProviderOptions {
 public:
  ClusterBaseProviderOptions(std::shared_ptr<RefactoredClusterTraverserCache> cache,
                             std::unordered_map<ServerID, aql::EngineId> const* engines,
                             bool backward);

  RefactoredClusterTraverserCache* getCache();

  bool isBackward() const;

  [[nodiscard]] std::unordered_map<ServerID, aql::EngineId> const* engines() const;

 private:
  std::shared_ptr<RefactoredClusterTraverserCache> _cache;

  std::unordered_map<ServerID, aql::EngineId> const* _engines;

  bool _backward;
};

}  // namespace graph
}  // namespace arangodb
