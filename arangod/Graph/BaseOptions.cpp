////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
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
#include "Aql/Ast.h"
#include "Aql/AqlTransaction.h"
#include "Aql/Condition.h"
#include "Aql/ExecutionPlan.h"
#include "Aql/Expression.h"
#include "Aql/Query.h"
#include "Graph/ShortestPathOptions.h"
#include "Graph/SingleServerEdgeCursor.h"
#include "Graph/TraverserCache.h"
#include "Graph/TraverserCacheFactory.h"
#include "Indexes/Index.h"
#include "VocBase/TraverserOptions.h"

#include <velocypack/Builder.h>
#include <velocypack/Iterator.h>
#include <velocypack/Slice.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::graph;
using namespace arangodb::traverser;

BaseOptions::LookupInfo::LookupInfo()
    : expression(nullptr),
      indexCondition(nullptr),
      conditionNeedUpdate(false),
      conditionMemberToUpdate(0) {
  // NOTE: We need exactly one in this case for the optimizer to update
  idxHandles.resize(1);
};

BaseOptions::LookupInfo::~LookupInfo() {
  if (expression != nullptr) {
    delete expression;
  }
}

BaseOptions::LookupInfo::LookupInfo(arangodb::aql::Query* query,
                                    VPackSlice const& info,
                                    VPackSlice const& shards) {
  TRI_ASSERT(shards.isArray());
  idxHandles.reserve(shards.length());

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
  auto trx = query->trx();

  for (auto const& it : VPackArrayIterator(shards)) {
    if (!it.isString()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "Shards have to be a list of strings");
    }
    idxHandles.emplace_back(trx->getIndexByIdentifier(it.copyString(), idxId));
  }

  read = info.get("expression");
  if (read.isObject()) {
    expression = new aql::Expression(query->ast(), read);
  } else {
    expression = nullptr;
  }


  read = info.get("condition");
  if (!read.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "Each lookup requires condition to be an object");
  }
  indexCondition = new aql::AstNode(query->ast(), read);
}

BaseOptions::LookupInfo::LookupInfo(LookupInfo const& other)
    : idxHandles(other.idxHandles),
      expression(nullptr),
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

  idxHandles[0].toVelocyPack(result, false);

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
  auto idx = idxHandles[0].getIndex();
  if (idx->hasSelectivityEstimate()) {
    double expected = 1 / idx->selectivityEstimate();
    nrItems += static_cast<size_t>(expected);
    return expected;
  }
  // Some hard-coded value
  nrItems += 1000;
  return 1000.0;
}

std::unique_ptr<BaseOptions> BaseOptions::createOptionsFromSlice(
    transaction::Methods* trx, VPackSlice const& definition) {
  VPackSlice type = definition.get("type");
  if (type.isString() && type.isEqualString("shortestPath")) {
    return std::make_unique<ShortestPathOptions>(trx, definition);
  }
  return std::make_unique<TraverserOptions>(trx, definition);
}

BaseOptions::BaseOptions(transaction::Methods* trx)
    : _ctx(new aql::FixedVarExpressionContext()),
      _trx(trx),
      _tmpVar(nullptr),
      _isCoordinator(arangodb::ServerState::instance()->isCoordinator()),
      _cache(nullptr) {}

BaseOptions::BaseOptions(BaseOptions const& other)
    : _ctx(new aql::FixedVarExpressionContext()),
      _trx(other._trx),
      _tmpVar(nullptr),
      _isCoordinator(arangodb::ServerState::instance()->isCoordinator()),
      _cache(nullptr) {
  TRI_ASSERT(other._baseLookupInfos.empty());
  TRI_ASSERT(other._tmpVar == nullptr);
}

BaseOptions::BaseOptions(arangodb::aql::Query* query, VPackSlice info,
                         VPackSlice collections)
    : _ctx(new aql::FixedVarExpressionContext()),
      _trx(query->trx()),
      _tmpVar(nullptr),
      _isCoordinator(arangodb::ServerState::instance()->isCoordinator()),
      _cache(nullptr) {
  VPackSlice read = info.get("tmpVar");
  if (!read.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The options require a tmpVar");
  }
  _tmpVar = query->ast()->variables()->createVariable(read);

  read = info.get("baseLookupInfos");
  if (!read.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The options require a baseLookupInfos");
  }

  size_t length = read.length();
  TRI_ASSERT(read.length() == collections.length());
  _baseLookupInfos.reserve(length);
  for (size_t j = 0; j < length; ++j) {
    _baseLookupInfos.emplace_back(query, read.at(j), collections.at(j));
  }
}

BaseOptions::~BaseOptions() { delete _ctx; }

void BaseOptions::toVelocyPackIndexes(VPackBuilder& builder) const {
  builder.openObject();
  injectVelocyPackIndexes(builder);
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
                                aql::AstNode* condition) {
  injectLookupInfoInList(_baseLookupInfos, plan, collectionName, attributeName,
                         condition);
}

