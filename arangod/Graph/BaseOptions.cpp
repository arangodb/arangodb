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

#include "BaseOptions.h"

#include "Aql/AqlValueMaterializer.h"
#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/InputAqlItemRow.h"
#include "Aql/NonConstExpression.h"
#include "Aql/OptimizerUtils.h"
#include "Aql/Projections.h"
#include "Aql/Query.h"
#include "Basics/ResourceUsage.h"
#include "Basics/ScopeGuard.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterEdgeCursor.h"
#include "Containers/HashSet.h"
#include "Graph/Cache/RefactoredClusterTraverserCache.h"
#include "Graph/ShortestPathOptions.h"
#include "Graph/TraverserCache.h"
#include "Graph/TraverserCacheFactory.h"
#include "Graph/TraverserOptions.h"
#include "Indexes/Index.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>

using namespace arangodb;
using namespace arangodb::aql;
using namespace arangodb::graph;
using namespace arangodb::traverser;

using VPackHelper = arangodb::basics::VelocyPackHelper;

BaseOptions::LookupInfo::LookupInfo(TRI_edge_direction_e direction)
    : indexCondition(nullptr),
      direction(direction),
      conditionNeedUpdate(false),
      conditionMemberToUpdate(0) {
  // NOTE: We need exactly one in this case for the optimizer to update
  idxHandles.resize(1);
  TRI_ASSERT(direction == TRI_EDGE_IN || direction == TRI_EDGE_OUT);
}

BaseOptions::LookupInfo::~LookupInfo() = default;

BaseOptions::LookupInfo::LookupInfo(arangodb::aql::QueryContext& query,
                                    VPackSlice info, VPackSlice shards) {
  TRI_ASSERT(shards.isArray());
  idxHandles.reserve(shards.length());

  TRI_edge_direction_e dir = TRI_EDGE_ANY;
  VPackSlice dirSlice = info.get(StaticStrings::GraphDirection);
  if (dirSlice.isEqualString(StaticStrings::GraphDirectionInbound)) {
    dir = TRI_EDGE_IN;
  } else if (dirSlice.isEqualString(StaticStrings::GraphDirectionOutbound)) {
    dir = TRI_EDGE_OUT;
  }
  if (dir == TRI_EDGE_ANY) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "Missing or invalid direction attribute in graph definition");
  }
  // set direction for lookup info
  direction = dir;
  TRI_ASSERT(direction == TRI_EDGE_IN || direction == TRI_EDGE_OUT);

  conditionNeedUpdate = arangodb::basics::VelocyPackHelper::getBooleanValue(
      info, "condNeedUpdate", false);
  conditionMemberToUpdate =
      arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(
          info, "condMemberToUpdate", 0);

  VPackSlice read = info.get("handle");
  if (!read.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER, "Each lookup requires handle to be an object");
  }

  read = read.get("id");
  if (!read.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "Each handle requires id to be a string");
  }
  std::string idxId = read.copyString();
  aql::Collections const& collections = query.collections();

  for (VPackSlice it : VPackArrayIterator(shards)) {
    if (!it.isString()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "Shards have to be a list of strings");
    }
    auto* coll = collections.get(it.copyString());
    if (!coll) {
      TRI_ASSERT(false);
      THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
    }
    idxHandles.emplace_back(coll->indexByIdentifier(idxId));
  }

  read = info.get("expression");
  if (read.isObject()) {
    expression = std::make_unique<aql::Expression>(query.ast(), read);
  } else {
    expression.reset();
  }

  read = info.get("condition");
  if (!read.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "Each lookup requires condition to be an object");
  }
  indexCondition = query.ast()->createNode(read);
  read = info.get("nonConstContainer");
  if (read.isObject()) {
    _nonConstContainer =
        NonConstExpressionContainer::fromVelocyPack(query.ast(), read);
  }
}

BaseOptions::LookupInfo::LookupInfo(LookupInfo const& other)
    : idxHandles(other.idxHandles),
      indexCondition(other.indexCondition),
      direction(other.direction),
      conditionNeedUpdate(other.conditionNeedUpdate),
      conditionMemberToUpdate(other.conditionMemberToUpdate) {
  if (other.expression != nullptr) {
    expression = other.expression->clone(nullptr);
  }
  _nonConstContainer = other._nonConstContainer.clone(nullptr);

  TRI_ASSERT(direction == TRI_EDGE_IN || direction == TRI_EDGE_OUT);
}

