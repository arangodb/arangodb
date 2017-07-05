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
#include "Aql/Expression.h"
#include "Aql/Query.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterEdgeCursor.h"
#include "Indexes/Index.h"
#include "VocBase/SingleServerTraverser.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::graph;
using namespace arangodb::transaction;
using namespace arangodb::traverser;
using VPackHelper = arangodb::basics::VelocyPackHelper;

TraverserOptions::TraverserOptions(transaction::Methods* trx)
    : BaseOptions(trx),
      _baseVertexExpression(nullptr),
      _traverser(nullptr),
      minDepth(1),
      maxDepth(1),
      useBreadthFirst(false),
      uniqueVertices(UniquenessLevel::NONE),
      uniqueEdges(UniquenessLevel::PATH) {}

TraverserOptions::TraverserOptions(transaction::Methods* trx,
                                   VPackSlice const& obj)
    : BaseOptions(trx),
      _baseVertexExpression(nullptr),
      _traverser(nullptr),
      minDepth(1),
      maxDepth(1),
      useBreadthFirst(false),
      uniqueVertices(UniquenessLevel::NONE),
      uniqueEdges(UniquenessLevel::PATH) {
  TRI_ASSERT(obj.isObject());

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  VPackSlice type = obj.get("type");
  TRI_ASSERT(type.isString());
  TRI_ASSERT(type.isEqualString("traversal"));
#endif

  minDepth = VPackHelper::getNumericValue<uint64_t>(obj, "minDepth", 1);
  maxDepth = VPackHelper::getNumericValue<uint64_t>(obj, "maxDepth", 1);
  TRI_ASSERT(minDepth <= maxDepth);
  useBreadthFirst = VPackHelper::getBooleanValue(obj, "bfs", false);
  std::string tmp = VPackHelper::getStringValue(obj, "uniqueVertices", "");
  if (tmp == "path") {
    uniqueVertices = TraverserOptions::UniquenessLevel::PATH;
  } else if (tmp == "global") {
    if (!useBreadthFirst) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "uniqueVertices: 'global' is only "
                                     "supported, with bfs: true due to "
                                     "unpredictable results.");
    }
    uniqueVertices = TraverserOptions::UniquenessLevel::GLOBAL;
  } else {
    uniqueVertices = TraverserOptions::UniquenessLevel::NONE;
  }

  tmp = VPackHelper::getStringValue(obj, "uniqueEdges", "");
  if (tmp == "none") {
    uniqueEdges = TraverserOptions::UniquenessLevel::NONE;
  } else if (tmp == "global") {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "uniqueEdges: 'global' is not supported, "
                                   "due to unpredictable results. Use 'path' "
                                   "or 'none' instead");
  } else {
    uniqueEdges = TraverserOptions::UniquenessLevel::PATH;
  }
}

arangodb::traverser::TraverserOptions::TraverserOptions(
    arangodb::aql::Query* query, VPackSlice info, VPackSlice collections)
    : BaseOptions(query, info, collections),
      _baseVertexExpression(nullptr),
      _traverser(nullptr),
      minDepth(1),
      maxDepth(1),
      useBreadthFirst(false),
      uniqueVertices(UniquenessLevel::NONE),
      uniqueEdges(UniquenessLevel::PATH) {
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  VPackSlice type = info.get("type");
  TRI_ASSERT(type.isString());
  TRI_ASSERT(type.isEqualString("traversal"));
#endif

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
    _depthLookupInfo.reserve(read.length());
    size_t length = collections.length();
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
      _vertexExpressions.emplace(d,
                                 new aql::Expression(query->ast(), info.value));
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
  TRI_ASSERT(uniqueEdges != TraverserOptions::UniquenessLevel::GLOBAL);
  TRI_ASSERT(uniqueVertices != TraverserOptions::UniquenessLevel::GLOBAL ||
             useBreadthFirst);
}

