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

#include <velocypack/Iterator.h>
#include <velocypack/velocypack-aliases.h>

#include "ApplicationFeatures/ApplicationServer.h"
#include "Basics/StaticStrings.h"
#include "Basics/VelocyPackHelper.h"
#include "Cluster/ClusterMethods.h"
#include "Graph/BreadthFirstEnumerator.h"
#include "Graph/ClusterTraverserCache.h"
#include "Graph/EdgeCursor.h"
#include "Graph/PathEnumerator.h"
#include "Graph/TraverserCache.h"
#include "Logger/LogMacros.h"
#include "Network/Methods.h"
#include "Network/NetworkFeature.h"
#include "Network/Utils.h"
#include "Transaction/Helpers.h"

using namespace arangodb;
using namespace arangodb::graph;

using ClusterTraverser = arangodb::traverser::ClusterTraverser;

ClusterTraverser::ClusterTraverser(arangodb::traverser::TraverserOptions* opts,
                                   std::unordered_map<ServerID, aql::EngineId> const* engines,
                                   std::string const& dbname)
    : Traverser(opts), 
      _dbname(dbname), 
      _engines(engines) { 
  _opts->linkTraverser(this);
  
  createEnumerator();
  TRI_ASSERT(_enumerator != nullptr);
}

void ClusterTraverser::setStartVertex(std::string const& vid) {
  arangodb::velocypack::StringRef const s(vid);

  auto it = _vertices.find(s);
  if (it == _vertices.end()) {
    size_t firstSlash = vid.find('/');
    if (firstSlash == std::string::npos ||
        vid.find('/', firstSlash + 1) != std::string::npos) {
      // We can stop here. The start vertex is not a valid _id
      traverserCache()->increaseFilterCounter();
      _done = true;
      return;
    }
  }

  if (!vertexMatchesConditions(s, 0)) {
    // Start vertex invalid
    _done = true;
    return;
  }
  
  arangodb::velocypack::StringRef persId = traverserCache()->persistString(s);
  _vertexGetter->reset(persId);
  _enumerator->setStartVertex(persId);
  _done = false;
}

void ClusterTraverser::clear() {
  traverserCache()->clear();

  _vertices.clear();
  _verticesToFetch.clear();
}

bool ClusterTraverser::getVertex(VPackSlice edge, arangodb::traverser::EnumeratedPath& path) {
  bool res = _vertexGetter->getVertex(edge, path);
  if (res) {
    arangodb::velocypack::StringRef const& other = path.lastVertex();
    if (_vertices.find(other) == _vertices.end()) {
      // Vertex not yet cached. Prepare it.
      _verticesToFetch.emplace(other);
    }
  }
  return res;
}

bool ClusterTraverser::getSingleVertex(arangodb::velocypack::Slice edge,
                                       arangodb::velocypack::StringRef sourceVertexId,
                                       uint64_t depth, arangodb::velocypack::StringRef& targetVertexId) {
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
  if (_opts->produceVertices()) {
    auto ch = static_cast<ClusterTraverserCache*>(traverserCache());
    ch->insertedDocuments() += _verticesToFetch.size();
    fetchVerticesFromEngines(*_trx, _engines, _verticesToFetch, _vertices, ch->datalake(),
                             /*forShortestPath*/ false);

    _enumerator->incHttpRequests(_engines->size()); 
  } else {
    for (auto const& it : _verticesToFetch) {
      _vertices.emplace(it, VPackSlice::nullSlice());
    }
  }
  _verticesToFetch.clear();
}

aql::AqlValue ClusterTraverser::fetchVertexData(arangodb::velocypack::StringRef idString) {
  // TRI_ASSERT(idString.isString());
  auto cached = _vertices.find(idString);
  if (cached == _vertices.end()) {
    // Vertex not yet cached. Prepare for load.
    
    // we need to make sure the idString remains valid afterwards
    idString = _opts->cache()->persistString(idString);
    _verticesToFetch.emplace(idString);
    fetchVertices();
    cached = _vertices.find(idString);
  }
  // Now all vertices are cached!!
  TRI_ASSERT(cached != _vertices.end());
  uint8_t const* ptr = cached->second.begin();
  return aql::AqlValue(ptr); // no copy constructor
}

//////////////////////////////////////////////////////////////////////////////
/// @brief Function to add the real data of a vertex into a velocypack builder
//////////////////////////////////////////////////////////////////////////////

void ClusterTraverser::addVertexToVelocyPack(arangodb::velocypack::StringRef vid, VPackBuilder& result) {
  auto cached = _vertices.find(vid);
  if (cached == _vertices.end()) {
    // Vertex not yet cached. Prepare for load.
    _verticesToFetch.emplace(vid);
    fetchVertices();
    cached = _vertices.find(vid);
  }
  // Now all vertices are cached!!
  TRI_ASSERT(cached != _vertices.end());
  result.add(cached->second);
}

void ClusterTraverser::destroyEngines() {
  // We have to clean up the engines in Coordinator Case.
  NetworkFeature const& nf = _trx->vocbase().server().getFeature<NetworkFeature>();
  network::ConnectionPool* pool = nf.pool();
  if (pool == nullptr) {
    return;
  }
  // nullptr only happens on controlled server shutdown

  _enumerator->incHttpRequests(_engines->size());

  VPackBuffer<uint8_t> body;
  
  network::RequestOptions options;
  options.database = _trx->vocbase().name();
  options.timeout = network::Timeout(30.0);
  options.skipScheduler = true; // hack to speed up future.get()

  // TODO: use collectAll to parallelize shutdown ?
  for (auto const& it : *_engines) {
    auto res =
        network::sendRequest(pool, "server:" + it.first, fuerte::RestVerb::Delete,
                             "/_internal/traverser/" + arangodb::basics::StringUtils::itoa(it.second),
                             body, options);
    res.wait();

    if (!res.hasValue() || res.get().fail()) {
      // Note If there was an error on server side we do not have ok()
      std::string message("Could not destroy all traversal engines");
      if (res.hasValue()) {
        message += ": " + network::fuerteToArangoErrorMessage(res.get());
      }
      LOG_TOPIC("8a7a0", ERR, arangodb::Logger::FIXME) << message;
    }
  }
}

void ClusterTraverser::createEnumerator() {
  TRI_ASSERT(_enumerator == nullptr);

  if (_opts->useBreadthFirst) {
    _enumerator.reset(new arangodb::graph::BreadthFirstEnumerator(this, _opts));
  } else {
    _enumerator.reset(new arangodb::traverser::DepthFirstEnumerator(this, _opts));
  }
}
