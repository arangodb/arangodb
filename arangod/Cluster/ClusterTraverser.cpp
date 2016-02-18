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

#include "ClusterTraverser.h"
#include "Cluster/ClusterMethods.h"

using ClusterTraversalPath = arangodb::traverser::ClusterTraversalPath;
using ClusterTraverser = arangodb::traverser::ClusterTraverser;

arangodb::basics::Json* ClusterTraversalPath::pathToJson(
    arangodb::Transaction*, arangodb::CollectionNameResolver*) {
  auto result =
      std::make_unique<arangodb::basics::Json>(arangodb::basics::Json::Object);

  size_t vCount = _path.vertices.size();
  arangodb::basics::Json vertices(arangodb::basics::Json::Array, vCount);
  for (auto& it : _path.vertices) {
    auto v = _traverser->vertexToJson(it);
    try {
      vertices.add(*v);
      delete v;
    } catch (...) {
      delete v;
      throw;
    }
  }
  arangodb::basics::Json edges(arangodb::basics::Json::Array,
                               _path.edges.size());
  for (auto& it : _path.edges) {
    auto e = _traverser->edgeToJson(it);
    try {
      edges.add(*e);
      delete e;
    } catch (...) {
      delete e;
      throw;
    }
  }
  result->set("edges", edges);
  result->set("vertices", vertices);
  return result.release();
}

arangodb::basics::Json* ClusterTraversalPath::lastEdgeToJson(
    arangodb::Transaction*, arangodb::CollectionNameResolver*) {
  return _traverser->edgeToJson(_path.edges.back());
}

arangodb::basics::Json* ClusterTraversalPath::lastVertexToJson(
    arangodb::Transaction*, arangodb::CollectionNameResolver*) {
  return _traverser->vertexToJson(_path.vertices.back());
}

bool ClusterTraverser::VertexGetter::operator()(std::string const& edgeId,
                                                std::string const& vertexId,
                                                size_t depth,
                                                std::string& result) {
  auto it = _traverser->_edges.find(edgeId);
  std::string def = "";
  if (it != _traverser->_edges.end()) {
    std::string from =
        arangodb::basics::JsonHelper::getStringValue(it->second, "_from", def);
    if (from != vertexId) {
      result = from;
    } else {
      std::string to =
          arangodb::basics::JsonHelper::getStringValue(it->second, "_to", def);
      result = to;
    }
    auto exp = _traverser->_expressions->find(depth);
    if (exp != _traverser->_expressions->end()) {
      auto v = _traverser->_vertices.find(result);
      if (v == _traverser->_vertices.end()) {
        // If the vertex ist not in list it means it has not passed any
        // filtering up to now
        ++_traverser->_filteredPaths;
        return false;
      }
      if (!_traverser->vertexMatchesCondition(v->second, exp->second)) {
        return false;
      }
    }
    return true;
  }
  // This should never be reached
  result = def;
  return false;
}

void ClusterTraverser::EdgeGetter::operator()(std::string const& startVertex,
                                              std::vector<std::string>& result,
                                              size_t*& last, size_t& eColIdx,
                                              bool& unused) {
  std::string collName;
  TRI_edge_direction_e dir;
  if (!_traverser->_opts.getCollection(eColIdx, collName, dir)) {
    // Nothing to do, caller has set a defined state already.
    return;
  }
  size_t depth = result.size();
  if (last == nullptr) {
    TRI_ASSERT(_traverser->_iteratorCache.size() == result.size());
    // We have to request the next level
    arangodb::basics::Json resultEdges(arangodb::basics::Json::Object);
    arangodb::rest::GeneralResponse::HttpResponseCode responseCode;
    std::string contentType;
    std::vector<TraverserExpression*> expEdges;
    auto found = _traverser->_expressions->find(depth);
    if (found != _traverser->_expressions->end()) {
      expEdges = found->second;
    }

    int res = getFilteredEdgesOnCoordinator(
        _traverser->_dbname, collName, startVertex, dir,
        expEdges, responseCode, contentType, resultEdges);
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
    arangodb::basics::Json edgesJson = resultEdges.get("edges");

    arangodb::basics::Json statsJson = resultEdges.get("stats");
    size_t read = arangodb::basics::JsonHelper::getNumericValue<size_t>(
        statsJson.json(), "scannedIndex", 0);
    size_t filter = arangodb::basics::JsonHelper::getNumericValue<size_t>(
        statsJson.json(), "filtered", 0);
    _traverser->_readDocuments += read;
    _traverser->_filteredPaths += filter;

    size_t count = edgesJson.size();
    if (count == 0) {
      last = nullptr;
      eColIdx++;
      operator()(startVertex, result, last, eColIdx, unused);
      return;
    }
    std::stack<std::string> stack;
    std::unordered_set<std::string> verticesToFetch;
    for (size_t i = 0; i < edgesJson.size(); ++i) {
      arangodb::basics::Json edge = edgesJson.at(i);
      std::string edgeId =
          arangodb::basics::JsonHelper::getStringValue(edge.json(), "_id", "");
      stack.push(edgeId);
      std::string fromId = arangodb::basics::JsonHelper::getStringValue(
          edge.json(), "_from", "");
      if (_traverser->_vertices.find(fromId) == _traverser->_vertices.end()) {
        verticesToFetch.emplace(fromId);
      }
      std::string toId =
          arangodb::basics::JsonHelper::getStringValue(edge.json(), "_to", "");
      if (_traverser->_vertices.find(toId) == _traverser->_vertices.end()) {
        verticesToFetch.emplace(toId);
      }
      std::unique_ptr<TRI_json_t> copy(edge.copy().steal());
      if (copy != nullptr) {
        if (_traverser->_edges.emplace(edgeId, copy.get()).second) {
          // if insertion was successful, hand over the ownership
          copy.release();
        }
        // else we have a duplicate and we need to free the copy again
      }
    }

    std::vector<TraverserExpression*> expVertices;
    found = _traverser->_expressions->find(depth + 1);
    if (found != _traverser->_expressions->end()) {
      expVertices = found->second;
    }

    std::unique_ptr<std::map<std::string, std::string>> headers(
        new std::map<std::string, std::string>());
    _traverser->_readDocuments += verticesToFetch.size();
    res = getFilteredDocumentsOnCoordinator(_traverser->_dbname, expVertices,
                                            headers, verticesToFetch,
                                            _traverser->_vertices);
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
    // By convention verticesToFetch now contains all _ids of vertices that
    // could not be found.
    // Store them as NULL
    for (auto const& it : verticesToFetch) {
      _traverser->_vertices.emplace(it,
                                    TRI_CreateNullJson(TRI_UNKNOWN_MEM_ZONE));
    }
    std::string next = stack.top();
    stack.pop();
    last = &_continueConst;
    _traverser->_iteratorCache.emplace(stack);
    auto search = std::find(result.begin(), result.end(), next);
    if (search != result.end()) {
      // result.push_back(next);
      // The edge is now included twice. Go on with the next
      operator()(startVertex, result, last, eColIdx, unused);
      return;
    }
    result.push_back(next);
  } else {
    if (_traverser->_iteratorCache.empty()) {
      last = nullptr;
      return;
    }
    std::stack<std::string>& tmp = _traverser->_iteratorCache.top();
    if (tmp.empty()) {
      _traverser->_iteratorCache.pop();
      last = nullptr;
      eColIdx++;
      operator()(startVertex, result, last, eColIdx, unused);
      return;
    } else {
      std::string next = tmp.top();
      tmp.pop();
      auto search = std::find(result.begin(), result.end(), next);
      if (search != result.end()) {
        // result.push_back(next);
        // The edge is now included twice. Go on with the next
        operator()(startVertex, result, last, eColIdx, unused);
        return;
      }
      result.push_back(next);
    }
  }
}

