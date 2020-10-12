////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2020 ArangoDB GmbH, Cologne, Germany
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

#include "BaseOptions.h"

#include "Aql/AqlTransaction.h"
#include "Aql/Ast.h"
#include "Aql/Collection.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/IndexNode.h"
#include "Aql/Query.h"
#include "Aql/OptimizerUtils.h"
#include "Containers/HashSet.h"
#include "Cluster/ClusterEdgeCursor.h"
#include "Graph/ShortestPathOptions.h"
#include "Graph/SingleServerEdgeCursor.h"
#include "Graph/TraverserCache.h"
#include "Graph/TraverserCacheFactory.h"
#include "Graph/TraverserOptions.h"
#include "Indexes/Index.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::graph;
using namespace arangodb::traverser;

BaseOptions::LookupInfo::LookupInfo()
    : indexCondition(nullptr),
      conditionNeedUpdate(false),
      conditionMemberToUpdate(0) {
  // NOTE: We need exactly one in this case for the optimizer to update
  idxHandles.resize(1);
}

BaseOptions::LookupInfo::~LookupInfo() = default;

BaseOptions::LookupInfo::LookupInfo(arangodb::aql::QueryContext& query,
                                    VPackSlice const& info, VPackSlice const& shards) {
  TRI_ASSERT(shards.isArray());
  idxHandles.reserve(shards.length());

  conditionNeedUpdate =
      arangodb::basics::VelocyPackHelper::getBooleanValue(info, "condNeedUpdate", false);
  conditionMemberToUpdate = arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(
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
}

BaseOptions::LookupInfo::LookupInfo(LookupInfo const& other)
    : idxHandles(other.idxHandles),
      indexCondition(other.indexCondition),
      conditionNeedUpdate(other.conditionNeedUpdate),
      conditionMemberToUpdate(other.conditionMemberToUpdate) {
  if (other.expression != nullptr) {
    expression = other.expression->clone(nullptr);
  }
}

void BaseOptions::LookupInfo::buildEngineInfo(VPackBuilder& result) const {
  result.openObject();
  result.add(VPackValue("handle"));
  // We only run toVelocyPack on Coordinator.
  TRI_ASSERT(idxHandles.size() == 1);

  idxHandles[0]->toVelocyPack(result, Index::makeFlags(Index::Serialize::Basics));

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
  result.close();
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

std::unique_ptr<BaseOptions> BaseOptions::createOptionsFromSlice(arangodb::aql::QueryContext& query,
                                                                 VPackSlice const& definition) {
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
      _parallelism(1),
      _produceVertices(true),
      _isCoordinator(arangodb::ServerState::instance()->isCoordinator()) {}

BaseOptions::BaseOptions(BaseOptions const& other, bool allowAlreadyBuiltCopy)
    : _trx(other._query.newTrxContext()),
      _expressionCtx(_trx, other._query, _aqlFunctionsInternalCache),
      _query(other._query),
      _tmpVar(nullptr),
      _collectionToShard(other._collectionToShard),
      _parallelism(other._parallelism),
      _produceVertices(other._produceVertices),
      _isCoordinator(arangodb::ServerState::instance()->isCoordinator()) {
  if (!allowAlreadyBuiltCopy) {
    TRI_ASSERT(other._baseLookupInfos.empty());
    TRI_ASSERT(other._tmpVar == nullptr);
  }
}

BaseOptions::BaseOptions(arangodb::aql::QueryContext& query,
                         VPackSlice info, VPackSlice collections)
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
    _baseLookupInfos.emplace_back(query, itLookup.value(), itCollections.value());

    itLookup.next();
    itCollections.next();
  }
      
  // parallelism is optional
  read = info.get("parallelism");
  if (read.isInteger()) {
    _parallelism = read.getNumber<size_t>();
  }
  
  TRI_ASSERT(_produceVertices);
  read = info.get("produceVertices");
  if (read.isBool() && !read.getBool()) {
    _produceVertices = false;
  }
}

BaseOptions::~BaseOptions() = default;

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
  builder.close(); // base
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

void BaseOptions::addLookupInfo(aql::ExecutionPlan* plan, std::string const& collectionName,
                                std::string const& attributeName,
                                aql::AstNode* condition, bool onlyEdgeIndexes) {
  injectLookupInfoInList(_baseLookupInfos, plan, collectionName, attributeName, condition, onlyEdgeIndexes);
}

void BaseOptions::injectLookupInfoInList(std::vector<LookupInfo>& list,
                                         aql::ExecutionPlan* plan,
                                         std::string const& collectionName,
                                         std::string const& attributeName,
                                         aql::AstNode* condition, bool onlyEdgeIndexes) {
  LookupInfo info;
  info.indexCondition = condition->clone(plan->getAst());
  auto coll = _query.collections().get(collectionName);
  if (!coll) {
    THROW_ARANGO_EXCEPTION(TRI_ERROR_ARANGO_DATA_SOURCE_NOT_FOUND);
  }

  bool res = aql::utils::getBestIndexHandleForFilterCondition(*coll, info.indexCondition,
                                                              _tmpVar, 1000, aql::IndexHint(),
                                                              info.idxHandles[0], onlyEdgeIndexes);
  // Right now we have an enforced edge index which should always fit.
  if (!res) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_INTERNAL,
                                   "expected edge index not found");
  }

  // We now have to check if we need _from / _to inside the index lookup and
  // which position
  // it is used in. Such that the traverser can update the respective string
  // value in-place

  std::pair<arangodb::aql::Variable const*, std::vector<basics::AttributeName>> pathCmp;
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
      if (pathCmp.second.size() == 1 && pathCmp.second[0].name == attributeName) {
        info.conditionNeedUpdate = true;
        info.conditionMemberToUpdate = i;
        break;
      }
      continue;
    }
  }

  ::arangodb::containers::HashSet<size_t> toRemove;
  aql::Condition::collectOverlappingMembers(plan, _tmpVar, condition, info.indexCondition,
                                            toRemove, nullptr, false);
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
    info.expression = std::make_unique<aql::Expression>(plan->getAst(), condition);
  }
  list.emplace_back(std::move(info));
}

