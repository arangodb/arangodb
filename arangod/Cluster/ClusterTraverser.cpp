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

using ClusterTraverser = arangodb::traverser::ClusterTraverser;

void ClusterTraverser::setStartVertex(std::string const& id) {
  _startIdBuilder->clear();
  _startIdBuilder->add(VPackValue(id));
  VPackSlice idSlice = _startIdBuilder->slice();

  auto it = _vertices.find(idSlice);
  if (it == _vertices.end()) {
    size_t firstSlash = id.find("/");
    if (firstSlash == std::string::npos ||
        id.find("/", firstSlash + 1) != std::string::npos) {
      // We can stop here. The start vertex is not a valid _id
      ++_filteredPaths;
      _done = true;
      return;
    }
    std::unordered_set<std::string> vertexToFetch;
    vertexToFetch.emplace(id);
    fetchVertices(vertexToFetch, 0); // this inserts the vertex
    it = _vertices.find(idSlice);
    if (it == _vertices.end()) {
      // We can stop here. The start vertex does not match condition.
      ++_filteredPaths;
      _done = true;
      return;
    }
  }

  if (!vertexMatchesConditions(idSlice, 0)) {
    // Start vertex invalid
    _done = true;
    return;
  }

  _vertexGetter->reset(idSlice);
  if (_opts->useBreadthFirst) {
    _enumerator.reset(
        new arangodb::traverser::BreadthFirstEnumerator(this, idSlice, _opts));
  } else {
    _enumerator.reset(
        new arangodb::traverser::DepthFirstEnumerator(this, idSlice, _opts));
  }
  _done = false;
}

bool ClusterTraverser::getVertex(VPackSlice edge,
                                 std::vector<VPackSlice>& result) {
  return _vertexGetter->getVertex(edge, result);
}

bool ClusterTraverser::getSingleVertex(VPackSlice edge, VPackSlice comp,
                                       size_t depth, VPackSlice& result) {
  return _vertexGetter->getSingleVertex(edge, comp, depth, result);
}


void ClusterTraverser::fetchVertices(std::unordered_set<std::string>& verticesToFetch, size_t depth) {
  _readDocuments += verticesToFetch.size();

#warning Reimplement this. Fetching Documents Coordinator-Case
  /*
  std::vector<TraverserExpression*> expVertices;
  auto found = _opts.expressions->find(depth);
  if (found != _opts.expressions->end()) {
    expVertices = found->second;
  }

  int res = getFilteredDocumentsOnCoordinator(_dbname, expVertices,
                                              verticesToFetch, _vertices);
  if (res != TRI_ERROR_NO_ERROR && 
      res != TRI_ERROR_ARANGO_COLLECTION_NOT_FOUND) {
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
  */
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

bool ClusterTraverser::next() {
  TRI_ASSERT(!_done);
  return _enumerator->next();
}

aql::AqlValue ClusterTraverser::fetchVertexData(VPackSlice idString) {
  TRI_ASSERT(idString.isString());
  auto cached = _vertices.find(idString);
  // All vertices are cached!!
  TRI_ASSERT(cached != _vertices.end());
  return aql::AqlValue((*cached).second->data());
}

aql::AqlValue ClusterTraverser::fetchEdgeData(VPackSlice edge) {
  return aql::AqlValue(edge);
}

//////////////////////////////////////////////////////////////////////////////
/// @brief Function to add the real data of a vertex into a velocypack builder
//////////////////////////////////////////////////////////////////////////////

void ClusterTraverser::addVertexToVelocyPack(VPackSlice id,
                                             VPackBuilder& result) {
  TRI_ASSERT(id.isString());
  auto cached = _vertices.find(id);
  // All vertices are cached!!
  TRI_ASSERT(cached != _vertices.end());
  result.add(VPackSlice((*cached).second->data()));
}

//////////////////////////////////////////////////////////////////////////////
/// @brief Function to add the real data of an edge into a velocypack builder
//////////////////////////////////////////////////////////////////////////////

void ClusterTraverser::addEdgeToVelocyPack(arangodb::velocypack::Slice edge,
                         arangodb::velocypack::Builder& result) {
  result.add(edge);
}

aql::AqlValue ClusterTraverser::lastVertexToAqlValue() {
  return _enumerator->lastVertexToAqlValue();
}

aql::AqlValue ClusterTraverser::lastEdgeToAqlValue() {
  return _enumerator->lastEdgeToAqlValue();
}

aql::AqlValue ClusterTraverser::pathToAqlValue(VPackBuilder& builder) {
  return _enumerator->pathToAqlValue(builder);
}
