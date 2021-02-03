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
/// @author Heiko Kernbach
////////////////////////////////////////////////////////////////////////////////

#include "RefactoredClusterTraverserCache.h"

#include "Aql/AqlValue.h"
#include "Aql/Query.h"
#include "Graph/BaseOptions.h"
#include "Graph/EdgeDocumentToken.h"

using namespace arangodb;
using namespace arangodb::basics;
using namespace arangodb::graph;

RefactoredClusterTraverserCache::RefactoredClusterTraverserCache(
    aql::QueryContext& query,
    std::unordered_map<ServerID, aql::EngineId> const* engines, BaseOptions* options)
    : TraverserCache(query, options),
      _datalake(options->resourceMonitor()),
      _engines(engines) {}


bool RefactoredClusterTraverserCache::appendVertex(arangodb::velocypack::StringRef id,
                                                   VPackBuilder& result) {
  TRI_ASSERT(false);
  // There will be no idString of length above uint32_t
  auto vertexSlice = getCachedVertex(VertexType{id.data(), static_cast<uint32_t>(id.length())});

  if (vertexSlice.isNone()) {
    // Register a warning. It is okay though but helps the user
    std::string msg = "vertex '" + id.toString() + "' not found";
    _query.warnings().registerWarning(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, msg.c_str());

    // Document not found append NULL
    result.add(arangodb::velocypack::Slice::nullSlice());
    return false;
  }

  result.add(vertexSlice);
  return true;
}

bool RefactoredClusterTraverserCache::appendVertex(arangodb::velocypack::StringRef id,
                                                   arangodb::aql::AqlValue& result) {
  TRI_ASSERT(false);
  // There will be no idString of length above uint32_t
  auto vertexSlice = getCachedVertex(VertexType{id.data(), static_cast<uint32_t>(id.length())});

  if (vertexSlice.isNone()) {
    // Register a warning. It is okay though but helps the user
    std::string msg = "vertex '" + id.toString() + "' not found";
    _query.warnings().registerWarning(TRI_ERROR_ARANGO_DOCUMENT_NOT_FOUND, msg.c_str());

    // Document not found append NULL
    result = arangodb::aql::AqlValue(arangodb::aql::AqlValueHintNull());
    return false;
  }

  result = arangodb::aql::AqlValue(vertexSlice);
  return true;
}

auto RefactoredClusterTraverserCache::cacheVertex(VertexType const& vertexId,
                                                  velocypack::Slice vertexSlice) -> void {
  _vertexData.try_emplace(vertexId, vertexSlice);
}

namespace {
VertexType getEdgeDestination(arangodb::velocypack::Slice edge, VertexType const& origin) {
  if (edge.isString()) {
    return VertexType{edge};
  }

  TRI_ASSERT(edge.isObject());
  auto from = edge.get(arangodb::StaticStrings::FromString);
  TRI_ASSERT(from.isString());
  if (from.stringRef() == origin.stringRef()) {
    auto to = edge.get(arangodb::StaticStrings::ToString);
    TRI_ASSERT(to.isString());
    return VertexType{to};
  }
  return VertexType{from};
}
}  // namespace

auto RefactoredClusterTraverserCache::cacheEdge(VertexType origin, EdgeType edgeId,
                                                velocypack::Slice edgeSlice,
                                                bool backward) -> void {
  TRI_ASSERT(isVertexCached(origin));

  auto edgeToEmplace =
      std::pair{edgeId, VertexType{getEdgeDestination(edgeSlice, origin)}};
  std::vector<std::pair<EdgeType, VertexType>> initVector{};

  // TODO: in the future handle ANY as well
  if (backward) {
    _edgeDataBackward.try_emplace(edgeId, edgeSlice);
    _vertexConnectedEdgesBackward.try_emplace(origin, initVector);  // TODO inert only if needed (better init)
    auto& edgeVectorBackward = _vertexConnectedEdgesBackward.at(origin);
    edgeVectorBackward.emplace_back(std::move(edgeToEmplace));
  } else {
    _edgeDataForward.try_emplace(edgeId, edgeSlice);
    _vertexConnectedEdgesForward.try_emplace(origin, initVector); // TODO insert only if needed (better init)
    auto& edgeVectorForward = _vertexConnectedEdgesForward.at(origin);
    edgeVectorForward.emplace_back(std::move(edgeToEmplace));
  }
}

auto RefactoredClusterTraverserCache::getVertexRelations(const VertexType& vertex, bool backward)
    -> std::vector<std::pair<EdgeType, VertexType>> {
  TRI_ASSERT(isVertexCached(vertex));

  if (!isVertexRelationCached(vertex, backward)) {
    return {};
  }

  if (backward) {
    return _vertexConnectedEdgesBackward.at(vertex);
  }

  return _vertexConnectedEdgesForward.at(vertex);
}

auto RefactoredClusterTraverserCache::isVertexCached(VertexType const& vertexKey) -> bool {
  return (_vertexData.find(vertexKey) == _vertexData.end()) ? false : true;
}

auto RefactoredClusterTraverserCache::isVertexRelationCached(VertexType const& vertexKey,
                                                             bool backward) -> bool {
  if (backward) {
    return (_vertexConnectedEdgesBackward.find(vertexKey) ==
            _vertexConnectedEdgesBackward.end())
               ? false
               : true;
  }
  return (_vertexConnectedEdgesForward.find(vertexKey) ==
          _vertexConnectedEdgesForward.end())
             ? false
             : true;
}

auto RefactoredClusterTraverserCache::isEdgeCached(EdgeType const& edgeKey,
                                                   bool backward) -> bool {
  if (backward) {
    return (_edgeDataBackward.find(edgeKey) == _edgeDataBackward.end()) ? false : true;
  }
  return (_edgeDataForward.find(edgeKey) == _edgeDataForward.end()) ? false : true;
}

auto RefactoredClusterTraverserCache::getCachedVertex(VertexType const& vertex) -> VPackSlice {
  if (!isVertexCached(vertex)) {
    return VPackSlice::noneSlice();
  }
  return _vertexData.at(vertex);
}

auto RefactoredClusterTraverserCache::getCachedEdge(EdgeType const& edge, bool backward)
    -> VPackSlice {
  if (!isEdgeCached(edge, backward)) {
    return VPackSlice::noneSlice();
  }
  if (backward) {
    return _edgeDataBackward.at(edge);
  }
  return _edgeDataForward.at(edge);
}