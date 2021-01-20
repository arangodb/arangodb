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

#include "TraverserOptions.h"

#include "Aql/Ast.h"
#include "Aql/Expression.h"
#include "Aql/PruneExpressionEvaluator.h"
#include "Aql/QueryContext.h"
#include "Basics/StringUtils.h"
#include "Basics/VelocyPackHelper.h"
#include "Basics/tryEmplaceHelper.h"
#include "Cluster/ClusterEdgeCursor.h"
#include "Graph/SingleServerEdgeCursor.h"
#include "Graph/SingleServerTraverser.h"
#include "Indexes/Index.h"

#include <velocypack/Iterator.h>
#include <velocypack/StringRef.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::graph;
using namespace arangodb::transaction;
using namespace arangodb::traverser;
using VPackHelper = arangodb::basics::VelocyPackHelper;

namespace {
arangodb::velocypack::StringRef getEdgeDestination(arangodb::velocypack::Slice edge,
                                                   arangodb::velocypack::StringRef origin) {
  if (edge.isString()) {
    return edge.stringRef();
  }

  TRI_ASSERT(edge.isObject());
  auto from = edge.get(arangodb::StaticStrings::FromString);
  TRI_ASSERT(from.isString());
  if (from.stringRef() == origin) {
    auto to = edge.get(arangodb::StaticStrings::ToString);
    TRI_ASSERT(to.isString());
    return to.stringRef();
  }
  return from.stringRef();
}
}  // namespace

TraverserOptions::TraverserOptions(arangodb::aql::QueryContext& query)
    : BaseOptions(query),
      _baseVertexExpression(nullptr),
      _traverser(nullptr),
      minDepth(1),
      maxDepth(1),
      useNeighbors(false),
      uniqueVertices(UniquenessLevel::NONE),
      uniqueEdges(UniquenessLevel::PATH),
      mode(Order::DFS),
      defaultWeight(1.0) {}

TraverserOptions::TraverserOptions(arangodb::aql::QueryContext& query,
                                   VPackSlice obj)
    : TraverserOptions(query) {
  TRI_ASSERT(obj.isObject());

#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
  VPackSlice type = obj.get("type");
  TRI_ASSERT(type.isString());
  TRI_ASSERT(type.isEqualString("traversal"));
#endif

  minDepth = VPackHelper::getNumericValue<uint64_t>(obj, "minDepth", 1);
  maxDepth = VPackHelper::getNumericValue<uint64_t>(obj, "maxDepth", 1);
  _parallelism = VPackHelper::getNumericValue<size_t>(obj, "parallelism", 1);
  _refactor = VPackHelper::getBooleanValue(obj, StaticStrings::GraphRefactorFlag, false);
  TRI_ASSERT(minDepth <= maxDepth);

  std::string tmp = VPackHelper::getStringValue(obj, StaticStrings::GraphQueryOrder, "");
  if (!tmp.empty()) {
    if (tmp == StaticStrings::GraphQueryOrderBFS) {
      mode = Order::BFS;
    } else if (tmp == StaticStrings::GraphQueryOrderWeighted) {
      mode = Order::WEIGHTED;
    } else if (tmp == StaticStrings::GraphQueryOrderBFS) {
      mode = Order::DFS;
    }
  } else {
    bool useBreadthFirst = VPackHelper::getBooleanValue(obj, "bfs", false);
    if (useBreadthFirst) {
      mode = Order::BFS;
    }
  }

  useNeighbors = VPackHelper::getBooleanValue(obj, "neighbors", false);

  TRI_ASSERT(!useNeighbors || isUseBreadthFirst());

  tmp = VPackHelper::getStringValue(obj, "uniqueVertices", "");
  if (tmp == "path") {
    uniqueVertices = TraverserOptions::UniquenessLevel::PATH;
  } else if (tmp == "global") {
    if (mode != Order::BFS && mode != Order::WEIGHTED) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                     "uniqueVertices: 'global' is only "
                                     "supported, with mode: bfs|weighted due to "
                                     "otherwise unpredictable results.");
    }
    uniqueVertices = TraverserOptions::UniquenessLevel::GLOBAL;
  } else {
    uniqueVertices = TraverserOptions::UniquenessLevel::NONE;
  }

  tmp = VPackHelper::getStringValue(obj, "uniqueEdges", "");
  if (tmp == "none") {
    uniqueEdges = TraverserOptions::UniquenessLevel::NONE;
  } else if (tmp == "global") {
    THROW_ARANGO_EXCEPTION_MESSAGE(
        TRI_ERROR_BAD_PARAMETER,
        "uniqueEdges: 'global' is not supported, "
        "due to otherwise unpredictable results. Use 'path' "
        "or 'none' instead");
  } else {
    uniqueEdges = TraverserOptions::UniquenessLevel::PATH;
  }

  weightAttribute = VPackHelper::getStringValue(obj, "weightAttribute", "");
  defaultWeight = VPackHelper::getNumericValue<double>(obj, "defaultWeight", 1);

  VPackSlice read = obj.get("vertexCollections");
  if (read.isString()) {
    auto c = read.stringRef();
    vertexCollections.emplace_back(c.data(), c.size());
  } else if (read.isArray()) {
    for (auto slice : VPackArrayIterator(read)) {
      if (!slice.isString()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_BAD_PARAMETER,
            "The options require vertexCollections to "
            "be a string or array of strings");
      }
      auto c = slice.stringRef();
      vertexCollections.emplace_back(c.data(), c.size());
    }
  } else if (!read.isNone()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The options require vertexCollections to "
                                   "be a string or array of strings");
  }

  read = obj.get("edgeCollections");
  if (read.isString()) {
    auto c = read.stringRef();
    edgeCollections.emplace_back(c.data(), c.size());
  } else if (read.isArray()) {
    for (auto slice : VPackArrayIterator(read)) {
      if (!slice.isString()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                       "The options require edgeCollections to "
                                       "be a string or array of strings");
      }
      auto c = slice.stringRef();
      edgeCollections.emplace_back(c.data(), c.size());
    }
  } else if (!read.isNone()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The options require edgeCollections to "
                                   "be a string or array of strings");
  }

  _produceVertices = VPackHelper::getBooleanValue(obj, "produceVertices", true);
}