void BaseOptions::LookupInfo::buildEngineInfo(VPackBuilder& result) const {
  VPackObjectBuilder objectGuard(&result);

  // direction
  TRI_ASSERT(direction == TRI_EDGE_IN || direction == TRI_EDGE_OUT);
  if (direction == TRI_EDGE_IN) {
    result.add(StaticStrings::GraphDirection,
               VPackValue(StaticStrings::GraphDirectionInbound));
  } else {
    result.add(StaticStrings::GraphDirection,
               VPackValue(StaticStrings::GraphDirectionOutbound));
  }

  result.add(VPackValue("handle"));
  // We only run toVelocyPack on Coordinator.
  TRI_ASSERT(idxHandles.size() == 1);

  idxHandles[0]->toVelocyPack(result,
                              Index::makeFlags(Index::Serialize::Basics));

  if (expression != nullptr) {
    result.add(VPackValue("expression"));
    result.openObject();  // We need to encapsulate the expression into an
                          // expression object
    result.add(VPackValue("expression"));
    expression->toVelocyPack(result, true);
    result.close();
  }
  result.add(VPackValue("condition"));
  indexCondition->toVelocyPack(result, true);
  result.add("condNeedUpdate", VPackValue(conditionNeedUpdate));
  result.add("condMemberToUpdate", VPackValue(conditionMemberToUpdate));
  result.add(VPackValue("nonConstContainer"));
  _nonConstContainer.toVelocyPack(result);
}

double BaseOptions::LookupInfo::estimateCost(size_t& nrItems) const {
  // If we do not have an index yet we cannot do anything.
  // Should NOT be the case
  TRI_ASSERT(!idxHandles.empty());
  std::shared_ptr<Index> const& idx = idxHandles[0];
  if (idx->hasSelectivityEstimate()) {
    double s = idx->selectivityEstimate();
    if (s > 0.0) {
      double expected = 1 / s;
      nrItems += static_cast<size_t>(expected);
      return expected;
    }
  }
  // Some hard-coded value
  nrItems += 1000;
  return 1000.0;
}

void BaseOptions::LookupInfo::initializeNonConstExpressions(
    aql::Ast* ast, aql::VarInfoMap const& varInfo,
    aql::Variable const* indexVariable) {
  _nonConstContainer = aql::utils::extractNonConstPartsOfIndexCondition(
      ast, varInfo, false, nullptr, indexCondition, indexVariable);
  // We cannot optimize V8 expressions
  TRI_ASSERT(!_nonConstContainer._hasV8Expression);
}

void BaseOptions::LookupInfo::calculateIndexExpressions(
    Ast* ast, ExpressionContext& ctx) {
  if (_nonConstContainer._expressions.empty()) {
    return;
  }

  // The following are needed to evaluate expressions with local data from
  // the current incoming item:
  for (auto& toReplace : _nonConstContainer._expressions) {
    auto exp = toReplace->expression.get();
    bool mustDestroy;
    AqlValue a = exp->execute(&ctx, mustDestroy);
    AqlValueGuard guard(a, mustDestroy);

    AqlValueMaterializer materializer(&(ctx.trx().vpackOptions()));
    VPackSlice slice = materializer.slice(a);
    AstNode* evaluatedNode = ast->nodeFromVPack(slice, true);

    AstNode* tmp = indexCondition;
    for (size_t x = 0; x < toReplace->indexPath.size(); x++) {
      size_t idx = toReplace->indexPath[x];
      AstNode* old = tmp->getMember(idx);
      // modify the node in place
      TEMPORARILY_UNLOCK_NODE(tmp);
      if (x + 1 < toReplace->indexPath.size()) {
        AstNode* cpy = old;
        tmp->changeMember(idx, cpy);
        tmp = cpy;
      } else {
        // insert the actual expression value
        tmp->changeMember(idx, evaluatedNode);
      }
    }
  }
}

