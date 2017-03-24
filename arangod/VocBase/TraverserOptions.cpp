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

#include "TraverserOptions.h"

#include "Aql/Ast.h"
#include "Aql/AstNode.h"
#include "Aql/Expression.h"
#include "Aql/Query.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterEdgeCursor.h"
#include "Indexes/Index.h"
#include "Transaction/Methods.h"
#include "VocBase/SingleServerTraverser.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using VPackHelper = arangodb::basics::VelocyPackHelper;
using TraverserOptions = arangodb::traverser::TraverserOptions;
using ShortestPathOptions = arangodb::traverser::ShortestPathOptions;
using BaseTraverserOptions = arangodb::traverser::BaseTraverserOptions;

BaseTraverserOptions::LookupInfo::LookupInfo()
    : expression(nullptr),
      indexCondition(nullptr),
      conditionNeedUpdate(false),
      conditionMemberToUpdate(0) {
  // NOTE: We need exactly one in this case for the optimizer to update
  idxHandles.resize(1);
};

BaseTraverserOptions::LookupInfo::~LookupInfo() {
  if (expression != nullptr) {
    delete expression;
  }
}

BaseTraverserOptions::LookupInfo::LookupInfo(
    arangodb::aql::Query* query, VPackSlice const& info, VPackSlice const& shards) {
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
        TRI_ERROR_BAD_PARAMETER,
        "Each lookup requires handle to be an object");
  }

  read = read.get("id");
  if (!read.isString()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "Each handle requires id to be a string");
  }
  std::string idxId = read.copyString();
  auto trx = query->trx();

  for (auto const& it : VPackArrayIterator(shards)) {
    if (!it.isString()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "Shards have to be a list of strings");
    }
    idxHandles.emplace_back(trx->getIndexByIdentifier(it.copyString(), idxId));
  }

  read = info.get("expression");
  if (!read.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "Each lookup requires expression to be an object");
  }

  expression = new aql::Expression(query->ast(), read);

  read = info.get("condition");
  if (!read.isObject()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "Each lookup requires condition to be an object");
  }
  indexCondition = new aql::AstNode(query->ast(), read); 
}

BaseTraverserOptions::LookupInfo::LookupInfo(
    LookupInfo const& other)
    : idxHandles(other.idxHandles),
      expression(nullptr),
      indexCondition(other.indexCondition),
      conditionNeedUpdate(other.conditionNeedUpdate),
      conditionMemberToUpdate(other.conditionMemberToUpdate) {
  expression = other.expression->clone(nullptr);
}

void BaseTraverserOptions::LookupInfo::buildEngineInfo(
    VPackBuilder& result) const {
  result.openObject();
  result.add(VPackValue("handle"));
  // We only run toVelocyPack on Coordinator.
  TRI_ASSERT(idxHandles.size() == 1);
  result.openObject();
  idxHandles[0].toVelocyPack(result, false);
  result.close();
  result.add(VPackValue("expression"));
  result.openObject(); // We need to encapsulate the expression into an expression object
  result.add(VPackValue("expression"));
  expression->toVelocyPack(result, true);
  result.close();
  result.add(VPackValue("condition"));
  indexCondition->toVelocyPack(result, true);
  result.add("condNeedUpdate", VPackValue(conditionNeedUpdate));
  result.add("condMemberToUpdate", VPackValue(conditionMemberToUpdate));
  result.close();
}

double TraverserOptions::LookupInfo::estimateCost(size_t& nrItems) const {
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

TraverserOptions::TraverserOptions(
    transaction::Methods* trx, VPackSlice const& slice)
    : BaseTraverserOptions(trx),
      _baseVertexExpression(nullptr),
      minDepth(1),
      maxDepth(1),
      useBreadthFirst(false),
      uniqueVertices(UniquenessLevel::NONE),
      uniqueEdges(UniquenessLevel::PATH) {
  VPackSlice obj = slice.get("traversalFlags");
  TRI_ASSERT(obj.isObject());

  minDepth = VPackHelper::getNumericValue<uint64_t>(obj, "minDepth", 1);
  maxDepth = VPackHelper::getNumericValue<uint64_t>(obj, "maxDepth", 1);
  TRI_ASSERT(minDepth <= maxDepth);
  useBreadthFirst = VPackHelper::getBooleanValue(obj, "bfs", false);
  std::string tmp = VPackHelper::getStringValue(obj, "uniqueVertices", "");
  if (tmp == "path") {
    uniqueVertices =
        TraverserOptions::UniquenessLevel::PATH;
  } else if (tmp == "global") {
    if (!useBreadthFirst) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "uniqueVertices: 'global' is only "
                                     "supported, with bfs: true due to "
                                     "unpredictable results.");
    }
    uniqueVertices =
        TraverserOptions::UniquenessLevel::GLOBAL;
  } else {
    uniqueVertices =
        TraverserOptions::UniquenessLevel::NONE;
  }

  tmp = VPackHelper::getStringValue(obj, "uniqueEdges", "");
  if (tmp == "none") {
    uniqueEdges =
        TraverserOptions::UniquenessLevel::NONE;
  } else if (tmp == "global") {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "uniqueEdges: 'global' is not supported, "
                                   "due to unpredictable results. Use 'path' "
                                   "or 'none' instead");
    uniqueEdges =
        TraverserOptions::UniquenessLevel::GLOBAL;
  } else {
    uniqueEdges =
        TraverserOptions::UniquenessLevel::PATH;
  }
}

