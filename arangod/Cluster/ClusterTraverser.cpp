////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2018 ArangoDB GmbH, Cologne, Germany
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
#include "Graph/BreadthFirstEnumerator.h"
#include "Graph/ClusterTraverserCache.h"
#include "Graph/TraverserCache.h"
#include "Transaction/Helpers.h"

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

using namespace arangodb;
using namespace arangodb::graph;

using ClusterTraverser = arangodb::traverser::ClusterTraverser;

ClusterTraverser::ClusterTraverser(
    arangodb::traverser::TraverserOptions* opts,
    ManagedDocumentResult* mmdr,
    std::unordered_map<ServerID, traverser::TraverserEngineID> const* engines,
    std::string const& dbname, transaction::Methods* trx)
    : Traverser(opts, trx, mmdr), _dbname(dbname), _engines(engines) {
  _opts->linkTraverser(this);
}

void ClusterTraverser::setStartVertex(std::string const& vid) {
  _verticesToFetch.clear();
  _startIdBuilder.clear();
  _startIdBuilder.add(VPackValue(vid));
  VPackSlice idSlice = _startIdBuilder.slice();

  auto it = _vertices.find(StringRef(vid));
  if (it == _vertices.end()) {
    size_t firstSlash = vid.find("/");
    if (firstSlash == std::string::npos ||
        vid.find("/", firstSlash + 1) != std::string::npos) {
      // We can stop here. The start vertex is not a valid _id
      traverserCache()->increaseFilterCounter();
      _done = true;
      return;
    }
  }

  if (!vertexMatchesConditions(StringRef(vid), 0)) {
    // Start vertex invalid
    _done = true;
    return;
  }
  StringRef persId = traverserCache()->persistString(StringRef(vid));

  _vertexGetter->reset(persId);
  if (_opts->useBreadthFirst) {
    _enumerator.reset(
        new arangodb::graph::BreadthFirstEnumerator(this, idSlice, _opts));
  } else {
    _enumerator.reset(
        new arangodb::traverser::DepthFirstEnumerator(this, vid, _opts));
  }
  _done = false;
}

bool ClusterTraverser::getVertex(VPackSlice edge,
                                 std::vector<StringRef>& result) {
  bool res = _vertexGetter->getVertex(edge, result);
  if (res) {
    StringRef const& other = result.back();
    if (_vertices.find(other) == _vertices.end()) {
      // Vertex not yet cached. Prepare it.
      _verticesToFetch.emplace(other);
    }
  }
  return res;
}

bool ClusterTraverser::getSingleVertex(arangodb::velocypack::Slice edge, StringRef const sourceVertexId,
                     uint64_t depth, StringRef& targetVertexId) {
  bool res = _vertexGetter->getSingleVertex(edge, sourceVertexId, depth, targetVertexId);
  if (res) {
    if (_vertices.find(targetVertexId) == _vertices.end()) {
      // Vertex not yet cached. Prepare it.
      _verticesToFetch.emplace(targetVertexId);
    }
  }
  return res;
}

void ClusterTraverser::fetchVertices() {
  auto ch = static_cast<ClusterTraverserCache*>(traverserCache());
  ch->insertedDocuments() += _verticesToFetch.size();
  transaction::BuilderLeaser lease(_trx);
  fetchVerticesFromEngines(_dbname, _engines, _verticesToFetch, _vertices,
                           *(lease.get()));
  _verticesToFetch.clear();
}

aql::AqlValue ClusterTraverser::fetchVertexData(StringRef idString) {
  //TRI_ASSERT(idString.isString());
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

//////////////////////////////////////////////////////////////////////////////
/// @brief Function to add the real data of a vertex into a velocypack builder
//////////////////////////////////////////////////////////////////////////////

void ClusterTraverser::addVertexToVelocyPack(StringRef vid,
                                             VPackBuilder& result) {
  auto cached = _vertices.find(vid);
  if (cached == _vertices.end()) {
    // Vertex not yet cached. Prepare for load.
    _verticesToFetch.emplace(vid);
    fetchVertices();
    cached = _vertices.find(vid);
  }
  // Now all vertices are cached!!
  TRI_ASSERT(cached != _vertices.end());
  result.add(VPackSlice((*cached).second->data()));
}
