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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "Graph/Providers/BaseProviderOptions.h"
#include "Aql/NonConstExpression.h"
#include "Aql/NonConstExpressionContainer.h"
#include "Aql/InAndOutRowExpressionContext.h"

using namespace arangodb;
using namespace arangodb::graph;

IndexAccessor::IndexAccessor(
    transaction::Methods::IndexHandle idx, aql::AstNode* condition,
    std::optional<size_t> memberToUpdate,
    std::unique_ptr<arangodb::aql::Expression> expression,
    std::optional<aql::NonConstExpressionContainer> nonConstPart,
    size_t cursorId, TRI_edge_direction_e direction)
    : _idx(idx),
      _indexCondition(condition),
      _memberToUpdate(memberToUpdate),
      _expression(std::move(expression)),
      _cursorId(cursorId),
      _nonConstContainer(std::move(nonConstPart)),
      _direction(direction) {}

aql::AstNode* IndexAccessor::getCondition() const noexcept {
  return _indexCondition;
}

aql::Expression* IndexAccessor::getExpression() const noexcept {
  return _expression.get();
}

transaction::Methods::IndexHandle IndexAccessor::indexHandle() const noexcept {
  return _idx;
}

std::optional<size_t> IndexAccessor::getMemberToUpdate() const {
  return _memberToUpdate;
}

size_t IndexAccessor::cursorId() const noexcept { return _cursorId; }

TRI_edge_direction_e IndexAccessor::direction() const noexcept {
  return _direction;
}

bool IndexAccessor::hasNonConstParts() const noexcept {
  return _nonConstContainer.has_value() &&
         !_nonConstContainer->_expressions.empty();
}

aql::NonConstExpressionContainer const& IndexAccessor::nonConstPart() const {
  TRI_ASSERT(hasNonConstParts());
  return _nonConstContainer.value();
}

SingleServerBaseProviderOptions::SingleServerBaseProviderOptions(
    aql::Variable const* tmpVar,
    std::pair<std::vector<IndexAccessor>,
              std::unordered_map<uint64_t, std::vector<IndexAccessor>>>&&
        indexInfo,
    aql::FixedVarExpressionContext& expressionContext,
    std::vector<std::pair<aql::Variable const*, aql::RegisterId>>
        filterConditionVariables,
    MonitoredCollectionToShardMap const& collectionToShardMap,
    aql::Projections const& vertexProjections,
    aql::Projections const& edgeProjections, bool produceVertices,
    bool useCache)
    : _temporaryVariable(tmpVar),
      _indexInformation(std::move(indexInfo)),
      _expressionContext(expressionContext),
      _collectionToShardMap(collectionToShardMap),
      _weightCallback(std::nullopt),
      _filterConditionVariables(filterConditionVariables),
      _vertexProjections{vertexProjections},
      _edgeProjections{edgeProjections},
      _produceVertices(produceVertices),
      _useCache(useCache) {}

aql::Variable const* SingleServerBaseProviderOptions::tmpVar() const {
  return _temporaryVariable;
}

// first is global index information, second is depth-based index information.
std::pair<std::vector<IndexAccessor>,
          std::unordered_map<uint64_t, std::vector<IndexAccessor>>>&
SingleServerBaseProviderOptions::indexInformations() {
  return _indexInformation;
}

MonitoredCollectionToShardMap const&
SingleServerBaseProviderOptions::collectionToShardMap() const {
  return _collectionToShardMap;
}

aql::FixedVarExpressionContext&
SingleServerBaseProviderOptions::expressionContext() const {
  return _expressionContext;
}

bool SingleServerBaseProviderOptions::hasWeightMethod() const noexcept {
  return _weightCallback.has_value();
}

bool SingleServerBaseProviderOptions::produceVertices() const noexcept {
  return _produceVertices;
}

bool SingleServerBaseProviderOptions::useCache() const noexcept {
  return _useCache;
}

void SingleServerBaseProviderOptions::setWeightEdgeCallback(
    WeightCallback callback) {
  _weightCallback = std::move(callback);
}

aql::Projections const& SingleServerBaseProviderOptions::getVertexProjections()
    const {
  return _vertexProjections;
}

aql::Projections const& SingleServerBaseProviderOptions::getEdgeProjections()
    const {
  return _edgeProjections;
}

double SingleServerBaseProviderOptions::weightEdge(
    double prefixWeight, arangodb::velocypack::Slice edge) const {
  if (!hasWeightMethod()) {
    // We do not have a weight. Hardcode.
    return prefixWeight + 1;
  }
  return _weightCallback.value()(prefixWeight, edge);
}