BaseTraverserOptions::BaseTraverserOptions(BaseTraverserOptions const& other)
    : _ctx(new aql::FixedVarExpressionContext()),
      _trx(other._trx),
      _tmpVar(nullptr),
      _isCoordinator(arangodb::ServerState::instance()->isCoordinator()) {
  TRI_ASSERT(other._baseLookupInfos.empty());
  TRI_ASSERT(other._tmpVar == nullptr);
}


BaseTraverserOptions::BaseTraverserOptions(
    arangodb::aql::Query* query, VPackSlice info, VPackSlice collections)
  : _ctx(new aql::FixedVarExpressionContext()),
    _trx(query->trx()),
    _tmpVar(nullptr),
    _isCoordinator(arangodb::ServerState::instance()->isCoordinator()) {
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

BaseTraverserOptions::~BaseTraverserOptions() {
  delete _ctx;
}


void BaseTraverserOptions::toVelocyPackIndexes(VPackBuilder& builder) const {
  builder.openObject();
  injectVelocyPackIndexes(builder);
  builder.close();
}

void BaseTraverserOptions::buildEngineInfo(VPackBuilder& result) const {
  result.openObject();
  injectEngineInfo(result);
  result.close();
}

void BaseTraverserOptions::setVariable(aql::Variable const* variable) {
  _tmpVar = variable;
}

void BaseTraverserOptions::addLookupInfo(aql::Ast* ast,
                                         std::string const& collectionName,
                                         std::string const& attributeName,
                                         aql::AstNode* condition) {
  injectLookupInfoInList(_baseLookupInfos, ast, collectionName, attributeName, condition);
}

void BaseTraverserOptions::injectLookupInfoInList(
    std::vector<LookupInfo>& list, aql::Ast* ast,
    std::string const& collectionName, std::string const& attributeName,
    aql::AstNode* condition) {
  traverser::TraverserOptions::LookupInfo info;
  info.indexCondition = condition;
  info.expression = new aql::Expression(ast, condition->clone(ast));
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
    if (mem->isAttributeAccessForVariable(pathCmp)) {
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
  list.emplace_back(std::move(info));
}

void BaseTraverserOptions::clearVariableValues() {
  _ctx->clearVariableValues();
}

void BaseTraverserOptions::setVariableValue(
    aql::Variable const* var, aql::AqlValue const value) {
  _ctx->setVariableValue(var, value);
}

void BaseTraverserOptions::serializeVariables(
    VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenArray());
  _ctx->serializeAllVariables(_trx, builder);
}

arangodb::transaction::Methods* BaseTraverserOptions::trx() const {
  return _trx;
}

void BaseTraverserOptions::injectVelocyPackIndexes(VPackBuilder& builder) const {
  TRI_ASSERT(builder.isOpenObject());

  // base indexes
  builder.add("base", VPackValue(VPackValueType::Array));
  for (auto const& it : _baseLookupInfos) {
    for (auto const& it2 : it.idxHandles) {
      builder.openObject();
      it2.getIndex()->toVelocyPack(builder, false);
      builder.close();
    }
  }
  builder.close();
}

void BaseTraverserOptions::injectEngineInfo(VPackBuilder& result) const {
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

arangodb::aql::Expression* BaseTraverserOptions::getEdgeExpression(
    size_t cursorId) const {
  TRI_ASSERT(!_baseLookupInfos.empty());
  TRI_ASSERT(_baseLookupInfos.size() > cursorId);
  return _baseLookupInfos[cursorId].expression;
}

bool BaseTraverserOptions::evaluateExpression(
    arangodb::aql::Expression* expression, VPackSlice value) const {
  if (expression == nullptr) {
    return true;
  }

  TRI_ASSERT(!expression->isV8());
  expression->setVariable(_tmpVar, value);
  bool mustDestroy = false;
  aql::AqlValue res = expression->execute(_trx, _ctx, mustDestroy);
  TRI_ASSERT(res.isBoolean());
  bool result = res.toBoolean();
  expression->clearVariable(_tmpVar);
  if (mustDestroy) {
    res.destroy();
  }
  return result;
}

double BaseTraverserOptions::costForLookupInfoList(
    std::vector<BaseTraverserOptions::LookupInfo> const& list,
    size_t& createItems) const {
  double cost = 0;
  createItems = 0;
  for (auto const& li : list) {
    cost += li.estimateCost(createItems);
  }
  return cost;
}

TraverserOptions::TraverserOptions(
    arangodb::aql::Query* query, VPackSlice info, VPackSlice collections)
    : BaseTraverserOptions(query, info, collections),
      _baseVertexExpression(nullptr),
      minDepth(1),
      maxDepth(1),
      useBreadthFirst(false),
      uniqueVertices(UniquenessLevel::NONE),
      uniqueEdges(UniquenessLevel::PATH) {
      // NOTE collections is an array of arrays of strings
  VPackSlice read = info.get("minDepth");
  if (!read.isInteger()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The options require a minDepth");
  }
  minDepth = read.getNumber<uint64_t>();

  read = info.get("maxDepth");
  if (!read.isInteger()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The options require a maxDepth");
  }
  maxDepth = read.getNumber<uint64_t>();

  read = info.get("bfs");
  if (!read.isBoolean()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The options require a bfs");
  }
  useBreadthFirst = read.getBool();

  read = info.get("uniqueVertices");
  if (!read.isInteger()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The options require a uniqueVertices");
  }
  size_t i = read.getNumber<size_t>();
  switch (i) {
    case 0:
      uniqueVertices = UniquenessLevel::NONE;
      break;
    case 1:
      uniqueVertices = UniquenessLevel::PATH;
      break;
    case 2:
      uniqueVertices = UniquenessLevel::GLOBAL;
      break;
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "The options require a uniqueVertices");
  }

  read = info.get("uniqueEdges");
  if (!read.isInteger()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The options require a uniqueEdges");
  }
  i = read.getNumber<size_t>();
  switch (i) {
    case 0:
      uniqueEdges = UniquenessLevel::NONE;
      break;
    case 1:
      uniqueEdges = UniquenessLevel::PATH;
      break;
    case 2:
      uniqueEdges = UniquenessLevel::GLOBAL;
      break;
    default:
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "The options require a uniqueEdges");
  }

  read = info.get("depthLookupInfo");
  if (!read.isNone()) {
    if (!read.isObject()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "The options require depthLookupInfo to be an object");
    }
    size_t length = collections.length();
    _depthLookupInfo.reserve(read.length());
    for (auto const& depth : VPackObjectIterator(read)) {
      uint64_t d = basics::StringUtils::uint64(depth.key.copyString());
      auto it = _depthLookupInfo.emplace(d, std::vector<LookupInfo>());
      TRI_ASSERT(it.second);
      VPackSlice list = depth.value;
      TRI_ASSERT(length == list.length());
      it.first->second.reserve(length);
      for (size_t j = 0; j < length; ++j) {
        it.first->second.emplace_back(query, list.at(j), collections.at(j));
      }
    }
  }

  read = info.get("vertexExpressions");
  if (!read.isNone()) {
    if (!read.isObject()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "The options require vertexExpressions to be an object");
    }

    _vertexExpressions.reserve(read.length());
    for (auto const& info : VPackObjectIterator(read)) {
      uint64_t d = basics::StringUtils::uint64(info.key.copyString());
#ifdef ARANGODB_ENABLE_MAINAINER_MODE
      auto it = _vertexExpressions.emplace(
          d, new aql::Expression(query->ast(), info.value));
      TRI_ASSERT(it.second);
#else
      _vertexExpressions.emplace(
          d, new aql::Expression(query->ast(), info.value));
#endif
    }
  }

  read = info.get("baseVertexExpression");
  if (!read.isNone()) {
    if (!read.isObject()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(
          TRI_ERROR_BAD_PARAMETER,
          "The options require vertexExpressions to be an object");
    }
    _baseVertexExpression = new aql::Expression(query->ast(), read);
  }
  // Check for illegal option combination:
  TRI_ASSERT(uniqueEdges !=
             TraverserOptions::UniquenessLevel::GLOBAL);
  TRI_ASSERT(
      uniqueVertices !=
          TraverserOptions::UniquenessLevel::GLOBAL ||
      useBreadthFirst);
}