TraverserOptions::TraverserOptions(arangodb::aql::QueryContext& query, VPackSlice info,
                                   VPackSlice collections)
    : BaseOptions(query, info, collections),
      _baseVertexExpression(nullptr),
      _traverser(nullptr),
      minDepth(1),
      maxDepth(1),
      useNeighbors(false),
      uniqueVertices(UniquenessLevel::NONE),
      uniqueEdges(UniquenessLevel::PATH),
      mode(Order::DFS) {

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

  read = info.get(StaticStrings::GraphQueryOrder);
  if (!read.isNone()) {
    if (!read.isNumber<size_t>()) {
      THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
          "The options require a order");
    }

    size_t i = read.getNumber<size_t>();
    switch (i) {
      case 0:
        mode = Order::DFS;
        break;
      case 1:
        mode = Order::BFS;
        break;
      case 2:
        mode = Order::WEIGHTED;
        break;
      default:
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
            "Bad mode parameter value");
    }
  } else {
    read = info.get("bfs");
    if (read.isBoolean()) {
      bool useBreadthFirst = read.getBool();
      if (useBreadthFirst) {
        mode = Order::BFS;
      }
    }
  }

  read = info.get("neighbors");
  if (read.isBoolean()) {
    useNeighbors = read.getBool();
  }
  TRI_ASSERT(!useNeighbors || isUseBreadthFirst());

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

  weightAttribute = VPackHelper::getStringValue(info, "weightAttribute", "");
  defaultWeight = VPackHelper::getNumericValue<double>(info, "defaultWeight", 1);

  read = info.get("vertexCollections");
  if (read.isString()) {
    auto c = read.stringRef();
    vertexCollections.emplace_back(c.data(), c.size());
  } else if (read.isArray()) {
    for (auto slice : VPackArrayIterator(read)) {
      if (!slice.isString()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(
            TRI_ERROR_BAD_PARAMETER,
            "The options require vertexCollections to "
            "be a string or array of strings");
      }
      auto c = slice.stringRef();
      vertexCollections.emplace_back(c.data(), c.size());
    }
  } else if (!read.isNone()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The options require vertexCollections to "
                                   "be a string or array of strings");
  }

  read = info.get("edgeCollections");
  if (read.isString()) {
    auto c = read.stringRef();
    edgeCollections.emplace_back(c.data(), c.size());
  } else if (read.isArray()) {
    for (auto slice : VPackArrayIterator(read)) {
      if (!slice.isString()) {
        THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                       "The options require edgeCollections to "
                                       "be a string or array of strings");
      }
      auto c = slice.stringRef();
      edgeCollections.emplace_back(c.data(), c.size());
    }
  } else if (!read.isNone()) {
    THROW_ARANGO_EXCEPTION_MESSAGE(TRI_ERROR_BAD_PARAMETER,
                                   "The options require edgeCollections to "
                                   "be a string or array of strings");
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
      auto [it, emplaced] = _depthLookupInfo.try_emplace(d, std::vector<LookupInfo>());
      TRI_ASSERT(emplaced);
      VPackSlice list = depth.value;
      TRI_ASSERT(length == list.length());
      it->second.reserve(length);
      for (size_t j = 0; j < length; ++j) {
        it->second.emplace_back(query, list.at(j), collections.at(j));
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
#ifdef ARANGODB_ENABLE_MAINTAINER_MODE
      bool emplaced = false;
      std::tie(std::ignore, emplaced) =
          _vertexExpressions.try_emplace(d, new aql::Expression(query.ast(), info.value));
      TRI_ASSERT(emplaced);
#else
      _vertexExpressions.try_emplace(d, arangodb::lazyConstruct([&] {
                                       return new aql::Expression(query.ast(), info.value);
                                     }));
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
    _baseVertexExpression.reset(new aql::Expression(query.ast(), read));
  }
  // Check for illegal option combination:
  TRI_ASSERT(uniqueEdges != TraverserOptions::UniquenessLevel::GLOBAL);
  TRI_ASSERT(uniqueVertices != TraverserOptions::UniquenessLevel::GLOBAL || isUniqueGlobalVerticesAllowed());

  _produceVertices = VPackHelper::getBooleanValue(info, "produceVertices", true);
}