std::unique_ptr<BaseOptions> BaseOptions::createOptionsFromSlice(
    arangodb::aql::QueryContext& query, VPackSlice const& definition) {
  VPackSlice type = definition.get("type");
  if (type.isString() && type.isEqualString("shortestPath")) {
    return std::make_unique<ShortestPathOptions>(query, definition);
  }
  return std::make_unique<TraverserOptions>(query, definition);
}

BaseOptions::BaseOptions(arangodb::aql::QueryContext& query)
    : _trx(query.newTrxContext()),
      _expressionCtx(_trx, query, _aqlFunctionsInternalCache),
      _query(query),
      _tmpVar(nullptr),
      _collectionToShard{resourceMonitor()},
      _parallelism(1),
      _produceVertices(true),
      _isCoordinator(arangodb::ServerState::instance()->isCoordinator()),
      _vertexProjections{},
      _edgeProjections{} {}

BaseOptions::BaseOptions(BaseOptions const& other, bool allowAlreadyBuiltCopy)
    : _trx(other._query.newTrxContext()),
      _expressionCtx(_trx, other._query, _aqlFunctionsInternalCache),
      _query(other._query),
      _tmpVar(nullptr),
      _collectionToShard(other._collectionToShard),
      _parallelism(other._parallelism),
      _produceVertices(other._produceVertices),
      _isCoordinator(arangodb::ServerState::instance()->isCoordinator()),
      _maxProjections{other._maxProjections},
      _hint(other._hint) {
  setVertexProjections(other._vertexProjections);
  setEdgeProjections(other._edgeProjections);

  if (!allowAlreadyBuiltCopy) {
    TRI_ASSERT(other._baseLookupInfos.empty());
    TRI_ASSERT(other._tmpVar == nullptr);
  }
}

BaseOptions::BaseOptions(arangodb::aql::QueryContext& query,
                         velocypack::Slice info, velocypack::Slice collections)
    : BaseOptions(query) {
  VPackSlice read = info.get("tmpVar");
  if (!read.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The options require a tmpVar");
  }
  _tmpVar = query.ast()->variables()->createVariable(read);

  read = info.get("baseLookupInfos");
  if (!read.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The options require a baseLookupInfos");
  }

  VPackArrayIterator itLookup(read);
  VPackArrayIterator itCollections(collections);
  _baseLookupInfos.reserve(itLookup.size());

  TRI_ASSERT(itLookup.size() == itCollections.size());

  while (itLookup.valid() && itCollections.valid()) {
    _baseLookupInfos.emplace_back(query, itLookup.value(),
                                  itCollections.value());

    itLookup.next();
    itCollections.next();
  }

  parseShardIndependentFlags(info);

  if (VPackSlice hintNode = info.get(StaticStrings::IndexHintOption);
      hintNode.isObject()) {
    setHint(IndexHint(hintNode));
  }
}

BaseOptions::~BaseOptions() {
  if (!getVertexProjections().empty()) {
    resourceMonitor().decreaseMemoryUsage(getVertexProjections().size() *
                                          sizeof(aql::Projections::Projection));
  }

  if (!getEdgeProjections().empty()) {
    resourceMonitor().decreaseMemoryUsage(getEdgeProjections().size() *
                                          sizeof(aql::Projections::Projection));
  }
}

arangodb::ResourceMonitor& BaseOptions::resourceMonitor() const {
  return _query.resourceMonitor();
}

void BaseOptions::toVelocyPackIndexes(VPackBuilder& builder) const {
  builder.openObject();
  // base indexes
  builder.add("base", VPackValue(VPackValueType::Array));
  for (auto const& it : _baseLookupInfos) {
    for (auto const& it2 : it.idxHandles) {
      builder.openObject();
      it2->toVelocyPack(builder, Index::makeFlags(Index::Serialize::Basics));
      builder.close();
    }
  }
  builder.close();  // base
  builder.close();
}

void BaseOptions::buildEngineInfo(VPackBuilder& result) const {
  result.openObject();
  injectEngineInfo(result);
  result.close();
}

void BaseOptions::setVariable(aql::Variable const* variable) {
  _tmpVar = variable;
}

void BaseOptions::addLookupInfo(aql::ExecutionPlan* plan,
                                std::string const& collectionName,
                                std::string const& attributeName,
                                aql::AstNode* condition, bool onlyEdgeIndexes,
                                TRI_edge_direction_e direction) {
  injectLookupInfoInList(_baseLookupInfos, plan, collectionName, attributeName,
                         condition, onlyEdgeIndexes, direction,
                         /*depth*/ std::nullopt);
}