void SingleServerBaseProviderOptions::prepareContext(
    aql::InputAqlItemRow input) {
  for (auto const& [var, reg] : _filterConditionVariables) {
    _expressionContext.setVariableValue(var, input.getValue(reg));
  }
}

void SingleServerBaseProviderOptions::unPrepareContext() {
  _expressionContext.clearVariableValues();
}

ClusterBaseProviderOptions::ClusterBaseProviderOptions(
    std::shared_ptr<RefactoredClusterTraverserCache> cache,
    std::unordered_map<ServerID, aql::EngineId> const* engines, bool backward,
    bool produceVertices)
    : _cache(std::move(cache)),
      _engines(engines),
      _backward(backward),
      _produceVertices(produceVertices),
      _expressionContext(nullptr),
      _weightCallback(std::nullopt) {
  TRI_ASSERT(_cache != nullptr);
  TRI_ASSERT(_engines != nullptr);
}

ClusterBaseProviderOptions::ClusterBaseProviderOptions(
    std::shared_ptr<RefactoredClusterTraverserCache> cache,
    std::unordered_map<ServerID, aql::EngineId> const* engines, bool backward,
    bool produceVertices, aql::FixedVarExpressionContext* expressionContext,
    std::vector<std::pair<aql::Variable const*, aql::RegisterId>>
        filterConditionVariables,
    std::unordered_set<uint64_t> availableDepthsSpecificConditions)
    : _cache(std::move(cache)),
      _engines(engines),
      _backward(backward),
      _produceVertices(produceVertices),
      _expressionContext(expressionContext),
      _filterConditionVariables(filterConditionVariables),
      _weightCallback(std::nullopt),
      _availableDepthsSpecificConditions(
          std::move(availableDepthsSpecificConditions)) {
  TRI_ASSERT(_cache != nullptr);
  TRI_ASSERT(_engines != nullptr);
}

RefactoredClusterTraverserCache* ClusterBaseProviderOptions::getCache() {
  TRI_ASSERT(_cache != nullptr);
  return _cache.get();
}

bool ClusterBaseProviderOptions::isBackward() const noexcept {
  return _backward;
}

bool ClusterBaseProviderOptions::produceVertices() const noexcept {
  return _produceVertices;
}

std::unordered_map<ServerID, aql::EngineId> const*
ClusterBaseProviderOptions::engines() const {
  TRI_ASSERT(_engines != nullptr);
  return _engines;
}

void ClusterBaseProviderOptions::prepareContext(aql::InputAqlItemRow input) {
  // [GraphRefactor] Note: Currently, only used in Traversal, but not
  // KShortestPath
  if (_expressionContext != nullptr) {
    for (auto const& [var, reg] : _filterConditionVariables) {
      _expressionContext->setVariableValue(var, input.getValue(reg));
    }
  }
}

void ClusterBaseProviderOptions::unPrepareContext() {
  // [GraphRefactor] Note: Currently, only used in Traversal, but not
  // KShortestPath
  if (_expressionContext != nullptr) {
    _expressionContext->clearVariableValues();
  }
}

aql::FixedVarExpressionContext*
ClusterBaseProviderOptions::expressionContext() {
  return _expressionContext;
}

bool ClusterBaseProviderOptions::hasWeightMethod() const {
  return _weightCallback.has_value();
}

void ClusterBaseProviderOptions::setWeightEdgeCallback(
    WeightCallback callback) {
  _weightCallback = std::move(callback);
}

double ClusterBaseProviderOptions::weightEdge(
    double prefixWeight, arangodb::velocypack::Slice edge) const {
  if (!hasWeightMethod()) {
    // We do not have a weight. Hardcode.
    return prefixWeight + 1;
  }
  return _weightCallback.value()(prefixWeight, edge);
}

bool ClusterBaseProviderOptions::hasDepthSpecificLookup(
    uint64_t depth) const noexcept {
  return _availableDepthsSpecificConditions.contains(depth);
}

#ifdef USE_ENTERPRISE
void ClusterBaseProviderOptions::setRPCCommunicator(
    std::unique_ptr<enterprise::SmartGraphRPCCommunicator> communicator) {
  _communicator = std::move(communicator);
}

enterprise::SmartGraphRPCCommunicator&
ClusterBaseProviderOptions::getRPCCommunicator() {
  TRI_ASSERT(_communicator != nullptr);
  return *_communicator;
}
#endif
