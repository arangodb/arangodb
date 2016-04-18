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
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterMethods.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

using ClusterTraversalPath = arangodb::traverser::ClusterTraversalPath;
using ClusterTraverser = arangodb::traverser::ClusterTraverser;

void ClusterTraversalPath::pathToVelocyPack(Transaction*, VPackBuilder& result) {
  result.openObject();
  result.add(VPackValue("edges"));
  result.openArray();
  for (auto const& it : _path.edges) {
    auto cached = _traverser->_edges.find(it);
    // All edges are cached!!
    TRI_ASSERT(cached != _traverser->_edges.end());
    result.add(VPackSlice(cached->second->data()));
  }
  result.close();
  result.add(VPackValue("vertices"));
  result.openArray();
  for (auto const& it : _path.vertices) {
    // All vertices are cached!!
    auto cached = _traverser->_vertices.find(it);
    TRI_ASSERT(cached != _traverser->_vertices.end());
    result.add(VPackSlice(cached->second->data()));
  }
  result.close();
  result.close();
}

void ClusterTraversalPath::lastVertexToVelocyPack(Transaction*, VPackBuilder& result) {
  auto cached = _traverser->_vertices.find(_path.vertices.back());
  TRI_ASSERT(cached != _traverser->_vertices.end());
  result.add(VPackSlice(cached->second->data()));
}

void ClusterTraversalPath::lastEdgeToVelocyPack(Transaction*, VPackBuilder& result) {
  auto cached = _traverser->_edges.find(_path.edges.back());
  // All edges are cached!!
  TRI_ASSERT(cached != _traverser->_edges.end());
  result.add(VPackSlice(cached->second->data()));
}

bool ClusterTraverser::VertexGetter::operator()(std::string const& edgeId,
                                                std::string const& vertexId,
                                                size_t depth,
                                                std::string& result) {
  auto it = _traverser->_edges.find(edgeId);
  if (it != _traverser->_edges.end()) {
    VPackSlice slice(it->second->data());
    std::string from = slice.get(TRI_VOC_ATTRIBUTE_FROM).copyString();
    if (from != vertexId) {
      result = from;
    } else {
      std::string to = slice.get(TRI_VOC_ATTRIBUTE_TO).copyString();
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
      if (!_traverser->vertexMatchesCondition(VPackSlice(v->second->data()), exp->second)) {
        return false;
      }
    }
    return true;
  }
  // This should never be reached
  result = "";
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
  if (last == nullptr) {
    size_t depth = result.size();
    TRI_ASSERT(_traverser->_iteratorCache.size() == result.size());
    // We have to request the next level
    arangodb::GeneralResponse::ResponseCode responseCode;
    std::string contentType;
    std::vector<TraverserExpression*> expEdges;
    auto found = _traverser->_expressions->find(depth);
    if (found != _traverser->_expressions->end()) {
      expEdges = found->second;
    }

    VPackBuilder resultEdges;
    resultEdges.openObject();
    int res = getFilteredEdgesOnCoordinator(
        _traverser->_dbname, collName, startVertex, dir,
        expEdges, responseCode, contentType, resultEdges);
    if (res != TRI_ERROR_NO_ERROR) {
      THROW_ARANGO_EXCEPTION(res);
    }
    resultEdges.close();
    VPackSlice resSlice = resultEdges.slice();
    VPackSlice edgesSlice = resSlice.get("edges");
    VPackSlice statsSlice = resSlice.get("stats");

    size_t read = arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(
        statsSlice, "scannedIndex", 0);
    size_t filter = arangodb::basics::VelocyPackHelper::getNumericValue<size_t>(
        statsSlice, "filtered", 0);
    _traverser->_readDocuments += read;
    _traverser->_filteredPaths += filter;

    if (edgesSlice.isNone() || edgesSlice.length() == 0) {
      last = nullptr;
      eColIdx++;
      operator()(startVertex, result, last, eColIdx, unused);
      return;
    }
    std::stack<std::string> stack;
    std::unordered_set<std::string> verticesToFetch;
    for (auto const& edge : VPackArrayIterator(edgesSlice)) {
      std::string edgeId = arangodb::basics::VelocyPackHelper::getStringValue(
          edge, TRI_VOC_ATTRIBUTE_ID, "");
      std::string fromId = arangodb::basics::VelocyPackHelper::getStringValue(
          edge, TRI_VOC_ATTRIBUTE_FROM, "");
      if (_traverser->_vertices.find(fromId) == _traverser->_vertices.end()) {
        verticesToFetch.emplace(std::move(fromId));
      }
      std::string toId = arangodb::basics::VelocyPackHelper::getStringValue(
          edge, TRI_VOC_ATTRIBUTE_TO, "");
      if (_traverser->_vertices.find(toId) == _traverser->_vertices.end()) {
        verticesToFetch.emplace(std::move(toId));
      }
      VPackBuilder tmpBuilder;
      tmpBuilder.add(edge);
      _traverser->_edges.emplace(edgeId, tmpBuilder.steal());
      stack.push(std::move(edgeId));
    }

    _traverser->fetchVertices(verticesToFetch, depth + 1);

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
      std::string const next = tmp.top();
      tmp.pop();
      auto search = std::find(result.begin(), result.end(), next);
      if (search != result.end()) {
        // The edge would be included twice. Go on with the next
        operator()(startVertex, result, last, eColIdx, unused);
        return;
      }
      result.push_back(next);
    }
  }
}

void ClusterTraverser::setStartVertex(std::string const& id) {
  _enumerator.reset(
      new arangodb::basics::PathEnumerator<std::string, std::string, size_t>(
          _edgeGetter, _vertexGetter, id));
  _done = false;
  auto it = _vertices.find(id);
  if (it == _vertices.end()) {
    std::unordered_set<std::string> vertexToFetch;
    vertexToFetch.emplace(id);
    fetchVertices(vertexToFetch, 0); // this inserts the vertex
    it = _vertices.find(id);
    if (it == _vertices.end()) {
      // We can stop here. The start vertex does not match condition.
      ++_filteredPaths;
      _done = true;
      return;
    }
  }

  auto exp = _expressions->find(0);
  if (exp != _expressions->end() &&
      !vertexMatchesCondition(VPackSlice(it->second->data()), exp->second)) {
    // We can stop here. The start vertex does not match condition
    _done = true;
  }
}

void ClusterTraverser::fetchVertices(std::unordered_set<std::string>& verticesToFetch, size_t depth) {
  std::unique_ptr<std::map<std::string, std::string>> headers(
      new std::map<std::string, std::string>());
  _readDocuments += verticesToFetch.size();

  std::vector<TraverserExpression*> expVertices;
  auto found = _expressions->find(depth);
  if (found != _expressions->end()) {
    expVertices = found->second;
  }

  int res = getFilteredDocumentsOnCoordinator(_dbname, expVertices, headers,
                                              verticesToFetch, _vertices);
  if (res != TRI_ERROR_NO_ERROR) {
    THROW_ARANGO_EXCEPTION(res);
  }

  // By convention verticesToFetch now contains all _ids of vertices that
  // could not be found.
  // Store them as NULL
  for (auto const& it : verticesToFetch) {
    VPackBuilder builder;
    builder.add(VPackValue(VPackValueType::Null));
    _vertices.emplace(it, builder.steal());
  }
}

bool ClusterTraverser::vertexMatchesCondition(
    VPackSlice const& v,
    std::vector<arangodb::traverser::TraverserExpression*> const& exp) {
  for (auto const& e : exp) {
    if (!e->isEdgeAccess) {
      if (v.isNone() || !e->matchesCheck(_trx, v)) {
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