void BaseOptions::injectLookupInfoInList(
    std::vector<LookupInfo>& list, aql::ExecutionPlan* plan,
    std::string const& collectionName, std::string const& attributeName,
    aql::AstNode* condition, bool onlyEdgeIndexes,
    TRI_edge_direction_e direction, std::optional<uint64_t> depth) {
  TRI_ASSERT(
      (direction == TRI_EDGE_IN && attributeName == StaticStrings::ToString) ||
      (direction == TRI_EDGE_OUT &&
       attributeName == StaticStrings::FromString));

  LookupInfo info(direction);
  info.indexCondition = condition->clone(plan->getAst());
  auto coll = _query.collections().get(collectionName);
  if (!coll) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  // arbitrary value for "number of edges in collection" used here. the
  // actual value does not matter much. 1000 has historically worked fine.
  constexpr size_t itemsInCollection = 1000;

  // use most specific index hint here
  auto indexHint =
      hint().getFromNested(direction == TRI_EDGE_IN ? "inbound" : "outbound",
                           coll->name(), depth.value_or(IndexHint::BaseDepth));

  auto& trx = plan->getAst()->query().trxForOptimization();
  bool res = aql::utils::getBestIndexHandleForFilterCondition(
      trx, *coll, info.indexCondition, _tmpVar, itemsInCollection, indexHint,
      info.idxHandles[0], ReadOwnWrites::no, onlyEdgeIndexes);
  // Right now we have an enforced edge index which should always fit.
  if (!res) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "expected edge index not found");
  }

  // We now have to check if we need _from / _to inside the index lookup and
  // which position
  // it is used in. Such that the traverser can update the respective string
  // value in-place

  std::pair<arangodb::aql::Variable const*, std::vector<basics::AttributeName>>
      pathCmp;
  for (size_t i = 0; i < info.indexCondition->numMembers(); ++i) {
    // We search through the nary-and and look for EQ - _from/_to
    auto eq = info.indexCondition->getMemberUnchecked(i);
    if (eq->type != arangodb::aql::AstNodeType::NODE_TYPE_OPERATOR_BINARY_EQ) {
      // No equality. Skip
      continue;
    }
    TRI_ASSERT(eq->numMembers() == 2);
    // It is sufficient to only check member one.
    // We build the condition this way.
    auto mem = eq->getMemberUnchecked(0);
    if (mem->isAttributeAccessForVariable(pathCmp, true)) {
      if (pathCmp.first != _tmpVar) {
        continue;
      }
      if (pathCmp.second.size() == 1 &&
          pathCmp.second[0].name == attributeName) {
        info.conditionNeedUpdate = true;
        info.conditionMemberToUpdate = i;
        break;
      }
      continue;
    }
  }

  ::arangodb::containers::HashSet<size_t> toRemove;
  aql::Condition::collectOverlappingMembers(
      plan, _tmpVar, condition, info.indexCondition, toRemove, nullptr, false);
  size_t n = condition->numMembers();
  if (n == toRemove.size()) {
    // FastPath, all covered.
    info.expression = nullptr;
  } else {
    // Slow path need to explicitly remove nodes.
    for (; n > 0; --n) {
      // Now n is one more than the idx we actually check
      if (toRemove.find(n - 1) != toRemove.end()) {
        // This index has to be removed.
        condition->removeMemberUnchecked(n - 1);
      }
    }
    info.expression =
        std::make_unique<aql::Expression>(plan->getAst(), condition);
  }
  list.emplace_back(std::move(info));
}

void BaseOptions::clearVariableValues() noexcept {
  _expressionCtx.clearVariableValues();
}

void BaseOptions::setVariableValue(aql::Variable const* var,
                                   aql::AqlValue value) {
  _expressionCtx.setVariableValue(var, value);
}

void BaseOptions::serializeVariables(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenArray());
  _expressionCtx.serializeAllVariables(_query.vpackOptions(), builder);
}