TraverserOptions::TraverserOptions(
    TraverserOptions const& other)
    : BaseTraverserOptions(other),
      _baseVertexExpression(nullptr),
      minDepth(other.minDepth),
      maxDepth(other.maxDepth),
      useBreadthFirst(other.useBreadthFirst),
      uniqueVertices(other.uniqueVertices),
      uniqueEdges(other.uniqueEdges) {
  TRI_ASSERT(other._depthLookupInfo.empty());
  TRI_ASSERT(other._vertexExpressions.empty());
  TRI_ASSERT(other._baseVertexExpression == nullptr);

  // Check for illegal option combination:
  TRI_ASSERT(uniqueEdges !=
             TraverserOptions::UniquenessLevel::GLOBAL);
  TRI_ASSERT(
      uniqueVertices !=
          TraverserOptions::UniquenessLevel::GLOBAL ||
      useBreadthFirst);
}

TraverserOptions::~TraverserOptions() {
  for (auto& pair : _vertexExpressions) {
    delete pair.second;
  }
  delete _baseVertexExpression;
}

void TraverserOptions::toVelocyPack(VPackBuilder& builder) const {
  VPackObjectBuilder guard(&builder);
  builder.add("minDepth", VPackValue(minDepth));
  builder.add("maxDepth", VPackValue(maxDepth));
  builder.add("bfs", VPackValue(useBreadthFirst));

  switch (uniqueVertices) {
    case TraverserOptions::UniquenessLevel::NONE:
      builder.add("uniqueVertices", VPackValue("none"));
      break;
    case TraverserOptions::UniquenessLevel::PATH:
      builder.add("uniqueVertices", VPackValue("path"));
      break;
    case TraverserOptions::UniquenessLevel::GLOBAL:
      builder.add("uniqueVertices", VPackValue("global"));
      break;
  }

  switch (uniqueEdges) {
    case TraverserOptions::UniquenessLevel::NONE:
      builder.add("uniqueEdges", VPackValue("none"));
      break;
    case TraverserOptions::UniquenessLevel::PATH:
      builder.add("uniqueEdges", VPackValue("path"));
      break;
    case TraverserOptions::UniquenessLevel::GLOBAL:
      builder.add("uniqueEdges", VPackValue("global"));
      break;
  }
  builder.add("type", VPackValue("traversal"));
}