arangodb::traverser::TraverserOptions::TraverserOptions(
    TraverserOptions const& other)
    : BaseOptions(other.trx()),
      _baseVertexExpression(nullptr),
      _traverser(nullptr),
      minDepth(other.minDepth),
      maxDepth(other.maxDepth),
      useBreadthFirst(other.useBreadthFirst),
      uniqueVertices(other.uniqueVertices),
      uniqueEdges(other.uniqueEdges) {
  TRI_ASSERT(other._baseLookupInfos.empty());
  TRI_ASSERT(other._depthLookupInfo.empty());
  TRI_ASSERT(other._vertexExpressions.empty());
  TRI_ASSERT(other._tmpVar == nullptr);
  TRI_ASSERT(other._baseVertexExpression == nullptr);

  // Check for illegal option combination:
  TRI_ASSERT(uniqueEdges != TraverserOptions::UniquenessLevel::GLOBAL);
  TRI_ASSERT(uniqueVertices != TraverserOptions::UniquenessLevel::GLOBAL ||
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

  // base indexes
  builder.add("base", VPackValue(VPackValueType::Array));
  for (auto const& it : _baseLookupInfos) {
    for (auto const& it2 : it.idxHandles) {
      it2.getIndex()->toVelocyPack(builder, false, false);
    }
  }
  builder.close();

  // depth lookup indexes
  builder.add("levels", VPackValue(VPackValueType::Object));
  for (auto const& it : _depthLookupInfo) {
    builder.add(VPackValue(std::to_string(it.first)));
    builder.add(VPackValue(VPackValueType::Array));
    for (auto const& it2 : it.second) {
      for (auto const& it3 : it2.idxHandles) {
        it3.getIndex()->toVelocyPack(builder, false, false);
      }
    }
    builder.close();
  }
  builder.close();
}

void TraverserOptions::buildEngineInfo(VPackBuilder& result) const {
  result.openObject();
  injectEngineInfo(result);
  result.add("type", VPackValue("traversal"));
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

  result.close();
}

void TraverserOptions::addDepthLookupInfo(aql::ExecutionPlan* plan,
                                          std::string const& collectionName,
                                          std::string const& attributeName,
                                          aql::AstNode* condition,
                                          uint64_t depth) {
  auto& list = _depthLookupInfo[depth];
  injectLookupInfoInList(list, plan, collectionName, attributeName, condition);
}

bool TraverserOptions::vertexHasFilter(uint64_t depth) const {
  if (_baseVertexExpression != nullptr) {
    return true;
  }
  return _vertexExpressions.find(depth) != _vertexExpressions.end();
}

bool TraverserOptions::hasEdgeFilter(int64_t depth, size_t cursorId) const {
  if (_isCoordinator) {
    // The Coordinator never checks conditions. The DBServer is responsible!
    return false;
  }
  arangodb::aql::Expression* expression = nullptr;
  
  auto specific = _depthLookupInfo.find(depth);
  
  if (specific != _depthLookupInfo.end()) {
    TRI_ASSERT(!specific->second.empty());
    TRI_ASSERT(specific->second.size() > cursorId);
    expression = specific->second[cursorId].expression;
  } else {
    bool unused;
    expression = getEdgeExpression(cursorId, unused);
  }
  return expression != nullptr;
}


bool TraverserOptions::evaluateEdgeExpression(arangodb::velocypack::Slice edge,
                                              StringRef vertexId,
                                              uint64_t depth,
                                              size_t cursorId) const {
  arangodb::aql::Expression* expression = nullptr;

  auto specific = _depthLookupInfo.find(depth);
  auto needToInjectVertex = false;

  if (specific != _depthLookupInfo.end()) {
    TRI_ASSERT(!specific->second.empty());
    TRI_ASSERT(specific->second.size() > cursorId);
    expression = specific->second[cursorId].expression;
    needToInjectVertex = !specific->second[cursorId].conditionNeedUpdate;
  } else {
    expression = getEdgeExpression(cursorId, needToInjectVertex);

  }
  if (expression == nullptr) {
    return true;
  }

  if (needToInjectVertex) {
    // If we have to inject the vertex value it has to be within
    // the last member of the condition.
    // We only get into this case iff the index used does
    // not cover _from resp. _to.
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
    idNode->setStringValue(vertexId.data(), vertexId.length());
  }
  if (edge.isExternal()) {
    edge = edge.resolveExternal();
  }
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

  if (vertex.isExternal()) {
    vertex = vertex.resolveExternal();
  }
  return evaluateExpression(expression, vertex);
}

EdgeCursor* arangodb::traverser::TraverserOptions::nextCursor(
    ManagedDocumentResult* mmdr, StringRef vid, uint64_t depth) {
  if (_isCoordinator) {
    return nextCursorCoordinator(vid, depth);
  }
  TRI_ASSERT(mmdr != nullptr);
  auto specific = _depthLookupInfo.find(depth);
  std::vector<LookupInfo> list;
  if (specific != _depthLookupInfo.end()) {
    list = specific->second;
  } else {
    list = _baseLookupInfos;
  }
  return nextCursorLocal(mmdr, vid, list);
}

EdgeCursor* TraverserOptions::nextCursorCoordinator(StringRef vid,
                                                    uint64_t depth) {
  TRI_ASSERT(_traverser != nullptr);
  auto cursor = std::make_unique<ClusterEdgeCursor>(vid, depth, this);
  return cursor.release();
}

void TraverserOptions::linkTraverser(ClusterTraverser* trav) {
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