void BaseOptions::setCollectionToShard(
    std::unordered_map<std::string, ShardID> const& in) {
  _collectionToShard.clear();
  _collectionToShard.reserve(in.size());
  for (auto const& [key, value] : in) {
    ResourceUsageAllocator<MonitoredCollectionToShardMap, ResourceMonitor>
        alloc = {_query.resourceMonitor()};
    auto myVec = MonitoredShardIDVector{alloc};
    myVec.emplace_back(value);
    _collectionToShard.emplace(key, std::move(myVec));
  }
}

arangodb::transaction::Methods* BaseOptions::trx() const { return &_trx; }

arangodb::aql::QueryContext& BaseOptions::query() const { return _query; }

void BaseOptions::injectEngineInfo(VPackBuilder& result) const {
  TRI_ASSERT(result.isOpenObject());
  result.add(VPackValue("baseLookupInfos"));
  result.openArray();
  for (auto const& it : _baseLookupInfos) {
    it.buildEngineInfo(result);
  }
  result.close();

  result.add(VPackValue("tmpVar"));
  TRI_ASSERT(_tmpVar != nullptr);
  _tmpVar->toVelocyPack(result);
  toVelocyPackBase(result);
}

arangodb::aql::Expression* BaseOptions::getEdgeExpression(
    size_t cursorId, bool& needToInjectVertex) const {
  TRI_ASSERT(!_baseLookupInfos.empty());
  TRI_ASSERT(_baseLookupInfos.size() > cursorId);
  needToInjectVertex = !_baseLookupInfos[cursorId].conditionNeedUpdate;
  return _baseLookupInfos[cursorId].expression.get();
}

bool BaseOptions::evaluateExpression(arangodb::aql::Expression* expression,
                                     VPackSlice value) {
  if (expression == nullptr) {
    return true;
  }

  TRI_ASSERT(value.isObject() || value.isNull());
  _expressionCtx.setVariableValue(_tmpVar,
                                  AqlValue(AqlValueHintSliceNoCopy(value)));
  ScopeGuard defer(
      [&]() noexcept { _expressionCtx.clearVariableValue(_tmpVar); });
  bool mustDestroy = false;
  aql::AqlValue res = expression->execute(&_expressionCtx, mustDestroy);
  aql::AqlValueGuard guard{res, mustDestroy};
  TRI_ASSERT(res.isBoolean());
  bool result = res.toBoolean();
  if (!result) {
    cache()->incrFiltered();
  }
  return result;
}

void BaseOptions::initializeIndexConditions(
    aql::Ast* ast, aql::VarInfoMap const& varInfo,
    aql::Variable const* indexVariable) {
  for (auto& it : _baseLookupInfos) {
    it.initializeNonConstExpressions(ast, varInfo, indexVariable);
  }
}

void BaseOptions::calculateIndexExpressions(aql::Ast* ast) {
  for (auto& it : _baseLookupInfos) {
    it.calculateIndexExpressions(ast, _expressionCtx);
  }
}

double BaseOptions::costForLookupInfoList(
    std::vector<BaseOptions::LookupInfo> const& list,
    size_t& createItems) const {
  double cost = 0;
  createItems = 0;
  for (auto const& li : list) {
    cost += li.estimateCost(createItems);
  }
  return cost;
}

arangodb::graph::TraverserCache* BaseOptions::cache() const {
  return _cache.get();
}

TraverserCache* BaseOptions::cache() {
  ensureCache();
  return _cache.get();
}

void BaseOptions::ensureCache() {
  if (_cache == nullptr) {
    // If the Coordinator does NOT activate the Cache
    // the datalake is not created and cluster data cannot
    // be persisted anywhere.
    TRI_ASSERT(!arangodb::ServerState::instance()->isCoordinator());
    // In production just gracefully initialize
    // the cache without document cache, s.t. system does not crash
    activateCache(false, nullptr);
  }
  TRI_ASSERT(_cache != nullptr);
}

void BaseOptions::activateCache(
    bool enableDocumentCache,
    std::unordered_map<ServerID, aql::EngineId> const* engines) {
  // Do not call this twice.
  TRI_ASSERT(_cache == nullptr);
  _cache.reset(
      CacheFactory::CreateCache(_query, enableDocumentCache, engines, this));
}

arangodb::aql::FixedVarExpressionContext& BaseOptions::getExpressionCtx() {
  return _expressionCtx;
}

