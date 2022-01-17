////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2022 ArangoDB GmbH, Cologne, Germany
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

#include "Graph/Providers/BaseProviderOptions.h"
#include "Aql/NonConstExpression.h"
#include "Aql/NonConstExpressionContainer.h"

using namespace arangodb;
using namespace arangodb::graph;

IndexAccessor::IndexAccessor(
    transaction::Methods::IndexHandle idx, aql::AstNode* condition,
    std::optional<size_t> memberToUpdate,
    std::unique_ptr<arangodb::aql::Expression> expression,
    std::optional<aql::NonConstExpressionContainer> nonConstPart,
    size_t cursorId)
    : _idx(idx),
      _indexCondition(condition),
      _memberToUpdate(memberToUpdate),
      _cursorId(cursorId),
      _nonConstContainer(std::move(nonConstPart)) {
  if (expression != nullptr) {
    _expression = std::move(expression);
  }
}

aql::AstNode* IndexAccessor::getCondition() const { return _indexCondition; }

aql::Expression* IndexAccessor::getExpression() const {
  return _expression.get();
}

transaction::Methods::IndexHandle IndexAccessor::indexHandle() const {
  return _idx;
}

std::optional<size_t> IndexAccessor::getMemberToUpdate() const {
  return _memberToUpdate;
}

size_t IndexAccessor::cursorId() const { return _cursorId; }

bool IndexAccessor::hasNonConstParts() const {
  return _nonConstContainer.has_value() &&
         !_nonConstContainer->_expressions.empty();
}

aql::NonConstExpressionContainer const& IndexAccessor::nonConstPart() const {
  TRI_ASSERT(hasNonConstParts());
  return _nonConstContainer.value();
}

BaseProviderOptions::BaseProviderOptions(
    aql::Variable const* tmpVar,
    std::pair<std::vector<IndexAccessor>,
              std::unordered_map<uint64_t, std::vector<IndexAccessor>>>&&
        indexInfo,
    aql::FixedVarExpressionContext& expressionContext,
    std::unordered_map<std::string, std::vector<std::string>> const&
        collectionToShardMap)
    : _temporaryVariable(tmpVar),
      _indexInformation(std::move(indexInfo)),
      _expressionContext(expressionContext),
      _collectionToShardMap(collectionToShardMap),
      _weightCallback(std::nullopt) {}

aql::Variable const* BaseProviderOptions::tmpVar() const {
  return _temporaryVariable;
}

// first is global index information, second is depth-based index information.
std::pair<std::vector<IndexAccessor>,
          std::unordered_map<uint64_t, std::vector<IndexAccessor>>>&
BaseProviderOptions::indexInformations() {
  return _indexInformation;
}

std::unordered_map<std::string, std::vector<std::string>> const&
BaseProviderOptions::collectionToShardMap() const {
  return _collectionToShardMap;
}

aql::FixedVarExpressionContext& BaseProviderOptions::expressionContext() const {
  return _expressionContext;
}

bool BaseProviderOptions::hasWeightMethod() const {
  return _weightCallback.has_value();
}

void BaseProviderOptions::setWeightEdgeCallback(WeightCallback callback) {
  _weightCallback = std::move(callback);
}

double BaseProviderOptions::weightEdge(double prefixWeight,
                                       arangodb::velocypack::Slice edge) const {
  if (!hasWeightMethod()) {
    // We do not have a weight. Hardcode.
    return 1.0;
  }
  return _weightCallback.value()(prefixWeight, edge);
}

ClusterBaseProviderOptions::ClusterBaseProviderOptions(
    std::shared_ptr<RefactoredClusterTraverserCache> cache,
    std::unordered_map<ServerID, aql::EngineId> const* engines, bool backward)
    : _cache(std::move(cache)), _engines(engines), _backward(backward) {
  TRI_ASSERT(_cache != nullptr);
  TRI_ASSERT(_engines != nullptr);
}

RefactoredClusterTraverserCache* ClusterBaseProviderOptions::getCache() {
  TRI_ASSERT(_cache != nullptr);
  return _cache.get();
}

bool ClusterBaseProviderOptions::isBackward() const { return _backward; }

std::unordered_map<ServerID, aql::EngineId> const*
ClusterBaseProviderOptions::engines() const {
  TRI_ASSERT(_engines != nullptr);
  return _engines;
}