void TraverserOptions::toVelocyPackIndexes(VPackBuilder& builder) const {
  VPackObjectBuilder guard(&builder);
  BaseTraverserOptions::injectVelocyPackIndexes(builder);
 
  // depth lookup indexes
  builder.add("levels", VPackValue(VPackValueType::Object));
  for (auto const& it : _depthLookupInfo) {
    builder.add(VPackValue(std::to_string(it.first)));
    builder.add(VPackValue(VPackValueType::Array));
    for (auto const& it2 : it.second) {
      for (auto const& it3 : it2.idxHandles) {
        builder.openObject();
        it3.getIndex()->toVelocyPack(builder, false);
        builder.close();
      }
    }
    builder.close();
  }
  builder.close();
}

void TraverserOptions::buildEngineInfo(VPackBuilder& result) const {
  result.openObject();
  BaseTraverserOptions::injectEngineInfo(result);
  result.add("minDepth", VPackValue(minDepth));
  result.add("maxDepth", VPackValue(maxDepth));
  result.add("bfs", VPackValue(useBreadthFirst));

  result.add(VPackValue("uniqueVertices"));
  switch (uniqueVertices) {
    case UniquenessLevel::NONE:
      result.add(VPackValue(0));
      break;
    case UniquenessLevel::PATH:
      result.add(VPackValue(1));
      break;
    case UniquenessLevel::GLOBAL:
      result.add(VPackValue(2));
      break;
  }

  result.add(VPackValue("uniqueEdges"));
  switch (uniqueEdges) {
    case UniquenessLevel::NONE:
      result.add(VPackValue(0));
      break;
    case UniquenessLevel::PATH:
      result.add(VPackValue(1));
      break;
    case UniquenessLevel::GLOBAL:
      result.add(VPackValue(2));
      break;
  }

  if (!_depthLookupInfo.empty()) {
    result.add(VPackValue("depthLookupInfo"));
    result.openObject();
    for (auto const& pair : _depthLookupInfo) {
      result.add(VPackValue(basics::StringUtils::itoa(pair.first)));
      result.openArray();
      for (auto const& it : pair.second) {
        it.buildEngineInfo(result);
      }
      result.close();
    }
    result.close();
  }

  if (!_vertexExpressions.empty()) {
    result.add(VPackValue("vertexExpressions"));
    result.openObject();
    for (auto const& pair : _vertexExpressions) {
      result.add(VPackValue(basics::StringUtils::itoa(pair.first)));
      result.openObject();
      result.add(VPackValue("expression"));
      pair.second->toVelocyPack(result, true);
      result.close();
    }
    result.close();
  }

  if (_baseVertexExpression != nullptr) {
    result.add(VPackValue("baseVertexExpression"));
    result.openObject();
    result.add(VPackValue("expression"));
    _baseVertexExpression->toVelocyPack(result, true);
    result.close();
  }
  result.add("type", VPackValue("traversal"));
  result.close();
}