TraverserOptions::TraverserOptions(TraverserOptions const& other, bool const allowAlreadyBuiltCopy)
    : BaseOptions(static_cast<BaseOptions const&>(other), allowAlreadyBuiltCopy),
      _baseVertexExpression(nullptr),
      _traverser(nullptr),
      minDepth(other.minDepth),
      maxDepth(other.maxDepth),
      useNeighbors(other.useNeighbors),
      uniqueVertices(other.uniqueVertices),
      uniqueEdges(other.uniqueEdges),
      mode(other.mode),
      weightAttribute(other.weightAttribute),
      defaultWeight(other.defaultWeight),
      vertexCollections(other.vertexCollections),
      edgeCollections(other.edgeCollections) {
  if (!allowAlreadyBuiltCopy) {
    TRI_ASSERT(other._baseLookupInfos.empty());
    TRI_ASSERT(other._depthLookupInfo.empty());
    TRI_ASSERT(other._vertexExpressions.empty());
    TRI_ASSERT(other._tmpVar == nullptr);
    TRI_ASSERT(other._baseVertexExpression == nullptr);
  }

  // Check for illegal option combination:
  TRI_ASSERT(uniqueEdges != TraverserOptions::UniquenessLevel::GLOBAL);
  TRI_ASSERT(uniqueVertices != TraverserOptions::UniquenessLevel::GLOBAL ||
             isUniqueGlobalVerticesAllowed());
}

TraverserOptions::~TraverserOptions() = default;