void BaseOptions::clearVariableValues() { _expressionCtx.clearVariableValues(); }

void BaseOptions::setVariableValue(aql::Variable const* var, aql::AqlValue const value) {
  _expressionCtx.setVariableValue(var, value);
}

void BaseOptions::serializeVariables(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenArray());
  _expressionCtx.serializeAllVariables(_query.vpackOptions(), builder);
}

void BaseOptions::setCollectionToShard(std::map<std::string, std::string> const& in) {
  _collectionToShard = std::move(in);
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
}

arangodb::aql::Expression* BaseOptions::getEdgeExpression(size_t cursorId,
                                                          bool& needToInjectVertex) const {
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
  expression->setVariable(_tmpVar, value);
  bool mustDestroy = false;
  aql::AqlValue res = expression->execute(&_expressionCtx, mustDestroy);
  TRI_ASSERT(res.isBoolean());
  bool result = res.toBoolean();
  expression->clearVariable(_tmpVar);
  if (mustDestroy) {
    res.destroy();
  }
  if (!result) {
    cache()->increaseFilterCounter();
  }
  return result;
}

double BaseOptions::costForLookupInfoList(std::vector<BaseOptions::LookupInfo> const& list,
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

void BaseOptions::activateCache(bool enableDocumentCache,
                                std::unordered_map<ServerID, aql::EngineId> const* engines) {
  // Do not call this twice.
  TRI_ASSERT(_cache == nullptr);
  _cache.reset(CacheFactory::CreateCache(_query, enableDocumentCache, engines, this));
}

void BaseOptions::injectTestCache(std::unique_ptr<TraverserCache>&& testCache) {
  TRI_ASSERT(_cache == nullptr);
  _cache = std::move(testCache);
}