bool arangodb::traverser::TraverserOptions::vertexHasFilter(
    uint64_t depth) const {
  if (_baseVertexExpression != nullptr) {
    return true;
  }
  return _vertexExpressions.find(depth) != _vertexExpressions.end();
}

bool TraverserOptions::evaluateEdgeExpression(
    arangodb::velocypack::Slice edge, arangodb::velocypack::Slice vertex,
    uint64_t depth, size_t cursorId) const {
  if (_isCoordinator) {
    // The Coordinator never checks conditions. The DBServer is responsible!
    return true;
  }
  arangodb::aql::Expression* expression = nullptr;

  auto specific = _depthLookupInfo.find(depth);

  if (specific != _depthLookupInfo.end()) {
    TRI_ASSERT(!specific->second.empty());
    TRI_ASSERT(specific->second.size() > cursorId);
    expression = specific->second[cursorId].expression;
  } else {
    expression = getEdgeExpression(cursorId);
  }
  if (expression == nullptr) {
    return true;
  }

  VPackValueLength vidLength;
  char const* vid = vertex.getString(vidLength);

  // inject _from/_to value
  auto node = expression->nodeForModification();

  TRI_ASSERT(node->numMembers() > 0);
  auto dirCmp = node->getMemberUnchecked(node->numMembers() - 1);
  TRI_ASSERT(dirCmp->type == aql::NODE_TYPE_OPERATOR_BINARY_EQ); 
  TRI_ASSERT(dirCmp->numMembers() == 2);

  auto idNode = dirCmp->getMemberUnchecked(1);
  TRI_ASSERT(idNode->type == aql::NODE_TYPE_VALUE);
  TRI_ASSERT(idNode->isValueType(aql::VALUE_TYPE_STRING));
  idNode->stealComputedValue();
  idNode->setStringValue(vid, vidLength);

  return evaluateExpression(expression, edge);
}

bool TraverserOptions::evaluateVertexExpression(
    arangodb::velocypack::Slice vertex, uint64_t depth) const {
  arangodb::aql::Expression* expression = nullptr;

  auto specific = _vertexExpressions.find(depth);

  if (specific != _vertexExpressions.end()) {
    expression = specific->second;
  } else {
    expression = _baseVertexExpression;
  }

  return evaluateExpression(expression, vertex);
}

arangodb::traverser::EdgeCursor*
TraverserOptions::nextCursor(ManagedDocumentResult* mmdr,
                                                  VPackSlice vertex,
                                                  uint64_t depth) const {
  if (_isCoordinator) {
    return nextCursorCoordinator(vertex, depth);
  }
  TRI_ASSERT(mmdr != nullptr);
  auto specific = _depthLookupInfo.find(depth);
  std::vector<LookupInfo> list;
  if (specific != _depthLookupInfo.end()) {
    list = specific->second;
  } else {
    list = _baseLookupInfos;
  }
  return nextCursorLocal(mmdr, vertex, depth, list);
}