void BaseOptions::injectLookupInfoInList(std::vector<LookupInfo>& list,
                                         aql::ExecutionPlan* plan,
                                         std::string const& collectionName,
                                         std::string const& attributeName,
                                         aql::AstNode* condition) {
  LookupInfo info;
  info.indexCondition = condition->clone(plan->getAst());
  bool res = _trx->getBestIndexHandleForFilterCondition(
      collectionName, info.indexCondition, _tmpVar, 1000, info.idxHandles[0]);
  TRI_ASSERT(res);  // Right now we have an enforced edge index which will
                    // always fit.
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
  std::unordered_set<size_t> toRemove;
  aql::Condition::CollectOverlappingMembers(plan, _tmpVar, condition, info.indexCondition, toRemove, false);
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
    info.expression = new aql::Expression(plan->getAst(), condition);
  }
  list.emplace_back(std::move(info));
}

void BaseOptions::clearVariableValues() { _ctx->clearVariableValues(); }

void BaseOptions::setVariableValue(aql::Variable const* var,
                                   aql::AqlValue const value) {
  _ctx->setVariableValue(var, value);
}

void BaseOptions::serializeVariables(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenArray());
  _ctx->serializeAllVariables(_trx, builder);
}

arangodb::transaction::Methods* BaseOptions::trx() const { return _trx; }

arangodb::graph::TraverserCache* BaseOptions::cache() const {
  return _cache.get();
}

void BaseOptions::injectVelocyPackIndexes(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenObject());

  // base indexes
  builder.add("base", VPackValue(VPackValueType::Array));
  for (auto const& it : _baseLookupInfos) {
    for (auto const& it2 : it.idxHandles) {
      builder.openObject();
      it2.getIndex()->toVelocyPack(builder, false, false);
      builder.close();
    }
  }
  builder.close();
}

void BaseOptions::injectEngineInfo(VPackBuilder& result) const {
  TRI_ASSERT(result.isOpenObject());
  result.add(VPackValue("baseLookupInfos"));
  result.openArray();
  for (auto const& it : _baseLookupInfos) {
    it.buildEngineInfo(result);
  }
  result.close();

  result.add(VPackValue("tmpVar"));
  _tmpVar->toVelocyPack(result);
}

arangodb::aql::Expression* BaseOptions::getEdgeExpression(
    size_t cursorId, bool& needToInjectVertex) const {
  TRI_ASSERT(!_baseLookupInfos.empty());
  TRI_ASSERT(_baseLookupInfos.size() > cursorId);
  needToInjectVertex = !_baseLookupInfos[cursorId].conditionNeedUpdate;
  return _baseLookupInfos[cursorId].expression;
}

bool BaseOptions::evaluateExpression(arangodb::aql::Expression* expression,
                                     VPackSlice value) const {
  if (expression == nullptr) {
    return true;
  }

  TRI_ASSERT(!expression->isV8());
  TRI_ASSERT(value.isObject() || value.isNull());
  expression->setVariable(_tmpVar, value);
  bool mustDestroy = false;
  aql::AqlValue res = expression->execute(_trx, _ctx, mustDestroy);
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

EdgeCursor* BaseOptions::nextCursorLocal(ManagedDocumentResult* mmdr,
                                         StringRef vid,
                                         std::vector<LookupInfo>& list) {
  TRI_ASSERT(mmdr != nullptr);
  auto allCursor =
      std::make_unique<SingleServerEdgeCursor>(mmdr, this, list.size());
  auto& opCursors = allCursor->getCursors();
  for (auto& info : list) {
    auto& node = info.indexCondition;
    TRI_ASSERT(node->numMembers() > 0);
    if (info.conditionNeedUpdate) {
      // We have to inject _from/_to iff the condition needs it
      auto dirCmp = node->getMemberUnchecked(info.conditionMemberToUpdate);
      TRI_ASSERT(dirCmp->type == aql::NODE_TYPE_OPERATOR_BINARY_EQ);
      TRI_ASSERT(dirCmp->numMembers() == 2);

      auto idNode = dirCmp->getMemberUnchecked(1);
      TRI_ASSERT(idNode->type == aql::NODE_TYPE_VALUE);
      TRI_ASSERT(idNode->isValueType(aql::VALUE_TYPE_STRING));
      idNode->setStringValue(vid.data(), vid.length());
    }
    std::vector<OperationCursor*> csrs;
    csrs.reserve(info.idxHandles.size());
    for (auto const& it : info.idxHandles) {
      csrs.emplace_back(_trx->indexScanForCondition(it, node, _tmpVar, mmdr,
                                                    UINT64_MAX, 1000, false));
    }
    opCursors.emplace_back(std::move(csrs));
  }
  return allCursor.release();
}

TraverserCache* BaseOptions::cache() {
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
  return _cache.get();
}

void BaseOptions::activateCache(
    bool enableDocumentCache,
    std::unordered_map<ServerID, traverser::TraverserEngineID> const* engines) {
  // Do not call this twice.
  TRI_ASSERT(_cache == nullptr);
  _cache.reset(cacheFactory::CreateCache(_trx, enableDocumentCache, engines));
}
