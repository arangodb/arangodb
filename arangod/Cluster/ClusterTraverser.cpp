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
  return nullptr;
}

triagens::basics::Json* ClusterTraversalPath::lastEdgeToJson (Transaction*,
                                                              CollectionNameResolver*) const {
  return nullptr;
}

triagens::basics::Json* ClusterTraversalPath::lastVertexToJson (Transaction*,
                                                                CollectionNameResolver*) const {
  return nullptr;
}

// -----------------------------------------------------------------------------
// --SECTION--                                            class ClusterTraverser
// -----------------------------------------------------------------------------

std::string ClusterTraverser::VertexGetter::operator() (std::string const& edgeId,
                                                        std::string const& vertexId) {
  auto it = _edges->find(edgeId);
  std::string def = "";
  if (it != _edges->end()) {
    std::string from = triagens::basics::JsonHelper::getStringValue(it->second, "_from", def);
    if (from != vertexId) {
      return from;
    }
    std::string to = triagens::basics::JsonHelper::getStringValue(it->second, "_to", def);
    return to;
  }
  return def;
}

#include <iostream>
void ClusterTraverser::EdgeGetter::operator() (std::string const& startVertex,
                                               std::vector<std::string>& result,
                                               size_t*& last,
                                               size_t& eColIdx,
                                               bool&) {
  TRI_ASSERT(eColIdx < _traverser->_edgeCols.size());
  if (last == nullptr) {
    TRI_ASSERT(_traverser->_iteratorCache.size() == result.size());
    // We have to request the next level
    triagens::basics::Json result(triagens::basics::Json::Object);
    triagens::rest::HttpResponse::HttpResponseCode responseCode;
    std::string contentType;
    std::string collName = _traverser->_edgeCols[eColIdx];

    int res = getAllEdgesOnCoordinator(_traverser->_dbname,
                                       collName,
                                       startVertex,
                                       _traverser->_opts.direction,
                                       responseCode,
                                       contentType,
                                       result);
    if (res != TRI_ERROR_NO_ERROR) {
      std::cout << "Result:" << res << std::endl;
    }
    std::cout << result << std::endl;
    TRI_ASSERT(false);
  }
  else {
    std::stack<std::string> tmp = _traverser->_iteratorCache.top();
    if (tmp.empty()) {
      _traverser->_iteratorCache.pop();
      last = nullptr;
    }
    else {
      std::string next = tmp.top();
      tmp.pop();
      result.push_back(next);
    }
  }
}

/*
void ClusterTraverser::defineInternalFunctions () {

  auto direction = _opts.direction;
  auto edgeCols = _edgeCols;
  _getEdge = [&edgeCols, &_iteratorCache] (std::string& startVertex, std::vector<std::string>& result, size_t*& last, size_t& eColIdx, bool&) {
    if (last == nullptr) {
      TRI_ASSERT(_iteratorCache.size() == result.size());
      // We have to request the next level
      // TODO
    }
    else {
      std::stack<std::string> tmp = _iteratorCache.top();
      if (tmp.empty()) {
        _iteratorCache.pop();
        last = nullptr;
      }
      else {
        result.push_back(tmp.pop());
      }
    }
  }
}
*/

void ClusterTraverser::setStartVertex (VertexId& v) {
  // TODO
  std::string id = "";
  _enumerator.reset(new triagens::basics::PathEnumerator<std::string, std::string, size_t> (_edgeGetter, _vertexGetter, id));
  _done = false;
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
