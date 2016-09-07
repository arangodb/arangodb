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
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterMethods.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;

using ClusterTraverser = arangodb::traverser::ClusterTraverser;

ClusterTraverser::ClusterTraverser(
    arangodb::traverser::TraverserOptions* opts,
    std::unordered_map<ServerID, traverser::TraverserEngineID> const* engines,
    std::string const& dbname, Transaction* trx)
    : Traverser(opts, trx), _dbname(dbname), _engines(engines) {
  _opts->linkTraverser(this);
}

void ClusterTraverser::setStartVertex(std::string const& id) {
  _verticesToFetch.clear();
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
  bool res = _vertexGetter->getVertex(edge, result);
  if (res) {
    VPackSlice other = result.back();
    if (_vertices.find(other) == _vertices.end()) {
      // Vertex not yet cached. Prepare it.
      _verticesToFetch.emplace(other);
    }
  }
  return res;
}

bool ClusterTraverser::getSingleVertex(VPackSlice edge, VPackSlice comp,
                                       size_t depth, VPackSlice& result) {
  bool res = _vertexGetter->getSingleVertex(edge, comp, depth, result);
  if (res) {
    if (_vertices.find(result) == _vertices.end()) {
      // Vertex not yet cached. Prepare it.
      _verticesToFetch.emplace(result);
    }
  }
  return res;
}

void ClusterTraverser::fetchVertices() {
  _readDocuments += _verticesToFetch.size();
  TransactionBuilderLeaser lease(_trx);
  fetchVerticesFromEngines(_dbname, _engines, _verticesToFetch, _vertices,
                           *(lease.get()));
  _verticesToFetch.clear();
}

aql::AqlValue ClusterTraverser::fetchVertexData(VPackSlice idString) {
  TRI_ASSERT(idString.isString());
  auto cached = _vertices.find(idString);
  if (cached == _vertices.end()) {
    // Vertex not yet cached. Prepare for load.
    _verticesToFetch.emplace(idString);
    fetchVertices();
    cached = _vertices.find(idString);
  }
  // Now all vertices are cached!!
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
  if (cached == _vertices.end()) {
    // Vertex not yet cached. Prepare for load.
    _verticesToFetch.emplace(id);
    fetchVertices();
    cached = _vertices.find(id);
  }
  // Now all vertices are cached!!
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