void TraverserOptions::toVelocyPack(VPackBuilder& builder) const {
  VPackObjectBuilder guard(&builder);

  builder.add("minDepth", VPackValue(minDepth));
  builder.add("maxDepth", VPackValue(maxDepth));
  builder.add("parallelism", VPackValue(_parallelism));
  builder.add(StaticStrings::GraphRefactorFlag, VPackValue(refactor()));
  
  builder.add("neighbors", VPackValue(useNeighbors));

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

  switch (mode) {
    case TraverserOptions::Order::DFS:
      builder.add(StaticStrings::GraphQueryOrder, VPackValue(StaticStrings::GraphQueryOrderDFS));
      break;
    case TraverserOptions::Order::BFS:
      builder.add(StaticStrings::GraphQueryOrder, VPackValue(StaticStrings::GraphQueryOrderBFS));
      break;
    case TraverserOptions::Order::WEIGHTED:
      builder.add(StaticStrings::GraphQueryOrder, VPackValue(StaticStrings::GraphQueryOrderWeighted));
      break;
  }

  builder.add("weightAttribute", VPackValue(weightAttribute));
  builder.add("defaultWeight", VPackValue(defaultWeight));

  if (!vertexCollections.empty()) {
    VPackArrayBuilder guard(&builder, "vertexCollections");
    for (auto& c : vertexCollections) {
      builder.add(VPackValue(c));
    }
  }

  if (!edgeCollections.empty()) {
    VPackArrayBuilder guard(&builder, "edgeCollections");
    for (auto& c : edgeCollections) {
      builder.add(VPackValue(c));
    }
  }

  builder.add("produceVertices", VPackValue(_produceVertices));
  builder.add("type", VPackValue("traversal"));
}