arangodb::aql::FixedVarExpressionContext const& BaseOptions::getExpressionCtx()
    const {
  return _expressionCtx;
}

aql::Variable const* BaseOptions::tmpVar() { return _tmpVar; }

void BaseOptions::isQueryKilledCallback() const {
  if (query().killed()) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_QUERY_KILLED);
  }
}

void BaseOptions::setVertexProjections(Projections projections) {
  if (!getVertexProjections().empty()) {
    resourceMonitor().decreaseMemoryUsage(getVertexProjections().size() *
                                          sizeof(aql::Projections::Projection));
  }
  try {
    resourceMonitor().increaseMemoryUsage(projections.size() *
                                          sizeof(aql::Projections::Projection));
    _vertexProjections = std::move(projections);
  } catch (...) {
    _vertexProjections.clear();
    throw;
  }
}

void BaseOptions::setEdgeProjections(Projections projections) {
  if (!getEdgeProjections().empty()) {
    resourceMonitor().decreaseMemoryUsage(getEdgeProjections().size() *
                                          sizeof(aql::Projections::Projection));
  }
  try {
    resourceMonitor().increaseMemoryUsage(projections.size() *
                                          sizeof(aql::Projections::Projection));
    _edgeProjections = std::move(projections);
  } catch (...) {
    _edgeProjections.clear();
    throw;
  }
}

void BaseOptions::setMaxProjections(size_t projections) noexcept {
  _maxProjections = projections;
}

size_t BaseOptions::getMaxProjections() const noexcept {
  return _maxProjections;
}

Projections const& BaseOptions::getVertexProjections() const {
  return _vertexProjections;
}

Projections const& BaseOptions::getEdgeProjections() const {
  return _edgeProjections;
}

void BaseOptions::toVelocyPackBase(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenObject());
  builder.add("parallelism", VPackValue(_parallelism));
  builder.add("produceVertices", VPackValue(_produceVertices));
  builder.add(StaticStrings::MaxProjections, VPackValue(getMaxProjections()));

  if (!_vertexProjections.empty()) {
    _vertexProjections.toVelocyPack(builder, "vertexProjections");
  }

  if (!_edgeProjections.empty()) {
    _edgeProjections.toVelocyPack(builder, "edgeProjections");
  }
}

void BaseOptions::parseShardIndependentFlags(arangodb::velocypack::Slice info) {
  // parallelism is optional
  _parallelism = VPackHelper::getNumericValue<size_t>(info, "parallelism", 1);

  TRI_ASSERT(_produceVertices);
  _produceVertices =
      VPackHelper::getBooleanValue(info, "produceVertices", true);

  // TODO: For some reason _produceEdges has not been deserialized (skipped
  // in refactoring to avoid side-effects)

  // read back projections
  setMaxProjections(VPackHelper::getNumericValue<size_t>(
      info, StaticStrings::MaxProjections,
      DocumentProducingNode::kMaxProjections));

  {
    if (!getVertexProjections().empty()) {
      resourceMonitor().decreaseMemoryUsage(
          getVertexProjections().size() * sizeof(aql::Projections::Projection));
      _vertexProjections.clear();
    }

    _vertexProjections = Projections::fromVelocyPack(
        _query.ast(), info, "vertexProjections", resourceMonitor());
    try {
      resourceMonitor().increaseMemoryUsage(
          getVertexProjections().size() * sizeof(aql::Projections::Projection));
    } catch (...) {
      _vertexProjections.clear();
      throw;
    }
  }

  {
    if (!getEdgeProjections().empty()) {
      resourceMonitor().decreaseMemoryUsage(
          getEdgeProjections().size() * sizeof(aql::Projections::Projection));
      _edgeProjections.clear();
    }
    _edgeProjections = Projections::fromVelocyPack(
        _query.ast(), info, "edgeProjections", resourceMonitor());
    try {
      resourceMonitor().increaseMemoryUsage(
          getEdgeProjections().size() * sizeof(aql::Projections::Projection));
    } catch (...) {
      _edgeProjections.clear();
      throw;
    }
  }
}

aql::IndexHint const& BaseOptions::hint() const { return _hint; }

void BaseOptions::setHint(aql::IndexHint hint) { _hint = std::move(hint); }