void ClusterTraverser::setStartVertex(arangodb::traverser::VertexId const& v) {
  std::string id = v.toString(_resolver);
  _enumerator.reset(
      new arangodb::basics::PathEnumerator<std::string, std::string, size_t>(
          _edgeGetter, _vertexGetter, id));
  _done = false;
  auto it = _vertices.find(id);
  if (it == _vertices.end()) {
    arangodb::rest::GeneralResponse::HttpResponseCode responseCode;
    std::unique_ptr<std::map<std::string, std::string>> headers(
        new std::map<std::string, std::string>());
    std::map<std::string, std::string> resultHeaders;
    std::vector<std::string> splitId =
        arangodb::basics::StringUtils::split(id, '/');
    TRI_ASSERT(splitId.size() == 2);
    std::string vertexResult;
    int res = getDocumentOnCoordinator(_dbname, splitId[0], splitId[1], 0,
                                       headers, true, responseCode,
                                       resultHeaders, vertexResult);
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
    ++_readDocuments;
    if (responseCode ==
        arangodb::rest::GeneralResponse::HttpResponseCode::NOT_FOUND) {
      _vertices.emplace(id, TRI_CreateNullJson(TRI_UNKNOWN_MEM_ZONE));
    } else {
      _vertices.emplace(id,
                        arangodb::basics::JsonHelper::fromString(vertexResult));
    }
    it = _vertices.find(id);
  }
  auto exp = _expressions->find(0);
  if (exp != _expressions->end() &&
      !vertexMatchesCondition(it->second, exp->second)) {
    // We can stop here. The start vertex does not match condition
    _done = true;
  }
}

bool ClusterTraverser::vertexMatchesCondition(
    TRI_json_t* v,
    std::vector<arangodb::traverser::TraverserExpression*> const& exp) {
  for (auto const& e : exp) {
    if (!e->isEdgeAccess) {
      if (v == nullptr || !e->matchesCheck(v)) {
        ++_filteredPaths;
        return false;
      }
    }
  }
  return true;
}

arangodb::traverser::TraversalPath* ClusterTraverser::next() {
  TRI_ASSERT(!_done);
  if (_pruneNext) {
    _pruneNext = false;
    _enumerator->prune();
  }
  TRI_ASSERT(!_pruneNext);
  const arangodb::basics::EnumeratedPath<std::string, std::string>& path =
      _enumerator->next();
  size_t countEdges = path.edges.size();
  if (countEdges == 0) {
    _done = true;
    // Done traversing
    return nullptr;
  }

  auto p = std::make_unique<ClusterTraversalPath>(this, path);
  if (countEdges >= _opts.maxDepth) {
    _pruneNext = true;
  }
  if (countEdges < _opts.minDepth) {
    return next();
  }
  return p.release();
}

arangodb::basics::Json* ClusterTraverser::edgeToJson(
    std::string const& id) const {
  auto it = _edges.find(id);
  TRI_ASSERT(it != _edges.end());
  return new arangodb::basics::Json(
      TRI_UNKNOWN_MEM_ZONE, TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, it->second));
}

arangodb::basics::Json* ClusterTraverser::vertexToJson(
    std::string const& id) const {
  auto it = _vertices.find(id);
  TRI_ASSERT(it != _vertices.end());
  return new arangodb::basics::Json(
      TRI_UNKNOWN_MEM_ZONE, TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, it->second));
}