void TraverserOptions::toVelocyPackIndexes(VPackBuilder& builder) const {
  VPackObjectBuilder guard(&builder);

  // base indexes
  builder.add("base", VPackValue(VPackValueType::Array));
  for (auto const& it : _baseLookupInfos) {
    for (auto const& it2 : it.idxHandles) {
      it2->toVelocyPack(builder, Index::makeFlags(Index::Serialize::Basics,
                                                  Index::Serialize::Estimates));
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
        it3->toVelocyPack(builder, Index::makeFlags(Index::Serialize::Basics,
                                                    Index::Serialize::Estimates));
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
  result.add("parallelism", VPackValue(_parallelism));
  result.add(StaticStrings::GraphRefactorFlag, VPackValue(_refactor));
  result.add("neighbors", VPackValue(useNeighbors));

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

  result.add(VPackValue(StaticStrings::GraphQueryOrder));
  switch (mode) {
    case Order::DFS:
      result.add(VPackValue(0));
      break;
    case Order::BFS:
      result.add(VPackValue(1));
      break;
    case Order::WEIGHTED:
      result.add(VPackValue(2));
      break;
  }

  result.add("weightAttribute", VPackValue(weightAttribute));
  result.add("defaultWeight", VPackValue(defaultWeight));

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

  if (!vertexCollections.empty()) {
    VPackArrayBuilder guard(&result, "vertexCollections");
    for (auto& c : vertexCollections) {
      result.add(VPackValue(c));
    }
  }

  if (!edgeCollections.empty()) {
    VPackArrayBuilder guard(&result, "edgeCollections");
    for (auto& c : edgeCollections) {
      result.add(VPackValue(c));
    }
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

bool TraverserOptions::shouldExcludeEdgeCollection(std::string const& name) const {
  return !edgeCollections.empty() &&
         std::find(edgeCollections.begin(), edgeCollections.end(), name) ==
             edgeCollections.end();
}

void TraverserOptions::addDepthLookupInfo(aql::ExecutionPlan* plan,
                                          std::string const& collectionName,
                                          std::string const& attributeName,
                                          aql::AstNode* condition, uint64_t depth,
                                          bool onlyEdgeIndexes) {
  auto& list = _depthLookupInfo[depth];
  injectLookupInfoInList(list, plan, collectionName, attributeName, condition, onlyEdgeIndexes);
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
    expression = specific->second[cursorId].expression.get();
  } else {
    bool unused;
    expression = getEdgeExpression(cursorId, unused);
  }
  return expression != nullptr;
}

bool TraverserOptions::hasVertexCollectionRestrictions() const {
  return !vertexCollections.empty();
}

bool TraverserOptions::evaluateEdgeExpression(arangodb::velocypack::Slice edge,
                                              arangodb::velocypack::StringRef vertexId,
                                              uint64_t depth, size_t cursorId) {
  arangodb::aql::Expression* expression = nullptr;

  auto specific = _depthLookupInfo.find(depth);
  auto needToInjectVertex = false;

  if (specific != _depthLookupInfo.end()) {
    TRI_ASSERT(!specific->second.empty());
    TRI_ASSERT(specific->second.size() > cursorId);
    expression = specific->second[cursorId].expression.get();
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
    idNode->setStringValue(vertexId.data(), vertexId.length());
  }
  edge = edge.resolveExternal();
  return evaluateExpression(expression, edge);
}

auto TraverserOptions::explicitDepthLookupAt() const -> std::unordered_set<std::size_t> {
  std::unordered_set<std::size_t> result;

  for (auto&& pair : _depthLookupInfo) {
    result.insert(pair.first);
  }
  return result;
}

bool TraverserOptions::evaluateVertexExpression(arangodb::velocypack::Slice vertex,
                                                uint64_t depth) {
  arangodb::aql::Expression* expression = nullptr;

  auto specific = _vertexExpressions.find(depth);

  if (specific != _vertexExpressions.end()) {
    expression = specific->second.get();
  } else {
    expression = _baseVertexExpression.get();
  }

  vertex = vertex.resolveExternal();
  return evaluateExpression(expression, vertex);
}

bool TraverserOptions::destinationCollectionAllowed(VPackSlice edge,
                                                    velocypack::StringRef sourceVertex) {
  if (hasVertexCollectionRestrictions()) {
    auto destination = ::getEdgeDestination(edge, sourceVertex);
    auto collection = transaction::helpers::extractCollectionFromId(destination);
    if (std::find(vertexCollections.begin(), vertexCollections.end(),
                  std::string_view(collection.data(), collection.size())) ==
        vertexCollections.end()) {
      // collection not found
      return false;
    }
  }

  return true;
}

std::unique_ptr<EdgeCursor> arangodb::traverser::TraverserOptions::buildCursor(uint64_t depth) {
  ensureCache();

  if (_isCoordinator) {
    return std::make_unique<ClusterTraverserEdgeCursor>(this);
  }

  auto specific = _depthLookupInfo.find(depth);
  if (specific != _depthLookupInfo.end()) {
    return std::make_unique<graph::SingleServerEdgeCursor>(this, _tmpVar, nullptr,
                                                           specific->second);
  }

  return std::make_unique<graph::SingleServerEdgeCursor>(this, _tmpVar, nullptr, _baseLookupInfos);
}

void TraverserOptions::linkTraverser(ClusterTraverser* trav) {
  _traverser = trav;
}

double TraverserOptions::estimateCost(size_t& nrItems) const {
  size_t count = 1;
  double cost = 0;
  size_t baseCreateItems = 0;
  double baseCost = costForLookupInfoList(_baseLookupInfos, baseCreateItems);

  for (uint64_t depth = 0; depth < maxDepth && depth < 10; ++depth) {
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

  if (maxDepth > 10) {
    // We have a too high depth this cost will be pruned anyway
    cost *= (maxDepth - 10) * 10;
    count *= (maxDepth - 10) * 10;
  }
  nrItems = count;
  return cost;
}

void TraverserOptions::activatePrune(std::vector<aql::Variable const*> vars,
                                     std::vector<aql::RegisterId> regs,
                                     size_t vertexVarIdx, size_t edgeVarIdx,
                                     size_t pathVarIdx, aql::Expression* expr) {
  _pruneExpression = std::make_unique<aql::PruneExpressionEvaluator>(
      _trx, _query, _aqlFunctionsInternalCache, std::move(vars),
      std::move(regs), vertexVarIdx, edgeVarIdx, pathVarIdx, expr);
}

double TraverserOptions::weightEdge(VPackSlice edge) const {
  TRI_ASSERT(mode == Order::WEIGHTED);
  return arangodb::basics::VelocyPackHelper::getNumericValue<double>(edge, weightAttribute,
                                                                     defaultWeight);
}

bool TraverserOptions::hasWeightAttribute() const {
  return !weightAttribute.empty();
}

auto TraverserOptions::estimateDepth() const noexcept -> uint64_t {
  // Upper bind this by a random number.
  // The depth will be used as a power for the estimates.
  // So having power 7 is evil enough...
  return std::min(maxDepth, static_cast<uint64_t>(7));
}