arangodb::traverser::EdgeCursor*
TraverserOptions::nextCursorLocal(ManagedDocumentResult* mmdr,
    VPackSlice vertex, uint64_t depth, std::vector<LookupInfo>& list) const {
  TRI_ASSERT(mmdr != nullptr);
  auto allCursor = std::make_unique<SingleServerEdgeCursor>(mmdr, _trx, list.size());
  auto& opCursors = allCursor->getCursors();
  VPackValueLength vidLength;
  char const* vid = vertex.getString(vidLength);
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
      idNode->setStringValue(vid, vidLength);
    }
    std::vector<OperationCursor*> csrs;
    csrs.reserve(info.idxHandles.size());
    for (auto const& it : info.idxHandles) {
      csrs.emplace_back(_trx->indexScanForCondition(
          it, node, _tmpVar, mmdr, UINT64_MAX, 1000, false));
    }
    opCursors.emplace_back(std::move(csrs));
  }
  return allCursor.release();
}

arangodb::traverser::EdgeCursor*
TraverserOptions::nextCursorCoordinator(
    VPackSlice vertex, uint64_t depth) const {
  TRI_ASSERT(_traverser != nullptr);
  auto cursor = std::make_unique<ClusterEdgeCursor>(vertex, depth, _traverser);
  return cursor.release();
}

void TraverserOptions::linkTraverser(
    arangodb::traverser::ClusterTraverser* trav) {
  _traverser = trav;
}

double TraverserOptions::estimateCost(size_t& nrItems) const {
  size_t count = 1;
  double cost = 0;
  size_t baseCreateItems = 0;
  double baseCost = costForLookupInfoList(_baseLookupInfos, baseCreateItems);

  for (uint64_t depth = 0; depth < maxDepth; ++depth) {
    auto liList = _depthLookupInfo.find(depth);
    if (liList == _depthLookupInfo.end()) {
      // No LookupInfo for this depth use base
      cost += baseCost * count;
      count *= baseCreateItems;
    } else {
      size_t createItems = 0;
      double depthCost = costForLookupInfoList(liList->second, createItems);
      cost += depthCost * count;
      count *= createItems;
    }
  }
  nrItems = count;
  return cost;
}

ShortestPathOptions::ShortestPathOptions(arangodb::aql::Query* query,
                                         VPackSlice info,
                                         VPackSlice collections,
                                         VPackSlice reverseCollections)
    : BaseTraverserOptions(query, info, collections),
      _defaultWeight(1),
      _weightAttribute("") {
  VPackSlice read = info.get("reverseLookupInfos");

  if (!read.isArray()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The options require a reverseLookupInfos");
  }
  size_t length = read.length();
  TRI_ASSERT(read.length() == reverseCollections.length());
  _reverseLookupInfos.reserve(length);
  for (size_t j = 0; j < length; ++j) {
    _reverseLookupInfos.emplace_back(query, read.at(j), reverseCollections.at(j));
  }

  read = info.get("weightAttribute");
  if (read.isString()) {
    _weightAttribute = read.copyString();

    read = info.get("defaultWeight");
    if (read.isNumber()) {
      _defaultWeight = read.getNumericValue<double>();
    }
  }
}

void ShortestPathOptions::toVelocyPack(VPackBuilder& builder) const {
  VPackObjectBuilder guard(&builder);
  builder.add("type", VPackValue("shortest"));
  // FIXME
}

void ShortestPathOptions::buildEngineInfo(VPackBuilder& result) const {
  VPackObjectBuilder guard(&result);
  BaseTraverserOptions::injectEngineInfo(result);
  result.add(VPackValue("reverseLookupInfos"));
  result.openArray();
  for (auto const& it : _reverseLookupInfos) {
    it.buildEngineInfo(result);
  }
  result.close();
  if (usesWeight()) {
    result.add("weightAttribute", VPackValue(weightAttribute()));
    result.add("defaultWeight", VPackValue(defaultWeight()));
  }
  result.add("type", VPackValue("shortest"));
}

void ShortestPathOptions::addReverseLookupInfo(
    aql::Ast* ast, std::string const& collectionName,
    std::string const& attributeName, aql::AstNode* condition) {
  injectLookupInfoInList(_reverseLookupInfos, ast, collectionName,
                         attributeName, condition);
}

double ShortestPathOptions::estimateCost(size_t& nrItems) const {
  // We estimate the cost with the 7 degrees of separation:
  // The shortest path between two vertices is most likely
  // less than seven hops long.
  size_t edgesCount = 0;
  double baseCost = costForLookupInfoList(_baseLookupInfos, edgesCount);
  baseCost = std::pow(baseCost, 7);
  nrItems = static_cast<size_t>(std::pow(edgesCount, 7));
  return nrItems;
}
