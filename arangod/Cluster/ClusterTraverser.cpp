////////////////////////////////////////////////////////////////////////////////
/// @brief Cluster Traverser
///
/// @file
///
/// DISCLAIMER
///
/// Copyright 2014-2015 ArangoDB GmbH, Cologne, Germany
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
// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Michael Hackstein
/// @author Copyright 2014-2015, ArangoDB GmbH, Cologne, Germany
/// @author Copyright 2012-2013, triAGENS GmbH, Cologne, Germany
////////////////////////////////////////////////////////////////////////////////

#include "Cluster/ClusterTraverser.h"
#include "Cluster/ClusterMethods.h"

using ClusterTraversalPath = triagens::arango::traverser::ClusterTraversalPath;
using ClusterTraverser = triagens::arango::traverser::ClusterTraverser;

// -----------------------------------------------------------------------------
// --SECTION--                                        class ClusterTraversalPath
// -----------------------------------------------------------------------------

triagens::basics::Json* ClusterTraversalPath::pathToJson (Transaction*,
                                                          CollectionNameResolver*) const {
  std::unique_ptr<triagens::basics::Json> result(new triagens::basics::Json(triagens::basics::Json::Object));
  size_t vCount = _path.vertices.size();
  triagens::basics::Json vertices(triagens::basics::Json::Array, vCount);
  for (auto& it : _path.vertices) {
    vertices.add(*_traverser->vertexToJson(it));
  } 
  triagens::basics::Json edges(triagens::basics::Json::Array, _path.edges.size());
  for (auto& it : _path.edges) {
    edges.add(*_traverser->edgeToJson(it));
  } 
  result->set("edges", edges);
  result->set("vertices", vertices);
  return result.release();
}

triagens::basics::Json* ClusterTraversalPath::lastEdgeToJson (Transaction*,
                                                              CollectionNameResolver*) const {
  return _traverser->edgeToJson(_path.edges.back());
}

triagens::basics::Json* ClusterTraversalPath::lastVertexToJson (Transaction*,
                                                                CollectionNameResolver*) const {
  return _traverser->vertexToJson(_path.vertices.back());
}

// -----------------------------------------------------------------------------
// --SECTION--                                            class ClusterTraverser
// -----------------------------------------------------------------------------

bool ClusterTraverser::VertexGetter::operator() (std::string const& edgeId,
                                                 std::string const& vertexId,
                                                 size_t depth,
                                                 std::string& result) {
  auto it = _traverser->_edges.find(edgeId);
  std::string def = "";
  if (it != _traverser->_edges.end()) {
    std::string from = triagens::basics::JsonHelper::getStringValue(it->second, "_from", def);
    if (from != vertexId) {
      result = from;
    }
    else {
      std::string to = triagens::basics::JsonHelper::getStringValue(it->second, "_to", def);
      result = to;
    }
    return true;
  }
  TRI_ASSERT(false);
  result = def;
  return false;
}

void ClusterTraverser::EdgeGetter::operator() (std::string const& startVertex,
                                               std::vector<std::string>& result,
                                               size_t*& last,
                                               size_t& eColIdx,
                                               bool& unused) {
  if (eColIdx >= _traverser->_edgeCols.size()) {
    // Nothing to do, caller has set a defined state already.
    return;
  }
  size_t depth = result.size();
  if (last == nullptr) {
    TRI_ASSERT(_traverser->_iteratorCache.size() == result.size());
    // We have to request the next level
    triagens::basics::Json resultEdges(triagens::basics::Json::Object);
    triagens::rest::HttpResponse::HttpResponseCode responseCode;
    std::string contentType;
    std::string collName = _traverser->_edgeCols[eColIdx];
    std::vector<TraverserExpression*> expEdges;
    auto found = _traverser->_expressions->find(depth);
    if (found != _traverser->_expressions->end()) {
      expEdges = found->second;
    }

    int res = getFilteredEdgesOnCoordinator(_traverser->_dbname,
                                            collName,
                                            startVertex,
                                            _traverser->_opts.direction,
                                            expEdges,
                                            responseCode,
                                            contentType,
                                            resultEdges);
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
    triagens::basics::Json edgesJson = resultEdges.get("edges");
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
      triagens::basics::Json edge = edgesJson.at(i);
      std::string edgeId = triagens::basics::JsonHelper::getStringValue(edge.json(), "_id", "");
      stack.push(edgeId);
      std::string fromId = triagens::basics::JsonHelper::getStringValue(edge.json(), "_from", "");
      if (_traverser->_vertices.find(fromId) == _traverser->_vertices.end()) {
        verticesToFetch.emplace(fromId);
      }
      std::string toId = triagens::basics::JsonHelper::getStringValue(edge.json(), "_to", "");
      if (_traverser->_vertices.find(toId) == _traverser->_vertices.end()) {
        verticesToFetch.emplace(toId);
      }
      _traverser->_edges.emplace(edgeId, edge.copy().steal());
    }

    std::vector<TraverserExpression*> expVertices;
    found = _traverser->_expressions->find(depth + 1);
    if (found != _traverser->_expressions->end()) {
      expVertices = found->second;
    }

    std::map<std::string, std::string> headers;
    res = getFilteredDocumentsOnCoordinator(_traverser->_dbname,
                                            expVertices,
                                            headers,
                                            verticesToFetch,
                                            _traverser->_vertices);
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
    if (! expVertices.empty() && ! verticesToFetch.empty()) {
      // There are some vertices that either do not exist or do not match the filter.
      // We have to exclude these from the traversal
    }
    std::string next = stack.top();
    stack.pop();
    last = &_continueConst;
    result.push_back(next);
    _traverser->_iteratorCache.emplace(stack);
  }
  else {
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
    }
    else {
      std::string next = tmp.top();
      tmp.pop();
      result.push_back(next);
    }
  }
}

void ClusterTraverser::setStartVertex (VertexId& v) {
  std::string id = v.toString(_resolver);
  _enumerator.reset(new triagens::basics::PathEnumerator<std::string, std::string, size_t> (_edgeGetter, _vertexGetter, id));
  _done = false;
  auto it = _vertices.find(id);
  if (it == _vertices.end()) {
    triagens::rest::HttpResponse::HttpResponseCode responseCode;
    std::map<std::string, std::string> headers;
    std::map<std::string, std::string> resultHeaders;
    std::vector<std::string> splitId = triagens::basics::StringUtils::split(id, '/'); 
    TRI_ASSERT(splitId.size() == 2);
    std::string vertexResult;
    int res = getDocumentOnCoordinator(_dbname,
                                       splitId[0],
                                       splitId[1],
                                       0,
                                       headers,
                                       true,
                                       responseCode,
                                       resultHeaders,
                                       vertexResult);
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
    if (responseCode == triagens::rest::HttpResponse::HttpResponseCode::NOT_FOUND) {
      _vertices.emplace(id, nullptr);
    }
    else {
      _vertices.emplace(id, triagens::basics::JsonHelper::fromString(vertexResult));
    }
    it = _vertices.find(id);
  }
  auto exp = _expressions->find(0);
  if (exp != _expressions->end() && ! vertexMatchesCondition(it->second, exp->second)) {
    // We can stop here. The start vertex does not match condition
    _done = true;
  }
}

bool ClusterTraverser::vertexMatchesCondition (TRI_json_t* v, std::vector<TraverserExpression*> const& exp) {
  for (auto const& e : exp) {
    if (! e->isEdgeAccess) {
      if (v == nullptr || ! e->matchesCheck(v)) {
        return false;
      }
    }
  }
  return true;
}

const triagens::arango::traverser::TraversalPath* ClusterTraverser::next () {
  TRI_ASSERT(!_done);
  if (_pruneNext) {
    _pruneNext = false;
    _enumerator->prune();
  }
  TRI_ASSERT(!_pruneNext);
  const triagens::basics::EnumeratedPath<std::string, std::string>& path = _enumerator->next();
  size_t countEdges = path.edges.size();
  if (countEdges == 0) {
    _done = true;
    // Done traversing
    return nullptr;
  }

  std::unique_ptr<ClusterTraversalPath> p(new ClusterTraversalPath(this, path));
  if (_opts.shouldPrunePath(p.get())) {
    _enumerator->prune();
    return next();
  }
  if (countEdges >= _opts.maxDepth) {
    _pruneNext = true;
  }
  if (countEdges < _opts.minDepth) {
    return next();
  }
  return p.release();
}

triagens::basics::Json* ClusterTraverser::edgeToJson (std::string id) const {
  auto it = _edges.find(id);
  TRI_ASSERT(it != _edges.end());
  return new triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE, TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, it->second));
}

triagens::basics::Json* ClusterTraverser::vertexToJson (std::string id) const {
  auto it = _vertices.find(id);
  TRI_ASSERT(it != _vertices.end());
  return new triagens::basics::Json(TRI_UNKNOWN_MEM_ZONE, TRI_CopyJson(TRI_UNKNOWN_MEM_ZONE, it->second));
}
